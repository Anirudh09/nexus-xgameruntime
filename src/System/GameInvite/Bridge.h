// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../../private.h"
#include <string>
#include <vector>

namespace nexus::invites
{
class Transport
{
public:
    virtual ~Transport() = default;
    virtual HRESULT Exchange( const std::vector<std::string> &request, std::vector<std::string> &reply ) noexcept = 0;
};
bool BridgeAvailable() noexcept;
Transport &GetBridge() noexcept;
std::string NewId();
}
