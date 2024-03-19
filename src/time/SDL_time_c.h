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

#ifndef SDL_time_c_h_
#define SDL_time_c_h_

#include "SDL_internal.h"

#define SDL_SECONDS_PER_DAY 86400

extern void SDL_InitTime(void);
extern void SDL_QuitTime(void);

/* Given a calendar date, returns days since Jan 1 1970, and optionally
 * the day of the week (0-6, 0 is Sunday) and day of the year (0-365).
 */
extern Sint64 SDL_CivilToDays(int year, int month, int day, int *day_of_week, int *day_of_year);

extern void SDL_GetSystemTimeLocalePreferences(SDL_DATE_FORMAT *df, SDL_TIME_FORMAT *tf);

#endif /* SDL_time_c_h_ */
