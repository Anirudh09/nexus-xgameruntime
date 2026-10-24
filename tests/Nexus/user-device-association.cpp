/* SPDX-License-Identifier: MIT
 * Real DLL ABI plus the same registration engine with allocation-failure tests.
 * No account, device association, synthetic event, game or network operation. */
#define INITGUID
#define __WINESRC__
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <type_traits>
#include "xuser.h"
#include "xtaskqueue.h"
#include "xasyncprovider.h"

static HRESULT( WINAPI *native_query )( REFCLSID, REFIID, void ** );
static IXUserDeviceImpl2 *device;
static IXThreadingImpl *threading;
static LONG checks, failures, callbacks, allocations, fail_allocation, fail_query;
#define CHECK( name, test )                                                                                            \
    do                                                                                                                 \
    {                                                                                                                  \
        InterlockedIncrement( &checks );                                                                               \
        if ( !( test ) )                                                                                               \
        {                                                                                                              \
            InterlockedIncrement( &failures );                                                                         \
            printf( "FAIL %s line=%d\n", name, __LINE__ );                                                             \
        }                                                                                                              \
    } while ( 0 )

static HRESULT QueryApiImpl( REFCLSID clsid, REFIID iid, void **out )
{
    if ( fail_query )
    {
        *out = NULL;
        return E_NOINTERFACE;
    }
    return native_query( clsid, iid, out );
}
static void *fixture_calloc( size_t count, size_t size )
{
    void *value;
    if ( InterlockedExchange( &fail_allocation, 0 ) ) return NULL;
    value = static_cast<void *>( calloc( count, size ) );
    if ( value ) InterlockedIncrement( &allocations );
    return value;
}
static void fixture_free( void *value )
{
    if ( value ) InterlockedDecrement( &allocations );
    free( value );
}
#define calloc fixture_calloc
#define free fixture_free
#include "../../src/System/User/DeviceAssociations.h"
#undef calloc
#undef free

static void WINAPI changed( void *context, const XUserDeviceAssociationChange *change )
{
    ( void )context;
    ( void )change;
    InterlockedIncrement( &callbacks );
}
static HRESULT reg( BOOL engine, XTaskQueueHandle queue, void *context, XUserDeviceAssociationChangedCallback *callback,
                    XTaskQueueRegistrationToken *token )
{
    return engine ? device_association_register( queue, context, callback, token )
                  : ( device )->XUserRegisterForDeviceAssociationChanged( queue, context, callback, token );
}
static BOOLEAN unreg( BOOL engine, XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    return engine ? device_association_unregister( token, wait )
                  : ( device )->XUserUnregisterForDeviceAssociationChanged( token, wait );
}
static XTaskQueueHandle queue( XTaskQueueDispatchMode mode )
{
    XTaskQueueHandle value = NULL;
    CHECK( "queue-create", ( threading )->XTaskQueueCreate( mode, mode, &value ) == S_OK && value );
    return value;
}
static void contracts( BOOL engine )
{
    XTaskQueueRegistrationToken token = { 0 }, second = { 0 }, unknown = { ~( UINT64 )0 };
    XTaskQueueHandle q, saved = NULL;
    int context = 42;
    CHECK( "null-token", reg( engine, NULL, NULL, changed, NULL ) == E_INVALIDARG );
    token.token = 123;
    CHECK( "null-callback", reg( engine, NULL, NULL, NULL, &token ) == E_INVALIDARG && !token.token );
    CHECK( "zero-token", !unreg( engine, token, TRUE ) );
    CHECK( "unknown-token", !unreg( engine, unknown, FALSE ) );
    CHECK( "process-queue-save", ( threading )->XTaskQueueGetCurrentProcessTaskQueue( &saved ) && saved );
    ( threading )->XTaskQueueSetCurrentProcessTaskQueue( NULL );
    token.token = 123;
    CHECK( "missing-process-queue",
           reg( engine, NULL, NULL, changed, &token ) == HRESULT_FROM_WIN32( ERROR_NO_TASK_QUEUE ) && !token.token );
    ( threading )->XTaskQueueSetCurrentProcessTaskQueue( saved );
    ( threading )->XTaskQueueCloseHandle( saved );
    CHECK( "default-queue", reg( engine, NULL, &context, changed, &token ) == S_OK && token.token );
    CHECK( "default-remove", unreg( engine, token, TRUE ) );
    CHECK( "repeat-remove", !unreg( engine, token, TRUE ) );
    for ( UINT mode = 0; mode < 4; mode++ )
    {
        q = queue( ( XTaskQueueDispatchMode )mode );
        CHECK( "register", reg( engine, q, &context, changed, &token ) == S_OK && token.token );
        CHECK( "second-register",
               reg( engine, q, NULL, changed, &second ) == S_OK && second.token && second.token != token.token );
        if ( engine )
        {
            CHECK( "stored-callback", device_association_registrations->callback == changed );
            CHECK( "stored-context", device_association_registrations->next->context == &context );
            CHECK( "queue-retained", device_association_registrations->queue != NULL );
        }
        CHECK( "no-registration-event", !( threading )->XTaskQueueDispatch( q, XTaskQueuePort::Completion, 0 ) );
        CHECK( "remove-no-wait", unreg( engine, token, FALSE ) );
        CHECK( "other-registration-remains", unreg( engine, second, TRUE ) );
        CHECK( "already-removed", !unreg( engine, token, FALSE ) );
        CHECK( "retain-before-close", reg( engine, q, &context, changed, &token ) == S_OK && token.token );
        ( threading )->XTaskQueueCloseHandle( q );
        if ( engine )
            CHECK( "retained-queue-usable", !( threading )
                                                 ->XTaskQueueDispatch( device_association_registrations->queue,
                                                                       XTaskQueuePort::Completion, 0 ) );
        CHECK( "remove-after-caller-close", unreg( engine, token, TRUE ) );
        CHECK( "no-synthetic-events", !callbacks );
    }
}
struct concurrent
{
    BOOL engine;
    XTaskQueueHandle queue;
    XTaskQueueRegistrationToken token;
    UINT index;
    LONG wins;
    UINT64 tokens[128];
};
static DWORD WINAPI registration_thread( void *arg )
{
    auto state = static_cast<concurrent *>( arg );
    for ( UINT i = 0; i < 128; i++ )
    {
        XTaskQueueRegistrationToken token = { 0 };
        HRESULT hr = reg( state->engine, state->queue, state, changed, &token );
        CHECK( "parallel-register", hr == S_OK && token.token );
        state->tokens[i] = token.token;
        if ( SUCCEEDED( hr ) ) CHECK( "parallel-remove", unreg( state->engine, token, ( i & 1 ) != 0 ) );
    }
    return 0;
}
static DWORD WINAPI removal_thread( void *arg )
{
    auto state = static_cast<concurrent *>( arg );
    if ( unreg( state->engine, state->token, ( state->index & 1 ) != 0 ) ) InterlockedIncrement( &state->wins );
    return 0;
}
static int compare_tokens( const void *a, const void *b )
{
    UINT64 left = *( const UINT64 * )a, right = *( const UINT64 * )b;
    return ( left > right ) - ( left < right );
}
static void parallel( BOOL engine )
{
    struct concurrent state[8] = { 0 };
    HANDLE threads[8];
    XTaskQueueHandle q = queue( XTaskQueueDispatchMode::Manual );
    XTaskQueueRegistrationToken token = { 0 };
    LONG wins = 0;
    UINT64 tokens[1024];
    for ( UINT i = 0; i < 8; i++ )
    {
        state[i].engine = engine;
        state[i].queue = q;
        state[i].index = i;
        threads[i] = CreateThread( NULL, 0, registration_thread, &state[i], 0, NULL );
        CHECK( "create-thread", threads[i] != NULL );
    }
    CHECK( "join-threads", WaitForMultipleObjects( 8, threads, TRUE, 15000 ) == WAIT_OBJECT_0 );
    for ( UINT i = 0; i < 8; i++ )
        CloseHandle( threads[i] );
    for ( UINT i = 0; i < 1024; i++ )
        tokens[i] = state[i / 128].tokens[i % 128];
    qsort( tokens, 1024, sizeof( *tokens ), compare_tokens );
    CHECK( "nonzero-tokens", tokens[0] != 0 );
    for ( UINT i = 1; i < 1024; i++ )
        CHECK( "unique-token", tokens[i - 1] != tokens[i] );
    CHECK( "race-registration", reg( engine, q, NULL, changed, &token ) == S_OK && token.token );
    for ( UINT i = 0; i < 8; i++ )
    {
        state[i].token = token;
        threads[i] = CreateThread( NULL, 0, removal_thread, &state[i], 0, NULL );
        CHECK( "create-remover", threads[i] != NULL );
    }
    CHECK( "join-removers", WaitForMultipleObjects( 8, threads, TRUE, 15000 ) == WAIT_OBJECT_0 );
    for ( UINT i = 0; i < 8; i++ )
    {
        CloseHandle( threads[i] );
        wins += state[i].wins;
    }
    CHECK( "one-remover-wins", wins == 1 );
    ( threading )->XTaskQueueCloseHandle( q );
    CHECK( "parallel-no-events", !callbacks );
}
int main( void )
{
    HMODULE dll = LoadLibraryA( "xgameruntime.dll" );
    XTaskQueueRegistrationToken token = { 0 };
    XTaskQueueHandle q;
    IXUserDeviceImpl *old = NULL;
    UINT64 saved;
    if ( !dll ||
         !( native_query = reinterpret_cast<decltype( native_query )>( GetProcAddress( dll, "QueryApiImpl" ) ) ) ||
         FAILED( native_query( CLSID_XUserDeviceImpl, IID_IXUserDeviceImpl2, ( void ** )&device ) ) ||
         FAILED( native_query( CLSID_XThreadingImpl, IID_IXThreadingImpl, ( void ** )&threading ) ) )
        return 2;
    CHECK( "legacy-interface",
           native_query( CLSID_XUserDeviceImpl, IID_IXUserDeviceImpl, ( void ** )&old ) == S_OK && old );
    if ( old ) ( old )->Release();
    contracts( FALSE );
    contracts( TRUE );
    q = queue( XTaskQueueDispatchMode::Manual );
    fail_query = 1;
    token.token = 123;
    CHECK( "query-failure",
           device_association_register( q, NULL, changed, &token ) == E_NOINTERFACE && !token.token && !allocations );
    fail_query = 0;
    fail_allocation = 1;
    token.token = 123;
    CHECK( "allocation-failure",
           device_association_register( q, NULL, changed, &token ) == E_OUTOFMEMORY && !token.token && !allocations );
    saved = device_association_next_token;
    device_association_next_token = ~( UINT64 )0;
    token.token = 123;
    CHECK( "token-exhaustion",
           device_association_register( q, NULL, changed, &token ) == HRESULT_FROM_WIN32( ERROR_ARITHMETIC_OVERFLOW ) &&
               !token.token && !allocations );
    device_association_next_token = saved;
    ( threading )->XTaskQueueCloseHandle( q );
    parallel( FALSE );
    parallel( TRUE );
    CHECK( "registry-empty", !device_association_registrations && !allocations );
    CHECK( "never-invent-events", !callbacks );
    ( device )->Release();
    ( threading )->Release();
    printf( "CHECKS %ld FAILURES %ld\n", checks, failures );
    return failures ? 1 : 0;
}
