// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "XUser.h"
#include <string>
#include <vector>

namespace nexus::presence
{
// The launcher owns HTTP deadlines and cleanup after the game process exits.
HRESULT Prepare( XUserHandle user, UINT32 title, std::vector<std::string> &fields ) noexcept;
void Start( XUserHandle user ) noexcept;
void Stop() noexcept;
}
