/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2016 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_AUDIO_DRIVER_JACK

#include "SDL_assert.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_jackaudio.h"
#include "SDL_loadso.h"
#include "../../thread/SDL_systhread.h"


/* !!! FIXME: my understanding is each JACK port is a _channel_ (like: stereo, mono...)
   !!! FIXME:  and not a logical device. So we'll have to figure out:
   !!! FIXME:   a) Can there be more than one device?
   !!! FIXME:   b) If so, how do you decide what port goes to what?
   !!! FIXME: (code in BROKEN_MULTI_DEVICE blocks was written when I assumed
   !!! FIXME:  enumerating ports meant listing separate devices. As such, it's
   !!! FIXME:  incomplete, as I discovered this as I went along writing.
*/
#define BROKEN_MULTI_DEVICE 0  /* !!! FIXME */


static jack_client_t * (*JACK_jack_client_open) (const char *, jack_options_t, jack_status_t *, ...);
static int (*JACK_jack_client_close) (jack_client_t *);
static void (*JACK_jack_on_shutdown) (jack_client_t *, JackShutdownCallback, void *);
static int (*JACK_jack_activate) (jack_client_t *);
static void * (*JACK_jack_port_get_buffer) (jack_port_t *, jack_nframes_t);
static int (*JACK_jack_port_unregister) (jack_client_t *, jack_port_t *);
static void (*JACK_jack_free) (void *);
static const char ** (*JACK_jack_get_ports) (jack_client_t *, const char *, const char *, unsigned long);
static jack_nframes_t (*JACK_jack_get_sample_rate) (jack_client_t *);
static jack_nframes_t (*JACK_jack_get_buffer_size) (jack_client_t *);
static jack_port_t * (*JACK_jack_port_register) (jack_client_t *, const char *, const char *, unsigned long, unsigned long);
static const char * (*JACK_jack_port_name) (const jack_port_t *);
static int (*JACK_jack_connect) (jack_client_t *, const char *, const char *);
static int (*JACK_jack_set_process_callback) (jack_client_t *, JackProcessCallback, void *);

static int load_jack_syms(void);


#ifdef SDL_AUDIO_DRIVER_JACK_DYNAMIC

static const char *jack_library = SDL_AUDIO_DRIVER_JACK_DYNAMIC;
static void *jack_handle = NULL;

/* !!! FIXME: this is copy/pasted in several places now */
static int
load_jack_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(jack_handle, fn);
    if (*addr == NULL) {
        /* Don't call SDL_SetError(): SDL_LoadFunction already did. */
        return 0;
    }

    return 1;
}

/* cast funcs to char* first, to please GCC's strict aliasing rules. */
#define SDL_JACK_SYM(x) \
    if (!load_jack_sym(#x, (void **) (char *) &JACK_##x)) return -1

static void
UnloadJackLibrary(void)
{
    if (jack_handle != NULL) {
        SDL_UnloadObject(jack_handle);
        jack_handle = NULL;
    }
}

static int
LoadJackLibrary(void)
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

static void
UnloadJackLibrary(void)
{
}

static int
LoadJackLibrary(void)
{
    load_jack_syms();
    return 0;
}

#endif /* SDL_AUDIO_DRIVER_JACK_DYNAMIC */


static int
load_jack_syms(void)
{
    SDL_JACK_SYM(jack_client_open);
    SDL_JACK_SYM(jack_client_close);
    SDL_JACK_SYM(jack_on_shutdown);
    SDL_JACK_SYM(jack_activate);
    SDL_JACK_SYM(jack_port_get_buffer);
    SDL_JACK_SYM(jack_port_unregister);
    SDL_JACK_SYM(jack_free);
    SDL_JACK_SYM(jack_get_ports);
    SDL_JACK_SYM(jack_get_sample_rate);
    SDL_JACK_SYM(jack_get_buffer_size);
    SDL_JACK_SYM(jack_port_register);
    SDL_JACK_SYM(jack_port_name);
    SDL_JACK_SYM(jack_connect);
    SDL_JACK_SYM(jack_set_process_callback);
    return 0;
}


static jack_client_t *JACK_client = NULL;

static void
DisconnectFromJackServer(void)
{
    if (JACK_client) {
        JACK_jack_client_close(JACK_client);
        JACK_client = NULL;
    }
}

static void
jackShutdownCallback(void *arg)
{
    /* !!! FIXME: alert SDL that _every_ open device is lost here */
    fprintf(stderr, "SDL JACK FIXME: shutdown callback fired! All audio devices are lost!\n");
    fflush(stderr);
// !!! FIXME: need to put the client (and callback) in the SDL device    SDL_SemPost(this->hidden->iosem);  /* unblock the SDL thread. */
}

static int
ConnectToJackServer(void)
{
    /* !!! FIXME: we _still_ need an API to specify an app name */
    jack_status_t status;
    JACK_client = JACK_jack_client_open("SDL", JackNoStartServer, &status, NULL);
    if (JACK_client == NULL) {
        return -1;
    }

    JACK_jack_on_shutdown(JACK_client, jackShutdownCallback, NULL);

#if 0  // !!! FIXME: we need to move JACK_client into the SDL audio device.
    if (JACK_jack_activate(JACK_client) != 0) {
        DisconnectFromJackServer();
        return -1;
    }
#endif

    return 0;
}


// !!! FIXME: implement and register these!
//typedef int(* JackSampleRateCallback)(jack_nframes_t nframes, void *arg)
//typedef int(* JackBufferSizeCallback)(jack_nframes_t nframes, void *arg)

static int
jackProcessPlaybackCallback(jack_nframes_t nframes, void *arg)
{
    SDL_AudioDevice *this = (SDL_AudioDevice *) arg;
    jack_port_t **ports = this->hidden->sdlports;
    const int total_channels = this->spec.channels;
    const int total_frames = this->spec.samples;
    int channelsi;

    if (!SDL_AtomicGet(&this->enabled)) {
        /* silence the buffer to avoid repeats and corruption. */
        SDL_memset(this->hidden->iobuffer, '\0', this->spec.size);
    }

    for (channelsi = 0; channelsi < total_channels; channelsi++) {
        float *dst = (float *) JACK_jack_port_get_buffer(ports[channelsi], nframes);
        if (dst) {
            const float *src = ((float *) this->hidden->iobuffer) + channelsi;
            int framesi;
            for (framesi = 0; framesi < total_frames; framesi++) {
                *(dst++) = *src;
                src += total_channels;
            }
        }
    }

    SDL_SemPost(this->hidden->iosem);  /* tell SDL thread we're done; refill the buffer. */
    return 0;  /* success */
}


/* This function waits until it is possible to write a full sound buffer */
static void
JACK_WaitDevice(_THIS)
{
    if (SDL_AtomicGet(&this->enabled)) {
        if (SDL_SemWait(this->hidden->iosem) == -1) {
            SDL_OpenedAudioDeviceDisconnected(this);
        }
    }
}

static Uint8 *
JACK_GetDeviceBuf(_THIS)
{
    return (Uint8 *) this->hidden->iobuffer;
}


#if 0 // !!! FIXME
/* JACK thread calls this. */
static int
jackProcessCaptureCallback(jack_nframes_t nframes, void *arg)
{
    jack_port_get_buffer(
asdasd
}

/* SDL thread calls this. */
static int
JACK_CaptureFromDevice(_THIS, void *buffer, int buflen)
{
    return SDL_SemWait(this->hidden->iosem) == 0) ? buflen : -1;
}
#endif

static void
JACK_CloseDevice(_THIS)
{
    if (this->hidden->sdlports) {
        const int channels = this->spec.channels;
        int i;
        for (i = 0; i < channels; i++) {
            JACK_jack_port_unregister(JACK_client, this->hidden->sdlports[i]);
        }
        SDL_free(this->hidden->sdlports);
    }

    if (this->hidden->iosem) {
        SDL_DestroySemaphore(this->hidden->iosem);
    }

    if (this->hidden->devports) {
        JACK_jack_free(this->hidden->devports);
    }

    SDL_free(this->hidden->iobuffer);
}

static int
JACK_OpenDevice(_THIS, void *handle, const char *devname, int iscapture)
{
    /* Note that JACK uses "output" for capture devices (they output audio
        data to us) and "input" for playback (we input audio data to them).
        Likewise, SDL's playback port will be "output" (we write data out)
        and capture will be "input" (we read data in). */
    const unsigned long sysportflags = iscapture ? JackPortIsOutput : JackPortIsInput;
    const unsigned long sdlportflags = iscapture ? JackPortIsInput : JackPortIsOutput;
    const char *sdlportstr = iscapture ? "input" : "output";
    const char **devports = NULL;
    int channels = 0;
    int i;

    /* Initialize all variables that we clean on shutdown */
    this->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, sizeof (*this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    devports = JACK_jack_get_ports(JACK_client, NULL, NULL, JackPortIsPhysical | sysportflags);
    this->hidden->devports = devports;
    if (!devports || !devports[0]) {
        return SDL_SetError("No physical JACK ports available");
    }

    while (devports[++channels]) {
        /* spin to count devports */
    }

    /* !!! FIXME: docs say about buffer size: "This size may change, clients that depend on it must register a bufsize_callback so they will be notified if it does." */

    /* Jack pretty much demands what it wants. */
    this->spec.format = AUDIO_F32SYS;
    this->spec.freq = JACK_jack_get_sample_rate(JACK_client);
    this->spec.channels = channels;
    this->spec.samples = JACK_jack_get_buffer_size(JACK_client);

    SDL_CalculateAudioSpec(&this->spec);

    this->hidden->iosem = SDL_CreateSemaphore(0);
    if (!this->hidden->iosem) {
        return -1;  /* error was set by SDL_CreateSemaphore */
    }

    this->hidden->iobuffer = (float *) SDL_calloc(1, this->spec.size);
    if (!this->hidden->iobuffer) {
        return SDL_OutOfMemory();
    }

    /* Build SDL's ports, which we will connect to the device ports. */
    this->hidden->sdlports = (jack_port_t **) SDL_calloc(channels, sizeof (jack_port_t *));
    if (this->hidden->sdlports == NULL) {
        return SDL_OutOfMemory();
    }

    if (JACK_jack_set_process_callback(JACK_client, jackProcessPlaybackCallback, this) != 0) {
        return SDL_SetError("JACK: Couldn't set process callback");
    }

    for (i = 0; i < channels; i++) {
        char portname[32];
        SDL_snprintf(portname, sizeof (portname), "sdl_jack_%s_%d", sdlportstr, i);
        this->hidden->sdlports[i] = JACK_jack_port_register(JACK_client, portname, JACK_DEFAULT_AUDIO_TYPE, sdlportflags, 0);
        if (this->hidden->sdlports[i] == NULL) {
            return SDL_SetError("jack_port_register failed");
        }
    }

    if (JACK_jack_activate(JACK_client) != 0) {
        return SDL_SetError("jack_activate failed");
    }

    /* once activated, we can connect all the ports. */
    for (i = 0; i < channels; i++) {
        char portname[32];
        SDL_snprintf(portname, sizeof (portname), "sdl_jack_%s_%d", sdlportstr, i);
        const char *sdlport = JACK_jack_port_name(this->hidden->sdlports[i]);
        const char *srcport = iscapture ? devports[i] : sdlport;
        const char *dstport = iscapture ? sdlport : devports[i];
        if (JACK_jack_connect(JACK_client, srcport, dstport) != 0) {
            return SDL_SetError("Couldn't connect JACK ports: %s => %s", srcport, dstport);
        }
    }

    /* don't need these anymore. */
    this->hidden->devports = NULL;
    JACK_jack_free(devports);

    /* We're ready to rock and roll. :-) */
    return 0;
}

#if BROKEN_MULTI_DEVICE  /* !!! FIXME */
static void
JackHotplugCallback(jack_port_id_t port_id, int register, void *arg)
{
    JackPortFlags flags;
    jack_port_t *port = JACK_jack_port_by_id(JACK_client, port_id);
    SDL_bool iscapture;
    const char *name;

    if (!port) {
        return;
    }

    name = JACK_jack_port_name(port);
    if (!name) {
        return;
    }

    flags = JACK_jack_port_flags(port);
    if ((flags & JackPortIsPhysical) == 0) {
        return;  /* not a physical device, don't care. */
    }


    if ((flags & JackPortIsInput|JackPortIsOutput) == 0) {
        return;  /* no input OR output? Don't care...? */
    }

    /* make sure it's not both, I guess. */
    SDL_assert((flags & JackPortIsInput|JackPortIsOutput) != (JackPortIsInput|JackPortIsOutput));

    /* JACK uses "output" for capture devices (they output audio data to us)
        and "input" for playback (we input audio data to them) */
    iscapture = ((flags & JackPortIsOutput) != 0);
    if (register) {
        SDL_AddAudioDevice(iscapture, name, port);
    } else {
        SDL_RemoveAudioDevice(iscapture, port);
    }
}

static void
JackEnumerateDevices(const SDL_bool iscapture)
{
    const JackPortFlags flags = (iscapture ? JackPortIsOutput : JackPortIsInput);
    const char **ports = JACK_jack_get_ports(JACK_client, NULL, NULL,
                                        JackPortIsPhysical | flags);
    const char **i;

    if (!ports) {
        return;
    }

    for (i = ports; *i != NULL; i++) {
        jack_port_t *port = JACK_jack_port_by_name(JACK_client, *i);
        if (port != NULL) {
            SDL_AddAudioDevice(iscapture, *i, port);
        }
    }

    JACK_jack_free(ports);
}

static void
JACK_DetectDevices()
{
    JackEnumerateDevices(SDL_FALSE);
    JackEnumerateDevices(SDL_TRUE);

    /* make JACK fire this callback automatically from now on. */
    JACK_jack_set_port_registration_callback(JACK_client, JackHotplugCallback, NULL);
}
#endif  /* BROKEN_MULTI_DEVICE */


static void
JACK_Deinitialize(void)
{
    DisconnectFromJackServer();
    UnloadJackLibrary();
}

static int
JACK_Init(SDL_AudioDriverImpl * impl)
{
    if (LoadJackLibrary() < 0) {
        return 0;
    }

    if (ConnectToJackServer() < 0) {
        UnloadJackLibrary();
        return 0;
    }

    /* Set the function pointers */

    #if BROKEN_MULTI_DEVICE  /* !!! FIXME */
    impl->DetectDevices = JACK_DetectDevices;
    #else
    impl->OnlyHasDefaultOutputDevice = SDL_TRUE;
    // !!! FIXME impl->OnlyHasDefaultCaptureDevice = SDL_TRUE;
    #endif

    impl->OpenDevice = JACK_OpenDevice;
    impl->WaitDevice = JACK_WaitDevice;
    impl->GetDeviceBuf = JACK_GetDeviceBuf;
    impl->CloseDevice = JACK_CloseDevice;
    impl->Deinitialize = JACK_Deinitialize;
    // !!! FIXME impl->CaptureFromDevice = JACK_CaptureFromDevice;
    // !!! FIXME impl->HasCaptureSupport = SDL_TRUE;

    return 1;   /* this audio target is available. */
}

AudioBootStrap JACK_bootstrap = {
    "jack", "JACK Audio Connection Kit", JACK_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_JACK */

/* vi: set ts=4 sw=4 expandtab: */
