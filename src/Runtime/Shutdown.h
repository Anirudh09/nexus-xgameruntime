// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <windows.h>
namespace nexus
{
using CloudShutdownCallback = HRESULT( WINAPI * )( void * );
HRESULT RegisterCloudShutdown( CloudShutdownCallback callback, void *context ) noexcept;
HRESULT DrainCloudShutdown();
}
