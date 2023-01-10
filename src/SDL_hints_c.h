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

/* This file defines useful function for working with SDL hints */

#ifndef SDL_hints_c_h_
#define SDL_hints_c_h_

extern SDL_bool SDL_GetStringBoolean(const char *value, SDL_bool default_value);

/* You can "register" a hint, which will set its default value to `default_value` and
   then whenever the hint changes, `*addr` will update, so you can check a static variable
   instead of searching a linked list with SDL_GetHint every time, or deciding how to parse
   a string for ints, floats, or bools, or having to manage your own callback just to get
   fast lookups. String hints store a pointer to the hint's internal string; it is not to
   be modified or freed through this interface, and may change at any time. Plan accordingly.
   `*addr` is set by this call, to either the default or an overridden hint value from the
   app or user. You can register the same hint to multiple variables, and registering
   the same hint to the same address multiple times is safe to do, it will only leave
   a single registration. */
extern SDL_bool SDL_RegisterIntHint(const char *name, int *addr, int default_value);
extern SDL_bool SDL_RegisterFloatHint(const char *name, float *addr, float default_value);
extern SDL_bool SDL_RegisterBoolHint(const char *name, SDL_bool *addr, SDL_bool default_value);
extern SDL_bool SDL_RegisterStringHint(const char *name, const char **addr, const char *default_value);

/* It's not strictly necessary to unregister hints, as long as they are registered to
   static memory, since SDL_ClearHint will clear it at shutdown.
   But in case you want or need to, you can. */
extern void SDL_UnregisterIntHint(const char *name, int *addr);
extern void SDL_UnregisterFloatHint(const char *name, float *addr);
extern void SDL_UnregisterBoolHint(const char *name, SDL_bool *addr);
extern void SDL_UnregisterStringHint(const char *name, const char **addr);

#endif /* SDL_hints_c_h_ */
