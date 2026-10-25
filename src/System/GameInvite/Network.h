// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "Bridge.h"
#include <functional>
#include <memory>
namespace nexus::invites
{
using Invitation = std::function<void( const std::string &, const std::string &, const std::string &, UINT64 )>;
class NetworkTransport
{
public:
    virtual ~NetworkTransport() = default;
    virtual HRESULT Request( XUserHandle user, const char *method, const std::string &url, const std::string &body, const char *contract, DWORD &status, std::string &reply ) noexcept = 0;
};
NetworkTransport &GetNetwork() noexcept;
HRESULT SendNetworkInvite( NetworkTransport &network, XUserHandle user, UINT32 title, const std::vector<std::string> &session, const std::string &recipient ) noexcept;
HRESULT SendActivityInvite( NetworkTransport &network, XUserHandle user, UINT64 owner, UINT32 title, const std::string &recipient ) noexcept;
HRESULT ParseNotification( const std::string &body, UINT64 receiver, UINT32 title, const Invitation &callback ) noexcept;
class WebSocketReader
{
    struct State;
    std::unique_ptr<State> state;
    static void CALLBACK Status( HINTERNET, DWORD_PTR, DWORD, void *, DWORD );
public:
    WebSocketReader();
    ~WebSocketReader();
    HRESULT Open( HINTERNET connection, const WCHAR *path, DWORD flags, const std::wstring &headers ) noexcept;
    HRESULT Send( const std::string &frame ) noexcept;
    HRESULT Next( std::string &frame ) noexcept;
    void Stop() noexcept;
};
class Notifications
{
    struct State;
    std::unique_ptr<State> state;
public:
    Notifications();
    ~Notifications();
    void Tick( XUserHandle user, UINT32 title, const Invitation &callback ) noexcept;
    void Close() noexcept;
};
}
