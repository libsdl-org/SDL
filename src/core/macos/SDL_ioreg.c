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

#include "SDL_ioreg.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/hid/IOHIDKeys.h>


static char *GetContainerIDFromProperties(CFTypeRef transport, CFTypeRef location)
{
    char *result = NULL;

    // Only USB devices can be matched with audio devices
    if (!transport || CFGetTypeID(transport) != CFStringGetTypeID() ||
        CFStringCompare((CFStringRef)transport, CFSTR(kIOHIDTransportUSBValue), 0) != kCFCompareEqualTo) {
        return NULL;
    }

    Uint32 location_id = 0;
    if (!location || CFGetTypeID(location) != CFNumberGetTypeID() ||
        !CFNumberGetValue((CFNumberRef)location, kCFNumberSInt32Type, &location_id) ||
        location_id == 0) {
        return NULL;
    }

    // This needs to match the format used in SDL_coreaudio.m
    SDL_asprintf(&result, "0x%08" SDL_PRIx32, location_id);
    return result;
}

char *SDL_GetContainerIDFromRegistryEntryID(Uint64 entry_id)
{
    char *result = NULL;

    io_service_t service = IOServiceGetMatchingService(0, IORegistryEntryIDMatching(entry_id));
    if (service == MACH_PORT_NULL) {
        return NULL;
    }

    CFTypeRef transport = IORegistryEntryCreateCFProperty(service, CFSTR(kIOHIDTransportKey), kCFAllocatorDefault, 0);
    if (transport) {
        CFTypeRef location = IORegistryEntryCreateCFProperty(service, CFSTR(kIOHIDLocationIDKey), kCFAllocatorDefault, 0);
        if (location) {
            result = GetContainerIDFromProperties(transport, location);
            CFRelease(location);
        }
        CFRelease(transport);
    }
    IOObjectRelease(service);

    return result;
}

char *SDL_GetContainerIDFromHIDDevice(IOHIDDeviceRef device)
{
    if (!device) {
        return NULL;
    }

    return GetContainerIDFromProperties(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDTransportKey)),
                                        IOHIDDeviceGetProperty(device, CFSTR(kIOHIDLocationIDKey)));
}
