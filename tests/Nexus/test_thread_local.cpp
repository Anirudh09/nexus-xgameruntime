// SPDX-License-Identifier: MIT
#include "../test_fixtures.h"
#include <windows.h>
#include <xasyncprovider.h>
#include <thread>

TEST( NexusThreading, TimeSensitiveStateIsThreadLocal )
{
    const auto dll = LoadLibraryW( L"xgameruntime.dll" );
    ASSERT_NE( dll, nullptr );
    using Query = HRESULT( WINAPI * )( REFCLSID, REFIID, void ** );
    const auto query = reinterpret_cast<Query>( GetProcAddress( dll, "QueryApiImpl" ) );
    ASSERT_NE( query, nullptr );
    IXThreadingImpl *api = nullptr;
    ASSERT_EQ( query( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &api ) ), S_OK );
    const auto ready = CreateEventW( nullptr, TRUE, FALSE, nullptr );
    const auto release = CreateEventW( nullptr, TRUE, FALSE, nullptr );
    ASSERT_NE( ready, nullptr );
    ASSERT_NE( release, nullptr );
    EXPECT_EQ( api->XThreadSetTimeSensitive( TRUE ), S_OK );
    std::thread other(
        [&]
        {
            EXPECT_FALSE( api->XThreadIsTimeSensitive() );
            EXPECT_EQ( api->XThreadSetTimeSensitive( FALSE ), S_OK );
            SetEvent( ready );
            EXPECT_EQ( WaitForSingleObject( release, 3000 ), WAIT_OBJECT_0 );
            EXPECT_EQ( api->XThreadSetTimeSensitive( TRUE ), S_OK );
            EXPECT_TRUE( api->XThreadIsTimeSensitive() );
        } );
    EXPECT_EQ( WaitForSingleObject( ready, 3000 ), WAIT_OBJECT_0 );
    EXPECT_TRUE( api->XThreadIsTimeSensitive() );
    SetEvent( release );
    other.join();
    EXPECT_TRUE( api->XThreadIsTimeSensitive() );
    EXPECT_EQ( api->XThreadSetTimeSensitive( FALSE ), S_OK );
    api->Release();
    CloseHandle( ready );
    CloseHandle( release );
    FreeLibrary( dll );
}
