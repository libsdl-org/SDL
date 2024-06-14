/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifdef SDL_AUDIO_DRIVER_ALSA

#ifndef SDL_ALSA_NON_BLOCKING
#define SDL_ALSA_NON_BLOCKING 0
#endif

// without the thread, you will detect devices on startup, but will not get further hotplug events. But that might be okay.
#ifndef SDL_ALSA_HOTPLUG_THREAD
#define SDL_ALSA_HOTPLUG_THREAD 1
#endif

// Allow access to a raw mixing buffer

#include <sys/types.h>
#include <signal.h> // For kill()
#include <string.h>

#include "../SDL_sysaudio.h"
#include "SDL_alsa_audio.h"

#ifdef SDL_AUDIO_DRIVER_ALSA_DYNAMIC
#endif

static int (*ALSA_snd_pcm_open)(snd_pcm_t **, const char *, snd_pcm_stream_t, int);
static int (*ALSA_snd_pcm_close)(snd_pcm_t *pcm);
static int (*ALSA_snd_pcm_start)(snd_pcm_t *pcm);
static snd_pcm_sframes_t (*ALSA_snd_pcm_writei)(snd_pcm_t *, const void *, snd_pcm_uframes_t);
static snd_pcm_sframes_t (*ALSA_snd_pcm_readi)(snd_pcm_t *, void *, snd_pcm_uframes_t);
static int (*ALSA_snd_pcm_recover)(snd_pcm_t *, int, int);
static int (*ALSA_snd_pcm_prepare)(snd_pcm_t *);
static int (*ALSA_snd_pcm_drain)(snd_pcm_t *);
static const char *(*ALSA_snd_strerror)(int);
static size_t (*ALSA_snd_pcm_hw_params_sizeof)(void);
static size_t (*ALSA_snd_pcm_sw_params_sizeof)(void);
static void (*ALSA_snd_pcm_hw_params_copy)(snd_pcm_hw_params_t *, const snd_pcm_hw_params_t *);
static int (*ALSA_snd_pcm_hw_params_any)(snd_pcm_t *, snd_pcm_hw_params_t *);
static int (*ALSA_snd_pcm_hw_params_set_access)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_access_t);
static int (*ALSA_snd_pcm_hw_params_set_format)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_format_t);
static int (*ALSA_snd_pcm_hw_params_set_channels)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int);
static int (*ALSA_snd_pcm_hw_params_get_channels)(const snd_pcm_hw_params_t *, unsigned int *);
static int (*ALSA_snd_pcm_hw_params_set_rate_near)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
static int (*ALSA_snd_pcm_hw_params_set_period_size_near)(snd_pcm_t *, snd_pcm_hw_params_t *, snd_pcm_uframes_t *, int *);
static int (*ALSA_snd_pcm_hw_params_get_period_size)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *, int *);
static int (*ALSA_snd_pcm_hw_params_set_periods_min)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
static int (*ALSA_snd_pcm_hw_params_set_periods_first)(snd_pcm_t *, snd_pcm_hw_params_t *, unsigned int *, int *);
static int (*ALSA_snd_pcm_hw_params_get_periods)(const snd_pcm_hw_params_t *, unsigned int *, int *);
static int (*ALSA_snd_pcm_hw_params_set_buffer_size_near)(snd_pcm_t *pcm, snd_pcm_hw_params_t *, snd_pcm_uframes_t *);
static int (*ALSA_snd_pcm_hw_params_get_buffer_size)(const snd_pcm_hw_params_t *, snd_pcm_uframes_t *);
static int (*ALSA_snd_pcm_hw_params)(snd_pcm_t *, snd_pcm_hw_params_t *);
static int (*ALSA_snd_pcm_sw_params_current)(snd_pcm_t *,
                                             snd_pcm_sw_params_t *);
static int (*ALSA_snd_pcm_sw_params_set_start_threshold)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);
static int (*ALSA_snd_pcm_sw_params)(snd_pcm_t *, snd_pcm_sw_params_t *);
static int (*ALSA_snd_pcm_nonblock)(snd_pcm_t *, int);
static int (*ALSA_snd_pcm_wait)(snd_pcm_t *, int);
static int (*ALSA_snd_pcm_sw_params_set_avail_min)(snd_pcm_t *, snd_pcm_sw_params_t *, snd_pcm_uframes_t);
static int (*ALSA_snd_pcm_reset)(snd_pcm_t *);
static int (*ALSA_snd_device_name_hint)(int, const char *, void ***);
static char *(*ALSA_snd_device_name_get_hint)(const void *, const char *);
static int (*ALSA_snd_device_name_free_hint)(void **);
static snd_pcm_sframes_t (*ALSA_snd_pcm_avail)(snd_pcm_t *);
#ifdef SND_CHMAP_API_VERSION
static snd_pcm_chmap_t *(*ALSA_snd_pcm_get_chmap)(snd_pcm_t *);
static int (*ALSA_snd_pcm_chmap_print)(const snd_pcm_chmap_t *map, size_t maxlen, char *buf);
#endif

#ifdef SDL_AUDIO_DRIVER_ALSA_DYNAMIC
#define snd_pcm_hw_params_sizeof ALSA_snd_pcm_hw_params_sizeof
#define snd_pcm_sw_params_sizeof ALSA_snd_pcm_sw_params_sizeof

static const char *alsa_library = SDL_AUDIO_DRIVER_ALSA_DYNAMIC;
static void *alsa_handle = NULL;

static int load_alsa_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(alsa_handle, fn);
    if (!*addr) {
        // Don't call SDL_SetError(): SDL_LoadFunction already did.
        return 0;
    }

    return 1;
}

// cast funcs to char* first, to please GCC's strict aliasing rules.
#define SDL_ALSA_SYM(x)                                 \
    if (!load_alsa_sym(#x, (void **)(char *)&ALSA_##x)) \
    return -1
#else
#define SDL_ALSA_SYM(x) ALSA_##x = x
#endif

static int load_alsa_syms(void)
{
    SDL_ALSA_SYM(snd_pcm_open);
    SDL_ALSA_SYM(snd_pcm_close);
    SDL_ALSA_SYM(snd_pcm_start);
    SDL_ALSA_SYM(snd_pcm_writei);
    SDL_ALSA_SYM(snd_pcm_readi);
    SDL_ALSA_SYM(snd_pcm_recover);
    SDL_ALSA_SYM(snd_pcm_prepare);
    SDL_ALSA_SYM(snd_pcm_drain);
    SDL_ALSA_SYM(snd_strerror);
    SDL_ALSA_SYM(snd_pcm_hw_params_sizeof);
    SDL_ALSA_SYM(snd_pcm_sw_params_sizeof);
    SDL_ALSA_SYM(snd_pcm_hw_params_copy);
    SDL_ALSA_SYM(snd_pcm_hw_params_any);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_access);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_format);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_channels);
    SDL_ALSA_SYM(snd_pcm_hw_params_get_channels);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_rate_near);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_period_size_near);
    SDL_ALSA_SYM(snd_pcm_hw_params_get_period_size);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_periods_min);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_periods_first);
    SDL_ALSA_SYM(snd_pcm_hw_params_get_periods);
    SDL_ALSA_SYM(snd_pcm_hw_params_set_buffer_size_near);
    SDL_ALSA_SYM(snd_pcm_hw_params_get_buffer_size);
    SDL_ALSA_SYM(snd_pcm_hw_params);
    SDL_ALSA_SYM(snd_pcm_sw_params_current);
    SDL_ALSA_SYM(snd_pcm_sw_params_set_start_threshold);
    SDL_ALSA_SYM(snd_pcm_sw_params);
    SDL_ALSA_SYM(snd_pcm_nonblock);
    SDL_ALSA_SYM(snd_pcm_wait);
    SDL_ALSA_SYM(snd_pcm_sw_params_set_avail_min);
    SDL_ALSA_SYM(snd_pcm_reset);
    SDL_ALSA_SYM(snd_device_name_hint);
    SDL_ALSA_SYM(snd_device_name_get_hint);
    SDL_ALSA_SYM(snd_device_name_free_hint);
    SDL_ALSA_SYM(snd_pcm_avail);
#ifdef SND_CHMAP_API_VERSION
    SDL_ALSA_SYM(snd_pcm_get_chmap);
    SDL_ALSA_SYM(snd_pcm_chmap_print);
#endif

    return 0;
}

#undef SDL_ALSA_SYM

#ifdef SDL_AUDIO_DRIVER_ALSA_DYNAMIC

static void UnloadALSALibrary(void)
{
    if (alsa_handle) {
        SDL_UnloadObject(alsa_handle);
        alsa_handle = NULL;
    }
}

static int LoadALSALibrary(void)
{
    int retval = 0;
    if (!alsa_handle) {
        alsa_handle = SDL_LoadObject(alsa_library);
        if (!alsa_handle) {
            retval = -1;
            // Don't call SDL_SetError(): SDL_LoadObject already did.
        } else {
            retval = load_alsa_syms();
            if (retval < 0) {
                UnloadALSALibrary();
            }
        }
    }
    return retval;
}

#else

static void UnloadALSALibrary(void)
{
}

static int LoadALSALibrary(void)
{
    load_alsa_syms();
    return 0;
}

#endif // SDL_AUDIO_DRIVER_ALSA_DYNAMIC

typedef struct ALSA_Device
{
    char *name;
    SDL_bool recording;
    struct ALSA_Device *next;
} ALSA_Device;

static const ALSA_Device default_playback_handle = {
    "default",
    SDL_FALSE,
    NULL
};

static const ALSA_Device default_recording_handle = {
    "default",
    SDL_TRUE,
    NULL
};

static const char *get_audio_device(void *handle, const int channels)
{
    SDL_assert(handle != NULL);  // SDL2 used NULL to mean "default" but that's not true in SDL3.

    ALSA_Device *dev = (ALSA_Device *)handle;
    if (SDL_strcmp(dev->name, "default") == 0) {
        const char *device = SDL_getenv("AUDIODEV"); // Is there a standard variable name?
        if (device) {
            return device;
        } else if (channels == 6) {
            return "plug:surround51";
        } else if (channels == 4) {
            return "plug:surround40";
        }
        return "default";
    }

    return dev->name;
}

// !!! FIXME: is there a channel swizzler in alsalib instead?

// https://bugzilla.libsdl.org/show_bug.cgi?id=110
//  "For Linux ALSA, this is FL-FR-RL-RR-C-LFE
//  and for Windows DirectX [and CoreAudio], this is FL-FR-C-LFE-RL-RR"
#define SWIZ6(T)                                                                  \
    static void swizzle_alsa_channels_6_##T(void *buffer, const Uint32 bufferlen) \
    {                                                                             \
        T *ptr = (T *)buffer;                                                     \
        Uint32 i;                                                                 \
        for (i = 0; i < bufferlen; i++, ptr += 6) {                               \
            T tmp;                                                                \
            tmp = ptr[2];                                                         \
            ptr[2] = ptr[4];                                                      \
            ptr[4] = tmp;                                                         \
            tmp = ptr[3];                                                         \
            ptr[3] = ptr[5];                                                      \
            ptr[5] = tmp;                                                         \
        }                                                                         \
    }


// !!! FIXME: is there a channel swizzler in alsalib instead?
// !!! FIXME: this screams for a SIMD shuffle operation.

// https://docs.microsoft.com/en-us/windows-hardware/drivers/audio/mapping-stream-formats-to-speaker-configurations
//  For Linux ALSA, this appears to be FL-FR-RL-RR-C-LFE-SL-SR
//  and for Windows DirectX [and CoreAudio], this is FL-FR-C-LFE-SL-SR-RL-RR"
#define SWIZ8(T)                                                                  \
    static void swizzle_alsa_channels_8_##T(void *buffer, const Uint32 bufferlen) \
    {                                                                             \
        T *ptr = (T *)buffer;                                                     \
        Uint32 i;                                                                 \
        for (i = 0; i < bufferlen; i++, ptr += 6) {                               \
            const T center = ptr[2];                                              \
            const T subwoofer = ptr[3];                                           \
            const T side_left = ptr[4];                                           \
            const T side_right = ptr[5];                                          \
            const T rear_left = ptr[6];                                           \
            const T rear_right = ptr[7];                                          \
            ptr[2] = rear_left;                                                   \
            ptr[3] = rear_right;                                                  \
            ptr[4] = center;                                                      \
            ptr[5] = subwoofer;                                                   \
            ptr[6] = side_left;                                                   \
            ptr[7] = side_right;                                                  \
        }                                                                         \
    }

#define CHANNEL_SWIZZLE(x) \
    x(Uint64)              \
        x(Uint32)          \
            x(Uint16)      \
                x(Uint8)

CHANNEL_SWIZZLE(SWIZ6)
CHANNEL_SWIZZLE(SWIZ8)

#undef CHANNEL_SWIZZLE
#undef SWIZ6
#undef SWIZ8

// Called right before feeding device->hidden->mixbuf to the hardware. Swizzle
//  channels from Windows/Mac order to the format alsalib will want.
static void swizzle_alsa_channels(SDL_AudioDevice *device, void *buffer, Uint32 bufferlen)
{
    switch (device->spec.channels) {
#define CHANSWIZ(chans)                                                \
    case chans:                                                        \
        switch ((device->spec.format & (0xFF))) {                        \
        case 8:                                                        \
            swizzle_alsa_channels_##chans##_Uint8(buffer, bufferlen);  \
            break;                                                     \
        case 16:                                                       \
            swizzle_alsa_channels_##chans##_Uint16(buffer, bufferlen); \
            break;                                                     \
        case 32:                                                       \
            swizzle_alsa_channels_##chans##_Uint32(buffer, bufferlen); \
            break;                                                     \
        case 64:                                                       \
            swizzle_alsa_channels_##chans##_Uint64(buffer, bufferlen); \
            break;                                                     \
        default:                                                       \
            SDL_assert(!"unhandled bitsize");                          \
            break;                                                     \
        }                                                              \
        return;

        CHANSWIZ(6);
        CHANSWIZ(8);
#undef CHANSWIZ
    default:
        break;
    }
}

#ifdef SND_CHMAP_API_VERSION
// Some devices have the right channel map, no swizzling necessary
static void no_swizzle(SDL_AudioDevice *device, void *buffer, Uint32 bufferlen)
{
}
#endif // SND_CHMAP_API_VERSION

// This function waits until it is possible to write a full sound buffer
static int ALSA_WaitDevice(SDL_AudioDevice *device)
{
    const int fulldelay = (int) ((((Uint64) device->sample_frames) * 1000) / device->spec.freq);
    const int delay = SDL_max(fulldelay, 10);

    while (!SDL_AtomicGet(&device->shutdown)) {
        const int rc = ALSA_snd_pcm_wait(device->hidden->pcm_handle, delay);
        if (rc < 0 && (rc != -EAGAIN)) {
            const int status = ALSA_snd_pcm_recover(device->hidden->pcm_handle, rc, 0);
            if (status < 0) {
                // Hmm, not much we can do - abort
                SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ALSA: snd_pcm_wait failed (unrecoverable): %s", ALSA_snd_strerror(rc));
                return -1;
            }
            continue;
        }

        if (rc > 0) {
            break;  // ready to go!
        }

        // Timed out! Make sure we aren't shutting down and then wait again.
    }

    return 0;
}

static int ALSA_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    SDL_assert(buffer == device->hidden->mixbuf);
    Uint8 *sample_buf = (Uint8 *) buffer;  // !!! FIXME: deal with this without casting away constness
    const int frame_size = SDL_AUDIO_FRAMESIZE(device->spec);
    snd_pcm_uframes_t frames_left = (snd_pcm_uframes_t) (buflen / frame_size);

    device->hidden->swizzle_func(device, sample_buf, frames_left);

    while ((frames_left > 0) && !SDL_AtomicGet(&device->shutdown)) {
        const int rc = ALSA_snd_pcm_writei(device->hidden->pcm_handle, sample_buf, frames_left);
        //SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "ALSA PLAYDEVICE: WROTE %d of %d bytes", (rc >= 0) ? ((int) (rc * frame_size)) : rc, (int) (frames_left * frame_size));
        SDL_assert(rc != 0);  // assuming this can't happen if we used snd_pcm_wait and queried for available space.
        if (rc < 0) {
            SDL_assert(rc != -EAGAIN);  // assuming this can't happen if we used snd_pcm_wait and queried for available space. snd_pcm_recover won't handle it!
            const int status = ALSA_snd_pcm_recover(device->hidden->pcm_handle, rc, 0);
            if (status < 0) {
                // Hmm, not much we can do - abort
                SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ALSA write failed (unrecoverable): %s", ALSA_snd_strerror(rc));
                return -1;
            }
            continue;
        }

        sample_buf += rc * frame_size;
        frames_left -= rc;
    }

    return 0;
}

static Uint8 *ALSA_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    snd_pcm_sframes_t rc = ALSA_snd_pcm_avail(device->hidden->pcm_handle);
    if (rc <= 0) {
        // Wait a bit and try again, maybe the hardware isn't quite ready yet?
        SDL_Delay(1);

        rc = ALSA_snd_pcm_avail(device->hidden->pcm_handle);
        if (rc <= 0) {
            // We'll catch it next time
            *buffer_size = 0;
            return NULL;
        }
    }

    const int requested_frames = SDL_min(device->sample_frames, rc);
    const int requested_bytes = requested_frames * SDL_AUDIO_FRAMESIZE(device->spec);
    SDL_assert(requested_bytes <= *buffer_size);
    //SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "ALSA GETDEVICEBUF: NEED %d BYTES", requested_bytes);
    *buffer_size = requested_bytes;
    return device->hidden->mixbuf;
}

static int ALSA_RecordDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    const int frame_size = SDL_AUDIO_FRAMESIZE(device->spec);
    SDL_assert((buflen % frame_size) == 0);

    const snd_pcm_sframes_t total_available = ALSA_snd_pcm_avail(device->hidden->pcm_handle);
    const int total_frames = SDL_min(buflen / frame_size, total_available);

    const int rc = ALSA_snd_pcm_readi(device->hidden->pcm_handle, buffer, total_frames);

    SDL_assert(rc != -EAGAIN);  // assuming this can't happen if we used snd_pcm_wait and queried for available space. snd_pcm_recover won't handle it!

    if (rc < 0) {
        const int status = ALSA_snd_pcm_recover(device->hidden->pcm_handle, rc, 0);
        if (status < 0) {
            // Hmm, not much we can do - abort
            SDL_LogError(SDL_LOG_CATEGORY_AUDIO, "ALSA read failed (unrecoverable): %s", ALSA_snd_strerror(rc));
            return -1;
        }
        return 0;  // go back to WaitDevice and try again.
    } else if (rc > 0) {
        device->hidden->swizzle_func(device, buffer, total_frames - rc);
    }

    //SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "ALSA: recorded %d bytes", rc * frame_size);

    return rc * frame_size;
}

static void ALSA_FlushRecording(SDL_AudioDevice *device)
{
    ALSA_snd_pcm_reset(device->hidden->pcm_handle);
}

static void ALSA_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->pcm_handle) {
            // Wait for the submitted audio to drain. ALSA_snd_pcm_drop() can hang, so don't use that.
            SDL_Delay(((device->sample_frames * 1000) / device->spec.freq) * 2);
            ALSA_snd_pcm_close(device->hidden->pcm_handle);
        }
        SDL_free(device->hidden->mixbuf);
        SDL_free(device->hidden);
    }
}

static int ALSA_set_buffer_size(SDL_AudioDevice *device, snd_pcm_hw_params_t *params)
{
    int status;
    snd_pcm_hw_params_t *hwparams;
    snd_pcm_uframes_t persize;
    unsigned int periods;

    // Copy the hardware parameters for this setup
    snd_pcm_hw_params_alloca(&hwparams);
    ALSA_snd_pcm_hw_params_copy(hwparams, params);

    // Attempt to match the period size to the requested buffer size
    persize = device->sample_frames;
    status = ALSA_snd_pcm_hw_params_set_period_size_near(
        device->hidden->pcm_handle, hwparams, &persize, NULL);
    if (status < 0) {
        return -1;
    }

    // Need to at least double buffer
    periods = 2;
    status = ALSA_snd_pcm_hw_params_set_periods_min(
        device->hidden->pcm_handle, hwparams, &periods, NULL);
    if (status < 0) {
        return -1;
    }

    status = ALSA_snd_pcm_hw_params_set_periods_first(
        device->hidden->pcm_handle, hwparams, &periods, NULL);
    if (status < 0) {
        return -1;
    }

    // "set" the hardware with the desired parameters
    status = ALSA_snd_pcm_hw_params(device->hidden->pcm_handle, hwparams);
    if (status < 0) {
        return -1;
    }

    device->sample_frames = persize;

    // This is useful for debugging
    if (SDL_getenv("SDL_AUDIO_ALSA_DEBUG")) {
        snd_pcm_uframes_t bufsize;

        ALSA_snd_pcm_hw_params_get_buffer_size(hwparams, &bufsize);

        SDL_LogError(SDL_LOG_CATEGORY_AUDIO,
                     "ALSA: period size = %ld, periods = %u, buffer size = %lu",
                     persize, periods, bufsize);
    }

    return 0;
}

static int ALSA_OpenDevice(SDL_AudioDevice *device)
{
    const SDL_bool recording = device->recording;
    int status = 0;

    // Initialize all variables that we clean on shutdown
    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return -1;
    }

    // Open the audio device
    // Name of device should depend on # channels in spec
    snd_pcm_t *pcm_handle = NULL;
    status = ALSA_snd_pcm_open(&pcm_handle,
                               get_audio_device(device->handle, device->spec.channels),
                               recording ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK,
                               SND_PCM_NONBLOCK);

    if (status < 0) {
        return SDL_SetError("ALSA: Couldn't open audio device: %s", ALSA_snd_strerror(status));
    }

    device->hidden->pcm_handle = pcm_handle;

    // Figure out what the hardware is capable of
    snd_pcm_hw_params_t *hwparams = NULL;
    snd_pcm_hw_params_alloca(&hwparams);
    status = ALSA_snd_pcm_hw_params_any(pcm_handle, hwparams);
    if (status < 0) {
        return SDL_SetError("ALSA: Couldn't get hardware config: %s", ALSA_snd_strerror(status));
    }

    // SDL only uses interleaved sample output
    status = ALSA_snd_pcm_hw_params_set_access(pcm_handle, hwparams,
                                               SND_PCM_ACCESS_RW_INTERLEAVED);
    if (status < 0) {
        return SDL_SetError("ALSA: Couldn't set interleaved access: %s", ALSA_snd_strerror(status));
    }

    // Try for a closest match on audio format
    snd_pcm_format_t format = 0;
    const SDL_AudioFormat *closefmts = SDL_ClosestAudioFormats(device->spec.format);
    SDL_AudioFormat test_format;
    while ((test_format = *(closefmts++)) != 0) {
        switch (test_format) {
        case SDL_AUDIO_U8:
            format = SND_PCM_FORMAT_U8;
            break;
        case SDL_AUDIO_S8:
            format = SND_PCM_FORMAT_S8;
            break;
        case SDL_AUDIO_S16LE:
            format = SND_PCM_FORMAT_S16_LE;
            break;
        case SDL_AUDIO_S16BE:
            format = SND_PCM_FORMAT_S16_BE;
            break;
        case SDL_AUDIO_S32LE:
            format = SND_PCM_FORMAT_S32_LE;
            break;
        case SDL_AUDIO_S32BE:
            format = SND_PCM_FORMAT_S32_BE;
            break;
        case SDL_AUDIO_F32LE:
            format = SND_PCM_FORMAT_FLOAT_LE;
            break;
        case SDL_AUDIO_F32BE:
            format = SND_PCM_FORMAT_FLOAT_BE;
            break;
        default:
            continue;
        }
        if (ALSA_snd_pcm_hw_params_set_format(pcm_handle, hwparams, format) >= 0) {
            break;
        }
    }
    if (!test_format) {
        return SDL_SetError("ALSA: Unsupported audio format");
    }
    device->spec.format = test_format;

    // Validate number of channels and determine if swizzling is necessary.
    //  Assume original swizzling, until proven otherwise.
    device->hidden->swizzle_func = swizzle_alsa_channels;
#ifdef SND_CHMAP_API_VERSION
    snd_pcm_chmap_t *chmap = ALSA_snd_pcm_get_chmap(pcm_handle);
    if (chmap) {
        char chmap_str[64];
        if (ALSA_snd_pcm_chmap_print(chmap, sizeof(chmap_str), chmap_str) > 0) {
            if (SDL_strcmp("FL FR FC LFE RL RR", chmap_str) == 0 ||
                SDL_strcmp("FL FR FC LFE SL SR", chmap_str) == 0) {
                device->hidden->swizzle_func = no_swizzle;
            }
        }
        free(chmap); // This should NOT be SDL_free()
    }
#endif // SND_CHMAP_API_VERSION

    // Set the number of channels
    status = ALSA_snd_pcm_hw_params_set_channels(pcm_handle, hwparams,
                                                 device->spec.channels);
    unsigned int channels = device->spec.channels;
    if (status < 0) {
        status = ALSA_snd_pcm_hw_params_get_channels(hwparams, &channels);
        if (status < 0) {
            return SDL_SetError("ALSA: Couldn't set audio channels");
        }
        device->spec.channels = channels;
    }

    // Set the audio rate
    unsigned int rate = device->spec.freq;
    status = ALSA_snd_pcm_hw_params_set_rate_near(pcm_handle, hwparams,
                                                  &rate, NULL);
    if (status < 0) {
        return SDL_SetError("ALSA: Couldn't set audio frequency: %s", ALSA_snd_strerror(status));
    }
    device->spec.freq = rate;

    // Set the buffer size, in samples
    status = ALSA_set_buffer_size(device, hwparams);
    if (status < 0) {
        return SDL_SetError("Couldn't set hardware audio parameters: %s", ALSA_snd_strerror(status));
    }

    // Set the software parameters
    snd_pcm_sw_params_t *swparams = NULL;
    snd_pcm_sw_params_alloca(&swparams);
    status = ALSA_snd_pcm_sw_params_current(pcm_handle, swparams);
    if (status < 0) {
        return SDL_SetError("ALSA: Couldn't get software config: %s", ALSA_snd_strerror(status));
    }
    status = ALSA_snd_pcm_sw_params_set_avail_min(pcm_handle, swparams, device->sample_frames);
    if (status < 0) {
        return SDL_SetError("Couldn't set minimum available samples: %s", ALSA_snd_strerror(status));
    }
    status =
        ALSA_snd_pcm_sw_params_set_start_threshold(pcm_handle, swparams, 1);
    if (status < 0) {
        return SDL_SetError("ALSA: Couldn't set start threshold: %s", ALSA_snd_strerror(status));
    }
    status = ALSA_snd_pcm_sw_params(pcm_handle, swparams);
    if (status < 0) {
        return SDL_SetError("Couldn't set software audio parameters: %s", ALSA_snd_strerror(status));
    }

    // Calculate the final parameters for this audio specification
    SDL_UpdatedAudioDeviceFormat(device);

    // Allocate mixing buffer
    if (!recording) {
        device->hidden->mixbuf = (Uint8 *)SDL_malloc(device->buffer_size);
        if (!device->hidden->mixbuf) {
            return -1;
        }
        SDL_memset(device->hidden->mixbuf, device->silence_value, device->buffer_size);
    }

#if !SDL_ALSA_NON_BLOCKING
    if (!recording) {
        ALSA_snd_pcm_nonblock(pcm_handle, 0);
    }
#endif

    ALSA_snd_pcm_start(pcm_handle);

    return 0;  // We're ready to rock and roll. :-)
}

static void add_device(const SDL_bool recording, const char *name, void *hint, ALSA_Device **pSeen)
{
    ALSA_Device *dev = SDL_malloc(sizeof(ALSA_Device));
    char *desc;
    char *ptr;

    if (!dev) {
        return;
    }

    // Not all alsa devices are enumerable via snd_device_name_get_hint
    //  (i.e. bluetooth devices).  Therefore if hint is passed in to this
    //  function as NULL, assume name contains desc.
    //  Make sure not to free the storage associated with desc in this case
    if (hint) {
        desc = ALSA_snd_device_name_get_hint(hint, "DESC");
        if (!desc) {
            SDL_free(dev);
            return;
        }
    } else {
        desc = (char *)name;
    }

    SDL_assert(name != NULL);

    // some strings have newlines, like "HDA NVidia, HDMI 0\nHDMI Audio Output".
    //  just chop the extra lines off, this seems to get a reasonable device
    //  name without extra details.
    ptr = SDL_strchr(desc, '\n');
    if (ptr) {
        *ptr = '\0';
    }

    //SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "ALSA: adding %s device '%s' (%s)", recording ? "recording" : "playback", name, desc);

    dev->name = SDL_strdup(name);
    if (!dev->name) {
        if (hint) {
            free(desc); // This should NOT be SDL_free()
        }
        SDL_free(dev->name);
        SDL_free(dev);
        return;
    }

    // Note that spec is NULL, because we are required to open the device before
    //  acquiring the mix format, making this information inaccessible at
    //  enumeration time
    SDL_AddAudioDevice(recording, desc, NULL, dev);
    if (hint) {
        free(desc); // This should NOT be SDL_free()
    }

    dev->recording = recording;
    dev->next = *pSeen;
    *pSeen = dev;
}

static ALSA_Device *hotplug_devices = NULL;

static void ALSA_HotplugIteration(SDL_bool *has_default_playback, SDL_bool *has_default_recording)
{
    void **hints = NULL;
    ALSA_Device *unseen = NULL;
    ALSA_Device *seen = NULL;

    if (ALSA_snd_device_name_hint(-1, "pcm", &hints) == 0) {
        const char *match = NULL;
        int bestmatch = 0xFFFF;
        int has_default = -1;
        size_t match_len = 0;
        static const char *const prefixes[] = {
            "hw:", "sysdefault:", "default:", NULL
        };

        unseen = hotplug_devices;
        seen = NULL;

        // Apparently there are several different ways that ALSA lists
        //  actual hardware. It could be prefixed with "hw:" or "default:"
        //  or "sysdefault:" and maybe others. Go through the list and see
        //  if we can find a preferred prefix for the system.
        for (int i = 0; hints[i]; i++) {
            char *name = ALSA_snd_device_name_get_hint(hints[i], "NAME");
            if (!name) {
                continue;
            }

            if (SDL_strcmp(name, "default") == 0) {
                if (has_default < 0) {
                    has_default = i;
                }
            } else {
                for (int j = 0; prefixes[j]; j++) {
                    const char *prefix = prefixes[j];
                    const size_t prefixlen = SDL_strlen(prefix);
                    if (SDL_strncmp(name, prefix, prefixlen) == 0) {
                        if (j < bestmatch) {
                            bestmatch = j;
                            match = prefix;
                            match_len = prefixlen;
                        }
                    }
                }

                free(name); // This should NOT be SDL_free()
            }
        }

        // look through the list of device names to find matches
        if (match || (has_default >= 0)) {  // did we find a device name prefix we like at all...?
            for (int i = 0; hints[i]; i++) {
                char *name = ALSA_snd_device_name_get_hint(hints[i], "NAME");
                if (!name) {
                    continue;
                }

                // only want physical hardware interfaces
                const SDL_bool is_default = (has_default == i);
                if (is_default || (match && SDL_strncmp(name, match, match_len) == 0)) {
                    char *ioid = ALSA_snd_device_name_get_hint(hints[i], "IOID");
                    const SDL_bool isoutput = (!ioid) || (SDL_strcmp(ioid, "Output") == 0);
                    const SDL_bool isinput = (!ioid) || (SDL_strcmp(ioid, "Input") == 0);
                    SDL_bool have_output = SDL_FALSE;
                    SDL_bool have_input = SDL_FALSE;

                    free(ioid); // This should NOT be SDL_free()

                    if (!isoutput && !isinput) {
                        free(name); // This should NOT be SDL_free()
                        continue;
                    }

                    if (is_default) {
                        if (has_default_playback && isoutput) {
                            *has_default_playback = SDL_TRUE;
                        } else if (has_default_recording && isinput) {
                            *has_default_recording = SDL_TRUE;
                        }
                        free(name); // This should NOT be SDL_free()
                        continue;
                    }

                    ALSA_Device *prev = NULL;
                    ALSA_Device *next;
                    for (ALSA_Device *dev = unseen; dev; dev = next) {
                        next = dev->next;
                        if ((SDL_strcmp(dev->name, name) == 0) && (((isinput) && dev->recording) || ((isoutput) && !dev->recording))) {
                            if (prev) {
                                prev->next = next;
                            } else {
                                unseen = next;
                            }
                            dev->next = seen;
                            seen = dev;
                            if (isinput) {
                                have_input = SDL_TRUE;
                            }
                            if (isoutput) {
                                have_output = SDL_TRUE;
                            }
                        } else {
                            prev = dev;
                        }
                    }

                    if (isinput && !have_input) {
                        add_device(SDL_TRUE, name, hints[i], &seen);
                    }
                    if (isoutput && !have_output) {
                        add_device(SDL_FALSE, name, hints[i], &seen);
                    }
                }

                free(name); // This should NOT be SDL_free()
            }
        }

        ALSA_snd_device_name_free_hint(hints);

        hotplug_devices = seen; // now we have a known-good list of attached devices.

        // report anything still in unseen as removed.
        ALSA_Device *next = NULL;
        for (ALSA_Device *dev = unseen; dev; dev = next) {
            //SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "ALSA: removing %s device '%s'", dev->recording ? "recording" : "playback", dev->name);
            next = dev->next;
            SDL_AudioDeviceDisconnected(SDL_FindPhysicalAudioDeviceByHandle(dev));
            SDL_free(dev->name);
            SDL_free(dev);
        }
    }
}

#if SDL_ALSA_HOTPLUG_THREAD
static SDL_AtomicInt ALSA_hotplug_shutdown;
static SDL_Thread *ALSA_hotplug_thread;

static int SDLCALL ALSA_HotplugThread(void *arg)
{
    SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);

    while (!SDL_AtomicGet(&ALSA_hotplug_shutdown)) {
        // Block awhile before checking again, unless we're told to stop.
        const Uint64 ticks = SDL_GetTicks() + 5000;
        while (!SDL_AtomicGet(&ALSA_hotplug_shutdown) && SDL_GetTicks() < ticks) {
            SDL_Delay(100);
        }

        ALSA_HotplugIteration(NULL, NULL); // run the check.
    }

    return 0;
}
#endif

static void ALSA_DetectDevices(SDL_AudioDevice **default_playback, SDL_AudioDevice **default_recording)
{
    // ALSA doesn't have a concept of a changeable default device, afaik, so we expose a generic default
    // device here. It's the best we can do at this level.
    SDL_bool has_default_playback = SDL_FALSE, has_default_recording = SDL_FALSE;
    ALSA_HotplugIteration(&has_default_playback, &has_default_recording); // run once now before a thread continues to check.
    if (has_default_playback) {
        *default_playback = SDL_AddAudioDevice(/*recording=*/SDL_FALSE, "ALSA default playback device", NULL, (void*)&default_playback_handle);
    }
    if (has_default_recording) {
        *default_recording = SDL_AddAudioDevice(/*recording=*/SDL_TRUE, "ALSA default recording device", NULL, (void*)&default_recording_handle);
    }

#if SDL_ALSA_HOTPLUG_THREAD
    SDL_AtomicSet(&ALSA_hotplug_shutdown, 0);
    ALSA_hotplug_thread = SDL_CreateThread(ALSA_HotplugThread, "SDLHotplugALSA", NULL);
    // if the thread doesn't spin, oh well, you just don't get further hotplug events.
#endif
}

static void ALSA_DeinitializeStart(void)
{
    ALSA_Device *dev;
    ALSA_Device *next;

#if SDL_ALSA_HOTPLUG_THREAD
    if (ALSA_hotplug_thread) {
        SDL_AtomicSet(&ALSA_hotplug_shutdown, 1);
        SDL_WaitThread(ALSA_hotplug_thread, NULL);
        ALSA_hotplug_thread = NULL;
    }
#endif

    // Shutting down! Clean up any data we've gathered.
    for (dev = hotplug_devices; dev; dev = next) {
        //SDL_LogInfo(SDL_LOG_CATEGORY_AUDIO, "ALSA: at shutdown, removing %s device '%s'", dev->recording ? "recording" : "playback", dev->name);
        next = dev->next;
        SDL_free(dev->name);
        SDL_free(dev);
    }
    hotplug_devices = NULL;
}

static void ALSA_Deinitialize(void)
{
    UnloadALSALibrary();
}

static SDL_bool ALSA_Init(SDL_AudioDriverImpl *impl)
{
    if (LoadALSALibrary() < 0) {
        return SDL_FALSE;
    }

    impl->DetectDevices = ALSA_DetectDevices;
    impl->OpenDevice = ALSA_OpenDevice;
    impl->WaitDevice = ALSA_WaitDevice;
    impl->GetDeviceBuf = ALSA_GetDeviceBuf;
    impl->PlayDevice = ALSA_PlayDevice;
    impl->CloseDevice = ALSA_CloseDevice;
    impl->DeinitializeStart = ALSA_DeinitializeStart;
    impl->Deinitialize = ALSA_Deinitialize;
    impl->WaitRecordingDevice = ALSA_WaitDevice;
    impl->RecordDevice = ALSA_RecordDevice;
    impl->FlushRecording = ALSA_FlushRecording;

    impl->HasRecordingSupport = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap ALSA_bootstrap = {
    "alsa", "ALSA PCM audio", ALSA_Init, SDL_FALSE
};

#endif // SDL_AUDIO_DRIVER_ALSA
