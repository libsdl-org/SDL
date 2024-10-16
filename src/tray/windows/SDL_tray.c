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

#include "SDL_internal.h"

#include "../../video/windows/SDL_surface_utils.h"

#include <windows.h>
#include <shellapi.h>

#include <stdlib.h>

#define WM_TRAYICON (WM_USER + 1)

struct SDL_TrayMenu {
    HMENU hMenu;

    size_t nEntries;
    SDL_TrayEntry **entries;

    SDL_Tray *parent_tray;
    SDL_TrayEntry *parent_entry;
};

struct SDL_TrayEntry {
    SDL_TrayMenu *parent;
    UINT_PTR id;

    SDL_TrayCallback callback;
    void *userdata;
    SDL_TrayMenu *submenu;
};

struct SDL_Tray {
    NOTIFYICONDATAW nid;
    HWND hwnd;
    HICON icon;
    SDL_TrayMenu *menu;
};

static UINT_PTR get_next_id(void)
{
    static UINT_PTR next_id = 0;
    return ++next_id;
}

static SDL_TrayEntry *find_entry_in_menu(SDL_TrayMenu *menu, UINT_PTR id)
{
    for (size_t i = 0; i < menu->nEntries; i++) {
        SDL_TrayEntry *entry = menu->entries[i];

        if (entry->id == id) {
            return entry;
        }

        if (entry->submenu) {
            SDL_TrayEntry *e = find_entry_in_menu(entry->submenu, id);

            if (e) {
                return e;
            }
        }
    }

    return NULL;
}

static SDL_TrayEntry *find_entry_with_id(SDL_Tray *tray, UINT_PTR id)
{
    if (!tray->menu) {
        return NULL;
    }

    return find_entry_in_menu(tray->menu, id);
}

LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SDL_Tray *tray = (SDL_Tray *) GetWindowLongPtr(hwnd, GWLP_USERDATA);
    SDL_TrayEntry *entry = NULL;

    if (!tray) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(tray->menu->hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            }
            break;

        case WM_COMMAND:
            entry = find_entry_with_id(tray, LOWORD(wParam));
            if (entry && entry->callback) {
                entry->callback(entry->userdata, entry);
            }
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

static void DestroySDLMenu(SDL_TrayMenu *menu)
{
    for (size_t i = 0; i < menu->nEntries; i++) {
        if (menu->entries[i] && menu->entries[i]->submenu) {
            DestroySDLMenu(menu->entries[i]->submenu);
        }
        SDL_free(menu->entries[i]);
    }
    SDL_free(menu->entries);
    DestroyMenu(menu->hMenu);
    SDL_free(menu);
}

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    SDL_Tray *tray = SDL_malloc(sizeof(SDL_Tray));

    if (!tray) {
        return NULL;
    }

    tray->hwnd = NULL;
    tray->menu = NULL;

    wchar_t classname[32] = { L'\0' };
    SDL_swprintf(classname, 32, L"SDLTray%d", (unsigned int) get_next_id());

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(WNDCLASS));
    wc.lpfnWndProc = TrayWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = classname;

    RegisterClassW(&wc);

    tray->hwnd = CreateWindowExW(0, classname, L"", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hInstance, NULL);
    ShowWindow(tray->hwnd, SW_HIDE);

    ZeroMemory(&tray->nid, sizeof(NOTIFYICONDATAW));
    tray->nid.cbSize = sizeof(NOTIFYICONDATAW);
    tray->nid.hWnd = tray->hwnd;
    tray->nid.uID = 1;
    tray->nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    tray->nid.uCallbackMessage = WM_TRAYICON;
    mbstowcs_s(NULL, tray->nid.szTip, sizeof(tray->nid.szTip) / sizeof(*tray->nid.szTip), tooltip, _TRUNCATE);

    if (icon) {
        tray->nid.hIcon = CreateIconFromSurface(icon);

        if (!tray->nid.hIcon) {
            tray->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }

        tray->icon = tray->nid.hIcon;
    } else {
        tray->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        tray->icon = tray->nid.hIcon;
    }

    Shell_NotifyIconW(NIM_ADD, &tray->nid);

    SetWindowLongPtr(tray->hwnd, GWLP_USERDATA, (LONG_PTR) tray);

    return tray;
}

void SDL_SetTrayIcon(SDL_Tray *tray, SDL_Surface *icon)
{
    if (tray->icon) {
        DestroyIcon(tray->icon);
    }

    if (icon) {
        tray->nid.hIcon = CreateIconFromSurface(icon);

        if (!tray->nid.hIcon) {
            tray->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        }

        tray->icon = tray->nid.hIcon;
    } else {
        tray->nid.hIcon = LoadIcon(NULL, IDI_APPLICATION);
        tray->icon = tray->nid.hIcon;
    }

    Shell_NotifyIconW(NIM_MODIFY, &tray->nid);
}

void SDL_SetTrayTooltip(SDL_Tray *tray, const char *tooltip)
{
    if (tooltip) {
        mbstowcs_s(NULL, tray->nid.szTip, sizeof(tray->nid.szTip) / sizeof(*tray->nid.szTip), tooltip, _TRUNCATE);
    } else {
        tray->nid.szTip[0] = '\0';
    }

    Shell_NotifyIconW(NIM_MODIFY, &tray->nid);
}

SDL_TrayMenu *SDL_CreateTrayMenu(SDL_Tray *tray)
{
    tray->menu = SDL_malloc(sizeof(SDL_TrayMenu));

    if (!tray->menu) {
        return NULL;
    }

    SDL_memset((void *) tray->menu, 0, sizeof(*tray->menu));

    tray->menu->hMenu = CreatePopupMenu();
    tray->menu->parent_tray = tray;

    return tray->menu;
}

SDL_TrayMenu *SDL_GetTrayMenu(SDL_Tray *tray)
{
    return tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    if (!entry->submenu) {
        SDL_SetError("Attempt to create a submenu on an entry that wasn't prepared for a submenu");
    }

    return entry->submenu;
}

SDL_TrayMenu *SDL_GetTraySubmenu(SDL_TrayEntry *entry)
{
    return entry->submenu;
}

const SDL_TrayEntry **SDL_GetTrayEntries(SDL_TrayMenu *menu, int *size)
{
    if (size) {
        *size = (int) menu->nEntries;
    }

    return (const SDL_TrayEntry **) menu->entries;
}

SDL_TrayEntry *SDL_GetTrayEntryAt(SDL_TrayMenu *menu, int pos)
{
    if (((size_t) pos) >= menu->nEntries || pos < 0) {
        return NULL;
    }

    return menu->entries[pos];
}

void SDL_RemoveTrayEntry(SDL_TrayEntry *entry)
{
    SDL_TrayMenu *menu = entry->parent;

    bool found = false;
    for (size_t i = 0; i < menu->nEntries - 1; i++) {
        if (menu->entries[i] == entry) {
            found = true;
        }

        if (found) {
            menu->entries[i] = menu->entries[i + 1];
        }
    }

    if (entry->submenu) {
        DestroySDLMenu(entry->submenu);
    }

    menu->nEntries--;
    SDL_TrayEntry ** new_entries = SDL_realloc(menu->entries, menu->nEntries * sizeof(SDL_TrayEntry *));

    /* Not sure why shrinking would fail, but even if it does, we can live with a "too big" array */
    if (new_entries) {
        menu->entries = new_entries;
    }

    if (!DeleteMenu(menu->hMenu, (UINT) entry->id, MF_BYCOMMAND)) {
        /* This is somewhat useless since we don't return anything, but might help with eventual bugs */
        SDL_SetError("Couldn't destroy tray entry");
    }

    SDL_free(entry);
}

void SDL_RemoveTrayEntryAt(SDL_TrayMenu *menu, int pos)
{
    if (pos < 0 || ((size_t) pos) >= menu->nEntries) {
        return;
    }

    SDL_TrayEntry *entry = menu->entries[pos];

    if (entry->submenu) {
        DestroySDLMenu(entry->submenu);
    }

    for (size_t i = pos; i < menu->nEntries - 1; i++) {
        menu->entries[i] = menu->entries[i + 1];
    }

    menu->nEntries--;
    SDL_TrayEntry ** new_entries = SDL_realloc(menu->entries, menu->nEntries * sizeof(SDL_TrayEntry *));

    /* Not sure why shrinking would fail, but even if it does, we can live with a "too big" array */
    if (new_entries) {
        menu->entries = new_entries;
    }

    if (!DeleteMenu(menu->hMenu, pos, MF_BYPOSITION)) {
        /* This is somewhat useless since we don't return anything, but might help with eventual bugs */
        SDL_SetError("Couldn't destroy tray entry");
    }

    SDL_free(entry);
}

SDL_TrayEntry *SDL_InsertTrayEntryAt(SDL_TrayMenu *menu, int pos, const char *label, SDL_TrayEntryFlags flags)
{
    if (pos < 0 || ((size_t) pos) > menu->nEntries) {
        return NULL;
    }

    SDL_TrayEntry *entry = SDL_malloc(sizeof(SDL_TrayEntry));

    if (!entry) {
        return NULL;
    }

    SDL_TrayMenu *submenu = NULL;

    /* Allocate early so freeing on error is easier */
    int label_wlen = MultiByteToWideChar(CP_UTF8, 0, label, -1, NULL, 0);
    wchar_t *label_w = (wchar_t *)SDL_malloc(label_wlen * sizeof(wchar_t));
    if (!label_w) {
        SDL_free(entry);
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, label, -1, label_w, label_wlen);

    if (flags & SDL_TRAYENTRY_SUBMENU) {
        submenu = SDL_malloc(sizeof(SDL_TrayMenu));

        if (!submenu) {
            SDL_free(label_w);
            SDL_free(entry);
            return NULL;
        }

        submenu->hMenu = CreatePopupMenu();
        submenu->nEntries = 0;
        submenu->entries = NULL;
    }

    entry->parent = menu;
    entry->callback = NULL;
    entry->userdata = NULL;
    entry->submenu = submenu;

    if (submenu) {
        entry->id = (UINT_PTR) submenu->hMenu;
    } else {
        entry->id = get_next_id();
    }

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **) SDL_realloc(menu->entries, (menu->nEntries + 1) * sizeof(SDL_TrayEntry **));

    if (!new_entries) {
        SDL_free(entry);
        DestroyMenu(submenu->hMenu);
        SDL_free(submenu);
        return NULL;
    }

    menu->entries = new_entries;
    menu->nEntries++;

    for (int i = (int) menu->nEntries - 1; i > pos; i--) {
        menu->entries[i] = menu->entries[i - 1];
    }

    new_entries[pos] = entry;

    UINT mf = MF_STRING | MF_BYPOSITION;
    if (flags & SDL_TRAYENTRY_SUBMENU) {
        mf = MF_POPUP;
    }

    if (flags & SDL_TRAYENTRY_DISABLED) {
        mf |= MF_DISABLED | MF_GRAYED;
    }

    if (flags & SDL_TRAYENTRY_CHECKED) {
        mf |= MF_CHECKED;
    }

    InsertMenuW(menu->hMenu, (pos == menu->nEntries) ? -1 : pos, mf, entry->id, label_w);

    return entry;
}

SDL_TrayEntry *SDL_AppendTrayEntry(SDL_TrayMenu *menu, const char *label, SDL_TrayEntryFlags flags)
{
    SDL_TrayEntry *entry = SDL_malloc(sizeof(SDL_TrayEntry));

    if (!entry) {
        return NULL;
    }

    SDL_TrayMenu *submenu = NULL;

    /* Allocate early so freeing on error is easier */
    int label_wlen = MultiByteToWideChar(CP_UTF8, 0, label, -1, NULL, 0);
    wchar_t *label_w = (wchar_t *)SDL_malloc(label_wlen * sizeof(wchar_t));
    if (!label_w) {
        SDL_free(entry);
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, label, -1, label_w, label_wlen);

    if (flags & SDL_TRAYENTRY_SUBMENU) {
        submenu = SDL_malloc(sizeof(SDL_TrayMenu));

        if (!submenu) {
            SDL_free(label_w);
            SDL_free(entry);
            return NULL;
        }

        submenu->hMenu = CreatePopupMenu();
        submenu->nEntries = 0;
        submenu->entries = NULL;
    }

    entry->parent = menu;
    entry->callback = NULL;
    entry->userdata = NULL;
    entry->submenu = submenu;

    if (submenu) {
        entry->id = (UINT_PTR) submenu->hMenu;
    } else {
        entry->id = get_next_id();
    }

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **) SDL_realloc(menu->entries, (menu->nEntries + 1) * sizeof(SDL_TrayEntry **));

    if (!new_entries) {
        SDL_free(entry);
        DestroyMenu(submenu->hMenu);
        SDL_free(submenu);
        return NULL;
    }

    new_entries[menu->nEntries] = entry;
    menu->entries = new_entries;
    menu->nEntries++;

    UINT mf = MF_STRING;
    if (flags & SDL_TRAYENTRY_SUBMENU) {
        mf = MF_POPUP;
    }

    if (flags & SDL_TRAYENTRY_DISABLED) {
        mf |= MF_DISABLED | MF_GRAYED;
    }

    if (flags & SDL_TRAYENTRY_CHECKED) {
        mf |= MF_CHECKED;
    }

    AppendMenuW(menu->hMenu, mf, entry->id, label_w);

    return entry;
}

void SDL_AppendTraySeparator(SDL_TrayMenu *menu)
{
    AppendMenuW(menu->hMenu, MF_SEPARATOR, 0, NULL);
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked)
{
    CheckMenuItem(entry->parent->hMenu, (UINT) entry->id, checked ? MF_CHECKED : MF_UNCHECKED);
}

bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    MENUITEMINFOW mii;
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_STATE;

    GetMenuItemInfoW(entry->parent->hMenu, (UINT) entry->id, FALSE, &mii);

    return !!(mii.fState & MFS_CHECKED);
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled)
{
    EnableMenuItem(entry->parent->hMenu, (UINT) entry->id, MF_BYCOMMAND | (enabled ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
}

bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    MENUITEMINFOW mii;
    mii.cbSize = sizeof(MENUITEMINFOW);
    mii.fMask = MIIM_STATE;

    GetMenuItemInfoW(entry->parent->hMenu, (UINT) entry->id, FALSE, &mii);

    return !!(mii.fState & MFS_ENABLED);
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    entry->callback = callback;
    entry->userdata = userdata;
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    if (!tray) {
        return;
    }

    Shell_NotifyIconW(NIM_DELETE, &tray->nid);

    if (tray->menu) {
        DestroySDLMenu(tray->menu);
    }

    if (tray->icon) {
        DestroyIcon(tray->icon);
    }

    if (tray->hwnd) {
        DestroyWindow(tray->hwnd);
    }

    SDL_free(tray);
}
