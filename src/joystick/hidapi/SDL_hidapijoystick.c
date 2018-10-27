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
#include "SDL_mutex.h"
#include "SDL_thread.h"
#include "SDL_timer.h"
#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "SDL_hidapijoystick_c.h"

#if defined(__WIN32__)
#include "../../core/windows/SDL_windows.h"
#endif

#if defined(__MACOSX__)
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>
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
    SDL_HIDAPI_DeviceDriver *driver;
    void *context;

    SDL_mutex *mutex;
    hid_device *dev;
};

typedef struct _SDL_HIDAPI_Device
{
    SDL_JoystickID instance_id;
    char *name;
    char *path;
    Uint16 vendor_id;
    Uint16 product_id;
    Uint16 version;
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
            CFMutableDictionaryRef matchingDict = IOServiceMatching("IOUSBDevice");

            /* Note: IOServiceAddMatchingNotification consumes the reference to matchingDict */
            io_iterator_t portIterator = 0;
            io_object_t entry;
            if (IOServiceAddMatchingNotification(SDL_HIDAPI_discovery.m_notificationPort, kIOMatchedNotification, matchingDict, CallbackIOServiceFunc, &SDL_HIDAPI_discovery.m_bHaveDevicesChanged, &portIterator) == 0) {
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
            CFMutableDictionaryRef matchingDict = IOServiceMatching("IOBluetoothDevice");

            /* Note: IOServiceAddMatchingNotification consumes the reference to matchingDict */
            io_iterator_t portIterator = 0;
            io_object_t entry;
            if (IOServiceAddMatchingNotification(SDL_HIDAPI_discovery.m_notificationPort, kIOMatchedNotification, matchingDict, CallbackIOServiceFunc, &SDL_HIDAPI_discovery.m_bHaveDevicesChanged, &portIterator) == 0) {
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


const char *
HIDAPI_XboxControllerName(Uint16 vendor_id, Uint16 product_id)
{
    static struct
    {
        Uint32 vidpid;
        const char *name;
    } names[] = {
        { MAKE_VIDPID(0x0079, 0x18d4), "GPD Win 2 X-Box Controller" },
        { MAKE_VIDPID(0x044f, 0xb326), "Thrustmaster Gamepad GP XID" },
        { MAKE_VIDPID(0x045e, 0x028e), "Microsoft X-Box 360 pad" },
        { MAKE_VIDPID(0x045e, 0x028f), "Microsoft X-Box 360 pad v2" },
        { MAKE_VIDPID(0x045e, 0x0291), "Xbox 360 Wireless Receiver (XBOX)" },
        { MAKE_VIDPID(0x045e, 0x02d1), "Microsoft X-Box One pad" },
        { MAKE_VIDPID(0x045e, 0x02dd), "Microsoft X-Box One pad (Firmware 2015)" },
        { MAKE_VIDPID(0x045e, 0x02e3), "Microsoft X-Box One Elite pad" },
        { MAKE_VIDPID(0x045e, 0x02ea), "Microsoft X-Box One S pad" },
        { MAKE_VIDPID(0x045e, 0x02ff), "Microsoft X-Box One pad" },
        { MAKE_VIDPID(0x045e, 0x0719), "Xbox 360 Wireless Receiver" },
        { MAKE_VIDPID(0x046d, 0xc21d), "Logitech Gamepad F310" },
        { MAKE_VIDPID(0x046d, 0xc21e), "Logitech Gamepad F510" },
        { MAKE_VIDPID(0x046d, 0xc21f), "Logitech Gamepad F710" },
        { MAKE_VIDPID(0x046d, 0xc242), "Logitech Chillstream Controller" },
        { MAKE_VIDPID(0x046d, 0xcaa3), "Logitech DriveFx Racing Wheel" },
        { MAKE_VIDPID(0x056e, 0x2004), "Elecom JC-U3613M" },
        { MAKE_VIDPID(0x06a3, 0xf51a), "Saitek P3600" },
        { MAKE_VIDPID(0x0738, 0x4716), "Mad Catz Wired Xbox 360 Controller" },
        { MAKE_VIDPID(0x0738, 0x4718), "Mad Catz Street Fighter IV FightStick SE" },
        { MAKE_VIDPID(0x0738, 0x4726), "Mad Catz Xbox 360 Controller" },
        { MAKE_VIDPID(0x0738, 0x4728), "Mad Catz Street Fighter IV FightPad" },
        { MAKE_VIDPID(0x0738, 0x4736), "Mad Catz MicroCon Gamepad" },
        { MAKE_VIDPID(0x0738, 0x4738), "Mad Catz Wired Xbox 360 Controller (SFIV)" },
        { MAKE_VIDPID(0x0738, 0x4740), "Mad Catz Beat Pad" },
        { MAKE_VIDPID(0x0738, 0x4758), "Mad Catz Arcade Game Stick" },
        { MAKE_VIDPID(0x0738, 0x4a01), "Mad Catz FightStick TE 2" },
        { MAKE_VIDPID(0x0738, 0x9871), "Mad Catz Portable Drum" },
        { MAKE_VIDPID(0x0738, 0xb726), "Mad Catz Xbox controller - MW2" },
        { MAKE_VIDPID(0x0738, 0xb738), "Mad Catz MVC2TE Stick 2" },
        { MAKE_VIDPID(0x0738, 0xbeef), "Mad Catz JOYTECH NEO SE Advanced GamePad" },
        { MAKE_VIDPID(0x0738, 0xcb02), "Saitek Cyborg Rumble Pad - PC/Xbox 360" },
        { MAKE_VIDPID(0x0738, 0xcb03), "Saitek P3200 Rumble Pad - PC/Xbox 360" },
        { MAKE_VIDPID(0x0738, 0xcb29), "Saitek Aviator Stick AV8R02" },
        { MAKE_VIDPID(0x0738, 0xf738), "Super SFIV FightStick TE S" },
        { MAKE_VIDPID(0x07ff, 0xffff), "Mad Catz GamePad" },
        { MAKE_VIDPID(0x0e6f, 0x0105), "HSM3 Xbox360 dancepad" },
        { MAKE_VIDPID(0x0e6f, 0x0113), "Afterglow AX.1 Gamepad for Xbox 360" },
        { MAKE_VIDPID(0x0e6f, 0x011f), "Rock Candy Gamepad Wired Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0131), "PDP EA Sports Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0133), "Xbox 360 Wired Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0139), "Afterglow Prismatic Wired Controller" },
        { MAKE_VIDPID(0x0e6f, 0x013a), "PDP Xbox One Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0146), "Rock Candy Wired Controller for Xbox One" },
        { MAKE_VIDPID(0x0e6f, 0x0147), "PDP Marvel Xbox One Controller" },
        { MAKE_VIDPID(0x0e6f, 0x015c), "PDP Xbox One Arcade Stick" },
        { MAKE_VIDPID(0x0e6f, 0x0161), "PDP Xbox One Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0162), "PDP Xbox One Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0163), "PDP Xbox One Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0164), "PDP Battlefield One" },
        { MAKE_VIDPID(0x0e6f, 0x0165), "PDP Titanfall 2" },
        { MAKE_VIDPID(0x0e6f, 0x0201), "Pelican PL-3601 'TSZ' Wired Xbox 360 Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0213), "Afterglow Gamepad for Xbox 360" },
        { MAKE_VIDPID(0x0e6f, 0x021f), "Rock Candy Gamepad for Xbox 360" },
        { MAKE_VIDPID(0x0e6f, 0x0246), "Rock Candy Gamepad for Xbox One 2015" },
        { MAKE_VIDPID(0x0e6f, 0x02a4), "PDP Wired Controller for Xbox One - Stealth Series" },
        { MAKE_VIDPID(0x0e6f, 0x02ab), "PDP Controller for Xbox One" },
        { MAKE_VIDPID(0x0e6f, 0x0301), "Logic3 Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0346), "Rock Candy Gamepad for Xbox One 2016" },
        { MAKE_VIDPID(0x0e6f, 0x0401), "Logic3 Controller" },
        { MAKE_VIDPID(0x0e6f, 0x0413), "Afterglow AX.1 Gamepad for Xbox 360" },
        { MAKE_VIDPID(0x0e6f, 0x0501), "PDP Xbox 360 Controller" },
        { MAKE_VIDPID(0x0e6f, 0xf900), "PDP Afterglow AX.1" },
        { MAKE_VIDPID(0x0f0d, 0x000a), "Hori Co. DOA4 FightStick" },
        { MAKE_VIDPID(0x0f0d, 0x000c), "Hori PadEX Turbo" },
        { MAKE_VIDPID(0x0f0d, 0x000d), "Hori Fighting Stick EX2" },
        { MAKE_VIDPID(0x0f0d, 0x0016), "Hori Real Arcade Pro.EX" },
        { MAKE_VIDPID(0x0f0d, 0x001b), "Hori Real Arcade Pro VX" },
        { MAKE_VIDPID(0x0f0d, 0x0063), "Hori Real Arcade Pro Hayabusa (USA) Xbox One" },
        { MAKE_VIDPID(0x0f0d, 0x0067), "HORIPAD ONE" },
        { MAKE_VIDPID(0x0f0d, 0x0078), "Hori Real Arcade Pro V Kai Xbox One" },
        { MAKE_VIDPID(0x11c9, 0x55f0), "Nacon GC-100XF" },
        { MAKE_VIDPID(0x12ab, 0x0004), "Honey Bee Xbox360 dancepad" },
        { MAKE_VIDPID(0x12ab, 0x0301), "PDP AFTERGLOW AX.1" },
        { MAKE_VIDPID(0x12ab, 0x0303), "Mortal Kombat Klassic FightStick" },
        { MAKE_VIDPID(0x1430, 0x4748), "RedOctane Guitar Hero X-plorer" },
        { MAKE_VIDPID(0x1430, 0xf801), "RedOctane Controller" },
        { MAKE_VIDPID(0x146b, 0x0601), "BigBen Interactive XBOX 360 Controller" },
        { MAKE_VIDPID(0x1532, 0x0037), "Razer Sabertooth" },
        { MAKE_VIDPID(0x1532, 0x0a00), "Razer Atrox Arcade Stick" },
        { MAKE_VIDPID(0x1532, 0x0a03), "Razer Wildcat" },
        { MAKE_VIDPID(0x15e4, 0x3f00), "Power A Mini Pro Elite" },
        { MAKE_VIDPID(0x15e4, 0x3f0a), "Xbox Airflo wired controller" },
        { MAKE_VIDPID(0x15e4, 0x3f10), "Batarang Xbox 360 controller" },
        { MAKE_VIDPID(0x162e, 0xbeef), "Joytech Neo-Se Take2" },
        { MAKE_VIDPID(0x1689, 0xfd00), "Razer Onza Tournament Edition" },
        { MAKE_VIDPID(0x1689, 0xfd01), "Razer Onza Classic Edition" },
        { MAKE_VIDPID(0x1689, 0xfe00), "Razer Sabertooth" },
        { MAKE_VIDPID(0x1bad, 0x0002), "Harmonix Rock Band Guitar" },
        { MAKE_VIDPID(0x1bad, 0x0003), "Harmonix Rock Band Drumkit" },
        { MAKE_VIDPID(0x1bad, 0x0130), "Ion Drum Rocker" },
        { MAKE_VIDPID(0x1bad, 0xf016), "Mad Catz Xbox 360 Controller" },
        { MAKE_VIDPID(0x1bad, 0xf018), "Mad Catz Street Fighter IV SE Fighting Stick" },
        { MAKE_VIDPID(0x1bad, 0xf019), "Mad Catz Brawlstick for Xbox 360" },
        { MAKE_VIDPID(0x1bad, 0xf021), "Mad Cats Ghost Recon FS GamePad" },
        { MAKE_VIDPID(0x1bad, 0xf023), "MLG Pro Circuit Controller (Xbox)" },
        { MAKE_VIDPID(0x1bad, 0xf025), "Mad Catz Call Of Duty" },
        { MAKE_VIDPID(0x1bad, 0xf027), "Mad Catz FPS Pro" },
        { MAKE_VIDPID(0x1bad, 0xf028), "Street Fighter IV FightPad" },
        { MAKE_VIDPID(0x1bad, 0xf02e), "Mad Catz Fightpad" },
        { MAKE_VIDPID(0x1bad, 0xf030), "Mad Catz Xbox 360 MC2 MicroCon Racing Wheel" },
        { MAKE_VIDPID(0x1bad, 0xf036), "Mad Catz MicroCon GamePad Pro" },
        { MAKE_VIDPID(0x1bad, 0xf038), "Street Fighter IV FightStick TE" },
        { MAKE_VIDPID(0x1bad, 0xf039), "Mad Catz MvC2 TE" },
        { MAKE_VIDPID(0x1bad, 0xf03a), "Mad Catz SFxT Fightstick Pro" },
        { MAKE_VIDPID(0x1bad, 0xf03d), "Street Fighter IV Arcade Stick TE - Chun Li" },
        { MAKE_VIDPID(0x1bad, 0xf03e), "Mad Catz MLG FightStick TE" },
        { MAKE_VIDPID(0x1bad, 0xf03f), "Mad Catz FightStick SoulCaliber" },
        { MAKE_VIDPID(0x1bad, 0xf042), "Mad Catz FightStick TES+" },
        { MAKE_VIDPID(0x1bad, 0xf080), "Mad Catz FightStick TE2" },
        { MAKE_VIDPID(0x1bad, 0xf501), "HoriPad EX2 Turbo" },
        { MAKE_VIDPID(0x1bad, 0xf502), "Hori Real Arcade Pro.VX SA" },
        { MAKE_VIDPID(0x1bad, 0xf503), "Hori Fighting Stick VX" },
        { MAKE_VIDPID(0x1bad, 0xf504), "Hori Real Arcade Pro. EX" },
        { MAKE_VIDPID(0x1bad, 0xf505), "Hori Fighting Stick EX2B" },
        { MAKE_VIDPID(0x1bad, 0xf506), "Hori Real Arcade Pro.EX Premium VLX" },
        { MAKE_VIDPID(0x1bad, 0xf900), "Harmonix Xbox 360 Controller" },
        { MAKE_VIDPID(0x1bad, 0xf901), "Gamestop Xbox 360 Controller" },
        { MAKE_VIDPID(0x1bad, 0xf903), "Tron Xbox 360 controller" },
        { MAKE_VIDPID(0x1bad, 0xf904), "PDP Versus Fighting Pad" },
        { MAKE_VIDPID(0x1bad, 0xf906), "MortalKombat FightStick" },
        { MAKE_VIDPID(0x1bad, 0xfa01), "MadCatz GamePad" },
        { MAKE_VIDPID(0x1bad, 0xfd00), "Razer Onza TE" },
        { MAKE_VIDPID(0x1bad, 0xfd01), "Razer Onza" },
        { MAKE_VIDPID(0x24c6, 0x5000), "Razer Atrox Arcade Stick" },
        { MAKE_VIDPID(0x24c6, 0x5300), "PowerA MINI PROEX Controller" },
        { MAKE_VIDPID(0x24c6, 0x5303), "Xbox Airflo wired controller" },
        { MAKE_VIDPID(0x24c6, 0x530a), "Xbox 360 Pro EX Controller" },
        { MAKE_VIDPID(0x24c6, 0x531a), "PowerA Pro Ex" },
        { MAKE_VIDPID(0x24c6, 0x5397), "FUS1ON Tournament Controller" },
        { MAKE_VIDPID(0x24c6, 0x541a), "PowerA Xbox One Mini Wired Controller" },
        { MAKE_VIDPID(0x24c6, 0x542a), "Xbox ONE spectra" },
        { MAKE_VIDPID(0x24c6, 0x543a), "PowerA Xbox One wired controller" },
        { MAKE_VIDPID(0x24c6, 0x5500), "Hori XBOX 360 EX 2 with Turbo" },
        { MAKE_VIDPID(0x24c6, 0x5501), "Hori Real Arcade Pro VX-SA" },
        { MAKE_VIDPID(0x24c6, 0x5502), "Hori Fighting Stick VX Alt" },
        { MAKE_VIDPID(0x24c6, 0x5503), "Hori Fighting Edge" },
        { MAKE_VIDPID(0x24c6, 0x5506), "Hori SOULCALIBUR V Stick" },
        { MAKE_VIDPID(0x24c6, 0x550d), "Hori GEM Xbox controller" },
        { MAKE_VIDPID(0x24c6, 0x550e), "Hori Real Arcade Pro V Kai 360" },
        { MAKE_VIDPID(0x24c6, 0x551a), "PowerA FUSION Pro Controller" },
        { MAKE_VIDPID(0x24c6, 0x561a), "PowerA FUSION Controller" },
        { MAKE_VIDPID(0x24c6, 0x5b00), "ThrustMaster Ferrari 458 Racing Wheel" },
        { MAKE_VIDPID(0x24c6, 0x5b02), "Thrustmaster, Inc. GPX Controller" },
        { MAKE_VIDPID(0x24c6, 0x5b03), "Thrustmaster Ferrari 458 Racing Wheel" },
        { MAKE_VIDPID(0x24c6, 0x5d04), "Razer Sabertooth" },
        { MAKE_VIDPID(0x24c6, 0xfafe), "Rock Candy Gamepad for Xbox 360" },
    };
    int i;
    Uint32 vidpid = MAKE_VIDPID(vendor_id, product_id);

    for (i = 0; i < SDL_arraysize(names); ++i) {
        if (vidpid == names[i].vidpid) {
            return names[i].name;
        }
    }
    return NULL;
}

static SDL_bool
HIDAPI_IsDeviceSupported(Uint16 vendor_id, Uint16 product_id, Uint16 version)
{
    int i;

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled && driver->IsSupportedDevice(vendor_id, product_id, version, -1)) {
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

    if (SDL_ShouldIgnoreJoystick(device->name, device->guid)) {
        return NULL;
    }

    if (device->usage_page && device->usage_page != USAGE_PAGE_GENERIC_DESKTOP) {
        return NULL;
    }
    if (device->usage && device->usage != USAGE_JOYSTICK && device->usage != USAGE_GAMEPAD && device->usage != USAGE_MULTIAXISCONTROLLER) {
        return NULL;
    }

    for (i = 0; i < SDL_arraysize(SDL_HIDAPI_drivers); ++i) {
        SDL_HIDAPI_DeviceDriver *driver = SDL_HIDAPI_drivers[i];
        if (driver->enabled && driver->IsSupportedDevice(device->vendor_id, device->product_id, device->version, device->interface_number)) {
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
    HIDAPI_InitializeDiscovery();
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
    device->version = info->release_number;
    device->interface_number = info->interface_number;
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

    /* Need the device name before getting the driver to know whether to ignore this device */
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

    device->driver = HIDAPI_GetDeviceDriver(device);

    if (device->driver) {
        const char *name = device->driver->GetDeviceName(device->vendor_id, device->product_id);
        if (name) {
            SDL_free(device->name);
            device->name = SDL_strdup(name);
        }
    }

    device->path = SDL_strdup(info->path);
    if (!device->path) {
        SDL_free(device->name);
        SDL_free(device);
        return;
    }

#ifdef DEBUG_HIDAPI
    SDL_Log("Adding HIDAPI device '%s' VID 0x%.4x, PID 0x%.4x, version %d, interface %d, usage page 0x%.4x, usage 0x%.4x, driver = %s\n", device->name, device->vendor_id, device->product_id, device->version, device->interface_number, device->usage_page, device->usage, device->driver ? device->driver->hint : "NONE");
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
HIDAPI_IsDevicePresent(Uint16 vendor_id, Uint16 product_id, Uint16 version)
{
    SDL_HIDAPI_Device *device;

    /* Don't update the device list for devices we know aren't supported */
    if (!HIDAPI_IsDeviceSupported(vendor_id, product_id, version)) {
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
    HIDAPI_UpdateDiscovery();
    if (SDL_HIDAPI_discovery.m_bHaveDevicesChanged) {
        /* FIXME: We probably need to schedule an update in a few seconds as well */
        HIDAPI_UpdateDeviceList();
        SDL_HIDAPI_discovery.m_bHaveDevicesChanged = SDL_FALSE;
    }
}

static const char *
HIDAPI_JoystickGetDeviceName(int device_index)
{
    return HIDAPI_GetJoystickByIndex(device_index)->name;
}

static int
HIDAPI_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
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
    hwdata->mutex = SDL_CreateMutex();

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
    int result;

    SDL_LockMutex(hwdata->mutex);
    result = driver->Rumble(joystick, hwdata->dev, hwdata->context, low_frequency_rumble, high_frequency_rumble, duration_ms);
    SDL_UnlockMutex(hwdata->mutex);
    return result;
}

static void
HIDAPI_JoystickUpdate(SDL_Joystick * joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    SDL_HIDAPI_DeviceDriver *driver = hwdata->driver;
    SDL_bool succeeded;

    SDL_LockMutex(hwdata->mutex);
    succeeded = driver->Update(joystick, hwdata->dev, hwdata->context);
    SDL_UnlockMutex(hwdata->mutex);
    
    if (!succeeded) {
        SDL_HIDAPI_Device *device;
        for (device = SDL_HIDAPI_devices; device; device = device->next) {
            if (device->instance_id == joystick->instance_id) {
                HIDAPI_DelDevice(device, SDL_TRUE);
                break;
            }
        }
    }
}

static void
HIDAPI_JoystickClose(SDL_Joystick * joystick)
{
    struct joystick_hwdata *hwdata = joystick->hwdata;
    SDL_HIDAPI_DeviceDriver *driver = hwdata->driver;
    driver->Quit(joystick, hwdata->dev, hwdata->context);

    hid_close(hwdata->dev);
    SDL_DestroyMutex(hwdata->mutex);
    SDL_free(hwdata);
    joystick->hwdata = NULL;
}

static void
HIDAPI_JoystickQuit(void)
{
    int i;

    HIDAPI_ShutdownDiscovery();

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
    HIDAPI_JoystickGetDevicePlayerIndex,
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
