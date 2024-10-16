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

/**
 * Flags that control the creation of system tray entries.
 *
 * Some of these flags are mandatory; exactly one of them must be specified at
 * the time a tray entry is created. Other flags are optional; zero or more of
 * those can be OR'ed together with the mandatory flag.
 *
 * \since This datatype is available since SDL 3.0.0.
 *
 * \sa SDL_InsertTrayEntryAt
 * \sa SDL_AppendTrayEntry
 */
typedef Uint32 SDL_TrayEntryFlags;

/* The mandatory flags aren't incremental because internal code relies on bitwise operations. */
#define SDL_TRAYENTRY_BUTTON      0x00000001u /**< Make the entry a simple button. Mandatory. */
#define SDL_TRAYENTRY_CHECKBOX    0x00000002u /**< Make the entry a checkbox. Mandatory. */
#define SDL_TRAYENTRY_SUBMENU     0x00000004u /**< Prepatre the entry to have a submenu. Mandatory */
#define SDL_TRAYENTRY_DISABLED    0x80000000u /**< Make the entry disabled. Optional. */
#define SDL_TRAYENTRY_CHECKED     0x40000000u /**< Make the entry checked. This is valid only for checkboxes. Optional. */

/**
 * A callback that is invoked when a tray entry is selected.
 *
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 * \param entry the tray entry that was selected.
 */
typedef void (SDLCALL *SDL_TrayCallback)(void *userdata, SDL_TrayEntry *entry);

/**
 * Create an icon to be placed in the operating system's tray, or equivalent.
 *
 * Many platforms advise not using a system tray unless persistence is a
 * necessary feature. Avoid needlessly creating a tray icon, as the user may
 * feel like it clutters their interface.
 *
 * Using tray icons require the video subsystem.
 *
 * \param icon a surface to be used as icon. May be NULL.
 * \param tooltip a tooltip to be displayed when the mouse hovers the icon. Not
 *        supported on all platforms. May be NULL.
 * \returns The newly created system tray icon.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_SetTrayIcon
 * \sa SDL_SetTrayTooltip
 * \sa SDL_CreateTrayMenu
 * \sa SDL_DestroyTray
 */
extern SDL_DECLSPEC SDL_Tray *SDLCALL SDL_CreateTray(SDL_Surface *icon, const char *tooltip);

/**
 * Updates the system tray icon's icon.
 *
 * \param tray the tray icon to be updated.
 * \param icon the new icon. May be NULL.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTray
 * \sa SDL_SetTrayTooltip
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon);

/**
 * Updates the system tray icon's tooltip.
 *
 * \param tray the tray icon to be updated.
 * \param tooltip the new tooltip. May be NULL.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTray
 * \sa SDL_SetTrayIcon
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip);

/**
 * Create a menu for a system tray.
 *
 * This should be called at most once per tray icon.
 *
 * This function does the same thing as SDL_CreateTraySubmenu, except that it
 * takes a SDL_Tray instead of a SDL_TrayEntry.
 *
 * A menu does not need to be destroyed; it will be destroyed with the tray.
 *
 * \param tray the tray to bind the menu to.
 * \returns the newly created menu.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTray
 * \sa SDL_CreateTraySubmenu
 * \sa SDL_AppendTrayEntry
 * \sa SDL_AppendTraySeparator
 */
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_CreateTrayMenu(SDL_Tray *tray);

/**
 * Create a submenu for a system tray entry.
 *
 * This should be called at most once per tray entry.
 *
 * This function does the same thing as SDL_CreateTrayMenu, except that it
 * takes a SDL_TrayEntry instead of a SDL_Tray.
 *
 * A menu does not need to be destroyed; it will be destroyed with the tray.
 *
 * \param entry the tray entry to bind the menu to.
 * \returns the newly created menu.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTrayMenu
 * \sa SDL_AppendTrayEntry
 * \sa SDL_AppendTraySeparator
 */
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_CreateTraySubmenu(SDL_TrayEntry *entry);

/**
 * Gets a previously created tray menu.
 *
 * You should have called SDL_CreateTrayMenu() on the tray object. This
 * function allows you to fetch it again later.
 *
 * This function does the same thing as SDL_GetTraySubmenu(), except that it
 * takes a SDL_Tray instead of a SDL_TrayEntry.
 *
 * A menu does not need to be destroyed; it will be destroyed with the tray.
 *
 * \param tray the tray entry to bind the menu to.
 * \returns the newly created menu.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTrayMenu
 * \sa SDL_CreateOrGetTrayMenu
 */
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_GetTrayMenu(SDL_Tray *tray);

/**
 * Gets a previously created tray menu, or create it if it doesn't exist.
 *
 * This is a convenience function.
 *
 * \param tray the tray entry to bind the menu to.
 * \returns the newly created menu.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTrayMenu
 * \sa SDL_GetTrayMenu
 */
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_CreateOrGetTrayMenu(SDL_Tray *tray);

/**
 * Gets a previously created tray entry submenu.
 *
 * You should have called SDL_CreateTraySubenu() on the entry object. This
 * function allows you to fetch it again later.
 *
 * This function does the same thing as SDL_GetTrayMenu(), except that it
 * takes a SDL_TrayEntry instead of a SDL_Tray.
 *
 * A menu does not need to be destroyed; it will be destroyed with the tray.
 *
 * \param entry the tray entry to bind the menu to.
 * \returns the newly created menu.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTrayMenu
 * \sa SDL_CreateOrGetTrayMenu
 */
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_GetTraySubmenu(SDL_TrayEntry *entry);

/**
 * Gets a previously created tray menu, or create it if it doesn't exist.
 *
 * This is a convenience function.
 *
 * \param entry the tray entry to bind the menu to.
 * \returns the newly created menu.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTrayMenu
 * \sa SDL_GetTrayMenu
 */
extern SDL_DECLSPEC SDL_TrayMenu *SDLCALL SDL_CreateOrGetTraySubmenu(SDL_TrayEntry *entry);

/**
 * Returns a list of entries in the menu, in order.
 *
 * \param menu The menu to get entries from.
 * \param size An optional pointer to obtain the number of entries in the menu.
 * \returns the entries within the given menu. The pointer becomes invalid when
 *          any function that inserts or deletes entries in the menu is called.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTrayEntryAt
 * \sa SDL_RemoveTrayEntry
 * \sa SDL_RemoveTrayEntryAt
 * \sa SDL_InsertTrayEntryAt
 * \sa SDL_AppendTrayEntry
 */
extern SDL_DECLSPEC const SDL_TrayEntry **SDLCALL SDL_GetTrayEntries(SDL_TrayMenu *menu, int *size);

/**
 * Returns a tray entry at a given position.
 *
 * \param menu The menu to get the entry from.
 * \param pos the position of the desired entry.
 * \returns the entry, of NULL if pos is out of bounds.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTrayEntries
 * \sa SDL_RemoveTrayEntry
 * \sa SDL_RemoveTrayEntryAt
 * \sa SDL_InsertTrayEntryAt
 * \sa SDL_AppendTrayEntry
 */
extern SDL_DECLSPEC SDL_TrayEntry *SDLCALL SDL_GetTrayEntryAt(SDL_TrayMenu *menu, int pos);

/**
 * Removes a tray entry.
 *
 * \param entry The entry to be deleted.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTrayEntries
 * \sa SDL_GetTrayEntryAt
 * \sa SDL_RemoveTrayEntryAt
 * \sa SDL_InsertTrayEntryAt
 * \sa SDL_AppendTrayEntry
 */
extern SDL_DECLSPEC void SDLCALL SDL_RemoveTrayEntry(SDL_TrayEntry *entry);

/**
 * Removes a tray entry at a given position.
 *
 * \param menu The menu to get the entry from.
 * \param pos the position of the entry to be deleted.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTrayEntries
 * \sa SDL_GetTrayEntryAt
 * \sa SDL_RemoveTrayEntry
 * \sa SDL_InsertTrayEntryAt
 * \sa SDL_AppendTrayEntry
 */
extern SDL_DECLSPEC void SDLCALL SDL_RemoveTrayEntryAt(SDL_TrayMenu *menu, int pos);

/**
 * Insert a tray entry at a given position.
 *
 * An entry does not need to be destroyed; it will be destroyed with the tray.
 *
 * \param menu the menu to append the entry to.
 * \param pos the desired position for the new entry. Entries at or following
 *            this place will be moved.
 * \param label the label to be displayed on the entry.
 * \param flags a combination of flags, some of which are mandatory.
 * \returns the newly created entry, or NULL if pos is out of bounds.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTrayEntries
 * \sa SDL_GetTrayEntryAt
 * \sa SDL_RemoveTrayEntry
 * \sa SDL_RemoveTrayEntryAt
 * \sa SDL_AppendTrayEntry
 * \sa SDL_TrayEntryFlags
 * \sa SDL_CreateTrayMenu
 * \sa SDL_CreateTraySubmenu
 * \sa SDL_SetTrayEntryChecked
 * \sa SDL_GetTrayEntryChecked
 * \sa SDL_SetTrayEntryEnabled
 * \sa SDL_GetTrayEntryEnabled
 * \sa SDL_SetTrayEntryCallback
 * \sa SDL_AppendTraySeparator
 */
extern SDL_DECLSPEC SDL_TrayEntry *SDLCALL SDL_InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags);

/**
 * Create a menu item (entry) and append it to the given menu.
 *
 * An entry does not need to be destroyed; it will be destroyed with the tray.
 *
 * \param menu the menu to append the entry to.
 * \param label the label to be displayed on the entry.
 * \param flags a combination of flags, some of which are mandatory.
 * \returns the newly created entry.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetTrayEntries
 * \sa SDL_GetTrayEntryAt
 * \sa SDL_RemoveTrayEntry
 * \sa SDL_RemoveTrayEntryAt
 * \sa SDL_InsertTrayEntryAt
 * \sa SDL_TrayEntryFlags
 * \sa SDL_CreateTrayMenu
 * \sa SDL_CreateTraySubmenu
 * \sa SDL_SetTrayEntryChecked
 * \sa SDL_GetTrayEntryChecked
 * \sa SDL_SetTrayEntryEnabled
 * \sa SDL_GetTrayEntryEnabled
 * \sa SDL_SetTrayEntryCallback
 * \sa SDL_AppendTraySeparator
 */
extern SDL_DECLSPEC SDL_TrayEntry *SDLCALL SDL_AppendTrayEntry(SDL_TrayMenu *menu, const char *label, SDL_TrayEntryFlags flags);

/**
 * Append a separator to the given menu.
 *
 * \param menu the menu to append the entry to.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTrayMenu
 * \sa SDL_CreateTraySubmenu
 * \sa SDL_AppendTrayEntry
 */
extern SDL_DECLSPEC void SDLCALL SDL_AppendTraySeparator(SDL_TrayMenu *menu);

/**
 * Sets whether or not an entry is checked.
 *
 * The entry must have been created with the SDL_TRAYENTRY_CHECKBOX flag.
 *
 * \param entry the entry to be updated.
 * \param checked SDL_TRUE if the entry should be checked; SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_TrayEntryFlags
 * \sa SDL_AppendTrayEntry
 * \sa SDL_GetTrayEntryChecked
 * \sa SDL_SetTrayEntryEnabled
 * \sa SDL_GetTrayEntryEnabled
 * \sa SDL_SetTrayEntryCallback
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked);

/**
 * Gets whether or not an entry is checked.
 *
 * The entry must have been created with the SDL_TRAYENTRY_CHECKBOX flag.
 *
 * \param entry the entry to be read.
 * \returns SDL_TRUE if the entry is checked; SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_TrayEntryFlags
 * \sa SDL_AppendTrayEntry
 * \sa SDL_SetTrayEntryChecked
 * \sa SDL_SetTrayEntryEnabled
 * \sa SDL_GetTrayEntryEnabled
 * \sa SDL_SetTrayEntryCallback
 */
extern SDL_DECLSPEC bool SDLCALL SDL_GetTrayEntryChecked(SDL_TrayEntry *entry);

/**
 * Sets whether or not an entry is enabled.
 *
 * \param entry the entry to be updated.
 * \param enabled SDL_TRUE if the entry should be enabled; SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AppendTrayEntry
 * \sa SDL_SetTrayEntryChecked
 * \sa SDL_GetTrayEntryChecked
 * \sa SDL_GetTrayEntryEnabled
 * \sa SDL_SetTrayEntryCallback
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled);

/**
 * Gets whether or not an entry is enabled.
 *
 * \param entry the entry to be read.
 * \returns SDL_TRUE if the entry is enabled; SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_AppendTrayEntry
 * \sa SDL_SetTrayEntryChecked
 * \sa SDL_GetTrayEntryChecked
 * \sa SDL_SetTrayEntryEnabled
 * \sa SDL_SetTrayEntryCallback
 */
extern SDL_DECLSPEC bool SDLCALL SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry);

/**
 * Sets a callback to be invoked when the entry is selected.
 *
 * \param entry the entry to be updated.
 * \param callback a callback to be invoked when the entry is selected.
 * \param userdata an optional pointer to pass extra data to the callback when
 *                 it will be invoked.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_TrayCallback
 * \sa SDL_AppendTrayEntry
 * \sa SDL_SetTrayEntryChecked
 * \sa SDL_GetTrayEntryChecked
 * \sa SDL_SetTrayEntryEnabled
 * \sa SDL_GetTrayEntryEnabled
 */
extern SDL_DECLSPEC void SDLCALL SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata);

/**
 * Destroys a tray object.
 *
 * This also destroys all associated menus and entries.
 *
 * \param tray the tray icon to be destroyed.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_CreateTray
 */
extern SDL_DECLSPEC void SDLCALL SDL_DestroyTray(SDL_Tray *tray);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include <SDL3/SDL_close_code.h>

#endif /* SDL_tray_h_ */
