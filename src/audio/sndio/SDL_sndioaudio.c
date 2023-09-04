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

#ifdef SDL_AUDIO_DRIVER_SNDIO

// OpenBSD sndio target

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <poll.h>
#include <unistd.h>

#include "../SDL_audio_c.h"
#include "SDL_sndioaudio.h"

#ifdef SDL_AUDIO_DRIVER_SNDIO_DYNAMIC
#endif

#ifndef INFTIM
#define INFTIM -1
#endif

#ifndef SIO_DEVANY
#define SIO_DEVANY "default"
#endif

static struct sio_hdl *(*SNDIO_sio_open)(const char *, unsigned int, int);
static void (*SNDIO_sio_close)(struct sio_hdl *);
static int (*SNDIO_sio_setpar)(struct sio_hdl *, struct sio_par *);
static int (*SNDIO_sio_getpar)(struct sio_hdl *, struct sio_par *);
static int (*SNDIO_sio_start)(struct sio_hdl *);
static int (*SNDIO_sio_stop)(struct sio_hdl *);
static size_t (*SNDIO_sio_read)(struct sio_hdl *, void *, size_t);
static size_t (*SNDIO_sio_write)(struct sio_hdl *, const void *, size_t);
static int (*SNDIO_sio_nfds)(struct sio_hdl *);
static int (*SNDIO_sio_pollfd)(struct sio_hdl *, struct pollfd *, int);
static int (*SNDIO_sio_revents)(struct sio_hdl *, struct pollfd *);
static int (*SNDIO_sio_eof)(struct sio_hdl *);
static void (*SNDIO_sio_initpar)(struct sio_par *);

#ifdef SDL_AUDIO_DRIVER_SNDIO_DYNAMIC
static const char *sndio_library = SDL_AUDIO_DRIVER_SNDIO_DYNAMIC;
static void *sndio_handle = NULL;

static int load_sndio_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(sndio_handle, fn);
    if (*addr == NULL) {
        return 0;  // Don't call SDL_SetError(): SDL_LoadFunction already did.
    }

    return 1;
}

// cast funcs to char* first, to please GCC's strict aliasing rules.
#define SDL_SNDIO_SYM(x)                                  \
    if (!load_sndio_sym(#x, (void **)(char *)&SNDIO_##x)) \
    return -1
#else
#define SDL_SNDIO_SYM(x) SNDIO_##x = x
#endif

static int load_sndio_syms(void)
{
    SDL_SNDIO_SYM(sio_open);
    SDL_SNDIO_SYM(sio_close);
    SDL_SNDIO_SYM(sio_setpar);
    SDL_SNDIO_SYM(sio_getpar);
    SDL_SNDIO_SYM(sio_start);
    SDL_SNDIO_SYM(sio_stop);
    SDL_SNDIO_SYM(sio_read);
    SDL_SNDIO_SYM(sio_write);
    SDL_SNDIO_SYM(sio_nfds);
    SDL_SNDIO_SYM(sio_pollfd);
    SDL_SNDIO_SYM(sio_revents);
    SDL_SNDIO_SYM(sio_eof);
    SDL_SNDIO_SYM(sio_initpar);
    return 0;
}

#undef SDL_SNDIO_SYM

#ifdef SDL_AUDIO_DRIVER_SNDIO_DYNAMIC

static void UnloadSNDIOLibrary(void)
{
    if (sndio_handle != NULL) {
        SDL_UnloadObject(sndio_handle);
        sndio_handle = NULL;
    }
}

static int LoadSNDIOLibrary(void)
{
    int retval = 0;
    if (sndio_handle == NULL) {
        sndio_handle = SDL_LoadObject(sndio_library);
        if (sndio_handle == NULL) {
            retval = -1;  // Don't call SDL_SetError(): SDL_LoadObject already did.
        } else {
            retval = load_sndio_syms();
            if (retval < 0) {
                UnloadSNDIOLibrary();
            }
        }
    }
    return retval;
}

#else

static void UnloadSNDIOLibrary(void)
{
}

static int LoadSNDIOLibrary(void)
{
    load_sndio_syms();
    return 0;
}

#endif // SDL_AUDIO_DRIVER_SNDIO_DYNAMIC

static void SNDIO_WaitDevice(SDL_AudioDevice *device)
{
    const SDL_bool iscapture = device->iscapture;

    while (!SDL_AtomicGet(&device->shutdown)) {
        if (SNDIO_sio_eof(device->hidden->dev)) {
            SDL_AudioDeviceDisconnected(device);
            return;
        }

        const int nfds = SNDIO_sio_pollfd(device->hidden->dev, device->hidden->pfd, iscapture ? POLLIN : POLLOUT);
        if (nfds <= 0 || poll(device->hidden->pfd, nfds, 10) < 0) {
            SDL_AudioDeviceDisconnected(device);
            return;
        }

        const int revents = SNDIO_sio_revents(device->hidden->dev, device->hidden->pfd);
        if (iscapture && (revents & POLLIN)) {
            return;
        } else if (!iscapture && (revents & POLLOUT)) {
            return;
        } else if (revents & POLLHUP) {
            SDL_AudioDeviceDisconnected(device);
            return;
        }
    }
}

static int SNDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    // !!! FIXME: this should be non-blocking so we can check device->shutdown.
    // this is set to blocking, because we _have_ to send the entire buffer down, but hopefully WaitDevice took most of the delay time.
    if (SNDIO_sio_write(device->hidden->dev, buffer, buflen) != buflen) {
        return -1;  // If we couldn't write, assume fatal error for now
    }
#ifdef DEBUG_AUDIO
    fprintf(stderr, "Wrote %d bytes of audio data\n", written);
#endif
    return 0;
}

static int SNDIO_CaptureFromDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    // We set capture devices non-blocking; this can safely return 0 in SDL3, but we'll check for EOF to cause a device disconnect.
    const size_t br = SNDIO_sio_read(device->hidden->dev, buffer, buflen);
    if ((br == 0) && SNDIO_sio_eof(device->hidden->dev)) {
        return -1;
    }
    return (int) br;
}

static void SNDIO_FlushCapture(SDL_AudioDevice *device)
{
    char buf[512];
    while (!SDL_AtomicGet(&device->shutdown) && (SNDIO_sio_read(device->hidden->dev, buf, sizeof(buf)) > 0)) {
        // do nothing
    }
}

static Uint8 *SNDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return device->hidden->mixbuf;
}

static void SNDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->dev != NULL) {
            SNDIO_sio_stop(device->hidden->dev);
            SNDIO_sio_close(device->hidden->dev);
        }
        SDL_free(device->hidden->pfd);
        SDL_free(device->hidden->mixbuf);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static int SNDIO_OpenDevice(SDL_AudioDevice *device)
{
    device->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, sizeof(*device->hidden));
    if (device->hidden == NULL) {
        return SDL_OutOfMemory();
    }

    // !!! FIXME: we really should standardize this on a specific SDL hint.
    const char *audiodev = SDL_getenv("AUDIODEV");

    // Capture devices must be non-blocking for SNDIO_FlushCapture
    device->hidden->dev = SNDIO_sio_open(audiodev != NULL ? audiodev : SIO_DEVANY,
                                         device->iscapture ? SIO_REC : SIO_PLAY, device->iscapture);
    if (device->hidden->dev == NULL) {
        return SDL_SetError("sio_open() failed");
    }

    device->hidden->pfd = SDL_malloc(sizeof(struct pollfd) * SNDIO_sio_nfds(device->hidden->dev));
    if (device->hidden->pfd == NULL) {
        return SDL_OutOfMemory();
    }

    struct sio_par par;
    SNDIO_sio_initpar(&par);

    par.rate = device->spec.freq;
    par.pchan = device->spec.channels;
    par.round = device->sample_frames;
    par.appbufsz = par.round * 2;

    // Try for a closest match on audio format
    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts = SDL_ClosestAudioFormats(device->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (!SDL_AUDIO_ISFLOAT(test_format)) {
            par.le = SDL_AUDIO_ISLITTLEENDIAN(test_format) ? 1 : 0;
            par.sig = SDL_AUDIO_ISSIGNED(test_format) ? 1 : 0;
            par.bits = SDL_AUDIO_BITSIZE(test_format);

            if (SNDIO_sio_setpar(device->hidden->dev, &par) == 0) {
                continue;
            }
            if (SNDIO_sio_getpar(device->hidden->dev, &par) == 0) {
                return SDL_SetError("sio_getpar() failed");
            }
            if (par.bps != SIO_BPS(par.bits)) {
                continue;
            }
            if ((par.bits == 8 * par.bps) || (par.msb)) {
                break;
            }
        }
    }

    if (!test_format) {
        return SDL_SetError("sndio: Unsupported audio format");
    }

    if ((par.bps == 4) && (par.sig) && (par.le)) {
        device->spec.format = SDL_AUDIO_S32LE;
    } else if ((par.bps == 4) && (par.sig) && (!par.le)) {
        device->spec.format = SDL_AUDIO_S32BE;
    } else if ((par.bps == 2) && (par.sig) && (par.le)) {
        device->spec.format = SDL_AUDIO_S16LE;
    } else if ((par.bps == 2) && (par.sig) && (!par.le)) {
        device->spec.format = SDL_AUDIO_S16BE;
    } else if ((par.bps == 1) && (par.sig)) {
        device->spec.format = SDL_AUDIO_S8;
    } else if ((par.bps == 1) && (!par.sig)) {
        device->spec.format = SDL_AUDIO_U8;
    } else {
        return SDL_SetError("sndio: Got unsupported hardware audio format.");
    }

    device->spec.freq = par.rate;
    device->spec.channels = par.pchan;
    device->sample_frames = par.round;

    // Calculate the final parameters for this audio specification
    SDL_UpdatedAudioDeviceFormat(device);

    // Allocate mixing buffer
    device->hidden->mixbuf = (Uint8 *)SDL_malloc(device->buffer_size);
    if (device->hidden->mixbuf == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(device->hidden->mixbuf, device->silence_value, device->buffer_size);

    if (!SNDIO_sio_start(device->hidden->dev)) {
        return SDL_SetError("sio_start() failed");
    }

    return 0;  // We're ready to rock and roll. :-)
}

static void SNDIO_Deinitialize(void)
{
    UnloadSNDIOLibrary();
}

static void SNDIO_DetectDevices(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture)
{
    *default_output = SDL_AddAudioDevice(SDL_FALSE, DEFAULT_OUTPUT_DEVNAME, NULL, (void *)0x1);
    *default_capture = SDL_AddAudioDevice(SDL_TRUE, DEFAULT_INPUT_DEVNAME, NULL, (void *)0x2);
}

static SDL_bool SNDIO_Init(SDL_AudioDriverImpl *impl)
{
    if (LoadSNDIOLibrary() < 0) {
        return SDL_FALSE;
    }

    impl->OpenDevice = SNDIO_OpenDevice;
    impl->WaitDevice = SNDIO_WaitDevice;
    impl->PlayDevice = SNDIO_PlayDevice;
    impl->GetDeviceBuf = SNDIO_GetDeviceBuf;
    impl->CloseDevice = SNDIO_CloseDevice;
    impl->WaitCaptureDevice = SNDIO_WaitDevice;
    impl->CaptureFromDevice = SNDIO_CaptureFromDevice;
    impl->FlushCapture = SNDIO_FlushCapture;
    impl->Deinitialize = SNDIO_Deinitialize;
    impl->DetectDevices = SNDIO_DetectDevices;

    impl->AllowsArbitraryDeviceNames = SDL_TRUE;
    impl->HasCaptureSupport = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap SNDIO_bootstrap = {
    "sndio", "OpenBSD sndio", SNDIO_Init, SDL_FALSE
};

#endif // SDL_AUDIO_DRIVER_SNDIO
