// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include "../../src/private.h"
#include "../../src/System/Threading/AsyncIdentity.h"
#include <atomic>
#include <thread>
#include <vector>
namespace
{
const char firstIdentity = 0, secondIdentity = 0;
struct Operation
{
    std::atomic<unsigned> cleanups{};
};
HRESULT WINAPI Provider( XAsyncOp op, const XAsyncProviderData *data )
{
    if ( op == XAsyncOp::Cleanup ) ++static_cast<Operation *>( data->context )->cleanups;
    if ( op == XAsyncOp::GetResult )
    {
        *static_cast<DWORD *>( data->buffer ) = 42;
        return S_OK;
    }
    return S_OK;
}
struct Queue
{
    IXThreadingImpl *api{};
    XTaskQueueHandle queue{};
    Queue()
    {
        InitializeLogging();
        EXPECT_EQ( QueryApiImpl( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &api ) ),
                   S_OK );
        if ( api )
            EXPECT_EQ( api->XTaskQueueCreate( XTaskQueueDispatchMode::Manual, XTaskQueueDispatchMode::Manual, &queue ),
                       S_OK );
    }
    ~Queue()
    {
        if ( queue ) api->XTaskQueueCloseHandle( queue );
        if ( api ) api->Release();
    }
};
}
TEST( NexusAsyncIdentity, PendingAndPayloadFreeTerminalStatusStayBoundWithoutConsuming )
{
    Queue fixture;
    ASSERT_NE( fixture.queue, nullptr );
    Operation state;
    XAsyncBlock block{};
    block.queue = fixture.queue;
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &firstIdentity ), E_INVALIDARG );
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( nullptr, &firstIdentity ), E_INVALIDARG );
    ASSERT_EQ( fixture.api->XAsyncBegin( &block, &state, &firstIdentity, "Empty result", Provider ), S_OK );
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &firstIdentity ), E_PENDING );
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &secondIdentity ), E_INVALIDARG );
    fixture.api->XAsyncComplete( &block, E_ACCESSDENIED, 0 );
    for ( unsigned i = 0; i < 32; ++i )
    {
        EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &firstIdentity ), E_ACCESSDENIED );
        EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &secondIdentity ), E_INVALIDARG );
    }
    EXPECT_EQ( state.cleanups, 1u );
    ASSERT_EQ( fixture.api->XAsyncBegin( &block, &state, &secondIdentity, "Reused result", Provider ), S_OK );
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &firstIdentity ), E_INVALIDARG );
    fixture.api->XAsyncComplete( &block, S_OK, 0 );
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &secondIdentity ), S_OK );
    EXPECT_EQ( state.cleanups, 2u );
}
TEST( NexusAsyncIdentity, AConsumedPayloadRetainsOnlyItsExactIdentityAndTerminalStatus )
{
    Queue fixture;
    ASSERT_NE( fixture.queue, nullptr );
    Operation state;
    XAsyncBlock block{};
    block.queue = fixture.queue;
    ASSERT_EQ( fixture.api->XAsyncBegin( &block, &state, &firstIdentity, "Typed result", Provider ), S_OK );
    fixture.api->XAsyncComplete( &block, S_OK, sizeof( DWORD ) );
    DWORD value = 0;
    ASSERT_EQ( fixture.api->XAsyncGetResult( &block, &firstIdentity, sizeof( value ), &value, nullptr ), S_OK );
    EXPECT_EQ( value, 42u );
    EXPECT_EQ( state.cleanups, 1u );
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &firstIdentity ), S_OK );
    EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &secondIdentity ), E_INVALIDARG );
}
TEST( NexusAsyncIdentity, ConcurrentCompletionNeverInterpretsTerminalIdentityAsLiveState )
{
    Queue fixture;
    ASSERT_NE( fixture.queue, nullptr );
    Operation state;
    XAsyncBlock blocks[256]{};
    for ( auto &block : blocks )
    {
        block.queue = fixture.queue;
        ASSERT_EQ( fixture.api->XAsyncBegin( &block, &state, &firstIdentity, "Race", Provider ), S_OK );
    }
    std::atomic<bool> ready = false, finished = false;
    std::atomic<unsigned> failures{}, reads{};
    std::vector<std::thread> readers;
    for ( unsigned i = 0; i < 8; ++i )
        readers.emplace_back(
            [&, i]
            {
                while ( !ready.load() )
                    std::this_thread::yield();
                unsigned index = i;
                do
                {
                    auto &block = blocks[index++ % 256];
                    auto result = nexus::GetAsyncStatusForIdentity( &block, &firstIdentity );
                    if ( result != E_PENDING && result != E_FAIL ) ++failures;
                    if ( nexus::GetAsyncStatusForIdentity( &block, &secondIdentity ) != E_INVALIDARG ) ++failures;
                    ++reads;
                } while ( !finished.load() );
            } );
    ready = true;
    for ( auto &block : blocks )
        fixture.api->XAsyncComplete( &block, E_FAIL, 0 );
    finished = true;
    for ( auto &thread : readers )
        thread.join();
    EXPECT_EQ( failures, 0u );
    EXPECT_GE( reads, 8u );
    EXPECT_EQ( state.cleanups, 256u );
    for ( auto &block : blocks )
        EXPECT_EQ( nexus::GetAsyncStatusForIdentity( &block, &firstIdentity ), E_FAIL );
}
