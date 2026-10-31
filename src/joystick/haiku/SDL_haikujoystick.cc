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

#ifdef SDL_JOYSTICK_HAIKU

// This is the Haiku implementation of the SDL joystick API

#include <support/String.h>
#include <device/Joystick.h>

extern "C"
{

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"


// The private structure used to keep track of a joystick
    struct joystick_hwdata
    {
        BJoystick *stick;
        uint8 *new_hats;
        int16 *new_axes;
    };

    static bool HAIKU_AddJoystick(const char *port, const char *stick_name)
    {
        SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_HAIKU_JoystickDriver, 0);
        if (!device) {
            return false;
        }
        device->name = SDL_CreateJoystickName(0, 0, NULL, stick_name);
        device->path = SDL_strdup(port);
        if (!device->name || !device->path) {
            SDL_DestroyJoystickDevice(device);
            return false;
        }
        // the GUID is just the name for now
        device->guid = SDL_CreateJoystickGUIDForName(device->name);

        SDL_AddJoystickDevice(device, false);
        return true;
    }

    static bool HAIKU_JoystickInit(void)
    {
        BJoystick joystick;
        int i;
        int32 nports;
        char name[B_OS_NAME_LENGTH];

        // Search for attached joysticks
        nports = joystick.CountDevices();
        for (i = 0; i < nports; ++i) {
            if (joystick.GetDeviceName(i, name) == B_OK) {
                if (joystick.Open(name) != B_ERROR) {
                    BString stick_name;
                    joystick.GetControllerName(&stick_name);
                    joystick.Close();

                    HAIKU_AddJoystick(name, stick_name.String());
                }
            }
        }
        return true;
    }

    static void HAIKU_JoystickClose(SDL_Joystick *joystick);

    static bool HAIKU_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
    {
        BJoystick *stick;

        // Create the joystick data structure
        joystick->hwdata = (struct joystick_hwdata *) SDL_calloc(1, sizeof(*joystick->hwdata));
        if (joystick->hwdata == NULL) {
            return false;
        }
        stick = new BJoystick;
        joystick->hwdata->stick = stick;

        // Open the requested joystick for use
        if (stick->Open(device->path) == B_ERROR) {
            HAIKU_JoystickClose(joystick);
            return SDL_SetError("Unable to open joystick");
        }

        // Set the joystick to calibrated mode
        stick->EnableCalibration();

        // Get the number of buttons, hats, and axes on the joystick
        joystick->nbuttons = stick->CountButtons();
        joystick->naxes = stick->CountAxes();
        joystick->nhats = stick->CountHats();

        joystick->hwdata->new_axes = (int16 *) SDL_calloc(joystick->naxes, sizeof(int16));
        joystick->hwdata->new_hats = (uint8 *) SDL_calloc(joystick->nhats, sizeof(uint8));
        if (!joystick->hwdata->new_hats || !joystick->hwdata->new_axes) {
            HAIKU_JoystickClose(joystick);
            return false;
        }

        // We're done!
        return true;
    }

    static void HAIKU_JoystickUpdate(SDL_Joystick *joystick)
    {
        static const Uint8 hat_map[9] = {
            SDL_HAT_CENTERED,
            SDL_HAT_UP,
            SDL_HAT_RIGHTUP,
            SDL_HAT_RIGHT,
            SDL_HAT_RIGHTDOWN,
            SDL_HAT_DOWN,
            SDL_HAT_LEFTDOWN,
            SDL_HAT_LEFT,
            SDL_HAT_LEFTUP
        };

        BJoystick *stick;
        int i;
        int16 *axes;
        uint8 *hats;
        uint32 buttons;
        Uint64 timestamp = SDL_GetTicksNS();

        // Set up data pointers
        stick = joystick->hwdata->stick;
        axes = joystick->hwdata->new_axes;
        hats = joystick->hwdata->new_hats;

        // Get the new joystick state
        stick->Update();
        stick->GetAxisValues(axes);
        stick->GetHatValues(hats);
        buttons = stick->ButtonValues();

        // Generate axis motion events
        for (i = 0; i < joystick->naxes; ++i) {
            SDL_SendJoystickAxis(timestamp, joystick, i, axes[i]);
        }

        // Generate hat change events
        for (i = 0; i < joystick->nhats; ++i) {
            SDL_SendJoystickHat(timestamp, joystick, i, hat_map[hats[i]]);
        }

        // Generate button events
        for (i = 0; i < joystick->nbuttons; ++i) {
            bool down = ((buttons & 0x01) != 0);
            SDL_SendJoystickButton(timestamp, joystick, i, down);
            buttons >>= 1;
        }
    }

    static void HAIKU_JoystickClose(SDL_Joystick *joystick)
    {
        if (joystick->hwdata) {
            joystick->hwdata->stick->Close();
            delete joystick->hwdata->stick;
            SDL_free(joystick->hwdata->new_hats);
            SDL_free(joystick->hwdata->new_axes);
            SDL_free(joystick->hwdata);
        }
    }

    // This file is compiled as C++, so don't use designated initializers here
    SDL_JoystickDriver SDL_HAIKU_JoystickDriver =
    {
        HAIKU_JoystickInit,
        NULL, // QueryDevices
        NULL, // DetectDevices
        NULL, // PrepareDevice
        NULL, // UpdateSteamVirtualGamepadSlot
        NULL, // GetGamepadMapping
        HAIKU_JoystickOpen,
        NULL, // Rumble
        NULL, // RumbleTriggers
        NULL, // SetLED
        NULL, // SendEffect
        NULL, // SetSensorsEnabled
        NULL, // SetPlayerIndex
        HAIKU_JoystickUpdate,
        HAIKU_JoystickClose,
        NULL // Quit
    };

}                              // extern "C"

#endif // SDL_JOYSTICK_HAIKU
