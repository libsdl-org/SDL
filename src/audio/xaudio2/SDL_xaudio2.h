/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _SDL_XAUDIO2_H
#define _SDL_XAUDIO2_H

#include <windows.h>
#include <mmreg.h>
#include <objbase.h>

/* XAudio2 packs its structure members together as tightly as possible.
   This pragma is needed to ensure compatibility with XAudio2 on 64-bit
   platforms.
*/
#pragma pack(push, 1)

typedef interface IXAudio2 IXAudio2;
typedef interface IXAudio2SourceVoice IXAudio2SourceVoice;
typedef interface IXAudio2MasteringVoice IXAudio2MasteringVoice;
typedef interface IXAudio2EngineCallback IXAudio2EngineCallback;
typedef interface IXAudio2VoiceCallback IXAudio2VoiceCallback;
typedef interface IXAudio2Voice IXAudio2Voice;
typedef interface IXAudio2SubmixVoice IXAudio2SubmixVoice;

typedef enum _AUDIO_STREAM_CATEGORY {
    AudioCategory_Other = 0,
    AudioCategory_ForegroundOnlyMedia,
    AudioCategory_BackgroundCapableMedia,
    AudioCategory_Communications,
    AudioCategory_Alerts,
    AudioCategory_SoundEffects,
    AudioCategory_GameEffects,
    AudioCategory_GameMedia,
    AudioCategory_GameChat,
    AudioCategory_Movie,
    AudioCategory_Media
} AUDIO_STREAM_CATEGORY;

typedef struct XAUDIO2_BUFFER {
    UINT32     Flags;
    UINT32     AudioBytes;
    const BYTE *pAudioData;
    UINT32     PlayBegin;
    UINT32     PlayLength;
    UINT32     LoopBegin;
    UINT32     LoopLength;
    UINT32     LoopCount;
    void       *pContext;
} XAUDIO2_BUFFER;

typedef struct XAUDIO2_BUFFER_WMA {
    const UINT32 *pDecodedPacketCumulativeBytes;
    UINT32       PacketCount;
} XAUDIO2_BUFFER_WMA;

typedef struct XAUDIO2_SEND_DESCRIPTOR {
    UINT32        Flags;
    IXAudio2Voice *pOutputVoice;
} XAUDIO2_SEND_DESCRIPTOR;

typedef struct XAUDIO2_VOICE_SENDS {
    UINT32                  SendCount;
    XAUDIO2_SEND_DESCRIPTOR *pSends;
} XAUDIO2_VOICE_SENDS;

typedef struct XAUDIO2_EFFECT_DESCRIPTOR {
    IUnknown *pEffect;
    BOOL     InitialState;
    UINT32   OutputChannels;
} XAUDIO2_EFFECT_DESCRIPTOR;

typedef struct XAUDIO2_EFFECT_CHAIN {
    UINT32                    EffectCount;
    XAUDIO2_EFFECT_DESCRIPTOR *pEffectDescriptors;
} XAUDIO2_EFFECT_CHAIN;

typedef struct XAUDIO2_PERFORMANCE_DATA {
    UINT64 AudioCyclesSinceLastQuery;
    UINT64 TotalCyclesSinceLastQuery;
    UINT32 MinimumCyclesPerQuantum;
    UINT32 MaximumCyclesPerQuantum;
    UINT32 MemoryUsageInBytes;
    UINT32 CurrentLatencyInSamples;
    UINT32 GlitchesSinceEngineStarted;
    UINT32 ActiveSourceVoiceCount;
    UINT32 TotalSourceVoiceCount;
    UINT32 ActiveSubmixVoiceCount;
    UINT32 ActiveResamplerCount;
    UINT32 ActiveMatrixMixCount;
    UINT32 ActiveXmaSourceVoices;
    UINT32 ActiveXmaStreams;
} XAUDIO2_PERFORMANCE_DATA;

typedef struct XAUDIO2_DEBUG_CONFIGURATION {
    UINT32 TraceMask;
    UINT32 BreakMask;
    BOOL   LogThreadID;
    BOOL   LogFileline;
    BOOL   LogFunctionName;
    BOOL   LogTiming;
} XAUDIO2_DEBUG_CONFIGURATION;

typedef struct XAUDIO2_VOICE_DETAILS {
    UINT32 CreationFlags;
    UINT32 ActiveFlags;
    UINT32 InputChannels;
    UINT32 InputSampleRate;
} XAUDIO2_VOICE_DETAILS;

typedef enum XAUDIO2_FILTER_TYPE {
    LowPassFilter = 0,
    BandPassFilter = 1,
    HighPassFilter = 2,
    NotchFilter = 3,
    LowPassOnePoleFilter = 4,
    HighPassOnePoleFilter = 5
} XAUDIO2_FILTER_TYPE;

typedef struct XAUDIO2_FILTER_PARAMETERS {
    XAUDIO2_FILTER_TYPE Type;
    float               Frequency;
    float               OneOverQ;
} XAUDIO2_FILTER_PARAMETERS;

typedef struct XAUDIO2_VOICE_STATE {
    void   *pCurrentBufferContext;
    UINT32 BuffersQueued;
    UINT64 SamplesPlayed;
} XAUDIO2_VOICE_STATE;


typedef UINT32 XAUDIO2_PROCESSOR;
#define Processor1 0x00000001
#define XAUDIO2_DEFAULT_PROCESSOR Processor1

#define XAUDIO2_E_DEVICE_INVALIDATED 0x88960004
#define XAUDIO2_COMMIT_NOW 0
#define XAUDIO2_VOICE_NOSAMPLESPLAYED 0x0100
#define XAUDIO2_DEFAULT_CHANNELS 0

extern HRESULT __stdcall XAudio2Create(
    _Out_ IXAudio2          **ppXAudio2,
    _In_  UINT32            Flags,
    _In_  XAUDIO2_PROCESSOR XAudio2Processor
    );

#undef INTERFACE
#define INTERFACE IXAudio2
typedef interface IXAudio2 {
    const struct IXAudio2Vtbl FAR* lpVtbl;
} IXAudio2;
typedef const struct IXAudio2Vtbl IXAudio2Vtbl;
const struct IXAudio2Vtbl
{
    /* IUnknown */
    STDMETHOD(QueryInterface)(THIS_ REFIID iid, LPVOID *ppv) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    /* IXAudio2 */
    STDMETHOD_(HRESULT, RegisterForCallbacks)(THIS, IXAudio2EngineCallback *pCallback) PURE;
    STDMETHOD_(VOID, UnregisterForCallbacks)(THIS, IXAudio2EngineCallback *pCallback) PURE;
    STDMETHOD_(HRESULT, CreateSourceVoice)(THIS, IXAudio2SourceVoice **ppSourceVoice,
                                           const WAVEFORMATEX *pSourceFormat,
                                           UINT32 Flags,
                                           float MaxFrequencyRatio,
                                           IXAudio2VoiceCallback *pCallback,
                                           const XAUDIO2_VOICE_SENDS *pSendList,
                                           const XAUDIO2_EFFECT_CHAIN *pEffectChain) PURE;
    STDMETHOD_(HRESULT, CreateSubmixVoice)(THIS, IXAudio2SubmixVoice **ppSubmixVoice,
                                           UINT32 InputChannels,
                                           UINT32 InputSampleRate,
                                           UINT32 Flags,
                                           UINT32 ProcessingStage,
                                           const XAUDIO2_VOICE_SENDS *pSendList,
                                           const XAUDIO2_EFFECT_CHAIN *pEffectChain) PURE;
    STDMETHOD_(HRESULT, CreateMasteringVoice)(THIS, IXAudio2MasteringVoice **ppMasteringVoice,
                                              UINT32 InputChannels,
                                              UINT32 InputSampleRate,
                                              UINT32 Flags,
                                              LPCWSTR szDeviceId,
                                              const XAUDIO2_EFFECT_CHAIN *pEffectChain,
                                              AUDIO_STREAM_CATEGORY StreamCategory) PURE;
    STDMETHOD_(HRESULT, StartEngine)(THIS) PURE;
    STDMETHOD_(VOID, StopEngine)(THIS) PURE;
    STDMETHOD_(HRESULT, CommitChanges)(THIS, UINT32 OperationSet) PURE;
    STDMETHOD_(HRESULT, GetPerformanceData)(THIS, XAUDIO2_PERFORMANCE_DATA *pPerfData) PURE;
    STDMETHOD_(HRESULT, SetDebugConfiguration)(THIS, XAUDIO2_DEBUG_CONFIGURATION *pDebugConfiguration,
                                               VOID *pReserved) PURE;
};

#define IXAudio2_Release(A) ((A)->lpVtbl->Release(A))
#define IXAudio2_CreateSourceVoice(A,B,C,D,E,F,G,H) ((A)->lpVtbl->CreateSourceVoice(A,B,C,D,E,F,G,H))
#define IXAudio2_CreateMasteringVoice(A,B,C,D,E,F,G,H) ((A)->lpVtbl->CreateMasteringVoice(A,B,C,D,E,F,G,H))
#define IXAudio2_StartEngine(A) ((A)->lpVtbl->StartEngine(A))
#define IXAudio2_StopEngine(A) ((A)->lpVtbl->StopEngine(A))


#undef INTERFACE
#define INTERFACE IXAudio2SourceVoice
typedef interface IXAudio2SourceVoice {
    const struct IXAudio2SourceVoiceVtbl FAR* lpVtbl;
} IXAudio2SourceVoice;
typedef const struct IXAudio2SourceVoiceVtbl IXAudio2SourceVoiceVtbl;
const struct IXAudio2SourceVoiceVtbl
{
    /* MSDN says that IXAudio2Voice inherits from IXAudio2, but MSVC's debugger
     * says otherwise, and that IXAudio2Voice doesn't inherit from any other
     * interface!
     */

    /* IXAudio2Voice */
    STDMETHOD_(VOID, GetVoiceDetails)(THIS, XAUDIO2_VOICE_DETAILS *pVoiceDetails) PURE;
    STDMETHOD_(HRESULT, SetOutputVoices)(THIS, const XAUDIO2_VOICE_SENDS *pSendList) PURE;
    STDMETHOD_(HRESULT, SetEffectChain)(THIS, const XAUDIO2_EFFECT_CHAIN *pEffectChain) PURE;
    STDMETHOD_(HRESULT, EnableEffect)(THIS, UINT32 EffectIndex, UINT32 OperationSet) PURE;
    STDMETHOD_(HRESULT, DisableEffect)(THIS, UINT32 EffectIndex, UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetEffectState)(THIS, UINT32 EffectIndex, BOOL *pEnabled) PURE;
    STDMETHOD_(HRESULT, SetEffectParameters)(THIS, UINT32 EffectIndex,
                                             const void *pParameters,
                                             UINT32 ParametersByteSize,
                                             UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetEffectParameters)(THIS, UINT32 EffectIndex,
                                          void *pParameters,
                                          UINT32 ParametersByteSize) PURE;
    STDMETHOD_(HRESULT, SetFilterParameters)(THIS, const XAUDIO2_FILTER_PARAMETERS *pParameters,
                                             UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetFilterParameters)(THIS, XAUDIO2_FILTER_PARAMETERS *pParameters) PURE;
    STDMETHOD_(HRESULT, SetOutputFilterParameters)(THIS, IXAudio2Voice *pDestinationVoice,
                                                   XAUDIO2_FILTER_PARAMETERS *pParameters,
                                                   UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetOutputFilterParameters)(THIS, IXAudio2Voice *pDestinationVoice,
                                                XAUDIO2_FILTER_PARAMETERS *pParameters) PURE;
    STDMETHOD_(HRESULT, SetVolume)(THIS, float Volume,
                                   UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetVolume)(THIS, float *pVolume) PURE;
    STDMETHOD_(HRESULT, SetChannelVolumes)(THIS, UINT32 Channels,
                                           const float *pVolumes,
                                           UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetChannelVolumes)(THIS, UINT32 Channels,
                                        float *pVolumes) PURE;
    STDMETHOD_(HRESULT, SetOutputMatrix)(THIS, IXAudio2Voice *pDestinationVoice,
                                         UINT32 SourceChannels,
                                         UINT32 DestinationChannels,
                                         const float *pLevelMatrix,
                                         UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetOutputMatrix)(THIS, IXAudio2Voice *pDestinationVoice,
                                      UINT32 SourceChannels,
                                      UINT32 DestinationChannels,
                                      float *pLevelMatrix) PURE;
    STDMETHOD_(VOID, DestroyVoice)(THIS) PURE;

    /* IXAudio2SourceVoice */
    STDMETHOD_(HRESULT, Start)(THIS, UINT32 Flags,
                               UINT32 OperationSet) PURE;
    STDMETHOD_(HRESULT, Stop)(THIS, UINT32 Flags,
                              UINT32 OperationSet) PURE;
    STDMETHOD_(HRESULT, SubmitSourceBuffer)(THIS, const XAUDIO2_BUFFER *pBuffer,
                                            const XAUDIO2_BUFFER_WMA *pBufferWMA) PURE;
    STDMETHOD_(HRESULT, FlushSourceBuffers)(THIS) PURE;
    STDMETHOD_(HRESULT, Discontinuity)(THIS) PURE;
    STDMETHOD_(HRESULT, ExitLoop)(THIS, UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetState)(THIS, XAUDIO2_VOICE_STATE *pVoiceState,
                               UINT32 Flags) PURE;
    STDMETHOD_(HRESULT, SetFrequencyRatio)(THIS, float Ratio,
                                           UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetFrequencyRatio)(THIS, float *pRatio) PURE;
    STDMETHOD_(HRESULT, SetSourceSampleRate)(THIS, UINT32 NewSourceSampleRate) PURE;
};

#define IXAudio2SourceVoice_DestroyVoice(A) ((A)->lpVtbl->DestroyVoice(A))
#define IXAudio2SourceVoice_Start(A,B,C) ((A)->lpVtbl->Start(A,B,C))
#define IXAudio2SourceVoice_Stop(A,B,C) ((A)->lpVtbl->Stop(A,B,C))
#define IXAudio2SourceVoice_SubmitSourceBuffer(A,B,C) ((A)->lpVtbl->SubmitSourceBuffer(A,B,C))
#define IXAudio2SourceVoice_FlushSourceBuffers(A) ((A)->lpVtbl->FlushSourceBuffers(A))
#define IXAudio2SourceVoice_Discontinuity(A) ((A)->lpVtbl->Discontinuity(A))
#define IXAudio2SourceVoice_GetState(A,B,C) ((A)->lpVtbl->GetState(A,B,C))


#undef INTERFACE
#define INTERFACE IXAudio2MasteringVoice
typedef interface IXAudio2MasteringVoice {
    const struct IXAudio2MasteringVoiceVtbl FAR* lpVtbl;
} IXAudio2MasteringVoice;
typedef const struct IXAudio2MasteringVoiceVtbl IXAudio2MasteringVoiceVtbl;
const struct IXAudio2MasteringVoiceVtbl
{
    /* MSDN says that IXAudio2Voice inherits from IXAudio2, but MSVC's debugger
     * says otherwise, and that IXAudio2Voice doesn't inherit from any other
     * interface!
     */

    /* IXAudio2Voice */
    STDMETHOD_(VOID, GetVoiceDetails)(THIS, XAUDIO2_VOICE_DETAILS *pVoiceDetails) PURE;
    STDMETHOD_(HRESULT, SetOutputVoices)(THIS, const XAUDIO2_VOICE_SENDS *pSendList) PURE;
    STDMETHOD_(HRESULT, SetEffectChain)(THIS, const XAUDIO2_EFFECT_CHAIN *pEffectChain) PURE;
    STDMETHOD_(HRESULT, EnableEffect)(THIS, UINT32 EffectIndex, UINT32 OperationSet) PURE;
    STDMETHOD_(HRESULT, DisableEffect)(THIS, UINT32 EffectIndex, UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetEffectState)(THIS, UINT32 EffectIndex, BOOL *pEnabled) PURE;
    STDMETHOD_(HRESULT, SetEffectParameters)(THIS, UINT32 EffectIndex,
                                             const void *pParameters,
                                             UINT32 ParametersByteSize,
                                             UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetEffectParameters)(THIS, UINT32 EffectIndex,
                                          void *pParameters,
                                          UINT32 ParametersByteSize) PURE;
    STDMETHOD_(HRESULT, SetFilterParameters)(THIS, const XAUDIO2_FILTER_PARAMETERS *pParameters,
                                             UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetFilterParameters)(THIS, XAUDIO2_FILTER_PARAMETERS *pParameters) PURE;
    STDMETHOD_(HRESULT, SetOutputFilterParameters)(THIS, IXAudio2Voice *pDestinationVoice,
                                                   XAUDIO2_FILTER_PARAMETERS *pParameters,
                                                   UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetOutputFilterParameters)(THIS, IXAudio2Voice *pDestinationVoice,
                                                XAUDIO2_FILTER_PARAMETERS *pParameters) PURE;
    STDMETHOD_(HRESULT, SetVolume)(THIS, float Volume,
                                   UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetVolume)(THIS, float *pVolume) PURE;
    STDMETHOD_(HRESULT, SetChannelVolumes)(THIS, UINT32 Channels,
                                           const float *pVolumes,
                                           UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetChannelVolumes)(THIS, UINT32 Channels,
                                        float *pVolumes) PURE;
    STDMETHOD_(HRESULT, SetOutputMatrix)(THIS, IXAudio2Voice *pDestinationVoice,
                                         UINT32 SourceChannels,
                                         UINT32 DestinationChannels,
                                         const float *pLevelMatrix,
                                         UINT32 OperationSet) PURE;
    STDMETHOD_(VOID, GetOutputMatrix)(THIS, IXAudio2Voice *pDestinationVoice,
                                      UINT32 SourceChannels,
                                      UINT32 DestinationChannels,
                                      float *pLevelMatrix) PURE;
    STDMETHOD_(VOID, DestroyVoice)(THIS) PURE;

    /* IXAudio2SourceVoice */
    STDMETHOD_(VOID, GetChannelMask)(THIS, DWORD *pChannelMask) PURE;
};

#define IXAudio2MasteringVoice_DestroyVoice(A) ((A)->lpVtbl->DestroyVoice(A))


#undef INTERFACE
#define INTERFACE IXAudio2VoiceCallback
typedef interface IXAudio2VoiceCallback {
    const struct IXAudio2VoiceCallbackVtbl FAR* lpVtbl;
} IXAudio2VoiceCallback;
typedef const struct IXAudio2VoiceCallbackVtbl IXAudio2VoiceCallbackVtbl;
const struct IXAudio2VoiceCallbackVtbl
{
    /* MSDN says that IXAudio2VoiceCallback inherits from IXAudio2, but SDL's
     * own code says otherwise, and that IXAudio2VoiceCallback doesn't inherit
     * from any other interface!
     */

    /* IXAudio2VoiceCallback */
    STDMETHOD_(VOID, OnVoiceProcessingPassStart)(THIS, UINT32 BytesRequired) PURE;
    STDMETHOD_(VOID, OnVoiceProcessingPassEnd)(THIS) PURE;
    STDMETHOD_(VOID, OnStreamEnd)(THIS) PURE;
    STDMETHOD_(VOID, OnBufferStart)(THIS, void *pBufferContext) PURE;
    STDMETHOD_(VOID, OnBufferEnd)(THIS, void *pBufferContext) PURE;
    STDMETHOD_(VOID, OnLoopEnd)(THIS, void *pBufferContext) PURE;
    STDMETHOD_(VOID, OnVoiceError)(THIS, void *pBufferContext, HRESULT Error) PURE;
};

#pragma pack(pop)   /* Undo pragma push */

#endif  /* _SDL_XAUDIO2_H */

/* vi: set ts=4 sw=4 expandtab: */
