// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xgameinvite.h>
class XGameInviteImpl final : public nexus::StaticComObject<IXGameInviteImpl2, IXGameInviteImpl>
{
public:
    HRESULT WINAPI XGameInviteRegisterForEvent( XTaskQueueHandle queue, void *context,
                                                XGameInviteEventCallback *callback,
                                                XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XGameInviteUnregisterForEvent( XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
    HRESULT WINAPI XGameInviteRegisterForPendingEvent( XTaskQueueHandle queue, void *context,
                                                       XGameInviteEventCallback *callback,
                                                       XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XGameInviteUnregisterForPendingEvent( XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
    HRESULT WINAPI XGameInviteAcceptPendingInvite( const char *inviteUri ) override;
};
IUnknown *GetGameInviteInterface() noexcept;
