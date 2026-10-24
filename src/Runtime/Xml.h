// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include <windows.h>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace nexus
{
// Bounded, value-owned XML. DTDs and external entities are prohibited. Parsing
// must finish before the caller publishes GetGameConfiguration or session state.
struct XmlNode
{
    std::string name, text;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<XmlNode> children;
    const XmlNode *child( std::string_view key ) const noexcept;
    std::string_view attribute( std::string_view key ) const noexcept;
};
HRESULT ParseXml( std::string_view bytes, XmlNode &root ) noexcept;
HRESULT SerializeXml( const XmlNode &root, std::string &output ) noexcept;
HRESULT ReadXmlFile( const std::wstring &path, XmlNode &root ) noexcept;
HRESULT ToUtf8( std::wstring_view input, std::string &output ) noexcept;
HRESULT ToUtf16( std::string_view input, std::wstring &output ) noexcept;
}
