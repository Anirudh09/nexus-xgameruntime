/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XError
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

#include "XError.h"
HRESULT WINAPI XErrorImpl::__PADDING__()
{
    return E_NOTIMPL;
}
void WINAPI XErrorImpl::XErrorSetCallback( XErrorCallback *callback, void *context )
{
    AcquireSRWLockExclusive( &lock_ );
    callback_ = callback;
    context_ = context;
    ReleaseSRWLockExclusive( &lock_ );
}
void WINAPI XErrorImpl::XErrorSetOptions( XErrorOptions present, XErrorOptions absent )
{
    AcquireSRWLockExclusive( &lock_ );
    present_ = present;
    absent_ = absent;
    ReleaseSRWLockExclusive( &lock_ );
}
IUnknown *GetErrorInterface() noexcept
{
    static XErrorImpl value;
    return &value;
}
