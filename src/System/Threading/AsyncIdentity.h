// SPDX-License-Identifier: MIT
#pragma once
#include <windows.h>
#include <xasync.h>
namespace nexus
{
// Internal, non-consuming validation for result APIs whose only payload is an
// HRESULT. Only the async implementation interprets XAsyncBlock::internal.
HRESULT GetAsyncStatusForIdentity( XAsyncBlock *async, const void *identity ) noexcept;
}
