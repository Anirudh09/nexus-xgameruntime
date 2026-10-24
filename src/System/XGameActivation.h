// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xgameactivation.h>
class XGameActivationImpl final : public nexus::StaticComObject<IXGameActivationImpl>
{
public:
    HRESULT WINAPI XGameActivationRegisterForEvent( XTaskQueueHandle queue, void *context,
                                                    XGameActivationCallback *callback,
                                                    XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XGameActivationUnregisterForEvent( XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
    HRESULT WINAPI XGameActivationAcceptPendingInvite( const char *inviteUri ) override;
};
IUnknown *GetGameActivationInterface() noexcept;
