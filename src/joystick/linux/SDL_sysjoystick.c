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

#ifdef SDL_JOYSTICK_LINUX

#ifndef SDL_INPUT_LINUXEV
#error SDL now requires a Linux 2.4+ kernel with /dev/input/event support.
#endif

/* This is the Linux implementation of the SDL joystick API */

#include <sys/stat.h>
#include <errno.h>              /* errno, strerror */
#include <fcntl.h>
#include <limits.h>             /* For the definition of PATH_MAX */
#include <sys/ioctl.h>
#include <unistd.h>
#include <dirent.h>
#include <linux/joystick.h>

#include "SDL_assert.h"
#include "SDL_joystick.h"
#include "SDL_endian.h"
#include "SDL_timer.h"
#include "../../events/SDL_events_c.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "../steam/SDL_steamcontroller.h"
#include "SDL_sysjoystick_c.h"
#include "../hidapi/SDL_hidapijoystick_c.h"

/* This isn't defined in older Linux kernel headers */
#ifndef SYN_DROPPED
#define SYN_DROPPED 3
#endif

#include "../../core/linux/SDL_udev.h"

static int MaybeAddDevice(const char *path);
#if SDL_USE_LIBUDEV
static int MaybeRemoveDevice(const char *path);
#endif /* SDL_USE_LIBUDEV */

/* A linked list of available joysticks */
typedef struct SDL_joylist_item
{
    int device_instance;
    char *path;   /* "/dev/input/event2" or whatever */
    char *name;   /* "SideWinder 3D Pro" or whatever */
    SDL_JoystickGUID guid;
    dev_t devnum;
    struct joystick_hwdata *hwdata;
    struct SDL_joylist_item *next;

    /* Steam Controller support */
    SDL_bool m_bSteamController;
} SDL_joylist_item;

static SDL_joylist_item *SDL_joylist = NULL;
static SDL_joylist_item *SDL_joylist_tail = NULL;
static int numjoysticks = 0;

#if !SDL_USE_LIBUDEV
static Uint32 last_joy_detect_time;
static time_t last_input_dir_mtime;
#endif

#define test_bit(nr, addr) \
    (((1UL << ((nr) % (sizeof(long) * 8))) & ((addr)[(nr) / (sizeof(long) * 8)])) != 0)
#define NBITS(x) ((((x)-1)/(sizeof(long) * 8))+1)

static int
PrefixMatch(const char *a, const char *b)
{
    int matchlen = 0;
    while (*a && *b) {
        if (*a++ == *b++) {
            ++matchlen;
        } else {
            break;
        }
    }
    return matchlen;
}

static void
FixupDeviceInfoForMapping(int fd, struct input_id *inpid)
{
    if (inpid->vendor == 0x045e && inpid->product == 0x0b05 && inpid->version == 0x0903) {
        /* This is a Microsoft Xbox One Elite Series 2 controller */
        unsigned long keybit[NBITS(KEY_MAX)] = { 0 };

        /* The first version of the firmware duplicated all the inputs */
        if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) &&
            test_bit(0x2c0, keybit)) {
            /* Change the version to 0x0902, so we can map it differently */
            inpid->version = 0x0902;
        }
    }
}


static int
IsJoystick(int fd, char *namebuf, const size_t namebuflen, SDL_JoystickGUID *guid)
{
    struct input_id inpid;
    Uint16 *guid16 = (Uint16 *)guid->data;
    const char *name;
    const char *spot;

#if !SDL_USE_LIBUDEV
    /* When udev is enabled we only get joystick devices here, so there's no need to test them */
    unsigned long evbit[NBITS(EV_MAX)] = { 0 };
    unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
    unsigned long absbit[NBITS(ABS_MAX)] = { 0 };

    if ((ioctl(fd, EVIOCGBIT(0, sizeof(evbit)), evbit) < 0) ||
        (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) < 0) ||
        (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) < 0)) {
        return (0);
    }

    if (!(test_bit(EV_KEY, evbit) && test_bit(EV_ABS, evbit) &&
          test_bit(ABS_X, absbit) && test_bit(ABS_Y, absbit))) {
        return 0;
    }
#endif

    if (ioctl(fd, EVIOCGID, &inpid) < 0) {
        return 0;
    }

    name = SDL_GetCustomJoystickName(inpid.vendor, inpid.product);
    if (name) {
        SDL_strlcpy(namebuf, name, namebuflen);
    } else {
        if (ioctl(fd, EVIOCGNAME(namebuflen), namebuf) < 0) {
            return 0;
        }

        /* Remove duplicate manufacturer in the name */
        for (spot = namebuf + 1; *spot; ++spot) {
            int matchlen = PrefixMatch(namebuf, spot);
            if (matchlen > 0 && spot[matchlen - 1] == ' ') {
                SDL_memmove(namebuf, spot, SDL_strlen(spot)+1);
                break;
            }
        }
    }

#ifdef SDL_JOYSTICK_HIDAPI
    if (HIDAPI_IsDevicePresent(inpid.vendor, inpid.product, inpid.version, namebuf)) {
        /* The HIDAPI driver is taking care of this device */
        return 0;
    }
#endif

    FixupDeviceInfoForMapping(fd, &inpid);

#ifdef DEBUG_JOYSTICK
    printf("Joystick: %s, bustype = %d, vendor = 0x%.4x, product = 0x%.4x, version = %d\n", namebuf, inpid.bustype, inpid.vendor, inpid.product, inpid.version);
#endif

    SDL_memset(guid->data, 0, sizeof(guid->data));

    /* We only need 16 bits for each of these; space them out to fill 128. */
    /* Byteswap so devices get same GUID on little/big endian platforms. */
    *guid16++ = SDL_SwapLE16(inpid.bustype);
    *guid16++ = 0;

    if (inpid.vendor && inpid.product) {
        *guid16++ = SDL_SwapLE16(inpid.vendor);
        *guid16++ = 0;
        *guid16++ = SDL_SwapLE16(inpid.product);
        *guid16++ = 0;
        *guid16++ = SDL_SwapLE16(inpid.version);
        *guid16++ = 0;
    } else {
        SDL_strlcpy((char*)guid16, namebuf, sizeof(guid->data) - 4);
    }

    if (SDL_ShouldIgnoreJoystick(namebuf, *guid)) {
        return 0;
    }
    return 1;
}

#if SDL_USE_LIBUDEV
static void joystick_udev_callback(SDL_UDEV_deviceevent udev_type, int udev_class, const char *devpath)
{
    if (devpath == NULL) {
        return;
    }

    switch (udev_type) {
        case SDL_UDEV_DEVICEADDED:
            if (!(udev_class & SDL_UDEV_DEVICE_JOYSTICK)) {
                return;
            }
            MaybeAddDevice(devpath);
            break;
            
        case SDL_UDEV_DEVICEREMOVED:
            MaybeRemoveDevice(devpath);
            break;
            
        default:
            break;
    }
    
}
#endif /* SDL_USE_LIBUDEV */

static int
MaybeAddDevice(const char *path)
{
    struct stat sb;
    int fd = -1;
    int isstick = 0;
    char namebuf[128];
    SDL_JoystickGUID guid;
    SDL_joylist_item *item;

    if (path == NULL) {
        return -1;
    }

    if (stat(path, &sb) == -1) {
        return -1;
    }

    /* Check to make sure it's not already in list. */
    for (item = SDL_joylist; item != NULL; item = item->next) {
        if (sb.st_rdev == item->devnum) {
            return -1;  /* already have this one */
        }
    }

    fd = open(path, O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }

#ifdef DEBUG_INPUT_EVENTS
    printf("Checking %s\n", path);
#endif

    isstick = IsJoystick(fd, namebuf, sizeof (namebuf), &guid);
    close(fd);
    if (!isstick) {
        return -1;
    }

    item = (SDL_joylist_item *) SDL_malloc(sizeof (SDL_joylist_item));
    if (item == NULL) {
        return -1;
    }

    SDL_zerop(item);
    item->devnum = sb.st_rdev;
    item->path = SDL_strdup(path);
    item->name = SDL_strdup(namebuf);
    item->guid = guid;

    if ((item->path == NULL) || (item->name == NULL)) {
         SDL_free(item->path);
         SDL_free(item->name);
         SDL_free(item);
         return -1;
    }

    item->device_instance = SDL_GetNextJoystickInstanceID();
    if (SDL_joylist_tail == NULL) {
        SDL_joylist = SDL_joylist_tail = item;
    } else {
        SDL_joylist_tail->next = item;
        SDL_joylist_tail = item;
    }

    /* Need to increment the joystick count before we post the event */
    ++numjoysticks;

    SDL_PrivateJoystickAdded(item->device_instance);

    return numjoysticks;
}

#if SDL_USE_LIBUDEV
static int
MaybeRemoveDevice(const char *path)
{
    SDL_joylist_item *item;
    SDL_joylist_item *prev = NULL;

    if (path == NULL) {
        return -1;
    }

    for (item = SDL_joylist; item != NULL; item = item->next) {
        /* found it, remove it. */
        if (SDL_strcmp(path, item->path) == 0) {
            const int retval = item->device_instance;
            if (item->hwdata) {
                item->hwdata->item = NULL;
            }
            if (prev != NULL) {
                prev->next = item->next;
            } else {
                SDL_assert(SDL_joylist == item);
                SDL_joylist = item->next;
            }
            if (item == SDL_joylist_tail) {
                SDL_joylist_tail = prev;
            }

            /* Need to decrement the joystick count before we post the event */
            --numjoysticks;

            SDL_PrivateJoystickRemoved(item->device_instance);

            SDL_free(item->path);
            SDL_free(item->name);
            SDL_free(item);
            return retval;
        }
        prev = item;
    }

    return -1;
}
#endif

static void
HandlePendingRemovals(void)
{
    SDL_joylist_item *prev = NULL;
    SDL_joylist_item *item = SDL_joylist;

    while (item != NULL) {
        if (item->hwdata && item->hwdata->gone) {
            item->hwdata->item = NULL;

            if (prev != NULL) {
                prev->next = item->next;
            } else {
                SDL_assert(SDL_joylist == item);
                SDL_joylist = item->next;
            }
            if (item == SDL_joylist_tail) {
                SDL_joylist_tail = prev;
            }

            /* Need to decrement the joystick count before we post the event */
            --numjoysticks;

            SDL_PrivateJoystickRemoved(item->device_instance);

            SDL_free(item->path);
            SDL_free(item->name);
            SDL_free(item);

            if (prev != NULL) {
                item = prev->next;
            } else {
                item = SDL_joylist;
            }
        } else {
            prev = item;
            item = item->next;
        }
    }
}

static SDL_bool SteamControllerConnectedCallback(const char *name, SDL_JoystickGUID guid, int *device_instance)
{
    SDL_joylist_item *item;

    item = (SDL_joylist_item *) SDL_calloc(1, sizeof (SDL_joylist_item));
    if (item == NULL) {
        return SDL_FALSE;
    }

    item->path = SDL_strdup("");
    item->name = SDL_strdup(name);
    item->guid = guid;
    item->m_bSteamController = SDL_TRUE;

    if ((item->path == NULL) || (item->name == NULL)) {
         SDL_free(item->path);
         SDL_free(item->name);
         SDL_free(item);
         return SDL_FALSE;
    }

    *device_instance = item->device_instance = SDL_GetNextJoystickInstanceID();
    if (SDL_joylist_tail == NULL) {
        SDL_joylist = SDL_joylist_tail = item;
    } else {
        SDL_joylist_tail->next = item;
        SDL_joylist_tail = item;
    }

    /* Need to increment the joystick count before we post the event */
    ++numjoysticks;

    SDL_PrivateJoystickAdded(item->device_instance);

    return SDL_TRUE;
}

static void SteamControllerDisconnectedCallback(int device_instance)
{
    SDL_joylist_item *item;
    SDL_joylist_item *prev = NULL;

    for (item = SDL_joylist; item != NULL; item = item->next) {
        /* found it, remove it. */
        if (item->device_instance == device_instance) {
            if (item->hwdata) {
                item->hwdata->item = NULL;
            }
            if (prev != NULL) {
                prev->next = item->next;
            } else {
                SDL_assert(SDL_joylist == item);
                SDL_joylist = item->next;
            }
            if (item == SDL_joylist_tail) {
                SDL_joylist_tail = prev;
            }

            /* Need to decrement the joystick count before we post the event */
            --numjoysticks;

            SDL_PrivateJoystickRemoved(item->device_instance);

            SDL_free(item->name);
            SDL_free(item);
            return;
        }
        prev = item;
    }
}

static void
LINUX_JoystickDetect(void)
{
#if SDL_USE_LIBUDEV
    SDL_UDEV_Poll();
#else
    const Uint32 SDL_JOY_DETECT_INTERVAL_MS = 3000;  /* Update every 3 seconds */
    Uint32 now = SDL_GetTicks();

    if (!last_joy_detect_time || SDL_TICKS_PASSED(now, last_joy_detect_time + SDL_JOY_DETECT_INTERVAL_MS)) {
        struct stat sb;

        /* Opening input devices can generate synchronous device I/O, so avoid it if we can */
        if (stat("/dev/input", &sb) == 0 && sb.st_mtime != last_input_dir_mtime) {
            DIR *folder;
            struct dirent *dent;

            folder = opendir("/dev/input");
            if (folder) {
                while ((dent = readdir(folder))) {
                    int len = SDL_strlen(dent->d_name);
                    if (len > 5 && SDL_strncmp(dent->d_name, "event", 5) == 0) {
                        char path[PATH_MAX];
                        SDL_snprintf(path, SDL_arraysize(path), "/dev/input/%s", dent->d_name);
                        MaybeAddDevice(path);
                    }
                }

                closedir(folder);
            }

            last_input_dir_mtime = sb.st_mtime;
        }

        last_joy_detect_time = now;
    }
#endif

    HandlePendingRemovals();

    SDL_UpdateSteamControllers();
}

static int
LINUX_JoystickInit(void)
{
    /* First see if the user specified one or more joysticks to use */
    if (SDL_getenv("SDL_JOYSTICK_DEVICE") != NULL) {
        char *envcopy, *envpath, *delim;
        envcopy = SDL_strdup(SDL_getenv("SDL_JOYSTICK_DEVICE"));
        envpath = envcopy;
        while (envpath != NULL) {
            delim = SDL_strchr(envpath, ':');
            if (delim != NULL) {
                *delim++ = '\0';
            }
            MaybeAddDevice(envpath);
            envpath = delim;
        }
        SDL_free(envcopy);
    }

    SDL_InitSteamControllers(SteamControllerConnectedCallback,
                             SteamControllerDisconnectedCallback);

#if SDL_USE_LIBUDEV
    if (SDL_UDEV_Init() < 0) {
        return SDL_SetError("Could not initialize UDEV");
    }

    /* Set up the udev callback */
    if (SDL_UDEV_AddCallback(joystick_udev_callback) < 0) {
        SDL_UDEV_Quit();
        return SDL_SetError("Could not set up joystick <-> udev callback");
    }

    /* Force a scan to build the initial device list */
    SDL_UDEV_Scan();
#else
    /* Force immediate joystick detection */
    last_joy_detect_time = 0;
    last_input_dir_mtime = 0;

    /* Report all devices currently present */
    LINUX_JoystickDetect();
#endif

    return 0;
}

static int
LINUX_JoystickGetCount(void)
{
    return numjoysticks;
}

static SDL_joylist_item *
JoystickByDevIndex(int device_index)
{
    SDL_joylist_item *item = SDL_joylist;

    if ((device_index < 0) || (device_index >= numjoysticks)) {
        return NULL;
    }

    while (device_index > 0) {
        SDL_assert(item != NULL);
        device_index--;
        item = item->next;
    }

    return item;
}

/* Function to get the device-dependent name of a joystick */
static const char *
LINUX_JoystickGetDeviceName(int device_index)
{
    return JoystickByDevIndex(device_index)->name;
}

static int
LINUX_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void
LINUX_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_JoystickGUID
LINUX_JoystickGetDeviceGUID( int device_index )
{
    return JoystickByDevIndex(device_index)->guid;
}

/* Function to perform the mapping from device index to the instance id for this index */
static SDL_JoystickID
LINUX_JoystickGetDeviceInstanceID(int device_index)
{
    return JoystickByDevIndex(device_index)->device_instance;
}

static int
allocate_hatdata(SDL_Joystick * joystick)
{
    int i;

    joystick->hwdata->hats =
        (struct hwdata_hat *) SDL_malloc(joystick->nhats *
                                         sizeof(struct hwdata_hat));
    if (joystick->hwdata->hats == NULL) {
        return (-1);
    }
    for (i = 0; i < joystick->nhats; ++i) {
        joystick->hwdata->hats[i].axis[0] = 1;
        joystick->hwdata->hats[i].axis[1] = 1;
    }
    return (0);
}

static int
allocate_balldata(SDL_Joystick * joystick)
{
    int i;

    joystick->hwdata->balls =
        (struct hwdata_ball *) SDL_malloc(joystick->nballs *
                                          sizeof(struct hwdata_ball));
    if (joystick->hwdata->balls == NULL) {
        return (-1);
    }
    for (i = 0; i < joystick->nballs; ++i) {
        joystick->hwdata->balls[i].axis[0] = 0;
        joystick->hwdata->balls[i].axis[1] = 0;
    }
    return (0);
}

static void
ConfigJoystick(SDL_Joystick * joystick, int fd)
{
    int i, t;
    unsigned long keybit[NBITS(KEY_MAX)] = { 0 };
    unsigned long absbit[NBITS(ABS_MAX)] = { 0 };
    unsigned long relbit[NBITS(REL_MAX)] = { 0 };
    unsigned long ffbit[NBITS(FF_MAX)] = { 0 };

    /* See if this device uses the new unified event API */
    if ((ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keybit)), keybit) >= 0) &&
        (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absbit)), absbit) >= 0) &&
        (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(relbit)), relbit) >= 0)) {

        /* Get the number of buttons, axes, and other thingamajigs */
        for (i = BTN_JOYSTICK; i < KEY_MAX; ++i) {
            if (test_bit(i, keybit)) {
#ifdef DEBUG_INPUT_EVENTS
                printf("Joystick has button: 0x%x\n", i);
#endif
                joystick->hwdata->key_map[i] = joystick->nbuttons;
                ++joystick->nbuttons;
            }
        }
        for (i = 0; i < BTN_JOYSTICK; ++i) {
            if (test_bit(i, keybit)) {
#ifdef DEBUG_INPUT_EVENTS
                printf("Joystick has button: 0x%x\n", i);
#endif
                joystick->hwdata->key_map[i] = joystick->nbuttons;
                ++joystick->nbuttons;
            }
        }
        for (i = 0; i < ABS_MAX; ++i) {
            /* Skip hats */
            if (i == ABS_HAT0X) {
                i = ABS_HAT3Y;
                continue;
            }
            if (test_bit(i, absbit)) {
                struct input_absinfo absinfo;

                if (ioctl(fd, EVIOCGABS(i), &absinfo) < 0) {
                    continue;
                }
#ifdef DEBUG_INPUT_EVENTS
                printf("Joystick has absolute axis: 0x%.2x\n", i);
                printf("Values = { %d, %d, %d, %d, %d }\n",
                       absinfo.value, absinfo.minimum, absinfo.maximum,
                       absinfo.fuzz, absinfo.flat);
#endif /* DEBUG_INPUT_EVENTS */
                joystick->hwdata->abs_map[i] = joystick->naxes;
                if (absinfo.minimum == absinfo.maximum) {
                    joystick->hwdata->abs_correct[i].used = 0;
                } else {
                    joystick->hwdata->abs_correct[i].used = 1;
                    joystick->hwdata->abs_correct[i].coef[0] =
                        (absinfo.maximum + absinfo.minimum) - 2 * absinfo.flat;
                    joystick->hwdata->abs_correct[i].coef[1] =
                        (absinfo.maximum + absinfo.minimum) + 2 * absinfo.flat;
                    t = ((absinfo.maximum - absinfo.minimum) - 4 * absinfo.flat);
                    if (t != 0) {
                        joystick->hwdata->abs_correct[i].coef[2] =
                            (1 << 28) / t;
                    } else {
                        joystick->hwdata->abs_correct[i].coef[2] = 0;
                    }
                }
                ++joystick->naxes;
            }
        }
        for (i = ABS_HAT0X; i <= ABS_HAT3Y; i += 2) {
            if (test_bit(i, absbit) || test_bit(i + 1, absbit)) {
                struct input_absinfo absinfo;
                int hat_index = (i - ABS_HAT0X) / 2;

                if (ioctl(fd, EVIOCGABS(i), &absinfo) < 0) {
                    continue;
                }
#ifdef DEBUG_INPUT_EVENTS
                printf("Joystick has hat %d\n", hat_index);
                printf("Values = { %d, %d, %d, %d, %d }\n",
                       absinfo.value, absinfo.minimum, absinfo.maximum,
                       absinfo.fuzz, absinfo.flat);
#endif /* DEBUG_INPUT_EVENTS */
                joystick->hwdata->hats_indices[hat_index] = joystick->nhats++;
            }
        }
        if (test_bit(REL_X, relbit) || test_bit(REL_Y, relbit)) {
            ++joystick->nballs;
        }

        /* Allocate data to keep track of these thingamajigs */
        if (joystick->nhats > 0) {
            if (allocate_hatdata(joystick) < 0) {
                joystick->nhats = 0;
            }
        }
        if (joystick->nballs > 0) {
            if (allocate_balldata(joystick) < 0) {
                joystick->nballs = 0;
            }
        }
    }

    if (ioctl(fd, EVIOCGBIT(EV_FF, sizeof(ffbit)), ffbit) >= 0) {
        if (test_bit(FF_RUMBLE, ffbit)) {
            joystick->hwdata->ff_rumble = SDL_TRUE;
        }
        if (test_bit(FF_SINE, ffbit)) {
            joystick->hwdata->ff_sine = SDL_TRUE;
        }
    }
}


/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
static int
LINUX_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    SDL_joylist_item *item = JoystickByDevIndex(device_index);

    if (item == NULL) {
        return SDL_SetError("No such device");
    }

    joystick->instance_id = item->device_instance;
    joystick->hwdata = (struct joystick_hwdata *)
        SDL_calloc(1, sizeof(*joystick->hwdata));
    if (joystick->hwdata == NULL) {
        return SDL_OutOfMemory();
    }
    joystick->hwdata->item = item;
    joystick->hwdata->guid = item->guid;
    joystick->hwdata->effect.id = -1;
    joystick->hwdata->m_bSteamController = item->m_bSteamController;
    SDL_memset(joystick->hwdata->abs_map, 0xFF, sizeof(joystick->hwdata->abs_map));

    if (item->m_bSteamController) {
        joystick->hwdata->fd = -1;
        SDL_GetSteamControllerInputs(&joystick->nbuttons,
                                     &joystick->naxes,
                                     &joystick->nhats);
    } else {
        int fd = open(item->path, O_RDWR, 0);
        if (fd < 0) {
            SDL_free(joystick->hwdata);
            joystick->hwdata = NULL;
            return SDL_SetError("Unable to open %s", item->path);
        }

        joystick->hwdata->fd = fd;
        joystick->hwdata->fname = SDL_strdup(item->path);
        if (joystick->hwdata->fname == NULL) {
            SDL_free(joystick->hwdata);
            joystick->hwdata = NULL;
            close(fd);
            return SDL_OutOfMemory();
        }

        /* Set the joystick to non-blocking read mode */
        fcntl(fd, F_SETFL, O_NONBLOCK);

        /* Get the number of buttons and axes on the joystick */
        ConfigJoystick(joystick, fd);
    }

    SDL_assert(item->hwdata == NULL);
    item->hwdata = joystick->hwdata;

    /* mark joystick as fresh and ready */
    joystick->hwdata->fresh = 1;

    return (0);
}

static int
LINUX_JoystickRumble(SDL_Joystick * joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    struct input_event event;

    if (joystick->hwdata->ff_rumble) {
        struct ff_effect *effect = &joystick->hwdata->effect;

        effect->type = FF_RUMBLE;
        effect->replay.length = SDL_MAX_RUMBLE_DURATION_MS;
        effect->u.rumble.strong_magnitude = low_frequency_rumble;
        effect->u.rumble.weak_magnitude = high_frequency_rumble;
    } else if (joystick->hwdata->ff_sine) {
        /* Scale and average the two rumble strengths */
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
        /* The kernel may have lost this effect, try to allocate a new one */
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
    return 0;
}

static SDL_INLINE void
HandleHat(SDL_Joystick * stick, Uint8 hat, int axis, int value)
{
    struct hwdata_hat *the_hat;
    const Uint8 position_map[3][3] = {
        {SDL_HAT_LEFTUP, SDL_HAT_UP, SDL_HAT_RIGHTUP},
        {SDL_HAT_LEFT, SDL_HAT_CENTERED, SDL_HAT_RIGHT},
        {SDL_HAT_LEFTDOWN, SDL_HAT_DOWN, SDL_HAT_RIGHTDOWN}
    };

    the_hat = &stick->hwdata->hats[hat];
    if (value < 0) {
        value = 0;
    } else if (value == 0) {
        value = 1;
    } else if (value > 0) {
        value = 2;
    }
    if (value != the_hat->axis[axis]) {
        the_hat->axis[axis] = value;
        SDL_PrivateJoystickHat(stick, hat,
                               position_map[the_hat->axis[1]][the_hat->axis[0]]);
    }
}

static SDL_INLINE void
HandleBall(SDL_Joystick * stick, Uint8 ball, int axis, int value)
{
    stick->hwdata->balls[ball].axis[axis] += value;
}


static SDL_INLINE int
AxisCorrect(SDL_Joystick * joystick, int which, int value)
{
    struct axis_correct *correct;

    correct = &joystick->hwdata->abs_correct[which];
    if (correct->used) {
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
    }

    /* Clamp and return */
    if (value < -32768)
        return -32768;
    if (value > 32767)
        return 32767;

    return value;
}

static SDL_INLINE void
PollAllValues(SDL_Joystick * joystick)
{
    struct input_absinfo absinfo;
    int i;

    /* Poll all axis */
    for (i = ABS_X; i < ABS_MAX; i++) {
        if (i == ABS_HAT0X) {
            i = ABS_HAT3Y;
            continue;
        }
        if (joystick->hwdata->abs_correct[i].used) {
            if (ioctl(joystick->hwdata->fd, EVIOCGABS(i), &absinfo) >= 0) {
                absinfo.value = AxisCorrect(joystick, i, absinfo.value);

#ifdef DEBUG_INPUT_EVENTS
                printf("Joystick : Re-read Axis %d (%d) val= %d\n",
                    joystick->hwdata->abs_map[i], i, absinfo.value);
#endif
                SDL_PrivateJoystickAxis(joystick,
                        joystick->hwdata->abs_map[i],
                        absinfo.value);
            }
        }
    }
}

static SDL_INLINE void
HandleInputEvents(SDL_Joystick * joystick)
{
    struct input_event events[32];
    int i, len;
    int code;

    if (joystick->hwdata->fresh) {
        PollAllValues(joystick);
        joystick->hwdata->fresh = 0;
    }

    while ((len = read(joystick->hwdata->fd, events, (sizeof events))) > 0) {
        len /= sizeof(events[0]);
        for (i = 0; i < len; ++i) {
            code = events[i].code;
            switch (events[i].type) {
            case EV_KEY:
                SDL_PrivateJoystickButton(joystick,
                                          joystick->hwdata->key_map[code],
                                          events[i].value);
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
                    code -= ABS_HAT0X;
                    HandleHat(joystick, joystick->hwdata->hats_indices[code / 2], code % 2, events[i].value);
                    break;
                default:
                    if (joystick->hwdata->abs_map[code] != 0xFF) {
                        events[i].value =
                            AxisCorrect(joystick, code, events[i].value);
                        SDL_PrivateJoystickAxis(joystick,
                                                joystick->hwdata->abs_map[code],
                                                events[i].value);
                    }
                    break;
                }
                break;
            case EV_REL:
                switch (code) {
                case REL_X:
                case REL_Y:
                    code -= REL_X;
                    HandleBall(joystick, code / 2, code % 2, events[i].value);
                    break;
                default:
                    break;
                }
                break;
            case EV_SYN:
                switch (code) {
                case SYN_DROPPED :
#ifdef DEBUG_INPUT_EVENTS
                    printf("Event SYN_DROPPED detected\n");
#endif
                    PollAllValues(joystick);
                    break;
                default:
                    break;
                }
            default:
                break;
            }
        }
    }

    if (errno == ENODEV) {
        /* We have to wait until the JoystickDetect callback to remove this */
        joystick->hwdata->gone = SDL_TRUE;
    }
}

static void
LINUX_JoystickUpdate(SDL_Joystick * joystick)
{
    int i;

    if (joystick->hwdata->m_bSteamController) {
        SDL_UpdateSteamController(joystick);
        return;
    }

    HandleInputEvents(joystick);

    /* Deliver ball motion updates */
    for (i = 0; i < joystick->nballs; ++i) {
        int xrel, yrel;

        xrel = joystick->hwdata->balls[i].axis[0];
        yrel = joystick->hwdata->balls[i].axis[1];
        if (xrel || yrel) {
            joystick->hwdata->balls[i].axis[0] = 0;
            joystick->hwdata->balls[i].axis[1] = 0;
            SDL_PrivateJoystickBall(joystick, (Uint8) i, xrel, yrel);
        }
    }
}

/* Function to close a joystick after use */
static void
LINUX_JoystickClose(SDL_Joystick * joystick)
{
    if (joystick->hwdata) {
        if (joystick->hwdata->effect.id >= 0) {
            ioctl(joystick->hwdata->fd, EVIOCRMFF, joystick->hwdata->effect.id);
            joystick->hwdata->effect.id = -1;
        }
        if (joystick->hwdata->fd >= 0) {
            close(joystick->hwdata->fd);
        }
        if (joystick->hwdata->item) {
            joystick->hwdata->item->hwdata = NULL;
        }
        SDL_free(joystick->hwdata->hats);
        SDL_free(joystick->hwdata->balls);
        SDL_free(joystick->hwdata->fname);
        SDL_free(joystick->hwdata);
    }
}

/* Function to perform any system-specific joystick related cleanup */
static void
LINUX_JoystickQuit(void)
{
    SDL_joylist_item *item = NULL;
    SDL_joylist_item *next = NULL;

    for (item = SDL_joylist; item; item = next) {
        next = item->next;
        SDL_free(item->path);
        SDL_free(item->name);
        SDL_free(item);
    }

    SDL_joylist = SDL_joylist_tail = NULL;

    numjoysticks = 0;

#if SDL_USE_LIBUDEV
    SDL_UDEV_DelCallback(joystick_udev_callback);
    SDL_UDEV_Quit();
#endif

    SDL_QuitSteamControllers();
}

SDL_JoystickDriver SDL_LINUX_JoystickDriver =
{
    LINUX_JoystickInit,
    LINUX_JoystickGetCount,
    LINUX_JoystickDetect,
    LINUX_JoystickGetDeviceName,
    LINUX_JoystickGetDevicePlayerIndex,
    LINUX_JoystickSetDevicePlayerIndex,
    LINUX_JoystickGetDeviceGUID,
    LINUX_JoystickGetDeviceInstanceID,
    LINUX_JoystickOpen,
    LINUX_JoystickRumble,
    LINUX_JoystickUpdate,
    LINUX_JoystickClose,
    LINUX_JoystickQuit,
};

#endif /* SDL_JOYSTICK_LINUX */

/* vi: set ts=4 sw=4 expandtab: */
