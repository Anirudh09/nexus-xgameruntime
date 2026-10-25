/* SPDX-License-Identifier: MIT
 * Actual production file-write helper under the pinned SDK CRT. Synthetic
 * files only; preserve the original transfer fixture and all its assertions. */
#define main original_transfer_fixture_main
#include "gamesave-transfer.cpp"
#undef main

static void expect_bytes( const char *path, const char *expected )
{
    BYTE *bytes = nullptr;
    SIZE_T size = 0;
    const HRESULT hr = files_read( path, &bytes, &size );
    CHECK( "exact-file-bytes", hr == S_OK && size == strlen( expected ) && !memcmp( bytes, expected, size ) );
    free( bytes );
}
struct path_write
{
    char path[FILES_PATH];
    HRESULT result;
};
static DWORD WINAPI write_parallel( void *context )
{
    auto item = static_cast<path_write *>( context );
    item->result = S_OK;
    for ( unsigned i = 0; i < 32; ++i )
    {
        item->result = files_write( item->path, reinterpret_cast<const BYTE *>( "data" ), 4 );
        if ( FAILED( item->result ) ) break;
    }
    return 0;
}
int main( void )
{
    InitializeLogging();
    WCHAR temp[MAX_PATH], previous[FILES_PATH], wide[FILES_PATH], temporary[FILES_PATH];
    char directory[FILES_PATH], path[FILES_PATH], sentinel[FILES_PATH];
    CHECK( "capture-cwd", GetCurrentDirectoryW( ARRAY_SIZE( previous ), previous ) > 0 );
    CHECK( "temp", GetTempPathW( ARRAY_SIZE( temp ), temp ) > 0 );
    CHECK( "temp-utf8", WideCharToMultiByte( CP_UTF8, 0, temp, -1, root, sizeof( root ), nullptr, nullptr ) > 0 );
    const size_t length = strlen( root );
    snprintf( root + length, sizeof( root ) - length, "nexus-file-paths-%lu-%I64u", GetCurrentProcessId(), GetTickCount64() );
    CHECK( "own-root", files_mkdir( root ) == S_OK );
    snprintf( directory, sizeof( directory ), "%s\\unrelated-cwd", root );
    CHECK( "own-cwd", files_mkdir( directory ) == S_OK );
    CHECK( "cwd-wide", files_wide( directory, wide ) == S_OK );
    CHECK( "change-cwd", SetCurrentDirectoryW( wide ) );
    snprintf( sentinel, sizeof( sentinel ), "%s\\C.tmp", directory );
    CHECK( "cwd-sentinel", write_file( sentinel, "KEEP", 4 ) == S_OK );

    snprintf( directory, sizeof( directory ), "%s\\target", root );
    CHECK( "target-directory", files_mkdir( directory ) == S_OK );
    CHECK( "target-wide", files_wide( directory, wide ) == S_OK );
    wcscat( wide, L"\\stav-\x010d.dat" );
    CHECK( "target-utf8", WideCharToMultiByte( CP_UTF8, 0, wide, -1, path, sizeof( path ), nullptr, nullptr ) > 0 );
    CHECK( "write-unicode-away-from-cwd", files_write( path, reinterpret_cast<const BYTE *>( "first" ), 5 ) == S_OK );
    expect_bytes( path, "first" );
    expect_bytes( sentinel, "KEEP" );
    wcscpy( temporary, wide );wcscat( temporary, L".tmp" );
    CHECK( "temporary-clean-after-success", GetFileAttributesW( temporary ) == INVALID_FILE_ATTRIBUTES );
    CHECK( "replace-away-from-cwd", files_write( path, reinterpret_cast<const BYTE *>( "new" ), 3 ) == S_OK );
    expect_bytes( path, "new" );
    CHECK( "empty-write", files_write( path, reinterpret_cast<const BYTE *>( "" ), 0 ) == S_OK );
    expect_bytes( path, "" );
    CHECK( "seed-readonly", write_file( path, "old", 3 ) == S_OK );
    CHECK( "readonly-on", SetFileAttributesW( wide, FILE_ATTRIBUTE_READONLY ) );
    CHECK( "readonly-replacement-fails", FAILED( files_write( path, reinterpret_cast<const BYTE *>( "bad" ), 3 ) ) );
    expect_bytes( path, "old" );
    CHECK( "temporary-clean-after-failure", GetFileAttributesW( temporary ) == INVALID_FILE_ATTRIBUTES );
    CHECK( "readonly-off", SetFileAttributesW( wide, FILE_ATTRIBUTE_NORMAL ) );
    HANDLE occupied = CreateFileW( temporary, GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr );
    DWORD written = 0;
    CHECK( "occupied-temporary", occupied != INVALID_HANDLE_VALUE && WriteFile( occupied, "USER", 4, &written, nullptr ) && written == 4 );
    if ( occupied != INVALID_HANDLE_VALUE ) CloseHandle( occupied );
    CHECK( "foreign-temporary-not-overwritten", FAILED( files_write( path, reinterpret_cast<const BYTE *>( "bad" ), 3 ) ) );
    char temporary_utf8[FILES_PATH];
    CHECK( "temporary-utf8", WideCharToMultiByte( CP_UTF8, 0, temporary, -1, temporary_utf8, sizeof( temporary_utf8 ), nullptr, nullptr ) > 0 );
    expect_bytes( temporary_utf8, "USER" );expect_bytes( path, "old" );
    CHECK( "remove-own-temporary-fixture", DeleteFileW( temporary ) );

    path_write cases[4]{};HANDLE threads[4]{};
    for ( unsigned i = 0; i < 4; ++i )
    {
        snprintf( directory, sizeof( directory ), "%s\\parallel-%u", root, i );
        CHECK( "parallel-directory", files_mkdir( directory ) == S_OK );
        snprintf( cases[i].path, sizeof( cases[i].path ), "%s\\state.dat", directory );
        threads[i] = CreateThread( nullptr, 0, write_parallel, &cases[i], 0, nullptr );
        CHECK( "parallel-thread", threads[i] != nullptr );
    }
    for ( unsigned i = 0; i < 4; ++i )
    {
        CHECK( "parallel-completes", threads[i] && WaitForSingleObject( threads[i], 15000 ) == WAIT_OBJECT_0 );
        if ( threads[i] ) CloseHandle( threads[i] );
        CHECK( "parallel-independent-cwd", cases[i].result == S_OK );expect_bytes( cases[i].path, "data" );
    }
    expect_bytes( sentinel, "KEEP" );
    char too_long[FILES_PATH + 16];memset( too_long, 'x', sizeof( too_long ) - 1 );too_long[sizeof( too_long ) - 1] = 0;
    CHECK( "long-path-rejected", FAILED( files_write( too_long, reinterpret_cast<const BYTE *>( "x" ), 1 ) ) );
    CHECK( "restore-cwd", SetCurrentDirectoryW( previous ) );
    CHECK( "closed-synthetic-transport", remote.calls == 0 );
    printf( "CHECKS %u FAILURES %u\n", checks, failures );
    return failures ? 1 : 0;
}
