/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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

#ifndef SDL_unixtray_h_
#define SDL_unixtray_h_

#include "../../core/linux/SDL_dbus.h"
#include "../SDL_tray_utils.h"

typedef struct SDL_TrayDriver
{
    const char *name;

    unsigned int count;

    SDL_Tray *(*CreateTray)(struct SDL_TrayDriver *, SDL_Surface *, const char *);
    void (*DestroyTray)(SDL_Tray *);
    void (*UpdateTray)(SDL_Tray *);
    void (*SetTrayIcon)(SDL_Tray *, SDL_Surface *);
    void (*SetTrayTooltip)(SDL_Tray *, const char *);
    SDL_TrayMenu *(*CreateTrayMenu)(SDL_Tray *);
    SDL_TrayEntry *(*InsertTrayEntryAt)(SDL_TrayMenu *, int, const char *, SDL_TrayEntryFlags);
    SDL_TrayMenu *(*CreateTraySubmenu)(SDL_TrayEntry *);
    SDL_TrayMenu *(*GetTraySubmenu)(SDL_TrayEntry *);
    SDL_TrayEntry **(*GetTrayEntries)(SDL_TrayMenu *, int *);
    void (*RemoveTrayEntry)(SDL_TrayEntry *);
    void (*SetTrayEntryCallback)(SDL_TrayEntry *, SDL_TrayCallback, void *);

    void (*DestroyDriver)(struct SDL_TrayDriver *);
} SDL_TrayDriver;

struct SDL_TrayMenu
{
    SDL_Tray *parent_tray;
    SDL_TrayEntry *parent_entry;
};

struct SDL_TrayEntry
{
    SDL_TrayMenu *parent;
};

struct SDL_Tray
{
    SDL_TrayDriver *driver;

    SDL_TrayMenu *menu;
};

#ifdef SDL_USE_LIBDBUS
extern SDL_TrayDriver *SDL_Tray_CreateDBusDriver(void);
#endif

#endif // SDL_unixtray_h_
