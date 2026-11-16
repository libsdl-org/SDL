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

#ifdef SDL_JOYSTICK_IOKIT

#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "SDL_iokitjoystick_c.h"
#include "../hidapi/SDL_hidapijoystick_c.h"
#include "../../haptic/darwin/SDL_syshaptic_c.h" // For haptic hot plugging
#include "../usb_ids.h"
#include "../../SDL_hints_c.h"
#include "../../core/macos/SDL_ioreg.h"
#include <IOKit/IOKitLib.h>
#include <mach/mach.h>

#define CONVERT_MAGNITUDE(x) (((x)*10000) / 0x7FFF)

/* Device notifications are polled from the notification port's mach port, rather than
 * delivered on a run loop, so detection works from whatever thread runs joystick updates.
 */
static IONotificationPortRef gNotificationPort = NULL;
static mach_port_t gNotificationMach = MACH_PORT_NULL;
static io_iterator_t gAddedIterators[3];
static io_iterator_t gRemovedIterator = IO_OBJECT_NULL;

// Linked list of all available devices
static recDevice *gpDeviceList = NULL;

// Hashtable of devices seen by IOKit, indexed by product string, used to identify MFi controllers
typedef struct
{
    Uint64 entry_id;
    Uint16 bus;
    Uint16 vendor_id;
    Uint16 product_id;
    char *name;
} KnownDevice;

static SDL_HashTable *gKnownDevices = NULL;

static Sint32 GetDeviceNumberProperty(IOHIDDeviceRef hidDevice, CFStringRef key)
{
    Sint32 value = 0;
    CFTypeRef refCF = IOHIDDeviceGetProperty(hidDevice, key);
    if (refCF) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &value);
    }
    return value;
}

static void GetDeviceStringProperty(IOHIDDeviceRef hidDevice, CFStringRef key, char *string, size_t size)
{
    CFTypeRef refCF = IOHIDDeviceGetProperty(hidDevice, key);
    if (!refCF || CFGetTypeID(refCF) != CFStringGetTypeID() ||
        !CFStringGetCString(refCF, string, size, kCFStringEncodingUTF8)) {
        string[0] = '\0';
    }
}

static Uint16 GetDeviceBus(IOHIDDeviceRef hidDevice)
{
    CFTypeRef refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDTransportKey));
    if (refCF && CFGetTypeID(refCF) == CFStringGetTypeID() &&
        CFStringHasPrefix((CFStringRef)refCF, CFSTR(kIOHIDTransportBluetoothValue))) {
        return SDL_HARDWARE_BUS_BLUETOOTH;
    }
    return SDL_HARDWARE_BUS_USB;
}

static void SDLCALL DARWIN_DestroyKnownDevice(void *unused, const void *key, const void *value)
{
    KnownDevice *entry = (KnownDevice *)value;

    SDL_free((void *)key);
    SDL_free(entry->name);
    SDL_free(entry);
}

static void DARWIN_AddKnownDevice(IOHIDDeviceRef hidDevice, Uint64 entry_id)
{
    char manufacturer_string[256];
    char product_string[256];

    Uint16 vendor = (Uint16)GetDeviceNumberProperty(hidDevice, CFSTR(kIOHIDVendorIDKey));
    Uint16 product = (Uint16)GetDeviceNumberProperty(hidDevice, CFSTR(kIOHIDProductIDKey));
    if (!vendor && !product) {
        return;
    }

    GetDeviceStringProperty(hidDevice, CFSTR(kIOHIDProductKey), product_string, sizeof(product_string));
    if (!*product_string) {
        return;
    }
    GetDeviceStringProperty(hidDevice, CFSTR(kIOHIDManufacturerKey), manufacturer_string, sizeof(manufacturer_string));

    if (!gKnownDevices) {
        gKnownDevices = SDL_CreateHashTable(0, false, SDL_HashString, SDL_KeyMatchString, DARWIN_DestroyKnownDevice, NULL);
        if (!gKnownDevices) {
            return;
        }
    }

    char *name = SDL_CreateJoystickName(vendor, product, manufacturer_string, product_string);
    char *key = SDL_strdup(product_string);
    KnownDevice *value = (KnownDevice *)SDL_calloc(1, sizeof(*value));
    if (!name || !key || !value) {
        SDL_free(name);
        SDL_free(key);
        SDL_free(value);
        return;
    }
    value->entry_id = entry_id;
    value->bus = GetDeviceBus(hidDevice);
    value->vendor_id = vendor;
    value->product_id = product;
    value->name = name;
    SDL_InsertIntoHashTable(gKnownDevices, key, value, true);
}

typedef struct
{
    Uint64 entry_id;
    const char *key;
} KnownDeviceSearch;

static bool SDLCALL DARWIN_FindKnownDevice(void *userdata, const SDL_HashTable *table, const void *key, const void *value)
{
    KnownDeviceSearch *search = (KnownDeviceSearch *)userdata;
    const KnownDevice *entry = (const KnownDevice *)value;

    if (entry->entry_id == search->entry_id) {
        search->key = (const char *)key;
        return false;
    }
    return true;
}

static void DARWIN_RemoveKnownDevice(Uint64 entry_id)
{
    if (!gKnownDevices) {
        return;
    }

    // If another device with the same product string replaced this entry, it won't be found and is left alone
    KnownDeviceSearch search = { entry_id, NULL };
    SDL_IterateHashTable(gKnownDevices, DARWIN_FindKnownDevice, &search);
    if (search.key) {
        SDL_RemoveFromHashTable(gKnownDevices, search.key);
    }
}

// Returns the bus, VID/PID and a copy of the joystick name, which should be freed with SDL_free()
bool DARWIN_GetKnownDevice(const char *product_string, Uint16 *bus, Uint16 *vendor_id, Uint16 *product_id, char **name)
{
    const void *value;
    if (!gKnownDevices || !SDL_FindInHashTable(gKnownDevices, product_string, &value)) {
        return false;
    }

    const KnownDevice *entry = (const KnownDevice *)value;
    *name = SDL_strdup(entry->name);
    if (!*name) {
        return false;
    }
    *bus = entry->bus;
    *vendor_id = entry->vendor_id;
    *product_id = entry->product_id;
    return true;
}

static void DARWIN_DestroyKnownDevices(void)
{
    if (gKnownDevices) {
        SDL_DestroyHashTable(gKnownDevices);
        gKnownDevices = NULL;
    }
}

void FreeRumbleEffectData(FFEFFECT *effect)
{
    if (!effect) {
        return;
    }
    SDL_free(effect->rgdwAxes);
    SDL_free(effect->rglDirection);
    SDL_free(effect->lpvTypeSpecificParams);
    SDL_free(effect);
}

FFEFFECT *CreateRumbleEffectData(Sint16 magnitude)
{
    FFEFFECT *effect;
    FFPERIODIC *periodic;

    // Create the effect
    effect = (FFEFFECT *)SDL_calloc(1, sizeof(*effect));
    if (!effect) {
        return NULL;
    }
    effect->dwSize = sizeof(*effect);
    effect->dwGain = 10000;
    effect->dwFlags = FFEFF_OBJECTOFFSETS;
    effect->dwDuration = SDL_MAX_RUMBLE_DURATION_MS * 1000; // In microseconds.
    effect->dwTriggerButton = FFEB_NOTRIGGER;

    effect->cAxes = 2;
    effect->rgdwAxes = (DWORD *)SDL_calloc(effect->cAxes, sizeof(DWORD));
    if (!effect->rgdwAxes) {
        FreeRumbleEffectData(effect);
        return NULL;
    }

    effect->rglDirection = (LONG *)SDL_calloc(effect->cAxes, sizeof(LONG));
    if (!effect->rglDirection) {
        FreeRumbleEffectData(effect);
        return NULL;
    }
    effect->dwFlags |= FFEFF_CARTESIAN;

    periodic = (FFPERIODIC *)SDL_calloc(1, sizeof(*periodic));
    if (!periodic) {
        FreeRumbleEffectData(effect);
        return NULL;
    }
    periodic->dwMagnitude = CONVERT_MAGNITUDE(magnitude);
    periodic->dwPeriod = 1000000;

    effect->cbTypeSpecificParams = sizeof(*periodic);
    effect->lpvTypeSpecificParams = periodic;

    return effect;
}

static void FreeElementList(recElement *pElement)
{
    while (pElement) {
        recElement *pElementNext = pElement->pNext;
        SDL_free(pElement);
        pElement = pElementNext;
    }
}

static void ReleaseDeviceRef(recDevice *device)
{
    if (device->deviceRef) {
        if (device->opened) {
            IOHIDDeviceClose(device->deviceRef, kIOHIDOptionsTypeNone);
            device->opened = false;
        }
        CFRelease(device->deviceRef);
        device->deviceRef = NULL;
    }
}

static recDevice *FreeDevice(recDevice *removeDevice)
{
    recDevice *pDeviceNext = NULL;
    if (removeDevice) {
        ReleaseDeviceRef(removeDevice);

        /* clear out any reference to removeDevice from an associated,
         * live instance of SDL_Joystick  (Part-of fix for Bug 5034)
         */
        SDL_LockJoysticks();
        if (removeDevice->joystick) {
            removeDevice->joystick->hwdata = NULL;
        }
        SDL_UnlockJoysticks();

        // save next device prior to disposing of this device
        pDeviceNext = removeDevice->pNext;

        if (gpDeviceList == removeDevice) {
            gpDeviceList = pDeviceNext;
        } else if (gpDeviceList) {
            recDevice *device;

            for (device = gpDeviceList; device; device = device->pNext) {
                if (device->pNext == removeDevice) {
                    device->pNext = pDeviceNext;
                    break;
                }
            }
        }
        removeDevice->pNext = NULL;

        // free element lists
        FreeElementList(removeDevice->firstAxis);
        FreeElementList(removeDevice->firstButton);
        FreeElementList(removeDevice->firstHat);

        SDL_free(removeDevice);
    }
    return pDeviceNext;
}

static bool GetHIDElementState(recDevice *pDevice, recElement *pElement, SInt32 *pValue)
{
    SInt32 value = 0;
    bool result = false;

    if (pDevice && pDevice->deviceRef && pElement) {
        IOHIDValueRef valueRef;
        if (IOHIDDeviceGetValue(pDevice->deviceRef, pElement->elementRef, &valueRef) == kIOReturnSuccess) {
            value = (SInt32)IOHIDValueGetIntegerValue(valueRef);

            // record min and max for auto calibration
            if (value < pElement->minReport) {
                pElement->minReport = value;
            }
            if (value > pElement->maxReport) {
                pElement->maxReport = value;
            }
            *pValue = value;

            result = true;
        }
    }
    return result;
}

static bool GetHIDScaledCalibratedState(recDevice *pDevice, recElement *pElement, SInt32 min, SInt32 max, SInt32 *pValue)
{
    const float deviceScale = max - min;
    const float readScale = pElement->maxReport - pElement->minReport;
    bool result = false;
    if (GetHIDElementState(pDevice, pElement, pValue)) {
        if (readScale == 0) {
            result = true; // no scaling at all
        } else {
            *pValue = (Sint32)(((*pValue - pElement->minReport) * deviceScale / readScale) + min);
            result = true;
        }
    }
    return result;
}

static bool GetHIDScaledCalibratedState_NACON_Revolution_X_Unlimited(recDevice *pDevice, recElement *pElement, SInt32 min, SInt32 max, SInt32 *pValue)
{
    if (pElement->minReport == 0 && pElement->maxReport == 255) {
        return GetHIDScaledCalibratedState(pDevice, pElement, min, max, pValue);
    }

    // This device thumbstick axes have an unusual axis range that
    // doesn't work with GetHIDScaledCalibratedState() above.
    //
    // See https://github.com/libsdl-org/SDL/issues/13143 for details
    if (GetHIDElementState(pDevice, pElement, pValue)) {
        if (*pValue >= 0) {
            // Negative axis values range from 32767 (at rest) to 0 (minimum)
            *pValue = -32767 + *pValue;
        } else if (*pValue < 0) {
            // Positive axis values range from -32768 (at rest) to 0 (maximum)
            *pValue = 32768 + *pValue;
        }
        return true;
    }
    return false;
}

static Uint64 GetServiceEntryID(io_service_t service)
{
    uint64_t entry_id = 0;
    if (IORegistryEntryGetRegistryEntryID(service, &entry_id) != KERN_SUCCESS) {
        return 0;
    }
    return entry_id;
}

static void JoystickDeviceWasRemoved(io_service_t service)
{
    recDevice *device;
    Uint64 entry_id = GetServiceEntryID(service);

    if (!entry_id) {
        return;
    }

    DARWIN_RemoveKnownDevice(entry_id);

    for (device = gpDeviceList; device; device = device->pNext) {
        if (!device->removed && device->entry_id == entry_id) {
            break;
        }
    }
    if (!device) {
        return;
    }
    device->removed = true;

    ReleaseDeviceRef(device);

    if (device->ffeffect_ref) {
        FFDeviceReleaseEffect(device->ffdevice, device->ffeffect_ref);
        device->ffeffect_ref = NULL;
    }
    if (device->ffeffect) {
        FreeRumbleEffectData(device->ffeffect);
        device->ffeffect = NULL;
    }
    if (device->ffdevice) {
        FFReleaseDevice(device->ffdevice);
        device->ffdevice = NULL;
        device->ff_initialized = false;
    }
#ifdef SDL_HAPTIC_IOKIT
    MacHaptic_MaybeRemoveDevice(device->ffservice);
#endif
}

static void JoystickDevicesWereRemoved(void *refcon, io_iterator_t iterator)
{
    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        JoystickDeviceWasRemoved(service);
        IOObjectRelease(service);
    }
}

static void AddHIDElement(const void *value, void *parameter);

// Call AddHIDElement() on all elements in an array of IOHIDElementRefs
static void AddHIDElements(CFArrayRef array, recDevice *pDevice)
{
    const CFRange range = { 0, CFArrayGetCount(array) };
    CFArrayApplyFunction(array, range, AddHIDElement, pDevice);
}

static bool ElementAlreadyAdded(const IOHIDElementCookie cookie, const recElement *listitem)
{
    while (listitem) {
        if (listitem->cookie == cookie) {
            return true;
        }
        listitem = listitem->pNext;
    }
    return false;
}

// See if we care about this HID element, and if so, note it in our recDevice.
static void AddHIDElement(const void *value, void *parameter)
{
    recDevice *pDevice = (recDevice *)parameter;
    IOHIDElementRef refElement = (IOHIDElementRef)value;
    const CFTypeID elementTypeID = refElement ? CFGetTypeID(refElement) : 0;

    if (refElement && (elementTypeID == IOHIDElementGetTypeID())) {
        const IOHIDElementCookie cookie = IOHIDElementGetCookie(refElement);
        const uint32_t usagePage = IOHIDElementGetUsagePage(refElement);
        const uint32_t usage = IOHIDElementGetUsage(refElement);
        recElement *element = NULL;
        recElement **headElement = NULL;

        // look at types of interest
        switch (IOHIDElementGetType(refElement)) {
        case kIOHIDElementTypeInput_Misc:
        case kIOHIDElementTypeInput_Button:
        case kIOHIDElementTypeInput_Axis:
        {
            switch (usagePage) { // only interested in kHIDPage_GenericDesktop and kHIDPage_Button
            case kHIDPage_GenericDesktop:
                switch (usage) {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                case kHIDUsage_GD_Z:
                case kHIDUsage_GD_Rx:
                case kHIDUsage_GD_Ry:
                case kHIDUsage_GD_Rz:
                case kHIDUsage_GD_Slider:
                case kHIDUsage_GD_Dial:
                case kHIDUsage_GD_Wheel:
                    if (!ElementAlreadyAdded(cookie, pDevice->firstAxis)) {
                        element = (recElement *)SDL_calloc(1, sizeof(recElement));
                        if (element) {
                            pDevice->axes++;
                            headElement = &(pDevice->firstAxis);
                        }
                    }
                    break;

                case kHIDUsage_GD_Hatswitch:
                    if (!ElementAlreadyAdded(cookie, pDevice->firstHat)) {
                        element = (recElement *)SDL_calloc(1, sizeof(recElement));
                        if (element) {
                            pDevice->hats++;
                            headElement = &(pDevice->firstHat);
                        }
                    }
                    break;
                case kHIDUsage_GD_DPadUp:
                case kHIDUsage_GD_DPadDown:
                case kHIDUsage_GD_DPadRight:
                case kHIDUsage_GD_DPadLeft:
                case kHIDUsage_GD_Start:
                case kHIDUsage_GD_Select:
                case kHIDUsage_GD_SystemMainMenu:
                    if (!ElementAlreadyAdded(cookie, pDevice->firstButton)) {
                        element = (recElement *)SDL_calloc(1, sizeof(recElement));
                        if (element) {
                            pDevice->buttons++;
                            headElement = &(pDevice->firstButton);
                        }
                    }
                    break;
                }
                break;

            case kHIDPage_Simulation:
                switch (usage) {
                case kHIDUsage_Sim_Rudder:
                case kHIDUsage_Sim_Throttle:
                case kHIDUsage_Sim_Accelerator:
                case kHIDUsage_Sim_Brake:
                    if (!ElementAlreadyAdded(cookie, pDevice->firstAxis)) {
                        element = (recElement *)SDL_calloc(1, sizeof(recElement));
                        if (element) {
                            pDevice->axes++;
                            headElement = &(pDevice->firstAxis);
                        }
                    }
                    break;

                default:
                    break;
                }
                break;

            case kHIDPage_Button:
            case kHIDPage_Consumer: // e.g. 'pause' button on Steelseries MFi gamepads.
                if (!ElementAlreadyAdded(cookie, pDevice->firstButton)) {
                    element = (recElement *)SDL_calloc(1, sizeof(recElement));
                    if (element) {
                        pDevice->buttons++;
                        headElement = &(pDevice->firstButton);
                    }
                }
                break;

            default:
                break;
            }
        } break;

        case kIOHIDElementTypeCollection:
        {
            CFArrayRef array = IOHIDElementGetChildren(refElement);
            if (array) {
                AddHIDElements(array, pDevice);
            }
        } break;

        default:
            break;
        }

        if (element && headElement) { // add to list
            recElement *elementPrevious = NULL;
            recElement *elementCurrent = *headElement;
            while (elementCurrent && usage >= elementCurrent->usage) {
                elementPrevious = elementCurrent;
                elementCurrent = elementCurrent->pNext;
            }
            if (elementPrevious) {
                elementPrevious->pNext = element;
            } else {
                *headElement = element;
            }

            element->elementRef = refElement;
            element->usagePage = usagePage;
            element->usage = usage;
            element->pNext = elementCurrent;

            element->minReport = element->min = (SInt32)IOHIDElementGetLogicalMin(refElement);
            element->maxReport = element->max = (SInt32)IOHIDElementGetLogicalMax(refElement);
            element->cookie = IOHIDElementGetCookie(refElement);

            pDevice->elements++;
        }
    }
}

static int GetSteamVirtualGamepadSlot(Uint16 vendor_id, Uint16 product_id, const char *product_string)
{
    int slot = -1;

    if (vendor_id == USB_VENDOR_MICROSOFT && product_id == USB_PRODUCT_XBOX360_WIRED_CONTROLLER) {
        // Gamepad name is "GamePad-N", where N is slot + 1
        if (SDL_sscanf(product_string, "GamePad-%d", &slot) == 1) {
            slot -= 1;
        }
    }
    return slot;
}

static bool IsControlledBy360ControllerDriver(IOHIDDeviceRef hidDevice)
{
    bool controlled_by_360controller = false;
    io_service_t service = IOHIDDeviceGetService(hidDevice);
    if (service != MACH_PORT_NULL) {
        controlled_by_360controller = IOObjectConformsTo(service, "Xbox360ControllerClass");
    }
    return controlled_by_360controller;
}

static bool GetDeviceInfo(IOHIDDeviceRef hidDevice, recDevice *pDevice)
{
    Sint32 vendor = 0;
    Sint32 product = 0;
    Sint32 version = 0;
    char *name;
    char manufacturer_string[256];
    char product_string[256];
    CFTypeRef refCF = NULL;
    CFArrayRef array = NULL;

    // get usage page and usage
    refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDPrimaryUsagePageKey));
    if (refCF) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &pDevice->usagePage);
    }
    if (pDevice->usagePage != kHIDPage_GenericDesktop) {
        return false; // Filter device list to non-keyboard/mouse stuff
    }

    refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDPrimaryUsageKey));
    if (refCF) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &pDevice->usage);
    }

    if ((pDevice->usage != kHIDUsage_GD_Joystick &&
         pDevice->usage != kHIDUsage_GD_GamePad &&
         pDevice->usage != kHIDUsage_GD_MultiAxisController)) {
        return false; // Filter device list to non-keyboard/mouse stuff
    }

    /* Make sure we retain the use of the IOKit-provided device-object,
       lest the device get disconnected and we try to use it.  (Fixes
       SDL-Bugzilla #4961, aka. https://bugzilla.libsdl.org/show_bug.cgi?id=4961 )
    */
    CFRetain(hidDevice);

    /* Now that we've CFRetain'ed the device-object (for our use), we'll
       save the reference to it.
    */
    pDevice->deviceRef = hidDevice;

    vendor = GetDeviceNumberProperty(hidDevice, CFSTR(kIOHIDVendorIDKey));
    product = GetDeviceNumberProperty(hidDevice, CFSTR(kIOHIDProductIDKey));
    version = GetDeviceNumberProperty(hidDevice, CFSTR(kIOHIDVersionNumberKey));

    if (!IsControlledBy360ControllerDriver(hidDevice) && SDL_IsJoystickXboxOne(vendor, product)) {
        // We can't actually use this API for Xbox controllers without the 360Controller driver
        return false;
    }

    if (SDL_IsJoystickSteamVirtualGamepad(vendor, product, version)) {
        if (IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDVirtualHIDevice)) != kCFBooleanTrue) {
            // This is a real Xbox 360 controller, adjust the version so it's not detected as a Steam virtual gamepad
            version = 1;
        }
    }

    // get device name
    GetDeviceStringProperty(hidDevice, CFSTR(kIOHIDManufacturerKey), manufacturer_string, sizeof(manufacturer_string));
    GetDeviceStringProperty(hidDevice, CFSTR(kIOHIDProductKey), product_string, sizeof(product_string));
    name = SDL_CreateJoystickName(vendor, product, manufacturer_string, product_string);
    if (name) {
        SDL_strlcpy(pDevice->product, name, sizeof(pDevice->product));
        SDL_free(name);
    }

    Uint16 bus = GetDeviceBus(hidDevice);

    pDevice->vendor_id = (Uint16)vendor;
    pDevice->product_id = (Uint16)product;
    pDevice->guid = SDL_CreateJoystickGUID(bus, (Uint16)vendor, (Uint16)product, (Uint16)version, manufacturer_string, product_string, 0, 0);
    pDevice->steam_virtual_gamepad_slot = GetSteamVirtualGamepadSlot((Uint16)vendor, (Uint16)product, product_string);

    if (vendor == USB_VENDOR_NACON_ALT &&
        product == USB_PRODUCT_NACON_REVOLUTION_X_UNLIMITED_BT) {
        pDevice->nacon_revolution_x_unlimited = true;
    }

    array = IOHIDDeviceCopyMatchingElements(hidDevice, NULL, kIOHIDOptionsTypeNone);
    if (array) {
        AddHIDElements(array, pDevice);
        CFRelease(array);
    }

    return true;
}

static bool JoystickHandledByMFi(IOHIDDeviceRef ioHIDDeviceObject)
{
#ifdef SDL_JOYSTICK_MFI
    extern bool GCController_SupportedHIDDevice(IOHIDDeviceRef device);
    if (!IsControlledBy360ControllerDriver(ioHIDDeviceObject) &&
        GCController_SupportedHIDDevice(ioHIDDeviceObject)) {
        return true;
    }
#endif
    return false;
}

static void JoystickDeviceWasAdded(io_service_t service)
{
    recDevice *device;
    IOHIDDeviceRef ioHIDDeviceObject;
    Uint64 entry_id = GetServiceEntryID(service);

    if (!entry_id) {
        return;
    }

    // A device that matches more than one usage is reported once per matching notification
    for (device = gpDeviceList; device; device = device->pNext) {
        if (!device->removed && device->entry_id == entry_id) {
            return;
        }
    }

    ioHIDDeviceObject = IOHIDDeviceCreate(kCFAllocatorDefault, service);
    if (!ioHIDDeviceObject) {
        return;
    }

    DARWIN_AddKnownDevice(ioHIDDeviceObject, entry_id);

    if (JoystickHandledByMFi(ioHIDDeviceObject)) {
        CFRelease(ioHIDDeviceObject);
        return;
    }

    device = (recDevice *)SDL_calloc(1, sizeof(recDevice));
    if (!device) {
        CFRelease(ioHIDDeviceObject);
        return;
    }
    device->entry_id = entry_id;

    // GetDeviceInfo() retains the device if it's one we care about
    bool supported = GetDeviceInfo(ioHIDDeviceObject, device);
    CFRelease(ioHIDDeviceObject);
    if (!supported) {
        FreeDevice(device);
        return; // not a device we care about, probably.
    }

    if (IOHIDDeviceOpen(device->deviceRef, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        FreeDevice(device);
        return;
    }
    device->opened = true;

    // We have to do some storage of the io_service_t for SDL_OpenHapticFromJoystick
    io_service_t ioservice = IOHIDDeviceGetService(device->deviceRef);
    if ((ioservice) && (FFIsForceFeedback(ioservice) == FF_OK)) {
        device->ffservice = ioservice;
    }

    // Add device to the end of the list
    if (!gpDeviceList) {
        gpDeviceList = device;
    } else {
        recDevice *curdevice;

        curdevice = gpDeviceList;
        while (curdevice->pNext) {
            curdevice = curdevice->pNext;
        }
        curdevice->pNext = device;
    }
}

static void JoystickDevicesWereAdded(void *refcon, io_iterator_t iterator)
{
    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        JoystickDeviceWasAdded(service);
        IOObjectRelease(service);
    }
}

static CFMutableDictionaryRef CreateHIDDeviceMatchDictionary(UInt32 page, UInt32 usage)
{
    CFMutableDictionaryRef matching = IOServiceMatching(kIOHIDDeviceKey);
    if (!matching) {
        return NULL;
    }

    // IOHIDDevice matches these against all of the device's usage pairs, like IOHIDManager does
    CFNumberRef pageNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &page);
    CFNumberRef usageNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usage);
    if (pageNumRef && usageNumRef) {
        CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsagePageKey), pageNumRef);
        CFDictionarySetValue(matching, CFSTR(kIOHIDDeviceUsageKey), usageNumRef);
    } else {
        CFRelease(matching);
        matching = NULL;
    }
    if (pageNumRef) {
        CFRelease(pageNumRef);
    }
    if (usageNumRef) {
        CFRelease(usageNumRef);
    }
    return matching;
}

static void DARWIN_JoystickQuit(void);

static bool CreateNotifications(void)
{
    const UInt32 usages[] = {
        kHIDUsage_GD_Joystick,
        kHIDUsage_GD_GamePad,
        kHIDUsage_GD_MultiAxisController,
    };
    SDL_COMPILE_TIME_ASSERT(usages, SDL_arraysize(usages) == SDL_arraysize(gAddedIterators));

    gNotificationPort = IONotificationPortCreate(kIOMainPortDefault);
    if (!gNotificationPort) {
        return false;
    }

    for (int i = 0; i < SDL_arraysize(usages); ++i) {
        CFMutableDictionaryRef matching = CreateHIDDeviceMatchDictionary(kHIDPage_GenericDesktop, usages[i]);
        if (!matching) {
            return false;
        }

        if (IOServiceAddMatchingNotification(gNotificationPort, kIOFirstMatchNotification, matching, JoystickDevicesWereAdded, NULL, &gAddedIterators[i]) != KERN_SUCCESS) {
            return false;
        }
    }

    CFMutableDictionaryRef matching = IOServiceMatching(kIOHIDDeviceKey);
    if (!matching) {
        return false;
    }

    if (IOServiceAddMatchingNotification(gNotificationPort, kIOTerminatedNotification, matching, JoystickDevicesWereRemoved, NULL, &gRemovedIterator) != KERN_SUCCESS) {
        return false;
    }

    gNotificationMach = IONotificationPortGetMachPort(gNotificationPort);
    return true;
}

static bool DARWIN_JoystickInit(void)
{
    if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_IOKIT, true)) {
        return true;
    }

    if (!CreateNotifications()) {
        DARWIN_JoystickQuit();
        return SDL_SetError("Joystick: Couldn't create IOKit device notifications");
    }

    return true;
}

static SDL_JoystickDevice *DARWIN_CreateJoystickDevice(recDevice *data)
{
    SDL_JoystickDevice *device = SDL_CreateJoystickDevice(&SDL_DARWIN_JoystickDriver, 0);
    if (!device) {
        return NULL;
    }

    // The IOKit device is the unique ID for the device
    device->internal = (SDL_JoystickDeviceData *)data;
    device->unique_id = &device->internal;
    device->unique_id_len = sizeof(device->internal);

    device->name = SDL_strdup(data->product);
    if (!device->name) {
        SDL_DestroyJoystickDevice(device);
        return NULL;
    }
    device->guid = data->guid;
    device->steam_virtual_gamepad_slot = data->steam_virtual_gamepad_slot;

    SDL_AddJoystickDeviceMatchVIDPID(device, data->vendor_id, data->product_id);

    return device;
}

// Free devices that were removed during the last update, their SDL_JoystickDevices
// have been removed and their recDevice is no longer referenced
static void FreeRemovedDevices(void)
{
    recDevice *entry = gpDeviceList;
    while (entry) {
        if (entry->removed) {
            entry = FreeDevice(entry);
        } else {
            entry = entry->pNext;
        }
    }
}

static SDL_JoystickDevice *DARWIN_JoystickQueryDevices(void)
{
    SDL_JoystickDevice *devices = NULL;
    SDL_JoystickDevice *tail = NULL;
    recDevice *entry;

    FreeRemovedDevices();

    if (gNotificationPort) {
        struct
        {
            mach_msg_header_t hdr;
            char payload[4096];
        } msg;

        // Consume any pending notifications, this calls the notification callbacks
        while (mach_msg(&msg.hdr, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg), gNotificationMach, 0, MACH_PORT_NULL) == KERN_SUCCESS) {
            IODispatchCalloutFromMessage(NULL, &msg.hdr, gNotificationPort);
        }

        // Drain the iterators directly as well, this picks up the initial devices and arms the notifications
        JoystickDevicesWereRemoved(NULL, gRemovedIterator);
        for (int i = 0; i < SDL_arraysize(gAddedIterators); ++i) {
            JoystickDevicesWereAdded(NULL, gAddedIterators[i]);
        }
    }

    for (entry = gpDeviceList; entry; entry = entry->pNext) {
        if (!entry->removed) {
            SDL_JoystickDevice *device = DARWIN_CreateJoystickDevice(entry);
            if (device) {
                if (tail) {
                    tail->next = device;
                } else {
                    devices = device;
                }
                tail = device;
            }
        }
    }
    return devices;
}

static bool DARWIN_JoystickPrepareDevice(SDL_JoystickDevice *device)
{
    recDevice *data = (recDevice *)device->internal;

    device->container_id = SDL_GetContainerIDFromHIDDevice(data->deviceRef);

#ifdef SDL_HAPTIC_IOKIT
    if (data->ffservice) {
        MacHaptic_MaybeAddDevice(data->ffservice);
    }
#endif
    return true;
}

static bool DARWIN_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    recDevice *data = (recDevice *)device->internal;

    if (data->removed) {
        return SDL_SetError("Joystick has been disconnected");
    }

    joystick->hwdata = data;
    data->joystick = joystick;

    joystick->naxes = data->axes;
    joystick->nhats = data->hats;
    joystick->nbuttons = data->buttons;

    char serial[256];
    GetDeviceStringProperty(data->deviceRef, CFSTR(kIOHIDSerialNumberKey), serial, sizeof(serial));
    if (*serial) {
        joystick->serial = SDL_strdup(serial);
    }

    if (data->ffservice) {
        SDL_SetBooleanProperty(SDL_GetJoystickProperties(joystick), SDL_PROP_JOYSTICK_CAP_RUMBLE_BOOLEAN, true);
    }

    return true;
}

/*
 * Like strerror but for force feedback errors.
 */
static const char *FFStrError(unsigned int err)
{
    switch (err) {
    case FFERR_DEVICEFULL:
        return "device full";
    // This should be valid, but for some reason isn't defined...
    /* case FFERR_DEVICENOTREG:
        return "device not registered"; */
    case FFERR_DEVICEPAUSED:
        return "device paused";
    case FFERR_DEVICERELEASED:
        return "device released";
    case FFERR_EFFECTPLAYING:
        return "effect playing";
    case FFERR_EFFECTTYPEMISMATCH:
        return "effect type mismatch";
    case FFERR_EFFECTTYPENOTSUPPORTED:
        return "effect type not supported";
    case FFERR_GENERIC:
        return "undetermined error";
    case FFERR_HASEFFECTS:
        return "device has effects";
    case FFERR_INCOMPLETEEFFECT:
        return "incomplete effect";
    case FFERR_INTERNAL:
        return "internal fault";
    case FFERR_INVALIDDOWNLOADID:
        return "invalid download id";
    case FFERR_INVALIDPARAM:
        return "invalid parameter";
    case FFERR_MOREDATA:
        return "more data";
    case FFERR_NOINTERFACE:
        return "interface not supported";
    case FFERR_NOTDOWNLOADED:
        return "effect is not downloaded";
    case FFERR_NOTINITIALIZED:
        return "object has not been initialized";
    case FFERR_OUTOFMEMORY:
        return "out of memory";
    case FFERR_UNPLUGGED:
        return "device is unplugged";
    case FFERR_UNSUPPORTED:
        return "function call unsupported";
    case FFERR_UNSUPPORTEDAXIS:
        return "axis unsupported";

    default:
        return "unknown error";
    }
}

static bool DARWIN_JoystickInitRumble(recDevice *device, Sint16 magnitude)
{
    HRESULT result;

    if (!device->ffdevice) {
        result = FFCreateDevice(device->ffservice, &device->ffdevice);
        if (result != FF_OK) {
            return SDL_SetError("Unable to create force feedback device from service: %s", FFStrError(result));
        }
    }

    // Reset and then enable actuators
    result = FFDeviceSendForceFeedbackCommand(device->ffdevice, FFSFFC_RESET);
    if (result != FF_OK) {
        return SDL_SetError("Unable to reset force feedback device: %s", FFStrError(result));
    }

    result = FFDeviceSendForceFeedbackCommand(device->ffdevice, FFSFFC_SETACTUATORSON);
    if (result != FF_OK) {
        return SDL_SetError("Unable to enable force feedback actuators: %s", FFStrError(result));
    }

    // Create the effect
    device->ffeffect = CreateRumbleEffectData(magnitude);
    if (!device->ffeffect) {
        return false;
    }

    result = FFDeviceCreateEffect(device->ffdevice, kFFEffectType_Sine_ID,
                                  device->ffeffect, &device->ffeffect_ref);
    if (result != FF_OK) {
        return SDL_SetError("Haptic: Unable to create effect: %s", FFStrError(result));
    }
    return true;
}

static bool DARWIN_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble, Uint16 high_frequency_rumble)
{
    HRESULT result;
    recDevice *device = joystick->hwdata;

    // Scale and average the two rumble strengths
    Sint16 magnitude = (Sint16)(((low_frequency_rumble / 2) + (high_frequency_rumble / 2)) / 2);

    if (!device) {
        return SDL_SetError("Rumble failed, device disconnected");
    }

    if (!device->ffservice) {
        return SDL_Unsupported();
    }

    if (device->ff_initialized) {
        FFPERIODIC *periodic = ((FFPERIODIC *)device->ffeffect->lpvTypeSpecificParams);
        periodic->dwMagnitude = CONVERT_MAGNITUDE(magnitude);

        result = FFEffectSetParameters(device->ffeffect_ref, device->ffeffect,
                                       (FFEP_DURATION | FFEP_TYPESPECIFICPARAMS));
        if (result != FF_OK) {
            return SDL_SetError("Unable to update rumble effect: %s", FFStrError(result));
        }
    } else {
        if (!DARWIN_JoystickInitRumble(device, magnitude)) {
            return false;
        }
        device->ff_initialized = true;
    }

    result = FFEffectStart(device->ffeffect_ref, 1, 0);
    if (result != FF_OK) {
        return SDL_SetError("Unable to run the rumble effect: %s", FFStrError(result));
    }
    return true;
}

static void DARWIN_JoystickUpdate(SDL_Joystick *joystick)
{
    recDevice *device = joystick->hwdata;
    recElement *element;
    SInt32 value, range;
    int i, goodRead = false;
    Uint64 timestamp = SDL_GetTicksNS();

    if (!device) {
        return;
    }

    if (device->removed) { // device was unplugged; ignore it.
        if (joystick->hwdata) {
            joystick->hwdata = NULL;
        }
        return;
    }

    element = device->firstAxis;
    i = 0;

    while (element) {
        if (device->nacon_revolution_x_unlimited) {
            goodRead = GetHIDScaledCalibratedState_NACON_Revolution_X_Unlimited(device, element, -32768, 32767, &value);
        } else {
            goodRead = GetHIDScaledCalibratedState(device, element, -32768, 32767, &value);
        }
        if (goodRead) {
            SDL_SendJoystickAxis(timestamp, joystick, i, value);
        }

        element = element->pNext;
        ++i;
    }

    element = device->firstButton;
    i = 0;
    while (element) {
        goodRead = GetHIDElementState(device, element, &value);
        if (goodRead) {
            SDL_SendJoystickButton(timestamp, joystick, i, (value != 0));
        }

        element = element->pNext;
        ++i;
    }

    element = device->firstHat;
    i = 0;

    while (element) {
        Uint8 pos = 0;

        range = (element->max - element->min + 1);
        goodRead = GetHIDElementState(device, element, &value);
        if (goodRead) {
            value -= element->min;
            if (range == 4) { // 4 position hatswitch - scale up value
                value *= 2;
            } else if (range != 8) { // Neither a 4 nor 8 positions - fall back to default position (centered)
                value = -1;
            }
            switch (value) {
            case 0:
                pos = SDL_HAT_UP;
                break;
            case 1:
                pos = SDL_HAT_RIGHTUP;
                break;
            case 2:
                pos = SDL_HAT_RIGHT;
                break;
            case 3:
                pos = SDL_HAT_RIGHTDOWN;
                break;
            case 4:
                pos = SDL_HAT_DOWN;
                break;
            case 5:
                pos = SDL_HAT_LEFTDOWN;
                break;
            case 6:
                pos = SDL_HAT_LEFT;
                break;
            case 7:
                pos = SDL_HAT_LEFTUP;
                break;
            default:
                /* Every other value is mapped to center. We do that because some
                 * joysticks use 8 and some 15 for this value, and apparently
                 * there are even more variants out there - so we try to be generous.
                 */
                pos = SDL_HAT_CENTERED;
                break;
            }

            SDL_SendJoystickHat(timestamp, joystick, i, pos);
        }

        element = element->pNext;
        ++i;
    }
}

static void DARWIN_JoystickClose(SDL_Joystick *joystick)
{
    recDevice *device = joystick->hwdata;
    if (device) {
        device->joystick = NULL;
    }
}

static void DARWIN_JoystickQuit(void)
{
    while (FreeDevice(gpDeviceList)) {
        // spin
    }

    for (int i = 0; i < SDL_arraysize(gAddedIterators); ++i) {
        if (gAddedIterators[i]) {
            IOObjectRelease(gAddedIterators[i]);
            gAddedIterators[i] = IO_OBJECT_NULL;
        }
    }
    if (gRemovedIterator) {
        IOObjectRelease(gRemovedIterator);
        gRemovedIterator = IO_OBJECT_NULL;
    }
    if (gNotificationPort) {
        IONotificationPortDestroy(gNotificationPort);
        gNotificationPort = NULL;
    }
    gNotificationMach = MACH_PORT_NULL;

    DARWIN_DestroyKnownDevices();
}

SDL_JoystickDriver SDL_DARWIN_JoystickDriver = {
    .Init = DARWIN_JoystickInit,
    .QueryDevices = DARWIN_JoystickQueryDevices,
    .PrepareDevice = DARWIN_JoystickPrepareDevice,
    .Open = DARWIN_JoystickOpen,
    .Rumble = DARWIN_JoystickRumble,
    .Update = DARWIN_JoystickUpdate,
    .Close = DARWIN_JoystickClose,
    .Quit = DARWIN_JoystickQuit
};

#endif // SDL_JOYSTICK_IOKIT
