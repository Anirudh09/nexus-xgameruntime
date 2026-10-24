/*
 * Xbox Game runtime Library
 *
 * Written by Weather.
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

#ifndef __XGAMERUNTIME_PRIVATE_H__
#define __XGAMERUNTIME_PRIVATE_H__

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#define COBJMACROS
#include <private/logging.h>
#include <windows.h>
#include <wtypesbase.h>
#include <minwindef.h>

#ifdef __cplusplus
// Bug: WinRT in C++ within Wine lacks proper C++ type handling
// Redefine boolean as bool, and DOUBLE as double to prevent compile issues.
// Casting is purely handled by libstdc++, so it's not an issue here.
#undef boolean
#define boolean bool
#undef DOUBLE
#define DOUBLE double

// Bug: __WINESRC__ is not defined in C++ contexts.
#define __WINESRC__ 1
#include <cstdint>
#endif

#define WIDL_EXPLICIT_AGGREGATE_RETURNS

#include <xgameerr.h>
#include <xsystem.h>
#include <xgameruntimeinit.h>
#include <xgameruntimefeature.h>
#include <xnetworking.h>
#include <xuser.h>
#include <xgamesave.h>
#include <xasync.h>
#include <xasyncprovider.h>
#include <xtaskqueue.h>

#define WIDL_using_Windows_Foundation
#define WIDL_using_Windows_Foundation_Collections
#include "windows.foundation.h"
#define WIDL_using_Windows_Globalization
#include "windows.globalization.h"

#define E_ILLEGAL_METHOD_CALL                              _HRESULT_TYPEDEF_(0x8000000E)

#define WIDL_using_Windows_Data_Json
#include "windows.data.json.h"
#include <winstring.h>
#include <roapi.h>
using namespace ABI::Windows::Data::Json;
using IVector_IJsonValue = ABI::Windows::Foundation::Collections::IVector<IJsonValue *>;
#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#endif
#include <winhttp.h>
#include <type_traits>
#include <algorithm>
#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (static_cast<LONG>(status) >= 0)
#endif
#ifndef WEB_E_JSON_VALUE_NOT_FOUND
#define WEB_E_JSON_VALUE_NOT_FOUND _HRESULT_TYPEDEF_(0x83750009)
#endif

extern IXThreadingImpl *x_threading_impl;
extern IXUserImpl *x_user_impl;
extern IXUserDeviceImpl *x_user_device_impl;
extern "C" HRESULT WINAPI QueryApiImpl( REFCLSID clsid, REFIID iid, void **out );
UINT64 xuser_get_xuid( XUserHandle user );
BOOL xuser_is_current_user( XUserHandle user );
HRESULT xuser_device_authorization( XUserHandle user, const char *relyingParty, char **auth );
HRESULT xuser_signed_request( XUserHandle user, const char *method, const char *url, const char *auth, const char *extraHeaders,
    const void *body, DWORD bodySize, DWORD *status, BYTE **buffer, SIZE_T *bufferSize );
void nexus_store_metadata(unsigned int api,HRESULT hr,unsigned int flags,const GUID *iid,const void *caller);
#define NEXUS_STORE_META(api,hr,flags,iid) nexus_store_metadata(api,hr,flags,iid,__builtin_return_address(0))

constexpr UINT32 operator&(XUserAddOptions a, XUserAddOptions b) { return static_cast<UINT32>(a) & static_cast<UINT32>(b); }
constexpr UINT32 operator&(XUserGetTokenAndSignatureOptions a, XUserGetTokenAndSignatureOptions b) { return static_cast<UINT32>(a) & static_cast<UINT32>(b); }


#endif
