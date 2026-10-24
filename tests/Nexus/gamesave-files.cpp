/* SPDX-License-Identifier: MIT
 * File-provider ABI/lifetime probe. Synthetic identity, explicit offline mode;
 * no account, network or real saves. Actual provider, child and filesystem. */
#define COBJMACROS

#include "../../src/private.h"
#include <stdio.h>
#include "xtaskqueue.h"
#include "xasyncprovider.h"
#include "xuser.h"
#include "xgamesave.h"
#include "../../src/System/User/XUser.h"
#include <new>
class OfflineTransport final : public UserTransport
{
public:
    unsigned calls = 0;
    HRESULT Backend( UINT16, const char *, UINT16, char **out, UINT16 *size ) override
    {
        ++calls;
        *out = nullptr;
        *size = 0;
        return E_ACCESSDENIED;
    }
    HRESULT Http( const WCHAR *, const WCHAR *, char *, const WCHAR *, const WCHAR **, BYTE **out,
                  SIZE_T *size ) override
    {
        ++calls;
        *out = nullptr;
        *size = 0;
        return E_ACCESSDENIED;
    }
    HRESULT HttpRaw( const WCHAR *, const WCHAR *, const WCHAR *, const void *, DWORD, DWORD *status, BYTE **out,
                     SIZE_T *size ) override
    {
        ++calls;
        *status = 403;
        *out = nullptr;
        *size = 0;
        return E_ACCESSDENIED;
    }
};
static LONG checks, failures;
#define CHECK( n, v )                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        checks++;                                                                                                      \
        if ( !( v ) )                                                                                                  \
        {                                                                                                              \
            failures++;                                                                                                \
            printf( "FAIL %s\n", n );                                                                                  \
        }                                                                                                              \
    } while ( 0 )
static LONG WINAPI exception( EXCEPTION_POINTERS *e )
{
    printf( "FAIL exception=%08lx\n", e->ExceptionRecord->ExceptionCode );
    fflush( stdout );
    ExitProcess( 77 );
    return EXCEPTION_CONTINUE_SEARCH;
}
int main( void )
{
    InitializeLogging();
    AddVectoredExceptionHandler( 1, exception );
    SetEnvironmentVariableA( "XODUS_SAVE_SYNC", "none" );
    HMODULE dll = LoadLibraryA( "xgameruntime.dll" );
    using Query = HRESULT( WINAPI * )( REFCLSID, REFIID, void ** );
    Query query = dll ? reinterpret_cast<Query>( GetProcAddress( dll, "QueryApiImpl" ) ) : nullptr;
    IXThreadingImpl *threads = NULL;
    IXGameSaveImpl3 *saves = NULL;
    CHECK( "interfaces", query &&
                             SUCCEEDED( query( CLSID_XThreadingImpl, IID_IXThreadingImpl, ( void ** )&threads ) ) &&
                             SUCCEEDED( query( CLSID_XGameSaveImpl, IID_IXGameSaveImpl3, ( void ** )&saves ) ) );
    if ( !threads || !saves ) return 1;
    OfflineTransport transport;
    auto user = new ( std::nothrow ) XUser( transport );
    CHECK( "actual-cpp-user", user != nullptr );
    if ( !user ) return 1;
    user->ref = 1;
    user->xuid = 42;
    user->initDone = 1;
    user->initResult = S_OK;
    const char *scid = "00000000-0000-0000-0000-000000000042";
    XTaskQueueHandle queue = NULL;
    XAsyncBlock block = { 0 };
    CHECK( "queue", SUCCEEDED( ( threads )->XTaskQueueCreate( XTaskQueueDispatchMode::Manual,
                                                              XTaskQueueDispatchMode::Manual, &queue ) ) );
    block.queue = queue;
    CHECK( "null-user", ( saves )->XGameSaveFilesGetFolderWithUiAsync( NULL, scid, &block ) == E_INVALIDARG );
    CHECK( "invalid-scid", ( saves )->XGameSaveFilesGetFolderWithUiAsync( user, "../escape", &block ) == E_INVALIDARG );
    CHECK( "begin", SUCCEEDED( ( saves )->XGameSaveFilesGetFolderWithUiAsync( user, scid, &block ) ) );
    ULONGLONG start = GetTickCount64();
    CHECK( "work-dispatch", ( threads )->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 0 ) );
    CHECK( "nonblocking-dispatch", GetTickCount64() - start < 200 );
    HRESULT hr = E_PENDING;
    for ( unsigned i = 0; i < 200 && hr == E_PENDING; i++ )
    {
        Sleep( 50 );
        hr = ( threads )->XAsyncGetStatus( &block, FALSE );
    }
    printf( "FOLDER_STATUS %08lx\n", hr );
    CHECK( "folder-ready", hr == S_OK );
    char folder[2048] = { 0 };
    CHECK( "small-buffer", FAILED( ( saves )->XGameSaveFilesGetFolderWithUiResult( &block, 1, folder ) ) );
    CHECK( "folder-result",
           SUCCEEDED( ( saves )->XGameSaveFilesGetFolderWithUiResult( &block, sizeof( folder ), folder ) ) );
    CHECK( "canonical-user-namespace", strstr( folder, "\\XodusGameSaveFiles\\42\\" ) != nullptr );
    char expectedScid[73]{};
    constexpr char digits[] = "0123456789abcdef";
    for ( unsigned i = 0; i < 36; ++i )
    {
        const auto c = static_cast<unsigned char>( scid[i] );
        expectedScid[2 * i] = digits[c >> 4];
        expectedScid[2 * i + 1] = digits[c & 15];
    }
    const auto suffix = strrchr( folder, '\\' );
    CHECK( "canonical-scid-encoding", suffix && !strcmp( suffix + 1, expectedScid ) );
    WCHAR wide[2048], child[2048];
    CHECK( "UTF8", MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, folder, -1, wide, 2048 ) > 0 );
    CHECK( "directory", GetFileAttributesW( wide ) != INVALID_FILE_ATTRIBUTES );
    swprintf( child, 2048, L"%ls\\Save1", wide );
    CHECK( "container", CreateDirectoryW( child, NULL ) );
    swprintf( child, 2048, L"%ls\\Save1\\Nested", wide );
    CHECK( "nested-container", CreateDirectoryW( child, NULL ) );
    swprintf( child, 2048, L"%ls\\Save1\\Nested\\stav-\x010d.dat", wide );
    HANDLE file = CreateFileW( child, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL );
    DWORD written = 0;
    CHECK( "unicode-file",
           file != INVALID_HANDLE_VALUE && WriteFile( file, "test", 4, &written, NULL ) && written == 4 );
    if ( file != INVALID_HANDLE_VALUE ) CloseHandle( file );
    INT64 quota = 0;
    CHECK( "actual-quota", SUCCEEDED( ( saves )->XGameSaveFilesGetRemainingQuota( user, scid, &quota ) ) &&
                               quota == 256LL * 1024 * 1024 - 4 );
    swprintf( child, 2048, L"%ls\\ignored.txt", wide );
    file = CreateFileW( child, GENERIC_WRITE, 0, NULL, CREATE_NEW, 0, NULL );
    if ( file != INVALID_HANDLE_VALUE )
    {
        WriteFile( file, "ignore", 6, &written, NULL );
        CloseHandle( file );
    }
    CHECK( "root-files-ignored", SUCCEEDED( ( saves )->XGameSaveFilesGetRemainingQuota( user, scid, &quota ) ) &&
                                     quota == 256LL * 1024 * 1024 - 4 );
    ZeroMemory( &block, sizeof( block ) );
    block.queue = queue;
    char again[2048];
    CHECK( "repeat-begin", SUCCEEDED( ( saves )->XGameSaveFilesGetFolderWithUiAsync( user, scid, &block ) ) );
    ( threads )->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 0 );
    CHECK( "repeat-status", ( threads )->XAsyncGetStatus( &block, TRUE ) == S_OK );
    CHECK( "stable-folder",
           SUCCEEDED( ( saves )->XGameSaveFilesGetFolderWithUiResult( &block, sizeof( again ), again ) ) &&
               !strcmp( folder, again ) );
    /* Completion may wake the caller before the worker drops its last context
     * reference. Assert eventual cleanup, not a scheduler timing accident. */
    for ( unsigned i = 0; i < 1000 && InterlockedCompareExchange( &user->ref, 0, 0 ) != 1; i++ )
        Sleep( 1 );
    CHECK( "retained-user-released", InterlockedCompareExchange( &user->ref, 0, 0 ) == 1 );
    ( threads )->XTaskQueueCloseHandle( queue );
    CHECK( "no-network", transport.calls == 0 );
    user->Release();
    printf( "CHECKS %ld FAILURES %ld\n", checks, failures );
    return failures ? 1 : 0;
}
