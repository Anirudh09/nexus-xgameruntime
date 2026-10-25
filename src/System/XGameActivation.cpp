/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XGameActivation
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

#include "XGameActivation.h"
#include "GameInvite/Events.h"

HRESULT WINAPI XGameActivationImpl::XGameActivationRegisterForEvent( XTaskQueueHandle queue, void *context,
                                                                     XGameActivationCallback *callback,
                                                                     XTaskQueueRegistrationToken *token )
{
    ( void )0;

    if ( token ) token->token = 0;
    if ( !callback || !token ) return E_INVALIDARG;
    return nexus::invites::GetEvents().Register( nexus::invites::EventKind::Activation, queue, context, { nullptr, callback, nullptr }, token );
}

BOOLEAN WINAPI XGameActivationImpl::XGameActivationUnregisterForEvent( XTaskQueueRegistrationToken token, BOOLEAN wait )
{
    ( void )0;
    return nexus::invites::GetEvents().Unregister( nexus::invites::EventKind::Activation, token, wait );
}

HRESULT WINAPI XGameActivationImpl::XGameActivationAcceptPendingInvite( const char *inviteUri )
{
    ( void )0;
    return nexus::invites::Accept( inviteUri );
}

IUnknown *GetGameActivationInterface() noexcept
{
    static XGameActivationImpl value;
    return &value;
}
