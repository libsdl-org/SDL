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

#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_WASAPI

#include "../../core/windows/SDL_windows.h"
#include "SDL_audio.h"
#include "SDL_timer.h"
#include "../SDL_audio_c.h"
#include "../SDL_sysaudio.h"
#include "SDL_assert.h"
#include "SDL_log.h"

#define COBJMACROS
#include <mmdeviceapi.h>
#include <audioclient.h>

#include "SDL_wasapi.h"

static const ERole SDL_WASAPI_role = eConsole;  /* !!! FIXME: should this be eMultimedia? Should be a hint? */

/* This is global to the WASAPI target, to handle hotplug and default device lookup. */
static IMMDeviceEnumerator *enumerator = NULL;

/* This is a list of device id strings we have inflight, so we have consistent pointers to the same device. */
typedef struct DevIdList
{
	WCHAR *str;
	struct DevIdList *next;
} DevIdList;

static DevIdList *deviceid_list = NULL;

/* handle to Avrt.dll--Vista and later!--for flagging the callback thread as "Pro Audio" (low latency). */
#ifndef __WINRT__
static HMODULE libavrt = NULL;
#endif
typedef HANDLE (WINAPI *pfnAvSetMmThreadCharacteristicsW)(LPWSTR,LPDWORD);
typedef BOOL (WINAPI *pfnAvRevertMmThreadCharacteristics)(HANDLE);
static pfnAvSetMmThreadCharacteristicsW pAvSetMmThreadCharacteristicsW = NULL;
static pfnAvRevertMmThreadCharacteristics pAvRevertMmThreadCharacteristics = NULL;

/* Some GUIDs we need to know without linking to libraries that aren't available before Vista. */
static const CLSID SDL_CLSID_MMDeviceEnumerator = { 0xbcde0395, 0xe52f, 0x467c, { 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e } };
static const IID SDL_IID_IMMDeviceEnumerator = { 0xa95664d2, 0x9614, 0x4f35, { 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6 } };
static const IID SDL_IID_IMMNotificationClient = { 0x7991eec9, 0x7e89, 0x4d85, { 0x83, 0x90, 0x6c, 0x70, 0x3c, 0xec, 0x60, 0xc0 } };
static const IID SDL_IID_IMMEndpoint = { 0x1be09788, 0x6894, 0x4089, { 0x85, 0x86, 0x9a, 0x2a, 0x6c, 0x26, 0x5a, 0xc5 } };
static const IID SDL_IID_IAudioClient = { 0x1cb9ad4c, 0xdbfa, 0x4c32, { 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2 } };
static const IID SDL_IID_IAudioRenderClient = { 0xf294acfc, 0x3146, 0x4483, { 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2 } };
static const IID SDL_IID_IAudioCaptureClient = { 0xc8adbd64, 0xe71e, 0x48a0, { 0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17 } };
static const GUID SDL_KSDATAFORMAT_SUBTYPE_PCM = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID SDL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const PROPERTYKEY SDL_PKEY_Device_FriendlyName = { { 0xa45c254e, 0xdf1c, 0x4efd, { 0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0, } }, 14 };

/* PropVariantInit() is an inline function/macro in PropIdl.h that calls the C runtime's memset() directly. Use ours instead, to avoid dependency. */ 
#ifdef PropVariantInit
#undef PropVariantInit
#endif
#define PropVariantInit(p) SDL_zerop(p)

static void AddWASAPIDevice(const SDL_bool iscapture, IMMDevice *device, LPCWSTR devid);
static void RemoveWASAPIDevice(const SDL_bool iscapture, LPCWSTR devid);

/* We need a COM subclass of IMMNotificationClient for hotplug support, which is
   easy in C++, but we have to tapdance more to make work in C.
   Thanks to this page for coaching on how to make this work:
     https://www.codeproject.com/Articles/13601/COM-in-plain-C */

typedef struct SDLMMNotificationClient
{
    const IMMNotificationClientVtbl *lpVtbl;
    SDL_atomic_t refcount;
} SDLMMNotificationClient;

static HRESULT STDMETHODCALLTYPE
SDLMMNotificationClient_QueryInterface(IMMNotificationClient *this, REFIID iid, void **ppv)
{
    if ((WIN_IsEqualIID(iid, &IID_IUnknown)) || (WIN_IsEqualIID(iid, &SDL_IID_IMMNotificationClient)))
    {
        *ppv = this;
        this->lpVtbl->AddRef(this);
        return S_OK;
    }

    *ppv = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE
SDLMMNotificationClient_AddRef(IMMNotificationClient *ithis)
{
	SDLMMNotificationClient *this = (SDLMMNotificationClient *) ithis;
    return (ULONG) (SDL_AtomicIncRef(&this->refcount) + 1);
}

static ULONG STDMETHODCALLTYPE
SDLMMNotificationClient_Release(IMMNotificationClient *ithis)
{
    /* this is a static object; we don't ever free it. */
	SDLMMNotificationClient *this = (SDLMMNotificationClient *) ithis;
    const ULONG retval = SDL_AtomicDecRef(&this->refcount);
    if (retval == 0) {
        SDL_AtomicSet(&this->refcount, 0);  /* uhh... */
        return 0;
    }
    return retval - 1;
}

/* These are the entry points called when WASAPI device endpoints change. */
static HRESULT STDMETHODCALLTYPE
SDLMMNotificationClient_OnDefaultDeviceChanged(IMMNotificationClient *ithis, EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId)
{
#if 0
	const char *flowstr = "?";
	char *utf8;
	SDLMMNotificationClient *this = (SDLMMNotificationClient *) ithis;
	if (role != SDL_WASAPI_role) {
		return S_OK;  /* ignore it. */
	}

	// !!! FIXME: should probably switch endpoints if we have a default device opened; it's not clear how trivial this is, though.
	// !!! FIXME:  also not clear yet how painful it is to switch when someone opens a tablet's speaker and then plugs in headphones.
	utf8 = pwstrDeviceId ? WIN_StringToUTF8(pwstrDeviceId) : NULL;
	switch (flow) {
	case eRender: flowstr = "RENDER"; break;
	case eCapture: flowstr = "CAPTURE"; break;
	case eAll: flowstr = "ALL"; break;
	}
	SDL_Log("WASAPI: OnDefaultDeviceChanged! '%s' [%s]", utf8, flowstr);
	SDL_free(utf8);
#endif

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE
SDLMMNotificationClient_OnDeviceAdded(IMMNotificationClient *ithis, LPCWSTR pwstrDeviceId)
{
	/* we ignore this; devices added here then progress to ACTIVE, if appropriate, in 
	   OnDeviceStateChange, making that a better place to deal with device adds. More 
	   importantly: the first time you plug in a USB audio device, this callback will 
	   fire, but when you unplug it, it isn't removed (it's state changes to NOTPRESENT).
	   Plugging it back in won't fire this callback again. */
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE
SDLMMNotificationClient_OnDeviceRemoved(IMMNotificationClient *ithis, LPCWSTR pwstrDeviceId)
{
	/* See notes in OnDeviceAdded handler about why we ignore this. */
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE
SDLMMNotificationClient_OnDeviceStateChanged(IMMNotificationClient *ithis, LPCWSTR pwstrDeviceId, DWORD dwNewState)
{
	SDLMMNotificationClient *this = (SDLMMNotificationClient *) ithis;
	IMMDevice *device = NULL;

	if (SUCCEEDED(IMMDeviceEnumerator_GetDevice(enumerator, pwstrDeviceId, &device))) {
		IMMEndpoint *endpoint = NULL;
		if (SUCCEEDED(IMMDevice_QueryInterface(device, &SDL_IID_IMMEndpoint, (void **) &endpoint))) {
			EDataFlow flow;
			if (SUCCEEDED(IMMEndpoint_GetDataFlow(endpoint, &flow))) {
				const SDL_bool iscapture = (flow == eCapture);
				if (dwNewState == DEVICE_STATE_ACTIVE) {
					AddWASAPIDevice(iscapture, device, pwstrDeviceId);
				} else {
					RemoveWASAPIDevice(iscapture, pwstrDeviceId);
				}
			}
			IMMEndpoint_Release(endpoint);
		}
		IMMDevice_Release(device);
	}

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE
SDLMMNotificationClient_OnPropertyValueChanged(IMMNotificationClient *this, LPCWSTR pwstrDeviceId, const PROPERTYKEY key)
{
    return S_OK;  /* we don't care about these. */
}

static const IMMNotificationClientVtbl notification_client_vtbl = {
    SDLMMNotificationClient_QueryInterface,
    SDLMMNotificationClient_AddRef,
    SDLMMNotificationClient_Release,
    SDLMMNotificationClient_OnDeviceStateChanged,
    SDLMMNotificationClient_OnDeviceAdded,
    SDLMMNotificationClient_OnDeviceRemoved,
    SDLMMNotificationClient_OnDefaultDeviceChanged,
    SDLMMNotificationClient_OnPropertyValueChanged
};

static SDLMMNotificationClient notification_client = { &notification_client_vtbl, 1 };

static SDL_bool
WStrEqual(const WCHAR *a, const WCHAR *b)
{
	while (*a) {
		if (*a != *b) {
			return SDL_FALSE;
		}
		a++;
		b++;
	}
	return *b == 0;
}

static WCHAR *
WStrDupe(const WCHAR *wstr)
{
	const int len = (lstrlenW(wstr) + 1) * sizeof (WCHAR);
	WCHAR *retval = (WCHAR *) SDL_malloc(len);
	if (retval) {
		SDL_memcpy(retval, wstr, len);
	}
	return retval;
}

static void 
RemoveWASAPIDevice(const SDL_bool iscapture, LPCWSTR devid)
{
	DevIdList *i;
	DevIdList *next;
	DevIdList *prev = NULL;
	for (i = deviceid_list; i; i = next) {
		next = i->next;
		if (WStrEqual(i->str, devid)) {
			if (prev) {
				prev->next = next;
			} else {
				deviceid_list = next;
			}
			SDL_RemoveAudioDevice(iscapture, i->str);
			SDL_free(i->str);
			SDL_free(i);
		}
		prev = i;
	}
}

static void
AddWASAPIDevice(const SDL_bool iscapture, IMMDevice *device, LPCWSTR devid)
{
	IPropertyStore *props = NULL;
    char *utf8dev = NULL;
	DevIdList *devidlist;
    PROPVARIANT var;

	/* You can have multiple endpoints on a device that are mutually exclusive ("Speakers" vs "Line Out" or whatever).
       In a perfect world, things that are unplugged won't be in this collection. The only gotcha is probably for
       phones and tablets, where you might have an internal speaker and a headphone jack and expect both to be
       available and switch automatically. (!!! FIXME...?) */

    /* PKEY_Device_FriendlyName gives you "Speakers (SoundBlaster Pro)" which drives me nuts. I'd rather it be
       "SoundBlaster Pro (Speakers)" but I guess that's developers vs users. Windows uses the FriendlyName in
       its own UIs, like Volume Control, etc. */

	/* see if we already have this one. */
	for (devidlist = deviceid_list; devidlist; devidlist = devidlist->next) {
		if (WStrEqual(devidlist->str, devid)) {
			return;  /* we already have this. */
		}
	}

	devidlist = (DevIdList *) SDL_malloc(sizeof (*devidlist));
	if (!devidlist) {
		return;  /* oh well. */
	}

	devid = WStrDupe(devid);
	if (!devid) {
		SDL_free(devidlist);
		return;  /* oh well. */
	}

	devidlist->str = (WCHAR *) devid;
	devidlist->next = deviceid_list;
	deviceid_list = devidlist;

    if (SUCCEEDED(IMMDevice_OpenPropertyStore(device, STGM_READ, &props))) {
	    PropVariantInit(&var);
	    if (SUCCEEDED(IPropertyStore_GetValue(props, &SDL_PKEY_Device_FriendlyName, &var))) {
			utf8dev = WIN_StringToUTF8(var.pwszVal);
			if (utf8dev) {
				SDL_AddAudioDevice(iscapture, utf8dev, (void *) devid);
		        SDL_free(utf8dev);
			}
		}
	    PropVariantClear(&var);
	    IPropertyStore_Release(props);
	}
}

static void
EnumerateEndpoints(const SDL_bool iscapture)
{
    IMMDeviceCollection *collection = NULL;
    UINT i, total;

    /* Note that WASAPI separates "adapter devices" from "audio endpoint devices"
       ...one adapter device ("SoundBlaster Pro") might have multiple endpoint devices ("Speakers", "Line-Out"). */

    if (FAILED(IMMDeviceEnumerator_EnumAudioEndpoints(enumerator, iscapture ? eCapture : eRender, DEVICE_STATE_ACTIVE, &collection))) {
        return;
    }

    if (FAILED(IMMDeviceCollection_GetCount(collection, &total))) {
        IMMDeviceCollection_Release(collection);
        return;
    }

    for (i = 0; i < total; i++) {
        IMMDevice *device = NULL;
        if (SUCCEEDED(IMMDeviceCollection_Item(collection, i, &device))) {
			LPWSTR devid = NULL;
			if (SUCCEEDED(IMMDevice_GetId(device, &devid))) {
				AddWASAPIDevice(iscapture, device, devid);
				CoTaskMemFree(devid);
			}
	        IMMDevice_Release(device);
		}
	}

    IMMDeviceCollection_Release(collection);
}

static void
WASAPI_DetectDevices(void)
{
    EnumerateEndpoints(SDL_FALSE);  /* playback */
    EnumerateEndpoints(SDL_TRUE);   /* capture */

    /* if this fails, we just won't get hotplug events. Carry on anyhow. */
    IMMDeviceEnumerator_RegisterEndpointNotificationCallback(enumerator, (IMMNotificationClient *) &notification_client);
}

static int
WASAPI_GetPendingBytes(_THIS)
{
    UINT32 frames = 0;
    if (FAILED(IAudioClient_GetCurrentPadding(this->hidden->client, &frames))) {
        return 0;  /* oh well. */
    }

    return ((int) frames) * this->hidden->framesize;
}

static Uint8 *
WASAPI_GetDeviceBuf(_THIS)
{
    /* get an endpoint buffer from WASAPI. */
    BYTE *buffer = NULL;
	if (FAILED(IAudioRenderClient_GetBuffer(this->hidden->render, this->spec.samples, &buffer))) {
		IAudioClient_Stop(this->hidden->client);
        SDL_OpenedAudioDeviceDisconnected(this);  /* uhoh. */
        SDL_assert(buffer == NULL);
    }
    return (Uint8 *) buffer;
}

static void
WASAPI_PlayDevice(_THIS)
{
    if (SDL_AtomicGet(&this->enabled)) {  /* not shutting down? */
        if (FAILED(IAudioRenderClient_ReleaseBuffer(this->hidden->render, this->spec.samples, 0))) {
            IAudioClient_Stop(this->hidden->client);
            SDL_OpenedAudioDeviceDisconnected(this);  /* uhoh. */
        }
    }
}

static void
WASAPI_WaitDevice(_THIS)
{
	const UINT32 maxpadding = this->spec.samples;
    while (SDL_AtomicGet(&this->enabled)) {
		UINT32 padding = 0;
		if (FAILED(IAudioClient_GetCurrentPadding(this->hidden->client, &padding))) {
		    IAudioClient_Stop(this->hidden->client);
			SDL_OpenedAudioDeviceDisconnected(this);
        }

        if (padding <= maxpadding) {
			break;
		}

		/* Sleep long enough for half the buffer to be free. */
		SDL_Delay(((padding - maxpadding) * 1000) / this->spec.freq);
	}
}

static int
WASAPI_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
    SDL_AudioStream *stream = this->hidden->capturestream;
    const int avail = SDL_AudioStreamAvailable(stream);
    if (avail > 0) {
        const int cpy = SDL_min(buflen, avail);
        SDL_AudioStreamGet(stream, buffer, cpy);
        return cpy;
    }

    while (SDL_AtomicGet(&this->enabled)) {
        HRESULT ret;
        BYTE *ptr = NULL;
        UINT32 frames = 0;
        DWORD flags = 0;

        ret = IAudioCaptureClient_GetBuffer(this->hidden->capture, &ptr, &frames, &flags, NULL, NULL);
        if ((ret == AUDCLNT_S_BUFFER_EMPTY) || !frames) {
            WASAPI_WaitDevice(this);
        } else if (ret == S_OK) {
            const int total = ((int) frames) * this->hidden->framesize;
            const int cpy = SDL_min(buflen, total);
            const int leftover = total - cpy;
            const SDL_bool silent = (flags & AUDCLNT_BUFFERFLAGS_SILENT) ? SDL_TRUE : SDL_FALSE;

            if (silent) {
                SDL_memset(buffer, this->spec.silence, cpy);
            } else {
                SDL_memcpy(buffer, ptr, cpy);
            }
            
            if (leftover > 0) {
                ptr += cpy;
                if (silent) {
                    SDL_memset(ptr, this->spec.silence, leftover);  /* I guess this is safe? */
                }

                if (SDL_AudioStreamPut(stream, ptr, leftover) == -1) {
                    return -1;  /* uhoh, out of memory, etc. Kill device.  :( */
                }
            }

            IAudioCaptureClient_ReleaseBuffer(this->hidden->capture, frames);
            return cpy;
        } else {
            break;  /* something totally failed. */
        }
    }

    return -1;  /* unrecoverable error. */
}

static void
WASAPI_FlushCapture(_THIS)
{
    if (SDL_AtomicGet(&this->enabled)) {
        BYTE *ptr = NULL;
        UINT32 frames = 0;
        DWORD flags = 0;
        HRESULT ret;
        /* just read until we stop getting packets, throwing them away. */
        while ((ret = IAudioCaptureClient_GetBuffer(this->hidden->capture, &ptr, &frames, &flags, NULL, NULL)) == S_OK) {
            IAudioCaptureClient_ReleaseBuffer(this->hidden->capture, frames);
        }
        SDL_AudioStreamClear(this->hidden->capturestream);
    }
}

static void
WASAPI_CloseDevice(_THIS)
{
    /* don't touch this->hidden->task in here; it has to be reverted from
      our callback thread. We do that in WASAPI_ThreadDeinit().
      (likewise for this->hidden->coinitialized). */

	if (this->hidden->client) {
        IAudioClient_Stop(this->hidden->client);
	}

    if (this->hidden->render) {
        IAudioRenderClient_Release(this->hidden->render);
    }

    if (this->hidden->client) {
        IAudioClient_Release(this->hidden->client);
    }

    if (this->hidden->waveformat) {
        CoTaskMemFree(this->hidden->waveformat);
    }

    if (this->hidden->device) {
        IMMDevice_Release(this->hidden->device);
    }

    if (this->hidden->capturestream) {
        SDL_FreeAudioStream(this->hidden->capturestream);
    }

    SDL_free(this->hidden);
}

static int
WASAPI_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    /* !!! FIXME: we could request an exclusive mode stream, which is lower latency;
       !!!  it will write into the kernel's audio buffer directly instead of
       !!!  shared memory that a user-mode mixer then writes to the kernel with
       !!!  everything else. Doing this means any other sound using this device will
       !!!  stop playing, including the user's MP3 player and system notification
       !!!  sounds. You'd probably need to release the device when the app isn't in
       !!!  the foreground, to be a good citizen of the system. It's doable, but it's
       !!!  more work and causes some annoyances, and I don't know what the latency
       !!!  wins actually look like. Maybe add a hint to force exclusive mode at
       !!!  some point. To be sure, defaulting to shared mode is the right thing to
       !!!  do in any case. */
    const AUDCLNT_SHAREMODE sharemode = AUDCLNT_SHAREMODE_SHARED;
    const EDataFlow dataflow = iscapture ? eCapture : eRender;
    UINT32 bufsize = 0;  /* this is in sample frames, not samples, not bytes. */
    REFERENCE_TIME duration = 0;
    IMMDevice *device = NULL;
    IAudioClient *client = NULL;
    IAudioRenderClient *render = NULL;
    IAudioCaptureClient *capture = NULL;
    WAVEFORMATEX *waveformat = NULL;
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(this->spec.format);
    SDL_AudioFormat wasapi_format = 0;
    SDL_bool valid_format = SDL_FALSE;
    HRESULT ret = S_OK;

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(this->hidden);

    if (handle == NULL) {
        ret = IMMDeviceEnumerator_GetDefaultAudioEndpoint(enumerator, dataflow, SDL_WASAPI_role, &device);
	} else {
        ret = IMMDeviceEnumerator_GetDevice(enumerator, (LPCWSTR) handle, &device);
    }

    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't find requested audio endpoint", ret);
    }

    SDL_assert(device != NULL);
    this->hidden->device = device;

    ret = IMMDevice_Activate(device, &SDL_IID_IAudioClient, CLSCTX_ALL, NULL, (void **) &client);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't activate audio endpoint", ret);
    }

    SDL_assert(client != NULL);
    this->hidden->client = client;

    ret = IAudioClient_GetMixFormat(client, &waveformat);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine mix format", ret);
    }

    SDL_assert(waveformat != NULL);
    this->hidden->waveformat = waveformat;

    /* WASAPI will not do any conversion on our behalf. Force channels and sample rate. */
    this->spec.channels = (Uint8) waveformat->nChannels;
    this->spec.freq = waveformat->nSamplesPerSec;

    /* Make sure we have a valid format that we can convert to whatever WASAPI wants. */
    if ((waveformat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) && (waveformat->wBitsPerSample == 32)) {
        wasapi_format = AUDIO_F32SYS;
    } else if ((waveformat->wFormatTag == WAVE_FORMAT_PCM) && (waveformat->wBitsPerSample == 16)) {
        wasapi_format = AUDIO_S16SYS;
    } else if ((waveformat->wFormatTag == WAVE_FORMAT_PCM) && (waveformat->wBitsPerSample == 32)) {
        wasapi_format = AUDIO_S32SYS;
    } else if (waveformat->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        const WAVEFORMATEXTENSIBLE *ext = (const WAVEFORMATEXTENSIBLE *) waveformat;
        if ((SDL_memcmp(&ext->SubFormat, &SDL_KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof (GUID)) == 0) && (waveformat->wBitsPerSample == 32)) {
            wasapi_format = AUDIO_F32SYS;
        } else if ((SDL_memcmp(&ext->SubFormat, &SDL_KSDATAFORMAT_SUBTYPE_PCM, sizeof (GUID)) == 0) && (waveformat->wBitsPerSample == 16)) {
            wasapi_format = AUDIO_S16SYS;
        } else if ((SDL_memcmp(&ext->SubFormat, &SDL_KSDATAFORMAT_SUBTYPE_PCM, sizeof (GUID)) == 0) && (waveformat->wBitsPerSample == 32)) {
            wasapi_format = AUDIO_S32SYS;
        }
    }

    while ((!valid_format) && (test_format)) {
        if (test_format == wasapi_format) {
            this->spec.format = test_format;
            valid_format = SDL_TRUE;
            break;
        }
        test_format = SDL_NextAudioFormat();
    }

    if (!valid_format) {
        return SDL_SetError("WASAPI: Unsupported audio format");
    }

    ret = IAudioClient_GetDevicePeriod(client, NULL, &duration);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine minimum device period", ret);
    }

    ret = IAudioClient_Initialize(client, sharemode, 0, duration, sharemode == AUDCLNT_SHAREMODE_SHARED ? 0 : duration, waveformat, NULL);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't initialize audio client", ret);
    }

	ret = IAudioClient_GetBufferSize(client, &bufsize);
    if (FAILED(ret)) {
        return WIN_SetErrorFromHRESULT("WASAPI can't determine buffer size", ret);
    }

    this->spec.samples = (Uint16) bufsize;
	if (!iscapture) {
		this->spec.samples /= 2;  /* fill half of the DMA buffer on each run. */
	}

    /* Update the fragment size as size in bytes */
    SDL_CalculateAudioSpec(&this->spec);

    this->hidden->framesize = (SDL_AUDIO_BITSIZE(this->spec.format) / 8) * this->spec.channels;

    if (iscapture) {
        this->hidden->capturestream = SDL_NewAudioStream(this->spec.format, this->spec.channels, this->spec.freq, this->spec.format, this->spec.channels, this->spec.freq);
        if (!this->hidden->capturestream) {
            return -1;  /* already set SDL_Error */
        }

        ret = IAudioClient_GetService(client, &SDL_IID_IAudioCaptureClient, (void**) &capture);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't get capture client service", ret);
        }

        SDL_assert(capture != NULL);
        this->hidden->capture = capture;
        ret = IAudioClient_Start(client);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't start capture", ret);
        }

        WASAPI_FlushCapture(this);  /* MSDN says you should flush capture endpoint right after startup. */
    } else {
        ret = IAudioClient_GetService(client, &SDL_IID_IAudioRenderClient, (void**) &render);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't get render client service", ret);
        }

        SDL_assert(render != NULL);
        this->hidden->render = render;
        ret = IAudioClient_Start(client);
        if (FAILED(ret)) {
            return WIN_SetErrorFromHRESULT("WASAPI can't start playback", ret);
        }
    }

    return 0;  /* good to go. */
}

static void
WASAPI_ThreadInit(_THIS)
{
    /* this thread uses COM. */
    if (SUCCEEDED(WIN_CoInitialize())) {    /* can't report errors, hope it worked! */
        this->hidden->coinitialized = SDL_TRUE;
    }

    /* Set this thread to very high "Pro Audio" priority. */
    if (pAvSetMmThreadCharacteristicsW) {
        DWORD idx = 0;
        this->hidden->task = pAvSetMmThreadCharacteristicsW(TEXT("Pro Audio"), &idx);
    }
}

static void
WASAPI_ThreadDeinit(_THIS)
{
    /* Set this thread to very high "Pro Audio" priority. */
    if (this->hidden->task && pAvRevertMmThreadCharacteristics) {
        pAvRevertMmThreadCharacteristics(this->hidden->task);
        this->hidden->task = NULL;
    }

    if (this->hidden->coinitialized) {
        WIN_CoUninitialize();
    }
}

static void
WASAPI_Deinitialize(void)
{
	DevIdList *devidlist;
	DevIdList *next;

    if (enumerator) {
        IMMDeviceEnumerator_UnregisterEndpointNotificationCallback(enumerator, (IMMNotificationClient *) &notification_client);
        IMMDeviceEnumerator_Release(enumerator);
        enumerator = NULL;
    }

    #ifndef __WINRT__
    if (libavrt) {
        FreeLibrary(libavrt);
        libavrt = NULL;
    }
    #endif

    pAvSetMmThreadCharacteristicsW = NULL;
    pAvRevertMmThreadCharacteristics = NULL;

	for (devidlist = deviceid_list; devidlist; devidlist = next) {
		next = devidlist->next;
		SDL_free(devidlist->str);
		SDL_free(devidlist);
	}
	deviceid_list = NULL;

    WIN_CoUninitialize();
}

static int
WASAPI_Init(SDL_AudioDriverImpl * impl)
{
	HRESULT ret;

	/* just skip the discussion with COM here. */
	if (!WIN_IsWindowsVistaOrGreater()) {
		return SDL_SetError("WASAPI support requires Windows Vista or later");
	}

	if (FAILED(WIN_CoInitialize())) {
        SDL_SetError("WASAPI: CoInitialize() failed");
        return 0;
    }

	ret = CoCreateInstance(&SDL_CLSID_MMDeviceEnumerator, NULL, CLSCTX_INPROC_SERVER, &SDL_IID_IMMDeviceEnumerator, (LPVOID) &enumerator);
    if (FAILED(ret)) {
        WIN_CoUninitialize();
        WIN_SetErrorFromHRESULT("WASAPI CoCreateInstance(MMDeviceEnumerator)", ret);
        return 0;  /* oh well. */
    }

    #ifdef __WINRT__
    pAvSetMmThreadCharacteristicsW = AvSetMmThreadCharacteristicsW;
    pAvRevertMmThreadCharacteristics = AvRevertMmThreadCharacteristics;
    #else
    libavrt = LoadLibraryW(L"avrt.dll");  /* this library is available in Vista and later. No WinXP, so have to LoadLibrary to use it for now! */
    if (libavrt) {
        pAvSetMmThreadCharacteristicsW = (pfnAvSetMmThreadCharacteristicsW) GetProcAddress(libavrt, "AvSetMmThreadCharacteristicsW");
        pAvRevertMmThreadCharacteristics = (pfnAvRevertMmThreadCharacteristics) GetProcAddress(libavrt, "AvRevertMmThreadCharacteristics");
    }
    #endif

    /* Set the function pointers */
    impl->DetectDevices = WASAPI_DetectDevices;
    impl->ThreadInit = WASAPI_ThreadInit;
    impl->ThreadDeinit = WASAPI_ThreadDeinit;
    impl->OpenDevice = WASAPI_OpenDevice;
    impl->PlayDevice = WASAPI_PlayDevice;
    impl->WaitDevice = WASAPI_WaitDevice;
    impl->GetPendingBytes = WASAPI_GetPendingBytes;
    impl->GetDeviceBuf = WASAPI_GetDeviceBuf;
    impl->CaptureFromDevice = WASAPI_CaptureFromDevice;
    impl->FlushCapture = WASAPI_FlushCapture;
    impl->CloseDevice = WASAPI_CloseDevice;
    impl->Deinitialize = WASAPI_Deinitialize;
    impl->HasCaptureSupport = 1;

    return 1;   /* this audio target is available. */
}

AudioBootStrap WASAPI_bootstrap = {
    "wasapi", "WASAPI", WASAPI_Init, 0
};

#endif  /* SDL_AUDIO_DRIVER_WASAPI */

/* vi: set ts=4 sw=4 expandtab: */
