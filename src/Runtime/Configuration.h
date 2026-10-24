// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "Xml.h"
#include <xgameruntimeinit.h>

namespace nexus
{
struct GameConfiguration
{
    XmlNode game;
    std::string msaAppId, storeId, packageFamilyName;
    UINT32 titleId = 0;
    bool fullTrust = false;
    std::wstring directory;
    std::vector<std::string> relatedProducts;
};
HRESULT InitializeGameConfiguration( const INITIALIZE_OPTIONS *options ) noexcept;
const GameConfiguration *GetGameConfiguration() noexcept;
HRESULT GetPackageFamilyName( std::string_view name, std::string_view publisher, std::string &result ) noexcept;
}
