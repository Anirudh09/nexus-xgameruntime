// SPDX-License-Identifier: LGPL-2.1-or-later
#include "../private.h"
#include "Xml.h"
#include <set>
#include <libxml/xmlreader.h>
#include <limits>
#include <memory>
#include <new>

namespace nexus
{
const XmlNode *XmlNode::child( std::string_view key ) const noexcept
{
    for ( const auto &node : children )
        if ( node.name == key ) return &node;
    return nullptr;
}
std::string_view XmlNode::attribute( std::string_view key ) const noexcept
{
    for ( const auto &[name, value] : attributes )
        if ( name == key ) return value;
    return {};
}
HRESULT ToUtf8( std::wstring_view input, std::string &output ) noexcept
{
    if ( input.size() > INT_MAX ) return E_INVALIDARG;
    try
    {
        if ( input.empty() )
        {
            output.clear();
            return S_OK;
        }
        int size = WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>( input.size() ),
                                        nullptr, 0, nullptr, nullptr );
        if ( !size ) return HRESULT_FROM_WIN32( GetLastError() );
        output.resize( size );
        if ( !WideCharToMultiByte( CP_UTF8, WC_ERR_INVALID_CHARS, input.data(), static_cast<int>( input.size() ),
                                   output.data(), size, nullptr, nullptr ) )
            return HRESULT_FROM_WIN32( GetLastError() );
        return S_OK;
    }
    catch ( const std::bad_alloc & )
    {
        return E_OUTOFMEMORY;
    }
}
HRESULT ToUtf16( std::string_view input, std::wstring &output ) noexcept
{
    if ( input.size() > INT_MAX ) return E_INVALIDARG;
    try
    {
        if ( input.empty() )
        {
            output.clear();
            return S_OK;
        }
        int size = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>( input.size() ),
                                        nullptr, 0 );
        if ( !size ) return HRESULT_FROM_WIN32( GetLastError() );
        output.resize( size );
        if ( !MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, input.data(), static_cast<int>( input.size() ),
                                   output.data(), size ) )
            return HRESULT_FROM_WIN32( GetLastError() );
        return S_OK;
    }
    catch ( const std::bad_alloc & )
    {
        return E_OUTOFMEMORY;
    }
}

HRESULT ParseXml( std::string_view bytes, XmlNode &root ) noexcept
{
    if ( bytes.empty() || bytes.size() > 4 * 1024 * 1024 ) return E_INVALIDARG;
    try
    {
        using Reader = std::unique_ptr<xmlTextReader, decltype( &xmlFreeTextReader )>;
        Reader reader( xmlReaderForMemory( bytes.data(), static_cast<int>( bytes.size() ), nullptr, nullptr,
                                           XML_PARSE_NONET | XML_PARSE_NOERROR | XML_PARSE_NOWARNING ),
                       xmlFreeTextReader );
        if ( !reader ) return E_OUTOFMEMORY;
        // Never load DTDs or substitute general entities, including local files.
        // The streaming reader rejects a DTD before advancing into its content.
        if ( xmlTextReaderSetParserProp( reader.get(), XML_PARSER_LOADDTD, 0 ) < 0 ||
             xmlTextReaderSetParserProp( reader.get(), XML_PARSER_SUBST_ENTITIES, 0 ) < 0 )
            return E_FAIL;
        XmlNode candidate;
        std::vector<XmlNode *> stack;
        bool found = false;
        size_t nodes = 0;
        int status;
        while ( ( status = xmlTextReaderRead( reader.get() ) ) == 1 )
        {
            int type = xmlTextReaderNodeType( reader.get() );
            if ( type == XML_READER_TYPE_DOCUMENT_TYPE || type == XML_READER_TYPE_ENTITY_REFERENCE ||
                 type == XML_READER_TYPE_ENTITY )
                return E_INVALIDARG;
            if ( type == XML_READER_TYPE_ELEMENT )
            {
                if ( ++nodes > 16384 || stack.size() >= 64 ) return E_INVALIDARG;
                XmlNode *node;
                if ( stack.empty() )
                {
                    if ( found ) return E_INVALIDARG;
                    node = &candidate;
                    found = true;
                }
                else
                {
                    stack.back()->children.emplace_back();
                    node = &stack.back()->children.back();
                }
                const auto *name = xmlTextReaderConstLocalName( reader.get() );
                if ( !name ) return E_INVALIDARG;
                node->name = reinterpret_cast<const char *>( name );
                int attr = xmlTextReaderMoveToFirstAttribute( reader.get() );
                while ( attr == 1 )
                {
                    if ( node->attributes.size() >= 256 ) return E_INVALIDARG;
                    const auto *key = xmlTextReaderConstName( reader.get() );
                    const auto *value = xmlTextReaderConstValue( reader.get() );
                    if ( !key || !value ) return E_INVALIDARG;
                    node->attributes.emplace_back( reinterpret_cast<const char *>( key ),
                                                   reinterpret_cast<const char *>( value ) );
                    attr = xmlTextReaderMoveToNextAttribute( reader.get() );
                }
                if ( attr < 0 ) return E_INVALIDARG;
                xmlTextReaderMoveToElement( reader.get() );
                if ( !xmlTextReaderIsEmptyElement( reader.get() ) ) stack.push_back( node );
            }
            else if ( type == XML_READER_TYPE_END_ELEMENT )
            {
                if ( stack.empty() ) return E_INVALIDARG;
                stack.pop_back();
            }
            else if ( type == XML_READER_TYPE_TEXT || type == XML_READER_TYPE_CDATA ||
                      type == XML_READER_TYPE_WHITESPACE || type == XML_READER_TYPE_SIGNIFICANT_WHITESPACE )
            {
                if ( !stack.empty() )
                {
                    const auto *text = xmlTextReaderConstValue( reader.get() );
                    if ( text ) stack.back()->text += reinterpret_cast<const char *>( text );
                }
            }
        }
        if ( status < 0 || !found || !stack.empty() ) return E_INVALIDARG;
        root = std::move( candidate );
        return S_OK;
    }
    catch ( const std::bad_alloc & )
    {
        return E_OUTOFMEMORY;
    }
}

HRESULT ReadXmlFile( const std::wstring &path, XmlNode &root ) noexcept
{
    HANDLE file = CreateFileW( path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                               FILE_ATTRIBUTE_NORMAL, nullptr );
    if ( file == INVALID_HANDLE_VALUE ) return HRESULT_FROM_WIN32( GetLastError() );
    HRESULT hr = E_FAIL;
    try
    {
        LARGE_INTEGER size;
        if ( !GetFileSizeEx( file, &size ) )
            hr = HRESULT_FROM_WIN32( GetLastError() );
        else if ( size.QuadPart <= 0 || size.QuadPart > 4 * 1024 * 1024 )
            hr = E_INVALIDARG;
        else
        {
            std::string bytes( static_cast<size_t>( size.QuadPart ), '\0' );
            DWORD read = 0;
            if ( !ReadFile( file, bytes.data(), static_cast<DWORD>( bytes.size() ), &read, nullptr ) )
                hr = HRESULT_FROM_WIN32( GetLastError() );
            else if ( read != bytes.size() )
                hr = HRESULT_FROM_WIN32( ERROR_HANDLE_EOF );
            else
                hr = ParseXml( bytes, root );
        }
    }
    catch ( const std::bad_alloc & )
    {
        hr = E_OUTOFMEMORY;
    }
    CloseHandle( file );
    return hr;
}
}

namespace nexus
{
static HRESULT PopulateXmlNode( xmlNode *target, const XmlNode &source, unsigned depth, unsigned &count )
{
    if ( depth > 64 || ++count > 16384 || source.attributes.size() > 256 ) return E_INVALIDARG;
    std::set<std::string> attributes;
    for ( const auto &[name, value] : source.attributes )
    {
        if ( name.empty() || xmlValidateName( BAD_CAST name.c_str(), 0 ) || !attributes.insert( name ).second )
            return E_INVALIDARG;
        if ( !xmlNewProp( target, BAD_CAST name.c_str(), BAD_CAST value.c_str() ) ) return E_OUTOFMEMORY;
    }
    if ( !source.text.empty() )
    {
        auto text = xmlNewText( BAD_CAST source.text.c_str() );
        if ( !text ) return E_OUTOFMEMORY;
        xmlAddChild( target, text );
    }
    for ( const auto &child : source.children )
    {
        if ( child.name.empty() || xmlValidateName( BAD_CAST child.name.c_str(), 0 ) ) return E_INVALIDARG;
        auto element = xmlNewChild( target, nullptr, BAD_CAST child.name.c_str(), nullptr );
        if ( !element ) return E_OUTOFMEMORY;
        auto hr = PopulateXmlNode( element, child, depth + 1, count );
        if ( FAILED( hr ) ) return hr;
    }
    return S_OK;
}
HRESULT SerializeXml( const XmlNode &root, std::string &output ) noexcept
{
    output.clear();
    if ( root.name.empty() || xmlValidateName( BAD_CAST root.name.c_str(), 0 ) ) return E_INVALIDARG;
    try
    {
        std::unique_ptr<xmlDoc, decltype( &xmlFreeDoc )> document( xmlNewDoc( BAD_CAST "1.0" ), xmlFreeDoc );
        if ( !document ) return E_OUTOFMEMORY;
        auto element = xmlNewNode( nullptr, BAD_CAST root.name.c_str() );
        if ( !element ) return E_OUTOFMEMORY;
        xmlDocSetRootElement( document.get(), element );
        unsigned count = 0;
        auto hr = PopulateXmlNode( element, root, 0, count );
        if ( FAILED( hr ) ) return hr;
        xmlChar *bytes = nullptr;
        int size = 0;
        xmlDocDumpMemoryEnc( document.get(), &bytes, &size, "UTF-8" );
        std::unique_ptr<xmlChar, decltype( xmlFree )> buffer( bytes, xmlFree );
        if ( !buffer ) return E_OUTOFMEMORY;
        if ( size <= 0 || size > 4 * 1024 * 1024 ) return E_INVALIDARG;
        output.assign( reinterpret_cast<const char *>( bytes ), size );
        return S_OK;
    }
    catch ( const std::bad_alloc & )
    {
        return E_OUTOFMEMORY;
    }
}
}
