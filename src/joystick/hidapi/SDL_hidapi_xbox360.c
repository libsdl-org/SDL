/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2018 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_HIDAPI

#include "SDL_hints.h"
#include "SDL_log.h"
#include "SDL_events.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "SDL_gamecontroller.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"


#ifdef SDL_JOYSTICK_HIDAPI_XBOX360

#define USB_PACKET_LENGTH   64

typedef struct
{
    Uint16 vendor_id;
    Uint16 product_id;
    const char *name;
} SDL_DriverXbox360_DeviceName;

static const SDL_DriverXbox360_DeviceName xbox360_devicenames[] = {
    { 0x0079, 0x18d4, "GPD Win 2 X-Box Controller" },
    { 0x044f, 0xb326, "Thrustmaster Gamepad GP XID" },
    { 0x045e, 0x028e, "Microsoft X-Box 360 pad" },
    { 0x045e, 0x028f, "Microsoft X-Box 360 pad v2" },
    { 0x045e, 0x0291, "Xbox 360 Wireless Receiver (XBOX)" },
    { 0x045e, 0x0719, "Xbox 360 Wireless Receiver" },
    { 0x046d, 0xc21d, "Logitech Gamepad F310" },
    { 0x046d, 0xc21e, "Logitech Gamepad F510" },
    { 0x046d, 0xc21f, "Logitech Gamepad F710" },
    { 0x046d, 0xc242, "Logitech Chillstream Controller" },
    { 0x046d, 0xcaa3, "Logitech DriveFx Racing Wheel" },
    { 0x056e, 0x2004, "Elecom JC-U3613M" },
    { 0x06a3, 0xf51a, "Saitek P3600" },
    { 0x0738, 0x4716, "Mad Catz Wired Xbox 360 Controller" },
    { 0x0738, 0x4718, "Mad Catz Street Fighter IV FightStick SE" },
    { 0x0738, 0x4726, "Mad Catz Xbox 360 Controller" },
    { 0x0738, 0x4728, "Mad Catz Street Fighter IV FightPad" },
    { 0x0738, 0x4736, "Mad Catz MicroCon Gamepad" },
    { 0x0738, 0x4738, "Mad Catz Wired Xbox 360 Controller (SFIV)" },
    { 0x0738, 0x4740, "Mad Catz Beat Pad" },
    { 0x0738, 0x4758, "Mad Catz Arcade Game Stick" },
    { 0x0738, 0x9871, "Mad Catz Portable Drum" },
    { 0x0738, 0xb726, "Mad Catz Xbox controller - MW2" },
    { 0x0738, 0xb738, "Mad Catz MVC2TE Stick 2" },
    { 0x0738, 0xbeef, "Mad Catz JOYTECH NEO SE Advanced GamePad" },
    { 0x0738, 0xcb02, "Saitek Cyborg Rumble Pad - PC/Xbox 360" },
    { 0x0738, 0xcb03, "Saitek P3200 Rumble Pad - PC/Xbox 360" },
    { 0x0738, 0xcb29, "Saitek Aviator Stick AV8R02" },
    { 0x0738, 0xf738, "Super SFIV FightStick TE S" },
    { 0x07ff, 0xffff, "Mad Catz GamePad" },
    { 0x0e6f, 0x0105, "HSM3 Xbox360 dancepad" },
    { 0x0e6f, 0x0113, "Afterglow AX.1 Gamepad for Xbox 360" },
    { 0x0e6f, 0x011f, "Rock Candy Gamepad Wired Controller" },
    { 0x0e6f, 0x0131, "PDP EA Sports Controller" },
    { 0x0e6f, 0x0133, "Xbox 360 Wired Controller" },
    { 0x0e6f, 0x0201, "Pelican PL-3601 'TSZ' Wired Xbox 360 Controller" },
    { 0x0e6f, 0x0213, "Afterglow Gamepad for Xbox 360" },
    { 0x0e6f, 0x021f, "Rock Candy Gamepad for Xbox 360" },
    { 0x0e6f, 0x0301, "Logic3 Controller" },
    { 0x0e6f, 0x0401, "Logic3 Controller" },
    { 0x0e6f, 0x0413, "Afterglow AX.1 Gamepad for Xbox 360" },
    { 0x0e6f, 0x0501, "PDP Xbox 360 Controller" },
    { 0x0e6f, 0xf900, "PDP Afterglow AX.1" },
    { 0x0f0d, 0x000a, "Hori Co. DOA4 FightStick" },
    { 0x0f0d, 0x000c, "Hori PadEX Turbo" },
    { 0x0f0d, 0x000d, "Hori Fighting Stick EX2" },
    { 0x0f0d, 0x0016, "Hori Real Arcade Pro.EX" },
    { 0x0f0d, 0x001b, "Hori Real Arcade Pro VX" },
    { 0x11c9, 0x55f0, "Nacon GC-100XF" },
    { 0x12ab, 0x0004, "Honey Bee Xbox360 dancepad" },
    { 0x12ab, 0x0301, "PDP AFTERGLOW AX.1" },
    { 0x12ab, 0x0303, "Mortal Kombat Klassic FightStick" },
    { 0x1430, 0x4748, "RedOctane Guitar Hero X-plorer" },
    { 0x1430, 0xf801, "RedOctane Controller" },
    { 0x146b, 0x0601, "BigBen Interactive XBOX 360 Controller" },
    { 0x1532, 0x0037, "Razer Sabertooth" },
    { 0x15e4, 0x3f00, "Power A Mini Pro Elite" },
    { 0x15e4, 0x3f0a, "Xbox Airflo wired controller" },
    { 0x15e4, 0x3f10, "Batarang Xbox 360 controller" },
    { 0x162e, 0xbeef, "Joytech Neo-Se Take2" },
    { 0x1689, 0xfd00, "Razer Onza Tournament Edition" },
    { 0x1689, 0xfd01, "Razer Onza Classic Edition" },
    { 0x1689, 0xfe00, "Razer Sabertooth" },
    { 0x1bad, 0x0002, "Harmonix Rock Band Guitar" },
    { 0x1bad, 0x0003, "Harmonix Rock Band Drumkit" },
    { 0x1bad, 0x0130, "Ion Drum Rocker" },
    { 0x1bad, 0xf016, "Mad Catz Xbox 360 Controller" },
    { 0x1bad, 0xf018, "Mad Catz Street Fighter IV SE Fighting Stick" },
    { 0x1bad, 0xf019, "Mad Catz Brawlstick for Xbox 360" },
    { 0x1bad, 0xf021, "Mad Cats Ghost Recon FS GamePad" },
    { 0x1bad, 0xf023, "MLG Pro Circuit Controller (Xbox)" },
    { 0x1bad, 0xf025, "Mad Catz Call Of Duty" },
    { 0x1bad, 0xf027, "Mad Catz FPS Pro" },
    { 0x1bad, 0xf028, "Street Fighter IV FightPad" },
    { 0x1bad, 0xf02e, "Mad Catz Fightpad" },
    { 0x1bad, 0xf030, "Mad Catz Xbox 360 MC2 MicroCon Racing Wheel" },
    { 0x1bad, 0xf036, "Mad Catz MicroCon GamePad Pro" },
    { 0x1bad, 0xf038, "Street Fighter IV FightStick TE" },
    { 0x1bad, 0xf039, "Mad Catz MvC2 TE" },
    { 0x1bad, 0xf03a, "Mad Catz SFxT Fightstick Pro" },
    { 0x1bad, 0xf03d, "Street Fighter IV Arcade Stick TE - Chun Li" },
    { 0x1bad, 0xf03e, "Mad Catz MLG FightStick TE" },
    { 0x1bad, 0xf03f, "Mad Catz FightStick SoulCaliber" },
    { 0x1bad, 0xf042, "Mad Catz FightStick TES+" },
    { 0x1bad, 0xf080, "Mad Catz FightStick TE2" },
    { 0x1bad, 0xf501, "HoriPad EX2 Turbo" },
    { 0x1bad, 0xf502, "Hori Real Arcade Pro.VX SA" },
    { 0x1bad, 0xf503, "Hori Fighting Stick VX" },
    { 0x1bad, 0xf504, "Hori Real Arcade Pro. EX" },
    { 0x1bad, 0xf505, "Hori Fighting Stick EX2B" },
    { 0x1bad, 0xf506, "Hori Real Arcade Pro.EX Premium VLX" },
    { 0x1bad, 0xf900, "Harmonix Xbox 360 Controller" },
    { 0x1bad, 0xf901, "Gamestop Xbox 360 Controller" },
    { 0x1bad, 0xf903, "Tron Xbox 360 controller" },
    { 0x1bad, 0xf904, "PDP Versus Fighting Pad" },
    { 0x1bad, 0xf906, "MortalKombat FightStick" },
    { 0x1bad, 0xfa01, "MadCatz GamePad" },
    { 0x1bad, 0xfd00, "Razer Onza TE" },
    { 0x1bad, 0xfd01, "Razer Onza" },
    { 0x24c6, 0x5000, "Razer Atrox Arcade Stick" },
    { 0x24c6, 0x5300, "PowerA MINI PROEX Controller" },
    { 0x24c6, 0x5303, "Xbox Airflo wired controller" },
    { 0x24c6, 0x530a, "Xbox 360 Pro EX Controller" },
    { 0x24c6, 0x531a, "PowerA Pro Ex" },
    { 0x24c6, 0x5397, "FUS1ON Tournament Controller" },
    { 0x24c6, 0x5500, "Hori XBOX 360 EX 2 with Turbo" },
    { 0x24c6, 0x5501, "Hori Real Arcade Pro VX-SA" },
    { 0x24c6, 0x5502, "Hori Fighting Stick VX Alt" },
    { 0x24c6, 0x5503, "Hori Fighting Edge" },
    { 0x24c6, 0x5506, "Hori SOULCALIBUR V Stick" },
    { 0x24c6, 0x550d, "Hori GEM Xbox controller" },
    { 0x24c6, 0x550e, "Hori Real Arcade Pro V Kai 360" },
    { 0x24c6, 0x5b00, "ThrustMaster Ferrari 458 Racing Wheel" },
    { 0x24c6, 0x5b02, "Thrustmaster, Inc. GPX Controller" },
    { 0x24c6, 0x5b03, "Thrustmaster Ferrari 458 Racing Wheel" },
    { 0x24c6, 0x5d04, "Razer Sabertooth" },
    { 0x24c6, 0xfafe, "Rock Candy Gamepad for Xbox 360" },
};

typedef struct {
    Uint8 last_state[USB_PACKET_LENGTH];
    Uint32 rumble_expiration;
} SDL_DriverXbox360_Context;


static SDL_bool
HIDAPI_DriverXbox360_IsSupportedDevice(Uint16 vendor_id, Uint16 product_id, Uint16 version, int interface_number, Uint16 usage_page, Uint16 usage)
{
#ifdef __MACOSX__
	if (vendor_id == 0x045e && product_id == 0x028e && version == 1) {
		/* This is the Steam Virtual Gamepad, which isn't supported by this driver */
		return SDL_FALSE;
	}
    return SDL_IsJoystickXbox360(vendor_id, product_id) || SDL_IsJoystickXboxOne(vendor_id, product_id);
#else
    return SDL_IsJoystickXbox360(vendor_id, product_id);
#endif
}

static const char *
HIDAPI_DriverXbox360_GetDeviceName(Uint16 vendor_id, Uint16 product_id)
{
    int i;

    for (i = 0; i < SDL_arraysize(xbox360_devicenames); ++i) {
        const SDL_DriverXbox360_DeviceName *entry = &xbox360_devicenames[i];
        if (vendor_id == entry->vendor_id && product_id == entry->product_id) {
            return entry->name;
        }
    }
    return NULL;
}

static SDL_bool SetSlotLED(hid_device *dev, Uint8 slot)
{
    const Uint8 led_packet[] = { 0x01, 0x03, (2 + slot) };

    if (hid_write(dev, led_packet, sizeof(led_packet)) != sizeof(led_packet)) {
		return SDL_FALSE;
	}
	return SDL_TRUE;
}

static SDL_bool
HIDAPI_DriverXbox360_Init(SDL_Joystick *joystick, hid_device *dev, Uint16 vendor_id, Uint16 product_id, void **context)
{
    SDL_DriverXbox360_Context *ctx;

    ctx = (SDL_DriverXbox360_Context *)SDL_calloc(1, sizeof(*ctx));
    if (!ctx) {
        SDL_OutOfMemory();
        return SDL_FALSE;
    }
    *context = ctx;

    /* Set the controller LED */
	SetSlotLED(dev, (joystick->instance_id % 4));

    /* Initialize the joystick capabilities */
    joystick->nbuttons = SDL_CONTROLLER_BUTTON_MAX;
    joystick->naxes = SDL_CONTROLLER_AXIS_MAX;
    joystick->epowerlevel = SDL_JOYSTICK_POWER_WIRED;

    return SDL_TRUE;
}

static int
HIDAPI_DriverXbox360_Rumble(SDL_Joystick *joystick, hid_device *dev, void *context, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)context;
#ifdef __MACOSX__
    /* On Mac OS X the 360Controller driver uses this short report,
       and we need to prefix it with a magic token so hidapi passes it through untouched
     */
    Uint8 rumble_packet[] = { 'M', 'A', 'G', 'I', 'C', '0', 0x00, 0x04, 0x00, 0x00 };

    rumble_packet[6+2] = (low_frequency_rumble >> 8);
    rumble_packet[6+3] = (high_frequency_rumble >> 8);
#else
    Uint8 rumble_packet[] = { 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

    rumble_packet[3] = (low_frequency_rumble >> 8);
    rumble_packet[4] = (high_frequency_rumble >> 8);
#endif

    if (hid_write(dev, rumble_packet, sizeof(rumble_packet)) != sizeof(rumble_packet)) {
        return SDL_SetError("Couldn't send rumble packet");
    }

    if ((low_frequency_rumble || high_frequency_rumble) && duration_ms) {
        ctx->rumble_expiration = SDL_GetTicks() + duration_ms;
    } else {
        ctx->rumble_expiration = 0;
    }
    return 0;
}

#if 0
 /* This is the packet format for Xbox 360 and Xbox One controllers on Windows,
    however with this interface there is no rumble support, no guide button,
    and the left and right triggers are tied together as a single axis.
  */
static void
HIDAPI_DriverXboxOne_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXbox360_Context *ctx, Uint8 *data, int size )
{
    Sint16 axis;

    if (ctx->last_state[10] != data[10]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[10] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[10] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[10] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[10] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[10] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[10] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[10] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[10] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[11] != data[11]) {
        SDL_bool dpad_up = SDL_FALSE;
        SDL_bool dpad_down = SDL_FALSE;
        SDL_bool dpad_left = SDL_FALSE;
        SDL_bool dpad_right = SDL_FALSE;

        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[11] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[11] & 0x02) ? SDL_PRESSED : SDL_RELEASED);

        switch (data[11] & 0x3C) {
        case 4:
            dpad_up = SDL_TRUE;
            break;
        case 8:
            dpad_up = SDL_TRUE;
            dpad_right = SDL_TRUE;
            break;
        case 12:
            dpad_right = SDL_TRUE;
            break;
        case 16:
            dpad_right = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 20:
            dpad_down = SDL_TRUE;
            break;
        case 24:
            dpad_left = SDL_TRUE;
            dpad_down = SDL_TRUE;
            break;
        case 28:
            dpad_left = SDL_TRUE;
            break;
        case 32:
            dpad_up = SDL_TRUE;
            dpad_left = SDL_TRUE;
            break;
        default:
            break;
        }
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, dpad_down);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, dpad_up);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, dpad_right);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, dpad_left);
    }

    axis = (int)*(Uint16*)(&data[0]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = (int)*(Uint16*)(&data[2]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = (int)*(Uint16*)(&data[4]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = (int)*(Uint16*)(&data[6]) - 0x8000;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    axis = (data[9] * 257) - 32768;
    if (data[9] < 0x80) {
        axis = -axis * 2 - 32769;
        SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
    } else if (data[9] > 0x80) {
        axis = axis * 2 - 32767;
        SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
    } else {
        SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, SDL_MIN_SINT16);
        SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, SDL_MIN_SINT16);
    }

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}
#endif /* 0 */

static void
HIDAPI_DriverXbox360_HandleStatePacket(SDL_Joystick *joystick, hid_device *dev, SDL_DriverXbox360_Context *ctx, Uint8 *data, int size)
{
    Sint16 axis;
#ifdef __MACOSX__
	const SDL_bool invert_y_axes = SDL_FALSE;
#else
	const SDL_bool invert_y_axes = SDL_TRUE;
#endif

    if (ctx->last_state[2] != data[2]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_UP, (data[2] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_DOWN, (data[2] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_LEFT, (data[2] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, (data[2] & 0x08) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_START, (data[2] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_BACK, (data[2] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSTICK, (data[2] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSTICK, (data[2] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    if (ctx->last_state[3] != data[3]) {
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_LEFTSHOULDER, (data[3] & 0x01) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, (data[3] & 0x02) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_GUIDE, (data[3] & 0x04) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_A, (data[3] & 0x10) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_B, (data[3] & 0x20) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_X, (data[3] & 0x40) ? SDL_PRESSED : SDL_RELEASED);
        SDL_PrivateJoystickButton(joystick, SDL_CONTROLLER_BUTTON_Y, (data[3] & 0x80) ? SDL_PRESSED : SDL_RELEASED);
    }

    axis = ((int)data[4] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, axis);
    axis = ((int)data[5] * 257) - 32768;
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, axis);
    axis = *(Sint16*)(&data[6]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTX, axis);
    axis = *(Sint16*)(&data[8]);
	if (invert_y_axes) {
		axis = ~axis;
	}
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_LEFTY, axis);
    axis = *(Sint16*)(&data[10]);
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTX, axis);
    axis = *(Sint16*)(&data[12]);
	if (invert_y_axes) {
		axis = ~axis;
	}
    SDL_PrivateJoystickAxis(joystick, SDL_CONTROLLER_AXIS_RIGHTY, axis);

    SDL_memcpy(ctx->last_state, data, SDL_min(size, sizeof(ctx->last_state)));
}

static SDL_bool
HIDAPI_DriverXbox360_Update(SDL_Joystick *joystick, hid_device *dev, void *context)
{
    SDL_DriverXbox360_Context *ctx = (SDL_DriverXbox360_Context *)context;
    Uint8 data[USB_PACKET_LENGTH];
    int size;

    while ((size = hid_read_timeout(dev, data, sizeof(data), 0)) > 0) {
        switch (data[0]) {
        case 0x00:
            HIDAPI_DriverXbox360_HandleStatePacket(joystick, dev, ctx, data, size);
            break;
        default:
#ifdef DEBUG_JOYSTICK
            SDL_Log("Unknown Xbox 360 packet: 0x%.2x\n", data[0]);
#endif
            break;
        }
    }

    if (ctx->rumble_expiration) {
        Uint32 now = SDL_GetTicks();
        if (SDL_TICKS_PASSED(now, ctx->rumble_expiration)) {
            HIDAPI_DriverXbox360_Rumble(joystick, dev, context, 0, 0, 0);
        }
    }

	return (size >= 0);
}

static void
HIDAPI_DriverXbox360_Quit(SDL_Joystick *joystick, hid_device *dev, void *context)
{
    SDL_free(context);
}

SDL_HIDAPI_DeviceDriver SDL_HIDAPI_DriverXbox360 =
{
    SDL_HINT_JOYSTICK_HIDAPI_XBOX360,
    SDL_TRUE,
    HIDAPI_DriverXbox360_IsSupportedDevice,
    HIDAPI_DriverXbox360_GetDeviceName,
    HIDAPI_DriverXbox360_Init,
    HIDAPI_DriverXbox360_Rumble,
    HIDAPI_DriverXbox360_Update,
    HIDAPI_DriverXbox360_Quit
};

#endif /* SDL_JOYSTICK_HIDAPI_XBOX360 */

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
