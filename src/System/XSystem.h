// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <unknwn.h>
namespace nexus
{
IUnknown *GetSystemInterface() noexcept;
IUnknown *GetSystemAnalyticsInterface() noexcept;
}
