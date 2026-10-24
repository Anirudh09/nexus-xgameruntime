// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../../private.h"

// An internal protocol dependency, not an exported runtime hook. The default
// implementation always uses the actual local backend and Microsoft transport.
class UserTransport
{
public:
    virtual ~UserTransport() = default;
    virtual void WaitForSignInRetry( DWORD milliseconds ) noexcept
    {
        Sleep( milliseconds );
    }
    virtual HRESULT Backend( UINT16 type, const char *body, UINT16 size, char **response, UINT16 *responseSize ) = 0;
    virtual HRESULT Http( const WCHAR *method, const WCHAR *url, char *body, const WCHAR *headers, const WCHAR **accept,
                          BYTE **response, SIZE_T *responseSize ) = 0;
    virtual HRESULT HttpRaw( const WCHAR *method, const WCHAR *url, const WCHAR *headers, const void *body,
                             DWORD bodySize, DWORD *status, BYTE **response, SIZE_T *responseSize ) = 0;
};

UserTransport &GetUserTransport() noexcept;
