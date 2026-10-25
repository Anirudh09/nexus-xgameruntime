// SPDX-License-Identifier: LGPL-2.1-or-later
#include "Network.h"
#include "../../HttpClient.h"
#include "../../Runtime/Xml.h"
#include <charconv>
#include <cmath>
#include <memory>
#include <new>
#include <atomic>
#include <deque>
#include <mutex>

namespace nexus::invites
{
struct Release { template<typename T> void operator()( T *value ) const { if ( value ) value->Release(); } };
template<typename T> using Com = std::unique_ptr<T, Release>;
static std::string Quote( const std::string &value )
{
    static const char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for ( const unsigned char c : value )
        if ( c == '"' || c == '\\' ) { out += '\\'; out += c; }
        else if ( c < 32 ) { out += "\\u00"; out += hex[c >> 4]; out += hex[c & 15]; }
        else out += c;
    return out + '"';
}
static std::string Encode( const std::string &value )
{
    static const char hex[] = "0123456789ABCDEF"; std::string out;
    for ( const unsigned char c : value )
        if ( ( c >= 'a' && c <= 'z' ) || ( c >= 'A' && c <= 'Z' ) || ( c >= '0' && c <= '9' ) || c == '-' || c == '_' || c == '.' || c == '~' ) out += c;
        else { out += '%'; out += hex[c >> 4]; out += hex[c & 15]; }
    return out;
}
static HRESULT Parse( const std::string &body, IJsonValue **value )
{
    *value = nullptr; if ( body.empty() || body.size() > 65536 ) return E_INVALIDARG;
    std::wstring wide; auto hr = ToUtf16( body, wide ); if ( FAILED( hr ) ) return hr;
    HSTRING className = nullptr, text = nullptr; IJsonValueStatics *factory = nullptr;
    hr = WindowsCreateString( RuntimeClass_Windows_Data_Json_JsonValue, wcslen( RuntimeClass_Windows_Data_Json_JsonValue ), &className );
    if ( SUCCEEDED( hr ) ) hr = RoGetActivationFactory( className, __uuidof( IJsonValueStatics ), reinterpret_cast<void **>( &factory ) );
    if ( SUCCEEDED( hr ) ) hr = WindowsCreateString( wide.data(), static_cast<UINT32>( wide.size() ), &text );
    if ( SUCCEEDED( hr ) ) hr = factory->Parse( text, value );
    if ( factory ) factory->Release(); WindowsDeleteString( className ); WindowsDeleteString( text ); return hr;
}
static std::string Text( IJsonObject *object, const WCHAR *key, bool required = true )
{
    HSTRING value = nullptr; const auto hr = get_json_string( object, key, &value );
    if ( FAILED( hr ) ) { if ( required ) throw E_INVALIDARG; return {}; }
    UINT32 count = 0; const auto *text = WindowsGetStringRawBuffer( value, &count ); std::string result;
    const auto converted = count <= 8192 ? ToUtf8( std::wstring_view( text, count ), result ) : E_INVALIDARG;
    WindowsDeleteString( value ); if ( FAILED( converted ) || result.find( '\0' ) != std::string::npos ) throw E_INVALIDARG;
    return result;
}
static HRESULT Count( IJsonArray *array, UINT32 *size )
{
    IVector_IJsonValue *raw = nullptr;
    const auto hr = array->QueryInterface( __uuidof( IVector_IJsonValue ), reinterpret_cast<void **>( &raw ) );
    if ( FAILED( hr ) ) return hr;
    Com<IVector_IJsonValue> vector( raw ); return raw->get_Size( size );
}
static bool UserId( const std::string &value )
{
    UINT64 number = 0; const auto parsed = std::from_chars( value.data(), value.data() + value.size(), number );
    return !value.empty() && value[0] != '0' && parsed.ec == std::errc() && parsed.ptr == value.data() + value.size() && number;
}
static bool Guid( const std::string &value )
{
    if ( value.size() != 36 ) return false;
    for ( unsigned i = 0; i < 36; ++i )
        if ( i == 8 || i == 13 || i == 18 || i == 23 ) { if ( value[i] != '-' ) return false; }
        else if ( !( value[i] >= '0' && value[i] <= '9' ) && !( value[i] >= 'a' && value[i] <= 'f' ) && !( value[i] >= 'A' && value[i] <= 'F' ) ) return false;
    return true;
}
static UINT64 Expiration( const std::string &value )
{
    if ( value.size() < 20 || value.size() > 30 || value.back() != 'Z' || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' || value[16] != ':' ) throw E_INVALIDARG;
    for ( unsigned i = 0; i < 19; ++i ) if ( i != 4 && i != 7 && i != 10 && i != 13 && i != 16 && ( value[i] < '0' || value[i] > '9' ) ) throw E_INVALIDARG;
    if ( value.size() > 20 )
    {
        if ( value[19] != '.' || value.size() < 22 ) throw E_INVALIDARG;
        for ( SIZE_T i = 20; i + 1 < value.size(); ++i ) if ( value[i] < '0' || value[i] > '9' ) throw E_INVALIDARG;
    }
    SYSTEMTIME time{}; unsigned year, month, day, hour, minute, second;
    if ( sscanf( value.c_str(), "%4u-%2u-%2uT%2u:%2u:%2u", &year, &month, &day, &hour, &minute, &second ) != 6 || year < 1970 || year > 9999 || month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59 || second > 59 ) throw E_INVALIDARG;
    time.wYear = year; time.wMonth = month; time.wDay = day; time.wHour = hour; time.wMinute = minute; time.wSecond = second;
    FILETIME file; if ( !SystemTimeToFileTime( &time, &file ) ) throw E_INVALIDARG;
    SYSTEMTIME canonical{};
    if ( !FileTimeToSystemTime( &file, &canonical ) || canonical.wYear != time.wYear || canonical.wMonth != time.wMonth || canonical.wDay != time.wDay ) throw E_INVALIDARG;
    ULARGE_INTEGER ticks; ticks.LowPart = file.dwLowDateTime; ticks.HighPart = file.dwHighDateTime;
    return ( ticks.QuadPart - 116444736000000000ULL ) / 10000;
}
static HRESULT Authorization( XUserHandle user, const char *method, const std::string &url, const std::string &body, std::wstring &headers )
{
    if ( !user || !xuser_is_current_user( user ) ) return E_ACCESSDENIED;
    XTaskQueueHandle queue = nullptr;
    auto hr = x_threading_impl->XTaskQueueCreate( XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::ThreadPool, &queue );
    if ( FAILED( hr ) ) return hr;
    XAsyncBlock async{}; async.queue = queue;
    hr = x_user_impl->XUserGetTokenAndSignatureAsync( user, XUserGetTokenAndSignatureOptions::None, method, url.c_str(), 0, nullptr, body.size(), body.data(), &async );
    if ( SUCCEEDED( hr ) ) hr = x_threading_impl->XAsyncGetStatus( &async, TRUE );
    SIZE_T size = 0; if ( SUCCEEDED( hr ) ) hr = x_user_impl->XUserGetTokenAndSignatureResultSize( &async, &size );
    if ( size > 65536 ) hr = E_INVALIDARG;
    std::vector<BYTE> buffer( SUCCEEDED( hr ) ? size : 0 ); XUserGetTokenAndSignatureData *result = nullptr;
    if ( SUCCEEDED( hr ) ) hr = x_user_impl->XUserGetTokenAndSignatureResult( &async, size, buffer.data(), &result, nullptr );
    if ( SUCCEEDED( hr ) && result && result->token && result->signature && xuser_is_current_user( user ) )
    {
        std::string value = std::string( "Authorization: " ) + result->token + "\r\nSignature: " + result->signature + "\r\n";
        hr = ToUtf16( value, headers );
        SecureZeroMemory( value.data(), value.size() );
    }
    else if ( SUCCEEDED( hr ) ) hr = E_ACCESSDENIED;
    if ( !buffer.empty() ) SecureZeroMemory( buffer.data(), buffer.size() );
    x_threading_impl->XTaskQueueCloseHandle( queue );
    return hr;
}
struct InternetCloser { void operator()( void *value ) const { if ( value ) WinHttpCloseHandle( value ); } };
using Internet = std::unique_ptr<void, InternetCloser>;
class SignedNetwork final : public NetworkTransport
{
public:
    HRESULT Request( XUserHandle user, const char *method, const std::string &url, const std::string &body, const char *contract, DWORD &status, std::string &reply ) noexcept override
    {
        status = 0; reply.clear();
        try
        {
            std::wstring headers, wideUrl, wideMethod;
            if ( FAILED( ToUtf16( url, wideUrl ) ) || FAILED( ToUtf16( method, wideMethod ) ) ) return E_INVALIDARG;
            URL_COMPONENTS parts{}; parts.dwStructSize = sizeof( parts ); parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = static_cast<DWORD>( -1 );
            if ( !WinHttpCrackUrl( wideUrl.c_str(), 0, 0, &parts ) || parts.nScheme != INTERNET_SCHEME_HTTPS || parts.nPort != 443 ) return E_INVALIDARG;
            const std::wstring host( parts.lpszHostName, parts.dwHostNameLength ), route( parts.lpszUrlPath, parts.dwUrlPathLength );
            if ( host != L"sessiondirectory.xboxlive.com" && host != L"notify.xboxlive.com" && host != L"multiplayeractivity.xboxlive.com" ) return E_ACCESSDENIED;
            auto hr = Authorization( user, method, url, body, headers ); if ( FAILED( hr ) ) return hr;
            Internet session( WinHttpOpen( L"Nexus runtime", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, 0 ) ); if ( !session ) return E_FAIL;
            WinHttpSetTimeouts( session.get(), 5000, 5000, 5000, 5000 );
            Internet connection( WinHttpConnect( session.get(), host.c_str(), 443, 0 ) ); if ( !connection ) return E_FAIL;
            Internet request( WinHttpOpenRequest( connection.get(), wideMethod.c_str(), route.c_str(), nullptr, nullptr, nullptr, WINHTTP_FLAG_SECURE ) ); if ( !request ) return E_FAIL;
            DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER; if ( !WinHttpSetOption( request.get(), WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof( redirects ) ) ) return E_FAIL;
            std::wstring version; if ( FAILED( ToUtf16( contract, version ) ) ) return E_INVALIDARG;
            headers += L"Content-Type: application/json\r\nx-xbl-contract-version: " + version + L"\r\n";
            const auto sent = WinHttpSendRequest( request.get(), headers.c_str(), static_cast<DWORD>( -1 ), body.empty() ? nullptr : const_cast<char *>( body.data() ), static_cast<DWORD>( body.size() ), static_cast<DWORD>( body.size() ), 0 );
            SecureZeroMemory( headers.data(), headers.size() * sizeof( WCHAR ) );
            if ( !sent || !WinHttpReceiveResponse( request.get(), nullptr ) ) return E_FAIL;
            DWORD size = sizeof( status ); if ( !WinHttpQueryHeaders( request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr ) ) return E_FAIL;
            if ( status != 200 && status != 201 && status != 204 ) return E_ACCESSDENIED;
            char chunk[4096]; DWORD received = 0;
            for ( ;; ) { if ( !WinHttpReadData( request.get(), chunk, sizeof( chunk ), &received ) ) return E_FAIL; if ( !received ) break; if ( reply.size() + received > 65536 ) return E_INVALIDARG; reply.append( chunk, received ); }
            return xuser_is_current_user( user ) ? S_OK : E_ACCESSDENIED;
        }
        catch ( ... ) { return E_OUTOFMEMORY; }
    }
};
NetworkTransport &GetNetwork() noexcept { static SignedNetwork network; return network; }
HRESULT SendNetworkInvite( NetworkTransport &network, XUserHandle user, UINT32 title, const std::vector<std::string> &session, const std::string &recipient ) noexcept
{
    try
    {
        if ( !title || session.size() != 5 || !Guid( session[0] ) || session[1].empty() || session[1].size() > 100 || session[2].empty() || session[2].size() > 100 || !UserId( recipient ) || session[3].size() > 1024 || session[4].size() > 1024 ) return E_INVALIDARG;
        const auto body = "{\"type\":\"invite\",\"version\":1,\"sessionRef\":{\"scid\":" + Quote( session[0] ) + ",\"templateName\":" + Quote( session[1] ) + ",\"name\":" + Quote( session[2] ) + "},\"invitedXuid\":" + Quote( recipient ) + ",\"inviteAttributes\":{\"titleId\":" + Quote( std::to_string( title ) ) + ( session[3].empty() ? "" : ",\"contextString\":" + Quote( session[3] ) ) + ( session[4].empty() ? "" : ",\"context\":" + Quote( session[4] ) ) + "}}";
        DWORD status = 0; std::string response;
        const auto hr = network.Request( user, "POST", "https://sessiondirectory.xboxlive.com/handles", body, "107", status, response );
        if ( FAILED( hr ) || ( status != 200 && status != 201 ) ) return FAILED( hr ) ? hr : E_FAIL;
        IJsonValue *value = nullptr; if ( FAILED( Parse( response, &value ) ) ) return E_FAIL; Com<IJsonValue> parsed( value );
        IJsonObject *object = nullptr; if ( FAILED( value->GetObject( &object ) ) ) return E_FAIL; Com<IJsonObject> owned( object );
        return Guid( Text( object, L"id" ) ) ? S_OK : E_FAIL;
    }
    catch ( ... ) { return E_INVALIDARG; }
}
HRESULT SendActivityInvite( NetworkTransport &network, XUserHandle user, UINT64 owner, UINT32 title, const std::string &recipient ) noexcept
{
    try
    {
        if ( !title || !owner || !UserId( recipient ) || recipient == std::to_string( owner ) ) return E_INVALIDARG;
        const auto root = "https://multiplayeractivity.xboxlive.com/titles/" + std::to_string( title );
        DWORD status = 0; std::string response;
        auto hr = network.Request( user, "POST", root + "/activities/query", "{\"users\":[" + Quote( std::to_string( owner ) ) + "]}", "1", status, response );
        if ( FAILED( hr ) || status != 200 ) return FAILED( hr ) ? hr : E_FAIL;
        IJsonValue *value = nullptr; if ( FAILED( Parse( response, &value ) ) ) return E_INVALIDARG; Com<IJsonValue> parsed( value );
        IJsonObject *object = nullptr; if ( FAILED( value->GetObject( &object ) ) ) return E_INVALIDARG; Com<IJsonObject> data( object );
        IJsonArray *array = nullptr; if ( FAILED( get_json_array( object, L"userActivities", &array ) ) ) return E_INVALIDARG; Com<IJsonArray> users( array );
        UINT32 count = 0; if ( FAILED( Count( array, &count ) ) || count > 10 ) return E_INVALIDARG;
        std::string connection;
        for ( UINT32 i = 0; i < count; ++i )
        {
            IJsonObject *row = nullptr; if ( FAILED( array->GetObjectAt( i, &row ) ) ) return E_INVALIDARG; Com<IJsonObject> person( row );
            if ( Text( row, L"userId" ) != std::to_string( owner ) ) continue;
            IJsonArray *list = nullptr; if ( FAILED( get_json_array( row, L"activities", &list ) ) ) return E_INVALIDARG; Com<IJsonArray> activities( list ); UINT32 size = 0;
            if ( FAILED( Count( list, &size ) ) || size > 20 ) return E_INVALIDARG;
            for ( UINT32 j = 0; j < size; ++j )
            {
                IJsonObject *item = nullptr; if ( FAILED( list->GetObjectAt( j, &item ) ) ) return E_INVALIDARG; Com<IJsonObject> activity( item ); DOUBLE itemTitle = 0, maximum = 0, current = 0;
                if ( FAILED( get_json_number( item, L"titleId", &itemTitle ) ) || itemTitle != title ) continue;
                if ( Text( item, L"joinRestriction", false ) == "Closed" ) continue;
                if ( SUCCEEDED( get_json_number( item, L"maxPlayers", &maximum ) ) && SUCCEEDED( get_json_number( item, L"currentPlayers", &current ) ) && maximum > 0 && current >= maximum ) continue;
                const auto candidate = Text( item, L"connectionString", false );
                if ( candidate.empty() || candidate.size() > 2048 ) continue;
                if ( !connection.empty() && connection != candidate ) return E_INVALIDARG;
                connection = candidate;
            }
        }
        if ( connection.empty() ) return E_ACCESSDENIED;
        response.clear(); hr = network.Request( user, "POST", root + "/invites", "{\"connectionString\":" + Quote( connection ) + ",\"invitedUsers\":[" + Quote( recipient ) + "]}", "1", status, response );
        return SUCCEEDED( hr ) && ( status == 200 || status == 201 || status == 204 ) ? S_OK : FAILED( hr ) ? hr : E_FAIL;
    }
    catch ( ... ) { return E_INVALIDARG; }
}
HRESULT ParseNotification( const std::string &body, UINT64 receiver, UINT32 title, const Invitation &callback ) noexcept
{
    try
    {
        IJsonValue *value = nullptr; if ( FAILED( Parse( body, &value ) ) ) return E_INVALIDARG; Com<IJsonValue> parsed( value );
        IJsonObject *root = nullptr; if ( FAILED( value->GetObject( &root ) ) ) return E_INVALIDARG; Com<IJsonObject> owned( root );
        const auto service = Text( root, L"service", false ); if ( service == "achievements" ) return E_INVALIDARG;
        IJsonObject *invite = nullptr;
        if ( SUCCEEDED( get_json_object( root, L"inviteHandle", &invite ) ) )
        {
            Com<IJsonObject> event( invite );
            const auto id = Text( invite, L"id" ), sender = Text( invite, L"senderXuid" ), protocol = Text( invite, L"inviteProtocol" );
            if ( !Guid( id ) || !UserId( sender ) || Text( invite, L"invitedXuid" ) != std::to_string( receiver ) || ( protocol != "ms-xbl-multiplayer" && protocol != "ms-xbl-multiplayer:" ) ) return E_INVALIDARG;
            IJsonObject *attributes = nullptr, *info = nullptr;
            if ( FAILED( get_json_object( invite, L"inviteAttributes", &attributes ) ) || FAILED( get_json_object( invite, L"inviteInfo", &info ) ) ) { if ( attributes ) attributes->Release(); return E_INVALIDARG; }
            Com<IJsonObject> attr( attributes ), details( info );
            const auto game = Text( attributes, L"titleId", false ); if ( !game.empty() && game != std::to_string( title ) ) return E_ACCESSDENIED;
            const auto context = Text( attributes, L"context", false ); if ( context.size() > 1024 ) return E_INVALIDARG;
            callback( id, "ms-xbl-multiplayer://inviteHandleAccept?handle=" + id + "&invitedXuid=" + std::to_string( receiver ) + "&senderXuid=" + sender + "&context=" + Encode( context ), Text( info, L"sender" ), Expiration( Text( invite, L"expiration" ) ) );
            return S_OK;
        }
        if ( FAILED( get_json_object( root, L"notificationData", &invite ) ) ) return E_INVALIDARG;
        Com<IJsonObject> event( invite ); IJsonObject *sender = nullptr;
        if ( FAILED( get_json_object( invite, L"sender", &sender ) ) ) return E_INVALIDARG; Com<IJsonObject> details( sender );
        const auto user = Text( sender, L"xuid" ), connection = Text( invite, L"connectionString" );
        if ( !UserId( user ) || Text( invite, L"invitedUser" ) != std::to_string( receiver ) || connection.empty() || connection.size() > 2048 ) return E_INVALIDARG;
        callback( NewId(), "ms-xbl-multiplayer://inviteAccept?invitedUser=" + std::to_string( receiver ) + "&sender=" + user + "&connectionString=" + Encode( connection ), Text( sender, L"gamertag" ), Expiration( Text( invite, L"expirationTime" ) ) );
        return S_OK;
    }
    catch ( ... ) { return E_INVALIDARG; }
}
struct UpgradeState
{
    HANDLE event = CreateEventW( nullptr, FALSE, FALSE, nullptr );
    HANDLE closed = CreateEventW( nullptr, TRUE, FALSE, nullptr );
    std::atomic<DWORD> status{ 0 }, error{ 0 };
    ~UpgradeState() { if ( event ) CloseHandle( event ); if ( closed ) CloseHandle( closed ); }
};
static void CALLBACK UpgradeStatus( HINTERNET, DWORD_PTR context, DWORD status, void *info, DWORD )
{
    if ( !context ) return;
    auto &state = *reinterpret_cast<UpgradeState *>( context );
    if ( status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING ) SetEvent( state.closed );
    else if ( status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR )
    {
        state.error = static_cast<WINHTTP_ASYNC_RESULT *>( info )->dwError; SetEvent( state.event );
    }
    else if ( status == WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE || status == WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE )
    {
        state.status = status; SetEvent( state.event );
    }
}
static bool UpgradeWait( UpgradeState &state, DWORD expected )
{
    return WaitForSingleObject( state.event, 6000 ) == WAIT_OBJECT_0 && !state.error && state.status == expected;
}
struct WebSocketReader::State
{
    HINTERNET socket = nullptr;
    HANDLE closed = CreateEventW( nullptr, TRUE, FALSE, nullptr );
    std::atomic<bool> ready{ false }, failed{ false };
    std::atomic<bool> writeDone{ true };
    DWORD received = 0;
    WINHTTP_WEB_SOCKET_BUFFER_TYPE type{};
    char bytes[4096];
    std::string frame;
    std::string outgoing;
    ~State() { if ( closed ) CloseHandle( closed ); }
};
WebSocketReader::WebSocketReader() : state( std::make_unique<State>() ) {}
WebSocketReader::~WebSocketReader() { Stop(); }
void CALLBACK WebSocketReader::Status( HINTERNET, DWORD_PTR context, DWORD status, void *info, DWORD )
{
    if ( !context ) return;
    auto &state = *reinterpret_cast<State *>( context );
    if ( status == WINHTTP_CALLBACK_STATUS_HANDLE_CLOSING ) SetEvent( state.closed );
    else if ( status == WINHTTP_CALLBACK_STATUS_REQUEST_ERROR ) state.failed = true;
    else if ( status == WINHTTP_CALLBACK_STATUS_WRITE_COMPLETE ) state.writeDone = true;
    else if ( status == WINHTTP_CALLBACK_STATUS_READ_COMPLETE )
    {
        const auto &result = *static_cast<WINHTTP_WEB_SOCKET_STATUS *>( info );
        state.received = result.dwBytesTransferred; state.type = result.eBufferType;
        state.ready = true;
    }
}
HRESULT WebSocketReader::Open( HINTERNET connection, const WCHAR *path, DWORD flags, const std::wstring &headers ) noexcept
{
    if ( !connection || state->socket || !state->closed ) return E_INVALIDARG;
    UpgradeState upgrade;
    if ( !upgrade.event || !upgrade.closed ) return E_OUTOFMEMORY;
    HINTERNET request = WinHttpOpenRequest( connection, L"GET", path, nullptr, nullptr, nullptr, flags );
    if ( !request ) return E_FAIL;
    // The connection must originate from an ASYNC session. Close cancels queued
    // I/O, unlike a synchronous WebSocket receive on the pinned Wine version.
    bool callback = WinHttpSetStatusCallback( request, UpgradeStatus, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0 ) != WINHTTP_INVALID_STATUS_CALLBACK;
    DWORD_PTR context = reinterpret_cast<DWORD_PTR>( &upgrade );
    callback = callback && WinHttpSetOption( request, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof( context ) );
    DWORD redirects = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    bool ok = callback && WinHttpSetOption( request, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof( redirects ) ) && WinHttpSetOption( request, WINHTTP_OPTION_UPGRADE_TO_WEB_SOCKET, nullptr, 0 );
    if ( ok )
    {
        const auto sent = WinHttpSendRequest( request, headers.empty() ? nullptr : headers.c_str(), headers.empty() ? 0 : static_cast<DWORD>( -1 ), nullptr, 0, 0, context );
        ok = ( sent || GetLastError() == ERROR_IO_PENDING ) && UpgradeWait( upgrade, WINHTTP_CALLBACK_STATUS_SENDREQUEST_COMPLETE );
    }
    if ( ok )
    {
        upgrade.status = 0;
        const auto received = WinHttpReceiveResponse( request, nullptr );
        ok = ( received || GetLastError() == ERROR_IO_PENDING ) && UpgradeWait( upgrade, WINHTTP_CALLBACK_STATUS_HEADERS_AVAILABLE );
    }
    DWORD code = 0, size = sizeof( code );
    if ( ok ) ok = WinHttpQueryHeaders( request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &code, &size, nullptr ) && code == 101;
    if ( ok ) state->socket = WinHttpWebSocketCompleteUpgrade( request, 0 );
    WinHttpCloseHandle( request );
    if ( callback ) WaitForSingleObject( upgrade.closed, INFINITE );
    if ( !state->socket ) return E_FAIL;
    ResetEvent( state->closed ); state->failed = false; state->ready = false;
    context = reinterpret_cast<DWORD_PTR>( state.get() );
    if ( !WinHttpSetOption( state->socket, WINHTTP_OPTION_CONTEXT_VALUE, &context, sizeof( context ) ) || WinHttpSetStatusCallback( state->socket, Status, WINHTTP_CALLBACK_FLAG_ALL_NOTIFICATIONS, 0 ) == WINHTTP_INVALID_STATUS_CALLBACK )
    {
        WinHttpSetStatusCallback( state->socket, nullptr, 0, 0 ); WinHttpCloseHandle( state->socket ); state->socket = nullptr; return E_FAIL;
    }
    if ( WinHttpWebSocketReceive( state->socket, state->bytes, sizeof( state->bytes ), nullptr, nullptr ) ) { Stop(); return E_FAIL; }
    return S_OK;
}
HRESULT WebSocketReader::Send( const std::string &frame ) noexcept
{
    try
    {
        if ( !state->socket || state->failed || frame.size() > 65536 || !state->writeDone ) return E_FAIL;
        state->outgoing = frame; state->writeDone = false;
        const auto result = WinHttpWebSocketSend( state->socket, WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE, state->outgoing.data(), static_cast<DWORD>( state->outgoing.size() ) );
        if ( result ) { state->writeDone = true; return E_FAIL; }
        return S_OK;
    }
    catch ( ... ) { return E_OUTOFMEMORY; }
}
HRESULT WebSocketReader::Next( std::string &frame ) noexcept
{
    try
    {
        if ( state->failed || !state->socket ) return E_FAIL;
        if ( !state->ready.exchange( false ) ) return E_PENDING;
        if ( ( state->type != WINHTTP_WEB_SOCKET_UTF8_FRAGMENT_BUFFER_TYPE && state->type != WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE ) || state->received > sizeof( state->bytes ) || state->frame.size() + state->received > 65536 ) return E_INVALIDARG;
        state->frame.append( state->bytes, state->received );
        const bool complete = state->type == WINHTTP_WEB_SOCKET_UTF8_MESSAGE_BUFFER_TYPE;
        if ( complete ) { frame = std::move( state->frame ); state->frame.clear(); }
        if ( WinHttpWebSocketReceive( state->socket, state->bytes, sizeof( state->bytes ), nullptr, nullptr ) ) { state->failed = true; return E_FAIL; }
        return complete ? S_OK : E_PENDING;
    }
    catch ( ... ) { return E_OUTOFMEMORY; }
}
void WebSocketReader::Stop() noexcept
{
    if ( state->socket ) { WinHttpCloseHandle( state->socket ); WaitForSingleObject( state->closed, INFINITE ); state->socket = nullptr; }
    state->frame.clear(); state->outgoing.clear(); state->ready = false; state->writeDone = true;
}
struct Notifications::State
{
    XUserHandle user = nullptr;
    UINT32 title = 0, subscription = 0;
    ULONGLONG retry = 0;
    Internet session, connection, request;
    WebSocketReader reader;
    bool connected = false;
    std::string endpoint, frame;
};
Notifications::Notifications() : state( std::make_unique<State>() ) {}
Notifications::~Notifications() { Close(); }
void Notifications::Close() noexcept
{
    if ( !state ) return;
    state->reader.Stop(); state->connected = false; state->request.reset(); state->connection.reset(); state->session.reset();
    if ( state->user )
    {
        if ( !state->endpoint.empty() && xuser_is_current_user( state->user ) ) { DWORD status; std::string response; GetNetwork().Request( state->user, "DELETE", "https://notify.xboxlive.com/system/notifications/endpoints/" + Encode( state->endpoint ), "", "1", status, response ); }
        x_user_impl->XUserCloseHandle( state->user ); state->user = nullptr;
    }
    state->endpoint.clear(); state->frame.clear(); state->subscription = 0;
}
void Notifications::Tick( XUserHandle user, UINT32 title, const Invitation &callback ) noexcept
{
    try
    {
        if ( state->user && ( state->user != user || state->title != title ) ) Close();
        if ( !state->connected )
        {
            if ( GetTickCount64() < state->retry ) return; state->retry = GetTickCount64() + 60000;
            if ( FAILED( x_user_impl->XUserDuplicateHandle( user, &state->user ) ) ) return; state->title = title;
            std::wstring authorization; if ( FAILED( Authorization( user, "GET", "https://rta.xboxlive.com/connect", "", authorization ) ) ) { Close(); return; }
            state->session.reset( WinHttpOpen( L"Nexus runtime", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, nullptr, nullptr, WINHTTP_FLAG_ASYNC ) );
            if ( !state->session ) { Close(); return; }
            WinHttpSetTimeouts( state->session.get(), 5000, 5000, 5000, 5000 );
            state->connection.reset( WinHttpConnect( state->session.get(), L"rta.xboxlive.com", 443, 0 ) );
            authorization += L"Sec-WebSocket-Protocol: rta.xboxlive.com.V2\r\n";
            const auto opened = state->reader.Open( state->connection.get(), L"/connect", WINHTTP_FLAG_SECURE, authorization );
            SecureZeroMemory( authorization.data(), authorization.size() * sizeof( WCHAR ) );
            if ( FAILED( opened ) ) { Close(); return; }
            const auto resource = "https://notify.xboxlive.com/users/xuid(" + std::to_string( xuser_get_xuid( user ) ) + ")/deviceId/current/titleId/" + std::to_string( title );
            const auto subscription = "[1,1," + Quote( resource ) + "]";
            if ( FAILED( state->reader.Send( subscription ) ) ) { Close(); return; }
            state->connected = true;
        }
        const auto received = state->reader.Next( state->frame ); if ( received == E_PENDING ) return; if ( FAILED( received ) ) { Close(); return; }
        IJsonValue *value = nullptr; if ( FAILED( Parse( state->frame, &value ) ) ) { state->frame.clear(); return; } state->frame.clear(); Com<IJsonValue> parsed( value );
        IJsonArray *array = nullptr; if ( FAILED( value->GetArray( &array ) ) ) return; Com<IJsonArray> frame( array ); DOUBLE operation = 0;
        if ( FAILED( array->GetNumberAt( 0, &operation ) ) ) return;
        if ( operation == 1 )
        {
            DOUBLE request = 0, status = -1, subscription = 0;
            if ( FAILED( array->GetNumberAt( 1, &request ) ) || FAILED( array->GetNumberAt( 2, &status ) ) || FAILED( array->GetNumberAt( 3, &subscription ) ) || request != 1 || status != 0 || subscription <= 0 || subscription > UINT32_MAX || floor( subscription ) != subscription ) { Close(); return; }
            state->subscription = static_cast<UINT32>( subscription );
            const auto resource = "https://notify.xboxlive.com/users/xuid(" + std::to_string( xuser_get_xuid( user ) ) + ")/deviceId/current/titleId/" + std::to_string( title );
            const auto body = "{\"systemId\":" + Quote( NewId() ) + ",\"endpointUri\":" + Quote( resource ) + ",\"platform\":\"Win32\",\"transport\":\"RTA\",\"locale\":\"en-US\",\"titleId\":" + Quote( std::to_string( title ) ) + ",\"filters\":[{\"action\":\"Include\",\"source\":6,\"type\":1},{\"action\":\"Include\",\"source\":6,\"type\":8}]}";
            DWORD http; std::string response;
            if ( FAILED( GetNetwork().Request( user, "POST", "https://notify.xboxlive.com/system/notifications/endpoints", body, "1", http, response ) ) ) { Close(); return; }
            IJsonValue *endpoint = nullptr; if ( FAILED( Parse( response, &endpoint ) ) ) { Close(); return; } Com<IJsonValue> answer( endpoint ); IJsonObject *object = nullptr;
            if ( FAILED( endpoint->GetObject( &object ) ) ) { Close(); return; } Com<IJsonObject> data( object ); state->endpoint = Text( object, L"endpointId" ); if ( state->endpoint.empty() || state->endpoint.size() > 128 ) Close();
        }
        else if ( operation == 3 && state->subscription && !state->endpoint.empty() )
        {
            DOUBLE subscription = 0; IJsonObject *event = nullptr;
            if ( FAILED( array->GetNumberAt( 1, &subscription ) ) || subscription != state->subscription || FAILED( array->GetObjectAt( 2, &event ) ) ) return; Com<IJsonObject> data( event );
            IJsonValue *eventValue = nullptr;
            if ( FAILED( event->QueryInterface( __uuidof( IJsonValue ), reinterpret_cast<void **>( &eventValue ) ) ) ) return;
            Com<IJsonValue> eventOwned( eventValue );
            HSTRING json = nullptr; if ( SUCCEEDED( eventValue->Stringify( &json ) ) ) { UINT32 length = 0; const auto *text = WindowsGetStringRawBuffer( json, &length ); std::string payload; const auto converted = ToUtf8( std::wstring_view( text, length ), payload ); WindowsDeleteString( json ); if ( SUCCEEDED( converted ) ) ParseNotification( payload, xuser_get_xuid( user ), title, callback ); }
        }
        else if ( operation == 4 ) Close();
    }
    catch ( ... ) { Close(); }
}
}
