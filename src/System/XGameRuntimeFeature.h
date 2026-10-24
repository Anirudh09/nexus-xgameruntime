// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xgameruntimefeature.h>
class XGameRuntimeFeatureImpl final : public nexus::StaticComObject<IXGameRuntimeFeatureImpl>
{
public:
    BOOLEAN WINAPI XGameRuntimeIsFeatureAvailable( XGameRuntimeFeature feature ) override;
};
IUnknown *GetGameRuntimeFeatureInterface() noexcept;
