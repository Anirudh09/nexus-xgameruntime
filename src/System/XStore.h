// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xstore.h>
class XStoreImpl final
    : public nexus::StaticComObject<IXStoreImpl6, IXStoreImpl, IXStoreImpl2, IXStoreImpl3, IXStoreImpl4, IXStoreImpl5>
{
public:
    HRESULT WINAPI XStoreCreateContext( const XUserHandle user, XStoreContextHandle *storeContextHandle ) override;
    void WINAPI XStoreCloseContextHandle( XStoreContextHandle storeContextHandle ) override;
    HRESULT WINAPI XStoreQueryAssociatedProductsAsync( const XStoreContextHandle storeContextHandle,
                                                       XStoreProductKind productKinds, UINT32 maxItemsToRetrievePerPage,
                                                       XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryAssociatedProductsResult( XAsyncBlock *async,
                                                        XStoreProductQueryHandle *productQueryHandle ) override;
    HRESULT WINAPI XStoreQueryProductsAsync( const XStoreContextHandle storeContextHandle,
                                             XStoreProductKind productKinds, const char **storeIds,
                                             SIZE_T storeIdsCount, const char **actionFilters,
                                             SIZE_T actionFiltersCount, XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryProductsResult( XAsyncBlock *async,
                                              XStoreProductQueryHandle *productQueryHandle ) override;
    HRESULT WINAPI XStoreQueryEntitledProductsAsync( const XStoreContextHandle storeContextHandle,
                                                     XStoreProductKind productKinds, UINT32 maxItemsToRetrievePerPage,
                                                     XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryEntitledProductsResult( XAsyncBlock *async,
                                                      XStoreProductQueryHandle *productQueryHandle ) override;
    HRESULT WINAPI XStoreQueryProductForCurrentGameAsync( const XStoreContextHandle storeContextHandle,
                                                          XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryProductForCurrentGameResult( XAsyncBlock *async,
                                                           XStoreProductQueryHandle *productQueryHandle ) override;
    HRESULT WINAPI XStoreQueryProductForPackageAsync( const XStoreContextHandle storeContextHandle,
                                                      XStoreProductKind productKinds, const char *packageIdentifier,
                                                      XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryProductForPackageResult( XAsyncBlock *async,
                                                       XStoreProductQueryHandle *productQueryHandle ) override;
    HRESULT WINAPI XStoreEnumerateProductsQuery( const XStoreProductQueryHandle productQueryHandle, void *context,
                                                 XStoreProductQueryCallback *callback ) override;
    BOOLEAN WINAPI XStoreProductsQueryHasMorePages( const XStoreProductQueryHandle productQueryHandle ) override;
    HRESULT WINAPI XStoreProductsQueryNextPageAsync( const XStoreProductQueryHandle productQueryHandle,
                                                     XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreProductsQueryNextPageResult( XAsyncBlock *async,
                                                      XStoreProductQueryHandle *productQueryHandle ) override;
    void WINAPI XStoreCloseProductsQueryHandle( XStoreProductQueryHandle productQueryHandle ) override;
    HRESULT WINAPI XStoreAcquireLicenseForPackageAsync( const XStoreProductQueryHandle productQueryHandle,
                                                        const char *packageIdentifier, XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreAcquireLicenseForPackageResult( XAsyncBlock *async,
                                                         XStoreLicenseHandle *storeLicenseHandle ) override;
    BOOLEAN WINAPI XStoreIsLicenseValid( const XStoreLicenseHandle storeLicenseHandle ) override;
    void WINAPI XStoreCloseLicenseHandle( XStoreLicenseHandle storeLicenseHandle ) override;
    HRESULT WINAPI XStoreCanAcquireLicenseForStoreIdAsync( const XStoreContextHandle storeContextHandle,
                                                           const char *storeProductId, XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreCanAcquireLicenseForStoreIdResult(
        XAsyncBlock *async, XStoreCanAcquireLicenseResult *storeCanAcquireLicense ) override;
    HRESULT WINAPI XStoreCanAcquireLicenseForPackageAsync( const XStoreContextHandle storeContextHandle,
                                                           const char *packageIdentifier, XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreCanAcquireLicenseForPackageResult(
        XAsyncBlock *async, XStoreCanAcquireLicenseResult *storeCanAcquireLicense ) override;
    HRESULT WINAPI XStoreQueryGameLicenseAsync( const XStoreContextHandle storeContextHandle,
                                                XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryGameLicenseResult( XAsyncBlock *async, XStoreGameLicense *license ) override;
    HRESULT WINAPI XStoreQueryAddOnLicensesAsync( const XStoreContextHandle storeContextHandle,
                                                  XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryAddOnLicensesResultCount( XAsyncBlock *async, UINT32 *count ) override;
    HRESULT WINAPI XStoreQueryAddOnLicensesResult( XAsyncBlock *async, UINT32 count,
                                                   XStoreAddonLicense *addOnLicenses ) override;
    HRESULT WINAPI XStoreQueryConsumableBalanceRemainingAsync( const XStoreContextHandle storeContextHandle,
                                                               const char *storeProductId,
                                                               XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryConsumableBalanceRemainingResult( XAsyncBlock *async,
                                                                XStoreConsumableResult *consumableResult ) override;
    HRESULT WINAPI XStoreReportConsumableFulfillmentAsync( const XStoreContextHandle storeContextHandle,
                                                           const char *storeProductId, UINT32 quantity, GUID trackingId,
                                                           XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreReportConsumableFulfillmentResult( XAsyncBlock *async,
                                                            XStoreConsumableResult *consumableResult ) override;
    HRESULT WINAPI XStoreGetUserCollectionsIdAsync( const XStoreContextHandle storeContextHandle,
                                                    const char *serviceTicket, const char *publisherUserId,
                                                    XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreGetUserCollectionsIdResultSize( XAsyncBlock *async, SIZE_T *size ) override;
    HRESULT WINAPI XStoreGetUserCollectionsIdResult( XAsyncBlock *async, SIZE_T size, char *result ) override;
    HRESULT WINAPI XStoreGetUserPurchaseIdAsync( const XStoreContextHandle storeContextHandle,
                                                 const char *serviceTicket, const char *publisherUserId,
                                                 XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreGetUserPurchaseIdResultSize( XAsyncBlock *async, SIZE_T *size ) override;
    HRESULT WINAPI XStoreGetUserPurchaseIdResult( XAsyncBlock *async, SIZE_T size, char *result ) override;
    HRESULT WINAPI XStoreQueryLicenseTokenAsync( const XStoreContextHandle storeContextHandle, const char **productIds,
                                                 SIZE_T productIdsCount, const char *customDeveloperString,
                                                 XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryLicenseTokenResultSize( XAsyncBlock *async, SIZE_T *size ) override;
    HRESULT WINAPI XStoreQueryLicenseTokenResult( XAsyncBlock *async, SIZE_T size, char *result ) override;
    HRESULT WINAPI __PADDING__() override;
    HRESULT WINAPI __PADDING_2__() override;
    HRESULT WINAPI __PADDING_3__() override;
    HRESULT WINAPI XStoreShowPurchaseUIAsync( const XStoreContextHandle storeContextHandle, const char *productId,
                                              const char *name, const char *extendedJsonData,
                                              XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowPurchaseUIResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowRateAndReviewUIAsync( const XStoreContextHandle storeContextHandle,
                                                   XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowRateAndReviewUIResult( XAsyncBlock *async, XStoreRateAndReviewResult *result ) override;
    HRESULT WINAPI XStoreShowRedeemTokenUIAsync( const XStoreContextHandle storeContextHandle, const char *token,
                                                 const char **allowedStoreIds, SIZE_T allowedStoreIdsCount,
                                                 BOOLEAN disallowCsvRedemption, XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowRedeemTokenUIResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryGameAndDlcPackageUpdatesAsync( const XStoreContextHandle storeContextHandle,
                                                             XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryGameAndDlcPackageUpdatesResultCount( XAsyncBlock *async, UINT32 *count ) override;
    HRESULT WINAPI XStoreQueryGameAndDlcPackageUpdatesResult( XAsyncBlock *async, UINT32 count,
                                                              XStorePackageUpdate *packageUpdates ) override;
    HRESULT WINAPI XStoreDownloadPackageUpdatesAsync( XStoreContextHandle storeContextHandle,
                                                      const char **packageIdentifiers, SIZE_T packageIdentifiersCount,
                                                      XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreDownloadPackageUpdatesResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreDownloadAndInstallPackageUpdatesAsync( const XStoreContextHandle storeContextHandle,
                                                                const char **packageIdentifiers,
                                                                SIZE_T packageIdentifiersCount,
                                                                XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreDownloadAndInstallPackageUpdatesResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreDownloadAndInstallPackagesAsync( const XStoreContextHandle storeContextHandle,
                                                          const char **storeIds, SIZE_T storeIdsCount,
                                                          XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreDownloadAndInstallPackagesResultCount( XAsyncBlock *async, UINT32 *count ) override;
    HRESULT WINAPI XStoreDownloadAndInstallPackagesResult( XAsyncBlock *async, UINT32 count,
                                                           char **packageIdentifiers ) override;
    HRESULT WINAPI XStoreQueryPackageIdentifier( const char *productId, SIZE_T size, char *packageIdentifier ) override;
    HRESULT WINAPI XStoreRegisterGameLicenseChanged( XStoreContextHandle storeContextHandle, XTaskQueueHandle queue,
                                                     void *context, XStoreGameLicenseChangedCallback *callback,
                                                     XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XStoreUnregisterGameLicenseChanged( XStoreContextHandle storeContextHandle,
                                                       XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
    HRESULT WINAPI XStoreRegisterPackageLicenseLost( XStoreLicenseHandle storeLicenseHandle, XTaskQueueHandle queue,
                                                     void *context, XStorePackageLicenseLostCallback *callback,
                                                     XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XStoreUnregisterPackageLicenseLost( XStoreLicenseHandle licenseHandle,
                                                       XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
    BOOLEAN WINAPI XStoreIsAvailabilityPurchasable( const XStoreAvailability availability ) override;
    HRESULT WINAPI XStoreAcquireLicenseForDurablesAsync( const XStoreContextHandle storeContextHandle,
                                                         const char *productId, XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreAcquireLicenseForDurablesResult( XAsyncBlock *async,
                                                          XStoreLicenseHandle *storeLicenseHandle ) override;
    HRESULT WINAPI XStoreShowAssociatedProductsUIAsync( const XStoreContextHandle storeContextHandle,
                                                        const char *productId, XStoreProductKind productKinds,
                                                        XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowAssociatedProductsUIResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowProductPageUIAsync( const XStoreContextHandle storeContextHandle, const char *productId,
                                                 XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowProductPageUIResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryAssociatedProductsForStoreIdAsync( const XStoreContextHandle storeContextHandle,
                                                                 const char *productId, XStoreProductKind productKinds,
                                                                 UINT32 maxItemsToRetrievePerPage,
                                                                 XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryAssociatedProductsForStoreIdResult(
        XAsyncBlock *async, XStoreProductQueryHandle *productQueryHandle ) override;
    HRESULT WINAPI XStoreQueryPackageUpdatesAsync( XStoreContextHandle storeContextHandle,
                                                   const char **packageIdentifiers, SIZE_T packageIdentifiersCount,
                                                   XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreQueryPackageUpdatesResultCount( XAsyncBlock *async, UINT32 *count ) override;
    HRESULT WINAPI XStoreQueryPackageUpdatesResult( XAsyncBlock *async, UINT32 count,
                                                    XStorePackageUpdate *packageUpdates ) override;
    HRESULT WINAPI XStoreShowGiftingUIAsync( const XStoreContextHandle storeContextHandle, const char *productId,
                                             const char *name, const char *extendedJsonData,
                                             XAsyncBlock *async ) override;
    HRESULT WINAPI XStoreShowGiftingUIResult( XAsyncBlock *async ) override;
};
IXStoreImpl *GetStoreInterface() noexcept;
