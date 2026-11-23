/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

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

#if defined(SDL_VIDEO_DRIVER_WAYLAND) || defined(SDL_VIDEO_DRIVER_X11)

#include "SDL_keyboard_c.h"
#include "SDL_keysym_to_scancode_c.h"
#include "imKStoUCS.h"

/* Unicode symbols for keysyms corresponding to dead keys.
 * The symbols used here are the spaced versions, when available.
 */
static const Uint16 dead_key_symbols_fe50_fe6f[] = {
     0x0060,  // XKB_KEY_dead_grave
     0x00b4,  // XKB_KEY_dead_acute
     0x005e,  // XKB_KEY_dead_circumflex
     0x1fc0,  // XKB_KEY_dead_perispomeni
     0x00af,  // XKB_KEY_dead_macron
     0x02d8,  // XKB_KEY_dead_breve
     0x02d9,  // XKB_KEY_dead_abovedot
     0x00a8,  // XKB_KEY_dead_diaeresis
     0x02da,  // XKB_KEY_dead_abovering
     0x02dd,  // XKB_KEY_dead_doubleacute
     0x02c7,  // XKB_KEY_dead_caron
     0x00b8,  // XKB_KEY_dead_cedilla
     0x02db,  // XKB_KEY_dead_ogonek
     0x037a,  // XKB_KEY_dead_iota
     0x309b,  // XKB_KEY_dead_voiced_sound
     0x309c,  // XKB_KEY_dead_semivoiced_sound
     0x0323,  // XKB_KEY_dead_belowdot
     0x0309,  // XKB_KEY_dead_hook
     0x031b,  // XKB_KEY_dead_horn
     0x002f,  // XKB_KEY_dead_stroke
     0x02bc,  // XKB_KEY_dead_psili
     0x02bd,  // XKB_KEY_dead_dasia
     0x02f5,  // XKB_KEY_dead_doublegrave
     0x02f3,  // XKB_KEY_dead_belowring
     0x005f,  // XKB_KEY_dead_belowmacron
     0x005e,  // XKB_KEY_dead_belowcircumflex
     0x02f7,  // XKB_KEY_dead_belowtilde
     0x032e,  // XKB_KEY_dead_belowbreve
     0x0324,  // XKB_KEY_dead_belowdiaeresis
     0x0311,  // XKB_KEY_dead_invertedbreve
     0x0326,  // XKB_KEY_dead_belowcomma
     0x00a4,  // XKB_KEY_dead_currency
};

static const Uint16 dead_key_symbols_fe80_fe93[] = {
    0x0061, // XKB_KEY_dead_a
    0x0041, // XKB_KEY_dead_A
    0x0065, // XKB_KEY_dead_e
    0x0045, // XKB_KEY_dead_E
    0x0069, // XKB_KEY_dead_i
    0x0049, // XKB_KEY_dead_I
    0x006f, // XKB_KEY_dead_o
    0x004f, // XKB_KEY_dead_O
    0x0075, // XKB_KEY_dead_u
    0x0055, // XKB_KEY_dead_U
    0x0259, // XKB_KEY_dead_schwa
    0x018f, // XKB_KEY_dead_SCHWA
    0x0000, // XKB_KEY_dead_greek
    0x0621, // XKB_KEY_dead_hamza
    0x0000,
    0x0000,
    0x005f, // XKB_KEY_dead_lowline
    0x02c8, // XKB_KEY_dead_aboveverticalline
    0x02cc, // XKB_KEY_dead_belowverticalline
    0x002f, // XKB_KEY_dead_longsolidusoverlay
};

// Extended key code mappings
static const struct
{
    Uint32 keysym;
    SDL_Keycode keycode;
} keysym_to_keycode_table[] = {
    { 0xfe03, SDLK_MODE },              // XK_ISO_Level3_Shift
    { 0xfe11, SDLK_LEVEL5_SHIFT },      // XK_ISO_Level5_Shift
    { 0xfe20, SDLK_LEFT_TAB },          // XK_ISO_Left_Tab
    { 0xff20, SDLK_MULTI_KEY_COMPOSE }, // XK_Multi_key
    { 0xffe7, SDLK_LMETA },             // XK_Meta_L
    { 0xffe8, SDLK_RMETA },             // XK_Meta_R
    { 0xffed, SDLK_LHYPER },            // XK_Hyper_L
    { 0xffee, SDLK_RHYPER },            // XK_Hyper_R
};

SDL_Keycode SDL_GetKeyCodeFromKeySym(Uint32 keysym, Uint32 keycode, SDL_Keymod modifiers)
{
    SDL_Keycode sdl_keycode = SDL_KeySymToUcs4(keysym);

    if (!sdl_keycode) {
        if (keysym >= 0xfe50 && keysym <= 0xfe6f) {
            sdl_keycode = dead_key_symbols_fe50_fe6f[keysym - 0xfe50];
        } else if (keysym >= 0xfe80 && keysym <= 0xfe93) {
            sdl_keycode = dead_key_symbols_fe80_fe93[keysym - 0xfe80];
        }
    }

    if (!sdl_keycode) {
        for (int i = 0; i < SDL_arraysize(keysym_to_keycode_table); ++i) {
            if (keysym == keysym_to_keycode_table[i].keysym) {
                return keysym_to_keycode_table[i].keycode;
            }
        }
    }

    if (!sdl_keycode) {
        const SDL_Scancode scancode = SDL_GetScancodeFromKeySym(keysym, keycode);
        if (scancode != SDL_SCANCODE_UNKNOWN) {
            sdl_keycode = SDL_GetKeymapKeycode(NULL, scancode, modifiers);
        }
    }

    return sdl_keycode;
}

#endif // SDL_VIDEO_DRIVER_WAYLAND || SDL_VIDEO_DRIVER_X11
