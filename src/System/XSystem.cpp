/*
 * Xbox Game runtime Library
 * Based on the XSystem implementation by Weather and Nexus device-ID and
 * desktop analytics fixes. Adapted to the upstream C++ COM interfaces.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * This library is distributed without any warranty. See LICENSE.
 */

#include "../private.h"
#include "StaticComObject.h"
#include "XSystem.h"
#include <cstring>

namespace nexus
{
namespace
{
class XSystemImpl final
    : public StaticComObject<IXSystemImpl5, IXSystemImpl, IXSystemImpl2, IXSystemImpl3, IXSystemImpl4>
{
  public:
    HRESULT WINAPI XSystemGetConsoleId( INT32 size, char *value, SIZE_T *used ) override
    {
        if ( !value || !used ) return E_POINTER;
        constexpr char id[] = "00000000.00000000.00000000.00000000.00";
        if ( size < 0 || static_cast<SIZE_T>( size ) < sizeof( id ) )
            return HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );
        std::memcpy( value, id, sizeof( id ) );
        *used = sizeof( id );
        return S_OK;
    }
    HRESULT WINAPI XSystemGetXboxLiveSandboxId( INT32 size, char *value, SIZE_T *used ) override
    {
        if ( !value ) return E_POINTER;
        if ( size < 16 ) return HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );
        constexpr char sandbox[] = "RETAIL";
        std::memcpy( value, sandbox, sizeof( sandbox ) );
        if ( used ) *used = sizeof( sandbox );
        return S_OK;
    }
    HRESULT WINAPI XSystemGetAppSpecificDeviceId( INT32 size, char *value, SIZE_T *used ) override
    {
        // The documented sentinel is not an account identity or attestation.
        // Do not synthesize a hardware ID from the account or installation path.
        constexpr char unavailable[] = "00000000000000000000000000000000000000000000";
        static_assert( sizeof( unavailable ) == 45 );
        if ( used ) *used = 0;
        if ( !value ) return E_POINTER;
        if ( size < 0 || static_cast<SIZE_T>( size ) < sizeof( unavailable ) )
            return HRESULT_FROM_WIN32( ERROR_INSUFFICIENT_BUFFER );
        std::memcpy( value, unavailable, sizeof( unavailable ) );
        if ( used ) *used = sizeof( unavailable );
        return S_OK;
    }
    HRESULT WINAPI XSystemHandleTrack( XSystemHandleCallback *, void * ) override
    {
        return E_NOTIMPL;
    }
    BOOLEAN WINAPI XSystemIsHandleValid( XSystemHandle ) override
    {
        // No handle registry has been ported yet. Never report unknown handles
        // as valid simply to make a caller proceed.
        return FALSE;
    }
    void WINAPI XSystemAllowFullDownloadBandwidth( BOOLEAN ) override {}
};

class XSystemAnalyticsImpl final : public StaticComObject<IXSystemAnalyticsImpl>
{
  public:
    XSystemAnalyticsInfo *WINAPI XSystemGetAnalyticsInfo( XSystemAnalyticsInfo *result ) override
    {
        if ( !result ) return nullptr;
        *result = {};
        OSVERSIONINFOW version{};
        version.dwOSVersionInfoSize = sizeof( version );
        using GetVersion = LONG( WINAPI * )( OSVERSIONINFOW * );
        const auto ntdll = GetModuleHandleW( L"ntdll.dll" );
        const auto getVersion =
            ntdll ? reinterpret_cast<GetVersion>( GetProcAddress( ntdll, "RtlGetVersion" ) ) : nullptr;
        if ( getVersion && getVersion( &version ) == 0 )
        {
            result->osVersion.major = static_cast<UINT16>( version.dwMajorVersion );
            result->osVersion.minor = static_cast<UINT16>( version.dwMinorVersion );
            result->osVersion.build = static_cast<UINT16>( version.dwBuildNumber );
            result->hostingOsVersion = result->osVersion;
        }
        // Report the desktop API environment, without requiring WinRT apartment
        // activation. Callers receive a real value, not a null aggregate pointer.
        std::strcpy( result->family, "Windows.Desktop" );
        std::strcpy( result->form, "Desktop" );
        return result;
    }
};
}
IUnknown *GetSystemInterface() noexcept
{
    static XSystemImpl value;
    return &value;
}
IUnknown *GetSystemAnalyticsInterface() noexcept
{
    static XSystemAnalyticsImpl value;
    return &value;
}
}
