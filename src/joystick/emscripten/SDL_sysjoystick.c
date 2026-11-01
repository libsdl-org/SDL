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

#ifdef SDL_JOYSTICK_EMSCRIPTEN

#include <stdio.h> // For the definition of NULL
#include <emscripten/html5.h>

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../usb_ids.h"


// A linked list of available joysticks
typedef struct SDL_joylist_item
{
    int index;
    char *name;
    char *mapping;
    SDL_JoystickID instance_id;
    SDL_Joystick *joystick;
    int first_hat_button;
    int first_trigger_button;
    bool triggers_are_buttons;
    bool hat_is_axis;
    int nhats;
    SDL_GUID guid;
    int nbuttons;
    int naxes;
    double timestamp;
    double axis[64];            // !!! FIXME: don't hardcode 64 on all of these.
    double analogButton[64];
    EM_BOOL digitalButton[64];
    Uint8 hat;  // there is (currently) only ever one of these, faked from the d-pad buttons or the axis 9.
    bool rumble_available;
    bool trigger_rumble_available;
    Uint16 weak_magnitude_rumble;
    Uint16 strong_magnitude_rumble;
    Uint16 left_trigger_rumble;
    Uint16 right_trigger_rumble;
    bool removed;

    struct SDL_joylist_item *next;
} SDL_joylist_item;

typedef SDL_joylist_item joystick_hwdata;

static SDL_joylist_item *SDL_joylist = NULL;
static SDL_joylist_item *SDL_joylist_tail = NULL;

static int SDL_GetEmscriptenJoystickVendor(int device_index)
{
    return MAIN_THREAD_EM_ASM_INT({
        let gamepad = navigator['getGamepads']()[$0];
        if (!gamepad) {
            return 0;
        }

        // Chrome, Edge, Opera: Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)
        let vendor_str = 'Vendor: ';
        if (gamepad['id']['indexOf'](vendor_str) > 0) {
            let vendor_str_index = gamepad['id']['indexOf'](vendor_str) + vendor_str['length'];
            return parseInt(gamepad['id']['substr'](vendor_str_index, 4), 16);
        }

        // Firefox, Safari: 046d-c216-Logitech Dual Action (or 46d-c216-Logicool Dual Action)
        let id_split = gamepad['id']['split']('-');
        if (id_split['length'] > 1 && !isNaN(parseInt(id_split[0], 16))) {
            return parseInt(id_split[0], 16);
        }

        return 0;
    }, device_index);
}

static int SDL_GetEmscriptenJoystickProduct(int device_index)
{
    return MAIN_THREAD_EM_ASM_INT({
        let gamepad = navigator['getGamepads']()[$0];
        if (!gamepad) {
            return 0;
        }

        // Chrome, Edge, Opera: Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)
        let product_str = 'Product: ';
        if (gamepad['id']['indexOf'](product_str) > 0) {
            let product_str_index = gamepad['id']['indexOf'](product_str) + product_str['length'];
            return parseInt(gamepad['id']['substr'](product_str_index, 4), 16);
        }

        // Firefox, Safari: 046d-c216-Logitech Dual Action (or 46d-c216-Logicool Dual Action)
        let id_split = gamepad['id']['split']('-');
        if (id_split['length'] > 1 && !isNaN(parseInt(id_split[1], 16))) {
            return parseInt(id_split[1], 16);
        }

        return 0;
    }, device_index);
}

static int SDL_IsEmscriptenJoystickXInput(int device_index)
{
    return MAIN_THREAD_EM_ASM_INT({
        let gamepad = navigator['getGamepads']()[$0];
        if (!gamepad) {
            return 0;
        }

        // Chrome, Edge, Opera: Xbox 360 Controller (XInput STANDARD GAMEPAD)
        // Firefox: xinput
        // TODO: Safari
        return gamepad['id']['toLowerCase']()['indexOf']('xinput') >= 0;
    }, device_index);
}

static int SDL_GetEmscriptenOSID()
{
    return MAIN_THREAD_EM_ASM_INT({
        const os = ([
            'Android',
            'Linux',
            'iPhone',
            'Macintosh',
            'Windows',
        ]);
        const ua = navigator['userAgent'];
        for (let i = 0; i < os.length; i++) {
            if (ua['indexOf'](os[i]) >= 0) {
                return i + 1;
            }
        }
        return 0;
    });
}

EM_JS_DEPS(sdljoystick, "$stringToUTF8");

static void SDL_GetEmscriptenNormalizedName(int device_index, char *out, int length)
{
    MAIN_THREAD_EM_ASM({
        let gamepad = navigator['getGamepads']()[$0];
        if (!gamepad) {
            stringToUTF8('\0', $1, $2); // Silence the compiler here, because '' is not valid
            return;
        }

        let id = gamepad['id'];
        let output = id;
        // Chrome
        if (id['indexOf'](' (STANDARD GAMEPAD') > 0) { // Wireless Controller (STANDARD GAMEPAD Vendor: 054c Product: 09cc)
            output = id['substr'](0, id['indexOf'](' (STANDARD GAMEPAD'));
        } else if (id['indexOf'](' (Vendor:') > 0) { // usb gamepad            (Vendor: 0810 Product: e501)
            output = id['substr'](0, id['indexOf'](' (Vendor:'));
        } else if (id['indexOf'](' (XInput') > 0) { // Xbox 360 Controller (XInput STANDARD GAMEPAD)
            output = id['substr'](0, id['indexOf'](' (XInput'));
        }

        // Firefox, Safari: "046d-c216-Logitech Dual Action", "46d-c216-Logicool Dual Action", or "xinput"
        let id_split = id['split']('-');
        if (id_split['length'] > 1 && !isNaN(parseInt(id_split[0], 16))) {
            // Let's not assume the length of the vendor/product IDs in the string
            // and just find the second '-' using indexOf
            let start = id['indexOf']('-', id['indexOf']('-')+1)+1;
            output = id['substr'](start);
        }

        stringToUTF8(output.trim(), $1, $2);
    }, device_index, out, length);
}

#define IS_EQUAL_APPROX(a, b) (((a)-(b)) < 0.00001 && ((a)-(b)) > -0.00001)
static Uint8 SDL_GetHatFromEmscriptenAxis(float hat_axis)
{
    if (IS_EQUAL_APPROX(hat_axis, -7.0/7)) {
        return SDL_HAT_UP;
    } else if (IS_EQUAL_APPROX(hat_axis, -5.0/7)) {
        return SDL_HAT_UP | SDL_HAT_RIGHT;
    } else if (IS_EQUAL_APPROX(hat_axis, -3.0/7)) {
        return SDL_HAT_RIGHT;
    } else if (IS_EQUAL_APPROX(hat_axis, -1.0/7)) {
        return SDL_HAT_DOWN | SDL_HAT_RIGHT;
    } else if (IS_EQUAL_APPROX(hat_axis, 1.0/7)) {
        return SDL_HAT_DOWN;
    } else if (IS_EQUAL_APPROX(hat_axis, 3.0/7)) {
        return SDL_HAT_DOWN | SDL_HAT_LEFT;
    } else if (IS_EQUAL_APPROX(hat_axis, 5.0/7)) {
        return SDL_HAT_LEFT;
    } else if (IS_EQUAL_APPROX(hat_axis, 7.0/7)) {
        return SDL_HAT_UP | SDL_HAT_LEFT;
    } else {
        return SDL_HAT_CENTERED;
    }
}
#undef IS_EQUAL_APPROX

// Returns item matching given HTML gamepad index.
static SDL_joylist_item *GetJoylistItemByIndex(int index)
{
    SDL_joylist_item *item = SDL_joylist;

    if (index < 0) {
        return NULL;
    }

    while (item) {
        if (item->index == index && !item->removed) {
            break;
        }
        item = item->next;
    }

    return item;
}

static EM_BOOL Emscripten_JoyStickConnected(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData)
{
    SDL_joylist_item *item;
    int i;
    Uint16 bus;
    Uint16 vendor, product;
    Uint8 os_id, subtype;
    bool is_xinput;
    char name[128];

    SDL_LockJoysticks();

    if (GetJoylistItemByIndex(gamepadEvent->index) != NULL) {
        goto done;
    }

    item = (SDL_joylist_item *)SDL_malloc(sizeof(SDL_joylist_item));
    if (!item) {
        goto done;
    }

    SDL_zerop(item);
    item->index = gamepadEvent->index;

    bus = SDL_HARDWARE_BUS_UNKNOWN;
    vendor = SDL_GetEmscriptenJoystickVendor(gamepadEvent->index);
    product = SDL_GetEmscriptenJoystickProduct(gamepadEvent->index);
    is_xinput = SDL_IsEmscriptenJoystickXInput(gamepadEvent->index);
    
    os_id = SDL_GetEmscriptenOSID();
    subtype = os_id;

    item->trigger_rumble_available = MAIN_THREAD_EM_ASM_INT({
        let gamepad = navigator['getGamepads']()[$0];
        // This effect is not supported in Safari, so it's okay for us to check the vibrationActuator.effects array here for the browsers that do support it
        if (!gamepad || !gamepad['vibrationActuator'] || !gamepad['vibrationActuator']['effects'] || !gamepad['vibrationActuator']['effects']['includes']('trigger-rumble')) {
            return false;
        }
        return true;
        }, item->index);

    if (os_id != 0) {
        if (os_id == 1 || os_id == 3) { // Android or iOS (mobile)
            bus = SDL_HARDWARE_BUS_BLUETOOTH;
        } else { // Desktop
            bus = SDL_HARDWARE_BUS_USB;
        }
    }

    if (!vendor && !product && is_xinput) {
        // Use a generic VID/PID representing an XInput controller
        vendor = USB_VENDOR_MICROSOFT;
        product = USB_PRODUCT_XBOX360_XUSB_CONTROLLER;

        if (item->trigger_rumble_available) {
            // Assume Xbox One S Controller
            if (bus == SDL_HARDWARE_BUS_BLUETOOTH) {
                product = USB_PRODUCT_XBOX_ONE_S_REV1_BLUETOOTH;
            } else {
                product = USB_PRODUCT_XBOX_ONE_S;
            }            
        }
    }

    if (SDL_strcmp(gamepadEvent->mapping, "standard") == 0) {
        // We should differentiate between devices that are mapped or unmapped by the browser.
        subtype += 0x80;
    }

    SDL_GetEmscriptenNormalizedName(gamepadEvent->index, name, sizeof(name));
    item->name = SDL_CreateJoystickName(vendor, product, NULL, name);
    if (!item->name) {
        SDL_free(item);
        goto done;
    }

    if (vendor && product) {
        item->guid = SDL_CreateJoystickGUID(bus, vendor, product, 0, NULL, name, 0, subtype);
    } else {
        item->guid = SDL_CreateJoystickGUIDForName(item->name);
        item->guid.data[15] = subtype;
    }

    item->mapping = SDL_strdup(gamepadEvent->mapping);
    if (!item->mapping) {
        SDL_free(item->name);
        SDL_free(item);
        goto done;
    }

    const int real_button_count = gamepadEvent->numButtons;
    const int real_axis_count = gamepadEvent->numAxes;
    int first_trigger_button = -1;
    int first_hat_button = -1;
    int num_buttons = gamepadEvent->numButtons;
    int num_axes = gamepadEvent->numAxes;
    bool triggers_are_buttons = false;
    if ((SDL_strcmp(gamepadEvent->mapping, "standard") == 0) && (num_buttons >= 16)) {  // maps to a game console gamepad layout, turn the d-pad into a hat, treat triggers as analog.
        num_buttons -= 4;  // 4 dpad buttons become a hat.
        first_hat_button = 12;

        if (num_axes == 4) {  // Chrome gives the triggers analog button values, Firefox exposes them as extra axes. Both have the digital buttons.
            num_axes += 2;  // the two trigger "buttons"
            triggers_are_buttons = true;
        }

        // dump the digital trigger buttons in any case.
        first_trigger_button = 6;
        num_buttons -= 2;
    } else if ((SDL_strcmp(gamepadEvent->mapping, "standard") != 0) && (num_axes >= 10) && (os_id == 5)) {
        // The browsers (Chromium and Firefox) on Windows usually convert the hat on unmapped controllers to axis 9
        // We can't know for sure that this axis is a hat, but most of the time it is
        item->hat_is_axis = true;
    }

    item->first_hat_button = first_hat_button;
    item->first_trigger_button = first_trigger_button;
    item->triggers_are_buttons = triggers_are_buttons;
    item->nhats = (first_hat_button >= 0 || item->hat_is_axis) ? 1 : 0;
    item->naxes = num_axes;
    item->nbuttons = num_buttons;
    item->instance_id = SDL_GetNextObjectID();

    item->timestamp = gamepadEvent->timestamp;

    int buttonidx = 0;
    for (i = 0; i < real_button_count; i++, buttonidx++) {
        if (buttonidx == first_hat_button) {
            buttonidx += 4;  // skip these buttons, we're treating them as hat input.
        } else if (buttonidx == first_trigger_button) {
            buttonidx += 2;  // skip these buttons, we're treating them as axes.
        }
        item->analogButton[i] = gamepadEvent->analogButton[buttonidx];
        item->digitalButton[i] = gamepadEvent->digitalButton[buttonidx];
    }

    for (i = 0; i < real_axis_count; i++) {
        item->axis[i] = gamepadEvent->axis[i];
    }

    if (item->triggers_are_buttons) {
        item->axis[real_axis_count] = (gamepadEvent->analogButton[first_trigger_button] * 2.0f) - 1.0f;
        item->axis[real_axis_count+1] = (gamepadEvent->analogButton[first_trigger_button+1] * 2.0f) - 1.0f;
    }

    SDL_assert(item->nhats <= 1);  // there is (currently) only ever one of these, faked from the d-pad buttons or the axis 9.
    if (first_hat_button != -1) {
        Uint8 value = SDL_HAT_CENTERED;
        // this currently expects the first button to be up, then down, then left, then right.
        if (gamepadEvent->digitalButton[first_hat_button + 0]) {
            value |= SDL_HAT_UP;
        }
        if (gamepadEvent->digitalButton[first_hat_button + 1]) {
            value |= SDL_HAT_DOWN;
        }
        if (gamepadEvent->digitalButton[first_hat_button + 2]) {
            value |= SDL_HAT_LEFT;
        }
        if (gamepadEvent->digitalButton[first_hat_button + 3]) {
            value |= SDL_HAT_RIGHT;
        }
        item->hat = value;
    } else if (item->hat_is_axis) {
        item->hat = SDL_GetHatFromEmscriptenAxis(gamepadEvent->axis[9]);
        item->axis[9] = 0.0;
    }

    if (!SDL_joylist_tail) {
        SDL_joylist = SDL_joylist_tail = item;
    } else {
        SDL_joylist_tail->next = item;
        SDL_joylist_tail = item;
    }

    SDL_RequestJoystickDeviceUpdate(true);

#ifdef DEBUG_JOYSTICK
    SDL_Log("Added joystick with index %d", item->index);
#endif

done:
    SDL_UnlockJoysticks();

    return 1;
}

static EM_BOOL Emscripten_JoyStickDisconnected(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData)
{
    SDL_joylist_item *item;

    SDL_LockJoysticks();

    item = GetJoylistItemByIndex(gamepadEvent->index);
    if (item) {
        if (item->joystick) {
            item->joystick->hwdata = NULL;
            item->joystick = NULL;
        }

        // The item is freed in the next device query, after its device has been removed
        item->removed = true;
        SDL_RequestJoystickDeviceUpdate(true);

#ifdef DEBUG_JOYSTICK
        SDL_Log("Removed joystick with id %d", item->instance_id);
#endif
    }

    SDL_UnlockJoysticks();

    return 1;
}

static void FreeJoylistItem(SDL_joylist_item *item)
{
    SDL_free(item->mapping);
    SDL_free(item->name);
    SDL_free(item);
}

static void EMSCRIPTEN_JoystickQuit(void)
{
    SDL_joylist_item *item = NULL;
    SDL_joylist_item *next = NULL;

    for (item = SDL_joylist; item; item = next) {
        next = item->next;
        FreeJoylistItem(item);
    }

    SDL_joylist = SDL_joylist_tail = NULL;

    emscripten_set_gamepadconnected_callback(NULL, 0, NULL);
    emscripten_set_gamepaddisconnected_callback(NULL, 0, NULL);
}

static bool EMSCRIPTEN_JoystickInit(void)
{
    int rc, i, numjs;
    EmscriptenGamepadEvent gamepadState;

    rc = emscripten_sample_gamepad_data();

    // Check if gamepad is supported by browser
    if (rc == EMSCRIPTEN_RESULT_NOT_SUPPORTED) {
        return SDL_SetError("Gamepads not supported");
    }

    numjs = emscripten_get_num_gamepads();

    // handle already connected gamepads
    if (numjs > 0) {
        for (i = 0; i < numjs; i++) {
            rc = emscripten_get_gamepad_status(i, &gamepadState);
            if (rc == EMSCRIPTEN_RESULT_SUCCESS) {
                Emscripten_JoyStickConnected(EMSCRIPTEN_EVENT_GAMEPADCONNECTED,
                                             &gamepadState,
                                             NULL);
            }
        }
    }

    rc = emscripten_set_gamepadconnected_callback(NULL,
                                                  0,
                                                  Emscripten_JoyStickConnected);

    if (rc != EMSCRIPTEN_RESULT_SUCCESS) {
        EMSCRIPTEN_JoystickQuit();
        return SDL_SetError("Could not set gamepad connect callback");
    }

    rc = emscripten_set_gamepaddisconnected_callback(NULL,
                                                         0,
                                                         Emscripten_JoyStickDisconnected);
    if (rc != EMSCRIPTEN_RESULT_SUCCESS) {
        EMSCRIPTEN_JoystickQuit();
        return SDL_SetError("Could not set gamepad disconnect callback");
    }

    return true;
}

static SDL_JoystickDevice *EMSCRIPTEN_CreateJoystickDevice(SDL_joylist_item *data)
{
    SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_EMSCRIPTEN_JoystickDriver, 0);
    if (!device) {
        return NULL;
    }

    device->internal = (SDL_JoystickDeviceData *)data;
    device->instance_id = data->instance_id;
    device->unique_id = &device->instance_id;
    device->unique_id_len = sizeof(device->instance_id);

    device->name = SDL_strdup(data->name);
    if (!device->name) {
        SDL_DestroyJoystickDevice(device);
        return NULL;
    }
    device->guid = data->guid;

    return device;
}

static void FreeRemovedJoylistItems(void)
{
    SDL_joylist_item *entry = SDL_joylist;
    SDL_joylist_item *prev = NULL;

    while (entry) {
        SDL_joylist_item *next = entry->next;
        if (entry->removed) {
            if (prev) {
                prev->next = next;
            } else {
                SDL_joylist = next;
            }
            if (entry == SDL_joylist_tail) {
                SDL_joylist_tail = prev;
            }
            FreeJoylistItem(entry);
        } else {
            prev = entry;
        }
        entry = next;
    }
}

static SDL_JoystickDevice *EMSCRIPTEN_JoystickQueryDevices(void)
{
    SDL_JoystickDevice *devices = NULL;
    SDL_JoystickDevice *tail = NULL;
    SDL_joylist_item *entry;

    SDL_AssertJoysticksLocked();

    FreeRemovedJoylistItems();

    for (entry = SDL_joylist; entry; entry = entry->next) {
        SDL_JoystickDevice *device = EMSCRIPTEN_CreateJoystickDevice(entry);
        if (device) {
            if (tail) {
                tail->next = device;
            } else {
                devices = device;
            }
            tail = device;
        }
    }
    return devices;
}

static bool EMSCRIPTEN_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    SDL_joylist_item *item = (SDL_joylist_item *)device->internal;

    if (item->removed) {
        return SDL_SetError("Joystick has been disconnected");
    }

    if (item->joystick) {
        return SDL_SetError("Joystick already opened");
    }

    joystick->hwdata = (struct joystick_hwdata *)item;
    item->joystick = joystick;

    // HTML5 Gamepad API doesn't offer hats, but we can fake it from the d-pad buttons on the "standard" mapping.
    joystick->nhats = item->nhats;
    joystick->nbuttons = item->nbuttons;
    joystick->naxes = item->naxes;

    item->rumble_available = MAIN_THREAD_EM_ASM_INT({
        let gamepad = navigator['getGamepads']()[$0];
        // Don't check the vibrationActuator.effects array here, because it's not defined in Safari
        if (!gamepad || !gamepad['vibrationActuator']) {
            return false;
        }
        return true;
        }, item->index);

    if (item->rumble_available) {
        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);
    }

    // item->trigger_rumble_available is set in Emscripten_JoyStickConnected
    if (item->trigger_rumble_available) {
        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_TRIGGER_RUMBLE_BOOLEAN, true);
    }

    return true;
}

static void EMSCRIPTEN_JoystickUpdate(SDL_Joystick *joystick)
{
    EmscriptenGamepadEvent gamepadState;
    SDL_joylist_item *item = (SDL_joylist_item *)joystick->hwdata;
    int i, result;
    Uint64 timestamp = SDL_GetTicksNS();

    emscripten_sample_gamepad_data();

    if (item) {
        result = emscripten_get_gamepad_status(item->index, &gamepadState);
        if (result == EMSCRIPTEN_RESULT_SUCCESS) {
            if (gamepadState.timestamp == 0 || gamepadState.timestamp != item->timestamp) {
                const int first_hat_button = item->first_hat_button;
                const int first_trigger_button = item->first_trigger_button;
                const int real_button_count = gamepadState.numButtons;
                const int real_axis_count = gamepadState.numAxes;

                int buttonidx = 0;
                for (i = 0; i < real_button_count; i++, buttonidx++) {
                    if (buttonidx == first_hat_button) {
                        buttonidx += 4;  // skip these buttons, we're treating them as hat input.
                    } else if (buttonidx == first_trigger_button) {
                        buttonidx += 2;  // skip these buttons, we're treating them as axes.
                    }
                    if (item->digitalButton[i] != gamepadState.digitalButton[buttonidx]) {
                        const bool down = (gamepadState.digitalButton[buttonidx] != 0);
                        SDL_SendJoystickButton(timestamp, item->joystick, i, down);
                    }

                    // store values to compare them in the next update
                    item->analogButton[i] = gamepadState.analogButton[buttonidx];
                    item->digitalButton[i] = gamepadState.digitalButton[buttonidx];
                }

                for (i = 0; i < real_axis_count; i++) {
                    if (item->hat_is_axis && i == 9) {
                        continue;  // this axis is a hat
                    }
                    if (item->axis[i] != gamepadState.axis[i]) {
                        SDL_SendJoystickAxis(timestamp, item->joystick, i, (Sint16)(32767.0f * gamepadState.axis[i]));
                        item->axis[i] = gamepadState.axis[i];
                    }
                }

                if (item->triggers_are_buttons) {
                    for (i = 0; i < 2; i++) {
                        if (item->axis[real_axis_count+i] != gamepadState.analogButton[first_trigger_button+i]) {
                            SDL_SendJoystickAxis(timestamp, item->joystick, real_axis_count+i, (Sint16)(32767.0f * ((gamepadState.analogButton[first_trigger_button+i] * 2.0f) - 1.0f)));
                            item->axis[real_axis_count+i] = gamepadState.analogButton[first_trigger_button+i];
                        }
                    }
                }

                SDL_assert(item->nhats <= 1);  // there is (currently) only ever one of these, faked from the d-pad buttons.
                if (item->nhats) {
                    Uint8 value = SDL_HAT_CENTERED;
                    if (first_hat_button >= 0) {
                        // this currently expects the first button to be up, then down, then left, then right.
                        if (gamepadState.digitalButton[first_hat_button + 0]) {
                            value |= SDL_HAT_UP;
                        } else if (gamepadState.digitalButton[first_hat_button + 1]) {
                            value |= SDL_HAT_DOWN;
                        }
                        if (gamepadState.digitalButton[first_hat_button + 2]) {
                            value |= SDL_HAT_LEFT;
                        } else if (gamepadState.digitalButton[first_hat_button + 3]) {
                            value |= SDL_HAT_RIGHT;
                        }
                    } else if (item->hat_is_axis) {
                        value = SDL_GetHatFromEmscriptenAxis(gamepadState.axis[9]);
                    }
                    if (item->hat != value) {
                        item->hat = value;
                        SDL_SendJoystickHat(timestamp, item->joystick, 0, value);
                    }
                }

                item->timestamp = gamepadState.timestamp;
            }
        }
    }
}

static void EMSCRIPTEN_JoystickClose(SDL_Joystick *joystick)
{
    SDL_joylist_item *item = (SDL_joylist_item *)joystick->hwdata;
    if (item) {
        item->joystick = NULL;
    }
}

static bool Emscripten_UpdateRumble(SDL_joylist_item *item)
{
    bool result = MAIN_THREAD_EM_ASM_INT({
        let gamepad = navigator['getGamepads']()[$0];
        if (!gamepad) {
            return false;
        }
        // We check if rumble is available in EMSCRIPTEN_JoystickRumble() and EMSCRIPTEN_JoystickRumbleTriggers().
        // From my testing using "dual-rumble" here covers both main rumble and trigger rumble.
        gamepad['vibrationActuator']['playEffect']('dual-rumble', {
            'startDelay': 0,
            'duration': 3000,
            'weakMagnitude': $1 / 0xFFFF,
            'strongMagnitude': $2 / 0xFFFF,
            'leftTrigger': $3 / 0xFFFF,
            'rightTrigger': $4 / 0xFFFF,
        });

        return true;
        }, item->index, item->weak_magnitude_rumble, item->strong_magnitude_rumble, item->left_trigger_rumble, item->right_trigger_rumble);
    return result;
}

static bool EMSCRIPTEN_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    SDL_joylist_item *item = (SDL_joylist_item *)joystick->hwdata;
    if (!item || !item->rumble_available) {
        return SDL_Unsupported();
    }
    item->strong_magnitude_rumble = low_frequency_rumble;
    item->weak_magnitude_rumble = high_frequency_rumble;
    return Emscripten_UpdateRumble(item);
}

static bool EMSCRIPTEN_JoystickRumbleTriggers(SDL_Joystick *joystick, Uint16 left_rumble, Uint16 right_rumble)
{
    SDL_joylist_item *item = (SDL_joylist_item *)joystick->hwdata;
    if (!item || !item->trigger_rumble_available) {
        return SDL_Unsupported();
    }
    item->left_trigger_rumble = left_rumble;
    item->right_trigger_rumble = right_rumble;
    return Emscripten_UpdateRumble(item);
}

SDL_JoystickDriver SDL_EMSCRIPTEN_JoystickDriver = {
    .Init = EMSCRIPTEN_JoystickInit,
    .QueryDevices = EMSCRIPTEN_JoystickQueryDevices,
    .Open = EMSCRIPTEN_JoystickOpen,
    .Rumble = EMSCRIPTEN_JoystickRumble,
    .RumbleTriggers = EMSCRIPTEN_JoystickRumbleTriggers,
    .Update = EMSCRIPTEN_JoystickUpdate,
    .Close = EMSCRIPTEN_JoystickClose,
    .Quit = EMSCRIPTEN_JoystickQuit
};

#endif // SDL_JOYSTICK_EMSCRIPTEN
