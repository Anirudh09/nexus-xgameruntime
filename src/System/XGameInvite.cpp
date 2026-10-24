/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XGameInvite
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

#include "XGameInvite.h"

HRESULT WINAPI XGameInviteImpl::XGameInviteRegisterForEvent( XTaskQueueHandle queue, void *context,
                                                             XGameInviteEventCallback *callback,
                                                             XTaskQueueRegistrationToken *token )
{
    ( void )0;

    if ( token ) token->token = 0;
    if ( !callback || !token ) return E_INVALIDARG;
    return E_NOTIMPL;
}

BOOLEAN WINAPI XGameInviteImpl::XGameInviteUnregisterForEvent( XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    ( void )0;
    return FALSE;
}

HRESULT WINAPI XGameInviteImpl::XGameInviteRegisterForPendingEvent( XTaskQueueHandle queue, void *context,
                                                                    XGameInviteEventCallback *callback,
                                                                    XTaskQueueRegistrationToken *token )
{
    ( void )0;

    if ( token ) token->token = 0;
    if ( !callback || !token ) return E_INVALIDARG;
    return E_NOTIMPL;
}

BOOLEAN WINAPI XGameInviteImpl::XGameInviteUnregisterForPendingEvent( XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    ( void )0;
    return FALSE;
}

HRESULT WINAPI XGameInviteImpl::XGameInviteAcceptPendingInvite( const char *inviteUri )
{
    ( void )0;
    return E_NOTIMPL;
}

IUnknown *GetGameInviteInterface() noexcept
{
    static XGameInviteImpl value;
    return &value;
}
