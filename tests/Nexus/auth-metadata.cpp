// SPDX-License-Identifier: LGPL-2.1-or-later
// Actual C++ logger with synthetic token-presence flags, private files only.
#include "../../src/System/XUser.cpp"
static LONG checks, failures;
#define CHECK( n, v )                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if ( !( v ) )                                                                                                  \
        {                                                                                                              \
            ++failures;                                                                                                \
            printf( "FAIL %s line=%u\n", n, __LINE__ );                                                                \
        }                                                                                                              \
    } while ( 0 )
static char log_path[MAX_PATH], output[32769];
class ClosedMetadataTransport final : public UserTransport
{
public:
    HRESULT Backend( UINT16, const char *, UINT16, char **, UINT16 * ) override
    {
        return E_ACCESSDENIED;
    }
    HRESULT Http( const WCHAR *, const WCHAR *, char *, const WCHAR *, const WCHAR **, BYTE **, SIZE_T * ) override
    {
        return E_ACCESSDENIED;
    }
    HRESULT HttpRaw( const WCHAR *, const WCHAR *, const WCHAR *, const void *, DWORD, DWORD *, BYTE **,
                     SIZE_T * ) override
    {
        return E_ACCESSDENIED;
    }
};
static void reset_log( void )
{
    HANDLE file =
        CreateFileA( log_path, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL );
    CHECK( "precreate private log", file != INVALID_HANDLE_VALUE );
    if ( file != INVALID_HANDLE_VALUE ) CloseHandle( file );
}
static DWORD read_log( void )
{
    DWORD size = 0;
    HANDLE file = CreateFileA( log_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, NULL );
    output[0] = 0;
    CHECK( "open private log", file != INVALID_HANDLE_VALUE );
    if ( file == INVALID_HANDLE_VALUE ) return 0;
    CHECK( "read private log", ReadFile( file, output, sizeof( output ) - 1, &size, NULL ) );
    CloseHandle( file );
    output[size] = 0;
    CHECK( "bounded to 32 KiB", size <= 32768 );
    CHECK( "no credential content", !strstr( output, "SECRET" ) && !strstr( output, "TICKET" ) &&
                                        !strstr( output, "https:" ) && !strstr( output, "Authorization" ) );
    return size;
}
static DWORD WINAPI writer( void *arg )
{
    unsigned i;
    ( void )arg;
    for ( i = 0; i < 1000; ++i )
        auth_metadata( "xsts", NULL, E_ACCESSDENIED, 401, FALSE, FALSE );
    return 0;
}
int main( void )
{
    InitializeLogging();
    ClosedMetadataTransport transport;
    XUser user( transport );
    char tmp[MAX_PATH], long_path[MAX_PATH + 32];
    DWORD size, written;
    HANDLE file, threads[4];
    unsigned i, n;
    HRESULT hr;
    GetTempPathA( sizeof( tmp ), tmp );
    GetTempFileNameA( tmp, "aut", 0, log_path );
    DeleteFileA( log_path );
    SetEnvironmentVariableA( "XUSER_AUTH_METADATA_LOG", NULL );
    SetLastError( 1234 );
    auth_metadata( "xsts", NULL, E_FAIL, 0, FALSE, FALSE );
    CHECK( "disabled preserves last error", GetLastError() == 1234 );
    SetEnvironmentVariableA( "XUSER_AUTH_METADATA_LOG", log_path );
    auth_metadata( "xsts", NULL, E_FAIL, 0, FALSE, FALSE );
    CHECK( "never creates log implicitly", GetFileAttributesA( log_path ) == INVALID_FILE_ATTRIBUTES );
    reset_log();
    SetLastError( 4567 );
    auth_metadata( "SECRET\nxsts", NULL, E_FAIL, 0, FALSE, FALSE );
    CHECK( "invalid phase preserves error", GetLastError() == 4567 );
    auth_metadata( NULL, NULL, E_FAIL, 0, FALSE, FALSE );
    CHECK( "unknown phase cannot inject text", read_log() == 0 );
    user.deviceToken = const_cast<char *>( "SECRET_DEVICE" );
    user.userToken = const_cast<char *>( "SECRET_USER" );
    user.titleToken = const_cast<char *>( "SECRET_TITLE" );
    SetLastError( 7890 );
    auth_metadata( "xsts", &user, E_ACCESSDENIED, 403, TRUE, FALSE );
    CHECK( "enabled preserves last error", GetLastError() == 7890 );
    read_log();
    CHECK( "numeric status and flags only",
           strstr( output,
                   "phase=xsts hr=80070005 http=403 device=1 user=1 title=1 requested_title=1 actual_title=0 tick=" ) );
    reset_log();
    memset( long_path, 'x', sizeof( long_path ) - 1 );
    long_path[sizeof( long_path ) - 1] = 0;
    SetEnvironmentVariableA( "XUSER_AUTH_METADATA_LOG", long_path );
    auth_metadata( "xsts", NULL, S_OK, 200, TRUE, TRUE );
    SetEnvironmentVariableA( "XUSER_AUTH_METADATA_LOG", log_path );
    CHECK( "oversized path ignored", read_log() == 0 );
    file = CreateFileA( log_path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    CHECK( "hold log exclusively", file != INVALID_HANDLE_VALUE );
    SetLastError( 3456 );
    auth_metadata( "xsts", NULL, E_FAIL, 0, FALSE, FALSE );
    CHECK( "busy log preserves caller error", GetLastError() == 3456 );
    CloseHandle( file );
    CHECK( "busy log skips writing", read_log() == 0 );
    for ( i = 0; i < 4; ++i )
    {
        threads[i] = CreateThread( NULL, 0, writer, NULL, 0, NULL );
        CHECK( "writer created", threads[i] != NULL );
    }
    CHECK( "concurrent writers finish", WaitForMultipleObjects( 4, threads, TRUE, 10000 ) == WAIT_OBJECT_0 );
    for ( i = 0; i < 4; ++i )
        CloseHandle( threads[i] );
    size = read_log();
    CHECK( "concurrent bounded complete lines", size && size <= 32768 && output[size - 1] == '\n' );
    for ( n = 0, i = 0; i < size; ++i )
        if ( output[i] == '\n' ) ++n;
    CHECK( "concurrent records present", n > 100 );
    {
        char *line = strtok( output, "\n" );
        while ( line )
        {
            unsigned long tick, thread;
            int end = 0;
            CHECK( "concurrent record not interleaved",
                   sscanf( line,
                           "AUTH_META phase=xsts hr=80070005 http=401 device=0 user=0 title=0 requested_title=0 "
                           "actual_title=0 tick=%lu thread=%lu%n",
                           &tick, &thread, &end ) == 2 &&
                       end > 0 && !line[end] );
            line = strtok( NULL, "\n" );
        }
    }
    /* Exact-capacity file must never be truncated, wrapped or appended. */
    reset_log();
    file = CreateFileA( log_path, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
    memset( output, 'X', 32768 );
    WriteFile( file, output, 32768, &written, NULL );
    CloseHandle( file );
    auth_metadata( "xsts", NULL, S_OK, 200, TRUE, TRUE );
    CHECK( "full file untouched", read_log() == 32768 && output[0] == 'X' && output[32767] == 'X' );
    SetEnvironmentVariableA( "XUSER_AUTH_METADATA_LOG", NULL );
    DeleteFileA( log_path );
    DeleteCriticalSection( &user.deviceAuthSection );
    printf( "CHECKS %ld FAILURES %ld\n", checks, failures );
    return failures ? 1 : 0;
}
