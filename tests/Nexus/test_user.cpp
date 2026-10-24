// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include "../../src/System/User/XUserImpl.h"
#include <wincrypt.h>
#include <string>
#include <vector>
#include <memory>
#include <cstring>

namespace
{
class RuntimeEnvironment final : public testing::Environment
{
public:
    void SetUp() override
    {
        InitializeLogging();
    }
};
const auto environment = testing::AddGlobalTestEnvironment( new RuntimeEnvironment );
NTSTATUS HashForVerification( BYTE *bytes, ULONG size, BYTE output[32] )
{
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    auto status = BCryptOpenAlgorithmProvider( &algorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0 );
    if ( status < 0 ) return status;
    status = BCryptHash( algorithm, nullptr, 0, bytes, size, output, 32 );
    BCryptCloseAlgorithmProvider( algorithm, 0 );
    return status;
}
class Apartment
{
    HRESULT status = CoInitializeEx( nullptr, COINIT_MULTITHREADED );

public:
    ~Apartment()
    {
        if ( SUCCEEDED( status ) ) CoUninitialize();
    }
};
class SyntheticTransport final : public UserTransport
{
public:
    unsigned backendCalls = 0, httpCalls = 0, rawCalls = 0, unexpected = 0;
    bool currentFails = false, titleToken = true;
    bool blockBackend = false;
    HANDLE backendEntered = CreateEventW( nullptr, TRUE, FALSE, nullptr );
    HANDLE backendContinue = CreateEventW( nullptr, TRUE, FALSE, nullptr );
    ~SyntheticTransport()
    {
        CloseHandle( backendEntered );
        CloseHandle( backendContinue );
    }
    std::string xml = "<MSATokenResponse><Token>synthetic-rps</Token><DeviceRps>synthetic-device-rps</DeviceRps>"
                      "<XboxMsaAccountId>0123456789ABCDEF</XboxMsaAccountId><StoreMsaAccountId>fedcba9876543210</"
                      "StoreMsaAccountId></MSATokenResponse>";
    std::string xid = "1234567890123456";
    std::string claimXid = "1234567890123456", privilegeText = "209 254";
    DWORD xstsStatus = 200;
    std::vector<std::string> signedBodies;
    static std::string Endpoints()
    {
        return R"({"SignaturePolicies":[{"Version":1,"MaxBodyBytes":1024}],"EndPoints":[{"Protocol":"https","Host":"service.example.invalid","Path":"/api","RelyingParty":"https://service.example.invalid/","TokenType":"JWT","SignaturePolicyIndex":0}]})";
    }
    static HRESULT Reply( std::string_view text, BYTE **output, SIZE_T *length )
    {
        *output = static_cast<BYTE *>( malloc( text.size() + 1 ) );
        if ( !*output ) return E_OUTOFMEMORY;
        memcpy( *output, text.data(), text.size() );
        ( *output )[text.size()] = 0;
        *length = text.size();
        return S_OK;
    }
    HRESULT Backend( UINT16 type, const char *body, UINT16 size, char **response, UINT16 *length ) override
    {
        ++backendCalls;
        if ( blockBackend )
        {
            SetEvent( backendEntered );
            if ( WaitForSingleObject( backendContinue, 5000 ) != WAIT_OBJECT_0 ) return E_ABORT;
        }
        if ( type != 3 || !body || !size || !strstr( body, "<MSATokenRequest>" ) )
        {
            ++unexpected;
            return E_FAIL;
        }
        SIZE_T count = 0;
        HRESULT hr = Reply( xml, reinterpret_cast<BYTE **>( response ), &count );
        *length = static_cast<UINT16>( count );
        return hr;
    }
    HRESULT Http( const WCHAR *method, const WCHAR *url, char *, const WCHAR *, const WCHAR **, BYTE **output,
                  SIZE_T *length ) override
    {
        ++httpCalls;
        *output = nullptr;
        *length = 0;
        if ( !wcscmp( url, L"https://title.mgt.xboxlive.com/titles/default/endpoints?type=1" ) )
            return Reply( Endpoints(), output, length );
        if ( !wcscmp( url, L"https://device.auth.xboxlive.com/device/authenticate" ) && !wcscmp( method, L"POST" ) )
            return Reply( R"({"Token":"synthetic-device-token"})", output, length );
        if ( !wcscmp( url, L"https://sisu.xboxlive.com/authorize" ) && !wcscmp( method, L"POST" ) )
        {
            std::string response = R"({"UserToken":{"Token":"synthetic-user-token"},)";
            if ( titleToken ) response += R"("TitleToken":{"Token":"synthetic-title-token"},)";
            response += R"("AuthorizationToken":{"Token":"synthetic-authorization","DisplayClaims":{"xui":[{"xid":")" +
                        xid + R"(","gtg":"Synthetic","mgt":"Synthetic","mgs":"","umg":"Synthetic","agg":"Adult"}]}}})";
            return Reply( response, output, length );
        }
        if ( !wcscmp( url, L"https://title.mgt.xboxlive.com/titles/current/endpoints" ) )
        {
            if ( currentFails ) return E_FAIL;
            return Reply( Endpoints(), output, length );
        }
        ++unexpected;
        return E_FAIL; // There is deliberately no network fallback.
    }
    HRESULT HttpRaw( const WCHAR *method, const WCHAR *url, const WCHAR *, const void *body, DWORD size, DWORD *status,
                     BYTE **output, SIZE_T *length ) override
    {
        ++rawCalls;
        *output = nullptr;
        *length = 0;
        *status = 0;
        if ( wcscmp( method, L"POST" ) || wcscmp( url, L"https://xsts.auth.xboxlive.com/xsts/authorize" ) )
        {
            ++unexpected;
            return E_FAIL;
        }
        signedBodies.emplace_back( static_cast<const char *>( body ), size );
        *status = xstsStatus;
        std::string response = R"({"Token":"synthetic-xsts","DisplayClaims":{"xui":[{"xid":")" + claimXid +
                               R"(","uhs":"synthetic-user-hash","prv":")" + privilegeText + R"("}]}})";
        return Reply( response, output, length );
    }
};
struct ReleaseUser
{
    void operator()( XUser *user ) const
    {
        user->Release();
    }
};
using User = std::unique_ptr<XUser, ReleaseUser>;
User CreateUser( UserTransport &transport )
{
    User user( new XUser( transport ) );
    user->ref = 1;
    EXPECT_EQ( user->Initialize( XUserAddOptions::AddDefaultUserSilently ), S_OK );
    return user;
}
}

TEST( NexusAuthentication, CompleteHandshakeKeepsActualXboxAndDistinctMsaIdentities )
{
    Apartment apartment;
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    ASSERT_EQ( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ), S_OK );
    EXPECT_EQ( user->xuid, 1234567890123456ull );
    EXPECT_STREQ( user->xbox_msa_id, "0123456789abcdef" );
    EXPECT_STREQ( user->store_msa_id, "fedcba9876543210" );
    EXPECT_STREQ( user->gamertag, "Synthetic" );
    EXPECT_EQ( user->age_group, XUserAgeGroup::Adult );
    EXPECT_EQ( transport.backendCalls, 1u );
    EXPECT_EQ( transport.httpCalls, 4u );
    EXPECT_EQ( transport.unexpected, 0u );
    user->initDone = 1;
    user->initResult = S_OK;
    endpoint info{};
    ASSERT_EQ( user->GetEndpointInfo( "https://service.example.invalid/api/test", &info ), S_OK );
    ASSERT_NE( info.policy, nullptr );
    EXPECT_EQ( info.policy->version, 1u );
    EXPECT_EQ( info.policy->maxBodyBytes, 1024u );
}
TEST( NexusAuthentication, MissingOptionalMsaMetadataDoesNotPreventAuthentication )
{
    Apartment apartment;
    SyntheticTransport transport;
    transport.xml =
        "<MSATokenResponse><Token>synthetic-rps</Token><DeviceRps>synthetic-device</DeviceRps></MSATokenResponse>";
    transport.titleToken = false;
    auto user = CreateUser( transport );
    ASSERT_EQ( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ), S_OK );
    EXPECT_EQ( user->xuid, 1234567890123456ull );
    EXPECT_STREQ( user->xbox_msa_id, "" );
    EXPECT_STREQ( user->store_msa_id, "" );
    EXPECT_EQ( user->titleToken, nullptr );
    user->initDone = 1;
    user->initResult = S_OK;
    WCHAR *auth = nullptr;
    ASSERT_EQ( user->GetAuthorization( L"https://service.example.invalid/", &auth ), S_OK );
    ASSERT_NE( auth, nullptr );
    free( auth );
    ASSERT_EQ( transport.signedBodies.size(), 1u );
    EXPECT_EQ( transport.signedBodies[0].find( "TitleToken" ), std::string::npos );
    EXPECT_EQ( transport.unexpected, 0u );
}
TEST( NexusAuthentication, DeviceAuthorizationCacheIsBoundToItsAudience )
{
    Apartment apartment;
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    ASSERT_EQ( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ), S_OK );
    user->initDone = 1;
    user->initResult = S_OK;
    for ( auto audience :
          { "https://one.example.invalid/", "https://one.example.invalid/", "https://two.example.invalid/" } )
    {
        char *auth = nullptr;
        ASSERT_EQ( xuser_device_authorization( user.get(), audience, &auth ), S_OK );
        ASSERT_NE( auth, nullptr );
        free( auth );
    }
    ASSERT_EQ( transport.signedBodies.size(), 2u );
    EXPECT_NE( transport.signedBodies[0].find( "https://one.example.invalid/" ), std::string::npos );
    EXPECT_NE( transport.signedBodies[1].find( "https://two.example.invalid/" ), std::string::npos );
}
TEST( NexusAuthentication, RetryingAnIncompleteHandshakeDoesNotAccumulateOldState )
{
    Apartment apartment;
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    transport.currentFails = true;
    EXPECT_TRUE( FAILED( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ) ) );
    transport.currentFails = false;
    ASSERT_EQ( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ), S_OK );
    EXPECT_EQ( user->endpointsLen, 2u );
    EXPECT_EQ( user->policiesLen, 2u );
    EXPECT_EQ( transport.backendCalls, 2u );
    EXPECT_EQ( transport.unexpected, 0u );
}
TEST( NexusAuthentication, PolicyReallocationDoesNotInvalidatePreviouslyMatchedEndpoints )
{
    Apartment apartment;
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    std::string first = SyntheticTransport::Endpoints();
    ASSERT_EQ( user->LoadEndpoints( reinterpret_cast<const BYTE *>( first.data() ), first.size() ), S_OK );
    std::string next = R"({"SignaturePolicies":[)";
    for ( unsigned i = 0; i < 4096; ++i )
    {
        if ( i ) next += ',';
        next += R"({"Version":2,"MaxBodyBytes":2048})";
    }
    next += R"(],"EndPoints":[{"Protocol":"https","Host":"second.example.invalid","SignaturePolicyIndex":4095}]})";
    ASSERT_EQ( user->LoadEndpoints( reinterpret_cast<const BYTE *>( next.data() ), next.size() ), S_OK );
    user->initDone = 1;
    user->initResult = S_OK;
    endpoint info{};
    ASSERT_EQ( user->GetEndpointInfo( "https://service.example.invalid/api/test", &info ), S_OK );
    ASSERT_NE( info.policy, nullptr );
    EXPECT_EQ( info.policy, &info.policyValue );
    EXPECT_EQ( info.policy->version, 1u );
    EXPECT_EQ( info.policy->maxBodyBytes, 1024u );
}
TEST( NexusAuthentication, MsaResponseMustHaveExpectedRootAndUniqueTickets )
{
    Apartment apartment;
    for ( auto xml : { "<Wrong><Token>synthetic</Token><DeviceRps>synthetic</DeviceRps></Wrong>",
                       "<MSATokenResponse><Token>first</Token><Token>second</Token><DeviceRps>synthetic</DeviceRps></"
                       "MSATokenResponse>" } )
    {
        SyntheticTransport transport;
        transport.xml = xml;
        auto user = CreateUser( transport );
        EXPECT_TRUE( FAILED( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ) ) );
        EXPECT_EQ( transport.backendCalls, 1u );
        EXPECT_EQ( transport.httpCalls, 1u );
        EXPECT_EQ( transport.unexpected, 0u );
    }
}
TEST( NexusAuthentication, SigningUsesTheActualGeneratedKeyAndCanonicalPayload )
{
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    char signature[104];
    ASSERT_EQ(
        user->GetSignature( 1, "post", "https://example.invalid/api?q=1", "synthetic-auth", 4, "body", signature ),
        S_OK );
    BYTE decoded[76];
    DWORD size = sizeof( decoded );
    ASSERT_TRUE(
        CryptStringToBinaryA( signature, sizeof( signature ), CRYPT_STRING_BASE64, decoded, &size, nullptr, nullptr ) );
    ASSERT_EQ( size, 76u );
    EXPECT_EQ( decoded[3], 1u );
    std::vector<BYTE> payload( decoded, decoded + 4 );
    payload.push_back( 0 );
    payload.insert( payload.end(), decoded + 4, decoded + 12 );
    payload.push_back( 0 );
    for ( auto text : { "POST", "/api?q=1", "synthetic-auth", "body" } )
    {
        payload.insert( payload.end(), text, text + strlen( text ) );
        payload.push_back( 0 );
    }
    BYTE digest[32];
    ASSERT_EQ( HashForVerification( payload.data(), static_cast<ULONG>( payload.size() ), digest ), 0 );
    EXPECT_EQ( BCryptVerifySignature( user->key, nullptr, digest, sizeof( digest ), decoded + 12, 64, 0 ), 0 );
    payload.back() = 'x';
    ASSERT_EQ( HashForVerification( payload.data(), static_cast<ULONG>( payload.size() ), digest ), 0 );
    EXPECT_NE( BCryptVerifySignature( user->key, nullptr, digest, sizeof( digest ), decoded + 12, 64, 0 ), 0 );
    EXPECT_EQ( transport.backendCalls + transport.httpCalls + transport.rawCalls, 0u );
}

TEST( NexusAuthentication, MalformedSignaturePoliciesAreRejectedWithoutIndexing )
{
    Apartment apartment;
    SyntheticTransport transport;
    for ( auto index : { "-1", "0.5", "99" } )
    {
        auto user = CreateUser( transport );
        std::string value = SyntheticTransport::Endpoints();
        auto offset = value.find( "SignaturePolicyIndex\":0" );
        ASSERT_NE( offset, std::string::npos );
        value.replace( offset, strlen( "SignaturePolicyIndex\":0" ), std::string( "SignaturePolicyIndex\":" ) + index );
        EXPECT_EQ( user->LoadEndpoints( reinterpret_cast<const BYTE *>( value.data() ), value.size() ), E_INVALIDARG );
    }
}
TEST( NexusAuthentication, EmptyEndpointMetadataIsNotAnAllocationFailure )
{
    Apartment apartment;
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    const std::string empty = R"({"SignaturePolicies":[],"EndPoints":[]})";
    EXPECT_EQ( user->LoadEndpoints( reinterpret_cast<const BYTE *>( empty.data() ), empty.size() ), S_OK );
    EXPECT_EQ( user->endpointsLen, 0u );
    EXPECT_EQ( user->policiesLen, 0u );
}
TEST( NexusAuthentication, PartialZeroOrOverflowXboxIdsAreNotAuthenticated )
{
    Apartment apartment;
    for ( auto xid : { "0", "12x", "+123", "18446744073709551616", "-1" } )
    {
        SyntheticTransport transport;
        transport.xid = xid;
        auto user = CreateUser( transport );
        EXPECT_TRUE( FAILED( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ) ) );
        EXPECT_EQ( user->xuid, 0u );
    }
}

TEST( NexusAuthentication, PrivilegesComeFromMatchingServerClaimsNotOptimisticConstants )
{
    Apartment apartment;
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    ASSERT_EQ( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ), S_OK );
    user->initDone = 1;
    user->initResult = S_OK;
    BOOLEAN granted = FALSE;
    XUserPrivilegeDenyReason reason = XUserPrivilegeDenyReason::Unknown;
    EXPECT_EQ( user->CheckPrivilege( XUserPrivilege::Multiplayer, &granted, &reason ), S_OK );
    EXPECT_TRUE( granted );
    EXPECT_EQ( reason, XUserPrivilegeDenyReason::None );
    granted = TRUE;
    EXPECT_EQ( user->CheckPrivilege( XUserPrivilege::CrossPlay, &granted, &reason ), S_OK );
    EXPECT_FALSE( granted );
    EXPECT_EQ( reason, XUserPrivilegeDenyReason::Unknown );
    EXPECT_EQ( transport.rawCalls, 1u );
}
TEST( NexusAuthentication, WrongIdentityOrServerDenialNeverGrantsAPrivilege )
{
    Apartment apartment;
    for ( bool mismatch : { true, false } )
    {
        SyntheticTransport transport;
        if ( mismatch )
            transport.claimXid = "9999999999999999";
        else
            transport.xstsStatus = 403;
        auto user = CreateUser( transport );
        ASSERT_EQ( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ), S_OK );
        user->initDone = 1;
        user->initResult = S_OK;
        BOOLEAN granted = TRUE;
        XUserPrivilegeDenyReason reason = XUserPrivilegeDenyReason::None;
        EXPECT_TRUE( FAILED( user->CheckPrivilege( XUserPrivilege::Multiplayer, &granted, &reason ) ) );
        EXPECT_FALSE( granted );
        EXPECT_EQ( reason, XUserPrivilegeDenyReason::Unknown );
        EXPECT_FALSE( user->privilegesKnown );
    }
}
TEST( NexusAuthentication, StoreUserRequiresActualMatchingMsaMetadata )
{
    Apartment apartment;
    for ( bool matching : { true, false } )
    {
        SyntheticTransport transport;
        if ( matching )
        {
            auto offset = transport.xml.find( "fedcba9876543210" );
            ASSERT_NE( offset, std::string::npos );
            transport.xml.replace( offset, 16, "0123456789abcdef" );
        }
        auto user = CreateUser( transport );
        ASSERT_EQ( user->InitializeNetwork( XUserAddOptions::AddDefaultUserSilently ), S_OK );
        user->initDone = 1;
        user->initResult = S_OK;
        EXPECT_EQ( !!user->IsStoreUser(), matching );
    }
}

TEST( NexusAuthentication, SigningRejectsInvalidPointersAndSizeOverflowBeforeReading )
{
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    char signature[104];
    memset( signature, 0x5a, sizeof( signature ) );
    EXPECT_EQ( user->GetSignature( 1, nullptr, "https://example.invalid/", "", 0, nullptr, signature ), E_INVALIDARG );
    EXPECT_EQ( user->GetSignature( 1, "GET", nullptr, "", 0, nullptr, signature ), E_INVALIDARG );
    EXPECT_EQ( user->GetSignature( 1, "GET", "https://example.invalid/", nullptr, 0, nullptr, signature ),
               E_INVALIDARG );
    EXPECT_EQ( user->GetSignature( 1, "GET", "https://example.invalid/", "", 1, nullptr, signature ), E_INVALIDARG );
    EXPECT_EQ( user->GetSignature( 1, "GET", "https://example.invalid/", "", 0, nullptr, nullptr ), E_POINTER );
    EXPECT_EQ(
        user->GetSignature( 1, "GET", "https://example.invalid/", "", MAXDWORD, "not-a-large-buffer", signature ),
        E_INVALIDARG );
    EXPECT_EQ( user->GetSignature( 1, "GET\r\ninjected", "https://example.invalid/", "", 0, nullptr, signature ),
               E_INVALIDARG );
    EXPECT_EQ( user->GetSignature( 1, "GET", "https:///missing-host", "", 0, nullptr, signature ), E_INVALIDARG );
    for ( auto byte : signature )
        EXPECT_EQ( byte, 0x5a );
}
TEST( NexusAuthentication, SigningIncludesRootQueryButNotAFragment )
{
    SyntheticTransport transport;
    auto user = CreateUser( transport );
    char signature[104];
    ASSERT_EQ( user->GetSignature( 1, "get", "https://example.invalid?query=1#not-sent", "", 0, nullptr, signature ),
               S_OK );
    BYTE decoded[76];
    DWORD size = sizeof( decoded );
    ASSERT_TRUE(
        CryptStringToBinaryA( signature, sizeof( signature ), CRYPT_STRING_BASE64, decoded, &size, nullptr, nullptr ) );
    std::vector<BYTE> payload( decoded, decoded + 4 );
    payload.push_back( 0 );
    payload.insert( payload.end(), decoded + 4, decoded + 12 );
    payload.push_back( 0 );
    for ( auto text : { "GET", "/?query=1", "", "" } )
    {
        payload.insert( payload.end(), text, text + strlen( text ) );
        payload.push_back( 0 );
    }
    BYTE digest[32];
    ASSERT_EQ( HashForVerification( payload.data(), static_cast<ULONG>( payload.size() ), digest ), 0 );
    EXPECT_EQ( BCryptVerifySignature( user->key, nullptr, digest, sizeof( digest ), decoded + 12, 64, 0 ), 0 );
}

TEST( NexusAuthentication, CancelSignInCompletesBeforeBlockedTransportReturns )
{
    Apartment apartment;
    SyntheticTransport transport;
    transport.blockBackend = true;
    XUserImpl users( transport );
    IXThreadingImpl *threading = nullptr;
    ASSERT_EQ(
        QueryApiImpl( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &threading ) ),
        S_OK );
    XTaskQueueHandle queue = nullptr;
    ASSERT_EQ(
        threading->XTaskQueueCreate( XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &queue ),
        S_OK );
    unsigned callbacks = 0;
    XAsyncBlock async{};
    async.queue = queue;
    async.context = &callbacks;
    async.callback = []( XAsyncBlock *block ) WINAPI { ++*static_cast<unsigned *>( block->context ); };
    ASSERT_EQ( users.XUserAddAsync( XUserAddOptions::AddDefaultUserSilently, &async ), S_OK );
    EXPECT_EQ( WaitForSingleObject( transport.backendEntered, 5000 ), WAIT_OBJECT_0 );
    threading->XAsyncCancel( &async );
    const auto cancelled = threading->XAsyncGetStatus( &async, FALSE );
    EXPECT_EQ( cancelled, E_ABORT );
    SetEvent( transport.backendContinue );
    EXPECT_EQ( WaitForUserSignInWorkers( 5000 ), S_OK );
    EXPECT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 5000 ) );
    EXPECT_EQ( callbacks, 1u );
    if ( cancelled == E_PENDING )
        EXPECT_EQ( threading->XAsyncGetStatus( &async, TRUE ), S_OK );
    else
        EXPECT_EQ( threading->XAsyncGetStatus( &async, TRUE ), E_ABORT );
    XUserHandle result = nullptr;
    if ( cancelled == E_PENDING && SUCCEEDED( users.XUserAddResult( &async, &result ) ) )
        users.XUserCloseHandle( result );
    threading->XTaskQueueCloseHandle( queue );
    threading->Release();
}

TEST( NexusAuthentication, AsyncResultTransfersRealIdentityAndChecksOperationIdentity )
{
    Apartment apartment;
    SyntheticTransport transport;
    transport.xml = "<MSATokenResponse><Token>synthetic</Token><DeviceRps>synthetic-device</"
                    "DeviceRps><XboxMsaAccountId>1234567890abcdef</"
                    "XboxMsaAccountId><StoreMsaAccountId>1234567890abcdef</StoreMsaAccountId></MSATokenResponse>";
    XUserImpl users( transport );
    IXThreadingImpl *threading = nullptr;
    ASSERT_EQ(
        QueryApiImpl( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &threading ) ),
        S_OK );
    XTaskQueueHandle queue = nullptr;
    ASSERT_EQ(
        threading->XTaskQueueCreate( XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &queue ),
        S_OK );
    unsigned callbacks = 0;
    XAsyncBlock async{};
    async.queue = queue;
    async.context = &callbacks;
    async.callback = []( XAsyncBlock *block ) WINAPI { ++*static_cast<unsigned *>( block->context ); };
    ASSERT_EQ( users.XUserAddAsync( XUserAddOptions::AddDefaultUserSilently, &async ), S_OK );
    EXPECT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 5000 ) );
    EXPECT_EQ( WaitForUserSignInWorkers( 5000 ), S_OK );
    EXPECT_EQ( callbacks, 1u );
    EXPECT_EQ( threading->XAsyncGetStatus( &async, TRUE ), S_OK );
    XUserHandle result = nullptr;
    ASSERT_EQ( users.XUserAddResult( &async, &result ), S_OK );
    ASSERT_NE( result, nullptr );
    UINT64 id = 0;
    EXPECT_EQ( users.XUserGetId( result, &id ), S_OK );
    EXPECT_EQ( id, 1234567890123456ull );
    EXPECT_TRUE( users.XUserIsStoreUser( result ) );
    char gamertag[32]{};
    SIZE_T used = 0;
    EXPECT_EQ( users.XUserGetGamertag( result, XUserGamertagComponent::Classic, sizeof( gamertag ), gamertag, &used ),
               S_OK );
    EXPECT_STREQ( gamertag, "Synthetic" );
    HKEY key = nullptr;
    ASSERT_EQ(
        RegOpenKeyExA( HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Store", 0, KEY_READ, &key ),
        ERROR_SUCCESS );
    char msa[32]{};
    DWORD size = sizeof( msa );
    EXPECT_EQ( RegGetValueA( key, nullptr, "PrimaryWebAccountId", RRF_RT_REG_SZ, nullptr, msa, &size ), ERROR_SUCCESS );
    EXPECT_STREQ( msa, "1234567890abcdef" );
    RegCloseKey( key );
    users.XUserCloseHandle( result );
    XAsyncBlock wrong{};
    wrong.queue = queue;
    ASSERT_EQ( threading->XAsyncRun( &wrong, []( XAsyncBlock * ) WINAPI -> HRESULT { return S_OK; } ), S_OK );
    EXPECT_EQ( threading->XAsyncGetStatus( &wrong, TRUE ), S_OK );
    result = reinterpret_cast<XUserHandle>( 1 );
    EXPECT_TRUE( FAILED( users.XUserAddResult( &wrong, &result ) ) );
    EXPECT_EQ( result, nullptr );
    threading->XTaskQueueCloseHandle( queue );
    threading->Release();
}
TEST( NexusAuthentication, CancelBeforeManualWorkDoesNotReadCredentials )
{
    Apartment apartment;
    SyntheticTransport transport;
    XUserImpl users( transport );
    IXThreadingImpl *threading = nullptr;
    ASSERT_EQ(
        QueryApiImpl( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &threading ) ),
        S_OK );
    XTaskQueueHandle queue = nullptr;
    ASSERT_EQ( threading->XTaskQueueCreate( XTaskQueueDispatchMode::Manual, XTaskQueueDispatchMode::Manual, &queue ),
               S_OK );
    unsigned callbacks = 0;
    XAsyncBlock async{};
    async.queue = queue;
    async.context = &callbacks;
    async.callback = []( XAsyncBlock *block ) WINAPI { ++*static_cast<unsigned *>( block->context ); };
    ASSERT_EQ( users.XUserAddAsync( XUserAddOptions::AddDefaultUserSilently, &async ), S_OK );
    threading->XAsyncCancel( &async );
    EXPECT_EQ( threading->XAsyncGetStatus( &async, FALSE ), E_ABORT );
    EXPECT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 5000 ) );
    threading->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 0 );
    EXPECT_EQ( WaitForUserSignInWorkers( 5000 ), S_OK );
    EXPECT_EQ( callbacks, 1u );
    EXPECT_EQ( transport.backendCalls, 0u );
    EXPECT_EQ( transport.httpCalls, 0u );
    XUserHandle result = reinterpret_cast<XUserHandle>( 1 );
    EXPECT_EQ( users.XUserAddResult( &async, &result ), E_ABORT );
    EXPECT_EQ( result, nullptr );
    threading->XTaskQueueCloseHandle( queue );
    threading->Release();
}
TEST( NexusAuthentication, CancelledBlockCanBeReusedWhileTransportIsStillBlocked )
{
    Apartment apartment;
    SyntheticTransport transport;
    transport.blockBackend = true;
    XUserImpl users( transport );
    IXThreadingImpl *threading = nullptr;
    ASSERT_EQ(
        QueryApiImpl( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &threading ) ),
        S_OK );
    XTaskQueueHandle queue = nullptr;
    ASSERT_EQ(
        threading->XTaskQueueCreate( XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &queue ),
        S_OK );
    struct Reuse
    {
        IXThreadingImpl *threading;
        XTaskQueueHandle queue;
        unsigned callbacks = 0;
    } state{ threading, queue };
    auto async = new XAsyncBlock{};
    async->queue = queue;
    async->context = &state;
    async->callback = []( XAsyncBlock *block ) WINAPI
    {
        auto state = static_cast<Reuse *>( block->context );
        ++state->callbacks;
        EXPECT_EQ( state->threading->XAsyncGetStatus( block, FALSE ), E_ABORT );
        *block = {};
        block->queue = state->queue;
        block->context = state;
        block->callback = []( XAsyncBlock *reused ) WINAPI
        {
            ++static_cast<Reuse *>( reused->context )->callbacks;
            delete reused;
        };
        EXPECT_EQ( state->threading->XAsyncRun( block, []( XAsyncBlock * ) WINAPI -> HRESULT { return S_OK; } ), S_OK );
    };
    ASSERT_EQ( users.XUserAddAsync( XUserAddOptions::AddDefaultUserSilently, async ), S_OK );
    EXPECT_EQ( WaitForSingleObject( transport.backendEntered, 5000 ), WAIT_OBJECT_0 );
    threading->XAsyncCancel( async );
    EXPECT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 5000 ) );
    EXPECT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 5000 ) );
    EXPECT_EQ( state.callbacks, 2u );
    SetEvent( transport.backendContinue );
    EXPECT_EQ( WaitForUserSignInWorkers( 5000 ), S_OK );
    EXPECT_EQ( transport.httpCalls, 1u );
    EXPECT_EQ( transport.rawCalls, 0u );
    EXPECT_FALSE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Completion, 0 ) );
    threading->XTaskQueueCloseHandle( queue );
    threading->Release();
}
