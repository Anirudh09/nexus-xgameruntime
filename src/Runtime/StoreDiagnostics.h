/* SPDX-License-Identifier: LGPL-2.1-or-later
 * Opt-in local diagnostic metadata only. No account IDs, handles, strings,
 * credentials, request bodies or URLs are accepted by this interface. */
void nexus_store_metadata( unsigned int api, HRESULT hr, unsigned int flags, const GUID *iid, const void *caller )
{
    static SRWLOCK lock = SRWLOCK_INIT;
    DWORD saved_error = GetLastError(), length, written;
    char file_name[MAX_PATH], module_name[81] = "unknown", line[384];
    WCHAR module_path[32768], *base;
    HMODULE module;
    HANDLE file;
    LARGE_INTEGER size, end;
    BY_HANDLE_FILE_INFORMATION info;
    OVERLAPPED range = { 0 };
    ULONGLONG rva = 0;
    GUID empty = { 0 };
    unsigned int i;

    if ( api < 1 || api > 33 || flags > 15 ) goto done;
    length = GetEnvironmentVariableA( "NEXUS_STORE_METADATA_LOG", file_name, sizeof( file_name ) );
    if ( !length || length >= sizeof( file_name ) ) goto done;
    if ( caller &&
         GetModuleHandleExW( GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             ( const WCHAR * )caller, &module ) )
    {
        rva = ( ULONG_PTR )caller - ( ULONG_PTR )module;
        length = GetModuleFileNameW( module, module_path, ARRAY_SIZE( module_path ) );
        if ( length && length < ARRAY_SIZE( module_path ) )
        {
            base = wcsrchr( module_path, '\\' );
            base = base ? base + 1 : module_path;
            for ( i = 0; base[i] && i < sizeof( module_name ) - 1; i++ )
            {
                if ( !( ( base[i] >= 'A' && base[i] <= 'Z' ) || ( base[i] >= 'a' && base[i] <= 'z' ) ||
                        ( base[i] >= '0' && base[i] <= '9' ) || base[i] == '.' || base[i] == '_' || base[i] == '-' ) )
                    break;
            }
            if ( !base[i] )
            {
                for ( i = 0; base[i]; i++ )
                    module_name[i] = base[i];
                module_name[i] = 0;
            }
        }
    }
    if ( !iid ) iid = &empty;
    length = snprintf( line, sizeof( line ),
                       "STORE_META api=%u hr=%08lx flags=%u iid=%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x "
                       "module=%s rva=%llx tick=%lu thread=%lu\n",
                       api, ( unsigned long )( UINT32 )hr, flags, ( unsigned long )iid->Data1, iid->Data2, iid->Data3,
                       iid->Data4[0], iid->Data4[1], iid->Data4[2], iid->Data4[3], iid->Data4[4], iid->Data4[5],
                       iid->Data4[6], iid->Data4[7], module_name, ( unsigned long long )rva,
                       ( unsigned long )GetTickCount(), ( unsigned long )GetCurrentThreadId() );
    if ( length >= sizeof( line ) ) goto done;
    if ( !TryAcquireSRWLockExclusive( &lock ) ) goto done;
    file = CreateFileA( file_name, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, NULL );
    if ( file != INVALID_HANDLE_VALUE )
    {
        if ( GetFileType( file ) == FILE_TYPE_DISK && GetFileInformationByHandle( file, &info ) &&
             !( info.dwFileAttributes & ( FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT ) ) &&
             LockFileEx( file, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, MAXDWORD, MAXDWORD, &range ) )
        {
            if ( GetFileSizeEx( file, &size ) && size.QuadPart >= 0 && size.QuadPart + length <= 65536 )
            {
                end.QuadPart = 0;
                if ( SetFilePointerEx( file, end, NULL, FILE_END ) ) WriteFile( file, line, length, &written, NULL );
            }
            UnlockFileEx( file, 0, MAXDWORD, MAXDWORD, &range );
        }
        CloseHandle( file );
    }
    ReleaseSRWLockExclusive( &lock );
done:
    SetLastError( saved_error );
}
