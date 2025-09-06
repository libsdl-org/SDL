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

#ifdef SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"

#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_scancode_tables_c.h"

#include <X11/keysym.h>

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLIB
#include <X11/XKBlib.h>
#endif

#include "../../events/imKStoUCS.h"
#include "../../events/SDL_keysym_to_scancode_c.h"
#include "../../events/SDL_keysym_to_keycode_c.h"

#ifdef X_HAVE_UTF8_STRING
#include <locale.h>
#endif

static SDL_ScancodeTable scancode_set[] = {
    SDL_SCANCODE_TABLE_DARWIN,
    SDL_SCANCODE_TABLE_XFREE86_1,
    SDL_SCANCODE_TABLE_XFREE86_2,
    SDL_SCANCODE_TABLE_XVNC,
};

static bool X11_ScancodeIsRemappable(SDL_Scancode scancode)
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
        return false;
    default:
        return true;
    }
}

static KeySym X11_KeyCodeToSym(SDL_VideoDevice *_this, KeyCode keycode, unsigned int group, unsigned int level)
{
    SDL_VideoData *data = _this->internal;
    KeySym keysym;

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLIB
    if (data->keyboard.xkb_enabled) {
        keysym = X11_XkbKeycodeToKeysym(data->display, keycode, group, level);
    } else
#endif
    {
        // TODO: Handle groups on the legacy path.
        if (keycode >= data->keyboard.core.min_keycode && keycode <= data->keyboard.core.max_keycode) {
            keysym = data->keyboard.core.keysym_map[(keycode - data->keyboard.core.min_keycode) * data->keyboard.core.keysyms_per_key];
        } else {
            keysym = NoSymbol;
        }
    }

    return keysym;
}

// This function only correctly maps letters and numbers for keyboards in US QWERTY layout
static SDL_Scancode X11_KeyCodeToSDLScancode(SDL_VideoDevice *_this, KeyCode keycode)
{
    const KeySym keysym = X11_KeyCodeToSym(_this, keycode, 0, 0);

    if (keysym == NoSymbol) {
        return SDL_SCANCODE_UNKNOWN;
    }

    return SDL_GetScancodeFromKeySym(keysym, keycode);
}

bool X11_InitKeyboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
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

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLIB
    int xkb_major = XkbMajorVersion;
    int xkb_minor = XkbMinorVersion;

    if (X11_XkbQueryExtension(data->display, NULL, &data->keyboard.xkb.event, NULL, &xkb_major, &xkb_minor)) {
        Bool xkb_repeat = 0;
        data->keyboard.xkb_enabled = true;
        data->keyboard.xkb.desc_ptr = X11_XkbGetMap(data->display, XkbAllClientInfoMask, XkbUseCoreKbd);

        // This will remove KeyRelease events for held keys.
        X11_XkbSetDetectableAutoRepeat(data->display, True, &xkb_repeat);

        // Enable the key mapping and state events.
        X11_XkbSelectEvents(data->display, XkbUseCoreKbd,
                            XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask,
                            XkbNewKeyboardNotifyMask | XkbMapNotifyMask | XkbStateNotifyMask);
        X11_XkbSelectEventDetails(data->display, XkbUseCoreKbd, XkbStateNotify, XkbGroupStateMask | XkbModifierStateMask, XkbGroupStateMask | XkbModifierStateMask);
    } else
#endif
    {
        // If XKB isn't available, initialize the legacy path.
        X11_XDisplayKeycodes(data->display, &data->keyboard.core.min_keycode, &data->keyboard.core.max_keycode);
        data->keyboard.core.keysym_map = X11_XGetKeyboardMapping(data->display, data->keyboard.core.min_keycode,
                                                              data->keyboard.core.max_keycode - data->keyboard.core.min_keycode,
                                                              &data->keyboard.core.keysyms_per_key);
    }

    // Open a connection to the X input manager
#ifdef X_HAVE_UTF8_STRING
    if (SDL_X11_HAVE_UTF8) {
        /* Set the locale, and call XSetLocaleModifiers before XOpenIM so that
           Compose keys will work correctly. */
        char *prev_locale = setlocale(LC_ALL, NULL);
        char *prev_xmods = X11_XSetLocaleModifiers(NULL);

        if (prev_locale) {
            prev_locale = SDL_strdup(prev_locale);
        }

        if (prev_xmods) {
            prev_xmods = SDL_strdup(prev_xmods);
        }

        (void)setlocale(LC_ALL, "");
        X11_XSetLocaleModifiers("");

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
    // Try to determine which scancodes are being used based on fingerprint
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
        // This is likely to be SDL_SCANCODE_TABLE_XFREE86_2 with remapped keys, double check a rarely remapped value
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
        SDL_Log("Using scancode set %d, min_keycode = %d, max_keycode = %d, table_size = %d", best_index, min_keycode, max_keycode, table_size);
#endif
        // This should never happen, but just in case...
        if (table_size > (SDL_arraysize(data->keyboard.key_layout) - min_keycode)) {
            table_size = (SDL_arraysize(data->keyboard.key_layout) - min_keycode);
        }
        SDL_memcpy(&data->keyboard.key_layout[min_keycode], table, sizeof(SDL_Scancode) * table_size);

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
            if (scancode == data->keyboard.key_layout[i]) {
                continue;
            }
            if ((SDL_GetKeymapKeycode(NULL, scancode, SDL_KMOD_NONE) & (SDLK_SCANCODE_MASK | SDLK_EXTENDED_MASK)) && X11_ScancodeIsRemappable(scancode)) {
                // Not a character key and the scancode is safe to remap
#ifdef DEBUG_KEYBOARD
                SDL_Log("Changing scancode, was %d (%s), now %d (%s)", data->key_layout[i], SDL_GetScancodeName(data->key_layout[i]), scancode, SDL_GetScancodeName(scancode));
#endif
                data->keyboard.key_layout[i] = scancode;
            }
        }
    } else {
#ifdef DEBUG_SCANCODES
        SDL_Log("Keyboard layout unknown, please report the following to the SDL forums/mailing list (https://discourse.libsdl.org/):");
#endif

        // Determine key_layout - only works on US QWERTY layout
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
                SDL_Log("scancode not found");
            } else {
                SDL_Log("scancode = %d (%s)", scancode, SDL_GetScancodeName(scancode));
            }
#endif
            data->keyboard.key_layout[i] = scancode;
        }
    }

    X11_UpdateKeymap(_this, false);

    SDL_SetScancodeName(SDL_SCANCODE_APPLICATION, "Menu");

    X11_ReconcileKeyboardState(_this);

    return true;
}

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLIB
static unsigned int X11_GetXkbVirtualModifierMask(SDL_VideoDevice *_this, const char *vmod_name)
{
    SDL_VideoData *videodata = _this->internal;
    unsigned int mod_mask = 0;

    const Atom vmod = X11_XInternAtom(videodata->display, vmod_name, True);
    if (vmod != None) {
        for (int i = 0; i < XkbNumVirtualMods; ++i) {
            if (vmod == videodata->keyboard.xkb.desc_ptr->names->vmods[i]) {
                mod_mask = videodata->keyboard.xkb.desc_ptr->server->vmods[i];
                break;
            }
        }
    }

    return mod_mask;
}
#endif

static unsigned X11_GetXModifierMask(SDL_VideoDevice *_this, SDL_Scancode scancode)
{
    SDL_VideoData *videodata = _this->internal;
    Display *display = videodata->display;
    unsigned int mod_mask = 0;

    XModifierKeymap *xmods = X11_XGetModifierMapping(display);
    unsigned int n = xmods->max_keypermod;
    for (int i = 3; i < 8; i++) {
        for (int j = 0; j < n; j++) {
            const KeyCode kc = xmods->modifiermap[i * n + j];
            if (videodata->keyboard.key_layout[kc] == scancode) {
                mod_mask = 1 << i;
                break;
            }
        }
    }
    X11_XFreeModifiermap(xmods);

    return mod_mask;
}

static void X11_AddKeymapEntry(SDL_Keymap *keymap, Uint32 xkeycode, KeySym xkeysym, SDL_Scancode sdl_scancode, SDL_Keymod sdl_mod_mask)
{
    SDL_Keycode keycode = SDL_GetKeyCodeFromKeySym(xkeysym, xkeycode, sdl_mod_mask);

    if (!keycode) {
        switch (sdl_scancode) {
        case SDL_SCANCODE_RETURN:
            keycode = SDLK_RETURN;
            break;
        case SDL_SCANCODE_ESCAPE:
            keycode = SDLK_ESCAPE;
            break;
        case SDL_SCANCODE_BACKSPACE:
            keycode = SDLK_BACKSPACE;
            break;
        case SDL_SCANCODE_DELETE:
            keycode = SDLK_DELETE;
            break;
        default:
            keycode = SDL_SCANCODE_TO_KEYCODE(sdl_scancode);
            break;
        }
    }

    SDL_SetKeymapEntry(keymap, sdl_scancode, sdl_mod_mask, keycode);
}

void X11_UpdateKeymap(SDL_VideoDevice *_this, bool send_event)
{
    SDL_VideoData *data = _this->internal;

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLIB
    if (data->keyboard.xkb_enabled) {
        XkbStateRec state;

        for (unsigned int i = 0; i < XkbNumKbdGroups; ++i) {
            SDL_DestroyKeymap(data->keyboard.xkb.keymaps[i]);
            data->keyboard.xkb.keymaps[i] = NULL;
        }

        X11_XkbGetNames(data->display, XkbVirtualModNamesMask, data->keyboard.xkb.desc_ptr);
        X11_XkbGetUpdatedMap(data->display, XkbAllClientInfoMask | XkbVirtualModsMask, data->keyboard.xkb.desc_ptr);

        if (X11_XkbGetState(data->display, XkbUseCoreKbd, &state) == Success) {
            data->keyboard.xkb.current_group = state.group;
        }

        data->keyboard.alt_mask = X11_GetXkbVirtualModifierMask(_this, "Alt");
        if (!data->keyboard.alt_mask) {
            data->keyboard.alt_mask = X11_GetXkbVirtualModifierMask(_this, "Meta");
        }
        data->keyboard.gui_mask = X11_GetXkbVirtualModifierMask(_this, "Super");
        data->keyboard.level3_mask = X11_GetXkbVirtualModifierMask(_this, "LevelThree");
        data->keyboard.level5_mask = X11_GetXkbVirtualModifierMask(_this, "LevelFive");
        data->keyboard.numlock_mask = X11_GetXkbVirtualModifierMask(_this, "NumLock");
        data->keyboard.scrolllock_mask = X11_GetXkbVirtualModifierMask(_this, "ScrollLock");

        for (unsigned int i = 0; i < XkbNumKbdGroups; ++i) {
            data->keyboard.xkb.keymaps[i] = SDL_CreateKeymap(false);
            if (!data->keyboard.xkb.keymaps[i]) {
                for (unsigned int j = 0; j < i; ++j) {
                    SDL_DestroyKeymap(data->keyboard.xkb.keymaps[i]);
                    data->keyboard.xkb.keymaps[j] = NULL;
                }

                return;
            }
        }

        const Uint32 valid_mod_mask = ShiftMask | LockMask | data->keyboard.alt_mask | data->keyboard.level3_mask | data->keyboard.level5_mask;

        for (Uint32 xkeycode = data->keyboard.xkb.desc_ptr->min_key_code; xkeycode < data->keyboard.xkb.desc_ptr->max_key_code; ++xkeycode) {
            const SDL_Scancode scancode = data->keyboard.key_layout[xkeycode];
            if (scancode == SDL_SCANCODE_UNKNOWN) {
                continue;
            }

            for (Uint32 group = 0; group < XkbNumKbdGroups; ++group) {
                SDL_Keymap *keymap = data->keyboard.xkb.keymaps[group];

                Uint32 effective_group = group;
                const unsigned char max_key_group = XkbKeyNumGroups(data->keyboard.xkb.desc_ptr, xkeycode);
                const unsigned char key_group_info = XkbKeyGroupInfo(data->keyboard.xkb.desc_ptr, xkeycode);

                if (max_key_group && effective_group >= max_key_group) {
                    const unsigned char action = XkbOutOfRangeGroupAction(key_group_info);

                    switch (action) {
                    default:
                        effective_group %= max_key_group;
                        break;
                    case XkbClampIntoRange:
                        effective_group = max_key_group - 1;
                        break;
                    case XkbRedirectIntoRange:
                        effective_group = XkbOutOfRangeGroupNumber(key_group_info);
                        if (effective_group >= max_key_group) {
                            effective_group = 0;
                        }
                        break;
                    }
                }

                XkbKeyTypePtr key_type = XkbKeyKeyType(data->keyboard.xkb.desc_ptr, xkeycode, effective_group);

                for (Uint32 level = 0; level < key_type->num_levels; ++level) {
                    const KeySym keysym = X11_KeyCodeToSym(_this, xkeycode, effective_group, level);

                    if (keysym != NoSymbol) {
                        bool key_added = false;

                        for (int map_idx = 0; map_idx < key_type->map_count; ++map_idx) {
                            if (key_type->map[map_idx].active && key_type->map[map_idx].level == level) {
                                const unsigned int xkb_mod_mask = key_type->map[map_idx].mods.mask;
                                if ((xkb_mod_mask | valid_mod_mask) == valid_mod_mask) {
                                    const SDL_Keymod sdl_mod_mask = (xkb_mod_mask & ShiftMask ? SDL_KMOD_SHIFT : 0) |
                                                                    (xkb_mod_mask & LockMask ? SDL_KMOD_CAPS : 0) |
                                                                    (xkb_mod_mask & data->keyboard.alt_mask ? SDL_KMOD_ALT : 0) |
                                                                    (xkb_mod_mask & data->keyboard.level3_mask ? SDL_KMOD_MODE : 0) |
                                                                    (xkb_mod_mask & data->keyboard.level5_mask ? SDL_KMOD_LEVEL5 : 0);

                                    X11_AddKeymapEntry(keymap, xkeycode, keysym, scancode, sdl_mod_mask);
                                    key_added = true;
                                }
                            }
                        }

                        // Add the unmodified key for level 0.
                        if (!level && !key_added) {
                            X11_AddKeymapEntry(keymap, xkeycode, keysym, scancode, 0);
                        }
                    }
                }
            }
        }

        SDL_SetKeymap(data->keyboard.xkb.keymaps[data->keyboard.xkb.current_group], send_event);
    } else
#endif
    {
        SDL_Keymap *keymap = SDL_CreateKeymap(true);
        if (!keymap) {
            return;
        }

        if (send_event) {
            if (data->keyboard.core.keysym_map) {
                X11_XFree(data->keyboard.core.keysym_map);
            }
            X11_XDisplayKeycodes(data->display, &data->keyboard.core.min_keycode, &data->keyboard.core.max_keycode);
            data->keyboard.core.keysym_map = X11_XGetKeyboardMapping(data->display, data->keyboard.core.min_keycode,
                                                                     data->keyboard.core.max_keycode - data->keyboard.core.min_keycode,
                                                                     &data->keyboard.core.keysyms_per_key);
        }

        for (Uint32 xkeycode = data->keyboard.core.min_keycode; xkeycode <= data->keyboard.core.max_keycode; ++xkeycode) {
            const SDL_Scancode scancode = data->keyboard.key_layout[xkeycode];
            if (scancode == SDL_SCANCODE_UNKNOWN) {
                continue;
            }

            const KeySym keysym = X11_KeyCodeToSym(_this, xkeycode, 0, 0);
            if (keysym != NoSymbol) {
                X11_AddKeymapEntry(keymap, xkeycode, keysym, scancode, 0);
            }
        }

        data->keyboard.alt_mask = Mod1Mask; // Alt or Meta
        data->keyboard.gui_mask = Mod4Mask; // Super
        data->keyboard.level3_mask = Mod5Mask; // Note: Not a typo, Mod5 = level 3 shift, and Mod3 = level 5 shift.
        data->keyboard.level5_mask = Mod3Mask;
        data->keyboard.numlock_mask = X11_GetXModifierMask(_this, SDL_SCANCODE_NUMLOCKCLEAR);
        data->keyboard.scrolllock_mask = X11_GetXModifierMask(_this, SDL_SCANCODE_SCROLLLOCK);

        SDL_SetKeymap(keymap, send_event);
    }
}

void X11_QuitKeyboard(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

#ifdef SDL_VIDEO_DRIVER_X11_HAS_XKBLIB
    if (data->keyboard.xkb_enabled) {
        for (int i = 0; i < XkbNumKbdGroups; ++i) {
            SDL_DestroyKeymap(data->keyboard.xkb.keymaps[i]);
            data->keyboard.xkb.keymaps[i] = NULL;
        }

        if (data->keyboard.xkb_enabled) {
            X11_XkbFreeKeyboard(data->keyboard.xkb.desc_ptr, 0, True);
            data->keyboard.xkb.desc_ptr = NULL;
        }
    } else
#endif
        if (data->keyboard.core.keysym_map) {
        X11_XFree(data->keyboard.core.keysym_map);
        data->keyboard.core.keysym_map = NULL;
    }
}

void X11_ClearComposition(SDL_WindowData *data)
{
    if (data->preedit_length > 0) {
        data->preedit_text[0] = '\0';
        data->preedit_length = 0;
    }

    if (data->ime_needs_clear_composition) {
        SDL_SendEditingText("", 0, 0);
        data->ime_needs_clear_composition = false;
    }
}

static void X11_SendEditingEvent(SDL_WindowData *data)
{
    if (data->preedit_length == 0) {
        X11_ClearComposition(data);
        return;
    }

    bool in_highlight = false;
    int start = -1, length = 0, i;
    for (i = 0; i < data->preedit_length; ++i) {
        if (data->preedit_feedback[i] & (XIMReverse | XIMHighlight)) {
            if (start < 0) {
                start = i;
                in_highlight = true;
            }
        } else if (in_highlight) {
            // Found the end of the highlight
            break;
        }
    }
    if (in_highlight) {
        length = (i - start);
    } else {
        start = SDL_clamp(data->preedit_cursor, 0, data->preedit_length);
    }
    SDL_SendEditingText(data->preedit_text, start, length);

    data->ime_needs_clear_composition = true;
}

static int preedit_start_callback(XIC xic, XPointer client_data, XPointer call_data)
{
    // No limit on preedit text length
    return -1;
}

static void preedit_done_callback(XIC xic, XPointer client_data, XPointer call_data)
{
}

static void preedit_draw_callback(XIC xic, XPointer client_data, XIMPreeditDrawCallbackStruct *call_data)
{
    SDL_WindowData *data = (SDL_WindowData *)client_data;
    int chg_first = SDL_clamp(call_data->chg_first, 0, data->preedit_length);
    int chg_length = SDL_clamp(call_data->chg_length, 0, data->preedit_length - chg_first);

    const char *start = data->preedit_text;
    if (chg_length > 0) {
        // Delete text in range
        for (int i = 0; start && *start && i < chg_first; ++i) {
            SDL_StepUTF8(&start, NULL);
        }

        const char *end = start;
        for (int i = 0; end && *end && i < chg_length; ++i) {
            SDL_StepUTF8(&end, NULL);
        }

        if (end > start) {
            SDL_memmove((char *)start, end, SDL_strlen(end) + 1);
            if ((chg_first + chg_length) > data->preedit_length) {
                SDL_memmove(&data->preedit_feedback[chg_first], &data->preedit_feedback[chg_first + chg_length], (data->preedit_length - chg_first - chg_length) * sizeof(*data->preedit_feedback));
            }
        }
        data->preedit_length -= chg_length;
    }

    XIMText *text = call_data->text;
    if (text) {
        // Insert text in range
        SDL_assert(!text->encoding_is_wchar);

        // The text length isn't calculated as directed by the spec, recalculate it now
        if (text->string.multi_byte) {
            text->length = SDL_utf8strlen(text->string.multi_byte);
        }

        size_t string_size = SDL_strlen(text->string.multi_byte);
        size_t size = string_size + 1;
        if (data->preedit_text) {
            size += SDL_strlen(data->preedit_text);
        }
        char *preedit_text = (char *)SDL_malloc(size * sizeof(*preedit_text));
        if (preedit_text) {
            size_t pre_size = (start - data->preedit_text);
            size_t post_size = start ? SDL_strlen(start) : 0;
            if (pre_size > 0) {
                SDL_memcpy(&preedit_text[0], data->preedit_text, pre_size);
            }
            SDL_memcpy(&preedit_text[pre_size], text->string.multi_byte, string_size);
            if (post_size > 0) {
                SDL_memcpy(&preedit_text[pre_size + string_size], start, post_size);
            }
            preedit_text[size - 1] = '\0';
        }

        size_t feedback_size = data->preedit_length + text->length;
        XIMFeedback *feedback = (XIMFeedback *)SDL_malloc(feedback_size * sizeof(*feedback));
        if (feedback) {
            size_t pre_size = (size_t)chg_first;
            size_t post_size = (size_t)data->preedit_length - pre_size;
            if (pre_size > 0) {
                SDL_memcpy(&feedback[0], data->preedit_feedback, pre_size * sizeof(*feedback));
            }
            SDL_memcpy(&feedback[pre_size], text->feedback, text->length * sizeof(*feedback));
            if (post_size > 0) {
                SDL_memcpy(&feedback[pre_size + text->length], &data->preedit_feedback[pre_size], post_size * sizeof(*feedback));
            }
        }

        if (preedit_text && feedback) {
            SDL_free(data->preedit_text);
            data->preedit_text = preedit_text;

            SDL_free(data->preedit_feedback);
            data->preedit_feedback = feedback;

            data->preedit_length += text->length;
        } else {
            SDL_free(preedit_text);
            SDL_free(feedback);
        }
    }

    data->preedit_cursor = call_data->caret;

#ifdef DEBUG_XIM
    if (call_data->chg_length > 0) {
        SDL_Log("Draw callback deleted %d characters at %d", call_data->chg_length, call_data->chg_first);
    }
    if (text) {
        SDL_Log("Draw callback inserted %s at %d, caret: %d", text->string.multi_byte, call_data->chg_first, call_data->caret);
    }
    SDL_Log("Pre-edit text: %s", data->preedit_text);
#endif

    X11_SendEditingEvent(data);
}

static void preedit_caret_callback(XIC xic, XPointer client_data, XIMPreeditCaretCallbackStruct *call_data)
{
    SDL_WindowData *data = (SDL_WindowData *)client_data;

    switch (call_data->direction) {
    case XIMAbsolutePosition:
        if (call_data->position != data->preedit_cursor) {
            data->preedit_cursor = call_data->position;
            X11_SendEditingEvent(data);
        }
        break;
    case XIMDontChange:
        break;
    default:
        // Not currently supported
        break;
    }
}

void X11_CreateInputContext(SDL_WindowData *data)
{
#ifdef X_HAVE_UTF8_STRING
    SDL_VideoData *videodata = data->videodata;

    if (SDL_X11_HAVE_UTF8 && videodata->im) {
        const char *hint = SDL_GetHint(SDL_HINT_IME_IMPLEMENTED_UI);
        if (hint && SDL_strstr(hint, "composition")) {
            XIMCallback draw_callback;
            draw_callback.client_data = (XPointer)data;
            draw_callback.callback = (XIMProc)preedit_draw_callback;

            XIMCallback start_callback;
            start_callback.client_data = (XPointer)data;
            start_callback.callback = (XIMProc)preedit_start_callback;

            XIMCallback done_callback;
            done_callback.client_data = (XPointer)data;
            done_callback.callback = (XIMProc)preedit_done_callback;

            XIMCallback caret_callback;
            caret_callback.client_data = (XPointer)data;
            caret_callback.callback = (XIMProc)preedit_caret_callback;

            XVaNestedList attr = X11_XVaCreateNestedList(0,
                                                         XNPreeditStartCallback, &start_callback,
                                                         XNPreeditDoneCallback, &done_callback,
                                                         XNPreeditDrawCallback, &draw_callback,
                                                         XNPreeditCaretCallback, &caret_callback,
                                                         NULL);
            if (attr) {
                data->ic = X11_XCreateIC(videodata->im,
                                         XNInputStyle, XIMPreeditCallbacks | XIMStatusCallbacks,
                                         XNPreeditAttributes, attr,
                                         XNClientWindow, data->xwindow,
                                         NULL);
                X11_XFree(attr);
            }
        }
        if (!data->ic) {
            data->ic = X11_XCreateIC(videodata->im,
                                     XNInputStyle, XIMPreeditNothing | XIMStatusNothing,
                                     XNClientWindow, data->xwindow,
                                     NULL);
        }
        data->xim_spot.x = -1;
        data->xim_spot.y = -1;
    }
#endif // X_HAVE_UTF8_STRING
}

static void X11_ResetXIM(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef X_HAVE_UTF8_STRING
    SDL_WindowData *data = window->internal;

    if (data && data->ic) {
        // Clear any partially entered dead keys
        char *contents = X11_Xutf8ResetIC(data->ic);
        if (contents) {
            X11_XFree(contents);
        }
    }
#endif // X_HAVE_UTF8_STRING
}

bool X11_StartTextInput(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
    X11_ResetXIM(_this, window);

    return X11_UpdateTextInputArea(_this, window);
}

bool X11_StopTextInput(SDL_VideoDevice *_this, SDL_Window *window)
{
    X11_ResetXIM(_this, window);
    return true;
}

bool X11_UpdateTextInputArea(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef X_HAVE_UTF8_STRING
    SDL_WindowData *data = window->internal;

    if (data && data->ic) {
        XPoint spot;
        spot.x = window->text_input_rect.x + window->text_input_cursor;
        spot.y = window->text_input_rect.y + window->text_input_rect.h;
        if (spot.x != data->xim_spot.x || spot.y != data->xim_spot.y) {
            XVaNestedList attr = X11_XVaCreateNestedList(0, XNSpotLocation, &spot, NULL);
            if (attr) {
                X11_XSetICValues(data->ic, XNPreeditAttributes, attr, NULL);
                X11_XFree(attr);
            }
            SDL_copyp(&data->xim_spot, &spot);
        }
    }
#endif
    return true;
}

bool X11_HasScreenKeyboardSupport(SDL_VideoDevice *_this)
{
    SDL_VideoData *videodata = _this->internal;
    return videodata->is_steam_deck;
}

void X11_ShowScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window, SDL_PropertiesID props)
{
    SDL_VideoData *videodata = _this->internal;

    if (videodata->is_steam_deck) {
        /* For more documentation of the URL parameters, see:
         * https://partner.steamgames.com/doc/api/ISteamUtils#ShowFloatingGamepadTextInput
         */
        const int k_EFloatingGamepadTextInputModeModeSingleLine = 0;    // Enter dismisses the keyboard
        const int k_EFloatingGamepadTextInputModeModeMultipleLines = 1; // User needs to explicitly dismiss the keyboard
        const int k_EFloatingGamepadTextInputModeModeEmail = 2;         // Keyboard is displayed in a special mode that makes it easier to enter emails
        const int k_EFloatingGamepadTextInputModeModeNumeric = 3;       // Numeric keypad is shown
        char deeplink[128];
        int mode;

        switch (SDL_GetTextInputType(props)) {
        case SDL_TEXTINPUT_TYPE_TEXT_EMAIL:
            mode = k_EFloatingGamepadTextInputModeModeEmail;
            break;
        case SDL_TEXTINPUT_TYPE_NUMBER:
        case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_HIDDEN:
        case SDL_TEXTINPUT_TYPE_NUMBER_PASSWORD_VISIBLE:
            mode = k_EFloatingGamepadTextInputModeModeNumeric;
            break;
        default:
            if (SDL_GetTextInputMultiline(props)) {
                mode = k_EFloatingGamepadTextInputModeModeMultipleLines;
            } else {
                mode = k_EFloatingGamepadTextInputModeModeSingleLine;
            }
            break;
        }
        (void)SDL_snprintf(deeplink, sizeof(deeplink),
                           "steam://open/keyboard?XPosition=%i&YPosition=%i&Width=%i&Height=%i&Mode=%d",
                           window->text_input_rect.x, window->text_input_rect.y,
                           window->text_input_rect.w, window->text_input_rect.h,
                           mode);
        SDL_OpenURL(deeplink);
        videodata->steam_keyboard_open = true;
    }
}

void X11_HideScreenKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *videodata = _this->internal;

    if (videodata->is_steam_deck) {
        SDL_OpenURL("steam://close/keyboard");
        videodata->steam_keyboard_open = false;
    }
}

bool X11_IsScreenKeyboardShown(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_VideoData *videodata = _this->internal;

    return videodata->steam_keyboard_open;
}

#endif // SDL_VIDEO_DRIVER_X11
