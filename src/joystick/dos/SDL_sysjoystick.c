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

#ifdef SDL_JOYSTICK_DOS

#include <limits.h>
#include <pc.h> /* for inportb */
#include <time.h>

#include "../SDL_joystick_c.h"
#include "../SDL_sysjoystick.h"

#define GAMEPORT 0x201

/* Gameport status byte axis bits */
#define GAMEPORT_AXIS_X 0x01 /* bit 0 */
#define GAMEPORT_AXIS_Y 0x02 /* bit 1 */

/* Gameport status byte button bits (active low) */
#define GAMEPORT_BUTTON1 0x10 /* bit 4 */
#define GAMEPORT_BUTTON2 0x20 /* bit 5 */
#define GAMEPORT_BUTTON3 0x40 /* bit 6 */
#define GAMEPORT_BUTTON4 0x80 /* bit 7 */

/* Static state for detection */
static bool dos_joystick_detected = false;
static SDL_JoystickID dos_joystick_id = 0;
static SDL_JoystickID dos_next_instance_id = 1;
static Uint64 dos_joystick_next_poll_ns = 0;
#define DOS_JOYSTICK_POLL_INTERVAL_NS SDL_MS_TO_NS(16) /* ~60 Hz is plenty for a 2-axis gameport stick */

struct joystick_hwdata
{
    int axis_min[2];    /* minimum raw axis values seen */
    int axis_max[2];    /* maximum raw axis values seen */
    int axis_center[2]; /* center raw axis values (captured on first read) */
    bool calibrated;    /* whether we've seen enough range */
};

/*
 * Read joystick axes directly from the gameport. This is done by timing how
 * long each axis bit stays high. This is similar to how the BIOS does it, but
 * with a tighter timeout. An axis that hasn't fired by the timeout is reported
 * as -1.
 */
static void ReadGameportAxes(int *axis_x, int *axis_y)
{
    Uint8 pending = GAMEPORT_AXIS_X | GAMEPORT_AXIS_Y;
    uclock_t start, elapsed;
    Uint8 val;

    *axis_x = -1;
    *axis_y = -1;

    outportb(GAMEPORT, 0xFF);
    start = uclock();
    do {
        val = inportb(GAMEPORT);
        elapsed = uclock() - start;
        if ((pending & GAMEPORT_AXIS_X) && !(val & GAMEPORT_AXIS_X)) {
            *axis_x = (int)elapsed;
            pending &= ~GAMEPORT_AXIS_X;
        }
        if ((pending & GAMEPORT_AXIS_Y) && !(val & GAMEPORT_AXIS_Y)) {
            *axis_y = (int)elapsed;
            pending &= ~GAMEPORT_AXIS_Y;
        }
    } while (pending && elapsed < (UCLOCKS_PER_SEC / 500)); /* 2 ms */
}

/*
 * Probe for joystick presence by reading the axes. With no stick the inputs are
 * open, and with no gameport the bus floats high, so both time out.
 * Returns true if the read doesn't timeout.
 */
static bool ProbeGameport(void)
{
    int axis_x, axis_y;
    ReadGameportAxes(&axis_x, &axis_y);
    return (axis_x >= 0) || (axis_y >= 0);
}

static Sint16 CalibrateAxis(int raw, struct joystick_hwdata *hwdata, int axis)
{
    int range;
    int centered;

    if (raw < 0) {
        return 0; /* axis not connected, report center */
    }

    if (raw < hwdata->axis_min[axis]) {
        hwdata->axis_min[axis] = raw;
    }
    if (raw > hwdata->axis_max[axis]) {
        hwdata->axis_max[axis] = raw;
    }

    if (!hwdata->calibrated) {
        hwdata->axis_center[axis] = raw;
        /* Consider calibrated once we've seen some range on either axis */
        if ((hwdata->axis_max[0] - hwdata->axis_min[0]) > 20 ||
            (hwdata->axis_max[1] - hwdata->axis_min[1]) > 20) {
            hwdata->calibrated = true;
        }
    }

    range = hwdata->axis_max[axis] - hwdata->axis_min[axis];
    if (range < 10) {
        range = 10; /* avoid division issues */
    }

    /* Map to -32768..32767 */
    centered = raw - hwdata->axis_center[axis];
    return (Sint16)SDL_clamp(((Sint64)centered * 65535) / range, -32768, 32767);
}

static bool DOS_JoystickInit(void)
{
    dos_joystick_detected = ProbeGameport();
    if (dos_joystick_detected) {
        dos_joystick_id = dos_next_instance_id++;
    }
    return true;
}

static int DOS_JoystickGetCount(void)
{
    return dos_joystick_detected ? 1 : 0;
}

static void DOS_JoystickDetect(void)
{
    /* This could probably be implemented calling ProbeGameport(). */
}

static bool DOS_JoystickIsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    return false;
}

static const char *DOS_JoystickGetDeviceName(int device_index)
{
    if (device_index == 0 && dos_joystick_detected) {
        return "DOS Gameport Joystick";
    }
    return NULL;
}

static const char *DOS_JoystickGetDevicePath(int device_index)
{
    if (device_index == 0 && dos_joystick_detected) {
        return "gameport:0x201";
    }
    return NULL;
}

static int DOS_JoystickGetDeviceSteamVirtualGamepadSlot(int device_index)
{
    return -1;
}

static int DOS_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void DOS_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_GUID DOS_JoystickGetDeviceGUID(int device_index)
{
    return SDL_CreateJoystickGUID(
        SDL_HARDWARE_BUS_UNKNOWN, /* bus */
        0x0000,                   /* vendor */
        0x0201,                   /* product (port number) */
        0x0001,                   /* version */
        NULL,                     /* vendor_name */
        "DOS Gameport Joystick",  /* product_name */
        0,                        /* driver_signature */
        0                         /* driver_data */
    );
}

static SDL_JoystickID DOS_JoystickGetDeviceInstanceID(int device_index)
{
    if (device_index == 0 && dos_joystick_detected) {
        return dos_joystick_id;
    }
    return 0;
}

static bool DOS_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    struct joystick_hwdata *hwdata;

    hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*hwdata));
    if (!hwdata) {
        return false;
    }

    hwdata->axis_min[0] = INT_MAX;
    hwdata->axis_min[1] = INT_MAX;
    hwdata->axis_max[0] = 0;
    hwdata->axis_max[1] = 0;
    hwdata->axis_center[0] = 0;
    hwdata->axis_center[1] = 0;
    hwdata->calibrated = false;

    joystick->hwdata = hwdata;
    joystick->naxes = 2;
    joystick->nbuttons = 4;
    joystick->nhats = 0;
    joystick->nballs = 0;

    return true;
}

static bool DOS_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static bool DOS_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static bool DOS_JoystickSetLED(SDL_Joystick *joystick, Uint8 red, Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static bool DOS_JoystickSendEffect(SDL_Joystick *joystick, const void *data, int size)
{
    return SDL_Unsupported();
}

static bool DOS_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    return SDL_Unsupported();
}

static void DOS_JoystickUpdate(SDL_Joystick *joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    int axis_x, axis_y;
    Uint8 val;
    Uint64 now;

    if (!hwdata) {
        return;
    }

    /* Buttons are a passive port read (no timing loop), always safe to poll */
    val = inportb(GAMEPORT);
    SDL_SendJoystickButton(0, joystick, 0, !(val & GAMEPORT_BUTTON1));
    SDL_SendJoystickButton(0, joystick, 1, !(val & GAMEPORT_BUTTON2));
    SDL_SendJoystickButton(0, joystick, 2, !(val & GAMEPORT_BUTTON3));
    SDL_SendJoystickButton(0, joystick, 3, !(val & GAMEPORT_BUTTON4));

    /* Polling the joystick axis will hang for up to 2ms if disconnected. */
    now = SDL_GetTicksNS();
    if (now < dos_joystick_next_poll_ns) {
        return;
    }
    dos_joystick_next_poll_ns = now + DOS_JOYSTICK_POLL_INTERVAL_NS;

    ReadGameportAxes(&axis_x, &axis_y);

    if (axis_x >= 0) {
        Sint16 cal_x = CalibrateAxis(axis_x, hwdata, 0);
        SDL_SendJoystickAxis(0, joystick, 0, cal_x);
    }

    if (axis_y >= 0) {
        Sint16 cal_y = CalibrateAxis(axis_y, hwdata, 1);
        SDL_SendJoystickAxis(0, joystick, 1, cal_y);
    }
}

static void DOS_JoystickClose(SDL_Joystick *joystick)
{
    if (joystick->hwdata) {
        SDL_free(joystick->hwdata);
        joystick->hwdata = NULL;
    }
}

static void DOS_JoystickQuit(void)
{
    dos_joystick_detected = false;
    dos_joystick_id = 0;
}

static bool DOS_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    return false;
}

SDL_JoystickDriver SDL_DOS_JoystickDriver = {
    DOS_JoystickInit,
    DOS_JoystickGetCount,
    DOS_JoystickDetect,
    DOS_JoystickIsDevicePresent,
    DOS_JoystickGetDeviceName,
    DOS_JoystickGetDevicePath,
    DOS_JoystickGetDeviceSteamVirtualGamepadSlot,
    DOS_JoystickGetDevicePlayerIndex,
    DOS_JoystickSetDevicePlayerIndex,
    DOS_JoystickGetDeviceGUID,
    DOS_JoystickGetDeviceInstanceID,
    DOS_JoystickOpen,
    DOS_JoystickRumble,
    DOS_JoystickRumbleTriggers,
    DOS_JoystickSetLED,
    DOS_JoystickSendEffect,
    DOS_JoystickSetSensorsEnabled,
    DOS_JoystickUpdate,
    DOS_JoystickClose,
    DOS_JoystickQuit,
    DOS_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_DOS */
