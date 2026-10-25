/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XGameRuntimeFeature
 *
 * Written by Weather
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

#include "XGameRuntimeFeature.h"

BOOLEAN WINAPI XGameRuntimeFeatureImpl::XGameRuntimeIsFeatureAvailable( XGameRuntimeFeature feature )
{
    // Availability describes implemented API families, not a placeholder COM
    // object. Individual operations can still fail for normal runtime reasons.
    switch ( feature )
    {
    case XGameRuntimeFeature::XAsync:
    case XGameRuntimeFeature::XAsyncProvider:
    case XGameRuntimeFeature::XGame:
    case XGameRuntimeFeature::XGameSave:
    case XGameRuntimeFeature::XNetworking:
    case XGameRuntimeFeature::XPackage:
    case XGameRuntimeFeature::XPersistentLocalStorage:
    case XGameRuntimeFeature::XStore:
    case XGameRuntimeFeature::XSystem:
    case XGameRuntimeFeature::XTaskQueue:
    case XGameRuntimeFeature::XThread:
    case XGameRuntimeFeature::XUser:
        return TRUE;
    default:
        return FALSE;
    }
}

IUnknown *GetGameRuntimeFeatureInterface() noexcept
{
    static XGameRuntimeFeatureImpl value;
    return &value;
}
