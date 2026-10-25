/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XGame
 *
 * Copyright 2026 Olivia Ryan
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "XGame.h"
#include "../Runtime/Configuration.h"
static UINT32 GetConfiguredTitleId()
{
    auto config = nexus::GetGameConfiguration();
    return config ? config->titleId : 0;
}

HRESULT WINAPI XGameImpl::XGameGetXboxTitleId( UINT32 *out )
{
    ( void )0;
    if ( !out ) return E_POINTER;
    if ( !( *out = GetConfiguredTitleId() ) ) return E_GAME_MISSING_GAME_CONFIG;
    return S_OK;
}

static void TraceLaunchPath( const WCHAR *executable ) noexcept
{
    const DWORD saved = GetLastError();
    char enabled[2];
    if ( GetEnvironmentVariableA( "NEXUS_LAUNCH_TRACE", enabled, sizeof( enabled ) ) == 1 && enabled[0] == '1' )
    {
        WCHAR directory[32768];
        const DWORD length = GetCurrentDirectoryW( ARRAY_SIZE( directory ), directory );
        if ( !length || length >= ARRAY_SIZE( directory ) ) directory[0] = 0;
        /* Deliberately exclude arguments, environment values and account data.
         * Paths are bounded diagnostic text, never a launch fallback policy. */
        INFO( "NEXUS_LAUNCH_META target=%.512ls cwd=%.512ls attributes=%08lx\n", executable, directory,
              GetFileAttributesW( executable ) );
    }
    SetLastError( saved );
}

static WCHAR *FindSiblingExecutable( const WCHAR *name ) noexcept
{
    /* A leaf name can also identify an executable beside the caller, as in
     * Windows process launch resolution. Never reinterpret an explicit path,
     * search PATH, change CWD or guess a different package root. */
    if ( wcspbrk( name, L"\\/:" ) || !wcscmp( name, L"." ) || !wcscmp( name, L".." ) ) return nullptr;
    auto *path = static_cast<WCHAR *>( malloc( 32768 * sizeof( WCHAR ) ) );
    if ( !path ) return nullptr;
    const DWORD length = GetModuleFileNameW( nullptr, path, 32768 );
    WCHAR *slash = length && length < 32768 ? wcsrchr( path, '\\' ) : nullptr;
    if ( !slash || static_cast<SIZE_T>( slash - path ) + 1 + wcslen( name ) >= 32768 )
    {
        free( path );
        return nullptr;
    }
    wcscpy( slash + 1, name );
    const DWORD attributes = GetFileAttributesW( path );
    if ( attributes == INVALID_FILE_ATTRIBUTES || ( attributes & FILE_ATTRIBUTE_DIRECTORY ) )
    {
        free( path );
        return nullptr;
    }
    return path;
}

void WINAPI XGameImpl::XLaunchNewGame( const char *exePath, const char *args, XUserHandle defaultUser )
{
    STARTUPINFOW startup = { sizeof( startup ) };
    PROCESS_INFORMATION process;
    WCHAR *executable = NULL, *arguments = NULL, *command = NULL, *sibling = NULL;
    SIZE_T length;
    int exe_count, args_count = 1;
    DWORD error = ERROR_INVALID_PARAMETER;

    /* The backend supplies one real signed-in account to the process tree.
     * A selected user must be that retained account, never a guessed handle
     * or a request to switch identities in the child. NULL keeps that account. */
    if ( defaultUser && !xuser_is_current_user( defaultUser ) )
    {
        error = ERROR_NOT_SUPPORTED;
        goto done;
    }
    if ( !exePath || !*exePath ||
         !( exe_count = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, exePath, -1, NULL, 0 ) ) )
        goto done;
    if ( args && !( args_count = MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, args, -1, NULL, 0 ) ) ) goto done;
    /* CreateProcessW's limit includes the quoted executable and terminator. */
    length = ( SIZE_T )exe_count + args_count + 2;
    if ( length > 32767 ) goto done;
    if ( !( executable = static_cast<std::remove_reference_t<decltype( executable )>>(
                malloc( exe_count * sizeof( WCHAR ) ) ) ) ||
         !( arguments = static_cast<std::remove_reference_t<decltype( arguments )>>(
                calloc( args_count, sizeof( WCHAR ) ) ) ) ||
         !( command =
                static_cast<std::remove_reference_t<decltype( command )>>( malloc( length * sizeof( WCHAR ) ) ) ) )
    {
        error = ERROR_NOT_ENOUGH_MEMORY;
        goto done;
    }
    if ( !MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, exePath, -1, executable, exe_count ) ||
         ( args && !MultiByteToWideChar( CP_UTF8, MB_ERR_INVALID_CHARS, args, -1, arguments, args_count ) ) )
        goto done;
    /* Quotes are not valid filename characters and would corrupt argv[0]. */
    if ( wcschr( executable, '"' ) ) goto done;
    command[0] = '"';
    memcpy( command + 1, executable, ( exe_count - 1 ) * sizeof( WCHAR ) );
    command[exe_count] = '"';
    command[exe_count + 1] = ' ';
    memcpy( command + exe_count + 2, arguments, args_count * sizeof( WCHAR ) );

    /* No shell or new authentication flow. Keep the current directory and
     * environment (package view, account bridge and runtime) in the new tree,
     * but do not leak the caller's unrelated inheritable Windows handles. */
    TraceLaunchPath( executable );
    if ( !CreateProcessW( executable, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process ) )
    {
        error = GetLastError();
        if ( error != ERROR_FILE_NOT_FOUND || !( sibling = FindSiblingExecutable( executable ) ) ) goto done;
        TraceLaunchPath( sibling );
        if ( !CreateProcessW( sibling, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process ) )
        {
            error = GetLastError();
            goto done;
        }
    }
    CloseHandle( process.hThread );
    CloseHandle( process.hProcess );
    error = ERROR_SUCCESS;
done:
    free( sibling );
    free( command );
    free( arguments );
    free( executable );
    if ( error ) ERR( "Game relaunch failed, error %lu.\n", error );
    /* The public contract never returns, including invalid-input/launch errors.
     * Normal process teardown retains the runtime's existing save cleanup. */
    ExitProcess( error ? 1 : 0 );
}

HRESULT WINAPI XGameImpl::XLaunchRestartOnCrash( const char *args, UINT32 reserved )
{
    ( void )0;
    return E_NOTIMPL;
}

IUnknown *GetGameInterface() noexcept
{
    static XGameImpl value;
    return &value;
}
