/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

#ifndef SDL_JOYSTICK_IOKIT_H


#include <IOKit/hid/IOHIDLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/IOKitLib.h>


struct recElement
{
    IOHIDElementCookie cookie;  /* unique value which identifies element, will NOT change */
    long usagePage, usage;      /* HID usage */
    long min;                   /* reported min value possible */
    long max;                   /* reported max value possible */
#if 0
    /* TODO: maybe should handle the following stuff somehow? */

    long scaledMin;             /* reported scaled min value possible */
    long scaledMax;             /* reported scaled max value possible */
    long size;                  /* size in bits of data return from element */
    Boolean relative;           /* are reports relative to last report (deltas) */
    Boolean wrapping;           /* does element wrap around (one value higher than max is min) */
    Boolean nonLinear;          /* are the values reported non-linear relative to element movement */
    Boolean preferredState;     /* does element have a preferred state (such as a button) */
    Boolean nullState;          /* does element have null state */
#endif                          /* 0 */

    /* runtime variables used for auto-calibration */
    long minReport;             /* min returned value */
    long maxReport;             /* max returned value */

    struct recElement *pNext;   /* next element in list */
};
typedef struct recElement recElement;

struct joystick_hwdata
{
    io_service_t ffservice;     /* Interface for force feedback, 0 = no ff */
    IOHIDDeviceInterface **interface;   /* interface to device, NULL = no interface */
    IONotificationPortRef notificationPort; /* port to be notified on joystick removal */
    io_iterator_t portIterator; /* iterator for removal callback */

    char product[256];          /* name of product */
    long usage;                 /* usage page from IOUSBHID Parser.h which defines general usage */
    long usagePage;             /* usage within above page from IOUSBHID Parser.h which defines specific usage */

    long axes;                  /* number of axis (calculated, not reported by device) */
    long buttons;               /* number of buttons (calculated, not reported by device) */
    long hats;                  /* number of hat switches (calculated, not reported by device) */
    long elements;              /* number of total elements (should be total of above) (calculated, not reported by device) */

    recElement *firstAxis;
    recElement *firstButton;
    recElement *firstHat;

    int removed;
    int uncentered;

    int instance_id;
    SDL_JoystickGUID guid;
    Uint8 send_open_event;      /* 1 if we need to send an Added event for this device */

    struct joystick_hwdata *pNext;      /* next device */
};
typedef struct joystick_hwdata recDevice;


#endif /* SDL_JOYSTICK_IOKIT_H */
