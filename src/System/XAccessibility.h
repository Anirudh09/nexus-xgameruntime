// SPDX-License-Identifier: LGPL-2.1-or-later
#pragma once
#include "../private.h"
#include "StaticComObject.h"
#include <xaccessibility.h>
class XAccessibilityImpl final : public nexus::StaticComObject<IXAccessibilityImpl2, IXAccessibilityImpl>
{
public:
    HRESULT WINAPI XClosedCaptionGetProperties( XClosedCaptionProperties *properties ) override;
    HRESULT WINAPI XClosedCaptionSetEnabled( BOOLEAN enabled ) override;
    HRESULT WINAPI XHighContrastGetMode( XHighContrastMode *mode ) override;
    HRESULT WINAPI XSpeechToTextSetPositionHint( XSpeechToTextPositionHint position ) override;
    HRESULT WINAPI XSpeechToTextSendString( const char *speakerName, const char *content,
                                            XSpeechToTextType type ) override;
    HRESULT WINAPI XSpeechSynthesizerEnumerateInstalledVoices(
        void *context, XSpeechSynthesizerInstalledVoicesCallback *callback ) override;
    HRESULT WINAPI XSpeechSynthesizerCreate( XSpeechSynthesizerHandle *speechSynthesizer ) override;
    HRESULT WINAPI XSpeechSynthesizerCloseHandle( XSpeechSynthesizerHandle speechSynthesizer ) override;
    HRESULT WINAPI XSpeechSynthesizerSetDefaultVoice( XSpeechSynthesizerHandle speechSynthesizer ) override;
    HRESULT WINAPI XSpeechSynthesizerSetCustomVoice( XSpeechSynthesizerHandle speechSynthesizer,
                                                     const char *voiceId ) override;
    HRESULT WINAPI
    XSpeechSynthesizerCreateStreamFromText( XSpeechSynthesizerHandle speechSynthesizer, const char *text,
                                            XSpeechSynthesizerStreamHandle *speechSynthesisStream ) override;
    HRESULT WINAPI XSpeechSynthesizerCloseStreamHandle( XSpeechSynthesizerStreamHandle speechSynthesisStream ) override;
    HRESULT WINAPI XSpeechSynthesizerGetStreamDataSize( XSpeechSynthesizerStreamHandle speechSynthesisStream,
                                                        SIZE_T *bufferSize ) override;
    HRESULT WINAPI XSpeechSynthesizerGetStreamData( XSpeechSynthesizerStreamHandle speechSynthesisStream,
                                                    SIZE_T bufferSize, void *buffer, SIZE_T *bufferUsed ) override;
    HRESULT WINAPI XSpeechToTextBeginHypothesisString( const char *speakerName, const char *content,
                                                       XSpeechToTextType type, UINT32 *hypothesisId ) override;
    HRESULT WINAPI XSpeechToTextUpdateHypothesisString( UINT32 hypothesisId, const char *content ) override;
    HRESULT WINAPI XSpeechToTextFinalizeHypothesisString( UINT32 hypothesisId, const char *content ) override;
    HRESULT WINAPI XSpeechToTextCancelHypothesisString( UINT32 hypothesisId ) override;
    HRESULT WINAPI
    XSpeechSynthesizerCreateStreamFromSsml( XSpeechSynthesizerHandle speechSynthesizer, const char *ssml,
                                            XSpeechSynthesizerStreamHandle *speechSynthesisStream ) override;
};
IUnknown *GetAccessibilityInterface() noexcept;
