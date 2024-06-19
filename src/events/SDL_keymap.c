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

#include "SDL_keymap_c.h"
#include "../SDL_hashtable.h"

struct SDL_Keymap
{
    SDL_HashTable *scancode_to_keycode;
    SDL_HashTable *keycode_to_scancode;
};

SDL_Keymap *SDL_CreateKeymap(void)
{
    SDL_Keymap *keymap = (SDL_Keymap *)SDL_malloc(sizeof(*keymap));
    if (!keymap) {
        return NULL;
    }

    keymap->scancode_to_keycode = SDL_CreateHashTable(NULL, 64, SDL_HashID, SDL_KeyMatchID, NULL, SDL_FALSE);
    keymap->keycode_to_scancode = SDL_CreateHashTable(NULL, 64, SDL_HashID, SDL_KeyMatchID, NULL, SDL_FALSE);
    if (!keymap->scancode_to_keycode || !keymap->keycode_to_scancode) {
        SDL_DestroyKeymap(keymap);
        return NULL;
    }
    return keymap;
}

static SDL_Keymod NormalizeModifierStateForKeymap(SDL_Keymod modstate)
{
    // The modifiers that affect the keymap are: SHIFT, CAPS, ALT, and MODE
    modstate &= (SDL_KMOD_SHIFT | SDL_KMOD_CAPS | SDL_KMOD_ALT | SDL_KMOD_MODE);

    // If either right or left Shift are set, set both in the output
    if (modstate & SDL_KMOD_SHIFT) {
        modstate |= SDL_KMOD_SHIFT;
    }

    // If either right or left Alt are set, set both in the output
    if (modstate & SDL_KMOD_ALT) {
        modstate |= SDL_KMOD_ALT;
    }

    return modstate;
}

void SDL_SetKeymapEntry(SDL_Keymap *keymap, SDL_Scancode scancode, SDL_Keymod modstate, SDL_Keycode keycode)
{
    if (!keymap) {
        return;
    }

    if (keycode == SDL_GetDefaultKeyFromScancode(scancode, modstate)) {
        return;
    }

    Uint32 key = ((Uint32)NormalizeModifierStateForKeymap(modstate) << 16) | scancode;
    SDL_InsertIntoHashTable(keymap->scancode_to_keycode, (void *)(uintptr_t)key, (void *)(uintptr_t)keycode);
    SDL_InsertIntoHashTable(keymap->keycode_to_scancode, (void *)(uintptr_t)keycode, (void *)(uintptr_t)key);
}

SDL_Keycode SDL_GetKeymapKeycode(SDL_Keymap *keymap, SDL_Scancode scancode, SDL_Keymod modstate)
{
    SDL_Keycode keycode;

    Uint32 key = ((Uint32)NormalizeModifierStateForKeymap(modstate) << 16) | scancode;
    const void *value;
    if (keymap && SDL_FindInHashTable(keymap->scancode_to_keycode, (void *)(uintptr_t)key, &value)) {
        keycode = (SDL_Keycode)(uintptr_t)value;
    } else {
        keycode = SDL_GetDefaultKeyFromScancode(scancode, modstate);
    }
    return keycode;
}

SDL_Scancode SDL_GetKeymapScancode(SDL_Keymap *keymap, SDL_Keycode keycode, SDL_Keymod *modstate)
{
    SDL_Scancode scancode;

    const void *value;
    if (keymap && SDL_FindInHashTable(keymap->keycode_to_scancode, (void *)(uintptr_t)keycode, &value)) {
        scancode = (SDL_Scancode)((uintptr_t)value & 0xFFFF);
        if (modstate) {
            *modstate = (SDL_Keymod)((uintptr_t)value >> 16);
        }
    } else {
        scancode = SDL_GetDefaultScancodeFromKey(keycode, modstate);
    }
    return scancode;
}

void SDL_ResetKeymap(SDL_Keymap *keymap)
{
    if (keymap) {
        SDL_EmptyHashTable(keymap->scancode_to_keycode);
        SDL_EmptyHashTable(keymap->keycode_to_scancode);
    }
}

void SDL_DestroyKeymap(SDL_Keymap *keymap)
{
    if (keymap) {
        SDL_DestroyHashTable(keymap->scancode_to_keycode);
        SDL_DestroyHashTable(keymap->keycode_to_scancode);
        SDL_free(keymap);
    }
}

static const SDL_Keycode normal_default_symbols[] = {
    SDLK_1,
    SDLK_2,
    SDLK_3,
    SDLK_4,
    SDLK_5,
    SDLK_6,
    SDLK_7,
    SDLK_8,
    SDLK_9,
    SDLK_0,
    SDLK_RETURN,
    SDLK_ESCAPE,
    SDLK_BACKSPACE,
    SDLK_TAB,
    SDLK_SPACE,
    SDLK_MINUS,
    SDLK_EQUALS,
    SDLK_LEFTBRACKET,
    SDLK_RIGHTBRACKET,
    SDLK_BACKSLASH,
    SDLK_HASH,
    SDLK_SEMICOLON,
    SDLK_APOSTROPHE,
    SDLK_GRAVE,
    SDLK_COMMA,
    SDLK_PERIOD,
    SDLK_SLASH,
};

static const SDL_Keycode shifted_default_symbols[] = {
    SDLK_EXCLAIM,
    SDLK_AT,
    SDLK_HASH,
    SDLK_DOLLAR,
    SDLK_PERCENT,
    SDLK_CARET,
    SDLK_AMPERSAND,
    SDLK_ASTERISK,
    SDLK_LEFTPAREN,
    SDLK_RIGHTPAREN,
    SDLK_RETURN,
    SDLK_ESCAPE,
    SDLK_BACKSPACE,
    SDLK_TAB,
    SDLK_SPACE,
    SDLK_UNDERSCORE,
    SDLK_PLUS,
    SDLK_LEFTBRACE,
    SDLK_RIGHTBRACE,
    SDLK_PIPE,
    SDLK_HASH,
    SDLK_COLON,
    SDLK_DBLAPOSTROPHE,
    SDLK_TILDE,
    SDLK_LESS,
    SDLK_GREATER,
    SDLK_QUESTION
};

SDL_Keycode SDL_GetDefaultKeyFromScancode(SDL_Scancode scancode, SDL_Keymod modstate)
{
    if (((int)scancode) < SDL_SCANCODE_UNKNOWN || scancode >= SDL_NUM_SCANCODES) {
        SDL_InvalidParamError("scancode");
        return SDLK_UNKNOWN;
    }

    if (modstate & SDL_KMOD_MODE) {
        return SDLK_UNKNOWN;
    }

    if (scancode < SDL_SCANCODE_A) {
        return SDLK_UNKNOWN;
    }

    if (scancode < SDL_SCANCODE_1) {
        SDL_bool shifted = (modstate & SDL_KMOD_SHIFT) ? SDL_TRUE : SDL_FALSE;
#ifdef SDL_PLATFORM_APPLE
        // Apple maps to upper case for either shift or capslock inclusive
        if (modstate & SDL_KMOD_CAPS) {
            shifted = SDL_TRUE;
        }
#else
        if (modstate & SDL_KMOD_CAPS) {
            shifted = !shifted;
        }
#endif
        if (!shifted) {
            return (SDL_Keycode)('a' + scancode - SDL_SCANCODE_A);
        } else {
            return (SDL_Keycode)('A' + scancode - SDL_SCANCODE_A);
        }
    }

    if (scancode < SDL_SCANCODE_CAPSLOCK) {
        SDL_bool shifted = (modstate & SDL_KMOD_SHIFT) ? SDL_TRUE : SDL_FALSE;

        if (!shifted) {
            return normal_default_symbols[scancode - SDL_SCANCODE_1];
        } else {
            return shifted_default_symbols[scancode - SDL_SCANCODE_1];
        }
    }

    if (scancode == SDL_SCANCODE_DELETE) {
        return SDLK_DELETE;
    }

    return SDL_SCANCODE_TO_KEYCODE(scancode);
}

SDL_Scancode SDL_GetDefaultScancodeFromKey(SDL_Keycode key, SDL_Keymod *modstate)
{
    if (modstate) {
        *modstate = SDL_KMOD_NONE;
    }

    if (key == SDLK_UNKNOWN) {
        return SDL_SCANCODE_UNKNOWN;
    }

    if (key & SDLK_SCANCODE_MASK) {
        return (SDL_Scancode)(key & ~SDLK_SCANCODE_MASK);
    }

    if (key >= SDLK_a && key <= SDLK_z) {
        return (SDL_Scancode)(SDL_SCANCODE_A + key - SDLK_a);
    }

    if (key >= SDLK_Z && key <= SDLK_Z) {
        if (modstate) {
            *modstate = SDL_KMOD_SHIFT;
        }
        return (SDL_Scancode)(SDL_SCANCODE_A + key - SDLK_Z);
    }

    for (int i = 0; i < SDL_arraysize(normal_default_symbols); ++i) {
        if (key == normal_default_symbols[i]) {
            return(SDL_Scancode)(SDL_SCANCODE_1 + i);
        }
    }

    for (int i = 0; i < SDL_arraysize(shifted_default_symbols); ++i) {
        if (key == shifted_default_symbols[i]) {
            if (modstate) {
                *modstate = SDL_KMOD_SHIFT;
            }
            return(SDL_Scancode)(SDL_SCANCODE_1 + i);
        }
    }

    if (key == SDLK_DELETE) {
        return SDL_SCANCODE_DELETE;
    }

    return SDL_SCANCODE_UNKNOWN;
}

static const char *SDL_scancode_names[SDL_NUM_SCANCODES] =
{
    /* 0 */ NULL,
    /* 1 */ NULL,
    /* 2 */ NULL,
    /* 3 */ NULL,
    /* 4 */ "A",
    /* 5 */ "B",
    /* 6 */ "C",
    /* 7 */ "D",
    /* 8 */ "E",
    /* 9 */ "F",
    /* 10 */ "G",
    /* 11 */ "H",
    /* 12 */ "I",
    /* 13 */ "J",
    /* 14 */ "K",
    /* 15 */ "L",
    /* 16 */ "M",
    /* 17 */ "N",
    /* 18 */ "O",
    /* 19 */ "P",
    /* 20 */ "Q",
    /* 21 */ "R",
    /* 22 */ "S",
    /* 23 */ "T",
    /* 24 */ "U",
    /* 25 */ "V",
    /* 26 */ "W",
    /* 27 */ "X",
    /* 28 */ "Y",
    /* 29 */ "Z",
    /* 30 */ "1",
    /* 31 */ "2",
    /* 32 */ "3",
    /* 33 */ "4",
    /* 34 */ "5",
    /* 35 */ "6",
    /* 36 */ "7",
    /* 37 */ "8",
    /* 38 */ "9",
    /* 39 */ "0",
    /* 40 */ "Return",
    /* 41 */ "Escape",
    /* 42 */ "Backspace",
    /* 43 */ "Tab",
    /* 44 */ "Space",
    /* 45 */ "-",
    /* 46 */ "=",
    /* 47 */ "[",
    /* 48 */ "]",
    /* 49 */ "\\",
    /* 50 */ "#",
    /* 51 */ ";",
    /* 52 */ "'",
    /* 53 */ "`",
    /* 54 */ ",",
    /* 55 */ ".",
    /* 56 */ "/",
    /* 57 */ "CapsLock",
    /* 58 */ "F1",
    /* 59 */ "F2",
    /* 60 */ "F3",
    /* 61 */ "F4",
    /* 62 */ "F5",
    /* 63 */ "F6",
    /* 64 */ "F7",
    /* 65 */ "F8",
    /* 66 */ "F9",
    /* 67 */ "F10",
    /* 68 */ "F11",
    /* 69 */ "F12",
    /* 70 */ "PrintScreen",
    /* 71 */ "ScrollLock",
    /* 72 */ "Pause",
    /* 73 */ "Insert",
    /* 74 */ "Home",
    /* 75 */ "PageUp",
    /* 76 */ "Delete",
    /* 77 */ "End",
    /* 78 */ "PageDown",
    /* 79 */ "Right",
    /* 80 */ "Left",
    /* 81 */ "Down",
    /* 82 */ "Up",
    /* 83 */ "Numlock",
    /* 84 */ "Keypad /",
    /* 85 */ "Keypad *",
    /* 86 */ "Keypad -",
    /* 87 */ "Keypad +",
    /* 88 */ "Keypad Enter",
    /* 89 */ "Keypad 1",
    /* 90 */ "Keypad 2",
    /* 91 */ "Keypad 3",
    /* 92 */ "Keypad 4",
    /* 93 */ "Keypad 5",
    /* 94 */ "Keypad 6",
    /* 95 */ "Keypad 7",
    /* 96 */ "Keypad 8",
    /* 97 */ "Keypad 9",
    /* 98 */ "Keypad 0",
    /* 99 */ "Keypad .",
    /* 100 */ NULL,
    /* 101 */ "Application",
    /* 102 */ "Power",
    /* 103 */ "Keypad =",
    /* 104 */ "F13",
    /* 105 */ "F14",
    /* 106 */ "F15",
    /* 107 */ "F16",
    /* 108 */ "F17",
    /* 109 */ "F18",
    /* 110 */ "F19",
    /* 111 */ "F20",
    /* 112 */ "F21",
    /* 113 */ "F22",
    /* 114 */ "F23",
    /* 115 */ "F24",
    /* 116 */ "Execute",
    /* 117 */ "Help",
    /* 118 */ "Menu",
    /* 119 */ "Select",
    /* 120 */ "Stop",
    /* 121 */ "Again",
    /* 122 */ "Undo",
    /* 123 */ "Cut",
    /* 124 */ "Copy",
    /* 125 */ "Paste",
    /* 126 */ "Find",
    /* 127 */ "Mute",
    /* 128 */ "VolumeUp",
    /* 129 */ "VolumeDown",
    /* 130 */ NULL,
    /* 131 */ NULL,
    /* 132 */ NULL,
    /* 133 */ "Keypad ,",
    /* 134 */ "Keypad = (AS400)",
    /* 135 */ NULL,
    /* 136 */ NULL,
    /* 137 */ NULL,
    /* 138 */ NULL,
    /* 139 */ NULL,
    /* 140 */ NULL,
    /* 141 */ NULL,
    /* 142 */ NULL,
    /* 143 */ NULL,
    /* 144 */ NULL,
    /* 145 */ NULL,
    /* 146 */ NULL,
    /* 147 */ NULL,
    /* 148 */ NULL,
    /* 149 */ NULL,
    /* 150 */ NULL,
    /* 151 */ NULL,
    /* 152 */ NULL,
    /* 153 */ "AltErase",
    /* 154 */ "SysReq",
    /* 155 */ "Cancel",
    /* 156 */ "Clear",
    /* 157 */ "Prior",
    /* 158 */ "Return",
    /* 159 */ "Separator",
    /* 160 */ "Out",
    /* 161 */ "Oper",
    /* 162 */ "Clear / Again",
    /* 163 */ "CrSel",
    /* 164 */ "ExSel",
    /* 165 */ NULL,
    /* 166 */ NULL,
    /* 167 */ NULL,
    /* 168 */ NULL,
    /* 169 */ NULL,
    /* 170 */ NULL,
    /* 171 */ NULL,
    /* 172 */ NULL,
    /* 173 */ NULL,
    /* 174 */ NULL,
    /* 175 */ NULL,
    /* 176 */ "Keypad 00",
    /* 177 */ "Keypad 000",
    /* 178 */ "ThousandsSeparator",
    /* 179 */ "DecimalSeparator",
    /* 180 */ "CurrencyUnit",
    /* 181 */ "CurrencySubUnit",
    /* 182 */ "Keypad (",
    /* 183 */ "Keypad )",
    /* 184 */ "Keypad {",
    /* 185 */ "Keypad }",
    /* 186 */ "Keypad Tab",
    /* 187 */ "Keypad Backspace",
    /* 188 */ "Keypad A",
    /* 189 */ "Keypad B",
    /* 190 */ "Keypad C",
    /* 191 */ "Keypad D",
    /* 192 */ "Keypad E",
    /* 193 */ "Keypad F",
    /* 194 */ "Keypad XOR",
    /* 195 */ "Keypad ^",
    /* 196 */ "Keypad %",
    /* 197 */ "Keypad <",
    /* 198 */ "Keypad >",
    /* 199 */ "Keypad &",
    /* 200 */ "Keypad &&",
    /* 201 */ "Keypad |",
    /* 202 */ "Keypad ||",
    /* 203 */ "Keypad :",
    /* 204 */ "Keypad #",
    /* 205 */ "Keypad Space",
    /* 206 */ "Keypad @",
    /* 207 */ "Keypad !",
    /* 208 */ "Keypad MemStore",
    /* 209 */ "Keypad MemRecall",
    /* 210 */ "Keypad MemClear",
    /* 211 */ "Keypad MemAdd",
    /* 212 */ "Keypad MemSubtract",
    /* 213 */ "Keypad MemMultiply",
    /* 214 */ "Keypad MemDivide",
    /* 215 */ "Keypad +/-",
    /* 216 */ "Keypad Clear",
    /* 217 */ "Keypad ClearEntry",
    /* 218 */ "Keypad Binary",
    /* 219 */ "Keypad Octal",
    /* 220 */ "Keypad Decimal",
    /* 221 */ "Keypad Hexadecimal",
    /* 222 */ NULL,
    /* 223 */ NULL,
    /* 224 */ "Left Ctrl",
    /* 225 */ "Left Shift",
    /* 226 */ "Left Alt",
    /* 227 */ "Left GUI",
    /* 228 */ "Right Ctrl",
    /* 229 */ "Right Shift",
    /* 230 */ "Right Alt",
    /* 231 */ "Right GUI",
    /* 232 */ NULL,
    /* 233 */ NULL,
    /* 234 */ NULL,
    /* 235 */ NULL,
    /* 236 */ NULL,
    /* 237 */ NULL,
    /* 238 */ NULL,
    /* 239 */ NULL,
    /* 240 */ NULL,
    /* 241 */ NULL,
    /* 242 */ NULL,
    /* 243 */ NULL,
    /* 244 */ NULL,
    /* 245 */ NULL,
    /* 246 */ NULL,
    /* 247 */ NULL,
    /* 248 */ NULL,
    /* 249 */ NULL,
    /* 250 */ NULL,
    /* 251 */ NULL,
    /* 252 */ NULL,
    /* 253 */ NULL,
    /* 254 */ NULL,
    /* 255 */ NULL,
    /* 256 */ NULL,
    /* 257 */ "ModeSwitch",
    /* 258 */ "Sleep",
    /* 259 */ "Wake",
    /* 260 */ "ChannelUp",
    /* 261 */ "ChannelDown",
    /* 262 */ "MediaPlay",
    /* 263 */ "MediaPause",
    /* 264 */ "MediaRecord",
    /* 265 */ "MediaFastForward",
    /* 266 */ "MediaRewind",
    /* 267 */ "MediaTrackNext",
    /* 268 */ "MediaTrackPrevious",
    /* 269 */ "MediaStop",
    /* 270 */ "Eject",
    /* 271 */ "MediaPlayPause",
    /* 272 */ "MediaSelect",
    /* 273 */ "AC New",
    /* 274 */ "AC Open",
    /* 275 */ "AC Close",
    /* 276 */ "AC Exit",
    /* 277 */ "AC Save",
    /* 278 */ "AC Print",
    /* 279 */ "AC Properties",
    /* 280 */ "AC Search",
    /* 281 */ "AC Home",
    /* 282 */ "AC Back",
    /* 283 */ "AC Forward",
    /* 284 */ "AC Stop",
    /* 285 */ "AC Refresh",
    /* 286 */ "AC Bookmarks",
    /* 287 */ "SoftLeft",
    /* 288 */ "SoftRight",
    /* 289 */ "Call",
    /* 290 */ "EndCall",
};

int SDL_SetScancodeName(SDL_Scancode scancode, const char *name)
{
    if (((int)scancode) < SDL_SCANCODE_UNKNOWN || scancode >= SDL_NUM_SCANCODES) {
        return SDL_InvalidParamError("scancode");
    }

    SDL_scancode_names[scancode] = name;
    return 0;
}

// these are static memory, so we don't use SDL_FreeLater on them.
const char *SDL_GetScancodeName(SDL_Scancode scancode)
{
    const char *name;
    if (((int)scancode) < SDL_SCANCODE_UNKNOWN || scancode >= SDL_NUM_SCANCODES) {
        SDL_InvalidParamError("scancode");
        return "";
    }

    name = SDL_scancode_names[scancode];
    if (name) {
        return name;
    }

    return "";
}

SDL_Scancode SDL_GetScancodeFromName(const char *name)
{
    int i;

    if (!name || !*name) {
        SDL_InvalidParamError("name");
        return SDL_SCANCODE_UNKNOWN;
    }

    for (i = 0; i < SDL_arraysize(SDL_scancode_names); ++i) {
        if (!SDL_scancode_names[i]) {
            continue;
        }
        if (SDL_strcasecmp(name, SDL_scancode_names[i]) == 0) {
            return (SDL_Scancode)i;
        }
    }

    SDL_InvalidParamError("name");
    return SDL_SCANCODE_UNKNOWN;
}

const char *SDL_GetKeyName(SDL_Keycode key)
{
    char name[8];
    char *end;

    if (key & SDLK_SCANCODE_MASK) {
        return SDL_GetScancodeName((SDL_Scancode)(key & ~SDLK_SCANCODE_MASK));
    }

    switch (key) {
    case SDLK_RETURN:
        return SDL_GetScancodeName(SDL_SCANCODE_RETURN);
    case SDLK_ESCAPE:
        return SDL_GetScancodeName(SDL_SCANCODE_ESCAPE);
    case SDLK_BACKSPACE:
        return SDL_GetScancodeName(SDL_SCANCODE_BACKSPACE);
    case SDLK_TAB:
        return SDL_GetScancodeName(SDL_SCANCODE_TAB);
    case SDLK_SPACE:
        return SDL_GetScancodeName(SDL_SCANCODE_SPACE);
    case SDLK_DELETE:
        return SDL_GetScancodeName(SDL_SCANCODE_DELETE);
    default:
        /* Unaccented letter keys on latin keyboards are normally
           labeled in upper case (and probably on others like Greek or
           Cyrillic too, so if you happen to know for sure, please
           adapt this). */
        if (key >= 'a' && key <= 'z') {
            key -= 32;
        }

        end = SDL_UCS4ToUTF8(key, name);
        *end = '\0';
        return SDL_FreeLater(SDL_strdup(name));
    }
}

SDL_Keycode SDL_GetKeyFromName(const char *name)
{
    SDL_Keycode key;

    /* Check input */
    if (!name) {
        return SDLK_UNKNOWN;
    }

    /* If it's a single UTF-8 character, then that's the keycode itself */
    key = *(const unsigned char *)name;
    if (key >= 0xF0) {
        if (SDL_strlen(name) == 4) {
            int i = 0;
            key = (Uint16)(name[i] & 0x07) << 18;
            key |= (Uint16)(name[++i] & 0x3F) << 12;
            key |= (Uint16)(name[++i] & 0x3F) << 6;
            key |= (Uint16)(name[++i] & 0x3F);
            return key;
        }
        return SDLK_UNKNOWN;
    } else if (key >= 0xE0) {
        if (SDL_strlen(name) == 3) {
            int i = 0;
            key = (Uint16)(name[i] & 0x0F) << 12;
            key |= (Uint16)(name[++i] & 0x3F) << 6;
            key |= (Uint16)(name[++i] & 0x3F);
            return key;
        }
        return SDLK_UNKNOWN;
    } else if (key >= 0xC0) {
        if (SDL_strlen(name) == 2) {
            int i = 0;
            key = (Uint16)(name[i] & 0x1F) << 6;
            key |= (Uint16)(name[++i] & 0x3F);
            return key;
        }
        return SDLK_UNKNOWN;
    } else {
        if (SDL_strlen(name) == 1) {
            if (key >= 'A' && key <= 'Z') {
                key += 32;
            }
            return key;
        }

        /* Get the scancode for this name, and the associated keycode */
        return SDL_GetKeyFromScancode(SDL_GetScancodeFromName(name), SDL_KMOD_NONE);
    }
}
