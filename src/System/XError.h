// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xerror.h>
class XErrorImpl final : public nexus::StaticComObject<IXErrorImpl>
{
public:
    HRESULT WINAPI __PADDING__() override;
    void WINAPI XErrorSetCallback( XErrorCallback *callback, void *context ) override;
    void WINAPI XErrorSetOptions( XErrorOptions present, XErrorOptions absent ) override;

private:
    SRWLOCK lock_ = SRWLOCK_INIT;
    XErrorCallback *callback_ = nullptr;
    void *context_ = nullptr;
    XErrorOptions present_ = XErrorOptions::None, absent_ = XErrorOptions::None;
};
IUnknown *GetErrorInterface() noexcept;
