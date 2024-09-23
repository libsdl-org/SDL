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

#include <windows.h>
#include <shellapi.h>

#include <stdlib.h>

#define WM_TRAYICON (WM_USER + 1)

struct SDL_TrayMenu {
    SDL_Tray *tray;
    HMENU hMenu;
};

struct SDL_TrayEntry {
    SDL_Tray *tray;
    SDL_TrayMenu *menu;
    UINT_PTR id;

    SDL_TrayCallback callback;
    void *userdata;
    SDL_TrayMenu submenu;
};

struct SDL_Tray {
    NOTIFYICONDATAW nid;
    HWND hwnd;
    HICON icon;
    SDL_TrayMenu menu;

    size_t nEntries;
    SDL_TrayEntry **entries;
};

static UINT_PTR get_next_id(void)
{
    static UINT_PTR next_id = 0;
    return ++next_id;
}

LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    SDL_Tray *tray = (SDL_Tray *) GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (!tray) {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg) {
        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP || lParam == WM_LBUTTONUP) {
                POINT pt;
                GetCursorPos(&pt);
                SetForegroundWindow(hwnd);
                TrackPopupMenu(tray->menu.hMenu, TPM_BOTTOMALIGN | TPM_RIGHTALIGN, pt.x, pt.y, 0, hwnd, NULL);
            }
            break;

        case WM_COMMAND:
            for (int i = 0; i < tray->nEntries; i++) {
                if (tray->entries[i]->id == LOWORD(wParam)) {
                    SDL_TrayEntry *entry = tray->entries[i];
                    if (entry->callback) {
                        entry->callback(entry->userdata, entry);
                    }
                    break;
                }
            }
            break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

HICON CreateIconFromRGBA(int width, int height, const BYTE* rgbaData) {
    // Create a BITMAPINFO structure
    BITMAPINFO bmpInfo;
    ZeroMemory(&bmpInfo, sizeof(BITMAPINFO));
    bmpInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmpInfo.bmiHeader.biWidth = width;
    bmpInfo.bmiHeader.biHeight = -height; // Negative to indicate a top-down bitmap
    bmpInfo.bmiHeader.biPlanes = 1;
    bmpInfo.bmiHeader.biBitCount = 32; // 32 bits for RGBA
    bmpInfo.bmiHeader.biCompression = BI_RGB;

    // Create a DIB section
    HDC hdc = GetDC(NULL);
    void* pBits = NULL;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmpInfo, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hBitmap) {
        ReleaseDC(NULL, hdc);
        return NULL; // Handle error
    }

    // Copy the RGBA data to the bitmap
    memcpy(pBits, rgbaData, width * height * 4); // 4 bytes per pixel

    // Create a mask bitmap (1 bit per pixel)
    HBITMAP hMask = CreateBitmap(width, height, 1, 1, NULL);
    if (!hMask) {
        DeleteObject(hBitmap);
        ReleaseDC(NULL, hdc);
        return NULL; // Handle error
    }

    // Create a compatible DC for the mask
    HDC hdcMem = CreateCompatibleDC(hdc);
    HGDIOBJ oldBitmap = SelectObject(hdcMem, hMask);

    // Create the mask based on the alpha channel
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            BYTE* pixel = (BYTE*)pBits + (y * width + x) * 4;
            BYTE alpha = pixel[3]; // Alpha channel
            // Set the mask pixel: 0 for transparent, 1 for opaque
            COLORREF maskColor = (alpha == 0) ? RGB(0, 0, 0) : RGB(255, 255, 255);
            SetPixel(hdcMem, x, y, maskColor);
        }
    }

    // Create the icon using CreateIconIndirect
    ICONINFO iconInfo;
    iconInfo.fIcon = TRUE; // TRUE for icon, FALSE for cursor
    iconInfo.xHotspot = 0; // Hotspot x
    iconInfo.yHotspot = 0; // Hotspot y
    iconInfo.hbmMask = hMask; // Mask bitmap
    iconInfo.hbmColor = hBitmap; // Color bitmap

    HICON hIcon = CreateIconIndirect(&iconInfo);

    // Clean up
    SelectObject(hdcMem, oldBitmap);
    DeleteDC(hdcMem);
    DeleteObject(hBitmap);
    DeleteObject(hMask);
    ReleaseDC(NULL, hdc);

    return hIcon;
}

SDL_Tray *SDL_CreateTray(SDL_Surface *icon, const char *tooltip)
{
    SDL_Tray *tray = SDL_malloc(sizeof(SDL_Tray));

    if (!tray) {
        return NULL;
    }

    tray->hwnd = NULL;
    tray->menu.tray = tray;
    tray->menu.hMenu = NULL;
    tray->nEntries = 0;
    tray->entries = NULL;

    char classname[32];
    SDL_snprintf(classname, sizeof(classname), "SDLTray%d", (unsigned int) get_next_id());

    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc;
    ZeroMemory(&wc, sizeof(WNDCLASS));
    wc.lpfnWndProc = TrayWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = classname;

    RegisterClass(&wc);

    tray->hwnd = CreateWindowEx(0, classname, "", WS_OVERLAPPEDWINDOW,
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
        SDL_Surface *iconfmt = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_RGBA32);
        if (!iconfmt) {
            goto no_icon;
        }

        tray->nid.hIcon = CreateIconFromRGBA(iconfmt->w, iconfmt->h, iconfmt->pixels);
        tray->icon = tray->nid.hIcon;
    } else {
no_icon:
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
        SDL_Surface *iconfmt = SDL_ConvertSurface(icon, SDL_PIXELFORMAT_RGBA32);
        if (!iconfmt) {
            /* TODO: Ignore errors silently, as in SDL_CreateTray? */
            return;
        }

        tray->nid.hIcon = CreateIconFromRGBA(iconfmt->w, iconfmt->h, iconfmt->pixels);
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
    tray->menu.hMenu = CreatePopupMenu();
    return &tray->menu;
}

SDL_TrayMenu *SDL_CreateTraySubmenu(SDL_TrayEntry *entry)
{
    return &entry->submenu;
}

SDL_TrayEntry *SDL_AppendTrayEntry(SDL_TrayMenu *menu, const char *label, SDL_TrayEntryFlags flags)
{
    SDL_TrayEntry *entry = SDL_malloc(sizeof(SDL_TrayEntry));

    if (!entry) {
        return NULL;
    }

    entry->tray = menu->tray;
    entry->menu = menu;
    entry->id = get_next_id();
    entry->callback = NULL;
    entry->userdata = NULL;
    entry->submenu.tray = entry->tray;
    entry->submenu.hMenu = NULL;

    SDL_TrayEntry **new_entries = (SDL_TrayEntry **) SDL_realloc(entry->tray->entries, (entry->tray->nEntries + 1) * sizeof(SDL_TrayEntry **));

    if (!new_entries) {
        SDL_free(entry);
        return NULL;
    }

    new_entries[entry->tray->nEntries] = entry;
    entry->tray->entries = new_entries;
    entry->tray->nEntries++;

    UINT mf = MF_STRING;
    if (flags & SDL_TRAYENTRY_SUBMENU) {
        mf = MF_POPUP;

        entry->submenu.tray = entry->tray;
        entry->submenu.hMenu = CreatePopupMenu();
        entry->id = (UINT_PTR) entry->submenu.hMenu;
    }

    if (flags & SDL_TRAYENTRY_DISABLED) {
        mf |= MF_DISABLED | MF_GRAYED;
    }

    if (flags & SDL_TRAYENTRY_CHECKED) {
        mf |= MF_CHECKED;
    }

    AppendMenuA(entry->menu->hMenu, mf, entry->id, label);

    return entry;
}

void SDL_AppendTraySeparator(SDL_TrayMenu *menu)
{
    AppendMenu(menu->hMenu, MF_SEPARATOR, 0, NULL);
}

void SDL_SetTrayEntryChecked(SDL_TrayEntry *entry, bool checked)
{
    CheckMenuItem(entry->menu->hMenu, (UINT) entry->id, checked ? MF_CHECKED : MF_UNCHECKED);
}

bool SDL_GetTrayEntryChecked(SDL_TrayEntry *entry)
{
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;

    GetMenuItemInfo(entry->menu->hMenu, (UINT) entry->id, FALSE, &mii);

    return !!(mii.fState & MFS_CHECKED);
}

void SDL_SetTrayEntryEnabled(SDL_TrayEntry *entry, bool enabled)
{
    EnableMenuItem(entry->menu->hMenu, (UINT) entry->id, MF_BYCOMMAND | (enabled ? MF_ENABLED : (MF_DISABLED | MF_GRAYED)));
}

bool SDL_GetTrayEntryEnabled(SDL_TrayEntry *entry)
{
    MENUITEMINFO mii;
    mii.cbSize = sizeof(MENUITEMINFO);
    mii.fMask = MIIM_STATE;

    GetMenuItemInfo(entry->menu->hMenu, (UINT) entry->id, FALSE, &mii);

    return !!(mii.fState & MFS_ENABLED);
}

void SDL_SetTrayEntryCallback(SDL_TrayEntry *entry, SDL_TrayCallback callback, void *userdata)
{
    entry->callback = callback;
    entry->userdata = userdata;
}

void SDL_DestroyTray(SDL_Tray *tray)
{
    Shell_NotifyIconW(NIM_DELETE, &tray->nid);

    for (size_t i = 0; i < tray->nEntries; i++) {
        if (tray->entries[i]->submenu.hMenu) {
            DestroyMenu(tray->entries[i]->submenu.hMenu);
        }
        SDL_free(tray->entries[i]);
    }

    SDL_free(tray->entries);

    if (tray->menu.hMenu) {
        DestroyMenu(tray->menu.hMenu);
    }

    if (tray->icon) {
        DestroyIcon(tray->icon);
    }

    SDL_free(tray);
}
