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

#include "SDL_devicewatcher.h"

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
#include "../core/windows/SDL_windows.h"
#endif

#ifdef SDL_PLATFORM_MACOS
#include <dispatch/dispatch.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <AvailabilityMacros.h>
// Things named "Master" were renamed to "Main" in macOS 12.0's SDK.
#if MAC_OS_X_VERSION_MIN_REQUIRED < 120000
#define kIOMainPortDefault kIOMasterPortDefault
#endif
#endif

#include "../core/linux/SDL_udev.h"

#ifdef HAVE_INOTIFY
#include <limits.h> // For the definition of NAME_MAX
#include <sys/inotify.h>
#endif

#if defined(SDL_USE_LIBUDEV) || defined(HAVE_INOTIFY)
// We wait on file descriptors instead of a condition variable
#define SDL_DEVICEWATCHER_POLL
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h> // strerror
#include <unistd.h>
#endif

struct SDL_DeviceWatcher
{
    SDL_AtomicU32 change_count;

#ifdef SDL_DEVICEWATCHER_POLL
    int wake_pipe[2];
    SDL_SpinLock change_lock;
    Uint64 change_time;
#ifdef SDL_USE_LIBUDEV
    const SDL_UDEV_Symbols *usyms;
    struct udev *udev;
    struct udev_monitor *udev_monitor;
    int udev_fd;
#endif
#ifdef HAVE_INOTIFY
    int inotify_fd;
    int dev_watch;
    int input_watch;
#endif
#else
    SDL_Mutex *lock;
    SDL_Condition *condition;
    bool woken;
    Uint64 change_time;
#endif

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    WIN_DeviceInterfaceNotification *notification;
#endif

#ifdef SDL_PLATFORM_MACOS
    dispatch_queue_t queue;
    IONotificationPortRef port;
    io_iterator_t added_iterator;
    io_iterator_t removed_iterator;
#endif
};

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)

static void WIN_DeviceInterfaceChanged(void *userdata)
{
    SDL_DeviceWatcher *watcher = (SDL_DeviceWatcher *)userdata;

    SDL_WakeDeviceWatcher(watcher, true);
}

static void WIN_InitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    // We want notifications for all interface classes, not just HID, so we see XInput (XUSB) devices
    // The callback is called on a system thread
    watcher->notification = WIN_RegisterDeviceInterfaceNotification(WIN_DeviceInterfaceChanged, watcher);
}

static void WIN_QuitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    if (watcher->notification) {
        WIN_UnregisterDeviceInterfaceNotification(watcher->notification);
        watcher->notification = NULL;
    }
}

#endif // SDL_PLATFORM_WIN32 || SDL_PLATFORM_WINGDK

#ifdef SDL_PLATFORM_MACOS

static bool DrainIterator(io_iterator_t iterator)
{
    bool changed = false;
    io_object_t entry;

    // Must drain the iterator, or we won't receive new notifications
    while ((entry = IOIteratorNext(iterator)) != 0) {
        IOObjectRelease(entry);
        changed = true;
    }
    return changed;
}

static void MACOS_DeviceChanged(void *refcon, io_iterator_t iterator)
{
    SDL_DeviceWatcher *watcher = (SDL_DeviceWatcher *)refcon;

    if (DrainIterator(iterator)) {
        SDL_WakeDeviceWatcher(watcher, true);
    }
}

static void MACOS_InitDeviceWatcherOnQueue(void *context)
{
    SDL_DeviceWatcher *watcher = (SDL_DeviceWatcher *)context;

    // Set up the notifications on the queue so callbacks can't run until we're done
    watcher->port = IONotificationPortCreate(kIOMainPortDefault);
    if (!watcher->port) {
        return;
    }
    IONotificationPortSetDispatchQueue(watcher->port, watcher->queue);

    if (IOServiceAddMatchingNotification(watcher->port, kIOFirstMatchNotification, IOServiceMatching(kIOHIDDeviceKey),
                                         MACOS_DeviceChanged, watcher, &watcher->added_iterator) == kIOReturnSuccess) {
        DrainIterator(watcher->added_iterator);
    }
    if (IOServiceAddMatchingNotification(watcher->port, kIOTerminatedNotification, IOServiceMatching(kIOHIDDeviceKey),
                                         MACOS_DeviceChanged, watcher, &watcher->removed_iterator) == kIOReturnSuccess) {
        DrainIterator(watcher->removed_iterator);
    }
}

static void MACOS_QuitDeviceWatcherOnQueue(void *context)
{
    SDL_DeviceWatcher *watcher = (SDL_DeviceWatcher *)context;

    // Tear down on the queue so no callback is running while we do it
    if (watcher->added_iterator) {
        IOObjectRelease(watcher->added_iterator);
        watcher->added_iterator = 0;
    }
    if (watcher->removed_iterator) {
        IOObjectRelease(watcher->removed_iterator);
        watcher->removed_iterator = 0;
    }
    if (watcher->port) {
        IONotificationPortDestroy(watcher->port);
        watcher->port = NULL;
    }
}

static void MACOS_InitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    watcher->queue = dispatch_queue_create("SDL device watcher", DISPATCH_QUEUE_SERIAL);
    if (watcher->queue) {
        dispatch_sync_f(watcher->queue, watcher, MACOS_InitDeviceWatcherOnQueue);
    }
}

static void MACOS_QuitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    if (watcher->queue) {
        dispatch_sync_f(watcher->queue, watcher, MACOS_QuitDeviceWatcherOnQueue);
        dispatch_release(watcher->queue);
        watcher->queue = NULL;
    }
}

#endif // SDL_PLATFORM_MACOS

#ifdef SDL_USE_LIBUDEV

static void UDEV_InitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    const SDL_UDEV_Symbols *usyms;

    if (SDL_GetHintBoolean("SDL_JOYSTICK_DISABLE_UDEV", false)) {
        return;
    }

    if (SDL_GetSandbox() != SDL_SANDBOX_NONE) {
        // udev events are unreliable in containers
        return;
    }

    usyms = SDL_UDEV_GetUdevSyms();
    if (!usyms) {
        return;
    }
    watcher->usyms = usyms;

    watcher->udev = usyms->udev_new();
    if (!watcher->udev) {
        return;
    }

    watcher->udev_monitor = usyms->udev_monitor_new_from_netlink(watcher->udev, "udev");
    if (!watcher->udev_monitor) {
        return;
    }
    usyms->udev_monitor_filter_add_match_subsystem_devtype(watcher->udev_monitor, "input", NULL);
    usyms->udev_monitor_filter_add_match_subsystem_devtype(watcher->udev_monitor, "hidraw", NULL);
    usyms->udev_monitor_filter_add_match_subsystem_devtype(watcher->udev_monitor, "usb", NULL);
    if (usyms->udev_monitor_enable_receiving(watcher->udev_monitor) < 0) {
        return;
    }
    watcher->udev_fd = usyms->udev_monitor_get_fd(watcher->udev_monitor);
}

static bool UDEV_ReadDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    const SDL_UDEV_Symbols *usyms = watcher->usyms;
    bool changed = false;

    for (;;) {
        struct pollfd fd;
        struct udev_device *device;

        fd.fd = watcher->udev_fd;
        fd.events = POLLIN;
        if (poll(&fd, 1, 0) != 1) {
            break;
        }

        device = usyms->udev_monitor_receive_device(watcher->udev_monitor);
        if (device) {
            // The udev monitor reports events after rules have run, so permissions are already set
            const char *action = usyms->udev_device_get_action(device);
            if (!action || SDL_strcmp(action, "add") == 0 || SDL_strcmp(action, "remove") == 0) {
                changed = true;
            }
            usyms->udev_device_unref(device);
        }
    }
    return changed;
}

static void UDEV_QuitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    const SDL_UDEV_Symbols *usyms = watcher->usyms;

    if (!usyms) {
        return;
    }

    if (watcher->udev_monitor) {
        usyms->udev_monitor_unref(watcher->udev_monitor);
        watcher->udev_monitor = NULL;
    }
    if (watcher->udev) {
        usyms->udev_unref(watcher->udev);
        watcher->udev = NULL;
    }
    watcher->udev_fd = -1;

    SDL_UDEV_ReleaseUdevSyms();
    watcher->usyms = NULL;
}

#endif // SDL_USE_LIBUDEV

#ifdef HAVE_INOTIFY

static void INOTIFY_InitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    // We need to watch for attribute changes in addition to creation,
    // because when a device is first created it has permissions that
    // we can't read. When udev chmods it to something that we maybe
    // *can* read, we'll get an IN_ATTRIB event to tell us.
    const Uint32 mask = IN_CREATE | IN_DELETE | IN_MOVE | IN_ATTRIB;

#ifdef HAVE_INOTIFY_INIT1
    watcher->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
#else
    watcher->inotify_fd = inotify_init();
    if (watcher->inotify_fd >= 0) {
        fcntl(watcher->inotify_fd, F_SETFL, O_NONBLOCK);
        fcntl(watcher->inotify_fd, F_SETFD, FD_CLOEXEC);
    }
#endif
    if (watcher->inotify_fd < 0) {
        return;
    }

    watcher->dev_watch = inotify_add_watch(watcher->inotify_fd, "/dev", mask);
    watcher->input_watch = inotify_add_watch(watcher->inotify_fd, "/dev/input", mask);
    if (watcher->dev_watch < 0 && watcher->input_watch < 0) {
        close(watcher->inotify_fd);
        watcher->inotify_fd = -1;
    }
}

static bool INOTIFY_ReadDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    union
    {
        struct inotify_event event;
        char storage[4096];
        char enough_for_inotify[sizeof(struct inotify_event) + NAME_MAX + 1];
    } buf;
    bool changed = false;
    ssize_t bytes;

    while ((bytes = read(watcher->inotify_fd, &buf, sizeof(buf))) > 0) {
        size_t remain = (size_t)bytes;
        const char *p = buf.storage;

        while (remain >= sizeof(struct inotify_event)) {
            const struct inotify_event *event = (const struct inotify_event *)p;
            size_t len = sizeof(struct inotify_event) + event->len;

            if (event->mask & IN_Q_OVERFLOW) {
                changed = true;
            } else if (event->len > 0) {
                if (event->wd == watcher->input_watch) {
                    if (SDL_startswith(event->name, "event") || SDL_startswith(event->name, "js")) {
                        changed = true;
                    }
                } else if (event->wd == watcher->dev_watch) {
                    if (SDL_startswith(event->name, "hidraw")) {
                        changed = true;
                    }
                }
            }

            if (len > remain) {
                break;
            }
            p += len;
            remain -= len;
        }
    }
    return changed;
}

static void INOTIFY_QuitDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    if (watcher->inotify_fd >= 0) {
        close(watcher->inotify_fd);
        watcher->inotify_fd = -1;
    }
}

#endif // HAVE_INOTIFY

#ifdef SDL_DEVICEWATCHER_POLL

static void SetChangeTime(SDL_DeviceWatcher *watcher, Uint64 change_time)
{
    SDL_LockSpinlock(&watcher->change_lock);
    watcher->change_time = change_time;
    SDL_UnlockSpinlock(&watcher->change_lock);
}

static Uint64 GetAndClearChangeTime(SDL_DeviceWatcher *watcher)
{
    Uint64 change_time;

    SDL_LockSpinlock(&watcher->change_lock);
    change_time = watcher->change_time;
    watcher->change_time = 0;
    SDL_UnlockSpinlock(&watcher->change_lock);

    return change_time;
}

static bool CreateWakePipe(SDL_DeviceWatcher *watcher)
{
    if (pipe(watcher->wake_pipe) < 0) {
        watcher->wake_pipe[0] = -1;
        watcher->wake_pipe[1] = -1;
        return SDL_SetError("Couldn't create device watcher pipe: %s", strerror(errno));
    }

    for (int i = 0; i < SDL_arraysize(watcher->wake_pipe); ++i) {
        fcntl(watcher->wake_pipe[i], F_SETFL, O_NONBLOCK);
        fcntl(watcher->wake_pipe[i], F_SETFD, FD_CLOEXEC);
    }
    return true;
}

static void DrainWakePipe(SDL_DeviceWatcher *watcher)
{
    char buf[64];

    while (read(watcher->wake_pipe[0], buf, sizeof(buf)) > 0) {
        continue;
    }
}

static void DestroyWakePipe(SDL_DeviceWatcher *watcher)
{
    for (int i = 0; i < SDL_arraysize(watcher->wake_pipe); ++i) {
        if (watcher->wake_pipe[i] >= 0) {
            close(watcher->wake_pipe[i]);
            watcher->wake_pipe[i] = -1;
        }
    }
}

#endif // SDL_DEVICEWATCHER_POLL

SDL_DeviceWatcher *SDL_CreateDeviceWatcher(void)
{
    SDL_DeviceWatcher *watcher = (SDL_DeviceWatcher *)SDL_calloc(1, sizeof(*watcher));
    if (!watcher) {
        return NULL;
    }

#ifdef SDL_DEVICEWATCHER_POLL
    watcher->wake_pipe[0] = -1;
    watcher->wake_pipe[1] = -1;
#ifdef SDL_USE_LIBUDEV
    watcher->udev_fd = -1;
#endif
#ifdef HAVE_INOTIFY
    watcher->inotify_fd = -1;
    watcher->dev_watch = -1;
    watcher->input_watch = -1;
#endif

    if (!CreateWakePipe(watcher)) {
        SDL_DestroyDeviceWatcher(watcher);
        return NULL;
    }

#ifdef SDL_USE_LIBUDEV
    UDEV_InitDeviceWatcher(watcher);
    if (watcher->udev_fd < 0) {
        UDEV_QuitDeviceWatcher(watcher);
    }
#endif
#ifdef HAVE_INOTIFY
#ifdef SDL_USE_LIBUDEV
    if (watcher->udev_fd < 0)
#endif
    {
        INOTIFY_InitDeviceWatcher(watcher);
    }
#endif
#else
    watcher->lock = SDL_CreateMutex();
    watcher->condition = SDL_CreateCondition();
    if (!watcher->lock || !watcher->condition) {
        SDL_DestroyDeviceWatcher(watcher);
        return NULL;
    }
#endif // SDL_DEVICEWATCHER_POLL

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    WIN_InitDeviceWatcher(watcher);
#endif

#ifdef SDL_PLATFORM_MACOS
    MACOS_InitDeviceWatcher(watcher);
#endif

    return watcher;
}

Uint64 SDL_WaitDeviceWatcher(SDL_DeviceWatcher *watcher, Sint32 timeoutMS)
{
    // We'll poll every 3 seconds as a fallback
    const Sint32 FALLBACK_POLL_INTERVAL = 3000;

#ifdef SDL_DEVICEWATCHER_POLL
    struct pollfd fds[2];
    int count = 0;
    int notify_fd = -1;

#ifdef SDL_USE_LIBUDEV
    if (watcher->udev_fd >= 0) {
        notify_fd = watcher->udev_fd;
    }
#endif
#ifdef HAVE_INOTIFY
    if (watcher->inotify_fd >= 0) {
        notify_fd = watcher->inotify_fd;
    }
#endif

    fds[count].fd = watcher->wake_pipe[0];
    fds[count].events = POLLIN;
    fds[count].revents = 0;
    ++count;

    if (notify_fd >= 0) {
        fds[count].fd = notify_fd;
        fds[count].events = POLLIN;
        fds[count].revents = 0;
        ++count;
    } else {
        if (timeoutMS < 0 || timeoutMS > FALLBACK_POLL_INTERVAL) {
            timeoutMS = FALLBACK_POLL_INTERVAL;
        }
    }

    if (poll(fds, count, timeoutMS) > 0) {
        if (fds[0].revents & POLLIN) {
            DrainWakePipe(watcher);
        }
        if (count > 1 && (fds[1].revents & POLLIN)) {
            bool changed = false;
#ifdef SDL_USE_LIBUDEV
            if (watcher->udev_fd >= 0) {
                changed = UDEV_ReadDeviceWatcher(watcher);
            }
#endif
#ifdef HAVE_INOTIFY
            if (watcher->inotify_fd >= 0) {
                changed = INOTIFY_ReadDeviceWatcher(watcher);
            }
#endif
            if (changed) {
                SDL_AddAtomicU32(&watcher->change_count, 1);
                SetChangeTime(watcher, SDL_GetTicks());
            }
        }
    }
    return GetAndClearChangeTime(watcher);
#else
    Uint64 change_time;
    bool use_fallback_timeout = true;

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    if (watcher->notification) {
        use_fallback_timeout = false;
    }
#endif

#ifdef SDL_PLATFORM_MACOS
    if (watcher->queue) {
        use_fallback_timeout = false;
    }
#endif

    if (use_fallback_timeout) {
        if (timeoutMS < 0 || timeoutMS > FALLBACK_POLL_INTERVAL) {
            timeoutMS = FALLBACK_POLL_INTERVAL;
        }
    }

    SDL_LockMutex(watcher->lock);
    if (!watcher->woken) {
        SDL_WaitConditionTimeout(watcher->condition, watcher->lock, timeoutMS);
    }
    change_time = watcher->change_time;
    watcher->woken = false;
    watcher->change_time = 0;
    SDL_UnlockMutex(watcher->lock);

    return change_time;
#endif // SDL_DEVICEWATCHER_POLL
}

void SDL_WakeDeviceWatcher(SDL_DeviceWatcher *watcher, bool device_changed)
{
#ifdef SDL_DEVICEWATCHER_POLL
    if (device_changed) {
        SDL_AddAtomicU32(&watcher->change_count, 1);
        SetChangeTime(watcher, SDL_GetTicks());
    }

    const char wake = 1;
    if (write(watcher->wake_pipe[1], &wake, sizeof(wake)) < 0) {
        // The pipe is full, so a wakeup is already pending
    }
#else
    SDL_LockMutex(watcher->lock);
    watcher->woken = true;
    if (device_changed) {
        SDL_AddAtomicU32(&watcher->change_count, 1);
        watcher->change_time = SDL_GetTicks();
    }
    SDL_SignalCondition(watcher->condition);
    SDL_UnlockMutex(watcher->lock);
#endif // SDL_DEVICEWATCHER_POLL
}

Uint32 SDL_GetDeviceWatcherChangeCount(SDL_DeviceWatcher *watcher)
{
    return SDL_GetAtomicU32(&watcher->change_count);
}

void SDL_DestroyDeviceWatcher(SDL_DeviceWatcher *watcher)
{
    if (!watcher) {
        return;
    }

#if defined(SDL_PLATFORM_WIN32) || defined(SDL_PLATFORM_WINGDK)
    WIN_QuitDeviceWatcher(watcher);
#endif

#ifdef SDL_PLATFORM_MACOS
    MACOS_QuitDeviceWatcher(watcher);
#endif

#ifdef SDL_DEVICEWATCHER_POLL
#ifdef SDL_USE_LIBUDEV
    UDEV_QuitDeviceWatcher(watcher);
#endif
#ifdef HAVE_INOTIFY
    INOTIFY_QuitDeviceWatcher(watcher);
#endif
    DestroyWakePipe(watcher);
#else
    if (watcher->condition) {
        SDL_DestroyCondition(watcher->condition);
    }
    if (watcher->lock) {
        SDL_DestroyMutex(watcher->lock);
    }
#endif // SDL_DEVICEWATCHER_POLL

    SDL_free(watcher);
}
