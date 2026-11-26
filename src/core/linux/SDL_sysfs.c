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

#ifdef SDL_PLATFORM_LINUX

#include "SDL_sysfs.h"

#include <limits.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>


static bool IsUSBDevice(const char *syspath)
{
    // USB devices have a bus number, USB interfaces don't
    char path[PATH_MAX];
    if (SDL_snprintf(path, sizeof(path), "%s/busnum", syspath) >= (int)sizeof(path)) {
        return false;
    }
    return access(path, F_OK) == 0;
}

// The container ID is the directory in /sys/devices that corresponds to the USB device, if available
static char *GetContainerIDFromSysfsPath(const char *syspath)
{
    char path[PATH_MAX];

    if (!realpath(syspath, path)) {
        return NULL;
    }

    // Only physical devices have a container, and for Bluetooth devices
    // the first USB device above them is the Bluetooth adapter.
    const char *devices_root = "/sys/devices/";
    if (!SDL_startswith(path, devices_root) ||
        SDL_startswith(path, "/sys/devices/virtual/") ||
        SDL_strstr(path, "/bluetooth/") != NULL) {
        return NULL;
    }

    // Walk back up to the USB device containing this path
    const char *base = path + SDL_strlen(devices_root);
    while (!IsUSBDevice(path)) {
        char *slash = SDL_strrchr(path, '/');
        if (!slash || slash <= base) {
            return NULL;
        }
        *slash = '\0';
    }

    // The sysfs path of the USB device is shared by all of its interfaces
    return SDL_strdup(path);
}

char *SDL_GetContainerIDFromDevicePath(const char *devpath)
{
    struct stat sb;
    char syspath[64];

    if (!devpath || stat(devpath, &sb) < 0 || !S_ISCHR(sb.st_mode)) {
        return NULL;
    }

    (void)SDL_snprintf(syspath, sizeof(syspath), "/sys/dev/char/%u:%u", major(sb.st_rdev), minor(sb.st_rdev));
    return GetContainerIDFromSysfsPath(syspath);
}

char *SDL_GetContainerIDFromALSACard(int card)
{
    char syspath[64];

    if (card < 0) {
        return NULL;
    }

    (void)SDL_snprintf(syspath, sizeof(syspath), "/sys/class/sound/card%d", card);
    return GetContainerIDFromSysfsPath(syspath);
}

#endif // SDL_PLATFORM_LINUX
