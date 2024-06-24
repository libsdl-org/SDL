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

#ifdef SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"

#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_scancode_tables_c.h"

#include <X11/keysym.h>
#include <X11/XKBlib.h>

#include "../../events/imKStoUCS.h"
#include "../../events/SDL_keysym_to_scancode_c.h"

#ifdef X_HAVE_UTF8_STRING
#include <locale.h>
#endif

static SDL_ScancodeTable scancode_set[] = {
    SDL_SCANCODE_TABLE_DARWIN,
    SDL_SCANCODE_TABLE_XFREE86_1,
    SDL_SCANCODE_TABLE_XFREE86_2,
    SDL_SCANCODE_TABLE_XVNC,
};

static SDL_bool X11_ScancodeIsRemappable(SDL_Scancode scancode)
{
    /*
     * XKB remappings can assign different keysyms for these scancodes, but
     * as these keys are in fixed positions, the scancodes themselves shouldn't
     * be switched. Mark them as not being remappable.
     */
    switch (scancode) {
    case SDL_SCANCODE_ESCAPE:
    case SDL_SCANCODE_CAPSLOCK:
    case SDL_SCANCODE_NUMLOCKCLEAR:
    case SDL_SCANCODE_LSHIFT:
    case SDL_SCANCODE_RSHIFT:
    case SDL_SCANCODE_LCTRL:
    case SDL_SCANCODE_RCTRL:
    case SDL_SCANCODE_LALT:
    case SDL_SCANCODE_RALT:
    case SDL_SCANCODE_LGUI:
    case SDL_SCANCODE_RGUI:
        return SDL_FALSE;
    default:
        return SDL_TRUE;
    }
}

/* This function only correctly maps letters and numbers for keyboards in US QWERTY layout */
static SDL_Scancode X11_KeyCodeToSDLScancode(SDL_VideoDevice *_this, KeyCode keycode)
{
    const KeySym keysym = X11_KeyCodeToSym(_this, keycode, 0, 0);

    if (keysym == NoSymbol) {
        return SDL_SCANCODE_UNKNOWN;
    }

    return SDL_GetScancodeFromKeySym(keysym, keycode);
}

KeySym X11_KeyCodeToSym(SDL_VideoDevice *_this, KeyCode keycode, unsigned char group, unsigned int mod_mask)
{
    SDL_VideoData *data = _this->driverdata;
    KeySym keysym;
    unsigned int mods_ret[16];

    SDL_zero(mods_ret);

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLOOKUPKEYSYM
    if (data->xkb) {
        int num_groups = XkbKeyNumGroups(data->xkb, keycode);
        unsigned char info = XkbKeyGroupInfo(data->xkb, keycode);

        if (num_groups && group >= num_groups) {

            int action = XkbOutOfRangeGroupAction(info);

            if (action == XkbRedirectIntoRange) {
                group = XkbOutOfRangeGroupNumber(info);
                if (group >= num_groups) {
                    group = 0;
                }
            } else if (action == XkbClampIntoRange) {
                group = num_groups - 1;
            } else {
                group %= num_groups;
            }
        }

        if (X11_XkbLookupKeySym(data->display, keycode, XkbBuildCoreState(mod_mask, group), mods_ret, &keysym) == NoSymbol) {
            keysym = NoSymbol;
        }
    } else
#endif
    {
        /* TODO: Handle groups and modifiers on the legacy path. */
        keysym = X11_XKeycodeToKeysym(data->display, keycode, 0);
    }

    return keysym;
}

int X11_InitKeyboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->driverdata;
    int i = 0;
    int j = 0;
    int min_keycode, max_keycode;
    struct
    {
        SDL_Scancode scancode;
        KeySym keysym;
        int value;
    } fingerprint[] = {
        { SDL_SCANCODE_HOME, XK_Home, 0 },
        { SDL_SCANCODE_PAGEUP, XK_Prior, 0 },
        { SDL_SCANCODE_UP, XK_Up, 0 },
        { SDL_SCANCODE_LEFT, XK_Left, 0 },
        { SDL_SCANCODE_DELETE, XK_Delete, 0 },
        { SDL_SCANCODE_KP_ENTER, XK_KP_Enter, 0 },
    };
    int best_distance;
    int best_index;
    int distance;
    Bool xkb_repeat = 0;

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLOOKUPKEYSYM
    {
        int xkb_major = XkbMajorVersion;
        int xkb_minor = XkbMinorVersion;

        if (X11_XkbQueryExtension(data->display, NULL, &data->xkb_event, NULL, &xkb_major, &xkb_minor)) {
            data->xkb = X11_XkbGetMap(data->display, XkbAllClientInfoMask, XkbUseCoreKbd);
        }

        /* This will remove KeyRelease events for held keys */
        X11_XkbSetDetectableAutoRepeat(data->display, True, &xkb_repeat);
    }
#endif

    /* Open a connection to the X input manager */
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        /* Set the locale, and call XSetLocaleModifiers before XOpenIM so that
           Compose keys will work correctly. */
        char *prev_locale = setlocale(LC_ALL, NULL);
        char *prev_xmods = X11_XSetLocaleModifiers(NULL);
        const char *new_xmods = "";
        const char *env_xmods = SDL_getenv("XMODIFIERS");
        SDL_bool has_dbus_ime_support = SDL_FALSE;

        if (prev_locale) {
            prev_locale = SDL_strdup(prev_locale);
        }

        if (prev_xmods) {
            prev_xmods = SDL_strdup(prev_xmods);
        }

        /* IBus resends some key events that were filtered by XFilterEvents
           when it is used via XIM which causes issues. Prevent this by forcing
           @im=none if XMODIFIERS contains @im=ibus. IBus can still be used via
           the DBus implementation, which also has support for pre-editing. */
        if (env_xmods && SDL_strstr(env_xmods, "@im=ibus") != NULL) {
            has_dbus_ime_support = SDL_TRUE;
        }
        if (env_xmods && SDL_strstr(env_xmods, "@im=fcitx") != NULL) {
            has_dbus_ime_support = SDL_TRUE;
        }
        if (has_dbus_ime_support || !xkb_repeat) {
            new_xmods = "@im=none";
        }

        (void)setlocale(LC_ALL, "");
        X11_XSetLocaleModifiers(new_xmods);

        data->im = X11_XOpenIM(data->display, NULL, NULL, NULL);

        /* Reset the locale + X locale modifiers back to how they were,
           locale first because the X locale modifiers depend on it. */
        (void)setlocale(LC_ALL, prev_locale);
        X11_XSetLocaleModifiers(prev_xmods);

        if (prev_locale) {
            SDL_free(prev_locale);
        }

        if (prev_xmods) {
            SDL_free(prev_xmods);
        }
    }
#endif
    /* Try to determine which scancodes are being used based on fingerprint */
    best_distance = SDL_arraysize(fingerprint) + 1;
    best_index = -1;
    X11_XDisplayKeycodes(data->display, &min_keycode, &max_keycode);
    for (i = 0; i < SDL_arraysize(fingerprint); ++i) {
        fingerprint[i].value = X11_XKeysymToKeycode(data->display, fingerprint[i].keysym) - min_keycode;
    }
    for (i = 0; i < SDL_arraysize(scancode_set); ++i) {
        int table_size;
        const SDL_Scancode *table = SDL_GetScancodeTable(scancode_set[i], &table_size);

        distance = 0;
        for (j = 0; j < SDL_arraysize(fingerprint); ++j) {
            if (fingerprint[j].value < 0 || fingerprint[j].value >= table_size) {
                distance += 1;
            } else if (table[fingerprint[j].value] != fingerprint[j].scancode) {
                distance += 1;
            }
        }
        if (distance < best_distance) {
            best_distance = distance;
            best_index = i;
        }
    }
    if (best_index < 0 || best_distance > 2) {
        /* This is likely to be SDL_SCANCODE_TABLE_XFREE86_2 with remapped keys, double check a rarely remapped value */
        int fingerprint_value = X11_XKeysymToKeycode(data->display, 0x1008FF5B /* XF86Documents */) - min_keycode;
        if (fingerprint_value == 235) {
            for (i = 0; i < SDL_arraysize(scancode_set); ++i) {
                if (scancode_set[i] == SDL_SCANCODE_TABLE_XFREE86_2) {
                    best_index = i;
                    best_distance = 0;
                    break;
                }
            }
        }
    }
    if (best_index >= 0 && best_distance <= 2) {
        int table_size;
        const SDL_Scancode *table = SDL_GetScancodeTable(scancode_set[best_index], &table_size);

#ifdef DEBUG_KEYBOARD
        SDL_Log("Using scancode set %d, min_keycode = %d, max_keycode = %d, table_size = %d\n", best_index, min_keycode, max_keycode, table_size);
#endif
        /* This should never happen, but just in case... */
        if (table_size > (SDL_arraysize(data->key_layout) - min_keycode)) {
            table_size = (SDL_arraysize(data->key_layout) - min_keycode);
        }
        SDL_memcpy(&data->key_layout[min_keycode], table, sizeof(SDL_Scancode) * table_size);

        /* Scancodes represent physical locations on the keyboard, unaffected by keyboard mapping.
           However, there are a number of extended scancodes that have no standard location, so use
           the X11 mapping for all non-character keys.
         */
        for (i = min_keycode; i <= max_keycode; ++i) {
            SDL_Scancode scancode = X11_KeyCodeToSDLScancode(_this, i);
#ifdef DEBUG_KEYBOARD
            {
                KeySym sym;
                sym = X11_KeyCodeToSym(_this, (KeyCode)i, 0);
                SDL_Log("code = %d, sym = 0x%X (%s) ", i - min_keycode,
                        (unsigned int)sym, sym == NoSymbol ? "NoSymbol" : X11_XKeysymToString(sym));
            }
#endif
            if (scancode == data->key_layout[i]) {
                continue;
            }
            if ((SDL_GetDefaultKeyFromScancode(scancode, SDL_KMOD_NONE) & SDLK_SCANCODE_MASK) && X11_ScancodeIsRemappable(scancode)) {
                /* Not a character key and the scancode is safe to remap */
#ifdef DEBUG_KEYBOARD
                SDL_Log("Changing scancode, was %d (%s), now %d (%s)\n", data->key_layout[i], SDL_GetScancodeName(data->key_layout[i]), scancode, SDL_GetScancodeName(scancode));
#endif
                data->key_layout[i] = scancode;
            }
        }
    } else {
#ifdef DEBUG_SCANCODES
        SDL_Log("Keyboard layout unknown, please report the following to the SDL forums/mailing list (https://discourse.libsdl.org/):\n");
#endif

        /* Determine key_layout - only works on US QWERTY layout */
        for (i = min_keycode; i <= max_keycode; ++i) {
            SDL_Scancode scancode = X11_KeyCodeToSDLScancode(_this, i);
#ifdef DEBUG_SCANCODES
            {
                KeySym sym;
                sym = X11_KeyCodeToSym(_this, (KeyCode)i, 0);
                SDL_Log("code = %d, sym = 0x%X (%s) ", i - min_keycode,
                        (unsigned int)sym, sym == NoSymbol ? "NoSymbol" : X11_XKeysymToString(sym));
            }
            if (scancode == SDL_SCANCODE_UNKNOWN) {
                SDL_Log("scancode not found\n");
            } else {
                SDL_Log("scancode = %d (%s)\n", scancode, SDL_GetScancodeName(scancode));
            }
#endif
            data->key_layout[i] = scancode;
        }
    }

    X11_UpdateKeymap(_this, SDL_FALSE);

    SDL_SetScancodeName(SDL_SCANCODE_APPLICATION, "Menu");

#ifdef SDL_USE_IME
    SDL_IME_Init();
#endif

    X11_ReconcileKeyboardState(_this);

    return 0;
}

void X11_UpdateKeymap(SDL_VideoDevice *_this, SDL_bool send_event)
{
    struct Keymod_masks
    {
        SDL_Keymod sdl_mask;
        unsigned int xkb_mask;
    } const keymod_masks[] = {
        { SDL_KMOD_NONE, 0 },
        { SDL_KMOD_SHIFT, ShiftMask },
        { SDL_KMOD_CAPS, LockMask },
        { SDL_KMOD_SHIFT | SDL_KMOD_CAPS, ShiftMask | LockMask },
        { SDL_KMOD_MODE, Mod5Mask },
        { SDL_KMOD_MODE | SDL_KMOD_SHIFT, Mod5Mask | ShiftMask },
        { SDL_KMOD_MODE | SDL_KMOD_CAPS, Mod5Mask | LockMask },
        { SDL_KMOD_MODE | SDL_KMOD_SHIFT | SDL_KMOD_CAPS, Mod5Mask | ShiftMask | LockMask }
    };

    SDL_VideoData *data = _this->driverdata;
    int i;
    SDL_Scancode scancode;
    SDL_Keymap *keymap;

    keymap = SDL_CreateKeymap();

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLOOKUPKEYSYM
    if (data->xkb) {
        XkbStateRec state;
        X11_XkbGetUpdatedMap(data->display, XkbAllClientInfoMask, data->xkb);

        if (X11_XkbGetState(data->display, XkbUseCoreKbd, &state) == Success) {
            data->xkb_group = state.group;
        }
    }
#endif

    for (int m = 0; m < SDL_arraysize(keymod_masks); ++m) {
        for (i = 0; i < SDL_arraysize(data->key_layout); i++) {
            SDL_Keycode keycode;

            /* Make sure this is a valid scancode */
            scancode = data->key_layout[i];
            if (scancode == SDL_SCANCODE_UNKNOWN) {
                continue;
            }

            KeySym keysym = X11_KeyCodeToSym(_this, i, data->xkb_group, keymod_masks[m].xkb_mask);

            /* Note: The default SDL scancode table sets this to right alt instead of AltGr/Mode, so handle it separately. */
            if (keysym != XK_ISO_Level3_Shift) {
                keycode = SDL_KeySymToUcs4(keysym);
            } else {
                keycode = SDLK_MODE;
            }

            if (!keycode) {
                SDL_Scancode keyScancode = SDL_GetScancodeFromKeySym(keysym, (KeyCode)i);

                switch (keyScancode) {
                case SDL_SCANCODE_UNKNOWN:
                    keycode = SDLK_UNKNOWN;
                    break;
                case SDL_SCANCODE_RETURN:
                    keycode = SDLK_RETURN;
                    break;
                case SDL_SCANCODE_ESCAPE:
                    keycode = SDLK_ESCAPE;
                    break;
                case SDL_SCANCODE_BACKSPACE:
                    keycode = SDLK_BACKSPACE;
                    break;
                case SDL_SCANCODE_TAB:
                    keycode = SDLK_TAB;
                    break;
                case SDL_SCANCODE_DELETE:
                    keycode = SDLK_DELETE;
                    break;
                default:
                    keycode = SDL_SCANCODE_TO_KEYCODE(keyScancode);
                    break;
                }
            }
            SDL_SetKeymapEntry(keymap, scancode, keymod_masks[m].sdl_mask, keycode);
        }
    }
    SDL_SetKeymap(keymap, send_event);
}

void X11_QuitKeyboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->driverdata;

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLOOKUPKEYSYM
    if (data->xkb) {
        X11_XkbFreeKeyboard(data->xkb, 0, True);
        data->xkb = NULL;
    }
#endif

#ifdef SDL_USE_IME
    SDL_IME_Quit();
#endif
}

static void X11_ResetXIM(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef X_HAVE_UTF8_STRING
    SDL_WindowData *data = window->driverdata;

    if (data && data->ic) {
        /* Clear any partially entered dead keys */
        char *contents = X11_Xutf8ResetIC(data->ic);
        if (contents) {
            X11_XFree(contents);
        }
    }
#endif
}

int X11_StartTextInput(SDL_VideoDevice *_this, SDL_Window *window)
{
    X11_ResetXIM(_this, window);

    return X11_UpdateTextInputRect(_this, window);
}

int X11_StopTextInput(SDL_VideoDevice *_this, SDL_Window *window)
{
    X11_ResetXIM(_this, window);
#ifdef SDL_USE_IME
    SDL_IME_Reset();
#endif
    return 0;
}

int X11_UpdateTextInputRect(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef SDL_USE_IME
    SDL_IME_UpdateTextRect(window);
#endif
    return 0;
}

SDL_bool X11_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    SDL_VideoData *videodata = _this->driverdata;
    return videodata->is_steam_deck;
}

void X11_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *videodata = _this->driverdata;

    if (videodata->is_steam_deck) {
        /* For more documentation of the URL parameters, see:
         * https://partner.steamgames.com/doc/api/ISteamUtils#ShowFloatingGamepadTextInput
         */
        char deeplink[128];
        (void)SDL_snprintf(deeplink, sizeof(deeplink),
                           "steam://open/keyboard?XPosition=0&YPosition=0&Width=0&Height=0&Mode=%d",
                           SDL_GetHintBoolean(SDL_HINT_RETURN_KEY_HIDES_IME, SDL_FALSE) ? 0 : 1);
        SDL_OpenURL(deeplink);
        videodata->steam_keyboard_open = SDL_TRUE;
    }
}

void X11_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *videodata = _this->driverdata;

    if (videodata->is_steam_deck) {
        SDL_OpenURL("steam://close/keyboard");
        videodata->steam_keyboard_open = SDL_FALSE;
    }
}

SDL_bool X11_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *videodata = _this->driverdata;

    return videodata->steam_keyboard_open;
}

#endif /* SDL_VIDEO_DRIVER_X11 */
