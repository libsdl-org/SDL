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

#if SDL_AUDIO_DRIVER_DISK

/* Output raw audio data to a file. */

#if HAVE_STDIO_H
#include <stdio.h>
#endif

#include "SDL_rwops.h"
#include "SDL_timer.h"
#include "SDL_audio.h"
#include "../SDL_audiomem.h"
#include "../SDL_audio_c.h"
#include "SDL_diskaudio.h"

/* environment variables and defaults. */
#define DISKENVR_OUTFILE         "SDL_DISKAUDIOFILE"
#define DISKDEFAULT_OUTFILE      "sdlaudio.raw"
#define DISKENVR_WRITEDELAY      "SDL_DISKAUDIODELAY"
#define DISKDEFAULT_WRITEDELAY   150

static const char *
DISKAUD_GetOutputFilename(const char *devname)
{
    if (devname == NULL) {
        devname = SDL_getenv(DISKENVR_OUTFILE);
        if (devname == NULL) {
            devname = DISKDEFAULT_OUTFILE;
        }
    }
    return devname;
}

/* This function waits until it is possible to write a full sound buffer */
static void
DISKAUD_WaitDevice(_THIS)
{
    SDL_Delay(this->hidden->write_delay);
}

static void
DISKAUD_PlayDevice(_THIS)
{
    size_t written;

    /* Write the audio data */
    written = SDL_RWwrite(this->hidden->output,
                          this->hidden->mixbuf, 1, this->hidden->mixlen);

    /* If we couldn't write, assume fatal error for now */
    if (written != this->hidden->mixlen) {
        this->enabled = 0;
    }
#ifdef DEBUG_AUDIO
    fprintf(stderr, "Wrote %d bytes of audio data\n", written);
#endif
}

static Uint8 *
DISKAUD_GetDeviceBuf(_THIS)
{
    return (this->hidden->mixbuf);
}

static void
DISKAUD_CloseDevice(_THIS)
{
    if (this->hidden != NULL) {
        SDL_FreeAudioMem(this->hidden->mixbuf);
        this->hidden->mixbuf = NULL;
        if (this->hidden->output != NULL) {
            SDL_RWclose(this->hidden->output);
            this->hidden->output = NULL;
        }
        SDL_free(this->hidden);
        this->hidden = NULL;
    }
}

static int
DISKAUD_OpenDevice(_THIS, const char *devname, int iscapture)
{
    const char *envr = SDL_getenv(DISKENVR_WRITEDELAY);
    const char *fname = DISKAUD_GetOutputFilename(devname);

    this->hidden = (struct SDL_PrivateAudioData *)
        SDL_malloc(sizeof(*this->hidden));
    if (this->hidden == NULL) {
        return SDL_OutOfMemory();
    }
    SDL_memset(this->hidden, 0, sizeof(*this->hidden));

    this->hidden->mixlen = this->spec.size;
    this->hidden->write_delay =
        (envr) ? SDL_atoi(envr) : DISKDEFAULT_WRITEDELAY;

    /* Open the audio device */
    this->hidden->output = SDL_RWFromFile(fname, "wb");
    if (this->hidden->output == NULL) {
        DISKAUD_CloseDevice(this);
        return -1;
    }

    /* Allocate mixing buffer */
    this->hidden->mixbuf = (Uint8 *) SDL_AllocAudioMem(this->hidden->mixlen);
    if (this->hidden->mixbuf == NULL) {
        DISKAUD_CloseDevice(this);
        return -1;
    }
    SDL_memset(this->hidden->mixbuf, this->spec.silence, this->spec.size);

#if HAVE_STDIO_H
    fprintf(stderr,
            "WARNING: You are using the SDL disk writer audio driver!\n"
            " Writing to file [%s].\n", fname);
#endif

    /* We're ready to rock and roll. :-) */
    return 0;
}

static int
DISKAUD_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->OpenDevice = DISKAUD_OpenDevice;
    impl->WaitDevice = DISKAUD_WaitDevice;
    impl->PlayDevice = DISKAUD_PlayDevice;
    impl->GetDeviceBuf = DISKAUD_GetDeviceBuf;
    impl->CloseDevice = DISKAUD_CloseDevice;

    return 1;   /* this audio target is available. */
}

AudioBootStrap DISKAUD_bootstrap = {
    "disk", "direct-to-disk audio", DISKAUD_Init, 1
};

#endif /* SDL_AUDIO_DRIVER_DISK */

/* vi: set ts=4 sw=4 expandtab: */
