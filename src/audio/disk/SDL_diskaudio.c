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

#ifdef SDL_AUDIO_DRIVER_DISK

// Output raw audio data to a file.

#include "../SDL_sysaudio.h"
#include "SDL_diskaudio.h"

// !!! FIXME: these should be SDL hints, not environment variables.
// environment variables and defaults.
#define DISKENVR_OUTFILE    "SDL_DISKAUDIOFILE"
#define DISKDEFAULT_OUTFILE "sdlaudio.raw"
#define DISKENVR_INFILE     "SDL_DISKAUDIOFILEIN"
#define DISKDEFAULT_INFILE  "sdlaudio-in.raw"
#define DISKENVR_IODELAY    "SDL_DISKAUDIODELAY"

static int DISKAUDIO_WaitDevice(SDL_AudioDevice *device)
{
    SDL_Delay(device->hidden->io_delay);
    return 0;
}

static int DISKAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buffer_size)
{
    const int written = (int)SDL_RWwrite(device->hidden->io, buffer, (size_t)buffer_size);
    if (written != buffer_size) { // If we couldn't write, assume fatal error for now
        return -1;
    }
#ifdef DEBUG_AUDIO
    SDL_Log("DISKAUDIO: Wrote %d bytes of audio data", (int) written);
#endif
    return 0;
}

static Uint8 *DISKAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return device->hidden->mixbuf;
}

static int DISKAUDIO_CaptureFromDevice(SDL_AudioDevice *device, void *buffer, int buflen)
{
    struct SDL_PrivateAudioData *h = device->hidden;
    const int origbuflen = buflen;

    if (h->io) {
        const int br = (int)SDL_RWread(h->io, buffer, (size_t)buflen);
        buflen -= br;
        buffer = ((Uint8 *)buffer) + br;
        if (buflen > 0) { // EOF (or error, but whatever).
            SDL_RWclose(h->io);
            h->io = NULL;
        }
    }

    // if we ran out of file, just write silence.
    SDL_memset(buffer, device->silence_value, buflen);

    return origbuflen;
}

static void DISKAUDIO_FlushCapture(SDL_AudioDevice *device)
{
    // no op...we don't advance the file pointer or anything.
}

static void DISKAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->io) {
            SDL_RWclose(device->hidden->io);
        }
        SDL_free(device->hidden->mixbuf);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static const char *get_filename(const SDL_bool iscapture)
{
    const char *devname = SDL_getenv(iscapture ? DISKENVR_INFILE : DISKENVR_OUTFILE);
    if (!devname) {
        devname = iscapture ? DISKDEFAULT_INFILE : DISKDEFAULT_OUTFILE;
    }
    return devname;
}

static int DISKAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    SDL_bool iscapture = device->iscapture;
    const char *fname = get_filename(iscapture);
    const char *envr = SDL_getenv(DISKENVR_IODELAY);

    device->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return -1;
    }

    if (envr) {
        device->hidden->io_delay = SDL_atoi(envr);
    } else {
        device->hidden->io_delay = ((device->sample_frames * 1000) / device->spec.freq);
    }

    // Open the "audio device"
    device->hidden->io = SDL_RWFromFile(fname, iscapture ? "rb" : "wb");
    if (!device->hidden->io) {
        return -1;
    }

    // Allocate mixing buffer
    if (!iscapture) {
        device->hidden->mixbuf = (Uint8 *)SDL_malloc(device->buffer_size);
        if (!device->hidden->mixbuf) {
            return -1;
        }
        SDL_memset(device->hidden->mixbuf, device->silence_value, device->buffer_size);
    }

    SDL_LogCritical(SDL_LOG_CATEGORY_AUDIO, "You are using the SDL disk i/o audio driver!");
    SDL_LogCritical(SDL_LOG_CATEGORY_AUDIO, " %s file [%s].\n", iscapture ? "Reading from" : "Writing to", fname);

    return 0;  // We're ready to rock and roll. :-)
}

static void DISKAUDIO_DetectDevices(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture)
{
    *default_output = SDL_AddAudioDevice(SDL_FALSE, DEFAULT_OUTPUT_DEVNAME, NULL, (void *)0x1);
    *default_capture = SDL_AddAudioDevice(SDL_TRUE, DEFAULT_INPUT_DEVNAME, NULL, (void *)0x2);
}

static SDL_bool DISKAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->OpenDevice = DISKAUDIO_OpenDevice;
    impl->WaitDevice = DISKAUDIO_WaitDevice;
    impl->WaitCaptureDevice = DISKAUDIO_WaitDevice;
    impl->PlayDevice = DISKAUDIO_PlayDevice;
    impl->GetDeviceBuf = DISKAUDIO_GetDeviceBuf;
    impl->CaptureFromDevice = DISKAUDIO_CaptureFromDevice;
    impl->FlushCapture = DISKAUDIO_FlushCapture;
    impl->CloseDevice = DISKAUDIO_CloseDevice;
    impl->DetectDevices = DISKAUDIO_DetectDevices;

    impl->HasCaptureSupport = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap DISKAUDIO_bootstrap = {
    "disk", "direct-to-disk audio", DISKAUDIO_Init, SDL_TRUE
};

#endif // SDL_AUDIO_DRIVER_DISK
