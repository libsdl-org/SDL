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

/* Output audio to nowhere... */

#include "SDL_audio.h"
#include "../SDL_audio_c.h"
#include "SDL_dummyaudio.h"

static int
DUMMYAUD_OpenDevice(_THIS, const char *devname, int iscapture)
{
    return 0;                   /* always succeeds. */
}

static int
DUMMYAUD_Init(SDL_AudioDriverImpl * impl)
{
    /* Set the function pointers */
    impl->OpenDevice = DUMMYAUD_OpenDevice;
    impl->OnlyHasDefaultOutputDevice = 1;
    return 1;   /* this audio target is available. */
}

AudioBootStrap DUMMYAUD_bootstrap = {
    "dummy", "SDL dummy audio driver", DUMMYAUD_Init, 1
};

/* vi: set ts=4 sw=4 expandtab: */
