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

/* Get the name of the audio device we use for output */

#if SDL_AUDIO_DRIVER_BSD || SDL_AUDIO_DRIVER_OSS || SDL_AUDIO_DRIVER_SUNAUDIO

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h> /* For close() */

#include "SDL_stdinc.h"
#include "SDL_audiodev_c.h"

#ifndef _PATH_DEV_DSP
#if defined(__NETBSD__) || defined(__OPENBSD__)
#define _PATH_DEV_DSP  "/dev/audio"
#else
#define _PATH_DEV_DSP  "/dev/dsp"
#endif
#endif
#ifndef _PATH_DEV_DSP24
#define _PATH_DEV_DSP24 "/dev/sound/dsp"
#endif
#ifndef _PATH_DEV_AUDIO
#define _PATH_DEV_AUDIO "/dev/audio"
#endif

static SDL_INLINE void
test_device(const char *fname, int flags, int (*test) (int fd),
            SDL_AddAudioDevice addfn)
{
    struct stat sb;
    if ((stat(fname, &sb) == 0) && (S_ISCHR(sb.st_mode))) {
        const int audio_fd = open(fname, flags, 0);
        if (audio_fd >= 0) {
            if (test(audio_fd)) {
                addfn(fname);
            }
            close(audio_fd);
        }
    }
}

static int
test_stub(int fd)
{
    return 1;
}

void
SDL_EnumUnixAudioDevices(int iscapture, int classic, int (*test)(int fd),
                         SDL_AddAudioDevice addfn)
{
    const int flags = ((iscapture) ? OPEN_FLAGS_INPUT : OPEN_FLAGS_OUTPUT);
    const char *audiodev;
    char audiopath[1024];

    if (test == NULL)
        test = test_stub;

    /* Figure out what our audio device is */
    if (((audiodev = SDL_getenv("SDL_PATH_DSP")) == NULL) &&
        ((audiodev = SDL_getenv("AUDIODEV")) == NULL)) {
        if (classic) {
            audiodev = _PATH_DEV_AUDIO;
        } else {
            struct stat sb;

            /* Added support for /dev/sound/\* in Linux 2.4 */
            if (((stat("/dev/sound", &sb) == 0) && S_ISDIR(sb.st_mode))
                && ((stat(_PATH_DEV_DSP24, &sb) == 0)
                    && S_ISCHR(sb.st_mode))) {
                audiodev = _PATH_DEV_DSP24;
            } else {
                audiodev = _PATH_DEV_DSP;
            }
        }
    }
    test_device(audiodev, flags, test, addfn);

    if (SDL_strlen(audiodev) < (sizeof(audiopath) - 3)) {
        int instance = 0;
        while (instance++ <= 64) {
            SDL_snprintf(audiopath, SDL_arraysize(audiopath),
                         "%s%d", audiodev, instance);
            test_device(audiopath, flags, test, addfn);
        }
    }
}

#endif /* Audio driver selection */
/* vi: set ts=4 sw=4 expandtab: */
