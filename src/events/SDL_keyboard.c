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

/* General keyboard handling code for SDL */

#include "SDL_events_c.h"
#include "SDL_keymap_c.h"
#include "../video/SDL_sysvideo.h"

/* #define DEBUG_KEYBOARD */

/* Global keyboard information */

#define KEYBOARD_HARDWARE        0x01
#define KEYBOARD_VIRTUAL         0x02
#define KEYBOARD_AUTORELEASE     0x04
#define KEYBOARD_IGNOREMODIFIERS 0x08

#define KEYBOARD_SOURCE_MASK (KEYBOARD_HARDWARE | KEYBOARD_AUTORELEASE)

#define KEYCODE_OPTION_FRENCH_NUMBERS   0x01
#define KEYCODE_OPTION_LATIN_LETTERS    0x02
#define DEFAULT_KEYCODE_OPTIONS (KEYCODE_OPTION_FRENCH_NUMBERS)

typedef struct SDL_KeyboardInstance
{
    SDL_KeyboardID instance_id;
    char *name;
} SDL_KeyboardInstance;

typedef struct SDL_Keyboard
{
    /* Data common to all keyboards */
    SDL_Window *focus;
    SDL_Keymod modstate;
    Uint8 keysource[SDL_NUM_SCANCODES];
    Uint8 keystate[SDL_NUM_SCANCODES];
    SDL_Keymap *keymap;
    SDL_bool french_numbers;
    SDL_bool non_latin_letters;
    Uint32 keycode_options;
    SDL_bool autorelease_pending;
    Uint64 hardware_timestamp;
    int next_reserved_scancode;
} SDL_Keyboard;

static SDL_Keyboard SDL_keyboard;
static int SDL_keyboard_count;
static SDL_KeyboardInstance *SDL_keyboards;

static void SDLCALL SDL_KeycodeOptionsChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    SDL_Keyboard *keyboard = (SDL_Keyboard *)userdata;

    if (hint && *hint) {
        keyboard->keycode_options = 0;
        if (!SDL_strstr(hint, "none")) {
            if (SDL_strstr(hint, "french_numbers")) {
                keyboard->keycode_options |= KEYCODE_OPTION_FRENCH_NUMBERS;
            }
            if (SDL_strstr(hint, "latin_letters")) {
                keyboard->keycode_options |= KEYCODE_OPTION_LATIN_LETTERS;
            }
        }
    } else {
        keyboard->keycode_options = DEFAULT_KEYCODE_OPTIONS;
    }
}

/* Public functions */
int SDL_InitKeyboard(void)
{
    SDL_AddHintCallback(SDL_HINT_KEYCODE_OPTIONS,
                        SDL_KeycodeOptionsChanged, &SDL_keyboard);
    return 0;
}

SDL_bool SDL_IsKeyboard(Uint16 vendor, Uint16 product, int num_keys)
{
    const int REAL_KEYBOARD_KEY_COUNT = 50;
    if (num_keys > 0 && num_keys < REAL_KEYBOARD_KEY_COUNT) {
        return SDL_FALSE;
    }

    /* Eventually we'll have a blacklist of devices that enumerate as keyboards but aren't really */
    return SDL_TRUE;
}

static int SDL_GetKeyboardIndex(SDL_KeyboardID keyboardID)
{
    for (int i = 0; i < SDL_keyboard_count; ++i) {
        if (keyboardID == SDL_keyboards[i].instance_id) {
            return i;
        }
    }
    return -1;
}

void SDL_AddKeyboard(SDL_KeyboardID keyboardID, const char *name, SDL_bool send_event)
{
    int keyboard_index = SDL_GetKeyboardIndex(keyboardID);
    if (keyboard_index >= 0) {
        /* We already know about this keyboard */
        return;
    }

    SDL_assert(keyboardID != 0);

    SDL_KeyboardInstance *keyboards = (SDL_KeyboardInstance *)SDL_realloc(SDL_keyboards, (SDL_keyboard_count + 1) * sizeof(*keyboards));
    if (!keyboards) {
        return;
    }
    SDL_KeyboardInstance *instance = &keyboards[SDL_keyboard_count];
    instance->instance_id = keyboardID;
    instance->name = SDL_strdup(name ? name : "");
    SDL_keyboards = keyboards;
    ++SDL_keyboard_count;

    if (send_event) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_KEYBOARD_ADDED;
        event.kdevice.which = keyboardID;
        SDL_PushEvent(&event);
    }
}

void SDL_RemoveKeyboard(SDL_KeyboardID keyboardID, SDL_bool send_event)
{
    int keyboard_index = SDL_GetKeyboardIndex(keyboardID);
    if (keyboard_index < 0) {
        /* We don't know about this keyboard */
        return;
    }

    SDL_FreeLater(SDL_keyboards[keyboard_index].name);

    if (keyboard_index != SDL_keyboard_count - 1) {
        SDL_memcpy(&SDL_keyboards[keyboard_index], &SDL_keyboards[keyboard_index + 1], (SDL_keyboard_count - keyboard_index - 1) * sizeof(SDL_keyboards[keyboard_index]));
    }
    --SDL_keyboard_count;

    if (send_event) {
        SDL_Event event;
        SDL_zero(event);
        event.type = SDL_EVENT_KEYBOARD_REMOVED;
        event.kdevice.which = keyboardID;
        SDL_PushEvent(&event);
    }
}

SDL_bool SDL_HasKeyboard(void)
{
    return (SDL_keyboard_count > 0);
}

SDL_KeyboardID *SDL_GetKeyboards(int *count)
{
    int i;
    SDL_KeyboardID *keyboards;

    keyboards = (SDL_JoystickID *)SDL_malloc((SDL_keyboard_count + 1) * sizeof(*keyboards));
    if (keyboards) {
        if (count) {
            *count = SDL_keyboard_count;
        }

        for (i = 0; i < SDL_keyboard_count; ++i) {
            keyboards[i] = SDL_keyboards[i].instance_id;
        }
        keyboards[i] = 0;
    } else {
        if (count) {
            *count = 0;
        }
    }

    return keyboards;
}

const char *SDL_GetKeyboardInstanceName(SDL_KeyboardID instance_id)
{
    int keyboard_index = SDL_GetKeyboardIndex(instance_id);
    if (keyboard_index < 0) {
        return NULL;
    }
    return SDL_keyboards[keyboard_index].name;
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
            SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, 0, scancode, SDL_RELEASED);
        }
    }
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
    for (int i = SDL_SCANCODE_1; i <= SDL_SCANCODE_0; ++i) {
        if (SDL_isdigit(SDL_GetKeymapKeycode(keymap, (SDL_Scancode)i, SDL_KMOD_NONE)) ||
            !SDL_isdigit(SDL_GetKeymapKeycode(keymap, (SDL_Scancode)i, SDL_KMOD_SHIFT))) {
            keyboard->french_numbers = SDL_FALSE;
            break;
        }
    }

    // Detect non-Latin keymap
    keyboard->non_latin_letters = SDL_TRUE;
    for (int i = SDL_SCANCODE_A; i <= SDL_SCANCODE_D; ++i) {
        if (SDL_GetKeymapKeycode(keymap, (SDL_Scancode)i, SDL_KMOD_NONE) <= 0xFF) {
            keyboard->non_latin_letters = SDL_FALSE;
            break;
        }
    }

    if (send_event) {
        SDL_SendKeymapChangedEvent();
    }
}

static SDL_Scancode GetNextReservedScancode(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    SDL_Scancode scancode;

    if (keyboard->next_reserved_scancode && keyboard->next_reserved_scancode < SDL_SCANCODE_RESERVED + 100) {
        scancode = (SDL_Scancode)keyboard->next_reserved_scancode;
    } else {
        scancode = SDL_SCANCODE_RESERVED;
    }
    keyboard->next_reserved_scancode = (int)scancode + 1;

    return scancode;
}

static void SetKeymapEntry(SDL_Scancode scancode, SDL_Keymod modstate, SDL_Keycode keycode)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    if (!keyboard->keymap) {
        keyboard->keymap = SDL_CreateKeymap();
    }

    SDL_SetKeymapEntry(keyboard->keymap, scancode, modstate, keycode);
}

SDL_Window *SDL_GetKeyboardFocus(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;

    return keyboard->focus;
}

int SDL_SetKeyboardFocus(SDL_Window *window)
{
    SDL_VideoDevice *video = SDL_GetVideoDevice();
    SDL_Keyboard *keyboard = &SDL_keyboard;

    if (window) {
        if (!SDL_ObjectValid(window, SDL_OBJECT_TYPE_WINDOW) || window->is_destroying) {
            return SDL_SetError("Invalid window");
        }
    }

    if (keyboard->focus && !window) {
        /* We won't get anymore keyboard messages, so reset keyboard state */
        SDL_ResetKeyboard();
    }

    /* See if the current window has lost focus */
    if (keyboard->focus && keyboard->focus != window) {
        SDL_SendWindowEvent(keyboard->focus, SDL_EVENT_WINDOW_FOCUS_LOST, 0, 0);

        /* Ensures IME compositions are committed */
        if (SDL_TextInputActive(keyboard->focus)) {
            if (video && video->StopTextInput) {
                video->StopTextInput(video, keyboard->focus);
            }
        }
    }

    keyboard->focus = window;

    if (keyboard->focus) {
        SDL_SendWindowEvent(keyboard->focus, SDL_EVENT_WINDOW_FOCUS_GAINED, 0, 0);

        if (SDL_TextInputActive(keyboard->focus)) {
            if (video && video->StartTextInput) {
                video->StartTextInput(video, keyboard->focus);
            }
        }
    }
    return 0;
}

static SDL_Keycode SDL_GetEventKeycode(SDL_Keyboard *keyboard, SDL_Scancode scancode, SDL_Keymod modstate)
{
    SDL_bool shifted = (modstate & SDL_KMOD_SHIFT) != 0;

    // We won't be applying any modifiers except numlock by default
    modstate &= SDL_KMOD_NUM;

    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        if (keyboard->non_latin_letters && (keyboard->keycode_options & KEYCODE_OPTION_LATIN_LETTERS)) {
            return SDL_GetDefaultKeyFromScancode(scancode, modstate);
        }
    }

    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_0) {
        if (keyboard->french_numbers && (keyboard->keycode_options & KEYCODE_OPTION_FRENCH_NUMBERS)) {
            // Invert the shift state to generate the correct keycode
            if (shifted) {
                modstate &= ~SDL_KMOD_SHIFT;
            } else {
                modstate |= SDL_KMOD_SHIFT;
            }
        }
    }

    return SDL_GetKeyFromScancode(scancode, modstate);
}

static int SDL_SendKeyboardKeyInternal(Uint64 timestamp, Uint32 flags, SDL_KeyboardID keyboardID, int rawcode, SDL_Scancode scancode, Uint8 state)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    int posted;
    SDL_Keycode keycode = SDLK_UNKNOWN;
    Uint32 type;
    Uint8 repeat = SDL_FALSE;
    const Uint8 source = flags & KEYBOARD_SOURCE_MASK;

#ifdef DEBUG_KEYBOARD
    printf("The '%s' key has been %s\n", SDL_GetScancodeName(scancode),
           state == SDL_PRESSED ? "pressed" : "released");
#endif

    /* Figure out what type of event this is */
    switch (state) {
    case SDL_PRESSED:
        type = SDL_EVENT_KEY_DOWN;
        break;
    case SDL_RELEASED:
        type = SDL_EVENT_KEY_UP;
        break;
    default:
        /* Invalid state -- bail */
        return 0;
    }

    if (scancode > SDL_SCANCODE_UNKNOWN && scancode < SDL_NUM_SCANCODES) {
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

        keycode = SDL_GetEventKeycode(keyboard, scancode, keyboard->modstate);

    } else if (rawcode == 0) {
        /* Nothing to do! */
        return 0;
    }

    if (source == KEYBOARD_HARDWARE) {
        keyboard->hardware_timestamp = SDL_GetTicks();
    } else if (source == KEYBOARD_AUTORELEASE) {
        keyboard->autorelease_pending = SDL_TRUE;
    }

    /* Update modifiers state if applicable */
    if (!(flags & KEYBOARD_IGNOREMODIFIERS) && !repeat) {
        SDL_Keymod modifier;

        switch (keycode) {
        case SDLK_LCTRL:
            modifier = SDL_KMOD_LCTRL;
            break;
        case SDLK_RCTRL:
            modifier = SDL_KMOD_RCTRL;
            break;
        case SDLK_LSHIFT:
            modifier = SDL_KMOD_LSHIFT;
            break;
        case SDLK_RSHIFT:
            modifier = SDL_KMOD_RSHIFT;
            break;
        case SDLK_LALT:
            modifier = SDL_KMOD_LALT;
            break;
        case SDLK_RALT:
            modifier = SDL_KMOD_RALT;
            break;
        case SDLK_LGUI:
            modifier = SDL_KMOD_LGUI;
            break;
        case SDLK_RGUI:
            modifier = SDL_KMOD_RGUI;
            break;
        case SDLK_MODE:
            modifier = SDL_KMOD_MODE;
            break;
        default:
            modifier = SDL_KMOD_NONE;
            break;
        }
        if (SDL_EVENT_KEY_DOWN == type) {
            switch (keycode) {
            case SDLK_NUMLOCKCLEAR:
                keyboard->modstate ^= SDL_KMOD_NUM;
                break;
            case SDLK_CAPSLOCK:
                keyboard->modstate ^= SDL_KMOD_CAPS;
                break;
            case SDLK_SCROLLLOCK:
                keyboard->modstate ^= SDL_KMOD_SCROLL;
                break;
            default:
                keyboard->modstate |= modifier;
                break;
            }
        } else {
            keyboard->modstate &= ~modifier;
        }
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(type)) {
        SDL_Event event;
        event.type = type;
        event.common.timestamp = timestamp;
        event.key.scancode = scancode;
        event.key.key = keycode;
        event.key.mod = keyboard->modstate;
        event.key.raw = (Uint16)rawcode;
        event.key.state = state;
        event.key.repeat = repeat;
        event.key.windowID = keyboard->focus ? keyboard->focus->id : 0;
        event.key.which = keyboardID;
        posted = (SDL_PushEvent(&event) > 0);
    }

    /* If the keyboard is grabbed and the grabbed window is in full-screen,
       minimize the window when we receive Alt+Tab, unless the application
       has explicitly opted out of this behavior. */
    if (keycode == SDLK_TAB &&
        state == SDL_PRESSED &&
        (keyboard->modstate & SDL_KMOD_ALT) &&
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

int SDL_SendKeyboardUnicodeKey(Uint64 timestamp, Uint32 ch)
{
    SDL_Keymod modstate = SDL_KMOD_NONE;
    SDL_Scancode scancode = SDL_GetScancodeFromKey(ch, &modstate);

    // Make sure we have this keycode in our keymap
    if (scancode == SDL_SCANCODE_UNKNOWN && ch < SDLK_SCANCODE_MASK) {
        scancode = GetNextReservedScancode();
        SetKeymapEntry(scancode, modstate, ch);
    }

    if (modstate & SDL_KMOD_SHIFT) {
        /* If the character uses shift, press shift down */
        SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_VIRTUAL, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_LSHIFT, SDL_PRESSED);
    }

    /* Send a keydown and keyup for the character */
    SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_VIRTUAL, SDL_GLOBAL_KEYBOARD_ID, 0, scancode, SDL_PRESSED);
    SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_VIRTUAL, SDL_GLOBAL_KEYBOARD_ID, 0, scancode, SDL_RELEASED);

    if (modstate & SDL_KMOD_SHIFT) {
        /* If the character uses shift, release shift */
        SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_VIRTUAL, SDL_GLOBAL_KEYBOARD_ID, 0, SDL_SCANCODE_LSHIFT, SDL_RELEASED);
    }
    return 0;
}

int SDL_SendKeyboardKey(Uint64 timestamp, SDL_KeyboardID keyboardID, int rawcode, SDL_Scancode scancode, Uint8 state)
{
    return SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_HARDWARE, keyboardID, rawcode, scancode, state);
}

int SDL_SendKeyboardKeyAndKeycode(Uint64 timestamp, SDL_KeyboardID keyboardID, int rawcode, SDL_Scancode scancode, SDL_Keycode keycode, Uint8 state)
{
    if (state == SDL_PRESSED) {
        // Make sure we have this keycode in our keymap
        SetKeymapEntry(scancode, SDL_GetModState(), keycode);
    }

    return SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_HARDWARE, keyboardID, rawcode, scancode, state);
}

int SDL_SendKeyboardKeyIgnoreModifiers(Uint64 timestamp, SDL_KeyboardID keyboardID, int rawcode, SDL_Scancode scancode, Uint8 state)
{
    return SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_HARDWARE | KEYBOARD_IGNOREMODIFIERS, keyboardID, rawcode, scancode, state);
}

int SDL_SendKeyboardKeyAutoRelease(Uint64 timestamp, SDL_Scancode scancode)
{
    return SDL_SendKeyboardKeyInternal(timestamp, KEYBOARD_AUTORELEASE, SDL_GLOBAL_KEYBOARD_ID, 0, scancode, SDL_PRESSED);
}

void SDL_ReleaseAutoReleaseKeys(void)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    SDL_Scancode scancode;

    if (keyboard->autorelease_pending) {
        for (scancode = SDL_SCANCODE_UNKNOWN; scancode < SDL_NUM_SCANCODES; ++scancode) {
            if (keyboard->keysource[scancode] == KEYBOARD_AUTORELEASE) {
                SDL_SendKeyboardKeyInternal(0, KEYBOARD_AUTORELEASE, SDL_GLOBAL_KEYBOARD_ID, 0, scancode, SDL_RELEASED);
            }
        }
        keyboard->autorelease_pending = SDL_FALSE;
    }

    if (keyboard->hardware_timestamp) {
        /* Keep hardware keyboard "active" for 250 ms */
        if (SDL_GetTicks() >= keyboard->hardware_timestamp + 250) {
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

    if (!SDL_TextInputActive(keyboard->focus)) {
        return 0;
    }

    if (!text || !*text) {
        return 0;
    }

    /* Don't post text events for unprintable characters */
    if (SDL_iscntrl((unsigned char)*text)) {
        return 0;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_TEXT_INPUT)) {
        SDL_Event event;
        event.type = SDL_EVENT_TEXT_INPUT;
        event.common.timestamp = 0;
        event.text.windowID = keyboard->focus ? keyboard->focus->id : 0;
        event.text.text = SDL_AllocateEventString(text);
        if (!event.text.text) {
            return 0;
        }
        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

int SDL_SendEditingText(const char *text, int start, int length)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    int posted;

    if (!SDL_TextInputActive(keyboard->focus)) {
        return 0;
    }

    if (!text) {
        return 0;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_TEXT_EDITING)) {
        SDL_Event event;

        event.type = SDL_EVENT_TEXT_EDITING;
        event.common.timestamp = 0;
        event.edit.windowID = keyboard->focus ? keyboard->focus->id : 0;
        event.edit.start = start;
        event.edit.length = length;
        event.edit.text = SDL_AllocateEventString(text);
        if (!event.edit.text) {
            return 0;
        }
        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

int SDL_SendEditingTextCandidates(char **candidates, int num_candidates, int selected_candidate, SDL_bool horizontal)
{
    SDL_Keyboard *keyboard = &SDL_keyboard;
    int posted;

    if (!SDL_TextInputActive(keyboard->focus)) {
        return 0;
    }

    /* Post the event, if desired */
    posted = 0;
    if (SDL_EventEnabled(SDL_EVENT_TEXT_EDITING_CANDIDATES)) {
        SDL_Event event;

        event.type = SDL_EVENT_TEXT_EDITING_CANDIDATES;
        event.common.timestamp = 0;
        event.edit.windowID = keyboard->focus ? keyboard->focus->id : 0;
        if (num_candidates > 0) {
            const char **event_candidates = (const char **)SDL_AllocateEventMemory((num_candidates + 1) * sizeof(*event_candidates));
            if (!event_candidates) {
                return 0;
            }
            for (int i = 0; i < num_candidates; ++i) {
                event_candidates[i] = SDL_AllocateEventString(candidates[i]);
            }
            event_candidates[num_candidates] = NULL;
            event.edit_candidates.candidates = event_candidates;
            event.edit_candidates.num_candidates = num_candidates;
            event.edit_candidates.selected_candidate = selected_candidate;
            event.edit_candidates.horizontal = horizontal;
        } else {
            event.edit_candidates.candidates = NULL;
            event.edit_candidates.num_candidates = 0;
            event.edit_candidates.selected_candidate = -1;
            event.edit_candidates.horizontal = SDL_FALSE;
        }
        posted = (SDL_PushEvent(&event) > 0);
    }
    return posted;
}

void SDL_QuitKeyboard(void)
{
    for (int i = SDL_keyboard_count; i--;) {
        SDL_RemoveKeyboard(SDL_keyboards[i].instance_id, SDL_FALSE);
    }
    SDL_free(SDL_keyboards);
    SDL_keyboards = NULL;

    if (SDL_keyboard.keymap) {
        SDL_DestroyKeymap(SDL_keyboard.keymap);
        SDL_keyboard.keymap = NULL;
    }

    SDL_DelHintCallback(SDL_HINT_KEYCODE_OPTIONS,
                        SDL_KeycodeOptionsChanged, &SDL_keyboard);
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

    return keyboard->modstate;
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

SDL_Keycode SDL_GetKeyFromScancode(SDL_Scancode scancode, SDL_Keymod modstate)
{
    return SDL_GetKeymapKeycode(SDL_keyboard.keymap, scancode, modstate);
}

SDL_Scancode SDL_GetScancodeFromKey(SDL_Keycode key, SDL_Keymod *modstate)
{
    return SDL_GetKeymapScancode(SDL_keyboard.keymap, key, modstate);
}

