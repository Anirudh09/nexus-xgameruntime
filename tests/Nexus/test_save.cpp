// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include "../../src/System/XGameSave.h"
#include "../../src/System/User/XUser.h"
#include "../../src/Runtime/Configuration.h"
#include <cstring>
#include <vector>

namespace
{
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
class SaveFixture : public testing::Test
{
protected:
    OfflineTransport transport;
    XUser *user = nullptr;
    IXGameSaveImpl *save = nullptr;
    IXThreadingImpl *threading = nullptr;
    XGameSaveProviderHandle provider = nullptr;
    XGameSaveContainerHandle container = nullptr;
    XTaskQueueHandle queue = nullptr;
    char scid[40]{};
    void SetUp() override
    {
        static const auto initialized = []
        {
            InitializeLogging();
            return true;
        }();
        ASSERT_EQ( nexus::GetGameConfiguration(), nullptr ); // A real title cannot enter this offline fixture.
        user = new XUser( transport );
        user->ref = 1;
        user->xuid = 42;
        user->initDone = 1;
        user->initResult = S_OK;
        GUID id{};
        ASSERT_EQ( CoCreateGuid( &id ), S_OK );
        WCHAR wide[40]{};
        ASSERT_GT( StringFromGUID2( id, wide, 40 ), 0 );
        ASSERT_EQ( WideCharToMultiByte( CP_UTF8, 0, wide + 1, 36, scid, 39, nullptr, nullptr ), 36 );
        ASSERT_EQ( QueryApiImpl( CLSID_XGameSaveImpl, __uuidof( IXGameSaveImpl ), reinterpret_cast<void **>( &save ) ),
                   S_OK );
        ASSERT_EQ(
            QueryApiImpl( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &threading ) ),
            S_OK );
        ASSERT_EQ( save->XGameSaveInitializeProvider( user, scid, FALSE, &provider ), S_OK );
        ASSERT_EQ( save->XGameSaveCreateContainer( provider, "Save", &container ), S_OK );
        ASSERT_EQ(
            threading->XTaskQueueCreate( XTaskQueueDispatchMode::Manual, XTaskQueueDispatchMode::Manual, &queue ),
            S_OK );
    }
    void Seed()
    {
        XGameSaveUpdateHandle update = nullptr;
        ASSERT_EQ( save->XGameSaveCreateUpdate( container, "Synthetic save", &update ), S_OK );
        BYTE bytes[] = { 1, 2, 3, 4, 5 };
        EXPECT_EQ( save->XGameSaveSubmitBlobWrite( update, "state.dat", bytes, sizeof( bytes ) ), S_OK );
        EXPECT_EQ( save->XGameSaveSubmitUpdate( update ), S_OK );
        save->XGameSaveCloseUpdate( update );
    }
    void TearDown() override
    {
        EXPECT_EQ( transport.calls, 0u ); // No live service or transport fallback is possible.
        if ( container ) save->XGameSaveCloseContainer( container );
        if ( provider ) save->XGameSaveCloseProvider( provider );
        if ( queue ) threading->XTaskQueueCloseHandle( queue );
        if ( threading ) threading->Release();
        if ( save ) save->Release();
        if ( user ) user->Release();
    }
};
}
TEST_F( SaveFixture, LocalBlobPersistsAfterProviderCloseAndReopen )
{
    Seed();
    save->XGameSaveCloseContainer( container );
    container = nullptr;
    save->XGameSaveCloseProvider( provider );
    provider = nullptr;
    ASSERT_EQ( save->XGameSaveInitializeProvider( user, scid, FALSE, &provider ), S_OK );
    ASSERT_EQ( save->XGameSaveCreateContainer( provider, "Save", &container ), S_OK );
    std::vector<BYTE> buffer( 512 );
    UINT32 count = 0;
    ASSERT_EQ( save->XGameSaveReadBlobData( container, nullptr, &count, buffer.size(),
                                            reinterpret_cast<XGameSaveBlob *>( buffer.data() ) ),
               S_OK );
    ASSERT_EQ( count, 1u );
    auto blob = reinterpret_cast<XGameSaveBlob *>( buffer.data() );
    EXPECT_STREQ( blob->info.name, "state.dat" );
    ASSERT_EQ( blob->info.size, 5u );
    const BYTE expected[] = { 1, 2, 3, 4, 5 };
    EXPECT_EQ( memcmp( blob->data, expected, sizeof( expected ) ), 0 );
}
TEST_F( SaveFixture, AsyncReadCanRetrySmallBufferAndReturnsActualBlobCount )
{
    Seed();
    XAsyncBlock async{};
    async.queue = queue;
    ASSERT_EQ( save->XGameSaveReadBlobDataAsync( container, nullptr, 0, &async ), S_OK );
    ASSERT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 5000 ) );
    ASSERT_EQ( threading->XAsyncGetStatus( &async, TRUE ), S_OK );
    std::vector<BYTE> buffer( 512 );
    UINT32 count = 99;
    EXPECT_TRUE( FAILED(
        save->XGameSaveReadBlobDataResult( &async, 1, reinterpret_cast<XGameSaveBlob *>( buffer.data() ), &count ) ) );
    EXPECT_EQ( count, 0u );
    ASSERT_EQ( save->XGameSaveReadBlobDataResult( &async, buffer.size(),
                                                  reinterpret_cast<XGameSaveBlob *>( buffer.data() ), &count ),
               S_OK );
    ASSERT_EQ( count, 1u );
    auto blob = reinterpret_cast<XGameSaveBlob *>( buffer.data() );
    EXPECT_STREQ( blob->info.name, "state.dat" );
    EXPECT_EQ( blob->info.size, 5u );
}
TEST_F( SaveFixture, EmptyAsyncReadDoesNotNeedAnOpaqueAsyncLayoutOrDummyCallerBuffer )
{
    XAsyncBlock async{};
    async.queue = queue;
    ASSERT_EQ( save->XGameSaveReadBlobDataAsync( container, nullptr, 0, &async ), S_OK );
    ASSERT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 5000 ) );
    ASSERT_EQ( threading->XAsyncGetStatus( &async, TRUE ), S_OK );
    UINT32 count = 99;
    EXPECT_EQ( save->XGameSaveReadBlobDataResult( &async, 0, nullptr, &count ), S_OK );
    EXPECT_EQ( count, 0u );
}

TEST_F( SaveFixture, WrongAsyncResultTypeDoesNotConsumeOrReinterpretThePayload )
{
    XAsyncBlock async{};
    async.queue = queue;
    ASSERT_EQ( save->XGameSaveGetRemainingQuotaAsync( provider, &async ), S_OK );
    ASSERT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 5000 ) );
    std::vector<BYTE> bytes( 512 );
    UINT32 count = 99;
    EXPECT_EQ( save->XGameSaveReadBlobDataResult( &async, bytes.size(),
                                                  reinterpret_cast<XGameSaveBlob *>( bytes.data() ), &count ),
               E_INVALIDARG );
    EXPECT_EQ( count, 0u );
    INT64 quota = 0;
    ASSERT_EQ( save->XGameSaveGetRemainingQuotaResult( &async, &quota ), S_OK );
    EXPECT_GT( quota, 0 );
}
TEST_F( SaveFixture, RejectedAsyncBeginsReleaseAllRetainedUserReferences )
{
    const auto before = InterlockedCompareExchange( &user->ref, 0, 0 );
    EXPECT_EQ( save->XGameSaveInitializeProviderAsync( user, scid, FALSE, nullptr ), E_INVALIDARG );
    EXPECT_EQ( InterlockedCompareExchange( &user->ref, 0, 0 ), before );
    XAsyncBlock busy{};
    busy.queue = queue;
    ASSERT_EQ( save->XGameSaveGetRemainingQuotaAsync( provider, &busy ), S_OK );
    EXPECT_EQ( save->XGameSaveInitializeProviderAsync( user, scid, FALSE, &busy ), E_INVALIDARG );
    EXPECT_EQ( InterlockedCompareExchange( &user->ref, 0, 0 ), before );
    ASSERT_TRUE( threading->XTaskQueueDispatch( queue, XTaskQueuePort::Work, 5000 ) );
    INT64 quota = 0;
    EXPECT_EQ( save->XGameSaveGetRemainingQuotaResult( &busy, &quota ), S_OK );
}

TEST_F( SaveFixture, ReadsTheExistingDecimalUserNamespaceAndLegacyIndexFormat )
{
    char local[2048]{};
    ASSERT_GT( GetEnvironmentVariableA( "LOCALAPPDATA", local, sizeof( local ) ), 0u );
    std::string directory = std::string( local ) + "\\XodusGameSave\\42\\" + scid + "\\53617665";
    ASSERT_TRUE( CreateDirectoryA( directory.c_str(), nullptr ) );
    auto write = []( const std::string &path, const char *bytes, DWORD size )
    {
        auto file = CreateFileA( path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, 0, nullptr );
        if ( file == INVALID_HANDLE_VALUE ) return false;
        DWORD written = 0;
        const bool result =
            WriteFile( file, bytes, size, &written, nullptr ) && written == size && FlushFileBuffers( file );
        CloseHandle( file );
        return result;
    };
    constexpr char index[] =
        "display 4c6567616379\ntime 1760000000\nremote -\ndirty 1\nrevision 7\nblob 73746174652e646174 4 -\n";
    ASSERT_TRUE( write( directory + "\\index", index, sizeof( index ) - 1 ) );
    ASSERT_TRUE( write( directory + "\\b_73746174652e646174", "past", 4 ) );
    save->XGameSaveCloseContainer( container );
    container = nullptr;
    save->XGameSaveCloseProvider( provider );
    provider = nullptr;
    ASSERT_EQ( save->XGameSaveInitializeProvider( user, scid, FALSE, &provider ), S_OK );
    ASSERT_EQ( save->XGameSaveCreateContainer( provider, "Save", &container ), S_OK );
    std::vector<BYTE> buffer( 512 );
    UINT32 count = 0;
    ASSERT_EQ( save->XGameSaveReadBlobData( container, nullptr, &count, buffer.size(),
                                            reinterpret_cast<XGameSaveBlob *>( buffer.data() ) ),
               S_OK );
    ASSERT_EQ( count, 1u );
    auto blob = reinterpret_cast<XGameSaveBlob *>( buffer.data() );
    EXPECT_STREQ( blob->info.name, "state.dat" );
    ASSERT_EQ( blob->info.size, 4u );
    EXPECT_EQ( memcmp( blob->data, "past", 4 ), 0 );
}
