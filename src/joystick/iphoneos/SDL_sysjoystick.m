/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2019 Sam Lantinga <slouken@libsdl.org>

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

/* This is the iOS implementation of the SDL joystick API */
#include "SDL_sysjoystick_c.h"

/* needed for SDL_IPHONE_MAX_GFORCE macro */
#include "SDL_config_iphoneos.h"

#include "SDL_assert.h"
#include "SDL_events.h"
#include "SDL_joystick.h"
#include "SDL_hints.h"
#include "SDL_stdinc.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"


#if !SDL_EVENTS_DISABLED
#include "../../events/SDL_events_c.h"
#endif

#if !TARGET_OS_TV
#import <CoreMotion/CoreMotion.h>
#endif

#ifdef SDL_JOYSTICK_MFI
#import <GameController/GameController.h>

static id connectObserver = nil;
static id disconnectObserver = nil;

#include <Availability.h>
#include <objc/message.h>

/* remove compilation warnings for strict builds by defining these selectors, even though
 * they are only ever used indirectly through objc_msgSend
 */
@interface GCExtendedGamepad (SDL)
#if (__IPHONE_OS_VERSION_MAX_ALLOWED < 121000) || (__APPLETV_OS_VERSION_MAX_ALLOWED < 121000) || (__MAC_OS_VERSION_MAX_ALLOWED < 1401000)
@property (nonatomic, readonly, nullable) GCControllerButtonInput *leftThumbstickButton;
@property (nonatomic, readonly, nullable) GCControllerButtonInput *rightThumbstickButton;
#endif
#if (__IPHONE_OS_VERSION_MAX_ALLOWED < 130000) || (__APPLETV_OS_VERSION_MAX_ALLOWED < 130000) || (__MAC_OS_VERSION_MAX_ALLOWED < 1500000)
@property (nonatomic, readonly) GCControllerButtonInput *buttonMenu;
@property (nonatomic, readonly, nullable) GCControllerButtonInput *buttonOptions;
#endif
@end
@interface GCMicroGamepad (SDL)
#if (__IPHONE_OS_VERSION_MAX_ALLOWED < 130000) || (__APPLETV_OS_VERSION_MAX_ALLOWED < 130000) || (__MAC_OS_VERSION_MAX_ALLOWED < 1500000)
@property (nonatomic, readonly) GCControllerButtonInput *buttonMenu;
#endif
@end

#endif /* SDL_JOYSTICK_MFI */

#if !TARGET_OS_TV
static const char *accelerometerName = "iOS Accelerometer";
static CMMotionManager *motionManager = nil;
#endif /* !TARGET_OS_TV */

static SDL_JoystickDeviceItem *deviceList = NULL;

static int numjoysticks = 0;
int SDL_AppleTVRemoteOpenedAsJoystick = 0;

static SDL_JoystickDeviceItem *
GetDeviceForIndex(int device_index)
{
    SDL_JoystickDeviceItem *device = deviceList;
    int i = 0;

    while (i < device_index) {
        if (device == NULL) {
            return NULL;
        }
        device = device->next;
        i++;
    }

    return device;
}

static void
IOS_AddMFIJoystickDevice(SDL_JoystickDeviceItem *device, GCController *controller)
{
#ifdef SDL_JOYSTICK_MFI
    const Uint16 VENDOR_APPLE = 0x05AC;
    const Uint16 VENDOR_MICROSOFT = 0x045e;
    const Uint16 VENDOR_SONY = 0x054C;
    Uint16 *guid16 = (Uint16 *)device->guid.data;
    Uint16 vendor = 0;
    Uint16 product = 0;
    Uint8 subtype = 0;

    const char *name = NULL;
    /* Explicitly retain the controller because SDL_JoystickDeviceItem is a
     * struct, and ARC doesn't work with structs. */
    device->controller = (__bridge GCController *) CFBridgingRetain(controller);

    if (controller.vendorName) {
        name = controller.vendorName.UTF8String;
    }

    if (!name) {
        name = "MFi Gamepad";
    }

    device->name = SDL_strdup(name);

    if (controller.extendedGamepad) {
        GCExtendedGamepad *gamepad = controller.extendedGamepad;
        BOOL is_xbox = [controller.vendorName containsString: @"Xbox"];
        BOOL is_ps4 = [controller.vendorName containsString: @"DUALSHOCK"];
        BOOL is_MFi = (!is_xbox && !is_ps4);
        int nbuttons = 0;

        /* These buttons are part of the original MFi spec */
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_A);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_B);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_X);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_Y);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        nbuttons += 6;

        /* These buttons are available on some newer controllers */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
        if ([gamepad respondsToSelector:@selector(leftThumbstickButton)] && gamepad.leftThumbstickButton) {
            device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_LEFTSTICK);
            ++nbuttons;
        }
        if ([gamepad respondsToSelector:@selector(rightThumbstickButton)] && gamepad.rightThumbstickButton) {
            device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_RIGHTSTICK);
            ++nbuttons;
        }
        if ([gamepad respondsToSelector:@selector(buttonOptions)] && gamepad.buttonOptions) {
            device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_BACK);
            ++nbuttons;
        }
        BOOL has_direct_menu = [gamepad respondsToSelector:@selector(buttonMenu)] && gamepad.buttonMenu;
#if TARGET_OS_TV
        /* On tvOS MFi controller menu button brings you to the home screen */
        if (is_MFi) {
            has_direct_menu = FALSE;
        }
#endif
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_START);
        ++nbuttons;
        if (!has_direct_menu) {
            device->uses_pause_handler = SDL_TRUE;
        }
#pragma clang diagnostic pop

        if (is_xbox) {
            vendor = VENDOR_MICROSOFT;
            product = 0x02E0; /* Assume Xbox One S BLE Controller unless/until GCController flows VID/PID */
        } else if (is_ps4) {
            vendor = VENDOR_SONY;
            product = 0x09CC; /* Assume DS4 Slim unless/until GCController flows VID/PID */
        } else {
            vendor = VENDOR_APPLE;
            product = 1;
            subtype = 1;
        }

        device->naxes = 6; /* 2 thumbsticks and 2 triggers */
        device->nhats = 1; /* d-pad */
        device->nbuttons = nbuttons;

    } else if (controller.gamepad) {
        int nbuttons = 0;

        /* These buttons are part of the original MFi spec */
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_A);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_B);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_X);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_Y);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_START);
        nbuttons += 7;
        device->uses_pause_handler = SDL_TRUE;

        vendor = VENDOR_APPLE;
        product = 2;
        subtype = 2;
        device->naxes = 0; /* no traditional analog inputs */
        device->nhats = 1; /* d-pad */
        device->nbuttons = nbuttons;
    }
#if TARGET_OS_TV
    else if (controller.microGamepad) {
        GCMicroGamepad *gamepad = controller.microGamepad;
        int nbuttons = 0;

        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_A);
        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_B); /* Button X on microGamepad */
        nbuttons += 2;

        device->button_mask |= (1 << SDL_CONTROLLER_BUTTON_START);
        ++nbuttons;
        device->uses_pause_handler = SDL_TRUE;

        vendor = VENDOR_APPLE;
        product = 3;
        subtype = 3;
        device->naxes = 2; /* treat the touch surface as two axes */
        device->nhats = 0; /* apparently the touch surface-as-dpad is buggy */
        device->nbuttons = nbuttons;

        controller.microGamepad.allowsRotation = SDL_GetHintBoolean(SDL_HINT_APPLE_TV_REMOTE_ALLOW_ROTATION, SDL_FALSE);
    }
#endif /* TARGET_OS_TV */

    /* We only need 16 bits for each of these; space them out to fill 128. */
    /* Byteswap so devices get same GUID on little/big endian platforms. */
    *guid16++ = SDL_SwapLE16(SDL_HARDWARE_BUS_BLUETOOTH);
    *guid16++ = 0;
    *guid16++ = SDL_SwapLE16(vendor);
    *guid16++ = 0;
    *guid16++ = SDL_SwapLE16(product);
    *guid16++ = 0;

    *guid16++ = SDL_SwapLE16(device->button_mask);

    if (subtype != 0) {
        /* Note that this is an MFI controller and what subtype it is */
        device->guid.data[14] = 'm';
        device->guid.data[15] = subtype;
    }

    /* This will be set when the first button press of the controller is
     * detected. */
    controller.playerIndex = -1;

#endif /* SDL_JOYSTICK_MFI */
}

static void
IOS_AddJoystickDevice(GCController *controller, SDL_bool accelerometer)
{
    SDL_JoystickDeviceItem *device = deviceList;

#if TARGET_OS_TV
    if (!SDL_GetHintBoolean(SDL_HINT_TV_REMOTE_AS_JOYSTICK, SDL_TRUE)) {
        /* Ignore devices that aren't actually controllers (e.g. remotes), they'll be handled as keyboard input */
        if (controller && !controller.extendedGamepad && !controller.gamepad && controller.microGamepad) {
            return;
        }
    }
#endif

    while (device != NULL) {
        if (device->controller == controller) {
            return;
        }
        device = device->next;
    }

    device = (SDL_JoystickDeviceItem *) SDL_calloc(1, sizeof(SDL_JoystickDeviceItem));
    if (device == NULL) {
        return;
    }

    device->accelerometer = accelerometer;
    device->instance_id = SDL_GetNextJoystickInstanceID();

    if (accelerometer) {
#if TARGET_OS_TV
        SDL_free(device);
        return;
#else
        device->name = SDL_strdup(accelerometerName);
        device->naxes = 3; /* Device acceleration in the x, y, and z axes. */
        device->nhats = 0;
        device->nbuttons = 0;

        /* Use the accelerometer name as a GUID. */
        SDL_memcpy(&device->guid.data, device->name, SDL_min(sizeof(SDL_JoystickGUID), SDL_strlen(device->name)));
#endif /* TARGET_OS_TV */
    } else if (controller) {
        IOS_AddMFIJoystickDevice(device, controller);
    }

    if (deviceList == NULL) {
        deviceList = device;
    } else {
        SDL_JoystickDeviceItem *lastdevice = deviceList;
        while (lastdevice->next != NULL) {
            lastdevice = lastdevice->next;
        }
        lastdevice->next = device;
    }

    ++numjoysticks;

    SDL_PrivateJoystickAdded(device->instance_id);
}

static SDL_JoystickDeviceItem *
IOS_RemoveJoystickDevice(SDL_JoystickDeviceItem *device)
{
    SDL_JoystickDeviceItem *prev = NULL;
    SDL_JoystickDeviceItem *next = NULL;
    SDL_JoystickDeviceItem *item = deviceList;

    if (device == NULL) {
        return NULL;
    }

    next = device->next;

    while (item != NULL) {
        if (item == device) {
            break;
        }
        prev = item;
        item = item->next;
    }

    /* Unlink the device item from the device list. */
    if (prev) {
        prev->next = device->next;
    } else if (device == deviceList) {
        deviceList = device->next;
    }

    if (device->joystick) {
        device->joystick->hwdata = NULL;
    }

#ifdef SDL_JOYSTICK_MFI
    @autoreleasepool {
        if (device->controller) {
            /* The controller was explicitly retained in the struct, so it
             * should be explicitly released before freeing the struct. */
            GCController *controller = CFBridgingRelease((__bridge CFTypeRef)(device->controller));
            controller.controllerPausedHandler = nil;
            device->controller = nil;
        }
    }
#endif /* SDL_JOYSTICK_MFI */

    --numjoysticks;

    SDL_PrivateJoystickRemoved(device->instance_id);

    SDL_free(device->name);
    SDL_free(device);

    return next;
}

#if TARGET_OS_TV
static void SDLCALL
SDL_AppleTVRemoteRotationHintChanged(void *udata, const char *name, const char *oldValue, const char *newValue)
{
    BOOL allowRotation = newValue != NULL && *newValue != '0';

    @autoreleasepool {
        for (GCController *controller in [GCController controllers]) {
            if (controller.microGamepad) {
                controller.microGamepad.allowsRotation = allowRotation;
            }
        }
    }
}
#endif /* TARGET_OS_TV */

static int
IOS_JoystickInit(void)
{
    @autoreleasepool {
        NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

#if !TARGET_OS_TV
        if (SDL_GetHintBoolean(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, SDL_TRUE)) {
            /* Default behavior, accelerometer as joystick */
            IOS_AddJoystickDevice(nil, SDL_TRUE);
        }
#endif /* !TARGET_OS_TV */

#ifdef SDL_JOYSTICK_MFI
        /* GameController.framework was added in iOS 7. */
        if (![GCController class]) {
            return 0;
        }

        for (GCController *controller in [GCController controllers]) {
            IOS_AddJoystickDevice(controller, SDL_FALSE);
        }

#if TARGET_OS_TV
        SDL_AddHintCallback(SDL_HINT_APPLE_TV_REMOTE_ALLOW_ROTATION,
                            SDL_AppleTVRemoteRotationHintChanged, NULL);
#endif /* TARGET_OS_TV */

        connectObserver = [center addObserverForName:GCControllerDidConnectNotification
                                              object:nil
                                               queue:nil
                                          usingBlock:^(NSNotification *note) {
                                              GCController *controller = note.object;
                                              IOS_AddJoystickDevice(controller, SDL_FALSE);
                                          }];

        disconnectObserver = [center addObserverForName:GCControllerDidDisconnectNotification
                                                 object:nil
                                                  queue:nil
                                             usingBlock:^(NSNotification *note) {
                                                 GCController *controller = note.object;
                                                 SDL_JoystickDeviceItem *device = deviceList;
                                                 while (device != NULL) {
                                                     if (device->controller == controller) {
                                                         IOS_RemoveJoystickDevice(device);
                                                         break;
                                                     }
                                                     device = device->next;
                                                 }
                                             }];
#endif /* SDL_JOYSTICK_MFI */
    }

    return 0;
}

static int
IOS_JoystickGetCount(void)
{
    return numjoysticks;
}

static void
IOS_JoystickDetect(void)
{
}

static const char *
IOS_JoystickGetDeviceName(int device_index)
{
    SDL_JoystickDeviceItem *device = GetDeviceForIndex(device_index);
    return device ? device->name : "Unknown";
}

static int
IOS_JoystickGetDevicePlayerIndex(int device_index)
{
    SDL_JoystickDeviceItem *device = GetDeviceForIndex(device_index);
    return device ? (int)device->controller.playerIndex : -1;
}

static void
IOS_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
    SDL_JoystickDeviceItem *device = GetDeviceForIndex(device_index);
    if (device) {
        device->controller.playerIndex = player_index;
    }
}

static SDL_JoystickGUID
IOS_JoystickGetDeviceGUID( int device_index )
{
    SDL_JoystickDeviceItem *device = GetDeviceForIndex(device_index);
    SDL_JoystickGUID guid;
    if (device) {
        guid = device->guid;
    } else {
        SDL_zero(guid);
    }
    return guid;
}

static SDL_JoystickID
IOS_JoystickGetDeviceInstanceID(int device_index)
{
    SDL_JoystickDeviceItem *device = GetDeviceForIndex(device_index);
    return device ? device->instance_id : -1;
}

static int
IOS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    SDL_JoystickDeviceItem *device = GetDeviceForIndex(device_index);
    if (device == NULL) {
        return SDL_SetError("Could not open Joystick: no hardware device for the specified index");
    }

    joystick->hwdata = device;
    joystick->instance_id = device->instance_id;

    joystick->naxes = device->naxes;
    joystick->nhats = device->nhats;
    joystick->nbuttons = device->nbuttons;
    joystick->nballs = 0;

    device->joystick = joystick;

    @autoreleasepool {
        if (device->accelerometer) {
#if !TARGET_OS_TV
            if (motionManager == nil) {
                motionManager = [[CMMotionManager alloc] init];
            }

            /* Shorter times between updates can significantly increase CPU usage. */
            motionManager.accelerometerUpdateInterval = 0.1;
            [motionManager startAccelerometerUpdates];
#endif /* !TARGET_OS_TV */
        } else {
#ifdef SDL_JOYSTICK_MFI
            if (device->uses_pause_handler) {
                GCController *controller = device->controller;
                controller.controllerPausedHandler = ^(GCController *c) {
                    if (joystick->hwdata) {
                        ++joystick->hwdata->num_pause_presses;
                    }
                };
            }
#endif /* SDL_JOYSTICK_MFI */
        }
    }
    if (device->remote) {
        ++SDL_AppleTVRemoteOpenedAsJoystick;
    }

    return 0;
}

static void
IOS_AccelerometerUpdate(SDL_Joystick * joystick)
{
#if !TARGET_OS_TV
    const float maxgforce = SDL_IPHONE_MAX_GFORCE;
    const SInt16 maxsint16 = 0x7FFF;
    CMAcceleration accel;

    @autoreleasepool {
        if (!motionManager.isAccelerometerActive) {
            return;
        }

        accel = motionManager.accelerometerData.acceleration;
    }

    /*
     Convert accelerometer data from floating point to Sint16, which is what
     the joystick system expects.

     To do the conversion, the data is first clamped onto the interval
     [-SDL_IPHONE_MAX_G_FORCE, SDL_IPHONE_MAX_G_FORCE], then the data is multiplied
     by MAX_SINT16 so that it is mapped to the full range of an Sint16.

     You can customize the clamped range of this function by modifying the
     SDL_IPHONE_MAX_GFORCE macro in SDL_config_iphoneos.h.

     Once converted to Sint16, the accelerometer data no longer has coherent
     units. You can convert the data back to units of g-force by multiplying
     it in your application's code by SDL_IPHONE_MAX_GFORCE / 0x7FFF.
     */

    /* clamp the data */
    accel.x = SDL_min(SDL_max(accel.x, -maxgforce), maxgforce);
    accel.y = SDL_min(SDL_max(accel.y, -maxgforce), maxgforce);
    accel.z = SDL_min(SDL_max(accel.z, -maxgforce), maxgforce);

    /* pass in data mapped to range of SInt16 */
    SDL_PrivateJoystickAxis(joystick, 0,  (accel.x / maxgforce) * maxsint16);
    SDL_PrivateJoystickAxis(joystick, 1, -(accel.y / maxgforce) * maxsint16);
    SDL_PrivateJoystickAxis(joystick, 2,  (accel.z / maxgforce) * maxsint16);
#endif /* !TARGET_OS_TV */
}

#ifdef SDL_JOYSTICK_MFI
static Uint8
IOS_MFIJoystickHatStateForDPad(GCControllerDirectionPad *dpad)
{
    Uint8 hat = 0;

    if (dpad.up.isPressed) {
        hat |= SDL_HAT_UP;
    } else if (dpad.down.isPressed) {
        hat |= SDL_HAT_DOWN;
    }

    if (dpad.left.isPressed) {
        hat |= SDL_HAT_LEFT;
    } else if (dpad.right.isPressed) {
        hat |= SDL_HAT_RIGHT;
    }

    if (hat == 0) {
        return SDL_HAT_CENTERED;
    }

    return hat;
}
#endif

static void
IOS_MFIJoystickUpdate(SDL_Joystick * joystick)
{
#if SDL_JOYSTICK_MFI
    @autoreleasepool {
        GCController *controller = joystick->hwdata->controller;
        Uint8 hatstate = SDL_HAT_CENTERED;
        int i;
        int pause_button_index = 0;

        if (controller.extendedGamepad) {
            GCExtendedGamepad *gamepad = controller.extendedGamepad;

            /* Axis order matches the XInput Windows mappings. */
            Sint16 axes[] = {
                (Sint16) (gamepad.leftThumbstick.xAxis.value * 32767),
                (Sint16) (gamepad.leftThumbstick.yAxis.value * -32767),
                (Sint16) ((gamepad.leftTrigger.value * 65535) - 32768),
                (Sint16) (gamepad.rightThumbstick.xAxis.value * 32767),
                (Sint16) (gamepad.rightThumbstick.yAxis.value * -32767),
                (Sint16) ((gamepad.rightTrigger.value * 65535) - 32768),
            };

            /* Button order matches the XInput Windows mappings. */
            Uint8 buttons[joystick->nbuttons];
            int button_count = 0;

            /* These buttons are part of the original MFi spec */
            buttons[button_count++] = gamepad.buttonA.isPressed;
            buttons[button_count++] = gamepad.buttonB.isPressed;
            buttons[button_count++] = gamepad.buttonX.isPressed;
            buttons[button_count++] = gamepad.buttonY.isPressed;
            buttons[button_count++] = gamepad.leftShoulder.isPressed;
            buttons[button_count++] = gamepad.rightShoulder.isPressed;

            /* These buttons are available on some newer controllers */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
            if (joystick->hwdata->button_mask & (1 << SDL_CONTROLLER_BUTTON_LEFTSTICK)) {
                buttons[button_count++] = gamepad.leftThumbstickButton.isPressed;
            }
            if (joystick->hwdata->button_mask & (1 << SDL_CONTROLLER_BUTTON_RIGHTSTICK)) {
                buttons[button_count++] = gamepad.rightThumbstickButton.isPressed;
            }
            if (joystick->hwdata->button_mask & (1 << SDL_CONTROLLER_BUTTON_BACK)) {
                buttons[button_count++] = gamepad.buttonOptions.isPressed;
            }
            /* This must be the last button, so we can optionally handle it with pause_button_index below */
            if (joystick->hwdata->button_mask & (1 << SDL_CONTROLLER_BUTTON_START)) {
                if (joystick->hwdata->uses_pause_handler) {
                    pause_button_index = button_count;
                    buttons[button_count++] = joystick->delayed_guide_button;
                } else {
                    buttons[button_count++] = gamepad.buttonMenu.isPressed;
                }
            }
#pragma clang diagnostic pop

            hatstate = IOS_MFIJoystickHatStateForDPad(gamepad.dpad);

            for (i = 0; i < SDL_arraysize(axes); i++) {
                SDL_PrivateJoystickAxis(joystick, i, axes[i]);
            }

            for (i = 0; i < button_count; i++) {
                SDL_PrivateJoystickButton(joystick, i, buttons[i]);
            }
        } else if (controller.gamepad) {
            GCGamepad *gamepad = controller.gamepad;

            /* Button order matches the XInput Windows mappings. */
            Uint8 buttons[joystick->nbuttons];
            int button_count = 0;
            buttons[button_count++] = gamepad.buttonA.isPressed;
            buttons[button_count++] = gamepad.buttonB.isPressed;
            buttons[button_count++] = gamepad.buttonX.isPressed;
            buttons[button_count++] = gamepad.buttonY.isPressed;
            buttons[button_count++] = gamepad.leftShoulder.isPressed;
            buttons[button_count++] = gamepad.rightShoulder.isPressed;
            pause_button_index = button_count;
            buttons[button_count++] = joystick->delayed_guide_button;

            hatstate = IOS_MFIJoystickHatStateForDPad(gamepad.dpad);

            for (i = 0; i < button_count; i++) {
                SDL_PrivateJoystickButton(joystick, i, buttons[i]);
            }
        }
#if TARGET_OS_TV
        else if (controller.microGamepad) {
            GCMicroGamepad *gamepad = controller.microGamepad;

            Sint16 axes[] = {
                (Sint16) (gamepad.dpad.xAxis.value * 32767),
                (Sint16) (gamepad.dpad.yAxis.value * -32767),
            };

            for (i = 0; i < SDL_arraysize(axes); i++) {
                SDL_PrivateJoystickAxis(joystick, i, axes[i]);
            }

            Uint8 buttons[joystick->nbuttons];
            int button_count = 0;
            buttons[button_count++] = gamepad.buttonA.isPressed;
            buttons[button_count++] = gamepad.buttonX.isPressed;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
            /* This must be the last button, so we can optionally handle it with pause_button_index below */
            if (joystick->hwdata->button_mask & (1 << SDL_CONTROLLER_BUTTON_START)) {
                if (joystick->hwdata->uses_pause_handler) {
                    pause_button_index = button_count;
                    buttons[button_count++] = joystick->delayed_guide_button;
                } else {
                    buttons[button_count++] = gamepad.buttonMenu.isPressed;
                }
            }
#pragma clang diagnostic pop

            for (i = 0; i < button_count; i++) {
                SDL_PrivateJoystickButton(joystick, i, buttons[i]);
            }
        }
#endif /* TARGET_OS_TV */

        if (joystick->nhats > 0) {
            SDL_PrivateJoystickHat(joystick, 0, hatstate);
        }

        if (joystick->hwdata->uses_pause_handler) {
            for (i = 0; i < joystick->hwdata->num_pause_presses; i++) {
                SDL_PrivateJoystickButton(joystick, pause_button_index, SDL_PRESSED);
                SDL_PrivateJoystickButton(joystick, pause_button_index, SDL_RELEASED);
            }
            joystick->hwdata->num_pause_presses = 0;
        }
    }
#endif /* SDL_JOYSTICK_MFI */
}

static int
IOS_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    return SDL_Unsupported();
}

static void
IOS_JoystickUpdate(SDL_Joystick * joystick)
{
    SDL_JoystickDeviceItem *device = joystick->hwdata;

    if (device == NULL) {
        return;
    }
    
    if (device->accelerometer) {
        IOS_AccelerometerUpdate(joystick);
    } else if (device->controller) {
        IOS_MFIJoystickUpdate(joystick);
    }
}

static void
IOS_JoystickClose(SDL_Joystick * joystick)
{
    SDL_JoystickDeviceItem *device = joystick->hwdata;

    if (device == NULL) {
        return;
    }

    device->joystick = NULL;

    @autoreleasepool {
        if (device->accelerometer) {
#if !TARGET_OS_TV
            [motionManager stopAccelerometerUpdates];
#endif /* !TARGET_OS_TV */
        } else if (device->controller) {
#ifdef SDL_JOYSTICK_MFI
            GCController *controller = device->controller;
            controller.controllerPausedHandler = nil;
            controller.playerIndex = -1;
#endif
        }
    }
    if (device->remote) {
        --SDL_AppleTVRemoteOpenedAsJoystick;
    }
}

static void
IOS_JoystickQuit(void)
{
    @autoreleasepool {
#ifdef SDL_JOYSTICK_MFI
        NSNotificationCenter *center = [NSNotificationCenter defaultCenter];

        if (connectObserver) {
            [center removeObserver:connectObserver name:GCControllerDidConnectNotification object:nil];
            connectObserver = nil;
        }

        if (disconnectObserver) {
            [center removeObserver:disconnectObserver name:GCControllerDidDisconnectNotification object:nil];
            disconnectObserver = nil;
        }

#if TARGET_OS_TV
        SDL_DelHintCallback(SDL_HINT_APPLE_TV_REMOTE_ALLOW_ROTATION,
                            SDL_AppleTVRemoteRotationHintChanged, NULL);
#endif /* TARGET_OS_TV */
#endif /* SDL_JOYSTICK_MFI */

        while (deviceList != NULL) {
            IOS_RemoveJoystickDevice(deviceList);
        }

#if !TARGET_OS_TV
        motionManager = nil;
#endif /* !TARGET_OS_TV */
    }

    numjoysticks = 0;
}

SDL_JoystickDriver SDL_IOS_JoystickDriver =
{
    IOS_JoystickInit,
    IOS_JoystickGetCount,
    IOS_JoystickDetect,
    IOS_JoystickGetDeviceName,
    IOS_JoystickGetDevicePlayerIndex,
    IOS_JoystickSetDevicePlayerIndex,
    IOS_JoystickGetDeviceGUID,
    IOS_JoystickGetDeviceInstanceID,
    IOS_JoystickOpen,
    IOS_JoystickRumble,
    IOS_JoystickUpdate,
    IOS_JoystickClose,
    IOS_JoystickQuit,
};

/* vi: set ts=4 sw=4 expandtab: */
