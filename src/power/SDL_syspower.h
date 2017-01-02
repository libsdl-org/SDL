/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2017 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

/* These are functions that need to be implemented by a port of SDL */

#ifndef SDL_syspower_h_
#define SDL_syspower_h_

#include "SDL_power.h"

#ifndef SDL_POWER_DISABLED
#ifdef SDL_POWER_LINUX          /* in order of preference. More than could work. */
SDL_bool SDL_GetPowerInfo_Linux_sys_class_power_supply(SDL_PowerState *, int *, int *);
SDL_bool SDL_GetPowerInfo_Linux_proc_acpi(SDL_PowerState *, int *, int *);
SDL_bool SDL_GetPowerInfo_Linux_proc_apm(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_WINDOWS        /* handles Win32, Win64, PocketPC. */
SDL_bool SDL_GetPowerInfo_Windows(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_UIKIT          /* handles iPhone/iPad/etc */
SDL_bool SDL_GetPowerInfo_UIKit(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_MACOSX         /* handles Mac OS X, Darwin. */
SDL_bool SDL_GetPowerInfo_MacOSX(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_HAIKU          /* with BeOS euc.jp apm driver. Does this work on Haiku? */
SDL_bool SDL_GetPowerInfo_Haiku(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_ANDROID        /* handles Android. */
SDL_bool SDL_GetPowerInfo_Android(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_PSP        /* handles PSP. */
SDL_bool SDL_GetPowerInfo_PSP(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_WINRT          /* handles WinRT */
SDL_bool SDL_GetPowerInfo_WinRT(SDL_PowerState *, int *, int *);
#endif
#ifdef SDL_POWER_EMSCRIPTEN     /* handles Emscripten */
SDL_bool SDL_GetPowerInfo_Emscripten(SDL_PowerState *, int *, int *);
#endif

#ifdef SDL_POWER_HARDWIRED
SDL_bool SDL_GetPowerInfo_Hardwired(SDL_PowerState *, int *, int *);
#endif
#endif

#endif /* SDL_syspower_h_ */

/* vi: set ts=4 sw=4 expandtab: */
