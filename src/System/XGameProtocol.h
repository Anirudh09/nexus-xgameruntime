// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xgameprotocol.h>
class XGameProtocolImpl final : public nexus::StaticComObject<IXGameProtocolImpl>
{
public:
    HRESULT WINAPI XGameProtocolRegisterForActivation( XTaskQueueHandle queue, void *context,
                                                       XGameProtocolActivationCallback *callback,
                                                       XTaskQueueRegistrationToken *token ) override;
    BOOLEAN WINAPI XGameProtocolUnregisterForActivation( XTaskQueueRegistrationToken token, BOOLEAN wait ) override;
};
IUnknown *GetGameProtocolInterface() noexcept;
