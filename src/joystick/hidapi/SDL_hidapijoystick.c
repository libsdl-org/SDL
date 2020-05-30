/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2020 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_assert.h"
#include "SDL_atomic.h"
#include "SDL_endian.h"
#include "SDL_hints.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"
#include "SDL_hidapi_rumble.h"
#include "../../SDL_hints_c.h"

#if defined(__WIN32__)
#include "../../core/windows/SDL_windows.h"
#include "../windows/SDL_rawinputjoystick_c.h"
#endif

#if defined(__MACOSX__)
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDDevice.h>
#include <IOKit/usb/USBSpec.h>
#endif

#if defined(__LINUX__)
#include "../../core/linux/SDL_udev.h"
#ifdef SDL_USE_LIBUDEV
#include <poll.h>
#endif
#endif

struct joystick_hwdata
{
    SDL_HIDAPI_Device *device;
};

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
    &SDL_HIDAPI_DriverXbox360W,
#endif
#ifdef SDL_JOYSTICK_HIDAPI_XBOXONE
    &SDL_HIDAPI_DriverXboxOne,
#endif
#ifdef SDL_JOYSTICK_HIDAPI_GAMECUBE
    &SDL_HIDAPI_DriverGameCube,
#endif
};
static int SDL_HIDAPI_numdrivers = 0;
static SDL_SpinLock SDL_HIDAPI_spinlock;
static SDL_HIDAPI_Device *SDL_HIDAPI_devices;
static int SDL_HIDAPI_numjoysticks = 0;
static SDL_bool initialized = SDL_FALSE;
static SDL_bool shutting_down = SDL_FALSE;

#if defined(SDL_USE_LIBUDEV)
static const SDL_UDEV_Symbols * usyms = NULL;
#endif

static struct
{
    SDL_bool m_bHaveDevicesChanged;
    SDL_bool m_bCanGetNotifications;
    Uint32 m_unLastDetect;

#if defined(__WIN32__)
    SDL_threadID m_nThreadID;
    WNDCLASSEXA m_wndClass;
    HWND m_hwndMsg;
    HDEVNOTIFY m_hNotify;
    double m_flLastWin32MessageCheck;
#endif

#if defined(__MACOSX__)
    IONotificationPortRef m_notificationPort;
    mach_port_t m_notificationMach;
#endif

#if defined(SDL_USE_LIBUDEV)
    struct udev *m_pUdev;
    struct udev_monitor *m_pUdevMonitor;
    int m_nUdevFd;
#endif
} SDL_HIDAPI_discovery;


#ifdef __WIN32__
struct _DEV_BROADCAST_HDR
{
    DWORD       dbch_size;
    DWORD       dbch_devicetype;
    DWORD       dbch_reserved;
};

typedef struct _DEV_BROADCAST_DEVICEINTERFACE_A
{
    DWORD       dbcc_size;
    DWORD       dbcc_devicetype;
    DWORD       dbcc_reserved;
    GUID        dbcc_classguid;
    char        dbcc_name[ 1 ];
} DEV_BROADCAST_DEVICEINTERFACE_A, *PDEV_BROADCAST_DEVICEINTERFACE_A;

typedef struct  _DEV_BROADCAST_HDR      DEV_BROADCAST_HDR;
#define DBT_DEVICEARRIVAL               0x8000  /* system detected a new device */
#define DBT_DEVICEREMOVECOMPLETE        0x8004  /* device was removed from the system */
#define DBT_DEVTYP_DEVICEINTERFACE      0x00000005  /* device interface class */
#define DBT_DEVNODES_CHANGED            0x0007
#define DBT_CONFIGCHANGED               0x0018
#define DBT_DEVICETYPESPECIFIC          0x8005  /* type specific event */
#define DBT_DEVINSTSTARTED              0x8008  /* device installed and started */

#include <initguid.h>
DEFINE_GUID(GUID_DEVINTERFACE_USB_DEVICE, 0xA5DCBF10L, 0x6530, 0x11D2, 0x90, 0x1F, 0x00, 0xC0, 0x4F, 0xB9, 0x51, 0xED);

static LRESULT CALLBACK ControllerWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    case WM_DEVICECHANGE:
        switch (wParam) {
        case DBT_DEVICEARRIVAL:
        case DBT_DEVICEREMOVECOMPLETE:
            if (((DEV_BROADCAST_HDR*)lParam)->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                SDL_HIDAPI_discovery.m_bHaveDevicesChanged = SDL_TRUE;
            }
            break;
        }
        return TRUE;
    }

    return DefWindowProc(hwnd, message, wParam, lParam);
}
#endif /* __WIN32__ */


#if defined(__MACOSX__)
static void CallbackIOServiceFunc(void *context, io_iterator_t portIterator)
{
    /* Must drain the iterator, or we won't receive new notifications */
    io_object_t entry;
    while ((entry = IOIteratorNext(portIterator)) != 0) {
        IOObjectRelease(entry);
        *(SDL_bool*)context = SDL_TRUE;
    }
}
#endif /* __MACOSX__ */

static void
HIDAPI_InitializeDiscovery()
{
    SDL_HIDAPI_discovery.m_bHaveDevicesChanged = SDL_TRUE;
    SDL_HIDAPI_discovery.m_bCanGetNotifications = SDL_FALSE;
    SDL_HIDAPI_discovery.m_unLastDetect = 0;

#if defined(__WIN32__)
    SDL_HIDAPI_discovery.m_nThreadID = SDL_ThreadID();

    SDL_memset(&SDL_HIDAPI_discovery.m_wndClass, 0x0, sizeof(SDL_HIDAPI_discovery.m_wndClass));
    SDL_HIDAPI_discovery.m_wndClass.hInstance = GetModuleHandle(NULL);
    SDL_HIDAPI_discovery.m_wndClass.lpszClassName = "SDL_HIDAPI_DEVICE_DETECTION";
    SDL_HIDAPI_discovery.m_wndClass.lpfnWndProc = ControllerWndProc;      /* This function is called by windows */
    SDL_HIDAPI_discovery.m_wndClass.cbSize = sizeof(WNDCLASSEX);

    RegisterClassExA(&SDL_HIDAPI_discovery.m_wndClass);
    SDL_HIDAPI_discovery.m_hwndMsg = CreateWindowExA(0, "SDL_HIDAPI_DEVICE_DETECTION", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    {
        DEV_BROADCAST_DEVICEINTERFACE_A devBroadcast;
        SDL_memset( &devBroadcast, 0x0, sizeof( devBroadcast ) );

        devBroadcast.dbcc_size = sizeof( devBroadcast );
        devBroadcast.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        devBroadcast.dbcc_classguid = GUID_DEVINTERFACE_USB_DEVICE;

        /* DEVICE_NOTIFY_ALL_INTERFACE_CLASSES is important, makes GUID_DEVINTERFACE_USB_DEVICE ignored,
         * but that seems to be necessary to get a notice after each individual usb input device actually
         * installs, rather than just as the composite device is seen.
         */
        SDL_HIDAPI_discovery.m_hNotify = RegisterDeviceNotification( SDL_HIDAPI_discovery.m_hwndMsg, &devBroadcast, DEVICE_NOTIFY_WINDOW_HANDLE | DEVICE_NOTIFY_ALL_INTERFACE_CLASSES );
        SDL_HIDAPI_discovery.m_bCanGetNotifications = ( SDL_HIDAPI_discovery.m_hNotify != 0 );
    }
#endif /* __WIN32__ */

#if defined(__MACOSX__)
    SDL_HIDAPI_discovery.m_notificationPort = IONotificationPortCreate(kIOMasterPortDefault);
    if (SDL_HIDAPI_discovery.m_notificationPort) {
        {
            io_iterator_t portIterator = 0;
            io_object_t entry;
            IOReturn result = IOServiceAddMatchingNotification(
                SDL_HIDAPI_discovery.m_notificationPort,
                kIOFirstMatchNotification,
                IOServiceMatching(kIOHIDDeviceKey),
                CallbackIOServiceFunc, &SDL_HIDAPI_discovery.m_bHaveDevicesChanged, &portIterator);

            if (result == 0) {
                /* Must drain the existing iterator, or we won't receive new notifications */
                while ((entry = IOIteratorNext(portIterator)) != 0) {
                    IOObjectRelease(entry);
                }
            } else {
                IONotificationPortDestroy(SDL_HIDAPI_discovery.m_notificationPort);
                SDL_HIDAPI_discovery.m_notificationPort = nil;
            }
        }
        {
            io_iterator_t portIterator = 0;
            io_object_t entry;
            IOReturn result = IOServiceAddMatchingNotification(
                SDL_HIDAPI_discovery.m_notificationPort,
                kIOTerminatedNotification,
                IOServiceMatching(kIOHIDDeviceKey),
                CallbackIOServiceFunc, &SDL_HIDAPI_discovery.m_bHaveDevicesChanged, &portIterator);

            if (result == 0) {
                /* Must drain the existing iterator, or we won't receive new notifications */
                while ((entry = IOIteratorNext(portIterator)) != 0) {
                    IOObjectRelease(entry);
                }
            } else {
                IONotificationPortDestroy(SDL_HIDAPI_discovery.m_notificationPort);
                SDL_HIDAPI_discovery.m_notificationPort = nil;
            }
        }
    }

    SDL_HIDAPI_discovery.m_notificationMach = MACH_PORT_NULL;
    if (SDL_HIDAPI_discovery.m_notificationPort) {
        SDL_HIDAPI_discovery.m_notificationMach = IONotificationPortGetMachPort(SDL_HIDAPI_discovery.m_notificationPort);
    }

    SDL_HIDAPI_discovery.m_bCanGetNotifications = (SDL_HIDAPI_discovery.m_notificationMach != MACH_PORT_NULL);

#endif // __MACOSX__

#if defined(SDL_USE_LIBUDEV)
    SDL_HIDAPI_discovery.m_pUdev = NULL;
    SDL_HIDAPI_discovery.m_pUdevMonitor = NULL;
    SDL_HIDAPI_discovery.m_nUdevFd = -1;

    usyms = SDL_UDEV_GetUdevSyms();
    if (usyms) {
        SDL_HIDAPI_discovery.m_pUdev = usyms->udev_new();
    }
    if (SDL_HIDAPI_discovery.m_pUdev) {
        SDL_HIDAPI_discovery.m_pUdevMonitor = usyms->udev_monitor_new_from_netlink(SDL_HIDAPI_discovery.m_pUdev, "udev");
        if (SDL_HIDAPI_discovery.m_pUdevMonitor) {
            usyms->udev_monitor_enable_receiving(SDL_HIDAPI_discovery.m_pUdevMonitor);
            SDL_HIDAPI_discovery.m_nUdevFd = usyms->udev_monitor_get_fd(SDL_HIDAPI_discovery.m_pUdevMonitor);
            SDL_HIDAPI_discovery.m_bCanGetNotifications = SDL_TRUE;
        }
    }

#endif /* SDL_USE_LIBUDEV */
}

static void
HIDAPI_UpdateDiscovery()
{
    if (!SDL_HIDAPI_discovery.m_bCanGetNotifications) {
        const Uint32 SDL_HIDAPI_DETECT_INTERVAL_MS = 3000;  /* Update every 3 seconds */
        Uint32 now = SDL_GetTicks();
        if (!SDL_HIDAPI_discovery.m_unLastDetect || SDL_TICKS_PASSED(now, SDL_HIDAPI_discovery.m_unLastDetect + SDL_HIDAPI_DETECT_INTERVAL_MS)) {
            SDL_HIDAPI_discovery.m_bHaveDevicesChanged = SDL_TRUE;
            SDL_HIDAPI_discovery.m_unLastDetect = now;
        }
        return;
    }

#if defined(__WIN32__)
#if 0 /* just let the usual SDL_PumpEvents loop dispatch these, fixing bug 4286. --ryan. */
    /* We'll only get messages on the same thread that created the window */
    if (SDL_ThreadID() == SDL_HIDAPI_discovery.m_nThreadID) {
        MSG msg;
        while (PeekMessage(&msg, SDL_HIDAPI_discovery.m_hwndMsg, 0, 0, PM_NOREMOVE)) {
            if (GetMessageA(&msg, SDL_HIDAPI_discovery.m_hwndMsg, 0, 0) != 0) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
#endif
#endif /* __WIN32__ */

#if defined(__MACOSX__)
    if (SDL_HIDAPI_discovery.m_notificationPort) {
        struct { mach_msg_header_t hdr; char payload[ 4096 ]; } msg;
        while (mach_msg(&msg.hdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), SDL_HIDAPI_discovery.m_notificationMach, 0, MACH_PORT_NULL) == KERN_SUCCESS) {
            IODispatchCalloutFromMessage(NULL, &msg.hdr, SDL_HIDAPI_discovery.m_notificationPort);
        }
    }
#endif

#if defined(SDL_USE_LIBUDEV)
    if (SDL_HIDAPI_discovery.m_nUdevFd >= 0) {
        /* Drain all notification events.
         * We don't expect a lot of device notifications so just
         * do a new discovery on any kind or number of notifications.
         * This could be made more restrictive if necessary.
         */
        for (;;) {
            struct pollfd PollUdev;
            struct udev_device *pUdevDevice;

            PollUdev.fd = SDL_HIDAPI_discovery.m_nUdevFd;
            PollUdev.events = POLLIN;
            if (poll(&PollUdev, 1, 0) != 1) {
                break;
            }

            SDL_HIDAPI_discovery.m_bHaveDevicesChanged = SDL_TRUE;

            pUdevDevice = usyms->udev_monitor_receive_device(SDL_HIDAPI_discovery.m_pUdevMonitor);
            if (pUdevDevice) {
                usyms->udev_device_unref(pUdevDevice);
            }
        }
    }
#endif
}

static void
HIDAPI_ShutdownDiscovery()
{
#if defined(__WIN32__)
    if (SDL_HIDAPI_discovery.m_hNotify)
        UnregisterDeviceNotification(SDL_HIDAPI_discovery.m_hNotify);

    if (SDL_HIDAPI_discovery.m_hwndMsg) {
        DestroyWindow(SDL_HIDAPI_discovery.m_hwndMsg);
    }

    UnregisterClassA(SDL_HIDAPI_discovery.m_wndClass.lpszClassName, SDL_HIDAPI_discovery.m_wndClass.hInstance);
#endif

#if defined(__MACOSX__)
    if (SDL_HIDAPI_discovery.m_notificationPort) {
        IONotificationPortDestroy(SDL_HIDAPI_discovery.m_notificationPort);
    }
#endif

#if defined(SDL_USE_LIBUDEV)
    if (usyms) {
        if (SDL_HIDAPI_discovery.m_pUdevMonitor) {
            usyms->udev_monitor_unref(SDL_HIDAPI_discovery.m_pUdevMonitor);
        }
        if (SDL_HIDAPI_discovery.m_pUdev) {
            usyms->udev_unref(SDL_HIDAPI_discovery.m_pUdev);
        }
        SDL_UDEV_ReleaseUdevSyms();
        usyms = NULL;
    }
#endif
}

static void HIDAPI_JoystickDetect(void);
static void HIDAPI_JoystickClose(SDL_Joystick * joystick);

static SDL_bool
HIDAPI_IsDeviceSupported(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    int i;
    SDL_GameControllerType type = SDL_GetJoystickGameControllerType(name, vendor_id, product_id, -1, 0, 0, 0);

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled && driver->IsSupportedDevice(name, type, vendor_id, product_id, version, -1, 0, 0, 0)) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static SDL_HIDAPI_DeviceDriver *
HIDAPI_GetDeviceDriver(SDL_HIDAPI_Device *device)
{
    const Uint16 USAGE_PAGE_GENERIC_DESKTOP = 0x0001;
    const Uint16 USAGE_JOYSTICK = 0x0004;
    const Uint16 USAGE_GAMEPAD = 0x0005;
    const Uint16 USAGE_MULTIAXISCONTROLLER = 0x0008;
    int i;
    SDL_GameControllerType type;

    if (SDL_ShouldIgnoreJoystick(device->name, device->guid)) {
        return NULL;
    }

#ifdef SDL_JOYSTICK_RAWINPUT
    if (RAWINPUT_IsDevicePresent(device->vendor_id, device->product_id, device->version)) {
        /* The RAWINPUT driver is taking care of this device */
        return NULL;
    }
#endif

	if (device->vendor_id != USB_VENDOR_VALVE) {
        if (device->usage_page && device->usage_page != USAGE_PAGE_GENERIC_DESKTOP) {
            return NULL;
        }
        if (device->usage && device->usage != USAGE_JOYSTICK && device->usage != USAGE_GAMEPAD && device->usage != USAGE_MULTIAXISCONTROLLER) {
            return NULL;
        }
    }

    type = SDL_GetJoystickGameControllerType(device->name, device->vendor_id, device->product_id, device->interface_number, device->interface_class, device->interface_subclass, device->interface_protocol);
    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled && driver->IsSupportedDevice(device->name, type, device->vendor_id, device->product_id, device->version, device->interface_number, device->interface_class, device->interface_subclass, device->interface_protocol)) {
            return driver;
        }
    }
    return NULL;
}

static SDL_HIDAPI_Device *
HIDAPI_GetDeviceByIndex(int device_index, SDL_JoystickID *pJoystickID)
{
    SDL_HIDAPI_Device *device = SDL_HIDAPI_devices;
    while (device) {
        if (device->driver) {
            if (device_index < device->num_joysticks) {
                if (pJoystickID) {
                    *pJoystickID = device->joysticks[device_index];
                }
                return device;
            }
            device_index -= device->num_joysticks;
        }
        device = device->next;
    }
    return NULL;
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

static void
HIDAPI_SetupDeviceDriver(SDL_HIDAPI_Device *device)
{
    if (device->driver) {
        /* Already setup */
        return;
    }

    device->driver = HIDAPI_GetDeviceDriver(device);
    if (device->driver) {
        const char *name = device->driver->GetDeviceName(device->vendor_id, device->product_id);
        if (name) {
            SDL_free(device->name);
            device->name = SDL_strdup(name);
        }
    }

    /* Initialize the device, which may cause a connected event */
    if (device->driver && !device->driver->InitDevice(device)) {
        device->driver = NULL;
    }
}

static void
HIDAPI_CleanupDeviceDriver(SDL_HIDAPI_Device *device)
{
    if (!device->driver) {
        /* Already cleaned up */
        return;
    }

    /* Disconnect any joysticks */
    while (device->num_joysticks) {
        HIDAPI_JoystickDisconnected(device, device->joysticks[0], SDL_FALSE);
    }

    device->driver->FreeDevice(device);
    device->driver = NULL;
}

static void SDLCALL
SDL_HIDAPIDriverHintChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    int i;
    SDL_HIDAPI_Device *device;
    SDL_bool enabled = SDL_GetStringBoolean(hint, SDL_TRUE);

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
            }
        }
    }

    SDL_HIDAPI_numdrivers = 0;
    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled) {
            ++SDL_HIDAPI_numdrivers;
        }
    }

    /* Update device list if driver availability changes */
    SDL_LockJoysticks();

    for (device = SDL_HIDAPI_devices; device; device = device->next) {
        if (device->driver && !device->driver->enabled) {
            HIDAPI_CleanupDeviceDriver(device);
        }
        HIDAPI_SetupDeviceDriver(device);
    }

    SDL_UnlockJoysticks();
}

static int
HIDAPI_JoystickInit(void)
{
    int i;

    if (initialized) {
        return 0;
    }

#if defined(__MACOSX__) || defined(__IPHONEOS__) || defined(__TVOS__)
	/* The hidapi framwork is weak-linked on Apple platforms */
    int HID_API_EXPORT HID_API_CALL hid_init(void) __attribute__((weak_import));

    if (hid_init == NULL) {
        SDL_SetError("Couldn't initialize hidapi, framework not available");
        return -1;
    }
#endif /* __MACOSX__ || __IPHONEOS__ || __TVOS__ */

    if (hid_init() < 0) {
        SDL_SetError("Couldn't initialize hidapi");
        return -1;
    }

#ifdef __WINDOWS__
    /* On Windows, turns out HIDAPI for Xbox controllers doesn't allow background input, so off by default */
    SDL_SetHintWithPriority(SDL_HINT_JOYSTICK_HIDAPI_XBOX, "0", SDL_HINT_DEFAULT);
#endif

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        SDL_AddHintCallback(driver->hint, SDL_HIDAPIDriverHintChanged, NULL);
    }
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_HIDAPI,
                        SDL_HIDAPIDriverHintChanged, NULL);
    HIDAPI_InitializeDiscovery();
    HIDAPI_JoystickDetect();
    HIDAPI_UpdateDevices();

    initialized = SDL_TRUE;

    return 0;
}

SDL_bool
HIDAPI_JoystickConnected(SDL_HIDAPI_Device *device, SDL_JoystickID *pJoystickID, SDL_bool is_external)
{
    SDL_JoystickID joystickID;
    SDL_JoystickID *joysticks = (SDL_JoystickID *)SDL_realloc(device->joysticks, (device->num_joysticks + 1)*sizeof(*device->joysticks));
    if (!joysticks) {
        return SDL_FALSE;
    }

    joystickID = SDL_GetNextJoystickInstanceID();
    device->joysticks = joysticks;
    device->joysticks[device->num_joysticks++] = joystickID;
    if (!is_external) {
        ++SDL_HIDAPI_numjoysticks;
    }

    SDL_PrivateJoystickAdded(joystickID);

    if (pJoystickID) {
        *pJoystickID = joystickID;
    }
    return SDL_TRUE;
}

void
HIDAPI_JoystickDisconnected(SDL_HIDAPI_Device *device, SDL_JoystickID joystickID, SDL_bool is_external)
{
    int i, size;

    for (i = 0; i < device->num_joysticks; ++i) {
        if (device->joysticks[i] == joystickID) {
            SDL_Joystick *joystick = SDL_JoystickFromInstanceID(joystickID);
            if (joystick && !is_external) {
                HIDAPI_JoystickClose(joystick);
            }

            size = (device->num_joysticks - i - 1) * sizeof(SDL_JoystickID);
            SDL_memmove(&device->joysticks[i], &device->joysticks[i+1], size);
            --device->num_joysticks;

            if (!is_external) {
                --SDL_HIDAPI_numjoysticks;
            }
            if (device->num_joysticks == 0) {
                SDL_free(device->joysticks);
                device->joysticks = NULL;
            }

            if (!shutting_down) {
                SDL_PrivateJoystickRemoved(joystickID);
            }
            return;
        }
    }
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
    device->path = SDL_strdup(info->path);
    if (!device->path) {
        SDL_free(device);
        return;
    }
    device->seen = SDL_TRUE;
    device->vendor_id = info->vendor_id;
    device->product_id = info->product_id;
    device->version = info->release_number;
    device->interface_number = info->interface_number;
    device->interface_class = info->interface_class;
    device->interface_subclass = info->interface_subclass;
    device->interface_protocol = info->interface_protocol;
    device->usage_page = info->usage_page;
    device->usage = info->usage;
    {
        /* FIXME: Is there any way to tell whether this is a Bluetooth device? */
        const Uint16 vendor = device->vendor_id;
        const Uint16 product = device->product_id;
        const Uint16 version = device->version;
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
    device->dev_lock = SDL_CreateMutex();

    /* Need the device name before getting the driver to know whether to ignore this device */
    {
        char *manufacturer_string = NULL;
        char *product_string = NULL;

        if (info->manufacturer_string) {
            manufacturer_string = SDL_iconv_string("UTF-8", "WCHAR_T", (char*)info->manufacturer_string, (SDL_wcslen(info->manufacturer_string)+1)*sizeof(wchar_t));
            if (!manufacturer_string) {
                if (sizeof(wchar_t) == sizeof(Uint16)) {
                    manufacturer_string = SDL_iconv_string("UTF-8", "UCS-2-INTERNAL", (char*)info->manufacturer_string, (SDL_wcslen(info->manufacturer_string)+1)*sizeof(wchar_t));
                } else if (sizeof(wchar_t) == sizeof(Uint32)) {
                    manufacturer_string = SDL_iconv_string("UTF-8", "UCS-4-INTERNAL", (char*)info->manufacturer_string, (SDL_wcslen(info->manufacturer_string)+1)*sizeof(wchar_t));
                }
            }
        }
        if (info->product_string) {
            product_string = SDL_iconv_string("UTF-8", "WCHAR_T", (char*)info->product_string, (SDL_wcslen(info->product_string)+1)*sizeof(wchar_t));
            if (!product_string) {
                if (sizeof(wchar_t) == sizeof(Uint16)) {
                    product_string = SDL_iconv_string("UTF-8", "UCS-2-INTERNAL", (char*)info->product_string, (SDL_wcslen(info->product_string)+1)*sizeof(wchar_t));
                } else if (sizeof(wchar_t) == sizeof(Uint32)) {
                    product_string = SDL_iconv_string("UTF-8", "UCS-4-INTERNAL", (char*)info->product_string, (SDL_wcslen(info->product_string)+1)*sizeof(wchar_t));
                }
            }
        }

        device->name = SDL_CreateJoystickName(device->vendor_id, device->product_id, manufacturer_string, product_string);

        if (manufacturer_string) {
            SDL_free(manufacturer_string);
        }
        if (product_string) {
            SDL_free(product_string);
        }

        if (!device->name) {
            SDL_free(device->path);
            SDL_free(device);
            return;
        }
    }

    /* Add it to the list */
    if (last) {
        last->next = device;
    } else {
        SDL_HIDAPI_devices = device;
    }

    HIDAPI_SetupDeviceDriver(device);

#ifdef DEBUG_HIDAPI
    SDL_Log("Added HIDAPI device '%s' VID 0x%.4x, PID 0x%.4x, version %d, interface %d, interface_class %d, interface_subclass %d, interface_protocol %d, usage page 0x%.4x, usage 0x%.4x, path = %s, driver = %s (%s)\n", device->name, device->vendor_id, device->product_id, device->version, device->interface_number, device->interface_class, device->interface_subclass, device->interface_protocol, device->usage_page, device->usage, device->path, device->driver ? device->driver->hint : "NONE", device->driver && device->driver->enabled ? "ENABLED" : "DISABLED");
#endif
}


static void
HIDAPI_DelDevice(SDL_HIDAPI_Device *device)
{
    SDL_HIDAPI_Device *curr, *last;
    for (curr = SDL_HIDAPI_devices, last = NULL; curr; last = curr, curr = curr->next) {
        if (curr == device) {
            if (last) {
                last->next = curr->next;
            } else {
                SDL_HIDAPI_devices = curr->next;
            }

            HIDAPI_CleanupDeviceDriver(device);

            SDL_DestroyMutex(device->dev_lock);
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

    SDL_LockJoysticks();

    /* Prepare the existing device list */
    device = SDL_HIDAPI_devices;
    while (device) {
        device->seen = SDL_FALSE;
        device = device->next;
    }

    /* Enumerate the devices */
    if (SDL_HIDAPI_numdrivers > 0) {
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
    }

    /* Remove any devices that weren't seen */
    device = SDL_HIDAPI_devices;
    while (device) {
        SDL_HIDAPI_Device *next = device->next;

        if (!device->seen) {
            HIDAPI_DelDevice(device);
        }
        device = next;
    }

    SDL_UnlockJoysticks();
}

SDL_bool
HIDAPI_IsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version, const char *name)
{
    SDL_HIDAPI_Device *device;
    SDL_bool supported = SDL_FALSE;
    SDL_bool result = SDL_FALSE;

    /* Make sure we're initialized, as this could be called from other drivers during startup */
    if (HIDAPI_JoystickInit() < 0) {
        return SDL_FALSE;
    }

    /* Only update the device list for devices we know might be supported.
       If we did this for every device, it would hit the USB driver too hard and potentially 
       lock up the system. This won't catch devices that we support but can only detect using 
       USB interface details, like Xbox controllers, but hopefully the device list update is
       responsive enough to catch those.
     */
    supported = HIDAPI_IsDeviceSupported(vendor_id, product_id, version, name);
#if defined(SDL_JOYSTICK_HIDAPI_XBOX360) || defined(SDL_JOYSTICK_HIDAPI_XBOXONE)
    if (!supported &&
        (SDL_strstr(name, "Xbox") || SDL_strstr(name, "X-Box") || SDL_strstr(name, "XBOX"))) {
        supported = SDL_TRUE;
    }
#endif /* SDL_JOYSTICK_HIDAPI_XBOX360 || SDL_JOYSTICK_HIDAPI_XBOXONE */
    if (supported) {
        if (SDL_AtomicTryLock(&SDL_HIDAPI_spinlock)) {
            HIDAPI_UpdateDeviceList();
            SDL_AtomicUnlock(&SDL_HIDAPI_spinlock);
        }
    }

    /* Note that this isn't a perfect check - there may be multiple devices with 0 VID/PID,
       or a different name than we have it listed here, etc, but if we support the device
       and we have something similar in our device list, mark it as present.
     */
    SDL_LockJoysticks();
    device = SDL_HIDAPI_devices;
    while (device) {
        if (device->vendor_id == vendor_id && device->product_id == product_id && device->driver) {
            result = SDL_TRUE;
        }
        device = device->next;
    }
    SDL_UnlockJoysticks();

    /* If we're looking for the wireless XBox 360 controller, also look for the dongle */
    if (!result && vendor_id == USB_VENDOR_MICROSOFT && product_id == 0x02a1) {
        return HIDAPI_IsDevicePresent(USB_VENDOR_MICROSOFT, 0x0719, version, name);
    }

#ifdef DEBUG_HIDAPI
    SDL_Log("HIDAPI_IsDevicePresent() returning %s for 0x%.4x / 0x%.4x\n", result ? "true" : "false", vendor_id, product_id);
#endif
    return result;
}

static void
HIDAPI_JoystickDetect(void)
{
    int i;
    if (SDL_AtomicTryLock(&SDL_HIDAPI_spinlock)) {
        HIDAPI_UpdateDiscovery();
        if (SDL_HIDAPI_discovery.m_bHaveDevicesChanged) {
            /* FIXME: We probably need to schedule an update in a few seconds as well */
            HIDAPI_UpdateDeviceList();
            SDL_HIDAPI_discovery.m_bHaveDevicesChanged = SDL_FALSE;
        }
        SDL_AtomicUnlock(&SDL_HIDAPI_spinlock);
    }
    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled && driver->PostUpdate) {
            driver->PostUpdate();
        }
    }
}

void
HIDAPI_UpdateDevices(void)
{
    SDL_HIDAPI_Device *device;

    /* Update the devices, which may change connected joysticks and send events */

    /* Prepare the existing device list */
    if (SDL_AtomicTryLock(&SDL_HIDAPI_spinlock)) {
        device = SDL_HIDAPI_devices;
        while (device) {
            if (device->driver) {
                if (SDL_TryLockMutex(device->dev_lock) == 0) {
                    device->driver->UpdateDevice(device);
                    SDL_UnlockMutex(device->dev_lock);
                }
            }
            device = device->next;
        }
        SDL_AtomicUnlock(&SDL_HIDAPI_spinlock);
    }
}

static const char *
HIDAPI_JoystickGetDeviceName(int device_index)
{
    SDL_HIDAPI_Device *device;
    const char *name = NULL;

    device = HIDAPI_GetDeviceByIndex(device_index, NULL);
    if (device) {
        /* FIXME: The device could be freed after this name is returned... */
        name = device->name;
    }

    return name;
}

static int
HIDAPI_JoystickGetDevicePlayerIndex(int device_index)
{
    SDL_HIDAPI_Device *device;
    SDL_JoystickID instance_id;
    int player_index = -1;

    device = HIDAPI_GetDeviceByIndex(device_index, &instance_id);
    if (device) {
        player_index = device->driver->GetDevicePlayerIndex(device, instance_id);
    }

    return player_index;
}

static void
HIDAPI_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
    SDL_HIDAPI_Device *device;
    SDL_JoystickID instance_id;

    device = HIDAPI_GetDeviceByIndex(device_index, &instance_id);
    if (device) {
        device->driver->SetDevicePlayerIndex(device, instance_id, player_index);
    }
}

static SDL_JoystickGUID
HIDAPI_JoystickGetDeviceGUID(int device_index)
{
    SDL_HIDAPI_Device *device;
    SDL_JoystickGUID guid;

    device = HIDAPI_GetDeviceByIndex(device_index, NULL);
    if (device) {
        SDL_memcpy(&guid, &device->guid, sizeof(guid));
    } else {
        SDL_zero(guid);
    }

    return guid;
}

static SDL_JoystickID
HIDAPI_JoystickGetDeviceInstanceID(int device_index)
{
    SDL_JoystickID joystickID = -1;
    HIDAPI_GetDeviceByIndex(device_index, &joystickID);
    return joystickID;
}

static int
HIDAPI_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    SDL_JoystickID joystickID;
    SDL_HIDAPI_Device *device = HIDAPI_GetDeviceByIndex(device_index, &joystickID);
    struct joystick_hwdata *hwdata;

    hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*hwdata));
    if (!hwdata) {
        return SDL_OutOfMemory();
    }
    hwdata->device = device;

    if (!device->driver->OpenJoystick(device, joystick)) {
        SDL_free(hwdata);
        return -1;
    }

    joystick->hwdata = hwdata;
    return 0;
}

static int
HIDAPI_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    int result;

    if (joystick->hwdata) {
        SDL_HIDAPI_Device *device = joystick->hwdata->device;

        result = device->driver->RumbleJoystick(device, joystick, low_frequency_rumble, high_frequency_rumble);
    } else {
        SDL_SetError("Rumble failed, device disconnected");
        result = -1;
    }

    return result;
}

static void
HIDAPI_JoystickUpdate(SDL_Joystick * joystick)
{
    /* This is handled in SDL_HIDAPI_UpdateDevices() */
}

static void
HIDAPI_JoystickClose(SDL_Joystick * joystick)
{
    if (joystick->hwdata) {
        SDL_HIDAPI_Device *device = joystick->hwdata->device;

        /* Wait for pending rumble to complete */
        while (SDL_AtomicGet(&device->rumble_pending) > 0) {
            SDL_Delay(10);
        }

        device->driver->CloseJoystick(device, joystick);

        SDL_free(joystick->hwdata);
        joystick->hwdata = NULL;
    }
}

static void
HIDAPI_JoystickQuit(void)
{
    int i;

    shutting_down = SDL_TRUE;

    HIDAPI_ShutdownDiscovery();

    while (SDL_HIDAPI_devices) {
        HIDAPI_DelDevice(SDL_HIDAPI_devices);
    }

    SDL_HIDAPI_QuitRumble();

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        SDL_DelHintCallback(driver->hint, SDL_HIDAPIDriverHintChanged, NULL);
    }
    SDL_DelHintCallback(SDL_HINT_JOYSTICK_HIDAPI,
                        SDL_HIDAPIDriverHintChanged, NULL);

    hid_exit();

    /* Make sure the drivers cleaned up properly */
    SDL_assert(SDL_HIDAPI_numjoysticks == 0);

    shutting_down = SDL_FALSE;
    initialized = SDL_FALSE;
}

static SDL_bool
HIDAPI_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    return SDL_FALSE;
}

SDL_JoystickDriver SDL_HIDAPI_JoystickDriver =
{
    HIDAPI_JoystickInit,
    HIDAPI_JoystickGetCount,
    HIDAPI_JoystickDetect,
    HIDAPI_JoystickGetDeviceName,
    HIDAPI_JoystickGetDevicePlayerIndex,
    HIDAPI_JoystickSetDevicePlayerIndex,
    HIDAPI_JoystickGetDeviceGUID,
    HIDAPI_JoystickGetDeviceInstanceID,
    HIDAPI_JoystickOpen,
    HIDAPI_JoystickRumble,
    HIDAPI_JoystickUpdate,
    HIDAPI_JoystickClose,
    HIDAPI_JoystickQuit,
    HIDAPI_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_HIDAPI */

/* vi: set ts=4 sw=4 expandtab: */
