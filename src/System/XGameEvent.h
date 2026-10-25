// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xgameevent.h>
class XGameEventImpl final : public nexus::StaticComObject<IXGameEventImpl>
{
public:
    HRESULT WINAPI XGameEventWrite( XUserHandle user, const char *serviceConfigId, const char *playSessionId,
                                    const char *eventName, const char *dimensionsJson,
                                    const char *measurementsJson ) override;
};
IUnknown *GetGameEventInterface() noexcept;
