/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

/*
  The PulseAudio target for SDL 1.3 is based on the 1.3 arts target, with
   the appropriate parts replaced with the 1.2 PulseAudio target code. This
   was the cleanest way to move it to 1.3. The 1.2 target was written by
   Stéphan Kochen: stephan .a.t. kochen.nl
*/
#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_PULSEAUDIO

/* Allow access to a raw mixing buffer */

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "SDL_pulseaudio.h"
#include "SDL_loadso.h"

#if (PA_API_VERSION < 12)
/** Return non-zero if the passed state is one of the connected states */
static SDL_INLINE int PA_CONTEXT_IS_GOOD(pa_context_state_t x) {
    return
        x == PA_CONTEXT_CONNECTING ||
        x == PA_CONTEXT_AUTHORIZING ||
        x == PA_CONTEXT_SETTING_NAME ||
        x == PA_CONTEXT_READY;
}
/** Return non-zero if the passed state is one of the connected states */
static SDL_INLINE int PA_STREAM_IS_GOOD(pa_stream_state_t x) {
    return
        x == PA_STREAM_CREATING ||
        x == PA_STREAM_READY;
}
#endif /* pulseaudio <= 0.9.10 */


static const char *(*PULSEAUDIO_pa_get_library_version) (void);
static pa_simple *(*PULSEAUDIO_pa_simple_new) (const char *, const char *,
    pa_stream_direction_t, const char *, const char *, const pa_sample_spec *,
    const pa_channel_map *, const pa_buffer_attr *, int *);
static void (*PULSEAUDIO_pa_simple_free) (pa_simple *);
static pa_channel_map *(*PULSEAUDIO_pa_channel_map_init_auto) (
    pa_channel_map *, unsigned, pa_channel_map_def_t);
static const char * (*PULSEAUDIO_pa_strerror) (int);
static pa_mainloop * (*PULSEAUDIO_pa_mainloop_new) (void);
static pa_mainloop_api * (*PULSEAUDIO_pa_mainloop_get_api) (pa_mainloop *);
static int (*PULSEAUDIO_pa_mainloop_iterate) (pa_mainloop *, int, int *);
static void (*PULSEAUDIO_pa_mainloop_free) (pa_mainloop *);

static pa_operation_state_t (*PULSEAUDIO_pa_operation_get_state) (
    pa_operation *);
static void (*PULSEAUDIO_pa_operation_cancel) (pa_operation *);
static void (*PULSEAUDIO_pa_operation_unref) (pa_operation *);

static pa_context * (*PULSEAUDIO_pa_context_new) (pa_mainloop_api *,
    const char *);
static int (*PULSEAUDIO_pa_context_connect) (pa_context *, const char *,
    pa_context_flags_t, const pa_spawn_api *);
static pa_context_state_t (*PULSEAUDIO_pa_context_get_state) (pa_context *);
static void (*PULSEAUDIO_pa_context_disconnect) (pa_context *);
static void (*PULSEAUDIO_pa_context_unref) (pa_context *);

static pa_stream * (*PULSEAUDIO_pa_stream_new) (pa_context *, const char *,
    const pa_sample_spec *, const pa_channel_map *);
static int (*PULSEAUDIO_pa_stream_connect_playback) (pa_stream *, const char *,
    const pa_buffer_attr *, pa_stream_flags_t, pa_cvolume *, pa_stream *);
static pa_stream_state_t (*PULSEAUDIO_pa_stream_get_state) (pa_stream *);
static size_t (*PULSEAUDIO_pa_stream_writable_size) (pa_stream *);
static int (*PULSEAUDIO_pa_stream_write) (pa_stream *, const void *, size_t,
    pa_free_cb_t, int64_t, pa_seek_mode_t);
static pa_operation * (*PULSEAUDIO_pa_stream_drain) (pa_stream *,
    pa_stream_success_cb_t, void *);
static int (*PULSEAUDIO_pa_stream_disconnect) (pa_stream *);
static void (*PULSEAUDIO_pa_stream_unref) (pa_stream *);

static int load_pulseaudio_syms(void);


#ifdef SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC

static const char *pulseaudio_library = SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC;
static void *pulseaudio_handle = NULL;

static int
load_pulseaudio_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(pulseaudio_handle, fn);
    if (*addr == NULL) {
        /* Don't call SDL_SetError(): SDL_LoadFunction already did. */
        return 0;
    }

    return 1;
}

/* cast funcs to char* first, to please GCC's strict aliasing rules. */
#define SDL_PULSEAUDIO_SYM(x) \
    if (!load_pulseaudio_sym(#x, (void **) (char *) &PULSEAUDIO_##x)) return -1

static void
UnloadPulseAudioLibrary(void)
{
    if (pulseaudio_handle != NULL) {
        SDL_UnloadObject(pulseaudio_handle);
        pulseaudio_handle = NULL;
    }
}

static int
LoadPulseAudioLibrary(void)
{
    int retval = 0;
    if (pulseaudio_handle == NULL) {
        pulseaudio_handle = SDL_LoadObject(pulseaudio_library);
        if (pulseaudio_handle == NULL) {
            retval = -1;
            /* Don't call SDL_SetError(): SDL_LoadObject already did. */
        } else {
            retval = load_pulseaudio_syms();
            if (retval < 0) {
                UnloadPulseAudioLibrary();
            }
        }
    }
    return retval;
}

#else

#define SDL_PULSEAUDIO_SYM(x) PULSEAUDIO_##x = x

static void
UnloadPulseAudioLibrary(void)
{
}

static int
LoadPulseAudioLibrary(void)
{
    load_pulseaudio_syms();
    return 0;
}

#endif /* SDL_AUDIO_DRIVER_PULSEAUDIO_DYNAMIC */


static int
load_pulseaudio_syms(void)
{
    SDL_PULSEAUDIO_SYM(pa_get_library_version);
    SDL_PULSEAUDIO_SYM(pa_simple_new);
    SDL_PULSEAUDIO_SYM(pa_simple_free);
    SDL_PULSEAUDIO_SYM(pa_mainloop_new);
    SDL_PULSEAUDIO_SYM(pa_mainloop_get_api);
    SDL_PULSEAUDIO_SYM(pa_mainloop_iterate);
    SDL_PULSEAUDIO_SYM(pa_mainloop_free);
    SDL_PULSEAUDIO_SYM(pa_operation_get_state);
    SDL_PULSEAUDIO_SYM(pa_operation_cancel);
    SDL_PULSEAUDIO_SYM(pa_operation_unref);
    SDL_PULSEAUDIO_SYM(pa_context_new);
    SDL_PULSEAUDIO_SYM(pa_context_connect);
    SDL_PULSEAUDIO_SYM(pa_context_get_state);
    SDL_PULSEAUDIO_SYM(pa_context_disconnect);
    SDL_PULSEAUDIO_SYM(pa_context_unref);
    SDL_PULSEAUDIO_SYM(pa_stream_new);
    SDL_PULSEAUDIO_SYM(pa_stream_connect_playback);
    SDL_PULSEAUDIO_SYM(pa_stream_get_state);
    SDL_PULSEAUDIO_SYM(pa_stream_writable_size);
    SDL_PULSEAUDIO_SYM(pa_stream_write);
    SDL_PULSEAUDIO_SYM(pa_stream_drain);
    SDL_PULSEAUDIO_SYM(pa_stream_disconnect);
    SDL_PULSEAUDIO_SYM(pa_stream_unref);
    SDL_PULSEAUDIO_SYM(pa_channel_map_init_auto);
    SDL_PULSEAUDIO_SYM(pa_strerror);
    return 0;
}


/* Check to see if we can connect to PulseAudio */
static SDL_bool
CheckPulseAudioAvailable()
{
    pa_simple *s;
    pa_sample_spec ss;

    ss.format = PA_SAMPLE_S16NE;
    ss.channels = 1;
    ss.rate = 22050;

    s = PULSEAUDIO_pa_simple_new(NULL, "SDL", PA_STREAM_PLAYBACK, NULL,
                                 "Test", &ss, NULL, NULL, NULL);
    if (s) {
        PULSEAUDIO_pa_simple_free(s);
        return SDL_TRUE;
    } else {
        return SDL_FALSE;
    }
}

/* This function waits until it is possible to write a full sound buffer */
static void
PULSEAUDIO_WaitDevice(_THIS)
{
    struct SDL_PrivateAudioData *h = this->hidden;

    while(1) {
        if (PULSEAUDIO_pa_context_get_state(h->context) != PA_CONTEXT_READY ||
            PULSEAUDIO_pa_stream_get_state(h->stream) != PA_STREAM_READY ||
            PULSEAUDIO_pa_mainloop_iterate(h->mainloop, 1, NULL) < 0) {
            this->enabled = 0;
            return;
        }
        if (PULSEAUDIO_pa_stream_writable_size(h->stream) >= h->mixlen) {
            return;
        }
    }
}

static void
PULSEAUDIO_PlayDevice(_THIS)
{
    /* Write the audio data */
    struct SDL_PrivateAudioData *h = this->hidden;
    if (PULSEAUDIO_pa_stream_write(h->stream, h->mixbuf, h->mixlen, NULL, 0LL,
                                   PA_SEEK_RELATIVE) < 0) {
        this->enabled = 0;
    }
}

static void
stream_drain_complete(pa_stream *s, int success, void *userdata)
{
    /* no-op for pa_stream_drain() to use for callback. */
}

static void
PULSEAUDIO_WaitDone(_THIS)
{
    struct SDL_PrivateAudioData *h = this->hidden;
    pa_operation *o;

    o = PULSEAUDIO_pa_stream_drain(h->stream, stream_drain_complete, NULL);
    if (!o) {
        return;
    }

    while (PULSEAUDIO_pa_operation_get_state(o) != PA_OPERATION_DONE) {
        if (PULSEAUDIO_pa_context_get_state(h->context) != PA_CONTEXT_READY ||
            PULSEAUDIO_pa_stream_get_state(h->stream) != PA_STREAM_READY ||
            PULSEAUDIO_pa_mainloop_iterate(h->mainloop, 1, NULL) < 0) {
            PULSEAUDIO_pa_operation_cancel(o);
            break;
        }
    }

    PULSEAUDIO_pa_operation_unref(o);
}



static Uint8 *
PULSEAUDIO_GetDeviceBuf(_THIS)
{
    return (this->hidden->mixbuf);
}


static void
PULSEAUDIO_CloseDevice(_THIS)
{
    if (this->hidden != NULL) {
        SDL_FreeAudioMem(this->hidden->mixbuf);
        this->hidden->mixbuf = NULL;
        if (this->hidden->stream) {
            PULSEAUDIO_pa_stream_disconnect(this->hidden->stream);
            PULSEAUDIO_pa_stream_unref(this->hidden->stream);
            this->hidden->stream = NULL;
        }
        if (this->hidden->context != NULL) {
            PULSEAUDIO_pa_context_disconnect(this->hidden->context);
            PULSEAUDIO_pa_context_unref(this->hidden->context);
            this->hidden->context = NULL;
        }
        if (this->hidden->mainloop != NULL) {
            PULSEAUDIO_pa_mainloop_free(this->hidden->mainloop);
            this->hidden->mainloop = NULL;
        }
        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}


static SDL_INLINE int
squashVersion(const int major, const int minor, const int patch)
{
    return ((major & 0xFF) << 16) | ((minor & 0xFF) << 8) | (patch & 0xFF);
}

/* Workaround for older pulse: pa_context_new() must have non-NULL appname */
static const char *
getAppName(void)
{
    const char *verstr = PULSEAUDIO_pa_get_library_version();
    if (verstr != NULL) {
        int maj, min, patch;
        if (SDL_sscanf(verstr, "%d.%d.%d", &maj, &min, &patch) == 3) {
            if (squashVersion(maj, min, patch) >= squashVersion(0, 9, 15)) {
                return NULL;  /* 0.9.15+ handles NULL correctly. */
            }
        }
    }
    return "SDL Application";  /* oh well. */
}

static int
PULSEAUDIO_OpenDevice(_THIS, const char *devname, int iscapture)
{
    struct SDL_PrivateAudioData *h = NULL;
    Uint16 test_format = 0;
    pa_sample_spec paspec;
    pa_buffer_attr paattr;
    pa_channel_map pacmap;
    pa_stream_flags_t flags = 0;
    int state = 0;

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc((sizeof *this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden, 0, (sizeof *this->hidden));
    h = this->hidden;

    paspec.format = PA_SAMPLE_INVALID;

    /* Try for a closest match on audio format */
    for (test_format = SDL_FirstAudioFormat(this->spec.format);
         (paspec.format == PA_SAMPLE_INVALID) && test_format;) {
#ifdef DEBUG_AUDIO
        fprintf(stderr, "Trying format 0x%4.4x\n", test_format);
#endif
        switch (test_format) {
        case AUDIO_U8:
            paspec.format = PA_SAMPLE_U8;
            break;
        case AUDIO_S16LSB:
            paspec.format = PA_SAMPLE_S16LE;
            break;
        case AUDIO_S16MSB:
            paspec.format = PA_SAMPLE_S16BE;
            break;
        case AUDIO_S32LSB:
            paspec.format = PA_SAMPLE_S32LE;
            break;
        case AUDIO_S32MSB:
            paspec.format = PA_SAMPLE_S32BE;
            break;
        case AUDIO_F32LSB:
            paspec.format = PA_SAMPLE_FLOAT32LE;
            break;
        case AUDIO_F32MSB:
            paspec.format = PA_SAMPLE_FLOAT32BE;
            break;
        default:
            paspec.format = PA_SAMPLE_INVALID;
            break;
        }
        if (paspec.format == PA_SAMPLE_INVALID) {
            test_format = SDL_NextAudioFormat();
        }
    }
    if (paspec.format == PA_SAMPLE_INVALID) {
        PULSEAUDIO_CloseDevice(this);
        return SDL_SetError("Couldn't find any hardware audio formats");
    }
    this->spec.format = test_format;

    /* Calculate the final parameters for this audio specification */
#ifdef PA_STREAM_ADJUST_LATENCY
    this->spec.samples /= 2; /* Mix in smaller chunck to avoid underruns */
#endif
    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate mixing buffer */
    h->mixlen = this->spec.size;
    h->mixbuf = (Uint8 *) SDL_AllocAudioMem(h->mixlen);
    if (h->mixbuf == NULL) {
        PULSEAUDIO_CloseDevice(this);
        return SDL_OutOfMemory();
    }
    SDL_memset(h->mixbuf, this->spec.silence, this->spec.size);

    paspec.channels = this->spec.channels;
    paspec.rate = this->spec.freq;

    /* Reduced prebuffering compared to the defaults. */
#ifdef PA_STREAM_ADJUST_LATENCY
    /* 2x original requested bufsize */
    paattr.tlength = h->mixlen * 4;
    paattr.prebuf = -1;
    paattr.maxlength = -1;
    /* -1 can lead to pa_stream_writable_size() >= mixlen never being true */
    paattr.minreq = h->mixlen;
    flags = PA_STREAM_ADJUST_LATENCY;
#else
    paattr.tlength = h->mixlen*2;
    paattr.prebuf = h->mixlen*2;
    paattr.maxlength = h->mixlen*2;
    paattr.minreq = h->mixlen;
#endif

    /* The SDL ALSA output hints us that we use Windows' channel mapping */
    /* http://bugzilla.libsdl.org/show_bug.cgi?id=110 */
    PULSEAUDIO_pa_channel_map_init_auto(&pacmap, this->spec.channels,
                                        PA_CHANNEL_MAP_WAVEEX);

    /* Set up a new main loop */
    if (!(h->mainloop = PULSEAUDIO_pa_mainloop_new())) {
        PULSEAUDIO_CloseDevice(this);
        return SDL_SetError("pa_mainloop_new() failed");
    }

    h->mainloop_api = PULSEAUDIO_pa_mainloop_get_api(h->mainloop);
    h->context = PULSEAUDIO_pa_context_new(h->mainloop_api, getAppName());
    if (!h->context) {
        PULSEAUDIO_CloseDevice(this);
        return SDL_SetError("pa_context_new() failed");
    }

    /* Connect to the PulseAudio server */
    if (PULSEAUDIO_pa_context_connect(h->context, NULL, 0, NULL) < 0) {
        PULSEAUDIO_CloseDevice(this);
        return SDL_SetError("Could not setup connection to PulseAudio");
    }

    do {
        if (PULSEAUDIO_pa_mainloop_iterate(h->mainloop, 1, NULL) < 0) {
            PULSEAUDIO_CloseDevice(this);
            return SDL_SetError("pa_mainloop_iterate() failed");
        }
        state = PULSEAUDIO_pa_context_get_state(h->context);
        if (!PA_CONTEXT_IS_GOOD(state)) {
            PULSEAUDIO_CloseDevice(this);
            return SDL_SetError("Could not connect to PulseAudio");
        }
    } while (state != PA_CONTEXT_READY);

    h->stream = PULSEAUDIO_pa_stream_new(
        h->context,
        "Simple DirectMedia Layer", /* stream description */
        &paspec,    /* sample format spec */
        &pacmap     /* channel map */
        );

    if (h->stream == NULL) {
        PULSEAUDIO_CloseDevice(this);
        return SDL_SetError("Could not set up PulseAudio stream");
    }

    if (PULSEAUDIO_pa_stream_connect_playback(h->stream, NULL, &paattr, flags,
            NULL, NULL) < 0) {
        PULSEAUDIO_CloseDevice(this);
        return SDL_SetError("Could not connect PulseAudio stream");
    }

    do {
        if (PULSEAUDIO_pa_mainloop_iterate(h->mainloop, 1, NULL) < 0) {
            PULSEAUDIO_CloseDevice(this);
            return SDL_SetError("pa_mainloop_iterate() failed");
        }
        state = PULSEAUDIO_pa_stream_get_state(h->stream);
        if (!PA_STREAM_IS_GOOD(state)) {
            PULSEAUDIO_CloseDevice(this);
            return SDL_SetError("Could not create to PulseAudio stream");
        }
    } while (state != PA_STREAM_READY);

    /* We're ready to rock and roll. :-) */
    return 0;
}


static void
PULSEAUDIO_Deinitialize(void)
{
    UnloadPulseAudioLibrary();
}

static int
PULSEAUDIO_Init(SDL_AudioDriverImpl * impl)
{
    if (LoadPulseAudioLibrary() < 0) {
        return 0;
    }

    if (!CheckPulseAudioAvailable()) {
        UnloadPulseAudioLibrary();
        return 0;
    }

    /* Set the function pointers */
    impl->OpenDevice = PULSEAUDIO_OpenDevice;
    impl->PlayDevice = PULSEAUDIO_PlayDevice;
    impl->WaitDevice = PULSEAUDIO_WaitDevice;
    impl->GetDeviceBuf = PULSEAUDIO_GetDeviceBuf;
    impl->CloseDevice = PULSEAUDIO_CloseDevice;
    impl->WaitDone = PULSEAUDIO_WaitDone;
    impl->Deinitialize = PULSEAUDIO_Deinitialize;
    impl->OnlyHasDefaultOutputDevice = 1;

    return 1;   /* this audio target is available. */
}


AudioBootStrap PULSEAUDIO_bootstrap = {
    "pulseaudio", "PulseAudio", PULSEAUDIO_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_PULSEAUDIO */

/* vi: set ts=4 sw=4 expandtab: */
