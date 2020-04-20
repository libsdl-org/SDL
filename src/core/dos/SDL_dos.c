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
#include <go32.h>
#include <pc.h>

#include "../../events/SDL_events_c.h"

#define KEYBOARD_INTERRUPT 0x09

#define PS2_DATA   0x60
#define PS2_STATUS 0x64

static const SDL_Scancode bios_to_sdl_scancode[128] = {
    0,
    SDL_SCANCODE_ESCAPE,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_4,
    SDL_SCANCODE_5,
    SDL_SCANCODE_6,
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
    SDL_SCANCODE_9,
    SDL_SCANCODE_0,
    SDL_SCANCODE_MINUS,
    SDL_SCANCODE_EQUALS,
    SDL_SCANCODE_BACKSPACE,
    SDL_SCANCODE_TAB,
    SDL_SCANCODE_Q,
    SDL_SCANCODE_W,
    SDL_SCANCODE_E,
    SDL_SCANCODE_R,
    SDL_SCANCODE_T,
    SDL_SCANCODE_Y,
    SDL_SCANCODE_U,
    SDL_SCANCODE_I,
    SDL_SCANCODE_O,
    SDL_SCANCODE_P,
    SDL_SCANCODE_LEFTBRACKET,
    SDL_SCANCODE_RIGHTBRACKET,
    SDL_SCANCODE_RETURN,
    SDL_SCANCODE_LCTRL,
    SDL_SCANCODE_A,
    SDL_SCANCODE_S,
    SDL_SCANCODE_D,
    SDL_SCANCODE_F,
    SDL_SCANCODE_G,
    SDL_SCANCODE_H,
    SDL_SCANCODE_J,
    SDL_SCANCODE_K,
    SDL_SCANCODE_L,
    SDL_SCANCODE_SEMICOLON,
    SDL_SCANCODE_APOSTROPHE,
    SDL_SCANCODE_GRAVE,
    SDL_SCANCODE_LSHIFT,
    SDL_SCANCODE_BACKSLASH,
    SDL_SCANCODE_Z,
    SDL_SCANCODE_X,
    SDL_SCANCODE_C,
    SDL_SCANCODE_V,
    SDL_SCANCODE_B,
    SDL_SCANCODE_N,
    SDL_SCANCODE_M,
    SDL_SCANCODE_COMMA,
    SDL_SCANCODE_PERIOD,
    SDL_SCANCODE_SLASH,
    SDL_SCANCODE_RSHIFT,
    SDL_SCANCODE_KP_MULTIPLY,
    SDL_SCANCODE_LALT,
    SDL_SCANCODE_SPACE,
    SDL_SCANCODE_CAPSLOCK,
    SDL_SCANCODE_F1,
    SDL_SCANCODE_F2,
    SDL_SCANCODE_F3,
    SDL_SCANCODE_F4,
    SDL_SCANCODE_F5,
    SDL_SCANCODE_F6,
    SDL_SCANCODE_F7,
    SDL_SCANCODE_F8,
    SDL_SCANCODE_F9,
    SDL_SCANCODE_F10,
    SDL_SCANCODE_NUMLOCKCLEAR,
    SDL_SCANCODE_SCROLLLOCK,
    SDL_SCANCODE_KP_7,
    SDL_SCANCODE_KP_8,
    SDL_SCANCODE_KP_9,
    SDL_SCANCODE_KP_MINUS,
    SDL_SCANCODE_KP_4,
    SDL_SCANCODE_KP_5,
    SDL_SCANCODE_KP_6,
    SDL_SCANCODE_KP_PLUS,
    SDL_SCANCODE_KP_1,
    SDL_SCANCODE_KP_2,
    SDL_SCANCODE_KP_3,
    SDL_SCANCODE_KP_0,
    SDL_SCANCODE_KP_PERIOD,
    SDL_SCANCODE_SYSREQ,
    0,
    SDL_SCANCODE_LGUI,
    SDL_SCANCODE_F11,
    SDL_SCANCODE_F12,
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
    kbd_isr.pm_selector = _my_cs();
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
DOS_ProcessScancode(Uint8 scancode)
{
    static SDL_bool extended_key = SDL_FALSE;
    Uint8 state = scancode & 0x80 ? SDL_RELEASED : SDL_PRESSED;

    /* Check if the code is an extended key prefix. */
    if (scancode == 0xE0) {
        extended_key = SDL_TRUE;
        return;
    }

    /* Mask off state bit. */
    scancode &= 0x7F;

    /* Generate SDL key event. */
    if (extended_key) {
        /* TODO: Handle extended keyboard scancodes. */
    } else {
        SDL_SendKeyboardKey(state, bios_to_sdl_scancode[scancode]);
    }

    /* Reset extended key flag. */
    extended_key = SDL_FALSE;
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
