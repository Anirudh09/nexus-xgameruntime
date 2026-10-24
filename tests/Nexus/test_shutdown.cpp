// SPDX-License-Identifier: LGPL-2.1-or-later
#include "../test_fixtures.h"
#include <windows.h>
#include <atomic>
#include <thread>
#include <vector>
namespace
{
using Callback = HRESULT( WINAPI * )( void * );
using Register = HRESULT( WINAPI * )( Callback, void * );
struct State
{
    Register registration{};
    std::atomic<unsigned> calls{}, failures{};
    HRESULT result = E_ABORT;
};
HRESULT WINAPI Drain( void *context )
{
    auto state = static_cast<State *>( context );
    ++state->calls;
    // Reentrant registration of the same immutable owner must not deadlock.
    if ( state->registration( Drain, state ) != S_OK ) ++state->failures;
    return state->result;
}
HRESULT WINAPI Other( void * )
{
    ADD_FAILURE() << "Foreign callback replaced the save provider";
    return E_FAIL;
}
}
TEST( NexusShutdown, ExplicitDrainPreservesOwnerFailureAndReentrantConcurrentRegistration )
{
    auto module = LoadLibraryW( L"xgameruntime.dll" );
    ASSERT_NE( module, nullptr );
    auto registration = reinterpret_cast<Register>( GetProcAddress( module, "NexusRegisterCloudShutdown" ) );
    auto shutdown = reinterpret_cast<HRESULT( WINAPI * )()>( GetProcAddress( module, "UninitializeApiImpl" ) );
    ASSERT_NE( registration, nullptr );
    ASSERT_NE( shutdown, nullptr );
    EXPECT_EQ( shutdown(), S_OK );
    EXPECT_EQ( registration( nullptr, nullptr ), E_INVALIDARG );
    static State state;
    state.registration = registration;
    ASSERT_EQ( registration( Drain, &state ), S_OK );
    EXPECT_EQ( registration( Drain, &state ), S_OK );
    EXPECT_EQ( registration( Other, &state ), HRESULT_FROM_WIN32( ERROR_ALREADY_EXISTS ) );
    EXPECT_EQ( registration( Drain, nullptr ), HRESULT_FROM_WIN32( ERROR_ALREADY_EXISTS ) );
    EXPECT_EQ( registration( nullptr, nullptr ), E_INVALIDARG );
    EXPECT_EQ( shutdown(), E_ABORT );
    EXPECT_EQ( state.calls, 1u );
    state.result = S_OK;
    std::vector<std::thread> threads;
    for ( unsigned i = 0; i < 8; ++i )
        threads.emplace_back(
            [&]
            {
                for ( unsigned j = 0; j < 32; ++j )
                    if ( registration( Drain, &state ) != S_OK || shutdown() != S_OK ) ++state.failures;
            } );
    for ( auto &thread : threads )
        thread.join();
    EXPECT_EQ( state.calls, 257u );
    EXPECT_EQ( state.failures, 0u );
    FreeLibrary( module );
}
