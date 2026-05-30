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

#ifndef SDL_ibus_h_
#define SDL_ibus_h_

#ifdef HAVE_IBUS
#define SDL_USE_IBUS 1

/* IBusModifierType and IBusCapabilite have been copied from ibustypes.h */

typedef enum
{
    IBUS_SHIFT_MASK    = 1 << 0,
    IBUS_LOCK_MASK     = 1 << 1,
    IBUS_CONTROL_MASK  = 1 << 2,
    IBUS_MOD1_MASK     = 1 << 3,
    IBUS_MOD2_MASK     = 1 << 4,
    IBUS_MOD3_MASK     = 1 << 5,
    IBUS_MOD4_MASK     = 1 << 6,
    IBUS_MOD5_MASK     = 1 << 7,
    IBUS_BUTTON1_MASK  = 1 << 8,
    IBUS_BUTTON2_MASK  = 1 << 9,
    IBUS_BUTTON3_MASK  = 1 << 10,
    IBUS_BUTTON4_MASK  = 1 << 11,
    IBUS_BUTTON5_MASK  = 1 << 12,

    /* ibus mask */
    IBUS_HANDLED_MASK  = 1 << 24,
    IBUS_FORWARD_MASK  = 1 << 25,
    IBUS_IGNORED_MASK  = IBUS_FORWARD_MASK,

    IBUS_SUPER_MASK    = 1 << 26,
    IBUS_HYPER_MASK    = 1 << 27,
    IBUS_META_MASK     = 1 << 28,

    IBUS_RELEASE_MASK  = 1 << 30,

    IBUS_MODIFIER_MASK = 0x5f001fff
} IBusModifierType;

typedef enum {
    IBUS_CAP_PREEDIT_TEXT       = 1 << 0,
    IBUS_CAP_AUXILIARY_TEXT     = 1 << 1,
    IBUS_CAP_LOOKUP_TABLE       = 1 << 2,
    IBUS_CAP_FOCUS              = 1 << 3,
    IBUS_CAP_PROPERTY           = 1 << 4,
    IBUS_CAP_SURROUNDING_TEXT   = 1 << 5,
    IBUS_CAP_OSK                = 1 << 6,
    IBUS_CAP_SYNC_PROCESS_KEY   = 1 << 7,
    IBUS_CAP_SYNC_PROCESS_KEY_V2 = IBUS_CAP_SYNC_PROCESS_KEY,
} IBusCapabilite;

extern bool SDL_IBus_Init(void);
extern void SDL_IBus_Quit(void);

// Lets the IBus server know about changes in window focus
extern void SDL_IBus_SetFocus(bool focused);

// Closes the candidate list and resets any text currently being edited
extern void SDL_IBus_Reset(void);

/* Sends a keypress event to IBus, returns true if IBus used this event to
   update its candidate list or change input methods. PumpEvents should be
   called some time after this, to receive the TextInput / TextEditing event back. */
extern bool SDL_IBus_ProcessKeyEvent(Uint32 keysym, Uint32 keycode, bool down);

/* Update the position of IBus' candidate list. If rect is NULL then this will
   just reposition it relative to the focused window's new position. */
extern void SDL_IBus_UpdateTextInputArea(SDL_Window *window);

/* Checks DBus for new IBus events, and calls SDL_SendKeyboardText /
   SDL_SendEditingText for each event it finds */
extern void SDL_IBus_PumpEvents(void);

#endif // HAVE_IBUS

#endif // SDL_ibus_h_
