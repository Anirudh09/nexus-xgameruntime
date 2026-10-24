// SPDX-License-Identifier: LGPL-2.1-or-later
// The actual C++ implementation is compiled here to exercise its internal
// providers with closed transports. No alternate runtime or exported test hook.
#include "../../src/System/XUser.cpp"
#include <memory>
static LONG checks, failures;
#define CHECK( name, value )                                                                                           \
    do                                                                                                                 \
    {                                                                                                                  \
        ++checks;                                                                                                      \
        if ( !( value ) )                                                                                              \
        {                                                                                                              \
            ++failures;                                                                                                \
            printf( "FAIL %s line=%u\n", name, __LINE__ );                                                             \
        }                                                                                                              \
    } while ( 0 )

static HRESULT adapter_transport_hr, adapter_init_hr;
static DWORD adapter_status;
static const char *adapter_response;
static UINT adapter_requests;
class PrivilegeTransport final : public UserTransport
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
    HRESULT HttpRaw( const WCHAR *method, const WCHAR *url, const WCHAR *headers, const void *data, DWORD size,
                     DWORD *status, BYTE **buffer, SIZE_T *length ) override
    {
        const char *with_title =
            "{\"RelyingParty\":\"http://"
            "xboxlive.com\",\"TokenType\":\"JWT\",\"Properties\":{\"SandboxId\":\"RETAIL\",\"DeviceToken\":\"device\","
            "\"TitleToken\":\"title\",\"UserTokens\":[\"user\"],\"ProofKey\":{\"kty\":\"EC\"}}}";
        const char *without_title =
            "{\"RelyingParty\":\"http://"
            "xboxlive.com\",\"TokenType\":\"JWT\",\"Properties\":{\"SandboxId\":\"RETAIL\",\"DeviceToken\":\"device\","
            "\"UserTokens\":[\"user\"],\"ProofKey\":{\"kty\":\"EC\"}}}";
        const char *body = static_cast<const char *>( data );
        ++adapter_requests;
        CHECK( "adapter-method-url",
               !wcscmp( method, L"POST" ) && !wcscmp( url, L"https://xsts.auth.xboxlive.com/xsts/authorize" ) );
        CHECK( "adapter-no-fabricated-authorization", !wcsstr( headers, L"Authorization:" ) );
        CHECK( "adapter-contract-header",
               wcsstr( headers, L"x-xbl-contract-version: 1\r\nContent-Type: application/json\r\n" ) &&
                   wcsstr( headers, L"Signature: " ) );
        CHECK( "adapter-accepted-request-shape", !strcmp( body, with_title ) || !strcmp( body, without_title ) );
        CHECK( "adapter-exact-body-size", size == strlen( body ) );
        if ( FAILED( adapter_transport_hr ) ) return adapter_transport_hr;
        *status = adapter_status;
        *length = strlen( adapter_response );
        *buffer = static_cast<BYTE *>( malloc( *length + 1 ) );
        if ( !*buffer ) return E_OUTOFMEMORY;
        memcpy( *buffer, adapter_response, *length + 1 );
        return S_OK;
    }
};
static void test_server_adapter( void )
{
    PrivilegeTransport transport;
    auto release = []( XUser *p ) { p->Release(); };
    std::unique_ptr<XUser, decltype( release )> owner( new XUser( transport ), release );
    auto &user = *owner;
    user.ref = 1;
    CHECK( "adapter-key-initialization", user.Initialize( XUserAddOptions::AddDefaultUserSilently ) == S_OK );
    free( user.proofKey );
    user.proofKey = strdup( "{\"kty\":\"EC\"}" );
    user.deviceToken = strdup( "device" );
    user.userToken = strdup( "user" );
    user.titleToken = strdup( "title" );
    user.xuid = 123;
    user.initDone = 1;
    user.initResult = S_OK;
    const struct
    {
        const char *response;
        HRESULT hr;
    } cases[] = {
        { "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"synthetic\",\"xid\":\"123\",\"prv\":\"185 "
          "254 252\"}]}}",
          S_OK },
        { "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"synthetic\",\"xid\":\"123\",\"prv\":\"185 "
          "252\"}]}}",
          E_GAMEUSER_FAILED_TO_RESOLVE },
        { "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"synthetic\",\"xid\":\"124\",\"prv\":\"254\"}"
          "]}}",
          E_GAMEUSER_NO_AUTH_USER },
    };
    const char *invalid[] = {
        "{}",
        "invalid",
        "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[]}}",
        "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"synthetic\",\"xid\":\"123\"}]}}",
        "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"synthetic\",\"xid\":\"123\",\"prv\":[254]}]}}",
        "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"synthetic\",\"xid\":\"123\",\"prv\":"
        "\"254bad\"}]}}",
    };
    UINT i, before;
    char *auth = NULL;
    adapter_status = 200;
    for ( i = 0; i < sizeof( cases ) / sizeof( *cases ); ++i )
    {
        adapter_response = cases[i].response;
        CHECK( "adapter-real-verifier",
               user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) == cases[i].hr );
    }
    for ( i = 0; i < sizeof( invalid ) / sizeof( *invalid ); ++i )
    {
        adapter_response = invalid[i];
        CHECK( "adapter-rejects-invalid-response",
               FAILED( user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) ) );
    }
    adapter_response = cases[0].response;
    adapter_status = 400;
    CHECK( "adapter-http-400-not-granted",
           user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) == E_FAIL );
    adapter_status = 403;
    CHECK( "adapter-http-403-not-granted",
           user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) == E_FAIL );
    adapter_status = 200;
    adapter_transport_hr = HRESULT_FROM_WIN32( ERROR_TIMEOUT );
    CHECK( "adapter-network-failure",
           user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) == adapter_transport_hr );
    adapter_transport_hr = S_OK;
    free( user.titleToken );
    user.titleToken = NULL;
    CHECK( "adapter-no-title-token-path", user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) == S_OK );
    user.titleToken = strdup( "title" );
    before = adapter_requests;
    free( user.userToken );
    user.userToken = NULL;
    CHECK( "adapter-no-auth-user",
           user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) == E_GAMEUSER_NO_AUTH_USER &&
               adapter_requests == before );
    user.userToken = strdup( "user" );
    adapter_init_hr = E_FAIL;
    user.initResult = E_FAIL;
    CHECK( "adapter-initialization-failure",
           user_verify_privilege( &user, static_cast<XUserPrivilege>( 254 ) ) == E_FAIL && adapter_requests == before );
    adapter_init_hr = S_OK;
    user.initResult = S_OK;
    /* Other token callers do not require privilege claims or change format. */
    adapter_response = "{\"Token\":\"synthetic\",\"DisplayClaims\":{\"xui\":[{\"uhs\":\"synthetic\"}]}}";
    CHECK( "ordinary-auth-without-privileges", xsts_authorize( &user, "http://xboxlive.com", TRUE, &auth ) == S_OK &&
                                                   auth && !strcmp( auth, "XBL3.0 x=synthetic;synthetic" ) );
    free( auth );
}
struct fixture_user : public XUser
{
    LONG calls{};
    HANDLE entered{}, gate{};
    DWORD thread{};
    HRESULT result{};
    fixture_user() : XUser( Closed() )
    {
        ref = 1;
    }
    ~fixture_user()
    {
        DeleteCriticalSection( &deviceAuthSection );
    }
    static UserTransport &Closed()
    {
        static PrivilegeTransport value;
        return value;
    }
};
static HRESULT verify( XUser *iface, XUserPrivilege privilege )
{
    auto user = static_cast<fixture_user *>( iface );
    BOOLEAN granted;
    HRESULT hr;
    user->thread = GetCurrentThreadId();
    InterlockedIncrement( &user->calls );
    SetEvent( user->entered );
    WaitForSingleObject( user->gate, 4000 );
    if ( FAILED( user->result ) ) return user->result;
    hr = privilege_claim( L"123", 3, 123, L"185 254 252", 11, static_cast<XUserPrivilege>( privilege ), &granted );
    return FAILED( hr ) ? hr : granted ? S_OK : E_GAMEUSER_FAILED_TO_RESOLVE;
}
static void test_claims( void )
{
    const struct
    {
        const WCHAR *prv;
        HRESULT hr;
        BOOLEAN granted;
    } cases[] = {
        { L"185 254 252", S_OK, TRUE },
        { L"254", S_OK, TRUE },
        { L"  254   ", S_OK, TRUE },
        { L"254 254", S_OK, TRUE },
        { L"0254", S_OK, TRUE },
        { L"", S_OK, FALSE },
        { L" ", S_OK, FALSE },
        { L"185 252", S_OK, FALSE },
        { L"1254 2540 25 4", S_OK, FALSE },
        { L"4294967295 254", S_OK, TRUE },
        { L"254x", HRESULT_FROM_WIN32( ERROR_INVALID_DATA ), FALSE },
        { L"254 -1", HRESULT_FROM_WIN32( ERROR_INVALID_DATA ), FALSE },
        { L"254 4294967296", HRESULT_FROM_WIN32( ERROR_INVALID_DATA ), FALSE },
        { L"+254", HRESULT_FROM_WIN32( ERROR_INVALID_DATA ), FALSE },
        { L"254\t252", HRESULT_FROM_WIN32( ERROR_INVALID_DATA ), FALSE },
        { L"254,252", HRESULT_FROM_WIN32( ERROR_INVALID_DATA ), FALSE },
    };
    static const WCHAR *bad_xids[] = { L"", L"-123", L"123x", L"123 ", L"18446744073709551616" };
    BOOLEAN granted;
    UINT i;
    HRESULT hr;
    for ( i = 0; i < sizeof( cases ) / sizeof( *cases ); ++i )
    {
        granted = 0x7f;
        hr = privilege_claim( L"123", 3, 123, cases[i].prv, wcslen( cases[i].prv ),
                              static_cast<XUserPrivilege>( XUserPrivilege::Multiplayer ), &granted );
        CHECK( "claim-status", hr == cases[i].hr );
        CHECK( "claim-grant", granted == cases[i].granted );
    }
    for ( i = 0; i < sizeof( bad_xids ) / sizeof( *bad_xids ); ++i )
    {
        CHECK( "bad-identity", FAILED( privilege_claim( bad_xids[i], wcslen( bad_xids[i] ), 123, L"254", 3,
                                                        static_cast<XUserPrivilege>( 254 ), &granted ) ) &&
                                   !granted );
    }
    CHECK( "other-user", privilege_claim( L"124", 3, 123, L"254", 3, static_cast<XUserPrivilege>( 254 ), &granted ) ==
                                 E_GAMEUSER_NO_AUTH_USER &&
                             !granted );
    CHECK( "zero-user",
           FAILED( privilege_claim( L"0", 1, 0, L"254", 3, static_cast<XUserPrivilege>( 254 ), &granted ) ) &&
               !granted );
    CHECK( "missing-claim",
           FAILED( privilege_claim( L"123", 3, 123, NULL, 0, static_cast<XUserPrivilege>( 254 ), &granted ) ) &&
               !granted );
    CHECK( "embedded-nul",
           FAILED( privilege_claim( L"123", 3, 123, L"254\0 252", 8, static_cast<XUserPrivilege>( 254 ), &granted ) ) &&
               !granted );
    CHECK( "oversized-claim",
           FAILED( privilege_claim( L"123", 3, 123, L"254", 16385, static_cast<XUserPrivilege>( 254 ), &granted ) ) &&
               !granted );
    CHECK( "invalid-privilege",
           FAILED( privilege_claim( L"123", 3, 123, L"0", 1, static_cast<XUserPrivilege>( 0 ), &granted ) ) &&
               !granted );
    CHECK( "max-xuid", privilege_claim( L"18446744073709551615", 20, ~( UINT64 )0, L"254", 3,
                                        static_cast<XUserPrivilege>( 254 ), &granted ) == S_OK &&
                           granted );
    CHECK( "null-output",
           privilege_claim( L"123", 3, 123, L"254", 3, static_cast<XUserPrivilege>( 254 ), NULL ) == E_INVALIDARG );
    for ( i = 0; i < 512; ++i )
        CHECK( "known-enum-only",
               privilege_valid( ( XUserPrivilege )i ) == ( i == 185 || i == 188 || i == 189 || i == 190 || i == 196 ||
                                                           i == 198 || i == 203 || i == 207 || i == 208 || i == 209 ||
                                                           i == 220 || i == 247 || i == 252 || i == 254 || i == 255 ) );
}
struct completion_context
{
    LONG callbacks;
    HRESULT result;
    IXThreadingImpl *threads;
    BOOL dispose;
};
static void WINAPI complete( XAsyncBlock *block )
{
    auto context = static_cast<completion_context *>( block->context );
    context->result = privilege_result( context->threads, block );
    InterlockedIncrement( &context->callbacks );
    if ( context->dispose )
    {
        memset( block, 0xdd, sizeof( *block ) );
        free( block );
    }
}
static void drain( IXThreadingImpl *threads, XTaskQueueHandle queue, struct fixture_user *user,
                   struct completion_context *context )
{
    ULONGLONG start = GetTickCount64();
    while ( ( user->ref != 1 || !context->callbacks ) && GetTickCount64() - start < 5000 )
    {
        ( threads )->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 0 );
        Sleep( 1 );
    }
}
static void test_async( IXThreadingImpl *threads, XTaskQueueDispatchMode mode, UINT test )
{
    fixture_user user;
    struct completion_context context = { 0 };
    XTaskQueueHandle queue = NULL;
    XAsyncBlock block = { 0 };
    HRESULT expected = test == 1   ? E_GAMEUSER_FAILED_TO_RESOLVE
                       : test == 2 ? E_FAIL
                       : test == 3 ? E_GAMEUSER_NO_AUTH_USER
                                   : S_OK;
    BOOL manual = mode == XTaskQueueDispatchMode::Manual, canceled = test >= 4;
    ULONGLONG start;
    user.ref = 1;
    user.result = test == 2 ? E_FAIL : test == 3 ? E_GAMEUSER_NO_AUTH_USER : S_OK;
    user.entered = CreateEventA( NULL, TRUE, FALSE, NULL );
    user.gate = CreateEventA( NULL, TRUE, FALSE, NULL );
    context.threads = threads;
    CHECK( "create-queue", ( threads )->XTaskQueueCreate( mode, XTaskQueueDispatchMode::Manual, &queue ) == S_OK );
    block.queue = queue;
    block.callback = complete;
    block.context = &context;
    CHECK( "begin", privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( test & 1 ? 1 : 0 ),
                                     static_cast<XUserPrivilege>( test == 1 ? 247 : 254 ), &block, verify ) == S_OK );
    CHECK( "pending-result", privilege_result( threads, &block ) == E_PENDING );
    CHECK( "reject-pending-reuse",
           privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( 0 ), static_cast<XUserPrivilege>( 254 ),
                            &block, verify ) == E_INVALIDARG );
    if ( test == 4 && manual )
    {
        ( threads )->XAsyncCancel( &block );
    }
    else
    {
        start = GetTickCount64();
        if ( manual ) CHECK( "manual-work", ( threads )->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 0 ) );
        CHECK( "dispatch-not-blocking", GetTickCount64() - start < 250 );
        CHECK( "verification-started", WaitForSingleObject( user.entered, 2500 ) == WAIT_OBJECT_0 );
        CHECK( "verification-not-caller-thread", user.thread != GetCurrentThreadId() );
        CHECK( "user-retained", user.ref == 2 );
        if ( canceled ) ( threads )->XAsyncCancel( &block );
    }
    if ( canceled )
    {
        CHECK( "abort-result", privilege_result( threads, &block ) == E_ABORT );
        while ( ( threads )->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 0 ) )
            ;
        if ( !( test == 4 && manual ) ) CHECK( "retained-until-worker-ends", user.ref == 2 );
    }
    SetEvent( user.gate );
    if ( test == 4 && manual ) ( threads )->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 0 );
    drain( threads, queue, &user, &context );
    CHECK( "one-completion", context.callbacks == 1 );
    CHECK( "real-result", context.result == ( canceled ? E_ABORT : expected ) );
    CHECK( "poll-result", privilege_result( threads, &block ) == context.result );
    CHECK( "reference-balanced", user.ref == 1 );
    CHECK( "verify-once", user.calls == ( test == 4 && manual ? 0 : 1 ) );
    /* The same block can be reused after completion without inheriting a grant. */
    context.callbacks = 0;
    user.result = E_FAIL;
    CHECK( "completed-reuse", privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( 0 ),
                                               static_cast<XUserPrivilege>( 254 ), &block, verify ) == S_OK );
    if ( manual ) ( threads )->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 0 );
    drain( threads, queue, &user, &context );
    CHECK( "reuse-real-error", context.callbacks == 1 && context.result == E_FAIL && user.ref == 1 );
    ( threads )->XTaskQueueCloseHandle( queue );
    CloseHandle( user.entered );
    CloseHandle( user.gate );
}
static void test_lifetime( IXThreadingImpl *threads )
{
    UINT i;
    for ( i = 0; i < 24; ++i )
    {
        fixture_user user;
        struct completion_context context = { 0 };
        XTaskQueueHandle queue = NULL;
        XAsyncBlock *block = static_cast<std::remove_reference_t<decltype( block )>>( calloc( 1, sizeof( *block ) ) );
        ULONGLONG start;
        user.ref = 1;
        user.entered = CreateEventA( NULL, TRUE, FALSE, NULL );
        user.gate = CreateEventA( NULL, TRUE, FALSE, NULL );
        context.threads = threads;
        context.dispose = TRUE;
        CHECK( "immediate-queue", ( threads )->XTaskQueueCreate( XTaskQueueDispatchMode::Immediate,
                                                                 XTaskQueueDispatchMode::Immediate, &queue ) == S_OK );
        block->queue = queue;
        block->context = &context;
        block->callback = complete;
        CHECK( "immediate-begin", privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( 0 ),
                                                   static_cast<XUserPrivilege>( 254 ), block, verify ) == S_OK );
        CHECK( "immediate-worker-ready", WaitForSingleObject( user.entered, 2500 ) == WAIT_OBJECT_0 );
        /* The operation must retain the queue after the caller closes it.
         * Completion consumes/frees the caller block while the worker lives. */
        ( threads )->XTaskQueueCloseHandle( queue );
        if ( i & 1 ) ( threads )->XAsyncCancel( block );
        SetEvent( user.gate );
        start = GetTickCount64();
        while ( ( user.ref != 1 || !context.callbacks ) && GetTickCount64() - start < 5000 )
            Sleep( 1 );
        CHECK( "closed-queue-completion", context.callbacks == 1 && context.result == ( i & 1 ? E_ABORT : S_OK ) );
        CHECK( "freed-caller-block-safe", user.ref == 1 && user.calls == 1 );
        CloseHandle( user.entered );
        CloseHandle( user.gate );
    }
}
int main( void )
{
    InitializeLogging();
    HMODULE dll = LoadLibraryA( "xgameruntime.dll" );
    HRESULT( WINAPI * query )( REFCLSID, REFIID, void ** );
    IXThreadingImpl *threads;
    IXUserImpl *users;
    XAsyncBlock block = { 0 };
    fixture_user user;
    UINT mode, test;
    if ( !dll || !( query = reinterpret_cast<decltype( query )>( GetProcAddress( dll, "QueryApiImpl" ) ) ) ||
         FAILED( query( CLSID_XThreadingImpl, IID_IXThreadingImpl, ( void ** )&threads ) ) ||
         FAILED( query( CLSID_XUserImpl, IID_IXUserImpl, ( void ** )&users ) ) )
        return 2;
    user.ref = 1;
    test_claims();
    test_server_adapter();
    CHECK( "null-user", privilege_begin( threads, NULL, static_cast<XUserPrivilegeOptions>( 0 ),
                                         static_cast<XUserPrivilege>( 254 ), &block, verify ) == E_INVALIDARG );
    CHECK( "null-block", privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( 0 ),
                                          static_cast<XUserPrivilege>( 254 ), NULL, verify ) == E_INVALIDARG );
    CHECK( "bad-options", privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( 2 ),
                                           static_cast<XUserPrivilege>( 254 ), &block, verify ) == E_INVALIDARG );
    CHECK( "bad-privilege", privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( 0 ),
                                             static_cast<XUserPrivilege>( 0 ), &block, verify ) == E_INVALIDARG );
    CHECK( "missing-verifier", privilege_begin( threads, &user, static_cast<XUserPrivilegeOptions>( 0 ),
                                                static_cast<XUserPrivilege>( 254 ), &block, NULL ) == E_INVALIDARG );
    CHECK( "no-retain-invalid", user.ref == 1 );
    CHECK( "uninitialized-result", privilege_result( threads, &block ) == E_INVALIDARG );
    CHECK( "null-result", privilege_result( threads, NULL ) == E_INVALIDARG );
    CHECK( "dll-null-user",
           ( users )->XUserResolvePrivilegeWithUiAsync( NULL, static_cast<XUserPrivilegeOptions>( 0 ),
                                                        static_cast<XUserPrivilege>( 254 ), &block ) == E_INVALIDARG );
    CHECK( "dll-foreign-user",
           ( users )->XUserResolvePrivilegeWithUiAsync( ( XUserHandle )&user, static_cast<XUserPrivilegeOptions>( 0 ),
                                                        static_cast<XUserPrivilege>( 254 ), &block ) == E_INVALIDARG );
    CHECK( "dll-null-result", ( users )->XUserResolvePrivilegeWithUiResult( NULL ) == E_INVALIDARG );
    CHECK( "dll-foreign-result", ( users )->XUserResolvePrivilegeWithUiResult( &block ) == E_INVALIDARG );
    for ( mode = 0; mode < 4; ++mode )
        for ( test = 0; test < 6; ++test )
            test_async( threads, ( XTaskQueueDispatchMode )mode, test );
    test_lifetime( threads );
    ( users )->Release();
    ( threads )->Release();
    printf( "CHECKS %ld FAILURES %ld\n", checks, failures );
    return failures ? 1 : 0;
}
