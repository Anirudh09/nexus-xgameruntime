// SPDX-License-Identifier: MIT
// Closed transports and synthetic local peers only. Never calls XUserAdd.
#include "../../src/System/GameInvite/Events.h"
#include "../../src/System/GameInvite/Network.h"
#include "../../src/System/GameInvite/Dialog.h"
#include <thread>
#include <cstdio>
#include <string>
#include <vector>
using namespace nexus::invites;
static unsigned checks, failures;
#define CHECK(label, expression) do { ++checks; if ( !( expression ) ) { ++failures; printf( "FAIL %s line=%u\n", label, __LINE__ ); } } while ( 0 )
static const char id[] = "12345678-1234-1234-1234-123456789abc";
static const char scid[] = "87654321-4321-4321-4321-cba987654321";
static const char uri[] = "ms-xbl-multiplayer://inviteHandleAccept?handle=12345678-1234-1234-1234-123456789abc&invitedXuid=42&senderXuid=43&context=";
struct Closed final : Transport { unsigned calls = 0; HRESULT Exchange( const std::vector<std::string> &, std::vector<std::string> & ) noexcept override { ++calls; return E_ACCESSDENIED; } };
struct Network final : NetworkTransport
{
    struct Call { std::string method, url, body, contract; };
    std::vector<Call> calls;
    std::string activity = "{\"userActivities\":[{\"userId\":\"42\",\"activities\":[{\"titleId\":123,\"connectionString\":\"actual synthetic activity\",\"maxPlayers\":4,\"currentPlayers\":1}]}]}";
    std::string answer = "{\"id\":\"12345678-1234-1234-1234-123456789abc\"}";
    DWORD code = 201;
    HRESULT result = S_OK;
    HRESULT Request( XUserHandle, const char *method, const std::string &url, const std::string &body, const char *contract, DWORD &status, std::string &reply ) noexcept override
    {
        calls.push_back( { method, url, body, contract } ); status = url.ends_with( "/activities/query" ) ? 200 : code; reply = url.ends_with( "/activities/query" ) ? activity : answer; return result;
    }
};
struct Seen { unsigned accepted = 0, pending = 0, protocol = 0, modernAccepted = 0, modernPending = 0; std::string last; };
static void CALLBACK Accepted( void *context, const char *text ) { auto &seen = *static_cast<Seen *>( context ); ++seen.accepted; seen.last = text; }
static void CALLBACK Pending( void *context, const char *text ) { auto &seen = *static_cast<Seen *>( context ); ++seen.pending; seen.last = text; }
static void CALLBACK Protocol( void *context, const char *text ) { auto &seen = *static_cast<Seen *>( context ); ++seen.protocol; seen.last = text; }
static void CALLBACK Modern( void *context, const XGameActivationInfo *info ) { auto &seen = *static_cast<Seen *>( context ); if ( info->type == XGameActivationType::PendingGameInvite ) ++seen.modernPending; else if ( info->type == XGameActivationType::AcceptedGameInvite ) ++seen.modernAccepted; else ++failures; seen.last = info->inviteUri; }
static void Drain( XTaskQueueHandle queue ) { for ( unsigned i = 0; i < 10000 && x_threading_impl->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 0 ); ++i ) {} }
struct Self { Events *events; XTaskQueueRegistrationToken token{}; bool called = false; };
static void CALLBACK SelfUnregister( void *context, const char * ) { auto &self = *static_cast<Self *>( context ); self.called = true; CHECK( "self-wait-does-not-deadlock", !self.events->Unregister( EventKind::Accepted, self.token, TRUE ) ); }
static void Callbacks()
{
    Closed closed; Events events( closed, false ); XTaskQueueHandle queue = nullptr;
    CHECK( "create-real-manual-queue", x_threading_impl->XTaskQueueCreate( XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &queue ) == S_OK );
    Seen seen; XTaskQueueRegistrationToken accepted{}, pending{}, modern{}, protocol{}, invalid{ 99 };
    CHECK( "null-callback-rejected", events.Register( EventKind::Accepted, queue, &seen, {}, &invalid ) == E_INVALIDARG && !invalid.token );
    CHECK( "null-token-rejected", events.Register( EventKind::Accepted, queue, &seen, { Accepted }, nullptr ) == E_INVALIDARG );
    CHECK( "accepted-registration", events.Register( EventKind::Accepted, queue, &seen, { Accepted }, &accepted ) == S_OK && accepted.token );
    CHECK( "pending-registration", events.Register( EventKind::Pending, queue, &seen, { Pending }, &pending ) == S_OK && pending.token != accepted.token );
    CHECK( "modern-registration", events.Register( EventKind::Activation, queue, &seen, { nullptr, Modern }, &modern ) == S_OK );
    CHECK( "protocol-registration", events.Register( EventKind::Protocol, queue, &seen, { nullptr, nullptr, Protocol }, &protocol ) == S_OK );
    CHECK( "wrong-family-token-rejected", !events.Unregister( EventKind::Pending, accepted, TRUE ) );
    events.Deliver( uri, true ); CHECK( "no-inline-callback", !seen.pending && !seen.modernPending ); Drain( queue );
    CHECK( "pending-routed", seen.pending == 1 && seen.modernPending == 1 && !seen.accepted && !seen.protocol );
    events.Deliver( uri, true ); Drain( queue ); CHECK( "duplicate-pending-suppressed", seen.pending == 1 );
    events.Deliver( uri, false ); Drain( queue ); CHECK( "accepted-routed", seen.accepted == 1 && seen.protocol == 1 && seen.modernAccepted == 1 && seen.pending == 1 ); CHECK( "exact-uri", seen.last == uri );
    Seen late; XTaskQueueRegistrationToken lateToken{};
    CHECK( "late-registration", events.Register( EventKind::Accepted, queue, &late, { Accepted }, &lateToken ) == S_OK ); Drain( queue ); CHECK( "cold-start-replay", late.accepted == 1 && late.last == uri );
    CHECK( "late-unregister", events.Unregister( EventKind::Accepted, lateToken, TRUE ) );
    events.Deliver( std::string( uri ) + "next", true ); CHECK( "pending-unregister-queued", events.Unregister( EventKind::Pending, pending, TRUE ) ); Drain( queue ); CHECK( "removed-callback-not-called", seen.pending == 1 );
    Self self{ &events }; CHECK( "self-registration", events.Register( EventKind::Accepted, queue, &self, { SelfUnregister }, &self.token ) == S_OK ); Drain( queue ); CHECK( "self-called", self.called );
    for ( unsigned i = 0; i < 128; ++i ) { events.Deliver( std::string( uri ) + std::to_string( i ), false ); Drain( queue ); }
    CHECK( "repeated-accepts-are-events", seen.accepted == 129 && seen.protocol == 129 && seen.modernAccepted == 129 );
    CHECK( "accepted-unregister", events.Unregister( EventKind::Accepted, accepted, TRUE ) ); CHECK( "modern-unregister", events.Unregister( EventKind::Activation, modern, TRUE ) ); CHECK( "protocol-unregister", events.Unregister( EventKind::Protocol, protocol, TRUE ) );
    CHECK( "invalid-unregister", !events.Unregister( EventKind::Accepted, accepted, TRUE ) );
    CHECK( "no-network-in-closed-fixture", !closed.calls ); x_threading_impl->XTaskQueueCloseHandle( queue );
}
static void Requests()
{
    const std::vector<std::string> session{ scid, "lobby", "actual-session", "", "opaque context" };
    Network network;
    CHECK( "actual-mpsd-shape", SendNetworkInvite( network, nullptr, 123, session, "43" ) == S_OK );
    CHECK( "one-post-no-retry", network.calls.size() == 1 && network.calls[0].method == "POST" && network.calls[0].url == "https://sessiondirectory.xboxlive.com/handles" && network.calls[0].contract == "107" );
    CHECK( "real-session-ref-not-guessed", network.calls[0].body.find( "actual-session" ) != std::string::npos && network.calls[0].body.find( "\"invitedXuid\":\"43\"" ) != std::string::npos );
    network.calls.clear(); CHECK( "bad-user-before-request", FAILED( SendNetworkInvite( network, nullptr, 123, session, "../escape" ) ) && network.calls.empty() );
    network.code = 403; CHECK( "denied-is-not-success", FAILED( SendNetworkInvite( network, nullptr, 123, session, "43" ) ) && network.calls.size() == 1 );
    network.calls.clear(); network.code = 201; network.answer = "{}"; CHECK( "missing-handle-is-not-success", FAILED( SendNetworkInvite( network, nullptr, 123, session, "43" ) ) && network.calls.size() == 1 );
    network.calls.clear(); network.code = 204;
    CHECK( "actual-mpa-activity", SendActivityInvite( network, nullptr, 42, 123, "43" ) == S_OK );
    CHECK( "mpa-query-then-one-send", network.calls.size() == 2 && network.calls[0].url.ends_with( "/titles/123/activities/query" ) && network.calls[1].url.ends_with( "/titles/123/invites" ) );
    CHECK( "connection-from-service", network.calls[1].body.find( "actual synthetic activity" ) != std::string::npos );
    network.calls.clear(); network.activity = "{\"userActivities\":[]}"; CHECK( "no-activity-no-send", FAILED( SendActivityInvite( network, nullptr, 42, 123, "43" ) ) && network.calls.size() == 1 );
    network.calls.clear(); network.activity = "{\"userActivities\":[{\"userId\":\"99\",\"activities\":[{\"titleId\":123,\"connectionString\":\"wrong owner\"}]}]}"; CHECK( "other-account-no-send", FAILED( SendActivityInvite( network, nullptr, 42, 123, "43" ) ) && network.calls.size() == 1 );
    network.calls.clear(); CHECK( "self-not-invited", FAILED( SendActivityInvite( network, nullptr, 42, 123, "42" ) ) && network.calls.empty() );
}
static void Dialogs()
{
    XTaskQueueHandle queue = nullptr;
    CHECK( "dialog-queue", x_threading_impl->XTaskQueueCreate( XTaskQueueDispatchMode::Manual, XTaskQueueDispatchMode::Manual, &queue ) == S_OK );
    XAsyncBlock block{}; block.queue = queue; std::atomic<bool> entered{ false }, release{ false };
    CHECK( "dialog-null-result", ShowInviteResult( nullptr ) == E_INVALIDARG && ShowActivityInviteResult( nullptr ) == E_INVALIDARG );
    CHECK( "uninitialized-result", ShowInviteResult( &block ) == E_INVALIDARG );
    CHECK( "empty-task-rejected", ScheduleDialog( &block, false, {} ) == E_INVALIDARG );
    CHECK( "dialog-schedule", ScheduleDialog( &block, false, [&]( const std::atomic<bool> &cancel ) { entered = true; while ( !release && !cancel ) Sleep( 1 ); return S_OK; } ) == S_OK );
    CHECK( "dialog-work-dispatched", x_threading_impl->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 1000 ) );
    const auto deadline = GetTickCount64() + 5000; while ( !entered && GetTickCount64() < deadline ) Sleep( 1 );
    CHECK( "dialog-worker-started", entered );
    CHECK( "external-ui-keeps-async-pending", ShowInviteResult( &block ) == E_PENDING );
    CHECK( "pending-wrong-typed-result", ShowActivityInviteResult( &block ) == E_INVALIDARG );
    release = true; CHECK( "dialog-completes-only-after-task", x_threading_impl->XAsyncGetStatus( &block, TRUE ) == S_OK );
    CHECK( "dialog-typed-completion", ShowInviteResult( &block ) == S_OK && ShowActivityInviteResult( &block ) == E_INVALIDARG );
    entered = false;
    CHECK( "activity-dialog-schedule", ScheduleDialog( &block, true, [&]( const std::atomic<bool> &cancel ) { entered = true; while ( !cancel ) Sleep( 1 ); return S_OK; } ) == S_OK );
    CHECK( "activity-work-dispatched", x_threading_impl->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 1000 ) );
    const auto cancelDeadline = GetTickCount64() + 5000; while ( !entered && GetTickCount64() < cancelDeadline ) Sleep( 1 );
    CHECK( "activity-worker-started", entered ); x_threading_impl->XAsyncCancel( &block );
    CHECK( "dialog-cancel-completes", x_threading_impl->XAsyncGetStatus( &block, TRUE ) == E_ABORT );
    CHECK( "cancel-typed-result", ShowActivityInviteResult( &block ) == E_ABORT && ShowInviteResult( &block ) == E_INVALIDARG );
    CHECK( "denied-dialog-schedule", ScheduleDialog( &block, false, []( const std::atomic<bool> & ) { return E_ACCESSDENIED; } ) == S_OK );
    CHECK( "denied-work-dispatched", x_threading_impl->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 1000 ) );
    CHECK( "denied-dialog-result", x_threading_impl->XAsyncGetStatus( &block, TRUE ) == E_ACCESSDENIED && ShowInviteResult( &block ) == E_ACCESSDENIED && ShowActivityInviteResult( &block ) == E_INVALIDARG );
    x_threading_impl->XTaskQueueCloseHandle( queue );
}
struct Blocked { HANDLE entered, release; };
static void CALLBACK BlockedCallback( void *context, const char * )
{
    auto &state = *static_cast<Blocked *>( context ); SetEvent( state.entered ); WaitForSingleObject( state.release, 5000 );
}
static void UnregisterWait()
{
    Closed closed; Events events( closed, false ); XTaskQueueHandle queue = nullptr;
    CHECK( "concurrent-queue", x_threading_impl->XTaskQueueCreate( XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::ThreadPool, &queue ) == S_OK );
    Blocked blocked{ CreateEventW( nullptr, TRUE, FALSE, nullptr ), CreateEventW( nullptr, TRUE, FALSE, nullptr ) };
    XTaskQueueRegistrationToken token{};
    CHECK( "concurrent-registration", events.Register( EventKind::Accepted, queue, &blocked, { BlockedCallback }, &token ) == S_OK );
    events.Deliver( uri, false ); CHECK( "callback-entered", WaitForSingleObject( blocked.entered, 5000 ) == WAIT_OBJECT_0 );
    std::atomic<bool> done{ false }; bool result = false;
    std::thread waiter( [&] { result = events.Unregister( EventKind::Accepted, token, TRUE ); done = true; } );
    Sleep( 40 ); CHECK( "unregister-waits-running-callback", !done ); SetEvent( blocked.release ); waiter.join(); CHECK( "unregister-drained", done && result );
    CloseHandle( blocked.entered ); CloseHandle( blocked.release ); x_threading_impl->XTaskQueueCloseHandle( queue );
    CHECK( "revoke-manual-queue", x_threading_impl->XTaskQueueCreate( XTaskQueueDispatchMode::Manual, XTaskQueueDispatchMode::Manual, &queue ) == S_OK );
    Seen seen; CHECK( "revoke-registration", events.Register( EventKind::Accepted, queue, &seen, { Accepted }, &token ) == S_OK );
    events.Deliver( uri, false ); events.Revoke(); Drain( queue ); CHECK( "revoked-queued-identity-not-delivered", !seen.accepted );
    CHECK( "new-registration-after-revoke", events.Register( EventKind::Accepted, queue, &seen, { Accepted }, &token ) == S_OK ); Drain( queue ); CHECK( "revoked-cached-capability-not-replayed", !seen.accepted );
    events.Unregister( EventKind::Accepted, token, TRUE ); x_threading_impl->XTaskQueueCloseHandle( queue );
}
static void Incoming()
{
    unsigned delivered = 0; std::string actual;
    const Invitation callback = [&]( const std::string &token, const std::string &text, const std::string &sender, UINT64 expires ) { ++delivered; actual = text; CHECK( "actual-notification-fields", !token.empty() && sender == "Example" && expires > 0 ); };
    const std::string first = std::string( "{\"inviteHandle\":{\"id\":\"" ) + id + "\",\"invitedXuid\":\"42\",\"senderXuid\":\"43\",\"inviteProtocol\":\"ms-xbl-multiplayer\",\"expiration\":\"2030-01-01T00:00:00Z\",\"inviteAttributes\":{\"titleId\":\"123\",\"context\":\"\"},\"inviteInfo\":{\"sender\":\"Example\"}}}";
    CHECK( "mpsd-notification", ParseNotification( first, 42, 123, callback ) == S_OK && delivered == 1 && actual == uri );
    CHECK( "receiver-bound", FAILED( ParseNotification( first, 99, 123, callback ) ) && delivered == 1 );
    CHECK( "title-bound", FAILED( ParseNotification( first, 42, 124, callback ) ) && delivered == 1 );
    const std::string second = "{\"notificationData\":{\"sender\":{\"xuid\":\"43\",\"gamertag\":\"Example\"},\"invitedUser\":\"42\",\"titleName\":\"Synthetic\",\"connectionString\":\"opaque &= % data\",\"expirationTime\":\"2030-01-01T00:00:00Z\"}}";
    CHECK( "mpa-notification", ParseNotification( second, 42, 123, callback ) == S_OK && delivered == 2 );
    CHECK( "opaque-connection-preserved", actual.find( "opaque%20%26%3D%20%25%20data" ) != std::string::npos );
    CHECK( "wrong-mpa-receiver", FAILED( ParseNotification( second, 43, 123, callback ) ) && delivered == 2 );
    for ( const auto &date : { "2030-02-30T00:00:00Z", "2030-01-01T00:00:00badZ", "2030-01-01T00:00:00.Z", "2030-01-01T00:00:00.abcZ" } )
    {
        auto invalid = first; invalid.replace( invalid.find( "2030-01-01T00:00:00Z" ), 20, date );
        CHECK( "malformed-expiration-rejected", FAILED( ParseNotification( invalid, 42, 123, callback ) ) && delivered == 2 );
    }
    CHECK( "other-service-rejected", FAILED( ParseNotification( "{\"service\":\"achievements\"," + first.substr( 1 ), 42, 123, callback ) ) && delivered == 2 );
    for ( const auto &bad : { std::string( "{}" ), std::string( "[]" ), std::string( "null" ), std::string( "not JSON" ), std::string( 65537, 'x' ) } ) CHECK( "malformed-notification", FAILED( ParseNotification( bad, 42, 123, callback ) ) );
}
static void Socket( const char *text )
{
    const auto port = static_cast<INTERNET_PORT>( strtoul( text, nullptr, 10 ) );
    HINTERNET session = WinHttpOpen( L"Nexus synthetic local probe", WINHTTP_ACCESS_TYPE_NO_PROXY, nullptr, nullptr, WINHTTP_FLAG_ASYNC ); CHECK( "socket-session", session );
    HINTERNET connection = WinHttpConnect( session, L"127.0.0.1", port, 0 ); CHECK( "socket-connection", connection );
    WebSocketReader reader; CHECK( "reader-opened", reader.Open( connection, L"/synthetic", 0, L"" ) == S_OK );
    std::vector<std::string> frames; const auto deadline = GetTickCount64() + 5000;
    while ( frames.size() < 2 && GetTickCount64() < deadline ) { std::string frame; const auto hr = reader.Next( frame ); if ( hr == S_OK ) frames.push_back( frame ); else if ( hr != E_PENDING ) break; Sleep( 5 ); }
    CHECK( "actual-socket-message", frames.size() == 2 && frames[0] == "synthetic first" );
    CHECK( "actual-fragment-reassembly", frames.size() == 2 && frames[1] == "synthetic fragmented" );
    const auto before = GetTickCount64(); reader.Stop(); CHECK( "blocked-reader-cancel-is-bounded", GetTickCount64() - before < 3000 );
    WinHttpCloseHandle( connection ); WinHttpCloseHandle( session );
}
int main( int argc, char **argv )
{
    InitializeLogging(); const auto com = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
    if ( argc == 2 && !strcmp( argv[1], "--component" ) ) { Callbacks(); Dialogs(); UnregisterWait(); }
    else if ( argc == 3 && !strcmp( argv[1], "--socket" ) ) Socket( argv[2] );
    else if ( argc == 2 && !strcmp( argv[1], "--bridge" ) )
    {
        std::vector<std::string> reply;
        CHECK( "actual-wire-hello", GetBridge().Exchange( { "hello-events", "42", "123" }, reply ) == S_OK );
        CHECK( "actual-wire-present", GetBridge().Exchange( { "present", id, scid, "lobby", "synthetic-session", "", "" }, reply ) == S_OK );
        CHECK( "actual-wire-selection", GetBridge().Exchange( { "selection", id }, reply ) == S_OK && reply == std::vector<std::string>{ "43" } );
        CHECK( "actual-wire-completion", GetBridge().Exchange( { "complete", id, "0" }, reply ) == S_OK );
    }
    else { Callbacks(); Requests(); Incoming(); Dialogs(); UnregisterWait(); }
    if ( SUCCEEDED( com ) ) CoUninitialize();
    printf( "CHECKS %u FAILURES %u\n", checks, failures ); return failures ? 1 : 0;
}
