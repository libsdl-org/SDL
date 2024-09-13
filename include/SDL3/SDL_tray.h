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

/**
 * # CategoryTray
 *
 * System tray menu support.
 */

#ifndef SDL_tray_h_
#define SDL_tray_h_

#include <SDL3/SDL_error.h>

#include <SDL3/SDL_video.h>

#include <SDL3/SDL_begin_code.h>
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef struct SDL_Tray SDL_Tray;
typedef struct SDL_TrayMenu SDL_TrayMenu;
typedef struct SDL_TrayEntry SDL_TrayEntry;

typedef enum {
    /* Mandatory; must be specified at creation time */
    SDL_TRAYENTRY_BUTTON = 0,
    SDL_TRAYENTRY_CHECKBOX = (1 << 0),
    SDL_TRAYENTRY_SUBMENU = (1 << 1),

    /* Optional; can be changed later */
    SDL_TRAYENTRY_DISABLED = (1 << 16),
    SDL_TRAYENTRY_CHECKED = (1 << 17),
} SDL_TrayEntryFlags;

typedef void (*SDL_TrayCallback)(void *userdata, SDL_TrayEntry *entry);

extern SDL_DECLSPEC SDL_Tray *SDLCALL SDL_CreateTray(SDL_Surface *icon, const char *tooltip);
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_CreateTrayMenu(SDL_Tray *tray);
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_CreateTraySubmenu(SDL_TrayEntry *entry);
extern SDL_DECLSPEC SDL_TrayEntry *SDLCALL SDL_AppendTrayEntry(SDL_TrayMenu *menu, const char *label, SDL_TrayEntryFlags flags);
extern SDL_DECLSPEC void SDLCALL SDL_AppendTraySeparator(SDL_TrayMenu *menu);
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, SDL_bool checked);
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GetTrayEntryChecked(SDL_TrayEntry *entry);
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, SDL_bool enabled);
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry);
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata);
extern SDL_DECLSPEC void SDLCALL SDL_DestroyTray(SDL_Tray *tray);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_tray_h_ */
