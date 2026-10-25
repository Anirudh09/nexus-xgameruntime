/*
 * Xbox Game runtime Library
 *  GDK Component: System API -> XAccessibility and XSpeechSynthesis
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

#include "XAccessibility.h"

HRESULT WINAPI XAccessibilityImpl::XClosedCaptionGetProperties( XClosedCaptionProperties *properties )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XClosedCaptionSetEnabled( BOOLEAN enabled )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XHighContrastGetMode( XHighContrastMode *mode )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechToTextSetPositionHint( XSpeechToTextPositionHint position )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechToTextSendString( const char *speakerName, const char *content,
                                                            XSpeechToTextType type )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerEnumerateInstalledVoices(
    void *context, XSpeechSynthesizerInstalledVoicesCallback *callback )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerCreate( XSpeechSynthesizerHandle *speechSynthesizer )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerCloseHandle( XSpeechSynthesizerHandle speechSynthesizer )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerSetDefaultVoice( XSpeechSynthesizerHandle speechSynthesizer )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerSetCustomVoice( XSpeechSynthesizerHandle speechSynthesizer,
                                                                     const char *voiceId )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerCreateStreamFromText(
    XSpeechSynthesizerHandle speechSynthesizer, const char *text,
    XSpeechSynthesizerStreamHandle *speechSynthesisStream )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI
XAccessibilityImpl::XSpeechSynthesizerCloseStreamHandle( XSpeechSynthesizerStreamHandle speechSynthesisStream )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerGetStreamDataSize(
    XSpeechSynthesizerStreamHandle speechSynthesisStream, SIZE_T *bufferSize )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerGetStreamData(
    XSpeechSynthesizerStreamHandle speechSynthesisStream, SIZE_T bufferSize, void *buffer, SIZE_T *bufferUsed )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechToTextBeginHypothesisString( const char *speakerName, const char *content,
                                                                       XSpeechToTextType type, UINT32 *hypothesisId )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechToTextUpdateHypothesisString( UINT32 hypothesisId, const char *content )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechToTextFinalizeHypothesisString( UINT32 hypothesisId, const char *content )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechToTextCancelHypothesisString( UINT32 hypothesisId )
{
    ( void )0;
    return E_NOTIMPL;
}

HRESULT WINAPI XAccessibilityImpl::XSpeechSynthesizerCreateStreamFromSsml(
    XSpeechSynthesizerHandle speechSynthesizer, const char *ssml,
    XSpeechSynthesizerStreamHandle *speechSynthesisStream )
{
    ( void )0;
    return E_NOTIMPL;
}

IUnknown *GetAccessibilityInterface() noexcept
{
    static XAccessibilityImpl value;
    return &value;
}
