/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XGameStreaming
 *
 * Copyright 2026 Olivia Ryan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "XGameStreaming.h"

HRESULT WINAPI XGameStreamingImpl::XGameStreamingInitialize()
{
    NEXUS_STORE_META( 3, E_NOTIMPL, 0, nullptr );
    return E_NOTIMPL;
}

void WINAPI XGameStreamingImpl::XGameStreamingUninitialize()
{
    ( void )0;
}

BOOLEAN WINAPI XGameStreamingImpl::XGameStreamingIsStreaming()
{
    ( void )0;
    return FALSE;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingRegisterClientPropertiesChanged(
    XGameStreamingClientId client, XTaskQueueHandle queue, void *context,
    XGameStreamingClientPropertiesChangedCallback *callback, XTaskQueueRegistrationToken *token )
{
    ( void )0;
    return E_NOTIMPL;
}

BOOLEAN WINAPI XGameStreamingImpl::XGameStreamingUnregisterClientPropertiesChanged( XGameStreamingClientId client,
                                                                                    XTaskQueueRegistrationToken token,
                                                                                    BOOLEAN wait )
{
    ( void )0;
    return FALSE;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetStreamPhysicalDimensions( XGameStreamingClientId client,
                                                                              UINT32 *horizontalMm, UINT32 *verticalMm )
{
    ( void )0;
    return E_NOTIMPL;
}

UINT32 WINAPI XGameStreamingImpl::XGameStreamingGetClientCount()
{
    ( void )0;
    return 0;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetClients( UINT32 clientCount, XGameStreamingClientId *clients,
                                                             UINT32 *clientsUsed )
{
    ( void )0;
    return E_NOTIMPL;
}

XGameStreamingConnectionState WINAPI
XGameStreamingImpl::XGameStreamingGetConnectionState( XGameStreamingClientId client )
{
    ( void )0;
    return XGameStreamingConnectionState::Disconnected;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingRegisterConnectionStateChanged(
    XTaskQueueHandle queue, void *context, XGameStreamingConnectionStateChangedCallback *callback,
    XTaskQueueRegistrationToken *token )
{
    ( void )0;
    return E_NOTIMPL;
}

BOOLEAN WINAPI XGameStreamingImpl::XGameStreamingUnregisterConnectionStateChanged( XTaskQueueRegistrationToken token,
                                                                                   BOOLEAN wait )
{
    ( void )0;
    return FALSE;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetStreamAddedLatency( XGameStreamingClientId client,
                                                                        UINT32 *averageInputLatencyUs,
                                                                        UINT32 *averageOutputLatencyUs,
                                                                        UINT32 *standardDeviationUs )
{
    ( void )0;
    return E_NOTIMPL;
}

SIZE_T WINAPI XGameStreamingImpl::XGameStreamingGetServerLocationNameSize()
{
    ( void )0;
    return 0;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetServerLocationName( SIZE_T serverLocationNameSize,
                                                                        char *serverLocationName )
{
    ( void )0;
    return E_NOTIMPL;
}

void WINAPI XGameStreamingImpl::XGameStreamingHideTouchControls()
{
    ( void )0;
}

void WINAPI XGameStreamingImpl::XGameStreamingShowTouchControlLayout( const char *layout )
{
    ( void )0;
}

void WINAPI XGameStreamingImpl::XGameStreamingHideTouchControlsOnClient( XGameStreamingClientId client )
{
    ( void )0;
}

void WINAPI XGameStreamingImpl::XGameStreamingShowTouchControlLayoutOnClient( XGameStreamingClientId client,
                                                                              const char *layout )
{
    ( void )0;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingIsTouchInputEnabled( XGameStreamingClientId client,
                                                                      BOOLEAN *touchInputEnabled )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetLastFrameDisplayed(
    XGameStreamingClientId client, D3D12XBOX_FRAME_PIPELINE_TOKEN *framePipelineToken )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetAssociatedFrame(
    IGameInputReading *gamepadReading, D3D12XBOX_FRAME_PIPELINE_TOKEN *framePipelineToken )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetGamepadPhysicality(
    IGameInputReading *gamepadReading, XGameStreamingGamepadPhysicality *gamepadPhysicality )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingUpdateTouchControlsState(
    SIZE_T operationCount, const XGameStreamingTouchControlsStateOperation *operations )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingUpdateTouchControlsStateOnClient(
    XGameStreamingClientId client, SIZE_T operationCount, const XGameStreamingTouchControlsStateOperation *operations )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingShowTouchControlsWithStateUpdate(
    const char *layout, SIZE_T operationCount, const XGameStreamingTouchControlsStateOperation *operations )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingShowTouchControlsWithStateUpdateOnClient(
    XGameStreamingClientId client, const char *layout, SIZE_T operationCount,
    const XGameStreamingTouchControlsStateOperation *operations )
{
    ( void )0;
    return E_NOTIMPL;
}

SIZE_T WINAPI XGameStreamingImpl::XGameStreamingGetTouchBundleVersionNameSize( XGameStreamingClientId client )
{
    ( void )0;
    return 0;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetTouchBundleVersion( XGameStreamingClientId client,
                                                                        XVersion *version, SIZE_T versionNameSize,
                                                                        char *versionName )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetClientIPAddress( XGameStreamingClientId client,
                                                                     SIZE_T ipAddressSize, char *ipAddress )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetSessionId( XGameStreamingClientId client, SIZE_T sessionIdSize,
                                                               char *sessionId, SIZE_T *sessionIdUsed )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingGetDisplayDetails( XGameStreamingClientId client,
                                                                    UINT32 maxSupportedPixels,
                                                                    float widestSupportedAspectRatio,
                                                                    float tallestSupportedAspectRatio,
                                                                    XGameStreamingDisplayDetails *displayDetails )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XGameStreamingImpl::XGameStreamingSetResolution( UINT32 width, UINT32 height )
{
    ( void )0;
    return E_NOTIMPL;
}

IUnknown *GetGameStreamingInterface() noexcept
{
    static XGameStreamingImpl value;
    return &value;
}
