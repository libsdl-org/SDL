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

#include "../../core/linux/SDL_dbus.h"
#include "../SDL_tray_utils.h"
#include "SDL_unixtray.h"

static SDL_TrayDriver *driver = NULL;

void SDL_UpdateTrays(void)
{
    SDL_Tray **trays;
    int active_trays;
    int count;
    int i;

    active_trays = SDL_GetActiveTrayCount();
    if (!active_trays) {
        return;
    }

    trays = SDL_calloc(active_trays, sizeof(SDL_Tray *));
    if (!trays) {
        return;
    }

    count = SDL_GetObjects(SDL_OBJECT_TYPE_TRAY, (void **)trays, active_trays);
    SDL_assert(count == active_trays);
    for (i = 0; i < count; i++) {
        if (trays[i]) {
            if (SDL_ObjectValid(trays[i], SDL_OBJECT_TYPE_TRAY)) {
                trays[i]->driver->UpdateTray(trays[i]);
            }
        }
    }

    SDL_free(trays);
}

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    SDL_Tray *tray;

    if (!driver) {
#ifdef SDL_USE_LIBDBUS
        driver = SDL_Tray_CreateDBusDriver();
#endif
    }

    if (driver) {
        tray = driver->CreateTray(driver, icon, tooltip);
        if (tray) {
            SDL_RegisterTray(tray);
        }
    } else {
        tray = NULL;
    }

    return tray;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    CHECK_PARAM(!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY))
    {
        SDL_InvalidParamError("tray");
        return;
    }

    tray->driver->SetTrayIcon(tray, icon);
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    CHECK_PARAM(!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY))
    {
        SDL_InvalidParamError("tray");
        return;
    }

    tray->driver->SetTrayTooltip(tray, tooltip);
}

SDL_TrayMenu *SDL_CreateTrayMenu(SDL_Tray *tray)
{
    CHECK_PARAM(!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY))
    {
        SDL_InvalidParamError("tray");
        return NULL;
    }

    return tray->driver->CreateTrayMenu(tray);
}

SDL_TrayMenu *SDL_GetTrayMenu(SDL_Tray *tray)
{
    CHECK_PARAM(!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY))
    {
        SDL_InvalidParamError("tray");
        return NULL;
    }

    return tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry)
    {
        SDL_InvalidParamError("entry");
        return NULL;
    }

    return entry->parent->parent_tray->driver->CreateTraySubmenu(entry);
}

SDL_TrayMenu *SDL_GetTraySubmenu(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry)
    {
        SDL_InvalidParamError("entry");
        return NULL;
    }
    
    return entry->parent->parent_tray->driver->GetTraySubmenu(entry);
}

const SDL_TrayEntry **SDL_GetTrayEntries(SDL_TrayMenu *menu, int *count)
{
    CHECK_PARAM(!menu)
    {
        SDL_InvalidParamError("menu");
        return NULL;
    }
    
    CHECK_PARAM(!count)
    {
        SDL_InvalidParamError("count");
        return NULL;
    }
    
    return (const SDL_TrayEntry **)menu->parent_tray->driver->GetTrayEntries(menu, count);
}

void SDL_RemoveTrayEntry(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry)
    {
        SDL_InvalidParamError("entry");
        return;
    }

    entry->parent->parent_tray->driver->RemoveTrayEntry(entry);
}

SDL_TrayEntry *SDL_InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags)
{
    CHECK_PARAM(!menu)
    {
        SDL_InvalidParamError("menu");
        return NULL;
    }

    return menu->parent_tray->driver->InsertTrayEntryAt(menu, pos, label, flags);
}

void SDL_SetTrayEntryLabel(SDL_TrayEntry *entry, const char *label)
{
}

const char *SDL_GetTrayEntryLabel(SDL_TrayEntry *entry)
{
    SDL_InvalidParamError("entry");
    return NULL;
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked)
{
}

bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    return SDL_InvalidParamError("entry");
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled)
{
}

bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    return SDL_InvalidParamError("entry");
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    CHECK_PARAM(!entry)
    {
        SDL_InvalidParamError("entry");
        return;
    }
    
    CHECK_PARAM(!callback)
    {
        SDL_InvalidParamError("callback");
        return;
    }
    
    entry->parent->parent_tray->driver->SetTrayEntryCallback(entry, callback, userdata);
}

void SDL_ClickTrayEntry(SDL_TrayEntry *entry)
{
}

SDL_TrayMenu *SDL_GetTrayEntryParent(SDL_TrayEntry *entry)
{
    CHECK_PARAM(!entry)
    {
        SDL_InvalidParamError("entry");
        return NULL;
    }
    
    return entry->parent;
}

SDL_TrayEntry *SDL_GetTrayMenuParentEntry(SDL_TrayMenu *menu)
{
    CHECK_PARAM(!menu)
    {
        SDL_InvalidParamError("menu");
        return NULL;
    }
    
    return menu->parent_entry;
}

SDL_Tray *SDL_GetTrayMenuParentTray(SDL_TrayMenu *menu)
{
    CHECK_PARAM(!menu)
    {
        SDL_InvalidParamError("menu");
        return NULL;
    }
    
    return menu->parent_tray;
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    if (!SDL_ObjectValid(tray, SDL_OBJECT_TYPE_TRAY)) {
        return;
    }
    
    tray->driver->DestroyTray(tray);
    SDL_UnregisterTray(tray);
    if (!SDL_GetActiveTrayCount()) {
        driver->DestroyDriver(driver);
    }
}
