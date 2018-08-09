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

#include "SDL_endian.h"
#include "SDL_hints.h"
#include "SDL_log.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"


struct joystick_hwdata
{
    SDL_HIDAPI_DeviceDriver *driver;
    void *context;

    hid_device *dev;
};

typedef struct _SDL_HIDAPI_Device
{
    SDL_JoystickID instance_id;
    char *name;
    char *path;
    Uint16 vendor_id;
    Uint16 product_id;
    SDL_JoystickGUID guid;
    int interface_number;   /* Available on Windows and Linux */
    Uint16 usage_page;      /* Available on Windows and Mac OS X */
    Uint16 usage;           /* Available on Windows and Mac OS X */
    SDL_HIDAPI_DeviceDriver *driver;

    /* Used during scanning for device changes */
    SDL_bool seen;

    struct _SDL_HIDAPI_Device *next;
} SDL_HIDAPI_Device;

static SDL_HIDAPI_DeviceDriver *SDL_HIDAPI_drivers[] = {
#ifdef SDL_JOYSTICK_HIDAPI_PS4
    &SDL_HIDAPI_DriverPS4,
#endif
#ifdef SDL_JOYSTICK_HIDAPI_STEAM
    &SDL_HIDAPI_DriverSteam,
#endif
#ifdef SDL_JOYSTICK_HIDAPI_SWITCH
    &SDL_HIDAPI_DriverSwitch,
#endif
#ifdef SDL_JOYSTICK_HIDAPI_XBOX360
    &SDL_HIDAPI_DriverXbox360,
#endif
#ifdef SDL_JOYSTICK_HIDAPI_XBOXONE
    &SDL_HIDAPI_DriverXboxOne,
#endif
};
static SDL_HIDAPI_Device *SDL_HIDAPI_devices;
static int SDL_HIDAPI_numjoysticks = 0;
static Uint32 SDL_HIDAPI_last_detect = 0;

static SDL_bool
HIDAPI_IsDeviceSupported(Uint16 vendor_id, Uint16 product_id)
{
    int i;

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled && driver->IsSupportedDevice(vendor_id, product_id, -1, 0, 0)) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static SDL_HIDAPI_DeviceDriver *
HIDAPI_GetDeviceDriver(SDL_HIDAPI_Device *device)
{
    int i;

    if (SDL_ShouldIgnoreJoystick(device->name, device->guid)) {
        return NULL;
    }

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled && driver->IsSupportedDevice(device->vendor_id, device->product_id, device->interface_number, device->usage_page, device->usage)) {
            return driver;
        }
    }
    return NULL;
}

static SDL_HIDAPI_Device *
HIDAPI_GetJoystickByIndex(int device_index)
{
    SDL_HIDAPI_Device *device = SDL_HIDAPI_devices;
    while (device) {
        if (device->driver) {
            if (device_index == 0) {
                break;
            }
            --device_index;
        }
        device = device->next;
    }
    return device;
}

static SDL_HIDAPI_Device *
HIDAPI_GetJoystickByInfo(const char *path, Uint16 vendor_id, Uint16 product_id)
{
    SDL_HIDAPI_Device *device = SDL_HIDAPI_devices;
    while (device) {
        if (device->vendor_id == vendor_id && device->product_id == product_id &&
            SDL_strcmp(device->path, path) == 0) {
            break;
        }
        device = device->next;
    }
    return device;
}

static void SDLCALL
SDL_HIDAPIDriverHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    int i;
    SDL_HIDAPI_Device *device = SDL_HIDAPI_devices;
    SDL_bool enabled = (!hint || !*hint || ((*hint != '0') && (SDL_strcasecmp(hint, "false") != 0)));

    if (SDL_strcmp(name, SDL_HINT_JOYSTICK_HIDAPI) == 0) {
        for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
            SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
            driver->enabled = SDL_GetHintBoolean(driver->hint, enabled);
        }
    } else {
        for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
            SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
            if (SDL_strcmp(name, driver->hint) == 0) {
                driver->enabled = enabled;
                break;
            }
        }
    }

    /* Update device list if driver availability changes */
    while (device) {
        if (device->driver) {
            if (!device->driver->enabled) {
                device->driver = NULL;

                --SDL_HIDAPI_numjoysticks;

                SDL_PrivateJoystickRemoved(device->instance_id);
            }
        } else {
            device->driver = HIDAPI_GetDeviceDriver(device);
            if (device->driver) {
                device->instance_id = SDL_GetNextJoystickInstanceID();

                ++SDL_HIDAPI_numjoysticks;

                SDL_PrivateJoystickAdded(device->instance_id);
            }
        }
        device = device->next;
    }
}

static void HIDAPI_JoystickDetect(void);

static int
HIDAPI_JoystickInit(void)
{
    int i;

    if (hid_init() < 0) {
        SDL_SetError("Couldn't initialize hidapi");
        return -1;
    }

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        SDL_AddHintCallback(driver->hint, SDL_HIDAPIDriverHintChanged, NULL);
    }
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI,
                        SDL_HIDAPIDriverHintChanged, NULL);
    SDL_HIDAPI_last_detect = 0;
    HIDAPI_JoystickDetect();
    return 0;
}

static int
HIDAPI_JoystickGetCount(void)
{
    return SDL_HIDAPI_numjoysticks;
}

static void
HIDAPI_AddDevice(struct hid_device_info *info)
{
    SDL_HIDAPI_Device *device;
    SDL_HIDAPI_Device *curr, *last = NULL;

    for (curr = SDL_HIDAPI_devices, last = NULL; curr; last = curr, curr = curr->next) {
        continue;
    }

    device = (SDL_HIDAPI_Device *)SDL_calloc(1, sizeof(*device));
    if (!device) {
        return;
    }
    device->instance_id = -1;
    device->seen = SDL_TRUE;
    device->vendor_id = info->vendor_id;
    device->product_id = info->product_id;
    device->interface_number = info->interface_number;
    device->usage_page = info->usage_page;
    device->usage = info->usage;
    {
        /* FIXME: Is there any way to tell whether this is a Bluetooth device? */
        const Uint16 vendor = device->vendor_id;
        const Uint16 product = device->product_id;
        const Uint16 version = 0;
        Uint16 *guid16 = (Uint16 *)device->guid.data;

        *guid16++ = SDL_SwapLE16(SDL_HARDWARE_BUS_USB);
        *guid16++ = 0;
        *guid16++ = SDL_SwapLE16(vendor);
        *guid16++ = 0;
        *guid16++ = SDL_SwapLE16(product);
        *guid16++ = 0;
        *guid16++ = SDL_SwapLE16(version);
        *guid16++ = 0;

        /* Note that this is a HIDAPI device for special handling elsewhere */
        device->guid.data[14] = 'h';
        device->guid.data[15] = 0;
    }
    device->driver = HIDAPI_GetDeviceDriver(device);

    if (device->driver) {
        const char *name = device->driver->GetDeviceName(device->vendor_id, device->product_id);
        if (name) {
            device->name = SDL_strdup(name);
        }
    }

    if (!device->name && info->manufacturer_string && info->product_string) {
        char *manufacturer_string = SDL_iconv_string("UTF-8", "WCHAR_T", (char*)info->manufacturer_string, (SDL_wcslen(info->manufacturer_string)+1)*sizeof(wchar_t));
        char *product_string = SDL_iconv_string("UTF-8", "WCHAR_T", (char*)info->product_string, (SDL_wcslen(info->product_string)+1)*sizeof(wchar_t));
        if (!manufacturer_string && !product_string) {
            if (sizeof(wchar_t) == sizeof(Uint16)) {
                manufacturer_string = SDL_iconv_string("UTF-8", "UCS-2-INTERNAL", (char*)info->manufacturer_string, (SDL_wcslen(info->manufacturer_string)+1)*sizeof(wchar_t));
                product_string = SDL_iconv_string("UTF-8", "UCS-2-INTERNAL", (char*)info->product_string, (SDL_wcslen(info->product_string)+1)*sizeof(wchar_t));
            } else if (sizeof(wchar_t) == sizeof(Uint32)) {
                manufacturer_string = SDL_iconv_string("UTF-8", "UCS-4-INTERNAL", (char*)info->manufacturer_string, (SDL_wcslen(info->manufacturer_string)+1)*sizeof(wchar_t));
                product_string = SDL_iconv_string("UTF-8", "UCS-4-INTERNAL", (char*)info->product_string, (SDL_wcslen(info->product_string)+1)*sizeof(wchar_t));
            }
        }
        if (manufacturer_string && product_string) {
            size_t name_size = (SDL_strlen(manufacturer_string) + 1 + SDL_strlen(product_string) + 1);
            device->name = (char *)SDL_malloc(name_size);
            if (device->name) {
                SDL_snprintf(device->name, name_size, "%s %s", manufacturer_string, product_string);
            }
        }
        if (manufacturer_string) {
            SDL_free(manufacturer_string);
        }
        if (product_string) {
            SDL_free(product_string);
        }
    }
    if (!device->name) {
        size_t name_size = (6 + 1 + 6 + 1);
        device->name = (char *)SDL_malloc(name_size);
        if (!device->name) {
            SDL_free(device);
            return;
        }
        SDL_snprintf(device->name, name_size, "0x%.4x/0x%.4x", info->vendor_id, info->product_id);
    }

    device->path = SDL_strdup(info->path);
    if (!device->path) {
        SDL_free(device->name);
        SDL_free(device);
        return;
    }

#ifdef DEBUG_HIDAPI
    SDL_Log("Adding HIDAPI device '%s' interface %d, usage page 0x%.4x, usage 0x%.4x\n", device->name, device->interface_number, device->usage_page, device->usage);
#endif

    /* Add it to the list */
    if (last) {
        last->next = device;
    } else {
        SDL_HIDAPI_devices = device;
    }

    if (device->driver) {
        /* It's a joystick! */
        device->instance_id = SDL_GetNextJoystickInstanceID();

        ++SDL_HIDAPI_numjoysticks;

        SDL_PrivateJoystickAdded(device->instance_id);
    }
}


static void
HIDAPI_DelDevice(SDL_HIDAPI_Device *device, SDL_bool send_event)
{
    SDL_HIDAPI_Device *curr, *last;
    for (curr = SDL_HIDAPI_devices, last = NULL; curr; last = curr, curr = curr->next) {
        if (curr == device) {
            if (last) {
                last->next = curr->next;
            } else {
                SDL_HIDAPI_devices = curr->next;
            }

            if (device->driver && send_event) {
                /* Need to decrement the joystick count before we post the event */
                --SDL_HIDAPI_numjoysticks;

                SDL_PrivateJoystickRemoved(device->instance_id);
            }

            SDL_free(device->name);
            SDL_free(device->path);
            SDL_free(device);
            return;
        }
    }
}

static void
HIDAPI_UpdateDeviceList(void)
{
    SDL_HIDAPI_Device *device;
    struct hid_device_info *devs, *info;

    /* Prepare the existing device list */
    device = SDL_HIDAPI_devices;
    while (device) {
        device->seen = SDL_FALSE;
        device = device->next;
    }

    /* Enumerate the devices */
    devs = hid_enumerate(0, 0);
    if (devs) {
        for (info = devs; info; info = info->next) {
            device = HIDAPI_GetJoystickByInfo(info->path, info->vendor_id, info->product_id);
            if (device) {
                device->seen = SDL_TRUE;
            } else {
                HIDAPI_AddDevice(info);
            }
        }
        hid_free_enumeration(devs);
    }

    /* Remove any devices that weren't seen */
    device = SDL_HIDAPI_devices;
    while (device) {
        SDL_HIDAPI_Device *next = device->next;

        if (!device->seen) {
            HIDAPI_DelDevice(device, SDL_TRUE);
        }
        device = next;
    }
}

SDL_bool
HIDAPI_IsDevicePresent(Uint16 vendor_id, Uint16 product_id)
{
    SDL_HIDAPI_Device *device;

    /* Don't update the device list for devices we know aren't supported */
    if (!HIDAPI_IsDeviceSupported(vendor_id, product_id)) {
        return SDL_FALSE;
    }

    /* Make sure the device list is completely up to date when we check for device presence */
    HIDAPI_UpdateDeviceList();

    device = SDL_HIDAPI_devices;
    while (device) {
        if (device->vendor_id == vendor_id && device->product_id == product_id && device->driver) {
            return SDL_TRUE;
        }
        device = device->next;
    }
    return SDL_FALSE;
}

static void
HIDAPI_JoystickDetect(void)
{
    const Uint32 SDL_HIDAPI_DETECT_INTERVAL_MS = 3000;  /* Update every 3 seconds */
    Uint32 now = SDL_GetTicks();
    if (!SDL_HIDAPI_last_detect || SDL_TICKS_PASSED(now, SDL_HIDAPI_last_detect + SDL_HIDAPI_DETECT_INTERVAL_MS)) {
        HIDAPI_UpdateDeviceList();
        SDL_HIDAPI_last_detect = now;
    }
}

static const char *
HIDAPI_JoystickGetDeviceName(int device_index)
{
    return HIDAPI_GetJoystickByIndex(device_index)->name;
}

static SDL_JoystickGUID
HIDAPI_JoystickGetDeviceGUID(int device_index)
{
    return HIDAPI_GetJoystickByIndex(device_index)->guid;
}

static SDL_JoystickID
HIDAPI_JoystickGetDeviceInstanceID(int device_index)
{
    return HIDAPI_GetJoystickByIndex(device_index)->instance_id;
}

static int
HIDAPI_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    SDL_HIDAPI_Device *device = HIDAPI_GetJoystickByIndex(device_index);
    struct joystick_hwdata *hwdata;

    hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*hwdata));
    if (!hwdata) {
        return SDL_OutOfMemory();
    }

    hwdata->driver = device->driver;
    hwdata->dev = hid_open_path(device->path, 0);
    if (!hwdata->dev) {
        SDL_free(hwdata);
        return SDL_SetError("Couldn't open HID device %s", device->path);
    }

    if (!device->driver->Init(joystick, hwdata->dev, device->vendor_id, device->product_id, &hwdata->context)) {
        hid_close(hwdata->dev);
        SDL_free(hwdata);
        return -1;
    }

    joystick->hwdata = hwdata;
    return 0;
}

static int
HIDAPI_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble, Uint32 duration_ms)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    SDL_HIDAPI_DeviceDriver *driver = hwdata->driver;
    return driver->Rumble(joystick, hwdata->dev, hwdata->context, low_frequency_rumble, high_frequency_rumble, duration_ms);
}

static void
HIDAPI_JoystickUpdate(SDL_Joystick * joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    SDL_HIDAPI_DeviceDriver *driver = hwdata->driver;
    driver->Update(joystick, hwdata->dev, hwdata->context);
}

static void
HIDAPI_JoystickClose(SDL_Joystick * joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    SDL_HIDAPI_DeviceDriver *driver = hwdata->driver;
    driver->Quit(joystick, hwdata->dev, hwdata->context);

    hid_close(hwdata->dev);
    SDL_free(hwdata);
    joystick->hwdata = NULL;
}

static void
HIDAPI_JoystickQuit(void)
{
    int i;

    while (SDL_HIDAPI_devices) {
        HIDAPI_DelDevice(SDL_HIDAPI_devices, SDL_FALSE);
    }
    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        SDL_DelHintCallback(driver->hint, SDL_HIDAPIDriverHintChanged, NULL);
    }
    SDL_DelHintCallback(SDL_HINT_JOYSTICK_HIDAPI,
                        SDL_HIDAPIDriverHintChanged, NULL);
    SDL_HIDAPI_numjoysticks = 0;

    hid_exit();
}

SDL_JoystickDriver SDL_HIDAPI_JoystickDriver =
{
    HIDAPI_JoystickInit,
    HIDAPI_JoystickGetCount,
    HIDAPI_JoystickDetect,
    HIDAPI_JoystickGetDeviceName,
    HIDAPI_JoystickGetDeviceGUID,
    HIDAPI_JoystickGetDeviceInstanceID,
    HIDAPI_JoystickOpen,
    HIDAPI_JoystickRumble,
    HIDAPI_JoystickUpdate,
    HIDAPI_JoystickClose,
    HIDAPI_JoystickQuit,
};

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
