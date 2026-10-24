// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xgame.h>
class XGameImpl final : public nexus::StaticComObject<IXGameImpl3, IXGameImpl2, IXGameImpl>
{
public:
    HRESULT WINAPI XGameGetXboxTitleId( UINT32 *out ) override;
    void WINAPI XLaunchNewGame( const char *exePath, const char *args, XUserHandle defaultUser ) override;
    HRESULT WINAPI XLaunchRestartOnCrash( const char *args, UINT32 reserved ) override;
};
IUnknown *GetGameInterface() noexcept;
