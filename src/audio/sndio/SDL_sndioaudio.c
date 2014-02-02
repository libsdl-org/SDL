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

#include "../../SDL_internal.h"

#if SDL_AUDIO_DRIVER_SNDIO

/* OpenBSD sndio target */

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif

#include <unistd.h>

#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "SDL_sndioaudio.h"

#ifdef SDL_AUDIO_DRIVER_SNDIO_DYNAMIC
#include "SDL_loadso.h"
#endif

static struct sio_hdl * (*SNDIO_sio_open)(const char *, unsigned int, int);
static void (*SNDIO_sio_close)(struct sio_hdl *);
static int (*SNDIO_sio_setpar)(struct sio_hdl *, struct sio_par *);
static int (*SNDIO_sio_getpar)(struct sio_hdl *, struct sio_par *);
static int (*SNDIO_sio_start)(struct sio_hdl *);
static int (*SNDIO_sio_stop)(struct sio_hdl *);
static size_t (*SNDIO_sio_read)(struct sio_hdl *, void *, size_t);
static size_t (*SNDIO_sio_write)(struct sio_hdl *, const void *, size_t);
static void (*SNDIO_sio_initpar)(struct sio_par *);

#ifdef SDL_AUDIO_DRIVER_SNDIO_DYNAMIC
static const char *sndio_library = SDL_AUDIO_DRIVER_SNDIO_DYNAMIC;
static void *sndio_handle = NULL;

static int
load_sndio_sym(const char *fn, void **addr)
{
    *addr = SDL_LoadFunction(sndio_handle, fn);
    if (*addr == NULL) {
        /* Don't call SDL_SetError(): SDL_LoadFunction already did. */
        return 0;
    }

    return 1;
}

/* cast funcs to char* first, to please GCC's strict aliasing rules. */
#define SDL_SNDIO_SYM(x) \
    if (!load_sndio_sym(#x, (void **) (char *) &SNDIO_##x)) return -1
#else
#define SDL_SNDIO_SYM(x) SNDIO_##x = x
#endif

static int
load_sndio_syms(void)
{
    SDL_SNDIO_SYM(sio_open);
    SDL_SNDIO_SYM(sio_close);
    SDL_SNDIO_SYM(sio_setpar);
    SDL_SNDIO_SYM(sio_getpar);
    SDL_SNDIO_SYM(sio_start);
    SDL_SNDIO_SYM(sio_stop);
    SDL_SNDIO_SYM(sio_read);
    SDL_SNDIO_SYM(sio_write);
    SDL_SNDIO_SYM(sio_initpar);
    return 0;
}

#undef SDL_SNDIO_SYM

#ifdef SDL_AUDIO_DRIVER_SNDIO_DYNAMIC

static void
UnloadSNDIOLibrary(void)
{
    if (sndio_handle != NULL) {
        SDL_UnloadObject(sndio_handle);
        sndio_handle = NULL;
    }
}

static int
LoadSNDIOLibrary(void)
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

static void
UnloadSNDIOLibrary(void)
{
}

static int
LoadSNDIOLibrary(void)
{
    load_sndio_syms();
    return 0;
}

#endif /* SDL_AUDIO_DRIVER_SNDIO_DYNAMIC */




static void
SNDIO_WaitDevice(_THIS)
{
    /* no-op; SNDIO_sio_write() blocks if necessary. */
}

static void
SNDIO_PlayDevice(_THIS)
{
    const int written = SNDIO_sio_write(this->hidden->dev,
                                        this->hidden->mixbuf,
                                        this->hidden->mixlen);

    /* If we couldn't write, assume fatal error for now */
    if ( written == 0 ) {
        this->enabled = 0;
    }
#ifdef DEBUG_AUDIO
    fprintf(stderr, "Wrote %d bytes of audio data\n", written);
#endif
}

static Uint8 *
SNDIO_GetDeviceBuf(_THIS)
{
    return this->hidden->mixbuf;
}

static void
SNDIO_WaitDone(_THIS)
{
    SNDIO_sio_stop(this->hidden->dev);
}

static void
SNDIO_CloseDevice(_THIS)
{
    if (this->hidden != NULL) {
        SDL_FreeAudioMem(this->hidden->mixbuf);
        this->hidden->mixbuf = NULL;
        if ( this->hidden->dev != NULL ) {
            SNDIO_sio_close(this->hidden->dev);
            this->hidden->dev = NULL;
        }
        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}

static int
SNDIO_OpenDevice(_THIS, const char *devname, int iscapture)
{
    SDL_AudioFormat test_format = SDL_FirstAudioFormat(this->spec.format);
    struct sio_par par;
    int status;

    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc(sizeof(*this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden, 0, sizeof(*this->hidden));

    this->hidden->mixlen = this->spec.size;

    /* !!! FIXME: SIO_DEVANY can be a specific device... */
    if ((this->hidden->dev = SNDIO_sio_open(NULL, SIO_PLAY, 0)) == NULL) {
        SNDIO_CloseDevice(this);
        return SDL_SetError("sio_open() failed");
    }

    SNDIO_sio_initpar(&par);

    par.rate = this->spec.freq;
    par.pchan = this->spec.channels;
    par.round = this->spec.samples;
    par.appbufsz = par.round * 2;

    /* Try for a closest match on audio format */
    status = -1;
    while (test_format && (status < 0)) {
        if (!SDL_AUDIO_ISFLOAT(test_format)) {
            par.le = SDL_AUDIO_ISLITTLEENDIAN(test_format) ? 1 : 0;
            par.sig = SDL_AUDIO_ISSIGNED(test_format) ? 1 : 0;
            par.bits = SDL_AUDIO_BITSIZE(test_format);

            if (SNDIO_sio_setpar(this->hidden->dev, &par) == 1) {
                status = 0;
                break;
            }
        }
        test_format = SDL_NextAudioFormat();
    }

    if (status < 0) {
        SNDIO_CloseDevice(this);
        return SDL_SetError("sndio: Couldn't find any hardware audio formats");
    }

    if (SNDIO_sio_getpar(this->hidden->dev, &par) == 0) {
        SNDIO_CloseDevice(this);
        return SDL_SetError("sio_getpar() failed");
    }

    if ((par.bits == 32) && (par.sig) && (par.le))
        this->spec.format = AUDIO_S32LSB;
    else if ((par.bits == 32) && (par.sig) && (!par.le))
        this->spec.format = AUDIO_S32MSB;
    else if ((par.bits == 16) && (par.sig) && (par.le))
        this->spec.format = AUDIO_S16LSB;
    else if ((par.bits == 16) && (par.sig) && (!par.le))
        this->spec.format = AUDIO_S16MSB;
    else if ((par.bits == 16) && (!par.sig) && (par.le))
        this->spec.format = AUDIO_U16LSB;
    else if ((par.bits == 16) && (!par.sig) && (!par.le))
        this->spec.format = AUDIO_U16MSB;
    else if ((par.bits == 8) && (par.sig))
        this->spec.format = AUDIO_S8;
    else if ((par.bits == 8) && (!par.sig))
        this->spec.format = AUDIO_U8;
    else {
        SNDIO_CloseDevice(this);
        return SDL_SetError("sndio: Got unsupported hardware audio format.");
    }

    this->spec.freq = par.rate;
    this->spec.channels = par.pchan;
    this->spec.samples = par.round;

    /* Calculate the final parameters for this audio specification */
    SDL_CalculateAudioSpec(&this->spec);

    /* Allocate mixing buffer */
    this->hidden->mixlen = this->spec.size;
    this->hidden->mixbuf = (Uint8 *) SDL_AllocAudioMem(this->hidden->mixlen);
    if (this->hidden->mixbuf == NULL) {
        SNDIO_CloseDevice(this);
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden->mixbuf, this->spec.silence, this->hidden->mixlen);

    if (!SNDIO_sio_start(this->hidden->dev)) {
        return SDL_SetError("sio_start() failed");
    }

    /* We're ready to rock and roll. :-) */
    return 0;
}

static void
SNDIO_Deinitialize(void)
{
    UnloadSNDIOLibrary();
}

static int
SNDIO_Init(SDL_AudioDriverImpl * impl)
{
    if (LoadSNDIOLibrary() < 0) {
        return 0;
    }

    /* Set the function pointers */
    impl->OpenDevice = SNDIO_OpenDevice;
    impl->WaitDevice = SNDIO_WaitDevice;
    impl->PlayDevice = SNDIO_PlayDevice;
    impl->GetDeviceBuf = SNDIO_GetDeviceBuf;
    impl->WaitDone = SNDIO_WaitDone;
    impl->CloseDevice = SNDIO_CloseDevice;
    impl->Deinitialize = SNDIO_Deinitialize;
    impl->OnlyHasDefaultOutputDevice = 1;  /* !!! FIXME: sndio can handle multiple devices. */

    return 1;   /* this audio target is available. */
}

AudioBootStrap SNDIO_bootstrap = {
    "sndio", "OpenBSD sndio", SNDIO_Init, 0
};

#endif /* SDL_AUDIO_DRIVER_SNDIO */

/* vi: set ts=4 sw=4 expandtab: */
