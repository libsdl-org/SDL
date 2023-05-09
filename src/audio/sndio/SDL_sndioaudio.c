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

/* OpenBSD sndio target */

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
        /* Don't call SDL_SetError(): SDL_LoadFunction already did. */
        return 0;
    }

    return 1;
}

/* cast funcs to char* first, to please GCC's strict aliasing rules. */
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
            retval = -1;
            /* Don't call SDL_SetError(): SDL_LoadObject already did. */
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

#endif /* SDL_AUDIO_DRIVER_SNDIO_DYNAMIC */

static void SNDIO_WaitDevice(SDL_AudioDevice *_this)
{
    /* no-op; SNDIO_sio_write() blocks if necessary. */
}

static void SNDIO_PlayDevice(SDL_AudioDevice *_this)
{
    const int written = SNDIO_sio_write(_this->hidden->dev,
                                        _this->hidden->mixbuf,
                                        _this->hidden->mixlen);

    /* If we couldn't write, assume fatal error for now */
    if (written == 0) {
        SDL_OpenedAudioDeviceDisconnected(_this);
    }
#ifdef DEBUG_AUDIO
    fprintf(stderr, "Wrote %d bytes of audio data\n", written);
#endif
}

static int SNDIO_CaptureFromDevice(SDL_AudioDevice *_this, void *buffer, int buflen)
{
    size_t r;
    int revents;
    int nfds;

    /* Emulate a blocking read */
    r = SNDIO_sio_read(_this->hidden->dev, buffer, buflen);
    while (r == 0 && !SNDIO_sio_eof(_this->hidden->dev)) {
        nfds = SNDIO_sio_pollfd(_this->hidden->dev, _this->hidden->pfd, POLLIN);
        if (nfds <= 0 || poll(_this->hidden->pfd, nfds, INFTIM) < 0) {
            return -1;
        }
        revents = SNDIO_sio_revents(_this->hidden->dev, _this->hidden->pfd);
        if (revents & POLLIN) {
            r = SNDIO_sio_read(_this->hidden->dev, buffer, buflen);
        }
        if (revents & POLLHUP) {
            break;
        }
    }
    return (int)r;
}

static void SNDIO_FlushCapture(SDL_AudioDevice *_this)
{
    char buf[512];

    while (SNDIO_sio_read(_this->hidden->dev, buf, sizeof(buf)) != 0) {
        /* do nothing */;
    }
}

static Uint8 *SNDIO_GetDeviceBuf(SDL_AudioDevice *_this)
{
    return _this->hidden->mixbuf;
}

static void SNDIO_CloseDevice(SDL_AudioDevice *_this)
{
    if (_this->hidden->pfd != NULL) {
        SDL_free(_this->hidden->pfd);
    }
    if (_this->hidden->dev != NULL) {
        SNDIO_sio_stop(_this->hidden->dev);
        SNDIO_sio_close(_this->hidden->dev);
    }
    SDL_free(_this->hidden->mixbuf);
    SDL_free(_this->hidden);
}

static int SNDIO_OpenDevice(SDL_AudioDevice *_this, const char *devname)
{
    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts;
    struct sio_par par;
    SDL_bool iscapture = _this->iscapture;

    _this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc(sizeof(*_this->hidden));
    if (_this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_zerop(_this->hidden);

    _this->hidden->mixlen = _this->spec.size;

    /* Capture devices must be non-blocking for SNDIO_FlushCapture */
    _this->hidden->dev = SNDIO_sio_open(devname != NULL ? devname : SIO_DEVANY,
                                       iscapture ? SIO_REC : SIO_PLAY, iscapture);
    if (_this->hidden->dev == NULL) {
        return SDL_SetError("sio_open() failed");
    }

    /* Allocate the pollfd array for capture devices */
    if (iscapture) {
        _this->hidden->pfd = SDL_malloc(sizeof(struct pollfd) * SNDIO_sio_nfds(_this->hidden->dev));
        if (_this->hidden->pfd == NULL) {
            return SDL_OutOfMemory();
        }
    }

    SNDIO_sio_initpar(&par);

    par.rate = _this->spec.freq;
    par.pchan = _this->spec.channels;
    par.round = _this->spec.samples;
    par.appbufsz = par.round * 2;

    /* Try for a closest match on audio format */
    closefmts = SDL_ClosestAudioFormats(_this->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        if (!SDL_AUDIO_ISFLOAT(test_format)) {
            par.le = SDL_AUDIO_ISLITTLEENDIAN(test_format) ? 1 : 0;
            par.sig = SDL_AUDIO_ISSIGNED(test_format) ? 1 : 0;
            par.bits = SDL_AUDIO_BITSIZE(test_format);

            if (SNDIO_sio_setpar(_this->hidden->dev, &par) == 0) {
                continue;
            }
            if (SNDIO_sio_getpar(_this->hidden->dev, &par) == 0) {
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
        return SDL_SetError("%s: Unsupported audio format", "sndio");
    }

    if ((par.bps == 4) && (par.sig) && (par.le)) {
        _this->spec.format = SDL_AUDIO_S32LSB;
    } else if ((par.bps == 4) && (par.sig) && (!par.le)) {
        _this->spec.format = SDL_AUDIO_S32MSB;
    } else if ((par.bps == 2) && (par.sig) && (par.le)) {
        _this->spec.format = SDL_AUDIO_S16LSB;
    } else if ((par.bps == 2) && (par.sig) && (!par.le)) {
        _this->spec.format = SDL_AUDIO_S16MSB;
    } else if ((par.bps == 1) && (par.sig)) {
        _this->spec.format = SDL_AUDIO_S8;
    } else if ((par.bps == 1) && (!par.sig)) {
        _this->spec.format = SDL_AUDIO_U8;
    } else {
        return SDL_SetError("sndio: Got unsupported hardware audio format.");
    }

    _this->spec.freq = par.rate;
    _this->spec.channels = par.pchan;
    _this->spec.samples = par.round;

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&_this->spec);

    /* Allocate mixing buffer */
    _this->hidden->mixlen = _this->spec.size;
    _this->hidden->mixbuf = (Uint8 *)SDL_malloc(_this->hidden->mixlen);
    if (_this->hidden->mixbuf == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(_this->hidden->mixbuf, _this->spec.silence, _this->hidden->mixlen);

    if (!SNDIO_sio_start(_this->hidden->dev)) {
        return SDL_SetError("sio_start() failed");
    }

    /* We're ready to rock and roll. :-) */
    return 0;
}

static void SNDIO_Deinitialize(void)
{
    UnloadSNDIOLibrary();
}

static void SNDIO_DetectDevices(void)
{
    SDL_AddAudioDevice(SDL_FALSE, DEFAULT_OUTPUT_DEVNAME, NULL, (void *)0x1);
    SDL_AddAudioDevice(SDL_TRUE, DEFAULT_INPUT_DEVNAME, NULL, (void *)0x2);
}

static SDL_bool SNDIO_Init(SDL_AudioDriverImpl *impl)
{
    if (LoadSNDIOLibrary() < 0) {
        return SDL_FALSE;
    }

    /* Set the function pointers */
    impl->OpenDevice = SNDIO_OpenDevice;
    impl->WaitDevice = SNDIO_WaitDevice;
    impl->PlayDevice = SNDIO_PlayDevice;
    impl->GetDeviceBuf = SNDIO_GetDeviceBuf;
    impl->CloseDevice = SNDIO_CloseDevice;
    impl->CaptureFromDevice = SNDIO_CaptureFromDevice;
    impl->FlushCapture = SNDIO_FlushCapture;
    impl->Deinitialize = SNDIO_Deinitialize;
    impl->DetectDevices = SNDIO_DetectDevices;

    impl->AllowsArbitraryDeviceNames = SDL_TRUE;
    impl->HasCaptureSupport = SDL_TRUE;

    return SDL_TRUE; /* this audio target is available. */
}

AudioBootStrap SNDIO_bootstrap = {
    "sndio", "OpenBSD sndio", SNDIO_Init, SDL_FALSE
};

#endif /* SDL_AUDIO_DRIVER_SNDIO */
