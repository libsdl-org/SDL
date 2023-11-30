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

#ifdef SDL_AUDIO_DRIVER_NETBSD

// Driver for native NetBSD audio(4).

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/audioio.h>

#include "../../core/unix/SDL_poll.h"
#include "../SDL_audiodev_c.h"
#include "SDL_netbsdaudio.h"

//#define DEBUG_AUDIO

static void NETBSDAUDIO_DetectDevices(SDL_AudioDevice **default_output, SDL_AudioDevice **default_capture)
{
    SDL_EnumUnixAudioDevices(SDL_FALSE, NULL);
}

static void NETBSDAUDIO_Status(SDL_AudioDevice *device)
{
#ifdef DEBUG_AUDIO
    /* *INDENT-OFF* */ /* clang-format off */
    audio_info_t info;
    const struct audio_prinfo *prinfo;

    if (ioctl(device->hidden->audio_fd, AUDIO_GETINFO, &info) < 0) {
        fprintf(stderr, "AUDIO_GETINFO failed.\n");
        return;
    }

    prinfo = device->iscapture ? &info.record : &info.play;

    fprintf(stderr, "\n"
            "[%s info]\n"
            "buffer size	:   %d bytes\n"
            "sample rate	:   %i Hz\n"
            "channels	:   %i\n"
            "precision	:   %i-bit\n"
            "encoding	:   0x%x\n"
            "seek		:   %i\n"
            "sample count	:   %i\n"
            "EOF count	:   %i\n"
            "paused		:   %s\n"
            "error occurred	:   %s\n"
            "waiting		:   %s\n"
            "active		:   %s\n"
            "",
            device->iscapture ? "record" : "play",
            prinfo->buffer_size,
            prinfo->sample_rate,
            prinfo->channels,
            prinfo->precision,
            prinfo->encoding,
            prinfo->seek,
            prinfo->samples,
            prinfo->eof,
            prinfo->pause ? "yes" : "no",
            prinfo->error ? "yes" : "no",
            prinfo->waiting ? "yes" : "no",
            prinfo->active ? "yes" : "no");

    fprintf(stderr, "\n"
            "[audio info]\n"
            "monitor_gain	:   %i\n"
            "hw block size	:   %d bytes\n"
            "hi watermark	:   %i\n"
            "lo watermark	:   %i\n"
            "audio mode	:   %s\n"
            "",
            info.monitor_gain,
            info.blocksize,
            info.hiwat, info.lowat,
            (info.mode == AUMODE_PLAY) ? "PLAY"
            : (info.mode == AUMODE_RECORD) ? "RECORD"
            : (info.mode == AUMODE_PLAY_ALL ? "PLAY_ALL" : "?"));

    fprintf(stderr, "\n"
            "[audio spec]\n"
            "format		:   0x%x\n"
            "size		:   %u\n"
            "",
            device->spec.format,
            device->buffer_size);
    /* *INDENT-ON* */ /* clang-format on */

#endif // DEBUG_AUDIO
}

static int NETBSDAUDIO_WaitDevice(SDL_AudioDevice *device)
{
    const SDL_bool iscapture = device->iscapture;
    while (!SDL_AtomicGet(&device->shutdown)) {
        audio_info_t info;
        const int rc = ioctl(device->hidden->audio_fd, AUDIO_GETINFO, &info);
        if (rc < 0) {
            if (errno == EAGAIN) {
                continue;
            }
            // Hmm, not much we can do - abort
            fprintf(stderr, "netbsdaudio WaitDevice ioctl failed (unrecoverable): %s\n", strerror(errno));
            return -1;
        }
        const size_t remain = (size_t)((iscapture ? info.record.seek : info.play.seek) * SDL_AUDIO_BYTESIZE(device->spec.format));
        if (!iscapture && (remain >= device->buffer_size)) {
            SDL_Delay(10);
        } else if (iscapture && (remain < device->buffer_size)) {
            SDL_Delay(10);
        } else {
            break; // ready to go!
        }
    }

    return 0;
}

static int NETBSDAUDIO_PlayDevice(SDL_AudioDevice *device, const Uint8 *buffer, int buflen)
{
    struct SDL_PrivateAudioData *h = device->hidden;
    const int written = write(h->audio_fd, buffer, buflen);
    if (written != buflen) {  // Treat even partial writes as fatal errors.
        return -1;
    }

#ifdef DEBUG_AUDIO
    fprintf(stderr, "Wrote %d bytes of audio data\n", written);
#endif
    return 0;
}

static Uint8 *NETBSDAUDIO_GetDeviceBuf(SDL_AudioDevice *device, int *buffer_size)
{
    return device->hidden->mixbuf;
}

static int NETBSDAUDIO_CaptureFromDevice(SDL_AudioDevice *device, void *vbuffer, int buflen)
{
    Uint8 *buffer = (Uint8 *)vbuffer;
    const int br = read(device->hidden->audio_fd, buffer, buflen);
    if (br == -1) {
        // Non recoverable error has occurred. It should be reported!!!
        perror("audio");
        return -1;
    }

#ifdef DEBUG_AUDIO
    fprintf(stderr, "Captured %d bytes of audio data\n", br);
#endif
    return br;
}

static void NETBSDAUDIO_FlushCapture(SDL_AudioDevice *device)
{
    struct SDL_PrivateAudioData *h = device->hidden;
    audio_info_t info;
    if (ioctl(device->hidden->audio_fd, AUDIO_GETINFO, &info) == 0) {
        size_t remain = (size_t)(info.record.seek * SDL_AUDIO_BYTESIZE(device->spec.format));
        while (remain > 0) {
            char buf[512];
            const size_t len = SDL_min(sizeof(buf), remain);
            const ssize_t br = read(h->audio_fd, buf, len);
            if (br <= 0) {
                break;
            }
            remain -= br;
        }
    }
}

static void NETBSDAUDIO_CloseDevice(SDL_AudioDevice *device)
{
    if (device->hidden) {
        if (device->hidden->audio_fd >= 0) {
            close(device->hidden->audio_fd);
        }
        SDL_free(device->hidden->mixbuf);
        SDL_free(device->hidden);
        device->hidden = NULL;
    }
}

static int NETBSDAUDIO_OpenDevice(SDL_AudioDevice *device)
{
    const SDL_bool iscapture = device->iscapture;
    int encoding = AUDIO_ENCODING_NONE;
    audio_info_t info, hwinfo;
    struct audio_prinfo *prinfo = iscapture ? &info.record : &info.play;

    // Initialize all variables that we clean on shutdown
    device->hidden = (struct SDL_PrivateAudioData *) SDL_calloc(1, sizeof(*device->hidden));
    if (!device->hidden) {
        return -1;
    }

    // Open the audio device; we hardcode the device path in `device->name` for lack of better info, so use that.
    const int flags = ((device->iscapture) ? O_RDONLY : O_WRONLY);
    device->hidden->audio_fd = open(device->name, flags | O_CLOEXEC);
    if (device->hidden->audio_fd < 0) {
        return SDL_SetError("Couldn't open %s: %s", device->name, strerror(errno));
    }

    AUDIO_INITINFO(&info);

#ifdef AUDIO_GETFORMAT // Introduced in NetBSD 9.0
    if (ioctl(device->hidden->audio_fd, AUDIO_GETFORMAT, &hwinfo) != -1) {
        // Use the device's native sample rate so the kernel doesn't have to resample.
        device->spec.freq = iscapture ? hwinfo.record.sample_rate : hwinfo.play.sample_rate;
    }
#endif

    prinfo->sample_rate = device->spec.freq;
    prinfo->channels = device->spec.channels;

    SDL_AudioFormat test_format;
    const SDL_AudioFormat *closefmts = SDL_ClosestAudioFormats(device->spec.format);
    while ((test_format = *(closefmts++)) != 0) {
        switch (test_format) {
        case SDL_AUDIO_U8:
            encoding = AUDIO_ENCODING_ULINEAR;
            break;
        case SDL_AUDIO_S8:
            encoding = AUDIO_ENCODING_SLINEAR;
            break;
        case SDL_AUDIO_S16LE:
            encoding = AUDIO_ENCODING_SLINEAR_LE;
            break;
        case SDL_AUDIO_S16BE:
            encoding = AUDIO_ENCODING_SLINEAR_BE;
            break;
        case SDL_AUDIO_S32LE:
            encoding = AUDIO_ENCODING_SLINEAR_LE;
            break;
        case SDL_AUDIO_S32BE:
            encoding = AUDIO_ENCODING_SLINEAR_BE;
            break;
        default:
            continue;
        }
        break;
    }

    if (!test_format) {
        return SDL_SetError("%s: Unsupported audio format", "netbsd");
    }
    prinfo->encoding = encoding;
    prinfo->precision = SDL_AUDIO_BITSIZE(test_format);

    info.hiwat = 5;
    info.lowat = 3;
    if (ioctl(device->hidden->audio_fd, AUDIO_SETINFO, &info) < 0) {
        return SDL_SetError("AUDIO_SETINFO failed for %s: %s", device->name, strerror(errno));
    }

    if (ioctl(device->hidden->audio_fd, AUDIO_GETINFO, &info) < 0) {
        return SDL_SetError("AUDIO_GETINFO failed for %s: %s", device->name, strerror(errno));
    }

    // Final spec used for the device.
    device->spec.format = test_format;
    device->spec.freq = prinfo->sample_rate;
    device->spec.channels = prinfo->channels;

    SDL_UpdatedAudioDeviceFormat(device);

    if (!iscapture) {
        // Allocate mixing buffer
        device->hidden->mixlen = device->buffer_size;
        device->hidden->mixbuf = (Uint8 *)SDL_malloc(device->hidden->mixlen);
        if (!device->hidden->mixbuf) {
            return -1;
        }
        SDL_memset(device->hidden->mixbuf, device->silence_value, device->buffer_size);
    }

    NETBSDAUDIO_Status(device);

    return 0;  // We're ready to rock and roll. :-)
}

static SDL_bool NETBSDAUDIO_Init(SDL_AudioDriverImpl *impl)
{
    impl->DetectDevices = NETBSDAUDIO_DetectDevices;
    impl->OpenDevice = NETBSDAUDIO_OpenDevice;
    impl->WaitDevice = NETBSDAUDIO_WaitDevice;
    impl->PlayDevice = NETBSDAUDIO_PlayDevice;
    impl->GetDeviceBuf = NETBSDAUDIO_GetDeviceBuf;
    impl->CloseDevice = NETBSDAUDIO_CloseDevice;
    impl->WaitCaptureDevice = NETBSDAUDIO_WaitDevice;
    impl->CaptureFromDevice = NETBSDAUDIO_CaptureFromDevice;
    impl->FlushCapture = NETBSDAUDIO_FlushCapture;

    impl->HasCaptureSupport = SDL_TRUE;

    return SDL_TRUE;
}

AudioBootStrap NETBSDAUDIO_bootstrap = {
    "netbsd", "NetBSD audio", NETBSDAUDIO_Init, SDL_FALSE
};

#endif // SDL_AUDIO_DRIVER_NETBSD
