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

void WINAPI XGameImpl::XLaunchNewGame( const char *exePath, const char *args, XUserHandle defaultUser )
{
    STARTUPINFOW startup = { sizeof( startup ) };
    PROCESS_INFORMATION process;
    WCHAR *executable = NULL, *arguments = NULL, *command = NULL;
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
    if ( !CreateProcessW( executable, command, NULL, NULL, FALSE, 0, NULL, NULL, &startup, &process ) )
    {
        error = GetLastError();
        goto done;
    }
    CloseHandle( process.hThread );
    CloseHandle( process.hProcess );
    error = ERROR_SUCCESS;
done:
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
