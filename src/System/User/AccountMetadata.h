/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Session-only Windows compatibility metadata. This is not authentication or
 * an entitlement grant. Inputs must come from the successful MSA/Xbox handshake.
 * Unmanaged registry keys/values are never replaced. */
struct account_metadata_record
{
    char xuid[21], xbox[17], store[17];
};
static const char account_metadata_class[] = "XodusNexus.AuthenticatedIdentity.v1";
static const char account_metadata_marker[] = "Software\\XodusNexusAccountMetadata";
static const char account_metadata_xbox[] = "Software\\Microsoft\\XboxLive";
static const char account_metadata_store[] = "Software\\Microsoft\\Windows\\CurrentVersion\\Store";

static BOOL account_metadata_id( const char *input, char output[17] )
{
    unsigned int i;
    BOOL nonzero = FALSE;
    if ( !input ) return FALSE;
    for ( i = 0; i < 16; i++ )
    {
        unsigned char c = input[i];
        if ( !c || !( ( c >= '0' && c <= '9' ) || ( c >= 'a' && c <= 'f' ) || ( c >= 'A' && c <= 'F' ) ) ) return FALSE;
        if ( c >= 'A' && c <= 'F' ) c += 'a' - 'A';
        output[i] = c;
        if ( c != '0' ) nonzero = TRUE;
    }
    output[16] = 0;
    return !input[16] && nonzero;
}

static BOOL account_metadata_owned( HKEY key )
{
    char klass[64];
    DWORD size = sizeof( klass );
    return RegQueryInfoKeyA( key, klass, &size, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL ) ==
               ERROR_SUCCESS &&
           !strcmp( klass, account_metadata_class );
}

static BOOL account_metadata_pair( const char *xbox, const char *store, char out_xbox[17], char out_store[17] )
{
    char a[17], b[17];
    out_xbox[0] = out_store[0] = 0;
    if ( !account_metadata_id( xbox, a ) || !account_metadata_id( store, b ) ) return FALSE;
    memcpy( out_xbox, a, sizeof( a ) );
    memcpy( out_store, b, sizeof( b ) );
    return TRUE;
}

static LSTATUS account_metadata_open( const char *path, HKEY *key )
{
    DWORD disposition;
    LSTATUS status = RegCreateKeyExA( HKEY_CURRENT_USER, path, 0, ( char * )account_metadata_class, REG_OPTION_VOLATILE,
                                      KEY_READ | KEY_WRITE, NULL, key, &disposition );
    if ( !status && !account_metadata_owned( *key ) )
    {
        RegCloseKey( *key );
        *key = NULL;
        return ERROR_ACCESS_DENIED;
    }
    return status;
}

static BOOL account_metadata_read( HKEY key, const char *name, char *out, DWORD size )
{
    return RegGetValueA( key, NULL, name, RRF_RT_REG_SZ, NULL, out, &size ) == ERROR_SUCCESS;
}

static BOOL account_metadata_current( HKEY key, const char *name, const char *expected, BOOL previous )
{
    char value[32];
    DWORD size = sizeof( value ), type;
    LSTATUS status = RegQueryValueExA( key, name, NULL, &type, ( BYTE * )value, &size );
    if ( status == ERROR_FILE_NOT_FOUND ) return TRUE;
    return previous && !status && type == REG_SZ && size && size <= sizeof( value ) && !value[size - 1] &&
           !strcmp( value, expected );
}

static LSTATUS account_metadata_write( HKEY key, const char *name, const char *value )
{
    return RegSetValueExA( key, name, 0, REG_SZ, ( const BYTE * )value, strlen( value ) + 1 );
}

/* An alive process retains its account view. PID creation time prevents reuse
 * from turning an old lease into authority over a different process. */
static BOOL account_metadata_process_alive( DWORD pid, ULONGLONG creation )
{
    HANDLE process = OpenProcess( PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid );
    FILETIME start, end, kernel, user;
    ULARGE_INTEGER time;
    DWORD code;
    BOOL alive = TRUE;
    if ( !process ) return GetLastError() != ERROR_INVALID_PARAMETER;
    if ( GetProcessTimes( process, &start, &end, &kernel, &user ) && GetExitCodeProcess( process, &code ) )
    {
        time.LowPart = start.dwLowDateTime;
        time.HighPart = start.dwHighDateTime;
        alive = time.QuadPart == creation && code == STILL_ACTIVE;
    }
    CloseHandle( process );
    return alive;
}

static BOOL account_metadata_live_leases( HKEY key, BOOL *live )
{
    DWORD index = 0, count = 0, name_size, data_size, type, pid;
    char name[32], *end;
    ULONGLONG creation;
    LSTATUS status;
    *live = FALSE;
    while ( count++ < 64 )
    {
        name_size = sizeof( name );
        status = RegEnumValueA( key, index, name, &name_size, NULL, &type, NULL, NULL );
        if ( status == ERROR_NO_MORE_ITEMS ) return TRUE;
        if ( status ) return FALSE;
        if ( name[0] != 'P' )
        {
            index++;
            continue;
        }
        data_size = sizeof( creation );
        status = RegQueryValueExA( key, name, NULL, &type, ( BYTE * )&creation, &data_size );
        if ( status ) return FALSE;
        pid = strtoul( name + 1, &end, 10 );
        if ( !pid || *end || type != REG_QWORD || data_size != sizeof( creation ) ) return FALSE;
        if ( account_metadata_process_alive( pid, creation ) )
        {
            *live = TRUE;
            index++;
        }
        else if ( RegDeleteValueA( key, name ) )
            return FALSE;
    }
    return FALSE;
}

static HRESULT account_metadata_publish( UINT64 xuid, const char *xbox_id, const char *store_id )
{
    struct account_metadata_record next, previous = { 0 };
    HKEY marker = NULL, xbox = NULL, store = NULL;
    HANDLE mutex = NULL;
    FILETIME start, end, kernel, user;
    ULARGE_INTEGER creation;
    char lease[32];
    DWORD saved_error = GetLastError(), wait;
    BOOL old = FALSE, live = FALSE, locked = FALSE;
    LSTATUS status = ERROR_INVALID_DATA;
    HRESULT result = S_FALSE;

    if ( !xuid || !account_metadata_id( xbox_id, next.xbox ) || !account_metadata_id( store_id, next.store ) )
        goto done;
    snprintf( next.xuid, sizeof( next.xuid ), "%llu", ( unsigned long long )xuid );
    if ( !( mutex = CreateMutexW( NULL, FALSE, L"Local\\XodusNexusAuthenticatedIdentity.v1" ) ) ) goto done;
    wait = WaitForSingleObject( mutex, 2000 );
    if ( wait != WAIT_OBJECT_0 && wait != WAIT_ABANDONED ) goto done;
    locked = TRUE;
    if ( ( status = account_metadata_open( account_metadata_marker, &marker ) ) ) goto done;
    old = account_metadata_read( marker, "Xuid", previous.xuid, sizeof( previous.xuid ) ) &&
          account_metadata_read( marker, "XboxAccountId", previous.xbox, sizeof( previous.xbox ) ) &&
          account_metadata_read( marker, "StoreAccountId", previous.store, sizeof( previous.store ) );
    if ( !account_metadata_live_leases( marker, &live ) ) goto done;
    if ( old && live &&
         ( strcmp( previous.xuid, next.xuid ) || strcmp( previous.xbox, next.xbox ) ||
           strcmp( previous.store, next.store ) ) )
        goto done;
    if ( ( status = account_metadata_open( account_metadata_xbox, &xbox ) ) ||
         ( status = account_metadata_open( account_metadata_store, &store ) ) )
        goto done;
    if ( !account_metadata_current( xbox, "Xuid", previous.xuid, old ) ||
         !account_metadata_current( xbox, "AccountId", previous.xbox, old ) ||
         !account_metadata_current( store, "PrimaryWebAccountId", previous.store, old ) )
        goto done;
    if ( !GetProcessTimes( GetCurrentProcess(), &start, &end, &kernel, &user ) ) goto done;
    creation.LowPart = start.dwLowDateTime;
    creation.HighPart = start.dwHighDateTime;
    snprintf( lease, sizeof( lease ), "P%lu", ( unsigned long )GetCurrentProcessId() );
    /* Commit Xuid last. A partial first publication remains unavailable rather
     * than falsely describing a successful identity match to legacy readers. */
    if ( ( status = account_metadata_write( marker, "Xuid", next.xuid ) ) ||
         ( status = account_metadata_write( marker, "XboxAccountId", next.xbox ) ) ||
         ( status = account_metadata_write( marker, "StoreAccountId", next.store ) ) ||
         ( status = RegSetValueExA( marker, lease, 0, REG_QWORD, ( BYTE * )&creation.QuadPart,
                                    sizeof( creation.QuadPart ) ) ) )
        goto done;
    /* Same-account child processes must not briefly remove an existing Xuid. */
    if ( old && !strcmp( previous.xuid, next.xuid ) && !strcmp( previous.xbox, next.xbox ) &&
         !strcmp( previous.store, next.store ) )
    {
        char a[21], b[17], c[17];
        if ( account_metadata_read( xbox, "Xuid", a, sizeof( a ) ) && !strcmp( a, next.xuid ) &&
             account_metadata_read( xbox, "AccountId", b, sizeof( b ) ) && !strcmp( b, next.xbox ) &&
             account_metadata_read( store, "PrimaryWebAccountId", c, sizeof( c ) ) && !strcmp( c, next.store ) )
        {
            result = S_OK;
            goto done;
        }
    }
    if ( ( status = RegDeleteValueA( xbox, "Xuid" ) ) && status != ERROR_FILE_NOT_FOUND ) goto done;
    if ( ( status = account_metadata_write( xbox, "AccountId", next.xbox ) ) ||
         ( status = account_metadata_write( store, "PrimaryWebAccountId", next.store ) ) ||
         ( status = account_metadata_write( xbox, "Xuid", next.xuid ) ) )
        goto done;
    result = S_OK;
done:
    if ( store ) RegCloseKey( store );
    if ( xbox ) RegCloseKey( xbox );
    if ( marker ) RegCloseKey( marker );
    if ( locked ) ReleaseMutex( mutex );
    if ( mutex ) CloseHandle( mutex );
    SetLastError( saved_error );
    return result;
}
