// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include <xpackage.h>
#include <atomic>

class XPackageImpl final : public IXPackageImpl4
{
public:
    HRESULT WINAPI QueryInterface( REFIID iid, void **out ) noexcept override
    {
        if ( !out ) return E_POINTER;
        *out = nullptr;
        static constexpr GUID legacy = {
            0xbe3f3e9f, 0xc4b3, 0x424d, { 0x9a, 0x95, 0xd1, 0xaf, 0x06, 0x05, 0x59, 0xa5 } };
        if ( iid != __uuidof( IUnknown ) && iid != legacy && iid != __uuidof( IXPackageImpl ) &&
             iid != __uuidof( IXPackageImpl2 ) && iid != __uuidof( IXPackageImpl3 ) &&
             iid != __uuidof( IXPackageImpl4 ) )
            return E_NOINTERFACE;
        *out = static_cast<IXPackageImpl4 *>( this );
        AddRef();
        return S_OK;
    }
    ULONG WINAPI AddRef() noexcept override
    {
        return ++ref;
    }
    ULONG WINAPI Release() noexcept override
    {
        return --ref;
    }
    HRESULT WINAPI XPackageGetCurrentProcessPackageIdentifier( SIZE_T bufferSize, char *buffer ) override;
    BOOLEAN WINAPI XPackageIsPackagedProcess() override;
    HRESULT WINAPI XPackageCreateInstallationMonitor( const char *packageIdentifier, UINT32 selectorCount,
                                                      XPackageChunkSelector *selectors, UINT32 minimumUpdateIntervalMs,
                                                      XTaskQueueHandle queue,
                                                      XPackageInstallationMonitorHandle *installationMonitor ) override;
    void WINAPI
    XPackageCloseInstallationMonitorHandle( XPackageInstallationMonitorHandle installationMonitor ) override;
    void WINAPI XPackageGetInstallationProgress( XPackageInstallationMonitorHandle installationMonitor,
                                                 XPackageInstallationProgress *progress ) override;
    BOOLEAN WINAPI XPackageUpdateInstallationMonitor( XPackageInstallationMonitorHandle installationMonitor ) override;
    HRESULT WINAPI XPackageRegisterInstallationProgressChanged( XPackageInstallationMonitorHandle installationMonitor,
                                                                void *context,
                                                                XPackageInstallationProgressCallback *callback,
                                                                XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XPackageUnregisterInstallationProgressChanged( XPackageInstallationMonitorHandle installationMonitor,
                                                                  XTaskQueueRegistrationToken token,
                                                                  BOOLEAN wait ) override;
    HRESULT WINAPI XPackageGetUserLocale( SIZE_T localeSize, char *locale ) override;
    HRESULT WINAPI XPackageFindChunkAvailability( const char *packageIdentifier, UINT32 selectorCount,
                                                  XPackageChunkSelector *selectors,
                                                  XPackageChunkAvailability *availability ) override;
    HRESULT WINAPI XPackageEnumerateChunkAvailability( const char *packageIdentifier, XPackageChunkSelectorType type,
                                                       void *context,
                                                       XPackageChunkAvailabilityCallback *callback ) override;
    HRESULT WINAPI XPackageChangeChunkInstallOrder( const char *packageIdentifier, UINT32 selectorCount,
                                                    XPackageChunkSelector *selectors ) override;
    HRESULT WINAPI XPackageInstallChunks( const char *packageIdentifier, UINT32 selectorCount,
                                          XPackageChunkSelector *selectors, UINT32 minimumUpdateIntervalMs,
                                          BOOLEAN suppressUserConfirmation, XTaskQueueHandle queue,
                                          XPackageInstallationMonitorHandle *installationMonitor ) override;
    HRESULT WINAPI XPackageInstallChunksAsync( const char *packageIdentifier, UINT32 selectorCount,
                                               XPackageChunkSelector *selectors, UINT32 minimumUpdateIntervalMs,
                                               BOOLEAN suppressUserConfirmation, XAsyncBlock *asyncBlock ) override;
    HRESULT WINAPI XPackageInstallChunksResult( XAsyncBlock *asyncBlock,
                                                XPackageInstallationMonitorHandle *installationMonitor ) override;
    HRESULT WINAPI XPackageEstimateDownloadSize( const char *packageIdentifier, UINT32 selectorCount,
                                                 XPackageChunkSelector *selectors, UINT64 *downloadSize,
                                                 BOOLEAN *shouldPresentUserConfirmation ) override;
    HRESULT WINAPI XPackageUninstallChunks( const char *packageIdentifier, UINT32 selectorCount,
                                            XPackageChunkSelector *selectors ) override;
    HRESULT WINAPI XPackageEnumeratePackagesLegacy( XPackageKind kind, XPackageEnumerationScope scope, void *context,
                                                    XPackageEnumerationCallback *callback ) override;
    HRESULT WINAPI XPackageRegisterPackageInstalledLegacy( XTaskQueueHandle queue, void *context,
                                                           XPackageInstalledCallback *callback,
                                                           XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XPackageUnregisterPackageInstalled( XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
    HRESULT WINAPI XPackageMountLegacy( const char *identifier, XPackageMountHandle *mount ) override;
    HRESULT WINAPI XPackageGetMountPathSize( XPackageMountHandle mount, SIZE_T *pathSize ) override;
    HRESULT WINAPI XPackageGetMountPath( XPackageMountHandle mount, SIZE_T pathSize, char *path ) override;
    void WINAPI XPackageCloseMountHandle( XPackageMountHandle mount ) override;
    HRESULT WINAPI XPackageGetIdentifierLegacy( const char *store, SIZE_T size, char *identifier ) override;
    HRESULT WINAPI XPackageEnumeratePackages( XPackageKind kind, XPackageEnumerationScope scope, void *context,
                                              XPackageEnumerationCallback *callback ) override;
    HRESULT WINAPI XPackageRegisterPackageInstalled( XTaskQueueHandle queue, void *context,
                                                     XPackageInstalledCallback *callback,
                                                     XTaskQueueRegistrationToken *token ) override;
    HRESULT WINAPI XPackageGetWriteStats( XPackageWriteStats *writeStats ) override;
    HRESULT WINAPI XPackageEnumerateFeaturesLegacy( const char *identifier, void *context,
                                                    XPackageFeatureEnumerationCallback *callback ) override;
    HRESULT WINAPI XPackageUninstallUWPInstance( const char *packageName ) override;
    HRESULT WINAPI XPackageEnumerateFeatures( const char *packageIdentifier, void *context,
                                              XPackageFeatureEnumerationCallback *callback ) override;
    BOOLEAN WINAPI XPackageUninstallPackage( const char *packageIdentifier ) override;
    HRESULT WINAPI XPackageEnumeratePackages2( XPackageKind kind, XPackageEnumerationScope scope, void *context,
                                               XPackageEnumerationCallback *callback ) override;
    HRESULT WINAPI XPackageRegisterPackageInstalled2( XTaskQueueHandle queue, void *context,
                                                      XPackageInstalledCallback *callback,
                                                      XTaskQueueRegistrationToken *token ) override;
    HRESULT WINAPI XPackageMountWithUiAsync( const char *packageIdentifier, XAsyncBlock *async ) override;
    HRESULT WINAPI XPackageMountWithUiResult( XAsyncBlock *async, XPackageMountHandle *mount ) override;
    HRESULT WINAPI XPackageEnumeratePackages3( XPackageKind kind, XPackageEnumerationScope scope, void *context,
                                               XPackageEnumerationCallback *callback ) override;
    HRESULT WINAPI XPackageRegisterPackageInstalled3( XTaskQueueHandle queue, void *context,
                                                      XPackageInstalledCallback *callback,
                                                      XTaskQueueRegistrationToken *token ) override;
    HRESULT WINAPI XPackageGetKind( const char *packageIdentifier, XPackageKind *kind ) override;

private:
    std::atomic<ULONG> ref{ 1 };
};
IXPackageImpl *GetPackageInterface() noexcept;
HRESULT xpackage_query_identifier( const char *, SIZE_T, char * );
HRESULT xpackage_query_content_store( const char *, SIZE_T, char * );
