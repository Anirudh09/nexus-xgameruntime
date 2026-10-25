// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "XUser.h"
#include <atomic>

class XUserImpl final : public IXUserImpl6, public IXUserGamertagImpl, public IXUserDeviceImpl2
{
public:
    XUserImpl() : XUserImpl( GetUserTransport() ) {}
    explicit XUserImpl( UserTransport &transport ) : transport( transport ) {}
    HRESULT WINAPI QueryInterface( REFIID iid, void **out ) noexcept override
    {
        if ( !out ) return E_POINTER;
        *out = nullptr;
        if ( iid == __uuidof( IUnknown ) || iid == __uuidof( IXUserImpl ) || iid == __uuidof( IXUserImpl2 ) ||
             iid == __uuidof( IXUserImpl3 ) || iid == __uuidof( IXUserImpl4 ) || iid == __uuidof( IXUserImpl5 ) ||
             iid == __uuidof( IXUserImpl6 ) )
            *out = static_cast<IXUserImpl6 *>( this );
        else if ( iid == __uuidof( IXUserGamertagImpl ) )
            *out = static_cast<IXUserGamertagImpl *>( this );
        else if ( iid == __uuidof( IXUserDeviceImpl ) || iid == __uuidof( IXUserDeviceImpl2 ) )
            *out = static_cast<IXUserDeviceImpl2 *>( this );
        else
            return E_NOINTERFACE;
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
    HRESULT WINAPI XUserDuplicateHandle( XUserHandle handle, XUserHandle *duplicatedHandle ) override;
    void WINAPI XUserCloseHandle( XUserHandle user ) override;
    INT32 WINAPI XUserCompare( XUserHandle user1, XUserHandle user2 ) override;
    HRESULT WINAPI XUserGetMaxUsers( UINT32 *maxUsers ) override;
    HRESULT WINAPI XUserAddAsync( XUserAddOptions options, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserAddResult( XAsyncBlock *async, XUserHandle *newUser ) override;
    HRESULT WINAPI XUserGetLocalId( XUserHandle user, XUserLocalId *userLocalId ) override;
    HRESULT WINAPI XUserFindUserByLocalId( XUserLocalId userLocalId, XUserHandle *handle ) override;
    HRESULT WINAPI XUserGetId( XUserHandle user, UINT64 *userId ) override;
    HRESULT WINAPI XUserFindUserById( UINT64 userId, XUserHandle *handle ) override;
    HRESULT WINAPI XUserGetIsGuest( XUserHandle user, BOOLEAN *isGuest ) override;
    HRESULT WINAPI XUserGetState( XUserHandle user, XUserState *state ) override;
    HRESULT WINAPI __PADDING__() override;
    HRESULT WINAPI XUserGetGamerPictureAsync( XUserHandle user, XUserGamerPictureSize pictureSize,
                                              XAsyncBlock *async ) override;
    HRESULT WINAPI XUserGetGamerPictureResultSize( XAsyncBlock *async, SIZE_T *bufferSize ) override;
    HRESULT WINAPI XUserGetGamerPictureResult( XAsyncBlock *async, SIZE_T bufferSize, void *buffer,
                                               SIZE_T *bufferUsed ) override;
    HRESULT WINAPI XUserGetAgeGroup( XUserHandle user, XUserAgeGroup *ageGroup ) override;
    HRESULT WINAPI XUserCheckPrivilege( XUserHandle user, XUserPrivilegeOptions options, XUserPrivilege privilege,
                                        BOOLEAN *hasPrivilege, XUserPrivilegeDenyReason *reason ) override;
    HRESULT WINAPI XUserResolvePrivilegeWithUiAsync( XUserHandle user, XUserPrivilegeOptions options,
                                                     XUserPrivilege privilege, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserResolvePrivilegeWithUiResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XUserGetTokenAndSignatureAsync( XUserHandle user, XUserGetTokenAndSignatureOptions options,
                                                   const char *method, const char *url, SIZE_T headerCount,
                                                   const XUserGetTokenAndSignatureHttpHeader *headers, SIZE_T bodySize,
                                                   const void *bodyBuffer, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserGetTokenAndSignatureResultSize( XAsyncBlock *async, SIZE_T *bufferSize ) override;
    HRESULT WINAPI XUserGetTokenAndSignatureResult( XAsyncBlock *async, SIZE_T bufferSize, void *buffer,
                                                    XUserGetTokenAndSignatureData **ptrToBuffer,
                                                    SIZE_T *bufferUsed ) override;
    HRESULT WINAPI XUserGetTokenAndSignatureUtf16Async( XUserHandle user, XUserGetTokenAndSignatureOptions options,
                                                        const WCHAR *method, const WCHAR *url, SIZE_T headerCount,
                                                        const XUserGetTokenAndSignatureUtf16HttpHeader *headers,
                                                        SIZE_T bodySize, const void *bodyBuffer,
                                                        XAsyncBlock *async ) override;
    HRESULT WINAPI XUserGetTokenAndSignatureUtf16ResultSize( XAsyncBlock *async, SIZE_T *bufferSize ) override;
    HRESULT WINAPI XUserGetTokenAndSignatureUtf16Result( XAsyncBlock *async, SIZE_T bufferSize, void *buffer,
                                                         XUserGetTokenAndSignatureUtf16Data **ptrToBuffer,
                                                         SIZE_T *bufferUsed ) override;
    HRESULT WINAPI XUserResolveIssueWithUiAsync( XUserHandle user, const char *url, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserResolveIssueWithUiResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XUserResolveIssueWithUiUtf16Async( XUserHandle user, const WCHAR *url, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserResolveIssueWithUiUtf16Result( XAsyncBlock *async ) override;
    HRESULT WINAPI XUserRegisterForChangeEvent( XTaskQueueHandle queue, void *context,
                                                XUserChangeEventCallback *callback,
                                                XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XUserUnregisterForChangeEvent( XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
    HRESULT WINAPI XUserGetSignOutDeferral( XUserSignOutDeferralHandle *deferral ) override;
    void WINAPI XUserCloseSignOutDeferralHandle( XUserSignOutDeferralHandle deferral ) override;
    HRESULT WINAPI XUserAddByIdWithUiAsync( UINT64 userId, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserAddByIdWithUiResult( XAsyncBlock *async, XUserHandle *newUser ) override;
    HRESULT WINAPI XUserGetMsaTokenSilentlyAsync( XUserHandle user, XUserGetMsaTokenSilentlyOptions options,
                                                  const char *scope, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserGetMsaTokenSilentlyResult( XAsyncBlock *async, SIZE_T resultTokenSize, char *resultToken,
                                                   SIZE_T *resultTokenUsed ) override;
    HRESULT WINAPI XUserGetMsaTokenSilentlyResultSize( XAsyncBlock *async, SIZE_T *tokenSize ) override;
    BOOLEAN WINAPI XUserIsStoreUser( XUserHandle user ) override;
    HRESULT WINAPI XUserPlatformRemoteConnectSetEventHandlers(
        XTaskQueueHandle queue, XUserPlatformRemoteConnectEventHandlers *handlers ) override;
    HRESULT WINAPI XUserPlatformRemoteConnectCancelPrompt( XUserPlatformOperation operation ) override;
    HRESULT WINAPI XUserPlatformSpopPromptSetEventHandlers( XTaskQueueHandle queue,
                                                            XUserPlatformSpopPromptEventHandler *handler,
                                                            void *context ) override;
    HRESULT WINAPI XUserPlatformSpopPromptComplete( XUserPlatformOperation operation,
                                                    XUserPlatformOperationResult result ) override;
    BOOLEAN WINAPI XUserIsSignOutPresent() override;
    HRESULT WINAPI XUserSignOutAsync( XUserHandle user, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserSignOutResult( XAsyncBlock *async ) override;
    HRESULT WINAPI XUserGetGamertag( XUserHandle user, XUserGamertagComponent gamertagComponent, SIZE_T gamertagSize,
                                     char *gamertag, SIZE_T *gamertagUsed ) override;
    HRESULT WINAPI XUserFindForDevice( const APP_LOCAL_DEVICE_ID *deviceId, XUserHandle *handle ) override;
    HRESULT WINAPI XUserRegisterForDeviceAssociationChanged( XTaskQueueHandle queue, void *context,
                                                             XUserDeviceAssociationChangedCallback *callback,
                                                             XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XUserUnregisterForDeviceAssociationChanged( XTaskQueueRegistrationToken token,
                                                               BOOLEAN wait ) override;
    HRESULT WINAPI XUserGetDefaultAudioEndpointUtf16( XUserLocalId user,
                                                      XUserDefaultAudioEndpointKind defaultAudioEndpointKind,
                                                      SIZE_T endpointIdUtf16Count, WCHAR *endpointIdUtf16,
                                                      SIZE_T *endpointIdUtf16Used ) override;
    HRESULT WINAPI XUserRegisterForDefaultAudioEndpointUtf16Changed(
        XTaskQueueHandle queue, void *context, XUserDefaultAudioEndpointUtf16ChangedCallback *callback,
        XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XUserUnregisterForDefaultAudioEndpointUtf16Changed( XTaskQueueRegistrationToken token,
                                                                       BOOLEAN wait ) override;
    HRESULT WINAPI XUserFindControllerForUserWithUiAsync( XUserHandle user, XAsyncBlock *async ) override;
    HRESULT WINAPI XUserFindControllerForUserWithUiResult( XAsyncBlock *async, APP_LOCAL_DEVICE_ID *deviceId ) override;

private:
    UserTransport &transport;
    std::atomic<ULONG> ref{ 1 };
};
