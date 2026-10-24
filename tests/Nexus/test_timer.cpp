// SPDX-License-Identifier: MIT
#include <gtest/gtest.h>
#include "../../src/System/Threading/WaitTimer.h"

TEST( NexusTimer, DeadlineClockIsWindowsMonotonicUptimeNotTheWallClock )
{
    OS::WaitTimer timer;
    const auto before = GetTickCount64();
    const auto current = timer.GetCurrentTime();
    const auto due = timer.GetDueTime( 1000 );
    const auto after = GetTickCount64();
    EXPECT_GE( current, before );
    EXPECT_LE( current, after );
    EXPECT_GE( due, before + 1000 );
    EXPECT_LE( due, after + 1000 );
}
TEST( NexusTimer, QueueNeverPublishesADelayedCallbackBeforeItsMonotonicDueTime )
{
    struct State
    {
        HANDLE event;
        ULONGLONG fired;
    } state{ CreateEventW( nullptr, FALSE, FALSE, nullptr ), 0 };
    ASSERT_NE( state.event, nullptr );
    OS::WaitTimer timer;
    InitializeLogging();
    IXThreadingImpl *threading = nullptr;
    ASSERT_EQ(
        QueryApiImpl( CLSID_XThreadingImpl, __uuidof( IXThreadingImpl ), reinterpret_cast<void **>( &threading ) ),
        S_OK );
    XTaskQueueHandle queue = nullptr;
    ASSERT_EQ(
        threading->XTaskQueueCreate( XTaskQueueDispatchMode::ThreadPool, XTaskQueueDispatchMode::Manual, &queue ),
        S_OK );
    for ( unsigned i = 0; i < 32; ++i )
    {
        const auto due = timer.GetDueTime( 10 );
        ASSERT_EQ( threading->XTaskQueueSubmitDelayedCallback( queue, XTaskQueuePort::Work, 10, &state,
                                                               []( void *context, BOOLEAN ) WINAPI
                                                               {
                                                                   auto state = static_cast<State *>( context );
                                                                   state->fired = GetTickCount64();
                                                                   SetEvent( state->event );
                                                               } ),
                   S_OK );
        EXPECT_EQ( WaitForSingleObject( state.event, 5000 ), WAIT_OBJECT_0 );
        EXPECT_GE( state.fired, due );
    }
    threading->XTaskQueueCloseHandle( queue );
    threading->Release();
    CloseHandle( state.event );
}
