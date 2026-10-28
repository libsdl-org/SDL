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

#ifndef SDL_JOYSTICK_IOKIT_H
#define SDL_JOYSTICK_IOKIT_H

#include <IOKit/hid/IOHIDLib.h>
#include <ForceFeedback/ForceFeedback.h>
#include <ForceFeedback/ForceFeedbackConstants.h>

struct recElement
{
    IOHIDElementRef elementRef;
    IOHIDElementCookie cookie;
    uint32_t usagePage, usage; // HID usage
    SInt32 min;                // reported min value possible
    SInt32 max;                // reported max value possible

    // runtime variables used for auto-calibration
    SInt32 minReport; // min returned value
    SInt32 maxReport; // max returned value

    struct recElement *pNext; // next element in list
};
typedef struct recElement recElement;

struct joystick_hwdata
{
    IOHIDDeviceRef deviceRef; // HID device handle, open while the joystick is open
    io_service_t ffservice;   // Interface for force feedback, 0 = no ff
    FFDeviceObjectReference ffdevice;
    FFEFFECT *ffeffect;
    FFEffectObjectReference ffeffect_ref;
    bool ff_initialized;

    int axes;     // number of axis (calculated, not reported by device)
    int buttons;  // number of buttons (calculated, not reported by device)
    int hats;     // number of hat switches (calculated, not reported by device)
    int elements; // number of total elements (should be total of above) (calculated, not reported by device)

    recElement *firstAxis;
    recElement *firstButton;
    recElement *firstHat;

    bool nacon_revolution_x_unlimited;
};
typedef struct joystick_hwdata recDevice;

#endif // SDL_JOYSTICK_IOKIT_H
