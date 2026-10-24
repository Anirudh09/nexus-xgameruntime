// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "Xml.h"
#include <xgameruntimeinit.h>

namespace nexus {
struct Configuration {
    XmlNode game;
    std::string msa_app_id, store_id, package_family;
    UINT32 title_id = 0;
    bool full_trust = false;
    std::wstring directory;
    std::vector<std::string> related_products;
};
HRESULT initialize_configuration(const INITIALIZE_OPTIONS* options) noexcept;
const Configuration* configuration() noexcept;
HRESULT package_family_name(std::string_view name, std::string_view publisher, std::string& result) noexcept;
}
