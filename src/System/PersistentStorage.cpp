// SPDX-License-Identifier: LGPL-2.1-or-later
// Adapted from XPersistentLocalStorage, Copyright 2026 Olivia Ryan.
#include "../private.h"
#include <xpersistentlocalstorage.h>
#include "PersistentStorage.h"
#include "StaticComObject.h"
#include "../Runtime/Configuration.h"
#include <shlobj.h>
#include <cstdio>
#include <cstring>

namespace nexus
{
static HRESULT GetStoragePath( std::string &path )
{
    const auto *config = GetGameConfiguration();
    if ( !config ) return E_GAMERUNTIME_NOT_INITIALIZED;
    WCHAR local[MAX_PATH];
    HRESULT hr =
        SHGetFolderPathW( nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr, SHGFP_TYPE_CURRENT, local );
    if ( FAILED( hr ) ) return hr;
    char title[9];
    std::snprintf( title, sizeof( title ), "%08X", config->titleId );
    std::wstring leaf;
    if ( FAILED( hr = ToUtf16( config->packageFamilyName.empty() ? title : config->packageFamilyName, leaf ) ) )
        return hr;
    std::wstring wide = std::wstring( local ) + L"\\XodusPersistentLocalStorage";
    if ( !CreateDirectoryW( wide.c_str(), nullptr ) && GetLastError() != ERROR_ALREADY_EXISTS )
        return HRESULT_FROM_WIN32( GetLastError() );
    wide += L"\\" + leaf;
    if ( !CreateDirectoryW( wide.c_str(), nullptr ) && GetLastError() != ERROR_ALREADY_EXISTS )
        return HRESULT_FROM_WIN32( GetLastError() );
    return ToUtf8( wide, path );
}
class XPersistentLocalStorageImpl final
    : public StaticComObject<IXPersistentLocalStorageImpl3, IXPersistentLocalStorageImpl2, IXPersistentLocalStorageImpl>
{
  public:
    HRESULT WINAPI XPersistentLocalStorageGetPathSize( SIZE_T *size ) override
    {
        if ( !size ) return E_POINTER;
        *size = 0;
        try
        {
            std::string path;
            HRESULT hr = GetStoragePath( path );
            if ( SUCCEEDED( hr ) ) *size = path.size() + 1;
            return hr;
        }
        catch ( const std::bad_alloc & )
        {
            return E_OUTOFMEMORY;
        }
    }
    HRESULT WINAPI XPersistentLocalStorageGetPath( SIZE_T size, char *output, SIZE_T *used ) override
    {
        if ( !output ) return E_POINTER;
        try
        {
            std::string path;
            HRESULT hr = GetStoragePath( path );
            if ( FAILED( hr ) ) return hr;
            if ( size < path.size() + 1 ) return E_NOT_SUFFICIENT_BUFFER;
            std::memcpy( output, path.c_str(), path.size() + 1 );
            if ( used ) *used = path.size() + 1;
            return S_OK;
        }
        catch ( const std::bad_alloc & )
        {
            return E_OUTOFMEMORY;
        }
    }
    HRESULT WINAPI XPersistentLocalStorageGetSpaceInfo( XPersistentLocalStorageSpaceInfo *info ) override
    {
        if ( !info ) return E_POINTER;
        *info = {};
        try
        {
            std::string path;
            HRESULT hr = GetStoragePath( path );
            if ( FAILED( hr ) ) return hr;
            ULARGE_INTEGER available, total, free;
            if ( !GetDiskFreeSpaceExA( path.c_str(), &available, &total, &free ) )
                return HRESULT_FROM_WIN32( GetLastError() );
            info->availableFreeBytes = available.QuadPart;
            info->totalFreeBytes = free.QuadPart;
            info->totalBytes = total.QuadPart;
            return S_OK;
        }
        catch ( const std::bad_alloc & )
        {
            return E_OUTOFMEMORY;
        }
    }
    HRESULT WINAPI XPersistentLocalStoragePromptUserForSpaceAsync( UINT64, XAsyncBlock * ) override
    {
        return E_NOTIMPL;
    }
    HRESULT WINAPI XPersistentLocalStoragePromptUserForSpaceResult( XAsyncBlock * ) override
    {
        return E_NOTIMPL;
    }
    HRESULT WINAPI XPersistentLocalStorageMountForPackage( const char *, XPackageMountHandle *handle ) override
    {
        if ( handle ) *handle = nullptr;
        return E_NOTIMPL;
    }
};
IUnknown *GetPersistentStorageInterface() noexcept
{
    static XPersistentLocalStorageImpl storage;
    return &storage;
}
}
