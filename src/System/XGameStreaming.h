// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xgamestreaming.h>
class XGameStreamingImpl final
    : public nexus::StaticComObject<IXGameStreamingImpl3, IXGameStreamingImpl2, IXGameStreamingImpl>
{
public:
    HRESULT WINAPI XGameStreamingInitialize() override;
    void WINAPI XGameStreamingUninitialize() override;
    BOOLEAN WINAPI XGameStreamingIsStreaming() override;
    HRESULT WINAPI XGameStreamingRegisterClientPropertiesChanged(
        XGameStreamingClientId client, XTaskQueueHandle queue, void *context,
        XGameStreamingClientPropertiesChangedCallback *callback, XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XGameStreamingUnregisterClientPropertiesChanged( XGameStreamingClientId client,
                                                                    XTaskQueueRegistrationToken token,
                                                                    BOOLEAN wait ) override;
    HRESULT WINAPI XGameStreamingGetStreamPhysicalDimensions( XGameStreamingClientId client, UINT32 *horizontalMm,
                                                              UINT32 *verticalMm ) override;
    UINT32 WINAPI XGameStreamingGetClientCount() override;
    HRESULT WINAPI XGameStreamingGetClients( UINT32 clientCount, XGameStreamingClientId *clients,
                                             UINT32 *clientsUsed ) override;
    XGameStreamingConnectionState WINAPI XGameStreamingGetConnectionState( XGameStreamingClientId client ) override;
    HRESULT WINAPI XGameStreamingRegisterConnectionStateChanged( XTaskQueueHandle queue, void *context,
                                                                 XGameStreamingConnectionStateChangedCallback *callback,
                                                                 XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XGameStreamingUnregisterConnectionStateChanged( XTaskQueueRegistrationToken token,
                                                                   BOOLEAN wait ) override;
    HRESULT WINAPI XGameStreamingGetStreamAddedLatency( XGameStreamingClientId client, UINT32 *averageInputLatencyUs,
                                                        UINT32 *averageOutputLatencyUs,
                                                        UINT32 *standardDeviationUs ) override;
    SIZE_T WINAPI XGameStreamingGetServerLocationNameSize() override;
    HRESULT WINAPI XGameStreamingGetServerLocationName( SIZE_T serverLocationNameSize,
                                                        char *serverLocationName ) override;
    void WINAPI XGameStreamingHideTouchControls() override;
    void WINAPI XGameStreamingShowTouchControlLayout( const char *layout ) override;
    void WINAPI XGameStreamingHideTouchControlsOnClient( XGameStreamingClientId client ) override;
    void WINAPI XGameStreamingShowTouchControlLayoutOnClient( XGameStreamingClientId client,
                                                              const char *layout ) override;
    HRESULT WINAPI XGameStreamingIsTouchInputEnabled( XGameStreamingClientId client,
                                                      BOOLEAN *touchInputEnabled ) override;
    HRESULT WINAPI XGameStreamingGetLastFrameDisplayed( XGameStreamingClientId client,
                                                        D3D12XBOX_FRAME_PIPELINE_TOKEN *framePipelineToken ) override;
    HRESULT WINAPI XGameStreamingGetAssociatedFrame( IGameInputReading *gamepadReading,
                                                     D3D12XBOX_FRAME_PIPELINE_TOKEN *framePipelineToken ) override;
    HRESULT WINAPI XGameStreamingGetGamepadPhysicality( IGameInputReading *gamepadReading,
                                                        XGameStreamingGamepadPhysicality *gamepadPhysicality ) override;
    HRESULT WINAPI XGameStreamingUpdateTouchControlsState(
        SIZE_T operationCount, const XGameStreamingTouchControlsStateOperation *operations ) override;
    HRESULT WINAPI XGameStreamingUpdateTouchControlsStateOnClient(
        XGameStreamingClientId client, SIZE_T operationCount,
        const XGameStreamingTouchControlsStateOperation *operations ) override;
    HRESULT WINAPI XGameStreamingShowTouchControlsWithStateUpdate(
        const char *layout, SIZE_T operationCount,
        const XGameStreamingTouchControlsStateOperation *operations ) override;
    HRESULT WINAPI XGameStreamingShowTouchControlsWithStateUpdateOnClient(
        XGameStreamingClientId client, const char *layout, SIZE_T operationCount,
        const XGameStreamingTouchControlsStateOperation *operations ) override;
    SIZE_T WINAPI XGameStreamingGetTouchBundleVersionNameSize( XGameStreamingClientId client ) override;
    HRESULT WINAPI XGameStreamingGetTouchBundleVersion( XGameStreamingClientId client, XVersion *version,
                                                        SIZE_T versionNameSize, char *versionName ) override;
    HRESULT WINAPI XGameStreamingGetClientIPAddress( XGameStreamingClientId client, SIZE_T ipAddressSize,
                                                     char *ipAddress ) override;
    HRESULT WINAPI XGameStreamingGetSessionId( XGameStreamingClientId client, SIZE_T sessionIdSize, char *sessionId,
                                               SIZE_T *sessionIdUsed ) override;
    HRESULT WINAPI XGameStreamingGetDisplayDetails( XGameStreamingClientId client, UINT32 maxSupportedPixels,
                                                    float widestSupportedAspectRatio, float tallestSupportedAspectRatio,
                                                    XGameStreamingDisplayDetails *displayDetails ) override;
    HRESULT WINAPI XGameStreamingSetResolution( UINT32 width, UINT32 height ) override;
};
IUnknown *GetGameStreamingInterface() noexcept;
