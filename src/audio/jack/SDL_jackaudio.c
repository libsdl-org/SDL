/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_AUDIO_DRIVER_JACK

#include "../SDL_audio_c.h"
#include "SDL_jackaudio.h"
#include "../../thread/SDL_systhread.h"

static jack_client_t *(*JACK_jack_client_open)(const char *, jack_options_t, jack_status_t *, ...);
static int (*JACK_jack_client_close)(jack_client_t *);
static void (*JACK_jack_on_shutdown)(jack_client_t *, JackShutdownCallback, void *);
static int (*JACK_jack_activate)(jack_client_t *);
static int (*JACK_jack_deactivate)(jack_client_t *);
static void *(*JACK_jack_port_get_buffer)(jack_port_t *, jack_nframes_t);
static int (*JACK_jack_port_unregister)(jack_client_t *, jack_port_t *);
static void (*JACK_jack_free)(void *);
static const char **(*JACK_jack_get_ports)(jack_client_t *, const char *, const char *, unsigned long);
static jack_nframes_t (*JACK_jack_get_sample_rate)(jack_client_t *);
static jack_nframes_t (*JACK_jack_get_buffer_size)(jack_client_t *);
static jack_port_t *(*JACK_jack_port_register)(jack_client_t *, const char *, const char *, unsigned long, unsigned long);
static jack_port_t *(*JACK_jack_port_by_name)(jack_client_t *, const char *);
static const char *(*JACK_jack_port_name)(const jack_port_t *);
static const char *(*JACK_jack_port_type)(const jack_port_t *);
static int (*JACK_jack_connect)(jack_client_t *, const char *, const char *);
static int (*JACK_jack_set_process_callback)(jack_client_t *, JackProcessCallback, void *);

static int load_jack_syms(void);

#ifdef SDL_AUDIO_DRIVER_JACK_DYNAMIC

static const char *jack_library = SDL_AUDIO_DRIVER_JACK_DYNAMIC;
static void *jack_handle = NULL;

/* !!! FIXME: this is copy/pasted in several places now */
static int load_jack_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(jack_handle, fn);
    if (*addr == NULL) {
        /* Don't call SDL_SetError(): SDL_LoadFunction already did. */
        return 0;
    }

    return 1;
}

/* cast funcs to char* first, to please GCC's strict aliasing rules. */
#define SDL_JACK_SYM(x)                                 \
    if (!load_jack_sym(#x, (void **)(char *)&JACK_##x)) \
    return -1

static void UnloadJackLibrary(void)
{
    if (jack_handle != NULL) {
        SDL_UnloadObject(jack_handle);
        jack_handle = NULL;
    }
}

static int LoadJackLibrary(void)
{
    int retval = 0;
    if (jack_handle == NULL) {
        jack_handle = SDL_LoadObject(jack_library);
        if (jack_handle == NULL) {
            retval = -1;
            /* Don't call SDL_SetError(): SDL_LoadObject already did. */
        } else {
            retval = load_jack_syms();
            if (retval < 0) {
                UnloadJackLibrary();
            }
        }
    }
    return retval;
}

#else

#define SDL_JACK_SYM(x) JACK_##x = x

static void UnloadJackLibrary(void)
{
}

static int LoadJackLibrary(void)
{
    load_jack_syms();
    return 0;
}

#endif /* SDL_AUDIO_DRIVER_JACK_DYNAMIC */

static int load_jack_syms(void)
{
    SDL_JACK_SYM(jack_client_open);
    SDL_JACK_SYM(jack_client_close);
    SDL_JACK_SYM(jack_on_shutdown);
    SDL_JACK_SYM(jack_activate);
    SDL_JACK_SYM(jack_deactivate);
    SDL_JACK_SYM(jack_port_get_buffer);
    SDL_JACK_SYM(jack_port_unregister);
    SDL_JACK_SYM(jack_free);
    SDL_JACK_SYM(jack_get_ports);
    SDL_JACK_SYM(jack_get_sample_rate);
    SDL_JACK_SYM(jack_get_buffer_size);
    SDL_JACK_SYM(jack_port_register);
    SDL_JACK_SYM(jack_port_by_name);
    SDL_JACK_SYM(jack_port_name);
    SDL_JACK_SYM(jack_port_type);
    SDL_JACK_SYM(jack_connect);
    SDL_JACK_SYM(jack_set_process_callback);

    return 0;
}

static void jackShutdownCallback(void *arg) /* JACK went away; device is lost. */
{
    SDL_AudioDeviceDisconnected((SDL_AudioDevice *)arg);
}

// !!! FIXME: implement and register these!
// typedef int(* JackSampleRateCallback)(jack_nframes_t nframes, void *arg)
// typedef int(* JackBufferSizeCallback)(jack_nframes_t nframes, void *arg)

static int jackProcessPlaybackCallback(jack_nframes_t nframes, void *arg)
{
    SDL_assert(nframes == ((SDL_AudioDevice *)arg)->sample_frames);
    SDL_OutputAudioThreadIterate((SDL_AudioDevice *)arg);
    return 0;
}

static int JACK_PlayDevice(SDL_AudioDevice *device, const Uint8 *ui8buffer, int buflen)
{
    const float *buffer = (float *) ui8buffer;
    jack_port_t **ports = device->hidden->sdlports;
    const int total_channels = device->spec.channels;
    const int total_frames = device->sample_frames;
    const jack_nframes_t nframes = (jack_nframes_t) device->sample_frames;

    for (int channelsi = 0; channelsi < total_channels; channelsi++) {
        float *dst = (float *)JACK_jack_port_get_buffer(ports[channelsi], nframes);
        if (dst) {
            const float *src = buffer + channelsi;
            for (int framesi = 0; framesi < total_frames; framesi++) {
                *(dst++) = *src;
                src += total_channels;
            }
        }
    }

    return 0;
}

static Uint8 *JACK_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return (Uint8 *)device->hidden->iobuffer;
}

static int jackProcessCaptureCallback(jack_nframes_t nframes, void *arg)
{
    SDL_assert(nframes == ((SDL_AudioDevice *)arg)->sample_frames);
    SDL_CaptureAudioThreadIterate((SDL_AudioDevice *)arg);
    return 0;
}

static int JACK_CaptureFromDevice(SDL_AudioDevice *device, void *vbuffer, int buflen)
{
    float *buffer = (float *) vbuffer;
    jack_port_t **ports = device->hidden->sdlports;
    const int total_channels = device->spec.channels;
    const int total_frames = device->sample_frames;
    const jack_nframes_t nframes = (jack_nframes_t) device->sample_frames;

    for (int channelsi = 0; channelsi < total_channels; channelsi++) {
        const float *src = (const float *)JACK_jack_port_get_buffer(ports[channelsi], nframes);
        if (src) {
            float *dst = buffer + channelsi;
            for (int framesi = 0; framesi < total_frames; framesi++) {
                *dst = *(src++);
                dst += total_channels;
            }
        }
    }

    return buflen;
}

static void JACK_FlushCapture(SDL_AudioDevice *device)
{
    // do nothing, the data will just be replaced next callback.
}

static void JACK_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->client) {
            JACK_jack_deactivate(device->hidden->client);

            if (device->hidden->sdlports) {
                const int channels = device->spec.channels;
                int i;
                for (i = 0; i < channels; i++) {
                    JACK_jack_port_unregister(device->hidden->client, device->hidden->sdlports[i]);
                }
                SDL_free(device->hidden->sdlports);
            }

            JACK_jack_client_close(device->hidden->client);
        }

        SDL_free(device->hidden->iobuffer);
        SDL_free(device->hidden);
        device->hidden = NULL;

        SDL_AudioThreadFinalize(device);
    }
}

// !!! FIXME: unify this (PulseAudio has a getAppName, Pipewire has a thing, etc
static const char *GetJackAppName(void)
{
    const char *retval = SDL_GetHint(SDL_HINT_AUDIO_DEVICE_APP_NAME);
    if (retval && *retval) {
        return retval;
    }
    retval = SDL_GetHint(SDL_HINT_APP_NAME);
    if (retval && *retval) {
        return retval;
    }
    return "SDL Application";
}

static int JACK_OpenDevice(SDL_AudioDevice *device)
{
    /* Note that JACK uses "output" for capture devices (they output audio
        data to us) and "input" for playback (we input audio data to them).
        Likewise, SDL's playback port will be "output" (we write data out)
        and capture will be "input" (we read data in). */
    SDL_bool iscapture = device->iscapture;
    const unsigned long sysportflags = iscapture ? JackPortIsOutput : JackPortIsInput;
    const unsigned long sdlportflags = iscapture ? JackPortIsInput : JackPortIsOutput;
    const JackProcessCallback callback = iscapture ? jackProcessCaptureCallback : jackProcessPlaybackCallback;
    const char *sdlportstr = iscapture ? "input" : "output";
    const char **devports = NULL;
    int *audio_ports;
    jack_client_t *client = NULL;
    jack_status_t status;
    int channels = 0;
    int ports = 0;
    int i;

    /* Initialize all variables that we clean on shutdown */
    device->hidden = (struct SDL_PrivateAudioData *)SDL_calloc(1, sizeof(*device->hidden));
    if (device->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    client = JACK_jack_client_open(GetJackAppName(), JackNoStartServer, &status, NULL);
    device->hidden->client = client;
    if (client == NULL) {
        return SDL_SetError("Can't open JACK client");
    }

    devports = JACK_jack_get_ports(client, NULL, NULL, JackPortIsPhysical | sysportflags);
    if (devports == NULL || !devports[0]) {
        return SDL_SetError("No physical JACK ports available");
    }

    while (devports[++ports]) {
        /* spin to count devports */
    }

    /* Filter out non-audio ports */
    audio_ports = SDL_calloc(ports, sizeof(*audio_ports));
    for (i = 0; i < ports; i++) {
        const jack_port_t *dport = JACK_jack_port_by_name(client, devports[i]);
        const char *type = JACK_jack_port_type(dport);
        const int len = SDL_strlen(type);
        /* See if type ends with "audio" */
        if (len >= 5 && !SDL_memcmp(type + len - 5, "audio", 5)) {
            audio_ports[channels++] = i;
        }
    }
    if (channels == 0) {
        SDL_free(audio_ports);
        return SDL_SetError("No physical JACK ports available");
    }

    /* !!! FIXME: docs say about buffer size: "This size may change, clients that depend on it must register a bufsize_callback so they will be notified if it does." */

    /* Jack pretty much demands what it wants. */
    device->spec.format = SDL_AUDIO_F32;
    device->spec.freq = JACK_jack_get_sample_rate(client);
    device->spec.channels = channels;
    device->sample_frames = JACK_jack_get_buffer_size(client);

    SDL_UpdatedAudioDeviceFormat(device);

    if (!device->iscapture) {
        device->hidden->iobuffer = (float *)SDL_calloc(1, device->buffer_size);
        if (!device->hidden->iobuffer) {
            SDL_free(audio_ports);
            return SDL_OutOfMemory();
        }
    }

    /* Build SDL's ports, which we will connect to the device ports. */
    device->hidden->sdlports = (jack_port_t **)SDL_calloc(channels, sizeof(jack_port_t *));
    if (device->hidden->sdlports == NULL) {
        SDL_free(audio_ports);
        return SDL_OutOfMemory();
    }

    for (i = 0; i < channels; i++) {
        char portname[32];
        (void)SDL_snprintf(portname, sizeof(portname), "sdl_jack_%s_%d", sdlportstr, i);
        device->hidden->sdlports[i] = JACK_jack_port_register(client, portname, JACK_DEFAULT_AUDIO_TYPE, sdlportflags, 0);
        if (device->hidden->sdlports[i] == NULL) {
            SDL_free(audio_ports);
            return SDL_SetError("jack_port_register failed");
        }
    }

    if (JACK_jack_set_process_callback(client, callback, device) != 0) {
        SDL_free(audio_ports);
        return SDL_SetError("JACK: Couldn't set process callback");
    }

    JACK_jack_on_shutdown(client, jackShutdownCallback, device);

    if (JACK_jack_activate(client) != 0) {
        SDL_free(audio_ports);
        return SDL_SetError("Failed to activate JACK client");
    }

    /* once activated, we can connect all the ports. */
    for (i = 0; i < channels; i++) {
        const char *sdlport = JACK_jack_port_name(device->hidden->sdlports[i]);
        const char *srcport = iscapture ? devports[audio_ports[i]] : sdlport;
        const char *dstport = iscapture ? sdlport : devports[audio_ports[i]];
        if (JACK_jack_connect(client, srcport, dstport) != 0) {
            SDL_free(audio_ports);
            return SDL_SetError("Couldn't connect JACK ports: %s => %s", srcport, dstport);
        }
    }

    /* don't need these anymore. */
    JACK_jack_free(devports);
    SDL_free(audio_ports);

    /* We're ready to rock and roll. :-) */
    return 0;
}

static void JACK_Deinitialize(void)
{
    UnloadJackLibrary();
}

static SDL_bool JACK_Init(SDL_AudioDriverImpl *impl)
{
    if (LoadJackLibrary() < 0) {
        return SDL_FALSE;
    } else {
        /* Make sure a JACK server is running and available. */
        jack_status_t status;
        jack_client_t *client = JACK_jack_client_open("SDL", JackNoStartServer, &status, NULL);
        if (client == NULL) {
            UnloadJackLibrary();
            return SDL_FALSE;
        }
        JACK_jack_client_close(client);
    }

    /* Set the function pointers */
    impl->OpenDevice = JACK_OpenDevice;
    impl->GetDeviceBuf = JACK_GetDeviceBuf;
    impl->PlayDevice = JACK_PlayDevice;
    impl->CloseDevice = JACK_CloseDevice;
    impl->Deinitialize = JACK_Deinitialize;
    impl->CaptureFromDevice = JACK_CaptureFromDevice;
    impl->FlushCapture = JACK_FlushCapture;
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    impl->OnlyHasDefaultCaptureDevice = SDL_TRUE;
    impl->HasCaptureSupport = SDL_TRUE;
    impl->ProvidesOwnCallbackThread = SDL_TRUE;

    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap JACK_bootstrap = {
    "jack", "JACK Audio Connection Kit", JACK_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_JACK */
