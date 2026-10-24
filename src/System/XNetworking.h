// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xnetworking.h>
class XNetworkingImpl final : public nexus::StaticComObject<IXNetworkingImpl2, IXNetworkingImpl>
{
public:
    HRESULT WINAPI
    XNetworkingQueryPreferredLocalUdpMultiplayerPort( UINT16 *preferredLocalUdpMultiplayerPort ) override;
    HRESULT WINAPI XNetworkingQueryPreferredLocalUdpMultiplayerPortAsync( XAsyncBlock *asyncBlock ) override;
    HRESULT WINAPI XNetworkingQueryPreferredLocalUdpMultiplayerPortAsyncResult(
        XAsyncBlock *asyncBlock, UINT16 *preferredLocalUdpMultiplayerPort ) override;
    HRESULT WINAPI XNetworkingRegisterPreferredLocalUdpMultiplayerPortChanged(
        XTaskQueueHandle queue, void *context, XNetworkingPreferredLocalUdpMultiplayerPortChangedCallback *callback,
        XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XNetworkingUnregisterPreferredLocalUdpMultiplayerPortChanged( XTaskQueueRegistrationToken token,
                                                                                 BOOLEAN wait ) override;
    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlAsync( const char *url, XAsyncBlock *asyncBlock ) override;
    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlAsyncResultSize(
        XAsyncBlock *asyncBlock, SIZE_T *securityInformationBufferByteCount ) override;
    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlAsyncResult(
        XAsyncBlock *asyncBlock, SIZE_T securityInformationBufferByteCount,
        SIZE_T *securityInformationBufferByteCountUsed, UINT8 *securityInformationBuffer,
        XNetworkingSecurityInformation **securityInformation ) override;
    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlUtf16Async( const WCHAR *url,
                                                                        XAsyncBlock *asyncBlock ) override;
    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlUtf16AsyncResultSize(
        XAsyncBlock *asyncBlock, SIZE_T *securityInformationBufferByteCount ) override;
    HRESULT WINAPI XNetworkingQuerySecurityInformationForUrlUtf16AsyncResult(
        XAsyncBlock *asyncBlock, SIZE_T securityInformationBufferByteCount,
        SIZE_T *securityInformationBufferByteCountUsed, UINT8 *securityInformationBuffer,
        XNetworkingSecurityInformation **securityInformation ) override;
    HRESULT WINAPI XNetworkingVerifyServerCertificate(
        void *requestHandle, const XNetworkingSecurityInformation *securityInformation ) override;
    HRESULT WINAPI XNetworkingGetConnectivityHint( XNetworkingConnectivityHint *connectivityHint ) override;
    HRESULT WINAPI XNetworkingRegisterConnectivityHintChanged( XTaskQueueHandle queue, void *context,
                                                               XNetworkingConnectivityHintChangedCallback *callback,
                                                               XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XNetworkingUnregisterConnectivityHintChanged( XTaskQueueRegistrationToken token,
                                                                 BOOLEAN wait ) override;
    HRESULT WINAPI XNetworkingQueryConfigurationSetting( XNetworkingConfigurationSetting configurationSetting,
                                                         UINT64 *value ) override;
    HRESULT WINAPI XNetworkingSetConfigurationSetting( XNetworkingConfigurationSetting configurationParameter,
                                                       UINT64 value ) override;
    HRESULT WINAPI XNetworkingQueryStatistics( XNetworkingStatisticsType statisticsType,
                                               XNetworkingStatisticsBuffer *statisticsBuffer ) override;
};
IXNetworkingImpl2 *GetNetworkingInterface() noexcept;
