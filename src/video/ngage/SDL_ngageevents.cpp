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

#ifdef SDL_VIDEO_DRIVER_NGAGE

/* Being a ngage driver, there's no event stream. We just define stubs for
   most of the API. */

#ifdef __cplusplus
extern "C" {
#endif

#include "../../events/SDL_events_c.h"
#include "../../events/SDL_keyboard_c.h"

#ifdef __cplusplus
}
#endif

#include "SDL_ngagevideo.h"
#include "SDL_ngageevents_c.h"

static void HandleWsEvent(SDL_VideoDevice *_this, const TWsEvent &aWsEvent);

void NGAGE_PumpEvents(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = _this->internal;

    while (data->NGAGE_WsEventStatus != KRequestPending) {
        data->NGAGE_WsSession.GetEvent(data->NGAGE_WsEvent);

        HandleWsEvent(_this, data->NGAGE_WsEvent);

        data->NGAGE_WsEventStatus = KRequestPending;
        data->NGAGE_WsSession.EventReady(&data->NGAGE_WsEventStatus);
    }
}

/*****************************************************************************/
// Internal
/*****************************************************************************/

#include <bautils.h>
#include <hal.h>

extern void DisableKeyBlocking(SDL_VideoDevice *_this);
extern void RedrawWindowL(SDL_VideoDevice *_this);

TBool isCursorVisible = EFalse;

static SDL_Scancode ConvertScancode(SDL_VideoDevice *_this, int key)
{
    SDL_Keycode scancode;

    switch (key) {
    case EStdKeyBackspace: // Clear key
        scancode = SDL_SCANCODE_BACKSPACE;
        break;
    case 0x31: // 1
        scancode = SDL_SCANCODE_1;
        break;
    case 0x32: // 2
        scancode = SDL_SCANCODE_2;
        break;
    case 0x33: // 3
        scancode = SDL_SCANCODE_3;
        break;
    case 0x34: // 4
        scancode = SDL_SCANCODE_4;
        break;
    case 0x35: // 5
        scancode = SDL_SCANCODE_5;
        break;
    case 0x36: // 6
        scancode = SDL_SCANCODE_6;
        break;
    case 0x37: // 7
        scancode = SDL_SCANCODE_7;
        break;
    case 0x38: // 8
        scancode = SDL_SCANCODE_8;
        break;
    case 0x39: // 9
        scancode = SDL_SCANCODE_9;
        break;
    case 0x30: // 0
        scancode = SDL_SCANCODE_0;
        break;
    case 0x2a: // Asterisk
        scancode = SDL_SCANCODE_ASTERISK;
        break;
    case EStdKeyHash: // Hash
        scancode = SDL_SCANCODE_HASH;
        break;
    case EStdKeyDevice0: // Left softkey
        scancode = SDL_SCANCODE_SOFTLEFT;
        break;
    case EStdKeyDevice1: // Right softkey
        scancode = SDL_SCANCODE_SOFTRIGHT;
        break;
    case EStdKeyApplication0: // Call softkey
        scancode = SDL_SCANCODE_CALL;
        break;
    case EStdKeyApplication1: // End call softkey
        scancode = SDL_SCANCODE_ENDCALL;
        break;
    case EStdKeyDevice3: // Middle softkey
        scancode = SDL_SCANCODE_SELECT;
        break;
    case EStdKeyUpArrow: // Up arrow
        scancode = SDL_SCANCODE_UP;
        break;
    case EStdKeyDownArrow: // Down arrow
        scancode = SDL_SCANCODE_DOWN;
        break;
    case EStdKeyLeftArrow: // Left arrow
        scancode = SDL_SCANCODE_LEFT;
        break;
    case EStdKeyRightArrow: // Right arrow
        scancode = SDL_SCANCODE_RIGHT;
        break;
    default:
        scancode = SDL_SCANCODE_UNKNOWN;
        break;
    }

    return scancode;
}

static void HandleWsEvent(SDL_VideoDevice *_this, const TWsEvent &aWsEvent)
{
    SDL_VideoData *data = _this->internal;

    switch (aWsEvent.Type()) {
    case EEventKeyDown: // Key events
        SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, aWsEvent.Key()->iScanCode, ConvertScancode(_this, aWsEvent.Key()->iScanCode), true);
        break;
    case EEventKeyUp: // Key events
        SDL_SendKeyboardKey(0, SDL_GLOBAL_KEYBOARD_ID, aWsEvent.Key()->iScanCode, ConvertScancode(_this, aWsEvent.Key()->iScanCode), false);
        break;
    case EEventFocusGained: // SDL window got focus
        data->NGAGE_IsWindowFocused = ETrue;
        // Draw window background and screen buffer
        DisableKeyBlocking(_this);
        RedrawWindowL(_this);
        break;
    case EEventFocusLost: // SDL window lost focus
    {
        data->NGAGE_IsWindowFocused = EFalse;
        RWsSession s;
        s.Connect();
        RWindowGroup g(s);
        g.Construct(TUint32(&g), EFalse);
        g.EnableReceiptOfFocus(EFalse);
        RWindow w(s);
        w.Construct(g, TUint32(&w));
        w.SetExtent(TPoint(0, 0), data->NGAGE_WsWindow.Size());
        w.SetOrdinalPosition(0);
        w.Activate();
        w.Close();
        g.Close();
        s.Close();
        break;
    }
    case EEventModifiersChanged:
        break;
    default:
        break;
    }
}

#endif // SDL_VIDEO_DRIVER_NGAGE
