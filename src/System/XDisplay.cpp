/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XDisplay and XLauncher
 *
 * Copyright 2026 Olivia Ryan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "XDisplay.h"

HRESULT WINAPI XDisplayImpl::QueryInterface( REFIID iid, void **out ) noexcept
{
    if ( !out ) return E_POINTER;
    *out = nullptr;
    if ( iid == __uuidof( IUnknown ) || iid == __uuidof( IXDisplayImpl ) )
        *out = static_cast<IXDisplayImpl *>( this );
    else if ( iid == __uuidof( IXLauncherImpl ) )
        *out = static_cast<IXLauncherImpl *>( this );
    else
        return E_NOINTERFACE;
    AddRef();
    return S_OK;
}
ULONG WINAPI XDisplayImpl::AddRef() noexcept
{
    return ++references_;
}
ULONG WINAPI XDisplayImpl::Release() noexcept
{
    return --references_;
}
HRESULT WINAPI XDisplayImpl::__PADDING__()
{
    return E_NOTIMPL;
}
XDisplayHdrModeResult WINAPI XDisplayImpl::XDisplayTryEnableHdrMode( XDisplayHdrModePreference preference,
                                                                     XDisplayHdrModeInfo *info )
{
    if ( info ) *info = {};
    return XDisplayHdrModeResult::Unknown;
}
HRESULT WINAPI XDisplayImpl::XLaunchUri( XUserHandle user, const char *uri )
{
    return E_NOTIMPL;
}
HRESULT WINAPI XDisplayImpl::XDisplayAcquireTimeoutDeferral( XDisplayTimeoutDeferralHandle *handle )
{
    if ( !handle ) return E_POINTER;
    *handle = nullptr;
    return E_NOTIMPL;
}
void WINAPI XDisplayImpl::XDisplayCloseTimeoutDeferralHandle( XDisplayTimeoutDeferralHandle handle ) {}
static XDisplayImpl &Display()
{
    static XDisplayImpl value;
    return value;
}
IXDisplayImpl *GetDisplayInterface() noexcept
{
    return &Display();
}
IXLauncherImpl *GetLauncherInterface() noexcept
{
    return &Display();
}
