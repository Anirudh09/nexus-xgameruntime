// SPDX-License-Identifier: MIT
#include "../test_fixtures.h"
#include <windows.h>
#include <inspectable.h>
#include <xasyncprovider.h>
#include <xtaskqueue.h>

TEST( NexusCom, PublicWrappersBalanceReturnedInterfaceReferences )
{
    auto dll = LoadLibraryW( L"xgameruntime.dll" );
    ASSERT_NE( dll, nullptr );
    using Query = HRESULT( WINAPI * )( REFCLSID, REFIID, void ** );
    auto query = reinterpret_cast<Query>( GetProcAddress( dll, "QueryApiImpl" ) );
    ASSERT_NE( query, nullptr );
    IXThreadingImpl *api = nullptr;
    ASSERT_EQ( query( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &api ) ), S_OK );
    const auto before = api->AddRef();
    api->Release();
    for ( unsigned i = 0; i < 32; ++i )
    {
        EXPECT_EQ( XThreadSetTimeSensitive( FALSE ), S_OK );
        EXPECT_FALSE( XThreadIsTimeSensitive() );
    }
    const auto after = api->AddRef();
    api->Release();
    EXPECT_EQ( after, before );
    api->Release();
    FreeLibrary( dll );
}
TEST( NexusCom, UnknownOnlyProviderMustNotAdvertiseAnInspectableVtable )
{
    auto dll = LoadLibraryW( L"xgameruntime.dll" );
    ASSERT_NE( dll, nullptr );
    using Query = HRESULT( WINAPI * )( REFCLSID, REFIID, void ** );
    auto query = reinterpret_cast<Query>( GetProcAddress( dll, "QueryApiImpl" ) );
    ASSERT_NE( query, nullptr );
    void *output = reinterpret_cast<void *>( 1 );
    HRESULT hr = query( CLSID_XThreadingImpl, __uuidof( IInspectable ), &output );
    EXPECT_EQ( hr, E_NOINTERFACE );
    EXPECT_EQ( output, nullptr );
    if ( SUCCEEDED( hr ) && output ) static_cast<IUnknown *>( output )->Release();
    FreeLibrary( dll );
}
