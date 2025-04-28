/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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
#include "../SDL_internal.h"

/* General keyboard handling code for SDL */

#include "SDL_hints.h"
#include "SDL_timer.h"
#include "SDL_events.h"
#include "SDL_events_c.h"
#include "../video/SDL_sysvideo.h"
#include "scancodes_ascii.h"
#include "SDL_keymap_c.h"

/* #define DEBUG_KEYBOARD */

/* Global keyboard information */

#define KEYBOARD_HARDWARE    0x01
#define KEYBOARD_VIRTUAL     0x02
#define KEYBOARD_AUTORELEASE 0x04

typedef struct SDL_Keyboard SDL_Keyboard;

struct SDL_Keyboard
{
    /* Data common to all keyboards */
    SDL_Window *focus;
    Uint16 modstate;
    Uint8 keysource[SDL_NUM_SCANCODES];
    Uint8 keystate[SDL_NUM_SCANCODES];
    SDL_Keymap *keymap;
    SDL_bool french_numbers;
    SDL_bool latin_letters;
    SDL_bool thai_keyboard;
    SDL_bool autorelease_pending;
    Uint32 hardware_timestamp;
};

static SDL_Keyboard SDL_keyboard;

/* Taken from SDL_iconv() */
char *SDL_UCS4ToUTF8(Uint32 ch, char *dst)
{
    Uint8 *p = (Uint8 *)dst;
    if (ch <= 0x7F) {
        *p = (Uint8)ch;
        ++dst;
    } else if (ch <= 0x7FF) {
        p[0] = 0xC0 | (Uint8)((ch >> 6) & 0x1F);
        p[1] = 0x80 | (Uint8)(ch & 0x3F);
        dst += 2;
    } else if (ch <= 0xFFFF) {
        p[0] = 0xE0 | (Uint8)((ch >> 12) & 0x0F);
        p[1] = 0x80 | (Uint8)((ch >> 6) & 0x3F);
        p[2] = 0x80 | (Uint8)(ch & 0x3F);
        dst += 3;
    } else {
        p[0] = 0xF0 | (Uint8)((ch >> 18) & 0x07);
        p[1] = 0x80 | (Uint8)((ch >> 12) & 0x3F);
        p[2] = 0x80 | (Uint8)((ch >> 6) & 0x3F);
        p[3] = 0x80 | (Uint8)(ch & 0x3F);
        dst += 4;
    }
    return dst;
}

/* Public functions */
int SDL_KeyboardInit(void)
{
    return 0;
}

void SDL_ResetKeyboard(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    SDL_Scancode scancode;

#ifdef DEBUG_KEYBOARD
    printf("Resetting keyboard\n");
#endif
    for (scancode = (SDL_Scancode)0; scancode < SDL_NUM_SCANCODES; ++scancode) {
        if (keyboard->keystate[scancode] == SDL_PRESSED) {
            SDL_SendKeyboardKey(SDL_RELEASED, scancode);
        }
    }
}

SDL_Keymap *SDL_GetCurrentKeymap(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    if (keyboard->thai_keyboard) {
        // Thai keyboards are QWERTY plus Thai characters, use the default QWERTY keymap
        return NULL;
    }

    if (!keyboard->latin_letters) {
        // We'll use the default QWERTY keymap
        return NULL;
    }

    return keyboard->keymap;
}

void SDL_SetKeymap(SDL_Keymap *keymap, SDL_bool send_event)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    if (keyboard->keymap) {
        SDL_DestroyKeymap(keyboard->keymap);
    }

    keyboard->keymap = keymap;

    // Detect French number row (all symbols)
    keyboard->french_numbers = SDL_TRUE;
    for (int i =    1; i <= SDL_SCANCODE_0; ++i) {
        if (SDL_isdigit(SDL_GetKeymapKeycode(keymap, (SDL_Scancode)i))) {
            keyboard->french_numbers = SDL_FALSE;
            break;
        }
    }

    // Detect non-Latin keymap
    keyboard->thai_keyboard = SDL_FALSE;
    keyboard->latin_letters = SDL_FALSE;
    for (int i = SDL_SCANCODE_A; i <= SDL_SCANCODE_D; ++i) {
        SDL_Keycode key = SDL_GetKeymapKeycode(keymap, (SDL_Scancode)i);
        if (key <= 0xFF) {
            keyboard->latin_letters = SDL_TRUE;
            break;
        }

        if (key >= 0x0E00 && key <= 0x0E7F) {
            keyboard->thai_keyboard = SDL_TRUE;
            break;
        }
    }

    if (send_event) {
        SDL_SendKeymapChangedEvent();
    }
}

SDL_Window *SDL_GetKeyboardFocus(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    return keyboard->focus;
}

void SDL_SetKeyboardFocus(SDL_Window *window)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    if (keyboard->focus && window == NULL) {
        /* We won't get anymore keyboard messages, so reset keyboard state */
        SDL_ResetKeyboard();
    }

    /* See if the current window has lost focus */
    if (keyboard->focus && keyboard->focus != window) {

        /* new window shouldn't think it has mouse captured. */
        SDL_assert(window == NULL || !(window->flags & SDL_WINDOW_MOUSE_CAPTURE));

        /* old window must lose an existing mouse capture. */
        if (keyboard->focus->flags & SDL_WINDOW_MOUSE_CAPTURE) {
            SDL_CaptureMouse(SDL_FALSE); /* drop the capture. */
            SDL_UpdateMouseCapture(SDL_TRUE);
            SDL_assert(!(keyboard->focus->flags & SDL_WINDOW_MOUSE_CAPTURE));
        }

        SDL_SendWindowEvent(keyboard->focus, SDL_WINDOWEVENT_FOCUS_LOST,
                            0, 0);

        /* Ensures IME compositions are committed */
        if (SDL_EventState(SDL_TEXTINPUT, SDL_QUERY)) {
            SDL_VideoDevice *video = SDL_GetVideoDevice();
            if (video && video->StopTextInput) {
                video->StopTextInput(video);
            }
        }
    }

    keyboard->focus = window;

    if (keyboard->focus) {
        SDL_SendWindowEvent(keyboard->focus, SDL_WINDOWEVENT_FOCUS_GAINED,
                            0, 0);

        if (SDL_EventState(SDL_TEXTINPUT, SDL_QUERY)) {
            SDL_VideoDevice *video = SDL_GetVideoDevice();
            if (video && video->StartTextInput) {
                video->StartTextInput(video);
            }
        }
    }
}

static int SDL_SendKeyboardKeyInternal(Uint8 source, Uint8 state, SDL_Scancode scancode, SDL_Keycode keycode)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    int posted;
    SDL_Keymod modifier;
    Uint32 type;
    Uint8 repeat = SDL_FALSE;

    if (scancode == SDL_SCANCODE_UNKNOWN || scancode >= SDL_NUM_SCANCODES) {
        return 0;
    }

#ifdef DEBUG_KEYBOARD
    printf("The '%s' key has been %s\n", SDL_GetScancodeName(scancode),
           state == SDL_PRESSED ? "pressed" : "released");
#endif

    /* Figure out what type of event this is */
    switch (state) {
    case SDL_PRESSED:
        type = SDL_KEYDOWN;
        break;
    case SDL_RELEASED:
        type = SDL_KEYUP;
        break;
    default:
        /* Invalid state -- bail */
        return 0;
    }

    /* Drop events that don't change state */
    if (state) {
        if (keyboard->keystate[scancode]) {
            if (!(keyboard->keysource[scancode] & source)) {
                keyboard->keysource[scancode] |= source;
                return 0;
            }
            repeat = SDL_TRUE;
        }
        keyboard->keysource[scancode] |= source;
    } else {
        if (!keyboard->keystate[scancode]) {
            return 0;
        }
        keyboard->keysource[scancode] = 0;
    }

    /* Update internal keyboard state */
    keyboard->keystate[scancode] = state;
    
    keycode = SDL_GetKeyFromScancode(scancode, SDL_TRUE);

    if (source == KEYBOARD_HARDWARE) {
        keyboard->hardware_timestamp = SDL_GetTicks();
    } else if (source == KEYBOARD_AUTORELEASE) {
        keyboard->autorelease_pending = SDL_TRUE;
    }

    /* Update modifiers state if applicable */
    switch (keycode) {
    case SDLK_LCTRL:
        modifier = KMOD_LCTRL;
        break;
    case SDLK_RCTRL:
        modifier = KMOD_RCTRL;
        break;
    case SDLK_LSHIFT:
        modifier = KMOD_LSHIFT;
        break;
    case SDLK_RSHIFT:
        modifier = KMOD_RSHIFT;
        break;
    case SDLK_LALT:
        modifier = KMOD_LALT;
        break;
    case SDLK_RALT:
        modifier = KMOD_RALT;
        break;
    case SDLK_LGUI:
        modifier = KMOD_LGUI;
        break;
    case SDLK_RGUI:
        modifier = KMOD_RGUI;
        break;
    case SDLK_MODE:
        modifier = KMOD_MODE;
        break;
    default:
        modifier = KMOD_NONE;
        break;
    }
    if (SDL_KEYDOWN == type) {
        switch (keycode) {
        case SDLK_NUMLOCKCLEAR:
            keyboard->modstate ^= KMOD_NUM;
            break;
        case SDLK_CAPSLOCK:
            keyboard->modstate ^= KMOD_CAPS;
            break;
        case SDLK_SCROLLLOCK:
            keyboard->modstate ^= KMOD_SCROLL;
            break;
        default:
            keyboard->modstate |= modifier;
            break;
        }
    } else {
        keyboard->modstate &= ~modifier;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_GetEventState(type) == SDL_ENABLE) {
        SDL_Event event;
        event.key.type = type;
        event.key.state = state;
        event.key.repeat = repeat;
        event.key.keysym.scancode = scancode;
        event.key.keysym.sym = keycode;
        event.key.keysym.mod = keyboard->modstate;
        event.key.windowID = keyboard->focus ? keyboard->focus->id : 0;
        posted = (SDL_PushEvent(&event) > 0);
    }

    /* If the keyboard is grabbed and the grabbed window is in full-screen,
       minimize the window when we receive Alt+Tab, unless the application
       has explicitly opted out of this behavior. */
    if (keycode == SDLK_TAB &&
        state == SDL_PRESSED &&
        (keyboard->modstate & KMOD_ALT) &&
        keyboard->focus &&
        (keyboard->focus->flags & SDL_WINDOW_KEYBOARD_GRABBED) &&
        (keyboard->focus->flags & SDL_WINDOW_FULLSCREEN) &&
        SDL_GetHintBoolean(SDL_HINT_ALLOW_ALT_TAB_WHILE_GRABBED, SDL_TRUE)) {
        /* We will temporarily forfeit our grab by minimizing our window,
           allowing the user to escape the application */
        SDL_MinimizeWindow(keyboard->focus);
    }

    return posted;
}

int SDL_SendKeyboardUnicodeKey(Uint32 ch)
{
    SDL_Scancode code = SDL_SCANCODE_UNKNOWN;
    uint16_t mod = 0;

    if (ch < SDL_arraysize(SDL_ASCIIKeyInfoTable)) {
        code = SDL_ASCIIKeyInfoTable[ch].code;
        mod = SDL_ASCIIKeyInfoTable[ch].mod;
    }

    if (mod & KMOD_SHIFT) {
        /* If the character uses shift, press shift down */
        SDL_SendKeyboardKeyInternal(KEYBOARD_VIRTUAL, SDL_PRESSED, SDL_SCANCODE_LSHIFT, SDLK_UNKNOWN);
    }

    /* Send a keydown and keyup for the character */
    SDL_SendKeyboardKeyInternal(KEYBOARD_VIRTUAL, SDL_PRESSED, code, SDLK_UNKNOWN);
    SDL_SendKeyboardKeyInternal(KEYBOARD_VIRTUAL, SDL_RELEASED, code, SDLK_UNKNOWN);

    if (mod & KMOD_SHIFT) {
        /* If the character uses shift, release shift */
        SDL_SendKeyboardKeyInternal(KEYBOARD_VIRTUAL, SDL_RELEASED, SDL_SCANCODE_LSHIFT, SDLK_UNKNOWN);
    }
    return 0;
}

int SDL_SendVirtualKeyboardKey(Uint8 state, SDL_Scancode scancode)
{
    return SDL_SendKeyboardKeyInternal(KEYBOARD_VIRTUAL, state, scancode, SDLK_UNKNOWN);
}

int SDL_SendKeyboardKey(Uint8 state, SDL_Scancode scancode)
{
    return SDL_SendKeyboardKeyInternal(KEYBOARD_HARDWARE, state, scancode, SDLK_UNKNOWN);
}

int SDL_SendKeyboardKeyAndKeycode(Uint8 state, SDL_Scancode scancode, SDL_Keycode keycode)
{
    return SDL_SendKeyboardKeyInternal(KEYBOARD_HARDWARE, state, scancode, keycode);
}

int SDL_SendKeyboardKeyAutoRelease(SDL_Scancode scancode)
{
    return SDL_SendKeyboardKeyInternal(KEYBOARD_AUTORELEASE, SDL_PRESSED, scancode, SDLK_UNKNOWN);
}

void SDL_ReleaseAutoReleaseKeys(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    SDL_Scancode scancode;

    if (keyboard->autorelease_pending) {
        for (scancode = SDL_SCANCODE_UNKNOWN; scancode < SDL_NUM_SCANCODES; ++scancode) {
            if (keyboard->keysource[scancode] == KEYBOARD_AUTORELEASE) {
                SDL_SendKeyboardKeyInternal(KEYBOARD_AUTORELEASE, SDL_RELEASED, scancode, SDLK_UNKNOWN);
            }
        }
        keyboard->autorelease_pending = SDL_FALSE;
    }

    if (keyboard->hardware_timestamp) {
        /* Keep hardware keyboard "active" for 250 ms */
        if (SDL_TICKS_PASSED(SDL_GetTicks(), keyboard->hardware_timestamp + 250)) {
            keyboard->hardware_timestamp = 0;
        }
    }
}

SDL_bool SDL_HardwareKeyboardKeyPressed(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    SDL_Scancode scancode;

    for (scancode = SDL_SCANCODE_UNKNOWN; scancode < SDL_NUM_SCANCODES; ++scancode) {
        if (keyboard->keysource[scancode] & KEYBOARD_HARDWARE) {
            return SDL_TRUE;
        }
    }

    return keyboard->hardware_timestamp ? SDL_TRUE : SDL_FALSE;
}

int SDL_SendKeyboardText(const char *text)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    int posted;

    /* Don't post text events for unprintable characters */
    if ((unsigned char)*text < ' ' || *text == 127) {
        return 0;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_GetEventState(SDL_TEXTINPUT) == SDL_ENABLE) {
        SDL_Event event;
        size_t pos = 0, advance, length = SDL_strlen(text);

        event.text.type = SDL_TEXTINPUT;
        event.text.windowID = keyboard->focus ? keyboard->focus->id : 0;
        while (pos < length) {
            advance = SDL_utf8strlcpy(event.text.text, text + pos, SDL_arraysize(event.text.text));
            if (!advance) {
                break;
            }
            pos += advance;
            posted |= (SDL_PushEvent(&event) > 0);
        }
    }
    return posted;
}

int SDL_SendEditingText(const char *text, int start, int length)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    int posted;

    /* Post the event, if desired */
    posted = 0;
    if (SDL_GetEventState(SDL_TEXTEDITING) == SDL_ENABLE) {
        SDL_Event event;

        if (SDL_GetHintBoolean(SDL_HINT_IME_SUPPORT_EXTENDED_TEXT, SDL_FALSE) &&
            SDL_strlen(text) >= SDL_arraysize(event.text.text)) {
            event.editExt.type = SDL_TEXTEDITING_EXT;
            event.editExt.windowID = keyboard->focus ? keyboard->focus->id : 0;
            event.editExt.text = text ? SDL_strdup(text) : NULL;
            event.editExt.start = start;
            event.editExt.length = length;
        } else {
            event.edit.type = SDL_TEXTEDITING;
            event.edit.windowID = keyboard->focus ? keyboard->focus->id : 0;
            event.edit.start = start;
            event.edit.length = length;
            SDL_utf8strlcpy(event.edit.text, text, SDL_arraysize(event.edit.text));
        }

        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

void SDL_KeyboardQuit(void)
{
}

const Uint8 *SDL_GetKeyboardState(int *numkeys)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    if (numkeys != (int *)0) {
        *numkeys = SDL_NUM_SCANCODES;
    }
    return keyboard->keystate;
}

SDL_Keymod SDL_GetModState(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    return (SDL_Keymod)keyboard->modstate;
}

void SDL_SetModState(SDL_Keymod modstate)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    keyboard->modstate = modstate;
}

/* Note that SDL_ToggleModState() is not a public API. SDL_SetModState() is. */
void SDL_ToggleModState(const SDL_Keymod modstate, const SDL_bool toggle)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    if (toggle) {
        keyboard->modstate |= modstate;
    } else {
        keyboard->modstate &= ~modstate;
    }
}

SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode, SDL_bool key_event)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    if (key_event) {
        SDL_Keymap *keymap = SDL_GetCurrentKeymap();
        return SDL_GetKeymapKeycode(keymap, scancode);;
    }

    return SDL_GetKeymapKeycode(keyboard->keymap, scancode);
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    return SDL_GetKeymapScancode(keyboard->keymap, key);
}

/* vi: set ts=4 sw=4 expandtab: */
