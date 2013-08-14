/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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

/* WinRT NOTICE:

   A number of changes were warranted to SDL's XAudio2 backend in order to
   get it compiling for Windows RT.

   When compiling for WinRT, XAudio2.h requires that it be compiled in a C++
   file, and not a straight C file.  Trying to compile it as C leads to lots
   of errors, at least with MSVC 2012 and Windows SDK 8.0, as of Nov 22, 2012.
   To address this specific issue, a few changes were made to SDL_xaudio2.c:
   
   1. SDL_xaudio2.c is compiled as a C++ file in WinRT builds.  Exported
      symbols, namely XAUDIO2_bootstrap, uses 'extern "C"' to make sure the
      rest of SDL can access it.  Non-WinRT builds continue to compile
      SDL_xaudio2.c as a C file.
   2. A macro redefines variables named 'this' to '_this', to prevent compiler
      errors (C2355 in Visual C++) related to 'this' being a reserverd keyword.
      This hack may need to be altered in the future, particularly if C++'s
      'this' keyword needs to be used (within SDL_xaudio2.c).  At the time
      WinRT support was initially added to SDL's XAudio2 backend, this
      capability was not needed.
   3. The C-style macros to invoke XAudio2's COM-based methods were
      rewritten to be C++-friendly.  These are provided in the file,
      SDL_xaudio2_winrthelpers.h.
   4. IXAudio2::CreateSourceVoice, when used in C++, requires its callbacks to
      be specified via a C++ class.  SDL's XAudio2 backend was written with
      C-style callbacks.  A class to bridge these two interfaces,
      SDL_XAudio2VoiceCallback, was written to make XAudio2 happy.  Its methods
      just call SDL's existing, C-style callbacks.
   5. Multiple checks for the __cplusplus macro were made, in appropriate
      places.  


   A few additional changes to SDL's XAudio2 backend were warranted by API
   changes to Windows.  Many, but not all of these are documented by Microsoft
   at:
   http://blogs.msdn.com/b/chuckw/archive/2012/04/02/xaudio2-and-windows-8-consumer-preview.aspx

   1. Windows' thread synchronization function, CreateSemaphore, was removed
      from Windows RT.  SDL's semaphore API was substituted instead.
   2. The method calls, IXAudio2::GetDeviceCount and IXAudio2::GetDeviceDetails
      were removed from the XAudio2 API.  Microsoft is telling developers to
      use APIs in Windows::Foundation instead.
      For SDL, the missing methods were reimplemented using the APIs Microsoft
      said to use.
   3. CoInitialize and CoUninitialize are not available in Windows RT.
      These calls were removed, as COM will have been initialized earlier,
      at least by the call to the WinRT app's main function
      (aka 'int main(Platform::Array<Platform::String^>^)).  (DLudwig:
      This was my understanding of how WinRT: the 'main' function uses
      a tag of [MTAThread], which should initialize COM.  My understanding
      of COM is somewhat limited, and I may be incorrect here.)
   4. IXAudio2::CreateMasteringVoice changed its integer-based 'DeviceIndex'
      argument to a string-based one, 'szDeviceId'.  In Windows RT, the
      string-based argument will be used.
*/

#include "SDL_config.h"

#if SDL_AUDIO_DRIVER_XAUDIO2

#ifdef __cplusplus
extern "C" {
#endif
#include "../../core/windows/SDL_windows.h"
#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_assert.h"
#ifdef __cplusplus
}
#endif

#ifdef __GNUC__
/* The configure script already did any necessary checking */
#  define SDL_XAUDIO2_HAS_SDK 1
#elif defined(__WINRT__)
/* WinRT always has access to the .the XAudio 2 SDK */
#  define SDL_XAUDIO2_HAS_SDK
#else
#include <dxsdkver.h> /* XAudio2 exists as of the March 2008 DirectX SDK */
#if (!defined(_DXSDK_BUILD_MAJOR) || (_DXSDK_BUILD_MAJOR < 1284))
#  pragma message("Your DirectX SDK is too old. Disabling XAudio2 support.")
#else
#  define SDL_XAUDIO2_HAS_SDK 1
#endif
#endif

#ifdef SDL_XAUDIO2_HAS_SDK

#define INITGUID 1
#include <xaudio2.h>

/* Hidden "this" pointer for the audio functions */
#define _THIS   SDL_AudioDevice *this

#ifdef __cplusplus
#define this _this
#include "SDL_xaudio2_winrthelpers.h"
#endif

struct SDL_PrivateAudioData
{
    IXAudio2 *ixa2;
    IXAudio2SourceVoice *source;
    IXAudio2MasteringVoice *mastering;
    SDL_sem * semaphore;
    Uint8 *mixbuf;
    int mixlen;
    Uint8 *nextbuf;
};


static void
XAUDIO2_DetectDevices(int iscapture, SDL_AddAudioDevice addfn)
{
    IXAudio2 *ixa2 = NULL;
    UINT32 devcount = 0;
    UINT32 i = 0;

    if (iscapture) {
        SDL_SetError("XAudio2: capture devices unsupported.");
        return;
    } else if (XAudio2Create(&ixa2, 0, XAUDIO2_DEFAULT_PROCESSOR) != S_OK) {
        SDL_SetError("XAudio2: XAudio2Create() failed at detection.");
        return;
    } else if (IXAudio2_GetDeviceCount(ixa2, &devcount) != S_OK) {
        SDL_SetError("XAudio2: IXAudio2::GetDeviceCount() failed.");
        IXAudio2_Release(ixa2);
        return;
    }

    for (i = 0; i < devcount; i++) {
        XAUDIO2_DEVICE_DETAILS details;
        if (IXAudio2_GetDeviceDetails(ixa2, i, &details) == S_OK) {
            char *str = WIN_StringToUTF8(details.DisplayName);
            if (str != NULL) {
                addfn(str);
                SDL_free(str);  /* addfn() made a copy of the string. */
            }
        }
    }

    IXAudio2_Release(ixa2);
}

static void STDMETHODCALLTYPE
VoiceCBOnBufferEnd(THIS_ void *data)
{
    /* Just signal the SDL audio thread and get out of XAudio2's way. */
    SDL_AudioDevice *this = (SDL_AudioDevice *) data;
    SDL_SemPost(this->hidden->semaphore);
}

static void STDMETHODCALLTYPE
VoiceCBOnVoiceError(THIS_ void *data, HRESULT Error)
{
    /* !!! FIXME: attempt to recover, or mark device disconnected. */
    SDL_assert(0 && "write me!");
}

/* no-op callbacks... */
static void STDMETHODCALLTYPE VoiceCBOnStreamEnd(THIS) {}
static void STDMETHODCALLTYPE VoiceCBOnVoiceProcessPassStart(THIS_ UINT32 b) {}
static void STDMETHODCALLTYPE VoiceCBOnVoiceProcessPassEnd(THIS) {}
static void STDMETHODCALLTYPE VoiceCBOnBufferStart(THIS_ void *data) {}
static void STDMETHODCALLTYPE VoiceCBOnLoopEnd(THIS_ void *data) {}

#if defined(__cplusplus)
class SDL_XAudio2VoiceCallback : public IXAudio2VoiceCallback
{
public:
    STDMETHOD_(void, OnBufferEnd)(void *pBufferContext) {
        VoiceCBOnBufferEnd(pBufferContext);
    }
    STDMETHOD_(void, OnBufferStart)(void *pBufferContext) {
        VoiceCBOnBufferStart(pBufferContext);
    }
    STDMETHOD_(void, OnLoopEnd)(void *pBufferContext) {
        VoiceCBOnLoopEnd(pBufferContext);
    }
    STDMETHOD_(void, OnStreamEnd)() {
        VoiceCBOnStreamEnd();
    }
    STDMETHOD_(void, OnVoiceError)(void *pBufferContext, HRESULT Error) {
        VoiceCBOnVoiceError(pBufferContext, Error);
    }
    STDMETHOD_(void, OnVoiceProcessingPassEnd)() {
        VoiceCBOnVoiceProcessPassEnd();
    }
    STDMETHOD_(void, OnVoiceProcessingPassStart)(UINT32 BytesRequired) {
        VoiceCBOnVoiceProcessPassStart(BytesRequired);
    }
};
#endif

static Uint8 *
XAUDIO2_GetDeviceBuf(_THIS)
{
    return this->hidden->nextbuf;
}

static void
XAUDIO2_PlayDevice(_THIS)
{
    XAUDIO2_BUFFER buffer;
    Uint8 *mixbuf = this->hidden->mixbuf;
    Uint8 *nextbuf = this->hidden->nextbuf;
    const int mixlen = this->hidden->mixlen;
    IXAudio2SourceVoice *source = this->hidden->source;
    HRESULT result = S_OK;

    if (!this->enabled) { /* shutting down? */
        return;
    }

    /* Submit the next filled buffer */
    SDL_zero(buffer);
    buffer.AudioBytes = mixlen;
    buffer.pAudioData = nextbuf;
    buffer.pContext = this;

    if (nextbuf == mixbuf) {
        nextbuf += mixlen;
    } else {
        nextbuf = mixbuf;
    }
    this->hidden->nextbuf = nextbuf;

    result = IXAudio2SourceVoice_SubmitSourceBuffer(source, &buffer, NULL);
    if (result == XAUDIO2_E_DEVICE_INVALIDATED) {
        /* !!! FIXME: possibly disconnected or temporary lost. Recover? */
    }

    if (result != S_OK) {  /* uhoh, panic! */
        IXAudio2SourceVoice_FlushSourceBuffers(source);
        this->enabled = 0;
    }
}

static void
XAUDIO2_WaitDevice(_THIS)
{
    if (this->enabled) {
        SDL_SemWait(this->hidden->semaphore);
    }
}

static void
XAUDIO2_WaitDone(_THIS)
{
    IXAudio2SourceVoice *source = this->hidden->source;
    XAUDIO2_VOICE_STATE state;
    SDL_assert(!this->enabled);  /* flag that stops playing. */
    IXAudio2SourceVoice_Discontinuity(source);
    IXAudio2SourceVoice_GetState(source, &state);
    while (state.BuffersQueued > 0) {
        SDL_SemWait(this->hidden->semaphore);
        IXAudio2SourceVoice_GetState(source, &state);
    }
}


static void
XAUDIO2_CloseDevice(_THIS)
{
    if (this->hidden != NULL) {
        IXAudio2 *ixa2 = this->hidden->ixa2;
        IXAudio2SourceVoice *source = this->hidden->source;
        IXAudio2MasteringVoice *mastering = this->hidden->mastering;

        if (source != NULL) {
            IXAudio2SourceVoice_Stop(source, 0, XAUDIO2_COMMIT_NOW);
            IXAudio2SourceVoice_FlushSourceBuffers(source);
            IXAudio2SourceVoice_DestroyVoice(source);
        }
        if (ixa2 != NULL) {
            IXAudio2_StopEngine(ixa2);
        }
        if (mastering != NULL) {
            IXAudio2MasteringVoice_DestroyVoice(mastering);
        }
        if (ixa2 != NULL) {
            IXAudio2_Release(ixa2);
        }
        if (this->hidden->mixbuf != NULL) {
            SDL_free(this->hidden->mixbuf);
        }
        if (this->hidden->semaphore != NULL) {
            SDL_DestroySemaphore(this->hidden->semaphore);
        }

        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}

static int
XAUDIO2_OpenDevice(_THIS, const char *devname, int iscapture)
{
    HRESULT result = S_OK;
    WAVEFORMATEX waveformat;
    int valid_format = 0;
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(this->spec.format);
    IXAudio2 *ixa2 = NULL;
    IXAudio2SourceVoice *source = NULL;
#if defined(__WINRT__)
    WCHAR devIdBuffer[256];
    LPCWSTR devId = 0;
#else
    UINT32 devId = 0;  /* 0 == system default device. */
#endif

#if defined(__cplusplus)
    static SDL_XAudio2VoiceCallback callbacks;
#else
	static IXAudio2VoiceCallbackVtbl callbacks_vtable = {
	    VoiceCBOnVoiceProcessPassStart,
        VoiceCBOnVoiceProcessPassEnd,
        VoiceCBOnStreamEnd,
        VoiceCBOnBufferStart,
        VoiceCBOnBufferEnd,
        VoiceCBOnLoopEnd,
        VoiceCBOnVoiceError
    };

	static IXAudio2VoiceCallback callbacks = { &callbacks_vtable };
#endif // ! defined(__cplusplus)

#if defined(__WINRT__)
    SDL_zero(devIdBuffer);
#endif

    if (iscapture) {
        return SDL_SetError("XAudio2: capture devices unsupported.");
    } else if (XAudio2Create(&ixa2, 0, XAUDIO2_DEFAULT_PROCESSOR) != S_OK) {
        return SDL_SetError("XAudio2: XAudio2Create() failed at open.");
    }
    /*
    XAUDIO2_DEBUG_CONFIGURATION debugConfig;
    debugConfig.TraceMask = XAUDIO2_LOG_ERRORS; //XAUDIO2_LOG_WARNINGS | XAUDIO2_LOG_DETAIL | XAUDIO2_LOG_FUNC_CALLS | XAUDIO2_LOG_TIMING | XAUDIO2_LOG_LOCKS | XAUDIO2_LOG_MEMORY | XAUDIO2_LOG_STREAMING;
    debugConfig.BreakMask = XAUDIO2_LOG_ERRORS; //XAUDIO2_LOG_WARNINGS;
    debugConfig.LogThreadID = TRUE;
    debugConfig.LogFileline = TRUE;
    debugConfig.LogFunctionName = TRUE;
    debugConfig.LogTiming = TRUE;
    ixa2->SetDebugConfiguration(&debugConfig);
    */

#if ! defined(__WINRT__) || WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
    if (devname != NULL) {
        UINT32 devcount = 0;
        UINT32 i = 0;

        if (IXAudio2_GetDeviceCount(ixa2, &devcount) != S_OK) {
            IXAudio2_Release(ixa2);
            return SDL_SetError("XAudio2: IXAudio2_GetDeviceCount() failed.");
        }
        for (i = 0; i < devcount; i++) {
            XAUDIO2_DEVICE_DETAILS details;
            if (IXAudio2_GetDeviceDetails(ixa2, i, &details) == S_OK) {
                char *str = WIN_StringToUTF8(details.DisplayName);
                if (str != NULL) {
                    const int match = (SDL_strcmp(str, devname) == 0);
                    SDL_free(str);
                    if (match) {
#if defined(__WINRT__)
                        wcsncpy_s(devIdBuffer, ARRAYSIZE(devIdBuffer), details.DeviceID, _TRUNCATE);
                        devId = (LPCWSTR) &devIdBuffer;
#else
                        devId = i;
#endif
                        break;
                    }
                }
            }
        }

        if (i == devcount) {
            IXAudio2_Release(ixa2);
            return SDL_SetError("XAudio2: Requested device not found.");
        }
    }
#endif

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *this->hidden));
    if (this->hidden == NULL) {
        IXAudio2_Release(ixa2);
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));

    this->hidden->ixa2 = ixa2;
    this->hidden->semaphore = SDL_CreateSemaphore(1);
    if (this->hidden->semaphore == NULL) {
        XAUDIO2_CloseDevice(this);
        return SDL_SetError("XAudio2: CreateSemaphore() failed!");
    }

    while ((!valid_format) && (test_format)) {
        switch (test_format) {
        case AUDIO_U8:
        case AUDIO_S16:
        case AUDIO_S32:
        case AUDIO_F32:
            this->spec.format = test_format;
            valid_format = 1;
            break;
        }
        test_format = SDL_NextAudioFormat();
    }

    if (!valid_format) {
        XAUDIO2_CloseDevice(this);
        return SDL_SetError("XAudio2: Unsupported audio format");
    }

    /* Update the fragment size as size in bytes */
    SDL_CalculateAudioSpec(&this->spec);

    /* We feed a Source, it feeds the Mastering, which feeds the device. */
    this->hidden->mixlen = this->spec.size;
    this->hidden->mixbuf = (Uint8 *) SDL_malloc(2 * this->hidden->mixlen);
    if (this->hidden->mixbuf == NULL) {
        XAUDIO2_CloseDevice(this);
        return SDL_OutOfMemory();
    }
    this->hidden->nextbuf = this->hidden->mixbuf;
    SDL_memset(this->hidden->mixbuf, 0, 2 * this->hidden->mixlen);

    /* We use XAUDIO2_DEFAULT_CHANNELS instead of this->spec.channels. On
       Xbox360, this means 5.1 output, but on Windows, it means "figure out
       what the system has." It might be preferable to let XAudio2 blast
       stereo output to appropriate surround sound configurations
       instead of clamping to 2 channels, even though we'll configure the
       Source Voice for whatever number of channels you supply. */
    result = IXAudio2_CreateMasteringVoice(ixa2, &this->hidden->mastering,
                                           XAUDIO2_DEFAULT_CHANNELS,
                                           this->spec.freq, 0, devId, NULL);
    if (result != S_OK) {
        XAUDIO2_CloseDevice(this);
        return SDL_SetError("XAudio2: Couldn't create mastering voice");
    }

    SDL_zero(waveformat);
    if (SDL_AUDIO_ISFLOAT(this->spec.format)) {
        waveformat.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    } else {
        waveformat.wFormatTag = WAVE_FORMAT_PCM;
    }
    waveformat.wBitsPerSample = SDL_AUDIO_BITSIZE(this->spec.format);
    waveformat.nChannels = this->spec.channels;
    waveformat.nSamplesPerSec = this->spec.freq;
    waveformat.nBlockAlign =
        waveformat.nChannels * (waveformat.wBitsPerSample / 8);
    waveformat.nAvgBytesPerSec =
        waveformat.nSamplesPerSec * waveformat.nBlockAlign;
    waveformat.cbSize = sizeof(waveformat);

#ifdef __WINRT__
    // DLudwig: for now, make XAudio2 do sample rate conversion, just to
    // get the loopwave test to work.
    //
    // TODO, WinRT: consider removing WinRT-specific source-voice creation code from SDL_xaudio2.c
    result = IXAudio2_CreateSourceVoice(ixa2, &source, &waveformat,
                                        0,
                                        1.0f, &callbacks, NULL, NULL);
#else
    result = IXAudio2_CreateSourceVoice(ixa2, &source, &waveformat,
                                        XAUDIO2_VOICE_NOSRC |
                                        XAUDIO2_VOICE_NOPITCH,
                                        1.0f, &callbacks, NULL, NULL);

#endif
    if (result != S_OK) {
        XAUDIO2_CloseDevice(this);
        return SDL_SetError("XAudio2: Couldn't create source voice");
    }
    this->hidden->source = source;

    /* Start everything playing! */
    result = IXAudio2_StartEngine(ixa2);
    if (result != S_OK) {
        XAUDIO2_CloseDevice(this);
        return SDL_SetError("XAudio2: Couldn't start engine");
    }

    result = IXAudio2SourceVoice_Start(source, 0, XAUDIO2_COMMIT_NOW);
    if (result != S_OK) {
        XAUDIO2_CloseDevice(this);
        return SDL_SetError("XAudio2: Couldn't start source voice");
    }

    return 0; /* good to go. */
}

static void
XAUDIO2_Deinitialize(void)
{
#if defined(__WIN32__)
    WIN_CoUninitialize();
#endif
}

#endif  /* SDL_XAUDIO2_HAS_SDK */


static int
XAUDIO2_Init(SDL_AudioDriverImpl * impl)
{
#ifndef SDL_XAUDIO2_HAS_SDK
    SDL_SetError("XAudio2: SDL was built without XAudio2 support (old DirectX SDK).");
    return 0;  /* no XAudio2 support, ever. Update your SDK! */
#else
    /* XAudio2Create() is a macro that uses COM; we don't load the .dll */
    IXAudio2 *ixa2 = NULL;
#if defined(__WIN32__)
    // TODO, WinRT: Investigate using CoInitializeEx here
    if (FAILED(WIN_CoInitialize())) {
        SDL_SetError("XAudio2: CoInitialize() failed");
        return 0;
    }
#endif

    if (XAudio2Create(&ixa2, 0, XAUDIO2_DEFAULT_PROCESSOR) != S_OK) {
#if defined(__WIN32__)
        WIN_CoUninitialize();
#endif
        SDL_SetError("XAudio2: XAudio2Create() failed at initialization");
        return 0;  /* not available. */
    }
    IXAudio2_Release(ixa2);

    /* Set the function pointers */
    impl->DetectDevices = XAUDIO2_DetectDevices;
    impl->OpenDevice = XAUDIO2_OpenDevice;
    impl->PlayDevice = XAUDIO2_PlayDevice;
    impl->WaitDevice = XAUDIO2_WaitDevice;
    impl->WaitDone = XAUDIO2_WaitDone;
    impl->GetDeviceBuf = XAUDIO2_GetDeviceBuf;
    impl->CloseDevice = XAUDIO2_CloseDevice;
    impl->Deinitialize = XAUDIO2_Deinitialize;

    return 1;   /* this audio target is available. */
#endif
}

#if defined(__cplusplus)
extern "C"
#endif
AudioBootStrap XAUDIO2_bootstrap = {
    "xaudio2", "XAudio2", XAUDIO2_Init, 0
};

#endif  /* SDL_AUDIO_DRIVER_XAUDIO2 */

/* vi: set ts=4 sw=4 expandtab: */
