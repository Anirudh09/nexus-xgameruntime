/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XGameProtocol
 *
 * Written by LukasPAH
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

#include "XGameProtocol.h"
#include "GameInvite/Events.h"

/* Protocol activation is how a title gets launched from a URI. Nothing here
 * delivers such launches yet, but refusing the registration reads as a broken
 * runtime rather than as "no URI launches will arrive". */
static LONG64 protocol_activation_token;

HRESULT WINAPI XGameProtocolImpl::XGameProtocolRegisterForActivation( XTaskQueueHandle queue, void *context,
                                                                      XGameProtocolActivationCallback *callback,
                                                                      XTaskQueueRegistrationToken *token )
{
    ( void )0;

    if ( token ) token->token = 0;
    if ( !callback || !token ) return E_INVALIDARG;
    return nexus::invites::GetEvents().Register( nexus::invites::EventKind::Protocol, queue, context, { nullptr, nullptr, callback }, token );
}

BOOLEAN WINAPI XGameProtocolImpl::XGameProtocolUnregisterForActivation( XTaskQueueRegistrationToken token,
                                                                        BOOLEAN wait )
{
    ( void )0;
    return nexus::invites::GetEvents().Unregister( nexus::invites::EventKind::Protocol, token, wait );
}

IUnknown *GetGameProtocolInterface() noexcept
{
    static XGameProtocolImpl value;
    return &value;
}
