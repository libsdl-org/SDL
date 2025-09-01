/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>
  Copyright (C) 2020 Jay Petacat <jay@jayschwa.net>

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

#include "../../SDL_internal.h"

#ifdef __MSDOS__

#include <dpmi.h>
#include <pc.h>

#include "../../events/SDL_events_c.h"

#define KEYBOARD_INTERRUPT 0x09

#define PS2_DATA   0x60
#define PS2_STATUS 0x64

static const SDL_Scancode bios_to_sdl_scancode[128] = {
    0, /* 0 0x00 */
    SDL_SCANCODE_ESCAPE, /* 1 0x01 */
    SDL_SCANCODE_1, /* 2 0x02 */
    SDL_SCANCODE_2, /* 3 0x03 */
    SDL_SCANCODE_3, /* 4 0x04 */
    SDL_SCANCODE_4, /* 5 0x05 */
    SDL_SCANCODE_5, /* 6 0x06 */
    SDL_SCANCODE_6, /* 7 0x07 */
    SDL_SCANCODE_7, /* 8 0x08 */
    SDL_SCANCODE_8, /* 9 0x09 */
    SDL_SCANCODE_9, /* 10 0x0a */
    SDL_SCANCODE_0, /* 11 0x0b */
    SDL_SCANCODE_MINUS, /* 12 0x0c */
    SDL_SCANCODE_EQUALS, /* 13 0x0d */
    SDL_SCANCODE_BACKSPACE, /* 14 0x0e */
    SDL_SCANCODE_TAB, /* 15 0x0f */
    SDL_SCANCODE_Q, /* 16 0x10 */
    SDL_SCANCODE_W, /* 17 0x11 */
    SDL_SCANCODE_E, /* 18 0x12 */
    SDL_SCANCODE_R, /* 19 0x13 */
    SDL_SCANCODE_T, /* 20 0x14 */
    SDL_SCANCODE_Y, /* 21 0x15 */
    SDL_SCANCODE_U, /* 22 0x16 */
    SDL_SCANCODE_I, /* 23 0x17 */
    SDL_SCANCODE_O, /* 24 0x18 */
    SDL_SCANCODE_P, /* 25 0x19 */
    SDL_SCANCODE_LEFTBRACKET, /* 26 0x1a */
    SDL_SCANCODE_RIGHTBRACKET, /* 27 0x1b */
    SDL_SCANCODE_RETURN, /* 28 0x1c */
    SDL_SCANCODE_LCTRL, /* 29 0x1d */
    SDL_SCANCODE_A, /* 30 0x1e */
    SDL_SCANCODE_S, /* 31 0x1f */
    SDL_SCANCODE_D, /* 32 0x20 */
    SDL_SCANCODE_F, /* 33 0x21 */
    SDL_SCANCODE_G, /* 34 0x22 */
    SDL_SCANCODE_H, /* 35 0x23 */
    SDL_SCANCODE_J, /* 36 0x24 */
    SDL_SCANCODE_K, /* 37 0x25 */
    SDL_SCANCODE_L, /* 38 0x26 */
    SDL_SCANCODE_SEMICOLON, /* 39 0x27 */
    SDL_SCANCODE_APOSTROPHE, /* 40 0x28 */
    SDL_SCANCODE_GRAVE, /* 41 0x29 */
    SDL_SCANCODE_LSHIFT, /* 42 0x2a */
    SDL_SCANCODE_BACKSLASH, /* 43 0x2b */
    SDL_SCANCODE_Z, /* 44 0x2c */
    SDL_SCANCODE_X, /* 45 0x2d */
    SDL_SCANCODE_C, /* 46 0x2e */
    SDL_SCANCODE_V, /* 47 0x2f */
    SDL_SCANCODE_B, /* 48 0x30 */
    SDL_SCANCODE_N, /* 49 0x31 */
    SDL_SCANCODE_M, /* 50 0x32 */
    SDL_SCANCODE_COMMA, /* 51 0x33 */
    SDL_SCANCODE_PERIOD, /* 52 0x34 */
    SDL_SCANCODE_SLASH, /* 53 0x35 */
    SDL_SCANCODE_RSHIFT, /* 54 0x36 */
    SDL_SCANCODE_KP_MULTIPLY, /* 55 0x37 */
    SDL_SCANCODE_LALT, /* 56 0x38 */
    SDL_SCANCODE_SPACE, /* 57 0x39 */
    SDL_SCANCODE_CAPSLOCK, /* 58 0x3a */
    SDL_SCANCODE_F1, /* 59 0x3b */
    SDL_SCANCODE_F2, /* 60 0x3c */
    SDL_SCANCODE_F3, /* 61 0x3d */
    SDL_SCANCODE_F4, /* 62 0x3e */
    SDL_SCANCODE_F5, /* 63 0x3f */
    SDL_SCANCODE_F6, /* 64 0x40 */
    SDL_SCANCODE_F7, /* 65 0x41 */
    SDL_SCANCODE_F8, /* 66 0x42 */
    SDL_SCANCODE_F9, /* 67 0x43 */
    SDL_SCANCODE_F10, /* 68 0x44 */
    SDL_SCANCODE_NUMLOCKCLEAR, /* 69 0x45 */
    SDL_SCANCODE_SCROLLLOCK, /* 70 0x46 */
    SDL_SCANCODE_KP_7, /* 71 0x47 */
    SDL_SCANCODE_KP_8, /* 72 0x48 */
    SDL_SCANCODE_KP_9, /* 73 0x49 */
    SDL_SCANCODE_KP_MINUS, /* 74 0x4a */
    SDL_SCANCODE_KP_4, /* 75 0x4b */
    SDL_SCANCODE_KP_5, /* 76 0x4c */
    SDL_SCANCODE_KP_6, /* 77 0x4d */
    SDL_SCANCODE_KP_PLUS, /* 78 0x4e */
    SDL_SCANCODE_KP_1, /* 79 0x4f */
    SDL_SCANCODE_KP_2, /* 80 0x50 */
    SDL_SCANCODE_KP_3, /* 81 0x51 */
    SDL_SCANCODE_KP_0, /* 82 0x52 */
    SDL_SCANCODE_KP_PERIOD, /* 83 0x53 */
    SDL_SCANCODE_SYSREQ, /* 84 0x54 */
    0, /* 85 0x55 */
    SDL_SCANCODE_LGUI, /* 86 0x56 */
    SDL_SCANCODE_F11, /* 87 0x57 */
    SDL_SCANCODE_F12, /* 88 0x58 */
};

/* Keys that are first indicated by 0xE0 */
static const SDL_Scancode extended_key_to_sdl_scancode[128] = {
    0, /* 0 0x00 */
    0, /* 1 0x01 */
    0, /* 2 0x02 */
    0, /* 3 0x03 */
    0, /* 4 0x04 */
    0, /* 5 0x05 */
    0, /* 6 0x06 */
    0, /* 7 0x07 */
    0, /* 8 0x08 */
    0, /* 9 0x09 */
    0, /* 10 0x0a */
    0, /* 11 0x0b */
    0, /* 12 0x0c */
    0, /* 13 0x0d */
    0, /* 14 0x0e */
    0, /* 15 0x0f */
    0, /* 16 0x10 */
    0, /* 17 0x11 */
    0, /* 18 0x12 */
    0, /* 19 0x13 */
    0, /* 20 0x14 */
    0, /* 21 0x15 */
    0, /* 22 0x16 */
    0, /* 23 0x17 */
    0, /* 24 0x18 */
    0, /* 25 0x19 */
    0, /* 26 0x1a */
    0, /* 27 0x1b */
    SDL_SCANCODE_KP_ENTER, /* 28 0x1c */
    SDL_SCANCODE_RALT, /* 29 0x1d */
    0, /* 30 0x1e */
    0, /* 31 0x1f */
    0, /* 32 0x20 */
    0, /* 33 0x21 */
    0, /* 34 0x22 */
    0, /* 35 0x23 */
    0, /* 36 0x24 */
    0, /* 37 0x25 */
    0, /* 38 0x26 */
    0, /* 39 0x27 */
    0, /* 40 0x28 */
    0, /* 41 0x29 */
    SDL_SCANCODE_LSHIFT, /* 42 0x2a */
    0, /* 43 0x2b */
    0, /* 44 0x2c */
    0, /* 45 0x2d */
    0, /* 46 0x2e */
    0, /* 47 0x2f */
    0, /* 48 0x30 */
    0, /* 49 0x31 */
    0, /* 50 0x32 */
    0, /* 51 0x33 */
    0, /* 52 0x34 */
    SDL_SCANCODE_KP_DIVIDE, /* 53 0x35 */
    SDL_SCANCODE_RSHIFT, /* 54 0x36 */
    SDL_SCANCODE_PRINTSCREEN, /* 55 0x37 */
    SDL_SCANCODE_RALT, /* 56 0x38 */
    0, /* 57 0x39 */
    0, /* 58 0x3a */
    0, /* 59 0x3b */
    0, /* 60 0x3c */
    0, /* 61 0x3d */
    0, /* 62 0x3e */
    0, /* 63 0x3f */
    0, /* 64 0x40 */
    0, /* 65 0x41 */
    0, /* 66 0x42 */
    0, /* 67 0x43 */
    0, /* 68 0x44 */
    0, /* 69 0x45 */
    SDL_SCANCODE_PAUSE, /* 70 0x46 */
    SDL_SCANCODE_HOME, /* 71 0x47 */
    SDL_SCANCODE_UP, /* 72 0x48 */
    SDL_SCANCODE_PAGEUP, /* 73 0x49 */
    0, /* 74 0x4a */
    SDL_SCANCODE_LEFT, /* 75 0x4b */
    0, /* 76 0x4c */
    SDL_SCANCODE_RIGHT, /* 77 0x4d */
    0, /* 78 0x4e */
    SDL_SCANCODE_END, /* 79 0x4f */
    SDL_SCANCODE_DOWN, /* 80 0x50 */
    SDL_SCANCODE_PAGEDOWN, /* 81 0x51 */
    SDL_SCANCODE_INSERT, /* 82 0x52 */
    SDL_SCANCODE_DELETE, /* 83 0x53 */
    0, /* 84 0x54 */
    0, /* 85 0x55 */
    0, /* 86 0x56 */
    0, /* 87 0x57 */
    0, /* 88 0x58 */
    0, /* 89 0x59 */
    0, /* 90 0x5a */
    0, /* 91 0x5b */
    0, /* 92 0x5c */
    0, /* 93 0x5d */
    0, /* 94 0x5e */
    0, /* 95 0x5f */
    0, /* 96 0x60 */
    0, /* 97 0x61 */
    0, /* 98 0x62 */
    0, /* 99 0x63 */
    0, /* 100 0x64 */
    0, /* 101 0x65 */
    0, /* 102 0x66 */
    0, /* 103 0x67 */
    0, /* 104 0x68 */
    0, /* 105 0x69 */
    0, /* 106 0x6a */
    0, /* 107 0x6b */
    0, /* 108 0x6c */
    0, /* 109 0x6d */
    0, /* 110 0x6e */
    0, /* 111 0x6f */
    0, /* 112 0x70 */
    0, /* 113 0x71 */
    0, /* 114 0x72 */
    0, /* 115 0x73 */
    0, /* 116 0x74 */
    0, /* 117 0x75 */
    0, /* 118 0x76 */
    0, /* 119 0x77 */
    0, /* 120 0x78 */
    0, /* 121 0x79 */
    0, /* 122 0x7a */
    0, /* 123 0x7b */
    0, /* 124 0x7c */
    0, /* 125 0x7d */
    0, /* 126 0x7e */
    0, /* 127 0x7f */
};

static const char shift_digits[16] = {
    ')', /* 0 */
    '!', /* 1 */
    '@', /* 2 */
    '#', /* 3 */
    '$', /* 4 */
    '%', /* 5 */
    '^', /* 6 */
    '&', /* 7 */
    '*', /* 8 */
    '(', /* 9 */
};

static volatile Uint8 scancode_buf[100];
static volatile int scancode_count;

static void
DOS_KeyboardISR(void)
{
    /* Read scancodes from keyboard into buffer. */
    while (inportb(PS2_STATUS) & 1 && scancode_count < SDL_arraysize(scancode_buf)) {
        scancode_buf[scancode_count++] = inportb(PS2_DATA);
    }

    /* Acknowledge interrupt. */
    outportb(0x20, 0x20);
}

static int
DOS_LockKeyboardISR(void)
{
    size_t len = (void *)DOS_LockKeyboardISR - (void *)DOS_KeyboardISR;

    SDL_LogDebug(SDL_LOG_CATEGORY_INPUT, "DOS: Keyboard ISR code size is %zd bytes", len);

    /* Lock interrupt service routine. */
    if (_go32_dpmi_lock_code(DOS_KeyboardISR, len)) {
        return SDL_SetError("DOS: Failed to lock keyboard ISR code (%zd bytes)", len);
    }

    /* Lock scancode buffer. */
    if (_go32_dpmi_lock_data((void *)scancode_buf, sizeof(scancode_buf))) {
        return SDL_SetError("DOS: Failed to lock scancode buffer (%zu bytes)", sizeof(scancode_buf));
    }

    /* Lock scancode counter. */
    if (_go32_dpmi_lock_data((void *)&scancode_count, sizeof(scancode_count))) {
        return SDL_SetError("DOS: Failed to lock scancode counter (%zu bytes)", sizeof(scancode_count));
    }

    return 0;
}

static SDL_bool kbd_is_init;
static _go32_dpmi_seginfo kbd_isr, old_kbd_isr;

static int
DOS_InitKeyboard(void)
{
    if (kbd_is_init) {
        return 0;
    }

    /* Save the original keyboard interrupt service routine. */
    if (_go32_dpmi_get_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &old_kbd_isr)) {
        return SDL_SetError("DOS: Failed to get original keyboard ISR");
    }

    /* Lock memory that is touched during an interrupt. */
    if (DOS_LockKeyboardISR()) {
        return -1;
    }

    /* Setup struct for input parameters. */
    kbd_isr.pm_offset = (unsigned long)DOS_KeyboardISR;

    /* Wrap the keyboard ISR so it can be used. */
    if (_go32_dpmi_allocate_iret_wrapper(&kbd_isr)) {
        return SDL_SetError("DOS: Failed to wrap keyboard ISR");
    }

    /* Use the new keyboard ISR. */
    if (_go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &kbd_isr)) {
        _go32_dpmi_free_iret_wrapper(&kbd_isr);
        return SDL_SetError("DOS: Failed to set new keyboard ISR");
    }

    kbd_is_init = SDL_TRUE;

    return 0;
}

static void
DOS_ProcessScancode(Uint8 raw)
{
    static SDL_bool extended_key = SDL_FALSE;
    Uint8 state = raw & 0x80 ? SDL_RELEASED : SDL_PRESSED;
    SDL_Scancode scancode;

    /* Check if the code is an extended key prefix. */
    if (raw == 0xE0) {
        extended_key = SDL_TRUE;
        return;
    }

    /* Mask off state bit. */
    raw &= 0x7F;

    /* Convert to SDL scancode. */
    if (extended_key) {
        scancode = extended_key_to_sdl_scancode[raw];
    } else {
        scancode = bios_to_sdl_scancode[raw];
    }

    /* Reset extended key flag. */
    extended_key = SDL_FALSE;

    /* Send a key event. Return if it wasn't posted. */
    if (SDL_SendKeyboardKey(state, scancode) == 0) return;

    /* If text input events are enabled, send one with basic US layout conversion. */
    if (state == SDL_PRESSED && SDL_GetEventState(SDL_TEXTINPUT) == SDL_ENABLE) {
        const SDL_Keymod modstate = SDL_GetModState();
        const SDL_Keycode keycode = SDL_GetKeyFromScancode(scancode);
        if (((modstate & (KMOD_CTRL | KMOD_ALT)) == 0) &&
            keycode >= SDLK_SPACE && keycode <= SDLK_z) {
            char buf[2];
            if ((modstate & KMOD_SHIFT)) {
                if (keycode >= SDLK_0 && keycode <= SDLK_9) {
                    buf[0] = shift_digits[keycode - '0'];
                } else if (keycode >= SDLK_a && keycode <= SDLK_z) {
                    buf[0] = keycode - ('a' - 'A');
                }
            } else {
                buf[0] = (char)keycode;
            }
            buf[1] = '\0';
            SDL_SendKeyboardText(buf);
        }
    }
}

static void
DOS_PollKeyboard(void)
{
    int i;

    /* Convert buffered scancodes to SDL key events. */
    for (i = 0; i < scancode_count; i++) {
        DOS_ProcessScancode(scancode_buf[i]);
    }

    /* Reset scancode buffer count. */
    /* TODO: Do not write to scancode buffer while it's being drained here. */
    scancode_count = 0;

    /* Read any remaining scancodes from keyboard and convert to SDL key events. */
    for (i = 0; inportb(PS2_STATUS) & 1; i++) {
        DOS_ProcessScancode(inportb(PS2_DATA));
    }

    /* Warn if scancode buffer reached maximum capacity. */
    if (i) {
        SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "DOS: %d keyboard scancode(s) not buffered", i);
    }
}

static void
DOS_QuitKeyboard(void)
{
    if (kbd_is_init) {
        return;
    }

    /* Restore original keyboard interrupt service routine. */
    _go32_dpmi_set_protected_mode_interrupt_vector(KEYBOARD_INTERRUPT, &old_kbd_isr);

    /* Cleanup keyboard ISR wrapper. */
    _go32_dpmi_free_iret_wrapper(&kbd_isr);

    kbd_is_init = SDL_FALSE;
}

int
SDL_DOS_Init(void)
{
    return DOS_InitKeyboard();
}

void
SDL_DOS_PumpEvents(void)
{
    DOS_PollKeyboard();
}

void
SDL_DOS_Quit(void)
{
    DOS_QuitKeyboard();
}

#endif /* __MSDOS__ */

/* vi: set ts=4 sw=4 expandtab: */
