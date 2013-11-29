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
#include "SDL_config.h"

#if SDL_AUDIO_DRIVER_DSOUND

/* Allow access to a raw mixing buffer */

#include "SDL_timer.h"
#include "SDL_loadso.h"
#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_directsound.h"

#ifndef WAVE_FORMAT_IEEE_FLOAT
#define WAVE_FORMAT_IEEE_FLOAT 0x0003
#endif

/* DirectX function pointers for audio */
static void* DSoundDLL = NULL;
typedef HRESULT(WINAPI*fnDirectSoundCreate8)(LPGUID,LPDIRECTSOUND*,LPUNKNOWN);
typedef HRESULT(WINAPI*fnDirectSoundEnumerateW)(LPDSENUMCALLBACKW, LPVOID);
typedef HRESULT(WINAPI*fnDirectSoundCaptureEnumerateW)(LPDSENUMCALLBACKW,LPVOID);
static fnDirectSoundCreate8 pDirectSoundCreate8 = NULL;
static fnDirectSoundEnumerateW pDirectSoundEnumerateW = NULL;
static fnDirectSoundCaptureEnumerateW pDirectSoundCaptureEnumerateW = NULL;

static void
DSOUND_Unload(void)
{
    pDirectSoundCreate8 = NULL;
    pDirectSoundEnumerateW = NULL;
    pDirectSoundCaptureEnumerateW = NULL;

    if (DSoundDLL != NULL) {
        SDL_UnloadObject(DSoundDLL);
        DSoundDLL = NULL;
    }
}


static int
DSOUND_Load(void)
{
    int loaded = 0;

    DSOUND_Unload();

    DSoundDLL = SDL_LoadObject("DSOUND.DLL");
    if (DSoundDLL == NULL) {
        SDL_SetError("DirectSound: failed to load DSOUND.DLL");
    } else {
        /* Now make sure we have DirectX 8 or better... */
        #define DSOUNDLOAD(f) { \
            p##f = (fn##f) SDL_LoadFunction(DSoundDLL, #f); \
            if (!p##f) loaded = 0; \
        }
        loaded = 1;  /* will reset if necessary. */
        DSOUNDLOAD(DirectSoundCreate8);
        DSOUNDLOAD(DirectSoundEnumerateW);
        DSOUNDLOAD(DirectSoundCaptureEnumerateW);
        #undef DSOUNDLOAD

        if (!loaded) {
            SDL_SetError("DirectSound: System doesn't appear to have DX8.");
        }
    }

    if (!loaded) {
        DSOUND_Unload();
    }

    return loaded;
}

static int
SetDSerror(const char *function, int code)
{
    static const char *error;
    static char errbuf[1024];

    errbuf[0] = 0;
    switch (code) {
    case E_NOINTERFACE:
        error = "Unsupported interface -- Is DirectX 8.0 or later installed?";
        break;
    case DSERR_ALLOCATED:
        error = "Audio device in use";
        break;
    case DSERR_BADFORMAT:
        error = "Unsupported audio format";
        break;
    case DSERR_BUFFERLOST:
        error = "Mixing buffer was lost";
        break;
    case DSERR_CONTROLUNAVAIL:
        error = "Control requested is not available";
        break;
    case DSERR_INVALIDCALL:
        error = "Invalid call for the current state";
        break;
    case DSERR_INVALIDPARAM:
        error = "Invalid parameter";
        break;
    case DSERR_NODRIVER:
        error = "No audio device found";
        break;
    case DSERR_OUTOFMEMORY:
        error = "Out of memory";
        break;
    case DSERR_PRIOLEVELNEEDED:
        error = "Caller doesn't have priority";
        break;
    case DSERR_UNSUPPORTED:
        error = "Function not supported";
        break;
    default:
        SDL_snprintf(errbuf, SDL_arraysize(errbuf),
                     "%s: Unknown DirectSound error: 0x%x", function, code);
        break;
    }
    if (!errbuf[0]) {
        SDL_snprintf(errbuf, SDL_arraysize(errbuf), "%s: %s", function,
                     error);
    }
    return SDL_SetError("%s", errbuf);
}


static BOOL CALLBACK
FindAllDevs(LPGUID guid, LPCWSTR desc, LPCWSTR module, LPVOID data)
{
    SDL_AddAudioDevice addfn = (SDL_AddAudioDevice) data;
    if (guid != NULL) {  /* skip default device */
        char *str = WIN_StringToUTF8(desc);
        if (str != NULL) {
            addfn(str);
            SDL_free(str);  /* addfn() makes a copy of this string. */
        }
    }
    return TRUE;  /* keep enumerating. */
}

static void
DSOUND_DetectDevices(int iscapture, SDL_AddAudioDevice addfn)
{
    if (iscapture) {
        pDirectSoundCaptureEnumerateW(FindAllDevs, addfn);
    } else {
        pDirectSoundEnumerateW(FindAllDevs, addfn);
    }
}


static void
DSOUND_WaitDevice(_THIS)
{
    DWORD status = 0;
    DWORD cursor = 0;
    DWORD junk = 0;
    HRESULT result = DS_OK;

    /* Semi-busy wait, since we have no way of getting play notification
       on a primary mixing buffer located in hardware (DirectX 5.0)
     */
    result = IDirectSoundBuffer_GetCurrentPosition(this->hidden->mixbuf,
                                                   &junk, &cursor);
    if (result != DS_OK) {
        if (result == DSERR_BUFFERLOST) {
            IDirectSoundBuffer_Restore(this->hidden->mixbuf);
        }
#ifdef DEBUG_SOUND
        SetDSerror("DirectSound GetCurrentPosition", result);
#endif
        return;
    }

    while ((cursor / this->hidden->mixlen) == this->hidden->lastchunk) {
        /* FIXME: find out how much time is left and sleep that long */
        SDL_Delay(1);

        /* Try to restore a lost sound buffer */
        IDirectSoundBuffer_GetStatus(this->hidden->mixbuf, &status);
        if ((status & DSBSTATUS_BUFFERLOST)) {
            IDirectSoundBuffer_Restore(this->hidden->mixbuf);
            IDirectSoundBuffer_GetStatus(this->hidden->mixbuf, &status);
            if ((status & DSBSTATUS_BUFFERLOST)) {
                break;
            }
        }
        if (!(status & DSBSTATUS_PLAYING)) {
            result = IDirectSoundBuffer_Play(this->hidden->mixbuf, 0, 0,
                                             DSBPLAY_LOOPING);
            if (result == DS_OK) {
                continue;
            }
#ifdef DEBUG_SOUND
            SetDSerror("DirectSound Play", result);
#endif
            return;
        }

        /* Find out where we are playing */
        result = IDirectSoundBuffer_GetCurrentPosition(this->hidden->mixbuf,
                                                       &junk, &cursor);
        if (result != DS_OK) {
            SetDSerror("DirectSound GetCurrentPosition", result);
            return;
        }
    }
}

static void
DSOUND_PlayDevice(_THIS)
{
    /* Unlock the buffer, allowing it to play */
    if (this->hidden->locked_buf) {
        IDirectSoundBuffer_Unlock(this->hidden->mixbuf,
                                  this->hidden->locked_buf,
                                  this->hidden->mixlen, NULL, 0);
    }

}

static Uint8 *
DSOUND_GetDeviceBuf(_THIS)
{
    DWORD cursor = 0;
    DWORD junk = 0;
    HRESULT result = DS_OK;
    DWORD rawlen = 0;

    /* Figure out which blocks to fill next */
    this->hidden->locked_buf = NULL;
    result = IDirectSoundBuffer_GetCurrentPosition(this->hidden->mixbuf,
                                                   &junk, &cursor);
    if (result == DSERR_BUFFERLOST) {
        IDirectSoundBuffer_Restore(this->hidden->mixbuf);
        result = IDirectSoundBuffer_GetCurrentPosition(this->hidden->mixbuf,
                                                       &junk, &cursor);
    }
    if (result != DS_OK) {
        SetDSerror("DirectSound GetCurrentPosition", result);
        return (NULL);
    }
    cursor /= this->hidden->mixlen;
#ifdef DEBUG_SOUND
    /* Detect audio dropouts */
    {
        DWORD spot = cursor;
        if (spot < this->hidden->lastchunk) {
            spot += this->hidden->num_buffers;
        }
        if (spot > this->hidden->lastchunk + 1) {
            fprintf(stderr, "Audio dropout, missed %d fragments\n",
                    (spot - (this->hidden->lastchunk + 1)));
        }
    }
#endif
    this->hidden->lastchunk = cursor;
    cursor = (cursor + 1) % this->hidden->num_buffers;
    cursor *= this->hidden->mixlen;

    /* Lock the audio buffer */
    result = IDirectSoundBuffer_Lock(this->hidden->mixbuf, cursor,
                                     this->hidden->mixlen,
                                     (LPVOID *) & this->hidden->locked_buf,
                                     &rawlen, NULL, &junk, 0);
    if (result == DSERR_BUFFERLOST) {
        IDirectSoundBuffer_Restore(this->hidden->mixbuf);
        result = IDirectSoundBuffer_Lock(this->hidden->mixbuf, cursor,
                                         this->hidden->mixlen,
                                         (LPVOID *) & this->
                                         hidden->locked_buf, &rawlen, NULL,
                                         &junk, 0);
    }
    if (result != DS_OK) {
        SetDSerror("DirectSound Lock", result);
        return (NULL);
    }
    return (this->hidden->locked_buf);
}

static void
DSOUND_WaitDone(_THIS)
{
    Uint8 *stream = DSOUND_GetDeviceBuf(this);

    /* Wait for the playing chunk to finish */
    if (stream != NULL) {
        SDL_memset(stream, this->spec.silence, this->hidden->mixlen);
        DSOUND_PlayDevice(this);
    }
    DSOUND_WaitDevice(this);

    /* Stop the looping sound buffer */
    IDirectSoundBuffer_Stop(this->hidden->mixbuf);
}

static void
DSOUND_CloseDevice(_THIS)
{
    if (this->hidden != NULL) {
        if (this->hidden->sound != NULL) {
            if (this->hidden->mixbuf != NULL) {
                /* Clean up the audio buffer */
                IDirectSoundBuffer_Release(this->hidden->mixbuf);
                this->hidden->mixbuf = NULL;
            }
            IDirectSound_Release(this->hidden->sound);
            this->hidden->sound = NULL;
        }

        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}

/* This function tries to create a secondary audio buffer, and returns the
   number of audio chunks available in the created buffer.
*/
static int
CreateSecondary(_THIS, HWND focus)
{
    LPDIRECTSOUND sndObj = this->hidden->sound;
    LPDIRECTSOUNDBUFFER *sndbuf = &this->hidden->mixbuf;
    Uint32 chunksize = this->spec.size;
    const int numchunks = 8;
    HRESULT result = DS_OK;
    DSBUFFERDESC format;
    LPVOID pvAudioPtr1, pvAudioPtr2;
    DWORD dwAudioBytes1, dwAudioBytes2;
    WAVEFORMATEX wfmt;

    SDL_zero(wfmt);

    if (SDL_AUDIO_ISFLOAT(this->spec.format)) {
        wfmt.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    } else {
        wfmt.wFormatTag = WAVE_FORMAT_PCM;
    }

    wfmt.wBitsPerSample = SDL_AUDIO_BITSIZE(this->spec.format);
    wfmt.nChannels = this->spec.channels;
    wfmt.nSamplesPerSec = this->spec.freq;
    wfmt.nBlockAlign = wfmt.nChannels * (wfmt.wBitsPerSample / 8);
    wfmt.nAvgBytesPerSec = wfmt.nSamplesPerSec * wfmt.nBlockAlign;

    /* Update the fragment size as size in bytes */
    SDL_CalculateAudioSpec(&this->spec);

    /* Try to set primary mixing privileges */
    if (focus) {
        result = IDirectSound_SetCooperativeLevel(sndObj,
                                                  focus, DSSCL_PRIORITY);
    } else {
        result = IDirectSound_SetCooperativeLevel(sndObj,
                                                  GetDesktopWindow(),
                                                  DSSCL_NORMAL);
    }
    if (result != DS_OK) {
        return SetDSerror("DirectSound SetCooperativeLevel", result);
    }

    /* Try to create the secondary buffer */
    SDL_zero(format);
    format.dwSize = sizeof(format);
    format.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
    if (!focus) {
        format.dwFlags |= DSBCAPS_GLOBALFOCUS;
    } else {
        format.dwFlags |= DSBCAPS_STICKYFOCUS;
    }
    format.dwBufferBytes = numchunks * chunksize;
    if ((format.dwBufferBytes < DSBSIZE_MIN) ||
        (format.dwBufferBytes > DSBSIZE_MAX)) {
        return SDL_SetError("Sound buffer size must be between %d and %d",
                            DSBSIZE_MIN / numchunks, DSBSIZE_MAX / numchunks);
    }
    format.dwReserved = 0;
    format.lpwfxFormat = &wfmt;
    result = IDirectSound_CreateSoundBuffer(sndObj, &format, sndbuf, NULL);
    if (result != DS_OK) {
        return SetDSerror("DirectSound CreateSoundBuffer", result);
    }
    IDirectSoundBuffer_SetFormat(*sndbuf, &wfmt);

    /* Silence the initial audio buffer */
    result = IDirectSoundBuffer_Lock(*sndbuf, 0, format.dwBufferBytes,
                                     (LPVOID *) & pvAudioPtr1, &dwAudioBytes1,
                                     (LPVOID *) & pvAudioPtr2, &dwAudioBytes2,
                                     DSBLOCK_ENTIREBUFFER);
    if (result == DS_OK) {
        SDL_memset(pvAudioPtr1, this->spec.silence, dwAudioBytes1);
        IDirectSoundBuffer_Unlock(*sndbuf,
                                  (LPVOID) pvAudioPtr1, dwAudioBytes1,
                                  (LPVOID) pvAudioPtr2, dwAudioBytes2);
    }

    /* We're ready to go */
    return (numchunks);
}

typedef struct FindDevGUIDData
{
    const char *devname;
    GUID guid;
    int found;
} FindDevGUIDData;

static BOOL CALLBACK
FindDevGUID(LPGUID guid, LPCWSTR desc, LPCWSTR module, LPVOID _data)
{
    if (guid != NULL) {  /* skip the default device. */
        FindDevGUIDData *data = (FindDevGUIDData *) _data;
        char *str = WIN_StringToUTF8(desc);
        const int match = (SDL_strcmp(str, data->devname) == 0);
        SDL_free(str);
        if (match) {
            data->found = 1;
            SDL_memcpy(&data->guid, guid, sizeof (data->guid));
            return FALSE;  /* found it! stop enumerating. */
        }
    }
    return TRUE;  /* keep enumerating. */
}

static int
DSOUND_OpenDevice(_THIS, const char *devname, int iscapture)
{
    HRESULT result;
    SDL_bool valid_format = SDL_FALSE;
    SDL_bool tried_format = SDL_FALSE;
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(this->spec.format);
    FindDevGUIDData devguid;
    LPGUID guid = NULL;

    if (devname != NULL) {
        devguid.found = 0;
        devguid.devname = devname;
        if (iscapture)
            pDirectSoundCaptureEnumerateW(FindDevGUID, &devguid);
        else
            pDirectSoundEnumerateW(FindDevGUID, &devguid);

        if (!devguid.found) {
            return SDL_SetError("DirectSound: Requested device not found");
        }
        guid = &devguid.guid;
    }

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));

    /* Open the audio device */
    result = pDirectSoundCreate8(guid, &this->hidden->sound, NULL);
    if (result != DS_OK) {
        DSOUND_CloseDevice(this);
        return SetDSerror("DirectSoundCreate", result);
    }

    while ((!valid_format) && (test_format)) {
        switch (test_format) {
        case AUDIO_U8:
        case AUDIO_S16:
        case AUDIO_S32:
        case AUDIO_F32:
            tried_format = SDL_TRUE;
            this->spec.format = test_format;
            this->hidden->num_buffers = CreateSecondary(this, NULL);
            if (this->hidden->num_buffers > 0) {
                valid_format = SDL_TRUE;
            }
            break;
        }
        test_format = SDL_NextAudioFormat();
    }

    if (!valid_format) {
        DSOUND_CloseDevice(this);
        if (tried_format) {
            return -1;  /* CreateSecondary() should have called SDL_SetError(). */
        }
        return SDL_SetError("DirectSound: Unsupported audio format");
    }

    /* The buffer will auto-start playing in DSOUND_WaitDevice() */
    this->hidden->mixlen = this->spec.size;

    return 0;                   /* good to go. */
}


static void
DSOUND_Deinitialize(void)
{
    DSOUND_Unload();
}


static int
DSOUND_Init(SDL_AudioDriverImpl * impl)
{
    if (!DSOUND_Load()) {
        return 0;
    }

    /* Set the function pointers */
    impl->DetectDevices = DSOUND_DetectDevices;
    impl->OpenDevice = DSOUND_OpenDevice;
    impl->PlayDevice = DSOUND_PlayDevice;
    impl->WaitDevice = DSOUND_WaitDevice;
    impl->WaitDone = DSOUND_WaitDone;
    impl->GetDeviceBuf = DSOUND_GetDeviceBuf;
    impl->CloseDevice = DSOUND_CloseDevice;
    impl->Deinitialize = DSOUND_Deinitialize;

    return 1;   /* this audio target is available. */
}

AudioBootStrap DSOUND_bootstrap = {
    "directsound", "DirectSound", DSOUND_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_DSOUND */

/* vi: set ts=4 sw=4 expandtab: */
