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

#ifdef SDL_VIDEO_DRIVER_PUMPKINOS

/* PumpkinOS input.

   PumpkinOS delivers input to a task in two ways, and this driver uses both:

   - The PalmOS event queue (EvtGetEvent): pen down/up, key down/up with the
     PalmOS character code, wheel, display size changes and the request to
     stop the application. Key events carry characters, not physical keys,
     and arrow keys show up as five-way navigation changes, so they are only
     used here for text input and for the few keys that have no bit in the
     key masks (F5..F10, Home, Insert, Delete, PgUp, PgDn).

   - The key and pen state polled with pumpkin_status(): the PalmOS hard key
     mask (arrows, F1..F4, End), the modifier mask and a bit mask of the
     ASCII keys currently held. These give proper press/release pairs and are
     what the Doom and GNUBoy ports use. The pen position is polled as well,
     because motion events are only queued while the pen is down. */

#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_windowevents_c.h"

#include "SDL_pumpkinvideo.h"
#include "SDL_pumpkinevents_c.h"
#include "../../core/pumpkin/SDL_pumpkin.h"

// upper bound on queued PalmOS events handled per pump
#define PUMPKIN_MAX_EVENTS_PER_PUMP 128

typedef struct PUMPKIN_MaskKey
{
    Uint32 bit;
    SDL_Scancode scancode;
} PUMPKIN_MaskKey;

// PalmOS hard keys, as set by the arrow, function and End keys of the host
static const PUMPKIN_MaskKey key_mask_table[] = {
    { keyBitPageUp, SDL_SCANCODE_UP },
    { keyBitPageDown, SDL_SCANCODE_DOWN },
    { keyBitLeft, SDL_SCANCODE_LEFT },
    { keyBitRight, SDL_SCANCODE_RIGHT },
    { keyBitHard1, SDL_SCANCODE_F1 },
    { keyBitHard2, SDL_SCANCODE_F2 },
    { keyBitHard3, SDL_SCANCODE_F3 },
    { keyBitHard4, SDL_SCANCODE_F4 },
    { keyBitPower, SDL_SCANCODE_END },
};

static const PUMPKIN_MaskKey mod_mask_table[] = {
    { WINDOW_MOD_SHIFT, SDL_SCANCODE_LSHIFT },
    { WINDOW_MOD_CTRL, SDL_SCANCODE_LCTRL },
    { WINDOW_MOD_RCTRL, SDL_SCANCODE_RCTRL },
    { WINDOW_MOD_LALT, SDL_SCANCODE_LALT },
    { WINDOW_MOD_RALT, SDL_SCANCODE_RALT },
};

// Physical key for an ASCII code, assuming a US layout. Shifted and
// unshifted characters of the same key map to the same scancode, so a key
// released after the modifier is let go still matches the press.
static SDL_Scancode PUMPKIN_ScancodeForASCII(int c)
{
    if (c >= 'a' && c <= 'z') {
        return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'a'));
    }
    if (c >= 'A' && c <= 'Z') {
        return (SDL_Scancode)(SDL_SCANCODE_A + (c - 'A'));
    }
    if (c >= '1' && c <= '9') {
        return (SDL_Scancode)(SDL_SCANCODE_1 + (c - '1'));
    }
    switch (c) {
    case '0': case ')': return SDL_SCANCODE_0;
    case '!': return SDL_SCANCODE_1;
    case '@': return SDL_SCANCODE_2;
    case '#': return SDL_SCANCODE_3;
    case '$': return SDL_SCANCODE_4;
    case '%': return SDL_SCANCODE_5;
    case '^': return SDL_SCANCODE_6;
    case '&': return SDL_SCANCODE_7;
    case '*': return SDL_SCANCODE_8;
    case '(': return SDL_SCANCODE_9;
    case ' ': return SDL_SCANCODE_SPACE;
    case '\b': return SDL_SCANCODE_BACKSPACE;
    case '\t': return SDL_SCANCODE_TAB;
    case '\r': case '\n': return SDL_SCANCODE_RETURN;
    case 27: return SDL_SCANCODE_ESCAPE;
    case 127: return SDL_SCANCODE_DELETE;
    case '-': case '_': return SDL_SCANCODE_MINUS;
    case '=': case '+': return SDL_SCANCODE_EQUALS;
    case '[': case '{': return SDL_SCANCODE_LEFTBRACKET;
    case ']': case '}': return SDL_SCANCODE_RIGHTBRACKET;
    case '\\': case '|': return SDL_SCANCODE_BACKSLASH;
    case ';': case ':': return SDL_SCANCODE_SEMICOLON;
    case '\'': case '"': return SDL_SCANCODE_APOSTROPHE;
    case '`': case '~': return SDL_SCANCODE_GRAVE;
    case ',': case '<': return SDL_SCANCODE_COMMA;
    case '.': case '>': return SDL_SCANCODE_PERIOD;
    case '/': case '?': return SDL_SCANCODE_SLASH;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

// Keys that only arrive through the event queue, as PalmOS virtual characters
// (pumpkin_set_native_keys() must be on for Home and F5 to be reported this way).
static SDL_Scancode PUMPKIN_ScancodeForVirtualChar(UInt16 chr)
{
    switch (chr) {
    case vchrHard5: return SDL_SCANCODE_F5;
    case vchrHard6: return SDL_SCANCODE_F6;
    case vchrHard7: return SDL_SCANCODE_F7;
    case vchrHard8: return SDL_SCANCODE_F8;
    case vchrHard10: return SDL_SCANCODE_F10;
    case vchrNativeInsert: return SDL_SCANCODE_INSERT;
    case vchrNativeDelete: return SDL_SCANCODE_DELETE;
    case vchrNativeHome: return SDL_SCANCODE_HOME;
    case vchrNativePgUp: return SDL_SCANCODE_PAGEUP;
    case vchrNativePgDown: return SDL_SCANCODE_PAGEDOWN;
    default: return SDL_SCANCODE_UNKNOWN;
    }
}

static void PUMPKIN_SendKey(SDL_Scancode scancode, bool down)
{
    if (scancode != SDL_SCANCODE_UNKNOWN) {
        SDL_SendKeyboardKey(0, SDL_DEFAULT_KEYBOARD_ID, 0, scancode, down);
    }
}

static void PUMPKIN_SendText(SDL_VideoData *data, UInt16 chr)
{
    char text[8];

    if (!data->window || !SDL_TextInputActive(data->window)) {
        return;
    }
    // printable Latin-1 only; control characters are keys, not text
    if (chr < 32 || chr == 127 || chr > 255) {
        return;
    }
    *SDL_UCS4ToUTF8((Uint32)chr, text) = '\0';
    SDL_SendKeyboardText(text);
}

static void PUMPKIN_SendQuit(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    if (!data->quit_sent) {
        data->quit_sent = true;
        PUMPKIN_ReleaseInput(_this);
        SDL_SendQuit();
    }
}

// Pen position, polled. Only the task under the pointer sees real values;
// a jump to (0,0) from elsewhere is PumpkinOS telling us the pointer left.
static void PUMPKIN_PollPen(SDL_VideoDevice *_this, bool force)
{
    SDL_VideoData *data = _this->internal;
    int x = 0, y = 0;

    pumpkin_status(&x, &y, NULL, NULL, NULL, NULL);

    if (!force && x == 0 && y == 0 && data->pen_valid && (data->pen_x != 0 || data->pen_y != 0)) {
        return;
    }
    if (!data->pen_valid || x != data->pen_x || y != data->pen_y) {
        data->pen_x = x;
        data->pen_y = y;
        data->pen_valid = true;
        if (data->window) {
            SDL_SendMouseMotion(0, data->window, SDL_GLOBAL_MOUSE_ID, false, (float)x, (float)y);
        }
    }
}

static void PUMPKIN_PenButton(SDL_VideoDevice *_this, Uint8 button, bool down)
{
    SDL_VideoData *data = _this->internal;
    const Uint8 mask = SDL_BUTTON_MASK(button);

    if (down == ((data->pen_buttons & mask) != 0)) {
        return;
    }
    PUMPKIN_PollPen(_this, true);
    if (down) {
        data->pen_buttons |= mask;
    } else {
        data->pen_buttons &= ~mask;
    }
    if (data->window) {
        SDL_SendMouseButton(0, data->window, SDL_GLOBAL_MOUSE_ID, button, down);
    }
}

// Key state, polled: hard keys, modifiers and the ASCII key bit mask.
static void PUMPKIN_PollKeys(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    uint32_t key_mask = 0, mod_mask = 0;
    uint64_t ext[2] = { 0, 0 };
    bool now[SDL_SCANCODE_COUNT];
    Uint32 diff;
    int i;

    pumpkin_status(NULL, NULL, &key_mask, &mod_mask, NULL, ext);

    diff = key_mask ^ data->key_mask;
    if (diff) {
        for (i = 0; i < (int)SDL_arraysize(key_mask_table); i++) {
            if (diff & key_mask_table[i].bit) {
                PUMPKIN_SendKey(key_mask_table[i].scancode, (key_mask & key_mask_table[i].bit) != 0);
            }
        }
        data->key_mask = key_mask;
    }

    diff = mod_mask ^ data->mod_mask;
    if (diff) {
        for (i = 0; i < (int)SDL_arraysize(mod_mask_table); i++) {
            if (diff & mod_mask_table[i].bit) {
                PUMPKIN_SendKey(mod_mask_table[i].scancode, (mod_mask & mod_mask_table[i].bit) != 0);
            }
        }
        data->mod_mask = mod_mask;
    }

    SDL_zeroa(now);
    for (i = 0; i < 128; i++) {
        if (ext[i >> 6] & (((uint64_t)1) << (i & 63))) {
            const SDL_Scancode scancode = PUMPKIN_ScancodeForASCII(i);
            if (scancode != SDL_SCANCODE_UNKNOWN) {
                now[scancode] = true;
            }
        }
    }
    for (i = 0; i < SDL_SCANCODE_COUNT; i++) {
        if (now[i] != data->ext_down[i]) {
            data->ext_down[i] = now[i];
            PUMPKIN_SendKey((SDL_Scancode)i, now[i]);
        }
    }
}

static void PUMPKIN_HandleEvent(SDL_VideoDevice *_this, const EventType *event)
{
    SDL_VideoData *data = _this->internal;
    SDL_Scancode scancode;
    UInt16 chr, mods, code;

    switch (event->eType) {
    case appStopEvent:
        PUMPKIN_SendQuit(_this);
        break;

    case penDownEvent:
        PUMPKIN_PenButton(_this, SDL_BUTTON_LEFT, true);
        break;

    case penDownRightEvent:
        PUMPKIN_PenButton(_this, SDL_BUTTON_RIGHT, true);
        break;

    case penUpEvent:
        // PumpkinOS reports a single pen up for whichever button was down
        PUMPKIN_PenButton(_this, SDL_BUTTON_LEFT, false);
        PUMPKIN_PenButton(_this, SDL_BUTTON_RIGHT, false);
        break;

    case penMoveEvent:
        PUMPKIN_PollPen(_this, false);
        break;

    case keyDownEvent:
        chr = event->data.keyDown.chr;
        mods = event->data.keyDown.modifiers;
        code = event->data.keyDown.keyCode;
        if (chr == vchrNavChange && (mods & commandKeyMask)) {
            // five-way select is the F9 key of the host; left/right are polled
            if (code & navChangeSelect) {
                PUMPKIN_SendKey(SDL_SCANCODE_F9, (code & navBitSelect) != 0);
            }
        } else if ((scancode = PUMPKIN_ScancodeForVirtualChar(chr)) != SDL_SCANCODE_UNKNOWN) {
            PUMPKIN_SendKey(scancode, true);
        } else if (!(mods & commandKeyMask)) {
            PUMPKIN_SendText(data, chr);
        }
        break;

    case keyUpEvent:
        chr = event->data.keyUp.chr;
        if ((scancode = PUMPKIN_ScancodeForVirtualChar(chr)) != SDL_SCANCODE_UNKNOWN) {
            PUMPKIN_SendKey(scancode, false);
        }
        break;

    case wheelEvent:
        if (data->window) {
            SDL_SendMouseWheel(0, data->window, SDL_GLOBAL_MOUSE_ID, (float)event->screenX, (float)event->screenY, SDL_MOUSEWHEEL_NORMAL);
        }
        break;

    case winDisplayChangedEvent:
        PUMPKIN_CheckScreenSize(_this);
        break;

    default:
        break;
    }
}

void PUMPKIN_InitEvents(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    // report Home, F5 and friends as keys instead of letting PumpkinOS act on them
    pumpkin_set_native_keys(1);

    data->key_mask = 0;
    data->mod_mask = 0;
    SDL_zeroa(data->ext_down);
    data->pen_valid = false;
    data->pen_buttons = 0;
    data->quit_sent = false;
}

// Release every key and button this driver reported as held.
void PUMPKIN_ReleaseInput(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    int i;

    for (i = 0; i < (int)SDL_arraysize(key_mask_table); i++) {
        if (data->key_mask & key_mask_table[i].bit) {
            PUMPKIN_SendKey(key_mask_table[i].scancode, false);
        }
    }
    data->key_mask = 0;
    for (i = 0; i < (int)SDL_arraysize(mod_mask_table); i++) {
        if (data->mod_mask & mod_mask_table[i].bit) {
            PUMPKIN_SendKey(mod_mask_table[i].scancode, false);
        }
    }
    data->mod_mask = 0;
    for (i = 0; i < SDL_SCANCODE_COUNT; i++) {
        if (data->ext_down[i]) {
            data->ext_down[i] = false;
            PUMPKIN_SendKey((SDL_Scancode)i, false);
        }
    }
    if (data->pen_buttons && data->window) {
        for (i = 1; i <= 5; i++) {
            if (data->pen_buttons & SDL_BUTTON_MASK(i)) {
                SDL_SendMouseButton(0, data->window, SDL_GLOBAL_MOUSE_ID, (Uint8)i, false);
            }
        }
    }
    data->pen_buttons = 0;
}

void PUMPKIN_PumpEvents(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;
    EventType event;
    int n;

    // PumpkinOS asks the task to finish (window closed, shutdown, ...)
    if (thread_must_end()) {
        PUMPKIN_SendQuit(_this);
        return;
    }

    for (n = 0; n < PUMPKIN_MAX_EVENTS_PER_PUMP; n++) {
        SDL_zero(event);
        EvtGetEventUs(&event, 0);
        if (event.eType == nilEvent) {
            break;
        }
        // lets PumpkinOS turn the launcher key into appStopEvent
        if (SysHandleEvent(&event)) {
            continue;
        }
        PUMPKIN_HandleEvent(_this, &event);
    }

    PUMPKIN_CheckScreenSize(_this);

    if (!data->quit_sent) {
        PUMPKIN_PollKeys(_this);
        PUMPKIN_PollPen(_this, false);
    }
}

#endif // SDL_VIDEO_DRIVER_PUMPKINOS
