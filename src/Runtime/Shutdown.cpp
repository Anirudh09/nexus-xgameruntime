// SPDX-License-Identifier: LGPL-2.1-or-later
#include "Shutdown.h"
namespace nexus
{
namespace
{
SRWLOCK lock = SRWLOCK_INIT;
CloudShutdownCallback callback = nullptr;
void *context = nullptr;
}
HRESULT RegisterCloudShutdown( CloudShutdownCallback next, void *data ) noexcept
{
    if ( !next ) return E_INVALIDARG;
    HRESULT result = S_OK;
    AcquireSRWLockExclusive( &lock );
    if ( callback && ( callback != next || context != data ) )
        result = HRESULT_FROM_WIN32( ERROR_ALREADY_EXISTS );
    else
    {
        callback = next;
        context = data;
    }
    ReleaseSRWLockExclusive( &lock );
    return result;
}
HRESULT DrainCloudShutdown()
{
    AcquireSRWLockShared( &lock );
    auto function = callback;
    auto data = context;
    ReleaseSRWLockShared( &lock );
    // This explicit synchronous shutdown is the last safe upload drain point.
    // Never wait on network or worker threads while inside DllMain.
    return function ? function( data ) : S_OK;
}
}
