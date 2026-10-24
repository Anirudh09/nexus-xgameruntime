// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include <xdisplay.h>
#include <atomic>

class XDisplayImpl final : public IXDisplayImpl, public IXLauncherImpl
{
public:
    HRESULT WINAPI QueryInterface( REFIID iid, void **out ) noexcept override;
    ULONG WINAPI AddRef() noexcept override;
    ULONG WINAPI Release() noexcept override;
    HRESULT WINAPI __PADDING__() override;
    XDisplayHdrModeResult WINAPI XDisplayTryEnableHdrMode( XDisplayHdrModePreference preference,
                                                           XDisplayHdrModeInfo *info ) override;
    HRESULT WINAPI XLaunchUri( XUserHandle user, const char *uri ) override;
    HRESULT WINAPI XDisplayAcquireTimeoutDeferral( XDisplayTimeoutDeferralHandle *handle ) override;
    void WINAPI XDisplayCloseTimeoutDeferralHandle( XDisplayTimeoutDeferralHandle handle ) override;

private:
    std::atomic<ULONG> references_{ 1 };
};
IXDisplayImpl *GetDisplayInterface() noexcept;
IXLauncherImpl *GetLauncherInterface() noexcept;
