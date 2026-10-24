// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace nexus {
// Bounded, value-owned XML. DTDs and external entities are prohibited. Parsing
// must finish before the caller publishes configuration or session state.
struct XmlNode {
    std::string name, text;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<XmlNode> children;
    const XmlNode* child(std::string_view key) const noexcept;
    std::string_view attribute(std::string_view key) const noexcept;
};
HRESULT parse_xml(std::string_view bytes, XmlNode& root) noexcept;
HRESULT read_xml_file(const std::wstring& path, XmlNode& root) noexcept;
HRESULT to_utf8(std::wstring_view input, std::string& output) noexcept;
HRESULT to_utf16(std::string_view input, std::wstring& output) noexcept;
}
