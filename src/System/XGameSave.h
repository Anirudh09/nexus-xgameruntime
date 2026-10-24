// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include <xgamesave.h>
#include <atomic>

class XGameSaveImpl final : public IXGameSaveImpl4
{
public:
    HRESULT WINAPI QueryInterface( REFIID iid, void **out ) noexcept override
    {
        if ( !out ) return E_POINTER;
        *out = nullptr;
        if ( iid != __uuidof( IUnknown ) && iid != __uuidof( IXGameSaveImpl ) && iid != __uuidof( IXGameSaveImpl2 ) &&
             iid != __uuidof( IXGameSaveImpl3 ) && iid != __uuidof( IXGameSaveImpl4 ) )
            return E_NOINTERFACE;
        *out = static_cast<IXGameSaveImpl4 *>( this );
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
    HRESULT WINAPI XGameSaveInitializeProvider( XUserHandle requestingUser, const char *configurationId,
                                                BOOLEAN syncOnDemand, XGameSaveProviderHandle *provider ) override;
    HRESULT WINAPI XGameSaveInitializeProviderAsync( XUserHandle requestingUser, const char *configurationId,
                                                     BOOLEAN syncOnDemand, XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveInitializeProviderResult( XAsyncBlock *async, XGameSaveProviderHandle *provider ) override;
    void WINAPI XGameSaveCloseProvider( XGameSaveProviderHandle provider ) override;
    HRESULT WINAPI XGameSaveGetRemainingQuota( XGameSaveProviderHandle provider, INT64 *remainingQuota ) override;
    HRESULT WINAPI XGameSaveGetRemainingQuotaAsync( XGameSaveProviderHandle provider, XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveGetRemainingQuotaResult( XAsyncBlock *async, INT64 *remainingQuota ) override;
    HRESULT WINAPI XGameSaveDeleteContainer( XGameSaveProviderHandle provider, const char *containerName ) override;
    HRESULT WINAPI XGameSaveDeleteContainerAsync( XGameSaveProviderHandle provider, const char *containerName,
                                                  XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveDeleteContainerResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveGetContainerInfo( XGameSaveProviderHandle provider, const char *containerName,
                                              void *context, XGameSaveContainerInfoCallback *callback ) override;
    HRESULT WINAPI XGameSaveEnumerateContainerInfo( XGameSaveProviderHandle provider, void *context,
                                                    XGameSaveContainerInfoCallback *callback ) override;
    HRESULT WINAPI XGameSaveEnumerateContainerInfoByName( XGameSaveProviderHandle provider,
                                                          const char *containerNamePrefix, void *context,
                                                          XGameSaveContainerInfoCallback *callback ) override;
    HRESULT WINAPI XGameSaveCreateContainer( XGameSaveProviderHandle provider, const char *containerName,
                                             XGameSaveContainerHandle *containerContext ) override;
    void WINAPI XGameSaveCloseContainer( XGameSaveContainerHandle context ) override;
    HRESULT WINAPI XGameSaveEnumerateBlobInfo( XGameSaveContainerHandle container, void *context,
                                               XGameSaveBlobInfoCallback *callback ) override;
    HRESULT WINAPI XGameSaveEnumerateBlobInfoByName( XGameSaveContainerHandle container, const char *blobNamePrefix,
                                                     void *context, XGameSaveBlobInfoCallback *callback ) override;
    HRESULT WINAPI XGameSaveReadBlobData( XGameSaveContainerHandle container, const char **blobNames,
                                          UINT32 *countOfBlobs, SIZE_T blobsSize, XGameSaveBlob *blobData ) override;
    HRESULT WINAPI XGameSaveReadBlobDataAsync( XGameSaveContainerHandle container, const char **blobNames,
                                               UINT32 countOfBlobs, XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveReadBlobDataResult( XAsyncBlock *async, SIZE_T blobsSize, XGameSaveBlob *blobData,
                                                UINT32 *countOfBlobs ) override;
    HRESULT WINAPI XGameSaveCreateUpdate( XGameSaveContainerHandle container, const char *containerDisplayName,
                                          XGameSaveUpdateHandle *updateContext ) override;
    void WINAPI XGameSaveCloseUpdate( XGameSaveUpdateHandle context ) override;
    HRESULT WINAPI XGameSaveSubmitBlobWrite( XGameSaveUpdateHandle updateContext, const char *blobName, UINT8 *data,
                                             SIZE_T byteCount ) override;
    HRESULT WINAPI XGameSaveSubmitBlobDelete( XGameSaveUpdateHandle updateContext, const char *blobName ) override;
    HRESULT WINAPI XGameSaveSubmitUpdate( XGameSaveUpdateHandle updateContext ) override;
    HRESULT WINAPI XGameSaveSubmitUpdateAsync( XGameSaveUpdateHandle updateContext, XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveSubmitUpdateResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveFilesGetFolderWithUiAsync( XUserHandle user, const char *scid,
                                                       XAsyncBlock *async ) override;
    HRESULT WINAPI XGameSaveFilesGetFolderWithUiResult( XAsyncBlock *async, SIZE_T size, char *folder ) override;
    HRESULT WINAPI XGameSaveFilesGetRemainingQuota( XUserHandle user, const char *scid, INT64 *quota ) override;
    // These legacy extension slots have no known contract. Preserve honest
    // E_NOTIMPL behavior instead of claiming unsupported save functionality.
    virtual HRESULT WINAPI Unknown0( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown1( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown2( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown3( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown4( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown5( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown6( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown7( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown8( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown9( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown10( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown11( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown12( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown13( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown14( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }
    virtual HRESULT WINAPI Unknown15( void *, void *, void *, void * )
    {
        return E_NOTIMPL;
    }

private:
    std::atomic<ULONG> ref{ 1 };
};
IXGameSaveImpl *GetGameSaveInterface() noexcept;
