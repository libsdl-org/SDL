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

#ifdef SDL_JOYSTICK_LINUX

#ifndef SDL_INPUT_LINUXEV
#error SDL now requires a Linux 2.4+ kernel with /dev/input/event support.
#endif

// This is the Linux implementation of the SDL joystick API

#include <sys/stat.h>
#include <errno.h> // errno, strerror
#include <fcntl.h>
#include <limits.h> // For the definition of PATH_MAX
#ifdef HAVE_INOTIFY
#include <sys/inotify.h>
#include <string.h> // strerror
#endif
#include <sys/ioctl.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/joystick.h>

#include "../../events/SDL_events_c.h"
#include "../../core/linux/SDL_evdev.h"
#include "../../core/unix/SDL_poll.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../usb_ids.h"
#include "SDL_sysjoystick_c.h"
#include "../hidapi/SDL_hidapijoystick_c.h"

// This isn't defined in older Linux kernel headers
#ifndef MSC_TIMESTAMP
#define MSC_TIMESTAMP 0x05
#endif

#ifndef SYN_DROPPED
#define SYN_DROPPED 3
#endif
#ifndef BTN_NORTH
#define BTN_NORTH 0x133
#endif
#ifndef BTN_WEST
#define BTN_WEST 0x134
#endif
#ifndef BTN_DPAD_UP
#define BTN_DPAD_UP 0x220
#endif
#ifndef BTN_DPAD_DOWN
#define BTN_DPAD_DOWN 0x221
#endif
#ifndef BTN_DPAD_LEFT
#define BTN_DPAD_LEFT 0x222
#endif
#ifndef BTN_DPAD_RIGHT
#define BTN_DPAD_RIGHT 0x223
#endif
#ifndef BTN_GRIPL
#define BTN_GRIPL 0x224
#endif
#ifndef BTN_GRIPR
#define BTN_GRIPR 0x225
#endif
#ifndef BTN_GRIPL2
#define BTN_GRIPL2 0x226
#endif
#ifndef BTN_GRIPR2
#define BTN_GRIPR2 0x227
#endif

#ifndef BTN_TRIGGER_HAPPY
#define BTN_TRIGGER_HAPPY       0x2c0
#define BTN_TRIGGER_HAPPY1      0x2c0
#define BTN_TRIGGER_HAPPY2      0x2c1
#define BTN_TRIGGER_HAPPY3      0x2c2
#define BTN_TRIGGER_HAPPY4      0x2c3
#define BTN_TRIGGER_HAPPY5      0x2c4
#define BTN_TRIGGER_HAPPY6      0x2c5
#define BTN_TRIGGER_HAPPY7      0x2c6
#define BTN_TRIGGER_HAPPY8      0x2c7
#define BTN_TRIGGER_HAPPY9      0x2c8
#define BTN_TRIGGER_HAPPY10     0x2c9
#define BTN_TRIGGER_HAPPY11     0x2ca
#define BTN_TRIGGER_HAPPY12     0x2cb
#define BTN_TRIGGER_HAPPY13     0x2cc
#define BTN_TRIGGER_HAPPY14     0x2cd
#define BTN_TRIGGER_HAPPY15     0x2ce
#define BTN_TRIGGER_HAPPY16     0x2cf
#define BTN_TRIGGER_HAPPY17     0x2d0
#define BTN_TRIGGER_HAPPY18     0x2d1
#define BTN_TRIGGER_HAPPY19     0x2d2
#define BTN_TRIGGER_HAPPY20     0x2d3
#define BTN_TRIGGER_HAPPY21     0x2d4
#define BTN_TRIGGER_HAPPY22     0x2d5
#define BTN_TRIGGER_HAPPY23     0x2d6
#define BTN_TRIGGER_HAPPY24     0x2d7
#define BTN_TRIGGER_HAPPY25     0x2d8
#define BTN_TRIGGER_HAPPY26     0x2d9
#define BTN_TRIGGER_HAPPY27     0x2da
#define BTN_TRIGGER_HAPPY28     0x2db
#define BTN_TRIGGER_HAPPY29     0x2dc
#define BTN_TRIGGER_HAPPY30     0x2dd
#define BTN_TRIGGER_HAPPY31     0x2de
#define BTN_TRIGGER_HAPPY32     0x2df
#define BTN_TRIGGER_HAPPY33     0x2e0
#define BTN_TRIGGER_HAPPY34     0x2e1
#define BTN_TRIGGER_HAPPY35     0x2e2
#define BTN_TRIGGER_HAPPY36     0x2e3
#define BTN_TRIGGER_HAPPY37     0x2e4
#define BTN_TRIGGER_HAPPY38     0x2e5
#define BTN_TRIGGER_HAPPY39     0x2e6
#define BTN_TRIGGER_HAPPY40     0x2e7
#endif


#include "../../core/linux/SDL_evdev_capabilities.h"
#include "../../core/linux/SDL_udev.h"

#if 0
#define DEBUG_INPUT_EVENTS 1
#endif

#if 0
#define DEBUG_GAMEPAD_MAPPING 1
#endif

typedef enum
{
    ENUMERATION_UNSET,
    ENUMERATION_LIBUDEV,
    ENUMERATION_FALLBACK
} EnumerationMethod;

static EnumerationMethod enumeration_method = ENUMERATION_UNSET;

static bool IsJoystickJSNode(const char *node);
static bool IsJoystickDeviceNode(const char *node);
static void MaybeAddDevice(const char *path);
static void MaybeRemoveDevice(const char *path);

// A device node that has been checked, owned by the joystick device thread. Sensors and nodes that aren't
// joysticks we use have no instance ID. Other nodes are remembered so the polling scan doesn't open them
// again; a notification for the node checks it again.
typedef struct LINUX_KnownNode
{
    char *path; // "/dev/input/event2" or whatever
    dev_t devnum;
    ino_t ino; // A node that's removed and created again gets a new inode, even with the same device number
    SDL_JoystickID instance_id; // Driver instance ID, not the public joystick ID (see SDL_JoystickDevice)
    bool is_sensor;
    struct LINUX_KnownNode *next;
} LINUX_KnownNode;

// A gamepad sensor node. The Linux driver creates a separate device for the gyro and accelerometer, with
// the same unique ID as the joystick. Sensors are found on the device thread and paired with a joystick
// when it's opened.
typedef struct LINUX_SensorNode
{
    char *path;
    char uniq[128];
    bool in_use;
    struct LINUX_SensorNode *next;
} LINUX_SensorNode;

// SDL_JoystickDevice driver data
typedef struct LINUX_DeviceData
{
    Uint16 vendor;
    char driver[32]; // "xpad" or whatever
    char uniq[128];
    char serial[128];

    // The gamepad mapping, generated the first time it's needed. Only used on the app thread.
    bool checked_mapping;
    bool has_mapping;
    SDL_GamepadMapping mapping;
} LINUX_DeviceData;

static bool SDL_classic_joysticks = false;
static char *LINUX_hint_devices = NULL;
static bool LINUX_first_pass = false;
static LINUX_KnownNode *LINUX_known_nodes = NULL;
static SDL_Mutex *LINUX_sensor_lock = NULL;
static LINUX_SensorNode *LINUX_sensors SDL_GUARDED_BY(LINUX_sensor_lock) = NULL;
static int inotify_fd = -1;

static Uint64 last_joy_detect_time;
static time_t last_input_dir_mtime;

// Joysticks the app thread found were gone (read errors), for the device thread to forget. This matters
// when the device thread can't see the node change, e.g. a static /dev or a fast replug with polling.
typedef struct LINUX_GoneJoystick
{
    SDL_JoystickID instance_id; // Driver instance ID, not the public joystick ID (see SDL_JoystickDevice)
    struct LINUX_GoneJoystick *next;
} LINUX_GoneJoystick;

static SDL_SpinLock LINUX_gone_lock;
static LINUX_GoneJoystick *LINUX_gone_joysticks = NULL;

#ifdef SDL_USE_LIBUDEV
// The device thread has its own udev context, because a udev context can't be used by more than one thread
static const SDL_UDEV_Symbols *LINUX_udev_syms = NULL;
static struct udev *LINUX_udev = NULL;
static struct udev_monitor *LINUX_udev_mon = NULL;
#endif

#ifdef SDL_USE_LIBUDEV
static bool LINUX_GetProductInfo(const char *path, struct input_id *inpid, int *class, char **driver)
{
    if (LINUX_udev) {
        return SDL_UDEV_GetProductInfo(LINUX_udev, path, inpid, class, driver);
    }
    return false;
}

static void LINUX_GetProductSerial(const char *path, char *serial, size_t size)
{
    if (LINUX_udev) {
        char *value = SDL_UDEV_GetProductSerial(LINUX_udev, path);
        if (value) {
            SDL_strlcpy(serial, value, size);
            SDL_free(value);
        }
    }
}
#endif

static void FixupDeviceInfoForMapping(int fd, struct input_id *inpid)
{
    if (inpid->vendor == 0x045e && inpid->product == 0x0b05 && inpid->version == 0x0903) {
        // This is a Microsoft Xbox One Elite Series 2 controller
        unsigned long keybit[NBITS(KEY_MAX)] = { 0 };

        // The first version of the firmware duplicated all the inputs
        if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) &&
            test_bit(0x2c0, keybit)) {
            // Change the version to 0x0902, so we can map it differently
            inpid->version = 0x0902;
        }
    }

    /* For Atari vcs modern and classic controllers have the version reflecting
     * firmware version, but the mapping stays stable so ignore
     * version information */
    if (inpid->vendor == 0x3250 && (inpid->product == 0x1001 || inpid->product == 0x1002)) {
        inpid->version = 0;
    }
}

#ifdef SDL_JOYSTICK_HIDAPI
static bool IsVirtualJoystick(Uint16 vendor, Uint16 product, Uint16 version, const char *name)
{
    if (vendor == USB_VENDOR_MICROSOFT && product == USB_PRODUCT_XBOX_ONE_S && version == 0 &&
        SDL_strcmp(name, "Xbox One S Controller") == 0) {
        // This is the virtual device created by the xow driver
        return true;
    }
    return false;
}
#else
static bool IsVirtualJoystick(Uint16 vendor, Uint16 product, Uint16 version, const char *name)
{
    return false;
}
#endif // SDL_JOYSTICK_HIDAPI

static bool GetSteamVirtualGamepadSlot(int fd, int *slot)
{
    char name[128];

    if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) > 0) {
        const char *digits = SDL_strstr(name, "pad ");
        if (digits) {
            digits += 4;
            if (SDL_isdigit(*digits)) {
                *slot = SDL_atoi(digits);
                return true;
            }
        }
    }
    return false;
}

static int GuessDeviceClass(int fd)
{
    unsigned long propbit[NBITS(INPUT_PROP_MAX)] = { 0 };
    unsigned long evbit[NBITS(EV_MAX)] = { 0 };
    unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
    unsigned long absbit[NBITS(ABS_MAX)] = { 0 };
    unsigned long relbit[NBITS(REL_MAX)] = { 0 };

    if ((ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) ||
        (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
        (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit) < 0) ||
        (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0)) {
        return 0;
    }

    /* This is a newer feature, so it's allowed to fail - if so, then the
     * device just doesn't have any properties. */
    (void) ioctl(fd, EVIOCGPROP(sizeof(propbit)), propbit);

    return SDL_EVDEV_GuessDeviceClass(propbit, evbit, absbit, keybit, relbit);
}

static bool GuessIsJoystick(int fd)
{
    if (GuessDeviceClass(fd) & SDL_UDEV_DEVICE_JOYSTICK) {
        return true;
    }
    return false;
}

static bool GuessIsSensor(int fd)
{
    if (GuessDeviceClass(fd) & SDL_UDEV_DEVICE_ACCELEROMETER) {
        return true;
    }
    return false;
}

static bool IsJoystick(const char *path, int *fd, char **name_return, Uint16 *vendor_return, Uint16 *product_return, SDL_GUID *guid, char **driver_return)
{
    struct input_id inpid;
    char *name = NULL;
    char *driver = NULL;
    char product_string[128];
    int class = 0;

    SDL_zero(inpid);
#ifdef SDL_USE_LIBUDEV
    // Opening input devices can generate synchronous device I/O, so avoid it if we can
    if (LINUX_GetProductInfo(path, &inpid, &class, &driver) &&
        !(class & SDL_UDEV_DEVICE_JOYSTICK)) {
        goto error;
    }
#endif

    if (fd && *fd < 0) {
        *fd = open(path, O_RDONLY | O_CLOEXEC, 0);
    }
    if (!fd || *fd < 0) {
        goto error;
    }

    if (ioctl(*fd, JSIOCGNAME(sizeof(product_string)), product_string) <= 0) {
        // When udev enumeration or classification, we only got joysticks here, so no need to test
        if (enumeration_method != ENUMERATION_LIBUDEV && !class && !GuessIsJoystick(*fd)) {
            goto error;
        }

        // Could have vendor and product already from udev, but should agree with evdev
        if (ioctl(*fd, EVIOCGID, &inpid) < 0) {
            goto error;
        }

        if (ioctl(*fd, EVIOCGNAME(sizeof(product_string)), product_string) < 0) {
            goto error;
        }
    }

    name = SDL_CreateJoystickName(inpid.vendor, inpid.product, NULL, product_string);
    if (!name) {
        goto error;
    }

    FixupDeviceInfoForMapping(*fd, &inpid);

#ifdef DEBUG_JOYSTICK
    SDL_Log("Joystick: %s, bustype = %d, vendor = 0x%.4x, product = 0x%.4x, version = %d", name, inpid.bustype, inpid.vendor, inpid.product, inpid.version);
#endif

    if (SDL_ShouldIgnoreJoystick(inpid.vendor, inpid.product, inpid.version, name)) {
        goto error;
    }
    *name_return = name;
    *driver_return = driver;
    *vendor_return = inpid.vendor;
    *product_return = inpid.product;
    *guid = SDL_CreateJoystickGUID(inpid.bustype, inpid.vendor, inpid.product, inpid.version, NULL, product_string, 0, 0);
    return true;

error:
    SDL_free(driver);
    SDL_free(name);
    return false;
}

static bool IsSensor(const char *path, int *fd)
{
    struct input_id inpid;
    int class = 0;

    SDL_zero(inpid);
#ifdef SDL_USE_LIBUDEV
    // Opening input devices can generate synchronous device I/O, so avoid it if we can
    if (LINUX_GetProductInfo(path, &inpid, &class, NULL) &&
        !(class & SDL_UDEV_DEVICE_ACCELEROMETER)) {
        return false;
    }
#endif

    if (fd && *fd < 0) {
        *fd = open(path, O_RDONLY | O_CLOEXEC, 0);
    }
    if (!fd || *fd < 0) {
        return false;
    }

    if (!class && !GuessIsSensor(*fd)) {
        return false;
    }

    if (ioctl(*fd, EVIOCGID, &inpid) < 0) {
        return false;
    }

    if (inpid.vendor == USB_VENDOR_NINTENDO && inpid.product == USB_PRODUCT_NINTENDO_WII_REMOTE) {
        // Wii extension controls
        // These may create 3 sensor devices but we only support reading from 1: ignore them
        return false;
    }

    return true;
}

static LINUX_KnownNode *CreateKnownNode(const char *path, const struct stat *sb)
{
    LINUX_KnownNode *node = (LINUX_KnownNode *)SDL_calloc(1, sizeof(*node));
    if (!node) {
        return NULL;
    }
    node->path = SDL_strdup(path);
    if (!node->path) {
        SDL_free(node);
        return NULL;
    }
    node->devnum = sb->st_rdev;
    node->ino = sb->st_ino;
    return node;
}

static void FreeKnownNode(LINUX_KnownNode *node)
{
    if (node) {
        SDL_free(node->path);
        SDL_free(node);
    }
}

static void FreeSensorNode(LINUX_SensorNode *sensor)
{
    if (sensor) {
        SDL_free(sensor->path);
        SDL_free(sensor);
    }
}

static void ReadUniq(int fd, char *uniq, size_t size)
{
    SDL_memset(uniq, 0, size);
    if (ioctl(fd, EVIOCGUNIQ(size - 1), uniq) < 0) {
        uniq[0] = '\0';
    }
}

static void AddJoystick(const char *path, const struct stat *sb, int fd, char *name, char *driver, Uint16 vendor, Uint16 product, SDL_GUID guid)
{
    LINUX_KnownNode *node = CreateKnownNode(path, sb);
    LINUX_DeviceData *data = (LINUX_DeviceData *)SDL_calloc(1, sizeof(*data));
    SDL_JoystickDevice *device = NULL;
    int steam_virtual_gamepad_slot = -1;
    Uint16 version = 0;

    if (node && data) {
        device = SDL_CreateJoystickDevice(&SDL_LINUX_JoystickDriver);
    }
    if (!device) {
        FreeKnownNode(node);
        SDL_free(data);
        return;
    }

    data->vendor = vendor;
    if (driver) {
        SDL_strlcpy(data->driver, driver, sizeof(data->driver));
    }
    ReadUniq(fd, data->uniq, sizeof(data->uniq));
#ifdef SDL_USE_LIBUDEV
    LINUX_GetProductSerial(path, data->serial, sizeof(data->serial));
#endif
    device->driver_data = data;

    if (vendor == USB_VENDOR_VALVE &&
        product == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD) {
        GetSteamVirtualGamepadSlot(fd, &steam_virtual_gamepad_slot);
    }

    SDL_SetJoystickDeviceDescription(device, name, path, guid, steam_virtual_gamepad_slot);
    device->vendor_id = vendor;
    device->product_id = product;
    device->type = SDL_GetGamepadTypeFromVIDPID(vendor, product, name, false);

    // Virtual devices are never the same controller as another driver's device
    SDL_GetJoystickGUIDInfo(guid, NULL, NULL, &version, NULL);
    device->match.never_group = IsVirtualJoystick(vendor, product, version, name);

    node->instance_id = device->driver_instance_id;
    node->next = LINUX_known_nodes;
    LINUX_known_nodes = node;

    SDL_ReportJoystickDevice(device);
}

static void AddSensor(const char *path, const struct stat *sb, int fd)
{
    LINUX_KnownNode *node = CreateKnownNode(path, sb);
    LINUX_SensorNode *sensor = (LINUX_SensorNode *)SDL_calloc(1, sizeof(*sensor));

    if (sensor) {
        sensor->path = SDL_strdup(path);
    }
    if (!node || !sensor || !sensor->path) {
        FreeKnownNode(node);
        FreeSensorNode(sensor);
        return;
    }
    ReadUniq(fd, sensor->uniq, sizeof(sensor->uniq));

    SDL_LockMutex(LINUX_sensor_lock);
    sensor->next = LINUX_sensors;
    LINUX_sensors = sensor;
    SDL_UnlockMutex(LINUX_sensor_lock);

    node->is_sensor = true;
    node->next = LINUX_known_nodes;
    LINUX_known_nodes = node;
}

// Remember a node that isn't a joystick or sensor we use
static void AddOtherNode(const char *path, const struct stat *sb)
{
    LINUX_KnownNode *node = CreateKnownNode(path, sb);
    if (node) {
        node->next = LINUX_known_nodes;
        LINUX_known_nodes = node;
    }
}

// Called on the device thread
static void MaybeAddDevice(const char *path)
{
    struct stat sb;
    int fd = -1;
    char *name = NULL;
    char *driver = NULL;
    Uint16 vendor, product;
    SDL_GUID guid;
    LINUX_KnownNode *node;

    if (!path) {
        return;
    }

    if (stat(path, &sb) == -1) {
        return;
    }

    // Check to make sure it's not already in list.
    for (node = LINUX_known_nodes; node; node = node->next) {
        if (sb.st_rdev == node->devnum && sb.st_ino == node->ino) {
            return; // already have this one
        }
    }

    // If we can't open it (e.g. its permissions haven't been set up yet), it's checked again later
    fd = open(path, O_RDONLY | O_CLOEXEC, 0);
    if (fd < 0) {
        return;
    }

#ifdef DEBUG_INPUT_EVENTS
    SDL_Log("Checking %s", path);
#endif

    if (IsJoystick(path, &fd, &name, &vendor, &product, &guid, &driver)) {
#ifdef DEBUG_INPUT_EVENTS
        SDL_Log("found joystick: %s", path);
#endif
        AddJoystick(path, &sb, fd, name, driver, vendor, product, guid);
        SDL_free(name);
        SDL_free(driver);

    } else if (IsSensor(path, &fd)) {
#ifdef DEBUG_INPUT_EVENTS
        SDL_Log("found sensor: %s", path);
#endif
        AddSensor(path, &sb, fd);

    } else {
        AddOtherNode(path, &sb);
    }

    close(fd);
}

// Check a node again after a notification about it, if it was remembered as not being a joystick or sensor
static void MaybeRecheckDevice(const char *path)
{
    LINUX_KnownNode *node, *prev = NULL;

    for (node = LINUX_known_nodes; node; prev = node, node = node->next) {
        if (SDL_strcmp(path, node->path) == 0) {
            if (!node->instance_id && !node->is_sensor) {
                if (prev) {
                    prev->next = node->next;
                } else {
                    LINUX_known_nodes = node->next;
                }
                FreeKnownNode(node);
            }
            break;
        }
    }
    MaybeAddDevice(path);
}

static void RemoveKnownNode(LINUX_KnownNode *node)
{
    LINUX_KnownNode *curr, *prev = NULL;

    for (curr = LINUX_known_nodes; curr; prev = curr, curr = curr->next) {
        if (curr == node) {
            if (prev) {
                prev->next = curr->next;
            } else {
                LINUX_known_nodes = curr->next;
            }
            break;
        }
    }

    if (node->instance_id) {
        SDL_ReportJoystickRemoved(node->instance_id);
    } else if (node->is_sensor) {
        LINUX_SensorNode *sensor, *prev_sensor = NULL;

        SDL_LockMutex(LINUX_sensor_lock);
        for (sensor = LINUX_sensors; sensor; prev_sensor = sensor, sensor = sensor->next) {
            if (SDL_strcmp(sensor->path, node->path) == 0) {
                if (prev_sensor) {
                    prev_sensor->next = sensor->next;
                } else {
                    LINUX_sensors = sensor->next;
                }
                FreeSensorNode(sensor);
                break;
            }
        }
        SDL_UnlockMutex(LINUX_sensor_lock);
    }
    FreeKnownNode(node);
}

// Called on the device thread
static void MaybeRemoveDevice(const char *path)
{
    LINUX_KnownNode *node, *next;

    if (!path) {
        return;
    }

    // There can be more than one node on the path if a removal was missed, and they're all gone now
    for (node = LINUX_known_nodes; node; node = next) {
        next = node->next;
        if (SDL_strcmp(path, node->path) == 0) {
            RemoveKnownNode(node);
        }
    }
}

// Remove devices whose nodes have gone away, when we can't be notified of removals
static void RemoveMissingDevices(void)
{
    LINUX_KnownNode *node, *next;

    for (node = LINUX_known_nodes; node; node = next) {
        struct stat sb;

        next = node->next;
        if (stat(node->path, &sb) < 0 || sb.st_rdev != node->devnum || sb.st_ino != node->ino) {
            RemoveKnownNode(node);
        }
    }
}

// Pair a sensor with a joystick that's being opened, returning the sensor's path.
// Only sensors with the same non-empty unique ID as the joystick are paired.
static char *ClaimSensor(const char *uniq)
{
    LINUX_SensorNode *sensor;
    char *path = NULL;

    if (!*uniq) {
        return NULL;
    }

    SDL_LockMutex(LINUX_sensor_lock);
    for (sensor = LINUX_sensors; sensor; sensor = sensor->next) {
        if (!sensor->in_use && SDL_strcmp(sensor->uniq, uniq) == 0) {
            path = SDL_strdup(sensor->path);
            if (path) {
                sensor->in_use = true;
            }
            break;
        }
    }
    SDL_UnlockMutex(LINUX_sensor_lock);

    return path;
}

static void ReleaseSensor(const char *path)
{
    LINUX_SensorNode *sensor;

    if (!path) {
        return;
    }

    SDL_LockMutex(LINUX_sensor_lock);
    for (sensor = LINUX_sensors; sensor; sensor = sensor->next) {
        if (sensor->in_use && SDL_strcmp(sensor->path, path) == 0) {
            sensor->in_use = false;
            break;
        }
    }
    SDL_UnlockMutex(LINUX_sensor_lock);
}

static bool StrIsInteger(const char *string)
{
    const char *p;

    if (*string == '\0') {
        return false;
    }

    for (p = string; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return false;
        }
    }

    return true;
}

static bool IsJoystickJSNode(const char *node)
{
    const char *last_slash = SDL_strrchr(node, '/');
    if (last_slash) {
        node = last_slash + 1;
    }
    return SDL_startswith(node, "js") && StrIsInteger(node + 2);
}

static bool IsJoystickEventNode(const char *node)
{
    const char *last_slash = SDL_strrchr(node, '/');
    if (last_slash) {
        node = last_slash + 1;
    }
    return SDL_startswith(node, "event") && StrIsInteger(node + 5);
}

static bool IsJoystickDeviceNode(const char *node)
{
    if (SDL_classic_joysticks) {
        return IsJoystickJSNode(node);
    } else {
        return IsJoystickEventNode(node);
    }
}

#ifdef HAVE_INOTIFY
#ifdef HAVE_INOTIFY_INIT1
static int SDL_inotify_init1(void)
{
    return inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
}
#else
static int SDL_inotify_init1(void)
{
    int fd = inotify_init();
    if (fd < 0) {
        return -1;
    }
    fcntl(fd, F_SETFL, O_NONBLOCK);
    fcntl(fd, F_SETFD, FD_CLOEXEC);
    return fd;
}
#endif

static void LINUX_InotifyJoystickDetect(void)
{
    union
    {
        struct inotify_event event;
        char storage[4096];
        char enough_for_inotify[sizeof(struct inotify_event) + NAME_MAX + 1];
    } buf;
    ssize_t bytes;
    size_t remain = 0;
    size_t len;
    char path[PATH_MAX];

    bytes = read(inotify_fd, &buf, sizeof(buf));

    if (bytes > 0) {
        remain = (size_t)bytes;
    }

    while (remain > 0) {
        if (buf.event.len > 0) {
            if (IsJoystickDeviceNode(buf.event.name)) {
                (void)SDL_snprintf(path, SDL_arraysize(path), "/dev/input/%s", buf.event.name);

                if (buf.event.mask & (IN_CREATE | IN_MOVED_TO | IN_ATTRIB)) {
                    MaybeRecheckDevice(path);
                } else if (buf.event.mask & (IN_DELETE | IN_MOVED_FROM)) {
                    MaybeRemoveDevice(path);
                }
            }
        }

        len = sizeof(struct inotify_event) + buf.event.len;
        remain -= len;

        if (remain != 0) {
            SDL_memmove(&buf.storage[0], &buf.storage[len], remain);
        }
    }
}
#endif // HAVE_INOTIFY

static int get_event_joystick_index(int event)
{
    int joystick_index = -1;
    int i, count;
    struct dirent **entries = NULL;
    char path[PATH_MAX];

    (void)SDL_snprintf(path, SDL_arraysize(path), "/sys/class/input/event%d/device", event);
    count = scandir(path, &entries, NULL, alphasort);
    for (i = 0; i < count; ++i) {
        if (SDL_strncmp(entries[i]->d_name, "js", 2) == 0) {
            joystick_index = SDL_atoi(entries[i]->d_name + 2);
        }
        free(entries[i]); // This should NOT be SDL_free()
    }
    free(entries); // This should NOT be SDL_free()

    return joystick_index;
}

/* Detect devices by reading /dev/input. In the inotify code path we
 * have to do this the first time, to detect devices that already existed
 * before we started; in the non-inotify code path we do this repeatedly
 * (polling). */
static int filter_entries(const struct dirent *entry)
{
    return IsJoystickDeviceNode(entry->d_name);
}
static int SDLCALL sort_entries(const void *_a, const void *_b)
{
    const struct dirent **a = (const struct dirent **)_a;
    const struct dirent **b = (const struct dirent **)_b;
    int numA, numB;
    int offset;

    if (SDL_classic_joysticks) {
        offset = 2; // strlen("js")
        numA = SDL_atoi((*a)->d_name + offset);
        numB = SDL_atoi((*b)->d_name + offset);
    } else {
        offset = 5; // strlen("event")
        numA = SDL_atoi((*a)->d_name + offset);
        numB = SDL_atoi((*b)->d_name + offset);

        // See if we can get the joystick ordering
        {
            int jsA = get_event_joystick_index(numA);
            int jsB = get_event_joystick_index(numB);
            if (jsA >= 0 && jsB >= 0) {
                numA = jsA;
                numB = jsB;
            } else if (jsA >= 0) {
                return -1;
            } else if (jsB >= 0) {
                return 1;
            }
        }
    }
    return numA - numB;
}

typedef struct
{
    char *path;
    int slot;
} VirtualGamepadEntry;

static int SDLCALL sort_virtual_gamepads(const void *_a, const void *_b)
{
    const VirtualGamepadEntry *a = (const VirtualGamepadEntry *)_a;
    const VirtualGamepadEntry *b = (const VirtualGamepadEntry *)_b;
    return a->slot - b->slot;
}

static void LINUX_ScanSteamVirtualGamepads(void)
{
    int i, count;
    int fd;
    struct dirent **entries = NULL;
    char path[PATH_MAX];
    struct input_id inpid;
    int num_virtual_gamepads = 0;
    int virtual_gamepad_slot;
    VirtualGamepadEntry *virtual_gamepads = NULL;
#ifdef SDL_USE_LIBUDEV
    int class;
#endif

    count = scandir("/dev/input", &entries, filter_entries, NULL);
    for (i = 0; i < count; ++i) {
        (void)SDL_snprintf(path, SDL_arraysize(path), "/dev/input/%s", entries[i]->d_name);

#ifdef SDL_USE_LIBUDEV
        // Opening input devices can generate synchronous device I/O, so avoid it if we can
        class = 0;
        SDL_zero(inpid);
        if (LINUX_GetProductInfo(path, &inpid, &class, NULL) &&
            (inpid.vendor != USB_VENDOR_VALVE || inpid.product != USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD)) {
            free(entries[i]); // This should NOT be SDL_free()
            continue;
        }
#endif
        fd = open(path, O_RDONLY | O_CLOEXEC, 0);
        if (fd >= 0) {
            if (ioctl(fd, EVIOCGID, &inpid) == 0 &&
                inpid.vendor == USB_VENDOR_VALVE &&
                inpid.product == USB_PRODUCT_STEAM_VIRTUAL_GAMEPAD &&
                GetSteamVirtualGamepadSlot(fd, &virtual_gamepad_slot)) {
                VirtualGamepadEntry *new_virtual_gamepads = (VirtualGamepadEntry *)SDL_realloc(virtual_gamepads, (num_virtual_gamepads + 1) * sizeof(*virtual_gamepads));
                if (new_virtual_gamepads) {
                    VirtualGamepadEntry *entry = &new_virtual_gamepads[num_virtual_gamepads];
                    entry->path = SDL_strdup(path);
                    entry->slot = virtual_gamepad_slot;
                    if (entry->path) {
                        virtual_gamepads = new_virtual_gamepads;
                        ++num_virtual_gamepads;
                    } else {
                        SDL_free(entry->path);
                        SDL_free(new_virtual_gamepads);
                    }
                }
            }
            close(fd);
        }
        free(entries[i]); // This should NOT be SDL_free()
    }
    free(entries); // This should NOT be SDL_free()

    if (num_virtual_gamepads > 1) {
        SDL_qsort(virtual_gamepads, num_virtual_gamepads, sizeof(*virtual_gamepads), sort_virtual_gamepads);
    }
    for (i = 0; i < num_virtual_gamepads; ++i) {
        MaybeAddDevice(virtual_gamepads[i].path);
        SDL_free(virtual_gamepads[i].path);
    }
    SDL_free(virtual_gamepads);
}

static void LINUX_ScanInputDevices(void)
{
    int i, count;
    struct dirent **entries = NULL;
    char path[PATH_MAX];

    count = scandir("/dev/input", &entries, filter_entries, NULL);
    if (count > 1) {
        SDL_qsort(entries, count, sizeof(*entries), sort_entries);
    }
    for (i = 0; i < count; ++i) {
        (void)SDL_snprintf(path, SDL_arraysize(path), "/dev/input/%s", entries[i]->d_name);
        MaybeAddDevice(path);

        free(entries[i]); // This should NOT be SDL_free()
    }
    free(entries); // This should NOT be SDL_free()
}

#ifdef SDL_USE_LIBUDEV
static void LINUX_UdevJoystickDetect(void)
{
    const int fd = LINUX_udev_syms->udev_monitor_get_fd(LINUX_udev_mon);

    while (SDL_IOReady(fd, SDL_IOR_READ, 0) > 0) {
        struct udev_device *dev = LINUX_udev_syms->udev_monitor_receive_device(LINUX_udev_mon);
        const char *action;
        const char *devnode;

        if (!dev) {
            break;
        }

        action = LINUX_udev_syms->udev_device_get_action(dev);
        devnode = LINUX_udev_syms->udev_device_get_devnode(dev);
        if (action && devnode && IsJoystickDeviceNode(devnode)) {
            if (SDL_strcmp(action, "add") == 0) {
                struct input_id inpid;
                int class = 0;

                SDL_zero(inpid);
                if (LINUX_GetProductInfo(devnode, &inpid, &class, NULL) &&
                    (class & (SDL_UDEV_DEVICE_JOYSTICK | SDL_UDEV_DEVICE_ACCELEROMETER))) {
                    // Wait a bit for the hidraw udev node to initialize
                    SDL_Delay(10);

                    MaybeRecheckDevice(devnode);
                }
            } else if (SDL_strcmp(action, "remove") == 0) {
                MaybeRemoveDevice(devnode);
            }
        }

        LINUX_udev_syms->udev_device_unref(dev);
    }
}
#endif // SDL_USE_LIBUDEV

static Sint32 LINUX_FallbackJoystickDetect(void)
{
    const Uint32 SDL_JOY_DETECT_INTERVAL_MS = 3000; // Update every 3 seconds
    Uint64 now = SDL_GetTicks();

    if (!last_joy_detect_time || now >= (last_joy_detect_time + SDL_JOY_DETECT_INTERVAL_MS)) {
        struct stat sb;

        // Opening input devices can generate synchronous device I/O, so avoid it if we can
        if (stat("/dev/input", &sb) == 0 && sb.st_mtime != last_input_dir_mtime) {
            RemoveMissingDevices();

            // Look for Steam virtual gamepads first, and sort by Steam controller slot
            LINUX_ScanSteamVirtualGamepads();

            LINUX_ScanInputDevices();

            last_input_dir_mtime = sb.st_mtime;
        }

        last_joy_detect_time = now;
    }

    return (Sint32)(last_joy_detect_time + SDL_JOY_DETECT_INTERVAL_MS - now);
}

// Ask the device thread to forget a joystick that has gone away. Called on the app thread.
static void LINUX_ForgetJoystick(SDL_JoystickID instance_id)
{
    LINUX_GoneJoystick *gone = (LINUX_GoneJoystick *)SDL_malloc(sizeof(*gone));
    if (!gone) {
        return;
    }
    gone->instance_id = instance_id;

    SDL_LockSpinlock(&LINUX_gone_lock);
    gone->next = LINUX_gone_joysticks;
    LINUX_gone_joysticks = gone;
    SDL_UnlockSpinlock(&LINUX_gone_lock);

    SDL_WakeJoystickDeviceThread();
}

static LINUX_GoneJoystick *LINUX_TakeGoneJoysticks(void)
{
    LINUX_GoneJoystick *gone;

    SDL_LockSpinlock(&LINUX_gone_lock);
    gone = LINUX_gone_joysticks;
    LINUX_gone_joysticks = NULL;
    SDL_UnlockSpinlock(&LINUX_gone_lock);

    return gone;
}

// Forget the joysticks the app thread found were gone, so their nodes are checked again. Device thread.
static void LINUX_ApplyGoneJoysticks(void)
{
    LINUX_GoneJoystick *gone = LINUX_TakeGoneJoysticks();

    while (gone) {
        LINUX_GoneJoystick *next = gone->next;
        LINUX_KnownNode *node;

        for (node = LINUX_known_nodes; node; node = node->next) {
            if (node->instance_id == gone->instance_id) {
                RemoveKnownNode(node);
                break;
            }
        }
        SDL_free(gone);
        gone = next;

        // Without notifications, scan again even if /dev/input hasn't changed
        last_input_dir_mtime = 0;
        last_joy_detect_time = 0;
    }
}

// Called on the joystick device thread
static Sint32 LINUX_JoystickDiscover(void)
{
    LINUX_ApplyGoneJoysticks();

    if (LINUX_first_pass) {
        LINUX_first_pass = false;

        // First see if the user specified one or more joysticks to use
        if (LINUX_hint_devices) {
            char *envpath = LINUX_hint_devices;
            while (envpath) {
                char *delim = SDL_strchr(envpath, ':');
                if (delim) {
                    *delim++ = '\0';
                }
                MaybeAddDevice(envpath);
                envpath = delim;
            }
        }

        // Scan before handling notifications, since we sort by device number and udev doesn't.
        // Notifications for devices found here are ignored.
        if (enumeration_method == ENUMERATION_LIBUDEV || inotify_fd >= 0) {
            // Look for Steam virtual gamepads first, and sort by Steam controller slot
            LINUX_ScanSteamVirtualGamepads();

            LINUX_ScanInputDevices();
        }
    }

#ifdef SDL_USE_LIBUDEV
    if (enumeration_method == ENUMERATION_LIBUDEV) {
        LINUX_UdevJoystickDetect();
        return -1;
    }
#endif
#ifdef HAVE_INOTIFY
    if (inotify_fd >= 0) {
        LINUX_InotifyJoystickDetect();
        return -1;
    }
#endif
    return LINUX_FallbackJoystickDetect();
}

static int LINUX_JoystickGetDiscoveryFD(void)
{
#ifdef SDL_USE_LIBUDEV
    if (LINUX_udev_mon) {
        return LINUX_udev_syms->udev_monitor_get_fd(LINUX_udev_mon);
    }
#endif
    return inotify_fd;
}

#ifdef SDL_USE_LIBUDEV
static void LINUX_QuitUdev(void)
{
    if (LINUX_udev_mon) {
        LINUX_udev_syms->udev_monitor_unref(LINUX_udev_mon);
        LINUX_udev_mon = NULL;
    }
    if (LINUX_udev) {
        LINUX_udev_syms->udev_unref(LINUX_udev);
        LINUX_udev = NULL;
    }
    if (LINUX_udev_syms) {
        SDL_UDEV_ReleaseUdevSyms();
        LINUX_udev_syms = NULL;
    }
}

// Set up the udev context used to look up devices. This is used whenever udev is available, even when
// devices aren't found with udev, because only udev's event notification is unreliable in containers.
static bool LINUX_InitUdev(void)
{
    LINUX_udev_syms = SDL_UDEV_GetUdevSyms();
    if (!LINUX_udev_syms) {
        return false;
    }

    LINUX_udev = LINUX_udev_syms->udev_new();
    if (!LINUX_udev) {
        LINUX_QuitUdev();
        return false;
    }
    return true;
}

// Set up udev event notification, for finding devices with udev
static bool LINUX_InitUdevMonitor(void)
{
    if (!LINUX_udev) {
        return false;
    }

    LINUX_udev_mon = LINUX_udev_syms->udev_monitor_new_from_netlink(LINUX_udev, "udev");
    if (!LINUX_udev_mon) {
        return false;
    }
    LINUX_udev_syms->udev_monitor_filter_add_match_subsystem_devtype(LINUX_udev_mon, "input", NULL);
    LINUX_udev_syms->udev_monitor_enable_receiving(LINUX_udev_mon);
    return true;
}
#endif // SDL_USE_LIBUDEV

static bool LINUX_JoystickInit(void)
{
    const char *devices = SDL_GetHint(SDL_HINT_JOYSTICK_DEVICE);

    SDL_classic_joysticks = SDL_GetHintBoolean(SDL_HINT_JOYSTICK_LINUX_CLASSIC, false);

    enumeration_method = ENUMERATION_UNSET;

    LINUX_sensor_lock = SDL_CreateMutex();
    if (!LINUX_sensor_lock) {
        return false;
    }

    // The device thread looks for these on its first pass
    if (devices) {
        LINUX_hint_devices = SDL_strdup(devices);
    }
    LINUX_first_pass = true;

    // Force immediate joystick detection if using fallback
    last_joy_detect_time = 0;
    last_input_dir_mtime = 0;

#ifdef SDL_USE_LIBUDEV
    LINUX_InitUdev();

    if (SDL_GetHintBoolean("SDL_JOYSTICK_DISABLE_UDEV", false)) {
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "udev disabled by SDL_JOYSTICK_DISABLE_UDEV");
        enumeration_method = ENUMERATION_FALLBACK;
    } else if (SDL_GetSandbox() != SDL_SANDBOX_NONE) {
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "Container detected, disabling udev integration");
        enumeration_method = ENUMERATION_FALLBACK;
    } else if (LINUX_InitUdevMonitor()) {
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "Using udev for joystick device discovery");
        enumeration_method = ENUMERATION_LIBUDEV;
    } else {
        SDL_LogDebug(SDL_LOG_CATEGORY_INPUT,
                     "udev init failed, disabling udev integration");
        enumeration_method = ENUMERATION_FALLBACK;
    }
#endif

    if (enumeration_method != ENUMERATION_LIBUDEV) {
#ifdef HAVE_INOTIFY
        inotify_fd = SDL_inotify_init1();

        if (inotify_fd < 0) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                        "Unable to initialize inotify, falling back to polling: %s",
                        strerror(errno));
        } else {
            /* We need to watch for attribute changes in addition to
             * creation, because when a device is first created, it has
             * permissions that we can't read. When udev chmods it to
             * something that we maybe *can* read, we'll get an
             * IN_ATTRIB event to tell us. */
            if (inotify_add_watch(inotify_fd, "/dev/input",
                                  IN_CREATE | IN_DELETE | IN_MOVE | IN_ATTRIB) < 0) {
                close(inotify_fd);
                inotify_fd = -1;
                SDL_LogWarn(SDL_LOG_CATEGORY_INPUT,
                            "Unable to add inotify watch, falling back to polling: %s",
                            strerror(errno));
            }
        }
#endif // HAVE_INOTIFY
    }

    return true;
}

static bool allocate_balldata(SDL_Joystick *joystick)
{
    SDL_AssertJoysticksLocked();

    joystick->hwdata->balls =
        (struct hwdata_ball *)SDL_calloc(joystick->nballs, sizeof(struct hwdata_ball));
    if (joystick->hwdata->balls == NULL) {
        return false;
    }
    return true;
}

static bool allocate_hatdata(SDL_Joystick *joystick)
{
    int i;

    SDL_AssertJoysticksLocked();

    joystick->hwdata->hats =
        (struct hwdata_hat *)SDL_malloc(joystick->nhats *
                                        sizeof(struct hwdata_hat));
    if (!joystick->hwdata->hats) {
        return false;
    }
    for (i = 0; i < joystick->nhats; ++i) {
        joystick->hwdata->hats[i].axis[0] = 1;
        joystick->hwdata->hats[i].axis[1] = 1;
    }
    return true;
}

static bool GuessIfAxesAreDigitalHat(struct input_absinfo *absinfo_x, struct input_absinfo *absinfo_y)
{
    /* A "hat" is assumed to be a digital input with at most 9 possible states
     * (3 per axis: negative/zero/positive), as opposed to a true "axis" which
     * can report a continuous range of possible values. Unfortunately the Linux
     * joystick interface makes no distinction between digital hat axes and any
     * other continuous analog axis, so we have to guess. */

    // If both axes are missing, they're not anything.
    if (!absinfo_x && !absinfo_y) {
        return false;
    }

    // If the hint says so, treat all hats as digital.
    if (SDL_GetHintBoolean(SDL_HINT_JOYSTICK_LINUX_DIGITAL_HATS, false)) {
        return true;
    }

    // If both axes have ranges constrained between -1 and 1, they're definitely digital.
    if ((!absinfo_x || (absinfo_x->minimum == -1 && absinfo_x->maximum == 1)) && (!absinfo_y || (absinfo_y->minimum == -1 && absinfo_y->maximum == 1))) {
        return true;
    }

    // If both axes lack fuzz, flat, and resolution values, they're probably digital.
    if ((!absinfo_x || (!absinfo_x->fuzz && !absinfo_x->flat && !absinfo_x->resolution)) && (!absinfo_y || (!absinfo_y->fuzz && !absinfo_y->flat && !absinfo_y->resolution))) {
        return true;
    }

    // Otherwise, treat them as analog.
    return false;
}

static void ConfigJoystick(SDL_Joystick *joystick, int fd, int fd_sensor)
{
    int i, t;
    unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
    unsigned long absbit[NBITS(ABS_MAX)] = { 0 };
    unsigned long relbit[NBITS(REL_MAX)] = { 0 };
    unsigned long ffbit[NBITS(FF_MAX)] = { 0 };
    Uint8 key_pam_size, abs_pam_size;
    bool use_deadzones = SDL_GetHintBoolean(SDL_HINT_JOYSTICK_LINUX_DEADZONES, false);
    bool use_hat_deadzones = SDL_GetHintBoolean(SDL_HINT_JOYSTICK_LINUX_HAT_DEADZONES, true);

    SDL_AssertJoysticksLocked();

    // See if this device uses the new unified event API
    if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) &&
        (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0) &&
        (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit) >= 0)) {

        // Get the number of buttons, axes, and other thingamajigs
        for (i = BTN_JOYSTICK; i < KEY_MAX; ++i) {
            if (test_bit(i, keybit)) {
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick has button: 0x%x", i);
#endif
                joystick->hwdata->key_map[i] = joystick->nbuttons;
                joystick->hwdata->has_key[i] = true;
                ++joystick->nbuttons;
            }
        }
        for (i = 0; i < BTN_JOYSTICK; ++i) {
            if (test_bit(i, keybit)) {
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick has button: 0x%x", i);
#endif
                joystick->hwdata->key_map[i] = joystick->nbuttons;
                joystick->hwdata->has_key[i] = true;
                ++joystick->nbuttons;
            }
        }
        for (i = ABS_HAT0X; i <= ABS_HAT3Y; i += 2) {
            int hat_x = -1;
            int hat_y = -1;
            struct input_absinfo absinfo_x;
            struct input_absinfo absinfo_y;
            if (test_bit(i, absbit)) {
                hat_x = ioctl(fd, EVIOCGABS(i), &absinfo_x);
            }
            if (test_bit(i + 1, absbit)) {
                hat_y = ioctl(fd, EVIOCGABS(i + 1), &absinfo_y);
            }
            if (GuessIfAxesAreDigitalHat((hat_x < 0 ? (void *)0 : &absinfo_x),
                                         (hat_y < 0 ? (void *)0 : &absinfo_y))) {
                const int hat_index = (i - ABS_HAT0X) / 2;
                struct hat_axis_correct *correct = &joystick->hwdata->hat_correct[hat_index];
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick has digital hat: #%d", hat_index);
                if (hat_x >= 0) {
                    SDL_Log("X Values = { val:%d, min:%d, max:%d, fuzz:%d, flat:%d, res:%d }",
                            absinfo_x.value, absinfo_x.minimum, absinfo_x.maximum,
                            absinfo_x.fuzz, absinfo_x.flat, absinfo_x.resolution);
                }
                if (hat_y >= 0) {
                    SDL_Log("Y Values = { val:%d, min:%d, max:%d, fuzz:%d, flat:%d, res:%d }",
                            absinfo_y.value, absinfo_y.minimum, absinfo_y.maximum,
                            absinfo_y.fuzz, absinfo_y.flat, absinfo_y.resolution);
                }
#endif // DEBUG_INPUT_EVENTS
                joystick->hwdata->hats_indices[hat_index] = joystick->nhats;
                joystick->hwdata->has_hat[hat_index] = true;
                correct->use_deadzones = use_hat_deadzones;
                correct->minimum[0] = (hat_x < 0) ? -1 : absinfo_x.minimum;
                correct->maximum[0] = (hat_x < 0) ? 1 : absinfo_x.maximum;
                correct->minimum[1] = (hat_y < 0) ? -1 : absinfo_y.minimum;
                correct->maximum[1] = (hat_y < 0) ? 1 : absinfo_y.maximum;
                ++joystick->nhats;
            }
        }
        for (i = 0; i < ABS_MAX; ++i) {
            // Skip digital hats
            if (i >= ABS_HAT0X && i <= ABS_HAT3Y && joystick->hwdata->has_hat[(i - ABS_HAT0X) / 2]) {
                continue;
            }
            if (test_bit(i, absbit)) {
                struct input_absinfo absinfo;
                struct axis_correct *correct = &joystick->hwdata->abs_correct[i];

                if (ioctl(fd, EVIOCGABS(i), &absinfo) < 0) {
                    continue;
                }
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick has absolute axis: 0x%.2x", i);
                SDL_Log("Values = { val:%d, min:%d, max:%d, fuzz:%d, flat:%d, res:%d }",
                        absinfo.value, absinfo.minimum, absinfo.maximum,
                        absinfo.fuzz, absinfo.flat, absinfo.resolution);
#endif // DEBUG_INPUT_EVENTS
                joystick->hwdata->abs_map[i] = joystick->naxes;
                joystick->hwdata->has_abs[i] = true;

                correct->minimum = absinfo.minimum;
                correct->maximum = absinfo.maximum;
                if (correct->minimum != correct->maximum) {
                    if (use_deadzones) {
                        correct->use_deadzones = true;
                        correct->coef[0] = (absinfo.maximum + absinfo.minimum) - 2 * absinfo.flat;
                        correct->coef[1] = (absinfo.maximum + absinfo.minimum) + 2 * absinfo.flat;
                        t = ((absinfo.maximum - absinfo.minimum) - 4 * absinfo.flat);
                        if (t != 0) {
                            correct->coef[2] = (1 << 28) / t;
                        } else {
                            correct->coef[2] = 0;
                        }
                    } else {
                        float value_range = (correct->maximum - correct->minimum);
                        float output_range = (SDL_JOYSTICK_AXIS_MAX - SDL_JOYSTICK_AXIS_MIN);

                        correct->scale = (output_range / value_range);
                    }
                }
                ++joystick->naxes;
            }
        }
        if (test_bit(REL_X, relbit) || test_bit(REL_Y, relbit)) {
            ++joystick->nballs;
        }

    } else if ((ioctl(fd, JSIOCGBUTTONS, &key_pam_size, sizeof(key_pam_size)) >= 0) &&
               (ioctl(fd, JSIOCGAXES, &abs_pam_size, sizeof(abs_pam_size)) >= 0)) {
        size_t len;

        joystick->hwdata->classic = true;

        len = (KEY_MAX - BTN_MISC + 1) * sizeof(*joystick->hwdata->key_pam);
        joystick->hwdata->key_pam = (Uint16 *)SDL_calloc(1, len);
        if (joystick->hwdata->key_pam) {
            if (ioctl(fd, JSIOCGBTNMAP, joystick->hwdata->key_pam, len) < 0) {
                SDL_free(joystick->hwdata->key_pam);
                joystick->hwdata->key_pam = NULL;
                key_pam_size = 0;
            }
        } else {
            key_pam_size = 0;
        }
        for (i = 0; i < key_pam_size; ++i) {
            Uint16 code = joystick->hwdata->key_pam[i];
#ifdef DEBUG_INPUT_EVENTS
            SDL_Log("Joystick has button: 0x%x", code);
#endif
            joystick->hwdata->key_map[code] = joystick->nbuttons;
            joystick->hwdata->has_key[code] = true;
            ++joystick->nbuttons;
        }

        len = ABS_CNT * sizeof(*joystick->hwdata->abs_pam);
        joystick->hwdata->abs_pam = (Uint8 *)SDL_calloc(1, len);
        if (joystick->hwdata->abs_pam) {
            if (ioctl(fd, JSIOCGAXMAP, joystick->hwdata->abs_pam, len) < 0) {
                SDL_free(joystick->hwdata->abs_pam);
                joystick->hwdata->abs_pam = NULL;
                abs_pam_size = 0;
            }
        } else {
            abs_pam_size = 0;
        }
        for (i = 0; i < abs_pam_size; ++i) {
            Uint8 code = joystick->hwdata->abs_pam[i];

            // TODO: is there any way to detect analog hats in advance via this API?
            if (code >= ABS_HAT0X && code <= ABS_HAT3Y) {
                int hat_index = (code - ABS_HAT0X) / 2;
                if (!joystick->hwdata->has_hat[hat_index]) {
#ifdef DEBUG_INPUT_EVENTS
                    SDL_Log("Joystick has digital hat: #%d", hat_index);
#endif
                    joystick->hwdata->hats_indices[hat_index] = joystick->nhats++;
                    joystick->hwdata->has_hat[hat_index] = true;
                    joystick->hwdata->hat_correct[hat_index].minimum[0] = -1;
                    joystick->hwdata->hat_correct[hat_index].maximum[0] = 1;
                    joystick->hwdata->hat_correct[hat_index].minimum[1] = -1;
                    joystick->hwdata->hat_correct[hat_index].maximum[1] = 1;
                }
            } else {
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick has absolute axis: 0x%.2x", code);
#endif
                joystick->hwdata->abs_map[code] = joystick->naxes;
                joystick->hwdata->has_abs[code] = true;
                ++joystick->naxes;
            }
        }
    }

    // Sensors are only available through the new unified event API
    if (fd_sensor >= 0 && (ioctl(fd_sensor, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0)) {
        if (test_bit(ABS_X, absbit) && test_bit(ABS_Y, absbit) && test_bit(ABS_Z, absbit)) {
            joystick->hwdata->has_accelerometer = true;
            for (i = 0; i < 3; ++i) {
                struct input_absinfo absinfo;
                if (ioctl(fd_sensor, EVIOCGABS(ABS_X + i), &absinfo) < 0) {
                    joystick->hwdata->has_accelerometer = false;
                    break; // do not report an accelerometer if we can't read all axes
                }
                joystick->hwdata->accelerometer_scale[i] = absinfo.resolution;
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick has accelerometer axis: 0x%.2x", ABS_X + i);
                SDL_Log("Values = { val:%d, min:%d, max:%d, fuzz:%d, flat:%d, res:%d }",
                        absinfo.value, absinfo.minimum, absinfo.maximum,
                        absinfo.fuzz, absinfo.flat, absinfo.resolution);
#endif // DEBUG_INPUT_EVENTS
            }
        }

        if (test_bit(ABS_RX, absbit) && test_bit(ABS_RY, absbit) && test_bit(ABS_RZ, absbit)) {
            joystick->hwdata->has_gyro = true;
            for (i = 0; i < 3; ++i) {
                struct input_absinfo absinfo;
                if (ioctl(fd_sensor, EVIOCGABS(ABS_RX + i), &absinfo) < 0) {
                    joystick->hwdata->has_gyro = false;
                    break; // do not report a gyro if we can't read all axes
                }
                joystick->hwdata->gyro_scale[i] = absinfo.resolution;
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick has gyro axis: 0x%.2x", ABS_RX + i);
                SDL_Log("Values = { val:%d, min:%d, max:%d, fuzz:%d, flat:%d, res:%d }",
                        absinfo.value, absinfo.minimum, absinfo.maximum,
                        absinfo.fuzz, absinfo.flat, absinfo.resolution);
#endif // DEBUG_INPUT_EVENTS
            }
        }
    }

    // Allocate data to keep track of these thingamajigs
    if (joystick->nballs > 0) {
        if (!allocate_balldata(joystick)) {
            joystick->nballs = 0;
        }
    }
    if (joystick->nhats > 0) {
        if (!allocate_hatdata(joystick)) {
            joystick->nhats = 0;
        }
    }

    if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0) {
        if (test_bit(FF_RUMBLE, ffbit)) {
            joystick->hwdata->ff_rumble = true;
        }
        if (test_bit(FF_SINE, ffbit)) {
            joystick->hwdata->ff_sine = true;
        }
    }
}

/* This is used to do the heavy lifting for LINUX_JoystickOpen and
   also LINUX_JoystickGetGamepadMapping, so we can query the hardware
   without adding an opened SDL_Joystick object to the system.
   This expects `joystick->hwdata` to be allocated and will not free it
   on error. Returns -1 on error, 0 on success. */
static bool PrepareJoystickHwdata(SDL_Joystick *joystick, const char *path, SDL_GUID guid, Uint16 vendor, const char *sensor_path)
{
    SDL_AssertJoysticksLocked();

    joystick->hwdata->guid = guid;
    joystick->hwdata->vendor = vendor;
    joystick->hwdata->effect.id = -1;
    SDL_memset(joystick->hwdata->key_map, 0xFF, sizeof(joystick->hwdata->key_map));
    SDL_memset(joystick->hwdata->abs_map, 0xFF, sizeof(joystick->hwdata->abs_map));

    int fd = -1, fd_sensor = -1;
    // Try read-write first, so we can do rumble
    fd = open(path, O_RDWR | O_CLOEXEC, 0);
    if (fd < 0) {
        // Try read-only again, at least we'll get events in this case
        fd = open(path, O_RDONLY | O_CLOEXEC, 0);
    }
    if (fd < 0) {
        return SDL_SetError("Unable to open %s", path);
    }
    // If opening sensor fail, continue with buttons and axes only
    if (sensor_path) {
        fd_sensor = open(sensor_path, O_RDONLY | O_CLOEXEC, 0);
    }

    joystick->hwdata->fd = fd;
    joystick->hwdata->fd_sensor = fd_sensor;
    joystick->hwdata->fname = SDL_strdup(path);
    if (!joystick->hwdata->fname) {
        close(fd);
        if (fd_sensor >= 0) {
            close(fd_sensor);
        }
        return false;
    }

    // Set the joystick to non-blocking read mode
    fcntl(fd, F_SETFL, O_NONBLOCK);
    if (fd_sensor >= 0) {
        fcntl(fd_sensor, F_SETFL, O_NONBLOCK);
    }

    // Get the number of buttons and axes on the joystick
    ConfigJoystick(joystick, fd, fd_sensor);
    return true;
}

static bool LINUX_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    const LINUX_DeviceData *data = (const LINUX_DeviceData *)device->driver_data;
    char *sensor_path;

    SDL_AssertJoysticksLocked();

    joystick->hwdata = (struct joystick_hwdata *)
        SDL_calloc(1, sizeof(*joystick->hwdata));
    if (!joystick->hwdata) {
        return false;
    }

    sensor_path = ClaimSensor(data->uniq);
    if (!PrepareJoystickHwdata(joystick, device->path, device->guid, data->vendor, sensor_path)) {
        ReleaseSensor(sensor_path);
        SDL_free(sensor_path);
        SDL_free(joystick->hwdata);
        joystick->hwdata = NULL;
        return false; // SDL_SetError will already have been called
    }
    joystick->hwdata->sensor_path = sensor_path;

    if (data->serial[0]) {
        joystick->serial = SDL_strdup(data->serial);
    }

    // mark joystick as fresh and ready
    joystick->hwdata->fresh = true;

    if (joystick->hwdata->has_gyro) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_GYRO, 0.0f);
    }
    if (joystick->hwdata->has_accelerometer) {
        SDL_PrivateJoystickAddSensor(joystick, SDL_SENSOR_ACCEL, 0.0f);
    }
    if (joystick->hwdata->fd_sensor >= 0) {
        // Don't keep fd_sensor opened while sensor is disabled
        close(joystick->hwdata->fd_sensor);
        joystick->hwdata->fd_sensor = -1;
    }

    if (joystick->hwdata->ff_rumble || joystick->hwdata->ff_sine) {
        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);
    }
    return true;
}

static bool LINUX_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    struct input_event event;

    SDL_AssertJoysticksLocked();

    if (joystick->hwdata->ff_rumble) {
        struct ff_effect *effect = &joystick->hwdata->effect;

        effect->type = FF_RUMBLE;
        effect->replay.length = SDL_MAX_RUMBLE_DURATION_MS;
        effect->u.rumble.strong_magnitude = low_frequency_rumble;
        effect->u.rumble.weak_magnitude = high_frequency_rumble;
    } else if (joystick->hwdata->ff_sine) {
        // Scale and average the two rumble strengths
        Sint16 magnitude = (Sint16)(((low_frequency_rumble / 2) + (high_frequency_rumble / 2)) / 2);
        struct ff_effect *effect = &joystick->hwdata->effect;

        effect->type = FF_PERIODIC;
        effect->replay.length = SDL_MAX_RUMBLE_DURATION_MS;
        effect->u.periodic.waveform = FF_SINE;
        effect->u.periodic.magnitude = magnitude;
    } else {
        return SDL_Unsupported();
    }

    if (ioctl(joystick->hwdata->fd, EVIOCSFF, &joystick->hwdata->effect) < 0) {
        // The kernel may have lost this effect, try to allocate a new one
        joystick->hwdata->effect.id = -1;
        if (ioctl(joystick->hwdata->fd, EVIOCSFF, &joystick->hwdata->effect) < 0) {
            return SDL_SetError("Couldn't update rumble effect: %s", strerror(errno));
        }
    }

    event.type = EV_FF;
    event.code = joystick->hwdata->effect.id;
    event.value = 1;
    if (write(joystick->hwdata->fd, &event, sizeof(event)) < 0) {
        return SDL_SetError("Couldn't start rumble effect: %s", strerror(errno));
    }
    return true;
}

static bool LINUX_JoystickSetSensorsEnabled(SDL_Joystick *joystick, bool enabled)
{
    SDL_AssertJoysticksLocked();

    if (!joystick->hwdata->has_accelerometer && !joystick->hwdata->has_gyro) {
        return SDL_Unsupported();
    }
    if (enabled == joystick->hwdata->report_sensor) {
        return true;
    }

    if (enabled) {
        if (!joystick->hwdata->sensor_path) {
            return SDL_SetError("Sensors unplugged.");
        }
        joystick->hwdata->fd_sensor = open(joystick->hwdata->sensor_path, O_RDONLY | O_CLOEXEC, 0);
        if (joystick->hwdata->fd_sensor < 0) {
            return SDL_SetError("Couldn't open sensor file %s.", joystick->hwdata->sensor_path);
        }
        fcntl(joystick->hwdata->fd_sensor, F_SETFL, O_NONBLOCK);
    } else {
        SDL_assert(joystick->hwdata->fd_sensor >= 0);
        close(joystick->hwdata->fd_sensor);
        joystick->hwdata->fd_sensor = -1;
    }

    joystick->hwdata->report_sensor = enabled;
    return true;
}

static void HandleHat(Uint64 timestamp, SDL_Joystick *stick, int hatidx, int axis, int value)
{
    int hatnum;
    struct hwdata_hat *the_hat;
    struct hat_axis_correct *correct;
    const Uint8 position_map[3][3] = {
        { SDL_HAT_LEFTUP, SDL_HAT_UP, SDL_HAT_RIGHTUP },
        { SDL_HAT_LEFT, SDL_HAT_CENTERED, SDL_HAT_RIGHT },
        { SDL_HAT_LEFTDOWN, SDL_HAT_DOWN, SDL_HAT_RIGHTDOWN }
    };

    SDL_AssertJoysticksLocked();

    hatnum = stick->hwdata->hats_indices[hatidx];
    the_hat = &stick->hwdata->hats[hatnum];
    correct = &stick->hwdata->hat_correct[hatidx];
    /* Hopefully we detected any analog axes and left them as is rather than trying
     * to use them as digital hats, but just in case, the deadzones here will
     * prevent the slightest of twitches on an analog axis from registering as a hat
     * movement. If the axes really are digital, this won't hurt since they should
     * only ever be sending min, 0, or max anyway. */
    if (value < 0) {
        if (value <= correct->minimum[axis]) {
            correct->minimum[axis] = value;
            value = 0;
        } else if (!correct->use_deadzones || value < correct->minimum[axis] / 3) {
            value = 0;
        } else {
            value = 1;
        }
    } else if (value > 0) {
        if (value >= correct->maximum[axis]) {
            correct->maximum[axis] = value;
            value = 2;
        } else if (!correct->use_deadzones || value > correct->maximum[axis] / 3) {
            value = 2;
        } else {
            value = 1;
        }
    } else { // value == 0
        value = 1;
    }
    if (value != the_hat->axis[axis]) {
        the_hat->axis[axis] = value;
        SDL_SendJoystickHat(timestamp, stick, hatnum,
                               position_map[the_hat->axis[1]][the_hat->axis[0]]);
    }
}

static void HandleBall(SDL_Joystick *stick, Uint8 ball, int axis, int value)
{
    SDL_AssertJoysticksLocked();

    stick->hwdata->balls[ball].axis[axis] += value;
}

static int AxisCorrect(SDL_Joystick *joystick, int which, int value)
{
    struct axis_correct *correct;

    SDL_AssertJoysticksLocked();

    correct = &joystick->hwdata->abs_correct[which];
    if (correct->minimum != correct->maximum) {
        if (correct->use_deadzones) {
            value *= 2;
            if (value > correct->coef[0]) {
                if (value < correct->coef[1]) {
                    return 0;
                }
                value -= correct->coef[1];
            } else {
                value -= correct->coef[0];
            }
            value *= correct->coef[2];
            value >>= 13;
        } else {
            value = (int)SDL_floorf((value - correct->minimum) * correct->scale + SDL_JOYSTICK_AXIS_MIN + 0.5f);
        }
    }

    // Clamp and return
    if (value < SDL_JOYSTICK_AXIS_MIN) {
        return SDL_JOYSTICK_AXIS_MIN;
    }
    if (value > SDL_JOYSTICK_AXIS_MAX) {
        return SDL_JOYSTICK_AXIS_MAX;
    }
    return value;
}

static void PollAllValues(Uint64 timestamp, SDL_Joystick *joystick)
{
    struct input_absinfo absinfo;
    unsigned long keyinfo[NBITS(KEY_MAX)];
    int i;

    SDL_AssertJoysticksLocked();

    // Poll all axis
    for (i = ABS_X; i < ABS_MAX; i++) {
        // We don't need to test for digital hats here, they won't have has_abs[] set
        if (joystick->hwdata->has_abs[i]) {
            if (ioctl(joystick->hwdata->fd, EVIOCGABS(i), &absinfo) >= 0) {
                absinfo.value = AxisCorrect(joystick, i, absinfo.value);

#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick : Re-read Axis %d (%d) val= %d",
                        joystick->hwdata->abs_map[i], i, absinfo.value);
#endif
                SDL_SendJoystickAxis(timestamp, joystick,
                                        joystick->hwdata->abs_map[i],
                                        absinfo.value);
            }
        }
    }

    // Poll all digital hats
    for (i = ABS_HAT0X; i <= ABS_HAT3Y; i++) {
        const int baseaxis = i - ABS_HAT0X;
        const int hatidx = baseaxis / 2;
        SDL_assert(hatidx < SDL_arraysize(joystick->hwdata->has_hat));
        // We don't need to test for analog axes here, they won't have has_hat[] set
        if (joystick->hwdata->has_hat[hatidx]) {
            if (ioctl(joystick->hwdata->fd, EVIOCGABS(i), &absinfo) >= 0) {
                const int hataxis = baseaxis % 2;
                HandleHat(timestamp, joystick, hatidx, hataxis, absinfo.value);
            }
        }
    }

    // Poll all buttons
    SDL_zeroa(keyinfo);
    if (ioctl(joystick->hwdata->fd, EVIOCGKEY(sizeof(keyinfo)), keyinfo) >= 0) {
        for (i = 0; i < KEY_MAX; i++) {
            if (joystick->hwdata->has_key[i]) {
                bool down = test_bit(i, keyinfo);
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick : Re-read Button %d (%d) val= %d",
                        joystick->hwdata->key_map[i], i, down);
#endif
                SDL_SendJoystickButton(timestamp, joystick,
                                          joystick->hwdata->key_map[i], down);
            }
        }
    }

    // Joyballs are relative input, so there's no poll state. Events only!
}

static void CorrectSensorData(struct joystick_hwdata *hwdata, float *values, float *data)
{
    if (hwdata->vendor == USB_VENDOR_NINTENDO) {
        // The Nintendo driver uses a different axis order than SDL
        data[0] = -values[1];
        data[1] = values[2];
        data[2] = -values[0];
    } else {
        data[0] = values[0];
        data[1] = values[1];
        data[2] = values[2];
    }
}

static void PollAllSensors(Uint64 timestamp, SDL_Joystick *joystick)
{
    struct input_absinfo absinfo;
    int i;

    SDL_AssertJoysticksLocked();

    SDL_assert(joystick->hwdata->fd_sensor >= 0);

    if (joystick->hwdata->has_gyro) {
        float values[3] = {0.0f, 0.0f, 0.0f};
        for (i = 0; i < 3; i++) {
            if (ioctl(joystick->hwdata->fd_sensor, EVIOCGABS(ABS_RX + i), &absinfo) >= 0) {
                values[i] = absinfo.value * (SDL_PI_F / 180.f) / joystick->hwdata->gyro_scale[i];
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick : Re-read Gyro (axis %d) val= %f", i, data[i]);
#endif
            }
        }
        float data[3];
        CorrectSensorData(joystick->hwdata, values, data);
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO, SDL_US_TO_NS(joystick->hwdata->sensor_tick), data, 3);
    }
    if (joystick->hwdata->has_accelerometer) {
        float values[3] = {0.0f, 0.0f, 0.0f};
        for (i = 0; i < 3; i++) {
            if (ioctl(joystick->hwdata->fd_sensor, EVIOCGABS(ABS_X + i), &absinfo) >= 0) {
                values[i] = absinfo.value * SDL_STANDARD_GRAVITY / joystick->hwdata->accelerometer_scale[i];
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Joystick : Re-read Accelerometer (axis %d) val= %f", i, data[i]);
#endif
            }
        }
        float data[3];
        CorrectSensorData(joystick->hwdata, values, data);
        SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL, SDL_US_TO_NS(joystick->hwdata->sensor_tick), data, 3);
    }
}

static void HandleInputEvents(SDL_Joystick *joystick)
{
    struct input_event events[32];
    int i, len, code, hat_index;

    SDL_AssertJoysticksLocked();

    if (joystick->hwdata->fresh) {
        Uint64 ticks = SDL_GetTicksNS();
        PollAllValues(ticks, joystick);
        if (joystick->hwdata->report_sensor) {
            PollAllSensors(ticks, joystick);
        }
        joystick->hwdata->fresh = false;
    }

    errno = 0;

    while ((len = read(joystick->hwdata->fd, events, sizeof(events))) > 0) {
        len /= sizeof(events[0]);
        for (i = 0; i < len; ++i) {
            struct input_event *event = &events[i];

            code = event->code;

            /* If the kernel sent a SYN_DROPPED, we are supposed to ignore the
               rest of the packet (the end of it signified by a SYN_REPORT) */
            if (joystick->hwdata->recovering_from_dropped &&
                ((event->type != EV_SYN) || (code != SYN_REPORT))) {
                continue;
            }

            switch (event->type) {
            case EV_KEY:
#ifdef DEBUG_INPUT_EVENTS
                SDL_Log("Key 0x%.2x %s", code, event->value ? "PRESSED" : "RELEASED");
#endif
                SDL_SendJoystickButton(SDL_EVDEV_GetEventTimestamp(event), joystick,
                                          joystick->hwdata->key_map[code],
                                          (event->value != 0));
                break;
            case EV_ABS:
                switch (code) {
                case ABS_HAT0X:
                case ABS_HAT0Y:
                case ABS_HAT1X:
                case ABS_HAT1Y:
                case ABS_HAT2X:
                case ABS_HAT2Y:
                case ABS_HAT3X:
                case ABS_HAT3Y:
                    hat_index = (code - ABS_HAT0X) / 2;
                    if (joystick->hwdata->has_hat[hat_index]) {
#ifdef DEBUG_INPUT_EVENTS
                        SDL_Log("Axis 0x%.2x = %d", code, event->value);
#endif
                        HandleHat(SDL_EVDEV_GetEventTimestamp(event), joystick, hat_index, code % 2, event->value);
                        break;
                    }
                    SDL_FALLTHROUGH;
                default:
#ifdef DEBUG_INPUT_EVENTS
                    SDL_Log("Axis 0x%.2x = %d", code, event->value);
#endif
                    event->value = AxisCorrect(joystick, code, event->value);
                    SDL_SendJoystickAxis(SDL_EVDEV_GetEventTimestamp(event), joystick,
                                            joystick->hwdata->abs_map[code],
                                            event->value);
                    break;
                }
                break;
            case EV_REL:
                switch (code) {
                case REL_X:
                case REL_Y:
                    code -= REL_X;
                    HandleBall(joystick, code / 2, code % 2, event->value);
                    break;
                default:
                    break;
                }
                break;
            case EV_SYN:
                switch (code) {
                case SYN_DROPPED:
#ifdef DEBUG_INPUT_EVENTS
                    SDL_Log("Event SYN_DROPPED detected");
#endif
                    joystick->hwdata->recovering_from_dropped = true;
                    break;
                case SYN_REPORT:
                    if (joystick->hwdata->recovering_from_dropped) {
                        joystick->hwdata->recovering_from_dropped = false;
                        PollAllValues(SDL_GetTicksNS(), joystick); // try to sync up to current state now
                    }
                    break;
                default:
                    break;
                }
                break;
            default:
                break;
            }
        }
    }

    if (errno == ENODEV) {
        // The device is gone, remove it now instead of waiting for the device thread to notice
        if (!joystick->hwdata->gone) {
            joystick->hwdata->gone = true;
            SDL_ReportJoystickRemoved(joystick->driver_instance_id);
            LINUX_ForgetJoystick(joystick->driver_instance_id);
        }
        errno = 0;
    }

    if (joystick->hwdata->report_sensor) {
        SDL_assert(joystick->hwdata->fd_sensor >= 0);

        while ((len = read(joystick->hwdata->fd_sensor, events, sizeof(events))) > 0) {
            len /= sizeof(events[0]);
            for (i = 0; i < len; ++i) {
                unsigned int j;
                struct input_event *event = &events[i];

                code = event->code;

                /* If the kernel sent a SYN_DROPPED, we are supposed to ignore the
                   rest of the packet (the end of it signified by a SYN_REPORT) */
                if (joystick->hwdata->recovering_from_dropped_sensor &&
                    ((event->type != EV_SYN) || (code != SYN_REPORT))) {
                    continue;
                }

                switch (event->type) {
                case EV_KEY:
                    SDL_assert(0);
                    break;
                case EV_ABS:
                    switch (code) {
                    case ABS_X:
                    case ABS_Y:
                    case ABS_Z:
                        j = code - ABS_X;
                        joystick->hwdata->accel_data[j] = event->value * SDL_STANDARD_GRAVITY
                                                        / joystick->hwdata->accelerometer_scale[j];
                        break;
                    case ABS_RX:
                    case ABS_RY:
                    case ABS_RZ:
                        j = code - ABS_RX;
                        joystick->hwdata->gyro_data[j] = event->value * (SDL_PI_F / 180.f)
                                                       / joystick->hwdata->gyro_scale[j];
                        break;
                    }
                    break;
                case EV_MSC:
                    if (code == MSC_TIMESTAMP) {
                        Sint32 tick = event->value;
                        Sint32 delta;
                        if (joystick->hwdata->last_tick <= tick) {
                            delta = (tick - joystick->hwdata->last_tick);
                        } else {
                            delta = (SDL_MAX_SINT32 - joystick->hwdata->last_tick + tick + 1);
                        }
                        joystick->hwdata->sensor_tick += delta;
                        joystick->hwdata->last_tick = tick;
                    }
                    break;
                case EV_SYN:
                    switch (code) {
                    case SYN_DROPPED:
    #ifdef DEBUG_INPUT_EVENTS
                        SDL_Log("Event SYN_DROPPED detected");
    #endif
                        joystick->hwdata->recovering_from_dropped_sensor = true;
                        break;
                    case SYN_REPORT:
                        if (joystick->hwdata->recovering_from_dropped_sensor) {
                            joystick->hwdata->recovering_from_dropped_sensor = false;
                            PollAllSensors(SDL_GetTicksNS(), joystick); // try to sync up to current state now
                        } else {
                            Uint64 timestamp = SDL_EVDEV_GetEventTimestamp(event);
                            float data[3];
                            CorrectSensorData(joystick->hwdata, joystick->hwdata->gyro_data, data);
                            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_GYRO,
                                                   SDL_US_TO_NS(joystick->hwdata->sensor_tick),
                                                   data, 3);
                            CorrectSensorData(joystick->hwdata, joystick->hwdata->accel_data, data);
                            SDL_SendJoystickSensor(timestamp, joystick, SDL_SENSOR_ACCEL,
                                                   SDL_US_TO_NS(joystick->hwdata->sensor_tick),
                                                   data, 3);
                        }
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
            }
        }
    }

    if (errno == ENODEV && joystick->hwdata->fd_sensor >= 0) {
        // The sensor device is gone, stop reading it
        close(joystick->hwdata->fd_sensor);
        joystick->hwdata->fd_sensor = -1;
        joystick->hwdata->report_sensor = false;
    }
}

static void HandleClassicEvents(SDL_Joystick *joystick)
{
    struct js_event events[32];
    int i, len, code, hat_index;
    Uint64 timestamp = SDL_GetTicksNS();

    SDL_AssertJoysticksLocked();

    joystick->hwdata->fresh = false;
    while ((len = read(joystick->hwdata->fd, events, sizeof(events))) > 0) {
        len /= sizeof(events[0]);
        for (i = 0; i < len; ++i) {
            switch (events[i].type) {
            case JS_EVENT_BUTTON:
                code = joystick->hwdata->key_pam[events[i].number];
                SDL_SendJoystickButton(timestamp, joystick,
                                       joystick->hwdata->key_map[code],
                                       (events[i].value != 0));
                break;
            case JS_EVENT_AXIS:
                code = joystick->hwdata->abs_pam[events[i].number];
                switch (code) {
                case ABS_HAT0X:
                case ABS_HAT0Y:
                case ABS_HAT1X:
                case ABS_HAT1Y:
                case ABS_HAT2X:
                case ABS_HAT2Y:
                case ABS_HAT3X:
                case ABS_HAT3Y:
                    hat_index = (code - ABS_HAT0X) / 2;
                    if (joystick->hwdata->has_hat[hat_index]) {
                        HandleHat(timestamp, joystick, hat_index, code % 2, events[i].value);
                        break;
                    }
                    SDL_FALLTHROUGH;
                default:
                    SDL_SendJoystickAxis(timestamp, joystick,
                                            joystick->hwdata->abs_map[code],
                                            events[i].value);
                    break;
                }
            }
        }
    }
}

static void LINUX_JoystickUpdate(SDL_Joystick *joystick)
{
    int i;

    SDL_AssertJoysticksLocked();

    if (joystick->hwdata->classic) {
        HandleClassicEvents(joystick);
    } else {
        HandleInputEvents(joystick);
    }

    // Deliver ball motion updates
    for (i = 0; i < joystick->nballs; ++i) {
        int xrel, yrel;

        xrel = joystick->hwdata->balls[i].axis[0];
        yrel = joystick->hwdata->balls[i].axis[1];
        if (xrel || yrel) {
            joystick->hwdata->balls[i].axis[0] = 0;
            joystick->hwdata->balls[i].axis[1] = 0;
            SDL_SendJoystickBall(0, joystick, (Uint8)i, xrel, yrel);
        }
    }
}

// Function to close a joystick after use
static void LINUX_JoystickClose(SDL_Joystick *joystick)
{
    SDL_AssertJoysticksLocked();

    if (joystick->hwdata) {
        if (joystick->hwdata->effect.id >= 0) {
            ioctl(joystick->hwdata->fd, EVIOCRMFF, joystick->hwdata->effect.id);
            joystick->hwdata->effect.id = -1;
        }
        if (joystick->hwdata->fd >= 0) {
            close(joystick->hwdata->fd);
        }
        if (joystick->hwdata->fd_sensor >= 0) {
            close(joystick->hwdata->fd_sensor);
        }
        ReleaseSensor(joystick->hwdata->sensor_path);
        SDL_free(joystick->hwdata->sensor_path);
        SDL_free(joystick->hwdata->key_pam);
        SDL_free(joystick->hwdata->abs_pam);
        SDL_free(joystick->hwdata->hats);
        SDL_free(joystick->hwdata->balls);
        SDL_free(joystick->hwdata->fname);
        SDL_free(joystick->hwdata);
    }
}

// Function to perform any system-specific joystick related cleanup
static void LINUX_JoystickQuit(void)
{
    LINUX_KnownNode *node, *next;
    LINUX_SensorNode *sensor, *next_sensor;
    LINUX_GoneJoystick *gone, *next_gone;

    SDL_AssertJoysticksLocked();

    if (inotify_fd >= 0) {
        close(inotify_fd);
        inotify_fd = -1;
    }

    for (node = LINUX_known_nodes; node; node = next) {
        next = node->next;
        FreeKnownNode(node);
    }
    LINUX_known_nodes = NULL;

    if (LINUX_sensor_lock) {
        SDL_LockMutex(LINUX_sensor_lock);
        for (sensor = LINUX_sensors; sensor; sensor = next_sensor) {
            next_sensor = sensor->next;
            FreeSensorNode(sensor);
        }
        LINUX_sensors = NULL;
        SDL_UnlockMutex(LINUX_sensor_lock);

        SDL_DestroyMutex(LINUX_sensor_lock);
        LINUX_sensor_lock = NULL;
    }

    for (gone = LINUX_TakeGoneJoysticks(); gone; gone = next_gone) {
        next_gone = gone->next;
        SDL_free(gone);
    }

    SDL_free(LINUX_hint_devices);
    LINUX_hint_devices = NULL;
    LINUX_first_pass = false;

#ifdef SDL_USE_LIBUDEV
    LINUX_QuitUdev();
#endif
    enumeration_method = ENUMERATION_UNSET;
}

/*
   This is based on the Linux Gamepad Specification
   available at: https://www.kernel.org/doc/html/v4.15/input/gamepad.html
   and the Android gamepad documentation,
   https://developer.android.com/develop/ui/views/touch-and-input/game-controllers/controller-input
 */
static bool LINUX_JoystickGetGamepadMapping(SDL_JoystickDevice *device, SDL_GamepadMapping *out)
{
    SDL_Joystick *joystick;
    LINUX_DeviceData *data = (LINUX_DeviceData *)device->driver_data;
    enum {
        MAPPED_TRIGGER_LEFT = 0x1,
        MAPPED_TRIGGER_RIGHT = 0x2,
        MAPPED_TRIGGER_BOTH = 0x3,

        MAPPED_DPAD_UP = 0x1,
        MAPPED_DPAD_DOWN = 0x2,
        MAPPED_DPAD_LEFT = 0x4,
        MAPPED_DPAD_RIGHT = 0x8,
        MAPPED_DPAD_ALL = 0xF,

        MAPPED_LEFT_PADDLE1 = 0x1,
        MAPPED_RIGHT_PADDLE1 = 0x2,
        MAPPED_LEFT_PADDLE2 = 0x4,
        MAPPED_RIGHT_PADDLE2 = 0x8,
        MAPPED_PADDLE_ALL = 0xF,
    };
    unsigned int mapped;
    bool result = false;

    SDL_AssertJoysticksLocked();

    if (data->checked_mapping) {
        if (data->has_mapping) {
            SDL_memcpy(out, &data->mapping, sizeof(*out));
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Prior mapping for %s", device->path);
#endif
            return true;
        } else {
            return false;
        }
    }

    /* We temporarily open the device to check how it's configured. Make
       a fake SDL_Joystick object to do so. */
    joystick = (SDL_Joystick *)SDL_calloc(1, sizeof(*joystick));
    if (!joystick) {
        return false;
    }
    SDL_memcpy(&joystick->guid, &device->guid, sizeof(device->guid));

    joystick->hwdata = (struct joystick_hwdata *)SDL_calloc(1, sizeof(*joystick->hwdata));
    if (!joystick->hwdata) {
        SDL_free(joystick);
        return false;
    }
    SDL_SetObjectValid(joystick, SDL_OBJECT_TYPE_JOYSTICK, true);

    data->checked_mapping = true;

    if (!PrepareJoystickHwdata(joystick, device->path, device->guid, data->vendor, NULL)) {
        goto done; // SDL_SetError will already have been called
    }

    // it is now safe to call LINUX_JoystickClose on this fake joystick.

    if (!joystick->hwdata->has_key[BTN_GAMEPAD]) {
        // Not a gamepad according to the specs.
        goto done;
    }

    // We have a gamepad, start filling out the mappings

#ifdef DEBUG_GAMEPAD_MAPPING
    SDL_Log("Mapping %s (VID/PID 0x%.4x/0x%.4x)", device->name, SDL_GetJoystickVendor(joystick), SDL_GetJoystickProduct(joystick));
#endif

    if (joystick->hwdata->has_key[BTN_A]) {
        out->a.kind = EMappingKind_Button;
        out->a.target = joystick->hwdata->key_map[BTN_A];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped A to button %d (BTN_A)", out->a.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_B]) {
        out->b.kind = EMappingKind_Button;
        out->b.target = joystick->hwdata->key_map[BTN_B];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped B to button %d (BTN_B)", out->b.target);
#endif
    }

    // Xbox controllers use BTN_X and BTN_Y, and PS4 controllers use BTN_WEST and BTN_NORTH
    if (SDL_GetJoystickVendor(joystick) == USB_VENDOR_SONY) {
        if (joystick->hwdata->has_key[BTN_WEST]) {
            out->x.kind = EMappingKind_Button;
            out->x.target = joystick->hwdata->key_map[BTN_WEST];
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Mapped X to button %d (BTN_WEST)", out->x.target);
#endif
        }

        if (joystick->hwdata->has_key[BTN_NORTH]) {
            out->y.kind = EMappingKind_Button;
            out->y.target = joystick->hwdata->key_map[BTN_NORTH];
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Mapped Y to button %d (BTN_NORTH)", out->y.target);
#endif
        }
    } else {
        if (joystick->hwdata->has_key[BTN_X]) {
            out->x.kind = EMappingKind_Button;
            out->x.target = joystick->hwdata->key_map[BTN_X];
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Mapped X to button %d (BTN_X)", out->x.target);
#endif
        }

        if (joystick->hwdata->has_key[BTN_Y]) {
            out->y.kind = EMappingKind_Button;
            out->y.target = joystick->hwdata->key_map[BTN_Y];
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Mapped Y to button %d (BTN_Y)", out->y.target);
#endif
        }
    }

    if (joystick->hwdata->has_key[BTN_SELECT]) {
        out->back.kind = EMappingKind_Button;
        out->back.target = joystick->hwdata->key_map[BTN_SELECT];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped BACK to button %d (BTN_SELECT)", out->back.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_START]) {
        out->start.kind = EMappingKind_Button;
        out->start.target = joystick->hwdata->key_map[BTN_START];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped START to button %d (BTN_START)", out->start.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_THUMBL]) {
        out->leftstick.kind = EMappingKind_Button;
        out->leftstick.target = joystick->hwdata->key_map[BTN_THUMBL];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTSTICK to button %d (BTN_THUMBL)", out->leftstick.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_THUMBR]) {
        out->rightstick.kind = EMappingKind_Button;
        out->rightstick.target = joystick->hwdata->key_map[BTN_THUMBR];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTSTICK to button %d (BTN_THUMBR)", out->rightstick.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_MODE]) {
        out->guide.kind = EMappingKind_Button;
        out->guide.target = joystick->hwdata->key_map[BTN_MODE];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped GUIDE to button %d (BTN_MODE)", out->guide.target);
#endif
    }

    /*
       According to the specs the D-Pad, the shoulder buttons and the triggers
       can be digital, or analog, or both at the same time.
     */

    // Prefer digital shoulder buttons, but settle for digital or analog hat.
    mapped = 0;

    if (joystick->hwdata->has_key[BTN_TL]) {
        out->leftshoulder.kind = EMappingKind_Button;
        out->leftshoulder.target = joystick->hwdata->key_map[BTN_TL];
        mapped |= 0x1;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTSHOULDER to button %d (BTN_TL)", out->leftshoulder.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_TR]) {
        out->rightshoulder.kind = EMappingKind_Button;
        out->rightshoulder.target = joystick->hwdata->key_map[BTN_TR];
        mapped |= 0x2;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTSHOULDER to button %d (BTN_TR)", out->rightshoulder.target);
#endif
    }

    if (mapped != 0x3 && joystick->hwdata->has_hat[1]) {
        int hat = joystick->hwdata->hats_indices[1] << 4;
        out->leftshoulder.kind = EMappingKind_Hat;
        out->rightshoulder.kind = EMappingKind_Hat;
        out->leftshoulder.target = hat | 0x4;
        out->rightshoulder.target = hat | 0x2;
        mapped |= 0x3;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFT+RIGHTSHOULDER to hat 1 (ABS_HAT1X, ABS_HAT1Y)");
#endif
    }

    if (!(mapped & 0x1) && joystick->hwdata->has_abs[ABS_HAT1Y]) {
        out->leftshoulder.kind = EMappingKind_Axis;
        out->leftshoulder.target = joystick->hwdata->abs_map[ABS_HAT1Y];
        mapped |= 0x1;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTSHOULDER to axis %d (ABS_HAT1Y)", out->leftshoulder.target);
#endif
    }

    if (!(mapped & 0x2) && joystick->hwdata->has_abs[ABS_HAT1X]) {
        out->rightshoulder.kind = EMappingKind_Axis;
        out->rightshoulder.target = joystick->hwdata->abs_map[ABS_HAT1X];
        mapped |= 0x2;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTSHOULDER to axis %d (ABS_HAT1X)", out->rightshoulder.target);
#endif
    }

    // Prefer analog triggers, but settle for digital hat or buttons.
    mapped = 0;

    /* Unfortunately there are several conventions for how analog triggers
     * are represented as absolute axes:
     *
     * - Linux Gamepad Specification:
     *   LT = ABS_HAT2Y, RT = ABS_HAT2X
     * - Android (and therefore many Bluetooth controllers):
     *   LT = ABS_BRAKE, RT = ABS_GAS
     * - De facto standard for older Xbox and Playstation controllers:
     *   LT = ABS_Z, RT = ABS_RZ
     *
     * We try each one in turn. */
    if (joystick->hwdata->has_abs[ABS_HAT2Y]) {
        // Linux Gamepad Specification
        out->lefttrigger.kind = EMappingKind_Axis;
        out->lefttrigger.target = joystick->hwdata->abs_map[ABS_HAT2Y];
        mapped |= MAPPED_TRIGGER_LEFT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTTRIGGER to axis %d (ABS_HAT2Y)", out->lefttrigger.target);
#endif
    } else if (joystick->hwdata->has_abs[ABS_BRAKE]) {
        // Android convention
        out->lefttrigger.kind = EMappingKind_Axis;
        out->lefttrigger.target = joystick->hwdata->abs_map[ABS_BRAKE];
        mapped |= MAPPED_TRIGGER_LEFT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTTRIGGER to axis %d (ABS_BRAKE)", out->lefttrigger.target);
#endif
    } else if (joystick->hwdata->has_abs[ABS_Z]) {
        // De facto standard for Xbox 360 and Playstation gamepads
        out->lefttrigger.kind = EMappingKind_Axis;
        out->lefttrigger.target = joystick->hwdata->abs_map[ABS_Z];
        mapped |= MAPPED_TRIGGER_LEFT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTTRIGGER to axis %d (ABS_Z)", out->lefttrigger.target);
#endif
    }

    if (joystick->hwdata->has_abs[ABS_HAT2X]) {
        // Linux Gamepad Specification
        out->righttrigger.kind = EMappingKind_Axis;
        out->righttrigger.target = joystick->hwdata->abs_map[ABS_HAT2X];
        mapped |= MAPPED_TRIGGER_RIGHT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTTRIGGER to axis %d (ABS_HAT2X)", out->righttrigger.target);
#endif
    } else if (joystick->hwdata->has_abs[ABS_GAS]) {
        // Android convention
        out->righttrigger.kind = EMappingKind_Axis;
        out->righttrigger.target = joystick->hwdata->abs_map[ABS_GAS];
        mapped |= MAPPED_TRIGGER_RIGHT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTTRIGGER to axis %d (ABS_GAS)", out->righttrigger.target);
#endif
    } else if (joystick->hwdata->has_abs[ABS_RZ]) {
        // De facto standard for Xbox 360 and Playstation gamepads
        out->righttrigger.kind = EMappingKind_Axis;
        out->righttrigger.target = joystick->hwdata->abs_map[ABS_RZ];
        mapped |= MAPPED_TRIGGER_RIGHT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTTRIGGER to axis %d (ABS_RZ)", out->righttrigger.target);
#endif
    }

    if (mapped != MAPPED_TRIGGER_BOTH && joystick->hwdata->has_hat[2]) {
        int hat = joystick->hwdata->hats_indices[2] << 4;
        out->lefttrigger.kind = EMappingKind_Hat;
        out->righttrigger.kind = EMappingKind_Hat;
        out->lefttrigger.target = hat | 0x4;
        out->righttrigger.target = hat | 0x2;
        mapped |= MAPPED_TRIGGER_BOTH;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFT+RIGHTTRIGGER to hat 2 (ABS_HAT2X, ABS_HAT2Y)");
#endif
    }

    if (!(mapped & MAPPED_TRIGGER_LEFT) && joystick->hwdata->has_key[BTN_TL2]) {
        out->lefttrigger.kind = EMappingKind_Button;
        out->lefttrigger.target = joystick->hwdata->key_map[BTN_TL2];
        mapped |= MAPPED_TRIGGER_LEFT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTTRIGGER to button %d (BTN_TL2)", out->lefttrigger.target);
#endif
    }

    if (!(mapped & MAPPED_TRIGGER_RIGHT) && joystick->hwdata->has_key[BTN_TR2]) {
        out->righttrigger.kind = EMappingKind_Button;
        out->righttrigger.target = joystick->hwdata->key_map[BTN_TR2];
        mapped |= MAPPED_TRIGGER_RIGHT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTTRIGGER to button %d (BTN_TR2)", out->righttrigger.target);
#endif
    }

    // Prefer digital D-Pad buttons, but settle for digital or analog hat.
    mapped = 0;

    if (joystick->hwdata->has_key[BTN_DPAD_UP]) {
        out->dpup.kind = EMappingKind_Button;
        out->dpup.target = joystick->hwdata->key_map[BTN_DPAD_UP];
        mapped |= MAPPED_DPAD_UP;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped DPUP to button %d (BTN_DPAD_UP)", out->dpup.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_DPAD_DOWN]) {
        out->dpdown.kind = EMappingKind_Button;
        out->dpdown.target = joystick->hwdata->key_map[BTN_DPAD_DOWN];
        mapped |= MAPPED_DPAD_DOWN;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped DPDOWN to button %d (BTN_DPAD_DOWN)", out->dpdown.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_DPAD_LEFT]) {
        out->dpleft.kind = EMappingKind_Button;
        out->dpleft.target = joystick->hwdata->key_map[BTN_DPAD_LEFT];
        mapped |= MAPPED_DPAD_LEFT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped DPLEFT to button %d (BTN_DPAD_LEFT)", out->dpleft.target);
#endif
    }

    if (joystick->hwdata->has_key[BTN_DPAD_RIGHT]) {
        out->dpright.kind = EMappingKind_Button;
        out->dpright.target = joystick->hwdata->key_map[BTN_DPAD_RIGHT];
        mapped |= MAPPED_DPAD_RIGHT;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped DPRIGHT to button %d (BTN_DPAD_RIGHT)", out->dpright.target);
#endif
    }

    if (mapped != MAPPED_DPAD_ALL) {
        if (joystick->hwdata->has_hat[0]) {
            int hat = joystick->hwdata->hats_indices[0] << 4;
            out->dpleft.kind = EMappingKind_Hat;
            out->dpright.kind = EMappingKind_Hat;
            out->dpup.kind = EMappingKind_Hat;
            out->dpdown.kind = EMappingKind_Hat;
            out->dpleft.target = hat | 0x8;
            out->dpright.target = hat | 0x2;
            out->dpup.target = hat | 0x1;
            out->dpdown.target = hat | 0x4;
            mapped |= MAPPED_DPAD_ALL;
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Mapped DPUP+DOWN+LEFT+RIGHT to hat 0 (ABS_HAT0X, ABS_HAT0Y)");
#endif
        } else if (joystick->hwdata->has_abs[ABS_HAT0X] && joystick->hwdata->has_abs[ABS_HAT0Y]) {
            out->dpleft.kind = EMappingKind_Axis;
            out->dpright.kind = EMappingKind_Axis;
            out->dpup.kind = EMappingKind_Axis;
            out->dpdown.kind = EMappingKind_Axis;
            out->dpleft.target = joystick->hwdata->abs_map[ABS_HAT0X];
            out->dpright.target = joystick->hwdata->abs_map[ABS_HAT0X];
            out->dpup.target = joystick->hwdata->abs_map[ABS_HAT0Y];
            out->dpdown.target = joystick->hwdata->abs_map[ABS_HAT0Y];
            mapped |= MAPPED_DPAD_ALL;
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Mapped DPUP+DOWN to axis %d (ABS_HAT0Y)", out->dpup.target);
            SDL_Log("Mapped DPLEFT+RIGHT to axis %d (ABS_HAT0X)", out->dpleft.target);
#endif
        } else if (SDL_strcmp(data->driver, "xpad") == 0) {
            // xpad will sometimes map the D-Pad as BTN_TRIGGER_HAPPY1 - BTN_TRIGGER_HAPPY4
            if (joystick->hwdata->has_key[BTN_TRIGGER_HAPPY1] &&
                joystick->hwdata->has_key[BTN_TRIGGER_HAPPY2] &&
                joystick->hwdata->has_key[BTN_TRIGGER_HAPPY3] &&
                joystick->hwdata->has_key[BTN_TRIGGER_HAPPY4]) {
                out->dpleft.kind = EMappingKind_Button;
                out->dpright.kind = EMappingKind_Button;
                out->dpup.kind = EMappingKind_Button;
                out->dpdown.kind = EMappingKind_Button;
                out->dpleft.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY1];
                out->dpright.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY2];
                out->dpup.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY3];
                out->dpdown.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY4];
#ifdef DEBUG_GAMEPAD_MAPPING
                SDL_Log("Mapped DPLEFT to button %d (BTN_TRIGGER_HAPPY1)", out->dpleft.target);
                SDL_Log("Mapped DPRIGHT to button %d (BTN_TRIGGER_HAPPY2)", out->dpright.target);
                SDL_Log("Mapped DPUP to button %d (BTN_TRIGGER_HAPPY3)", out->dpup.target);
                SDL_Log("Mapped DPDOWN to button %d (BTN_TRIGGER_HAPPY4)", out->dpdown.target);
#endif
            }
        }
    }

    if (joystick->hwdata->has_abs[ABS_X] && joystick->hwdata->has_abs[ABS_Y]) {
        out->leftx.kind = EMappingKind_Axis;
        out->lefty.kind = EMappingKind_Axis;
        out->leftx.target = joystick->hwdata->abs_map[ABS_X];
        out->lefty.target = joystick->hwdata->abs_map[ABS_Y];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFTX to axis %d (ABS_X)", out->leftx.target);
        SDL_Log("Mapped LEFTY to axis %d (ABS_Y)", out->lefty.target);
#endif
    }

    /* The Linux Gamepad Specification uses the RX and RY axes,
     * originally intended to represent X and Y rotation, as a second
     * joystick. This is common for USB gamepads, and also many Bluetooth
     * gamepads, particularly older ones.
     *
     * The Android mapping convention used by many Bluetooth controllers
     * instead uses the Z axis as a secondary X axis, and the RZ axis as
     * a secondary Y axis. */
    if (joystick->hwdata->has_abs[ABS_RX] && joystick->hwdata->has_abs[ABS_RY]) {
        // Linux Gamepad Specification, Xbox 360, Playstation etc.
        out->rightx.kind = EMappingKind_Axis;
        out->righty.kind = EMappingKind_Axis;
        out->rightx.target = joystick->hwdata->abs_map[ABS_RX];
        out->righty.target = joystick->hwdata->abs_map[ABS_RY];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTX to axis %d (ABS_RX)", out->rightx.target);
        SDL_Log("Mapped RIGHTY to axis %d (ABS_RY)", out->righty.target);
#endif
    } else if (joystick->hwdata->has_abs[ABS_Z] && joystick->hwdata->has_abs[ABS_RZ]) {
        // Android convention
        out->rightx.kind = EMappingKind_Axis;
        out->righty.kind = EMappingKind_Axis;
        out->rightx.target = joystick->hwdata->abs_map[ABS_Z];
        out->righty.target = joystick->hwdata->abs_map[ABS_RZ];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHTX to axis %d (ABS_Z)", out->rightx.target);
        SDL_Log("Mapped RIGHTY to axis %d (ABS_RZ)", out->righty.target);
#endif
    }

    mapped = 0;

    if (joystick->hwdata->has_key[BTN_GRIPR]) {
        out->right_paddle1.kind = EMappingKind_Button;
        out->right_paddle1.target = joystick->hwdata->key_map[BTN_GRIPR];
        mapped |= MAPPED_RIGHT_PADDLE1;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHT_PADDLE1 to button %d (BTN_GRIPR)", out->right_paddle1.target);
#endif
    }
    if (joystick->hwdata->has_key[BTN_GRIPL]) {
        out->left_paddle1.kind = EMappingKind_Button;
        out->left_paddle1.target = joystick->hwdata->key_map[BTN_GRIPL];
        mapped |= MAPPED_LEFT_PADDLE1;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFT_PADDLE1 to button %d (BTN_GRIPL)", out->left_paddle1.target);
#endif
    }
    if (joystick->hwdata->has_key[BTN_GRIPR2]) {
        out->right_paddle2.kind = EMappingKind_Button;
        out->right_paddle2.target = joystick->hwdata->key_map[BTN_GRIPR2];
        mapped |= MAPPED_RIGHT_PADDLE2;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped RIGHT_PADDLE2 to button %d (BTN_GRIPR)", out->right_paddle2.target);
#endif
    }
    if (joystick->hwdata->has_key[BTN_GRIPL2]) {
        out->left_paddle2.kind = EMappingKind_Button;
        out->left_paddle2.target = joystick->hwdata->key_map[BTN_GRIPL2];
        mapped |= MAPPED_LEFT_PADDLE2;
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped LEFT_PADDLE2 to button %d (BTN_GRIPL2)", out->left_paddle2.target);
#endif
    }

    if (mapped != MAPPED_PADDLE_ALL && SDL_GetJoystickVendor(joystick) == USB_VENDOR_MICROSOFT) {
        // The Xbox Elite controllers have the paddles as BTN_TRIGGER_HAPPY5 - BTN_TRIGGER_HAPPY8
        // in older drivers
        if (joystick->hwdata->has_key[BTN_TRIGGER_HAPPY5] &&
            joystick->hwdata->has_key[BTN_TRIGGER_HAPPY6] &&
            joystick->hwdata->has_key[BTN_TRIGGER_HAPPY7] &&
            joystick->hwdata->has_key[BTN_TRIGGER_HAPPY8]) {
            out->right_paddle1.kind = EMappingKind_Button;
            out->right_paddle1.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY5];
            out->left_paddle1.kind = EMappingKind_Button;
            out->left_paddle1.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY7];
            out->right_paddle2.kind = EMappingKind_Button;
            out->right_paddle2.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY6];
            out->left_paddle2.kind = EMappingKind_Button;
            out->left_paddle2.target = joystick->hwdata->key_map[BTN_TRIGGER_HAPPY8];
            mapped = MAPPED_PADDLE_ALL;
#ifdef DEBUG_GAMEPAD_MAPPING
            SDL_Log("Mapped RIGHT_PADDLE1 to button %d (BTN_TRIGGER_HAPPY5)", out->right_paddle1.target);
            SDL_Log("Mapped LEFT_PADDLE1 to button %d (BTN_TRIGGER_HAPPY7)", out->left_paddle1.target);
            SDL_Log("Mapped RIGHT_PADDLE2 to button %d (BTN_TRIGGER_HAPPY6)", out->right_paddle2.target);
            SDL_Log("Mapped LEFT_PADDLE2 to button %d (BTN_TRIGGER_HAPPY8)", out->left_paddle2.target);
#endif
        }
    }

    // Xbox Series controllers have the Share button as KEY_RECORD
    if (joystick->hwdata->has_key[KEY_RECORD]) {
        out->misc1.kind = EMappingKind_Button;
        out->misc1.target = joystick->hwdata->key_map[KEY_RECORD];
#ifdef DEBUG_GAMEPAD_MAPPING
        SDL_Log("Mapped MISC1 to button %d (KEY_RECORD)", out->misc1.target);
#endif
    }

    // Cache the mapping for later
    SDL_memcpy(&data->mapping, out, sizeof(*out));
    data->has_mapping = true;
#ifdef DEBUG_GAMEPAD_MAPPING
    SDL_Log("Generated mapping for %s", device->path);
#endif
    result = true;

done:
    LINUX_JoystickClose(joystick);
    SDL_SetObjectValid(joystick, SDL_OBJECT_TYPE_JOYSTICK, false);
    SDL_free(joystick);

    return result;
}

SDL_JoystickDriver SDL_LINUX_JoystickDriver = {
    .Init = LINUX_JoystickInit,
    .Discover = LINUX_JoystickDiscover,
    .GetDiscoveryFD = LINUX_JoystickGetDiscoveryFD,
    .GetGamepadMapping = LINUX_JoystickGetGamepadMapping,
    .Open = LINUX_JoystickOpen,
    .Rumble = LINUX_JoystickRumble,
    .SetSensorsEnabled = LINUX_JoystickSetSensorsEnabled,
    .Update = LINUX_JoystickUpdate,
    .Close = LINUX_JoystickClose,
    .Quit = LINUX_JoystickQuit,
};

#endif // SDL_JOYSTICK_LINUX
