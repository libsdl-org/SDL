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
#include <IOKit/IOKitLib.h>
#include <dispatch/dispatch.h>

#define CONVERT_MAGNITUDE(x) (((x)*10000) / 0x7FFF)

// SDL_JoystickDevice driver data
typedef struct DARWIN_DeviceData
{
    io_service_t service; // retained
    bool has_force_feedback;
    bool nacon_revolution_x_unlimited;
} DARWIN_DeviceData;

// A HID device that has been seen, owned by the joystick device thread. The instance ID is 0 if it's not a joystick we use.
typedef struct DARWIN_KnownDevice
{
    Uint64 entry_id;
    SDL_JoystickID instance_id; // Driver instance ID, not the public joystick ID (see SDL_JoystickDevice)
    io_service_t ffservice; // retained, for removing its haptic device when it's unplugged
    bool seen;
    struct DARWIN_KnownDevice *next;
} DARWIN_KnownDevice;

// A message to the app thread to remove the haptic device of a joystick that was unplugged
typedef struct DARWIN_HapticRemoval
{
    io_service_t service; // retained
} DARWIN_HapticRemoval;

static bool DARWIN_enabled = false;
static bool DARWIN_scanned = false;
static DARWIN_KnownDevice *DARWIN_known_devices = NULL;
static IONotificationPortRef DARWIN_notification_port = NULL;
static dispatch_queue_t DARWIN_notification_queue = NULL;
static io_iterator_t DARWIN_added_iterator = IO_OBJECT_NULL;
static io_iterator_t DARWIN_removed_iterator = IO_OBJECT_NULL;
static SDL_AtomicInt DARWIN_change_count;
static int DARWIN_last_change_count = 0;
static bool DARWIN_retry_needed = false; // A device failed for a reason that might go away

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

static bool IsControlledBy360ControllerDriver(io_service_t service)
{
    return IOObjectConformsTo(service, "Xbox360ControllerClass") ? true : false;
}

static Sint32 GetDeviceIntProperty(IOHIDDeviceRef hidDevice, CFStringRef key)
{
    Sint32 value = 0;
    CFTypeRef refCF = IOHIDDeviceGetProperty(hidDevice, key);
    if (refCF && CFGetTypeID(refCF) == CFNumberGetTypeID()) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &value);
    }
    return value;
}

static void GetDeviceStringProperty(IOHIDDeviceRef hidDevice, CFStringRef key, char *buffer, size_t size)
{
    CFTypeRef refCF = IOHIDDeviceGetProperty(hidDevice, key);
    if (!refCF || CFGetTypeID(refCF) != CFStringGetTypeID() ||
        !CFStringGetCString(refCF, buffer, size, kCFStringEncodingUTF8)) {
        buffer[0] = '\0';
    }
}

static void DARWIN_FreeDeviceData(void *driver_data)
{
    DARWIN_DeviceData *data = (DARWIN_DeviceData *)driver_data;

    IOObjectRelease(data->service);
    SDL_free(data);
}

// Report a HID device if it's a joystick we use, returning its instance ID or 0. Called on the device thread.
static SDL_JoystickID DARWIN_AddDevice(io_service_t service, bool *transient_failure)
{
    IOHIDDeviceRef hidDevice;
    Sint32 usagePage, usage, vendor, product, version;
    char manufacturer_string[256];
    char product_string[256];
    char *name;
    DARWIN_DeviceData *data;
    SDL_JoystickDevice *device = NULL;
    SDL_GUID guid;
    SDL_JoystickID instance_id = 0;

    // This doesn't open the device, it's only used to read its properties
    hidDevice = IOHIDDeviceCreate(kCFAllocatorDefault, service);
    *transient_failure = false;

    if (!hidDevice) {
        *transient_failure = true;
        return 0;
    }

    // Filter device list to non-keyboard/mouse stuff
    usagePage = GetDeviceIntProperty(hidDevice, CFSTR(kIOHIDPrimaryUsagePageKey));
    usage = GetDeviceIntProperty(hidDevice, CFSTR(kIOHIDPrimaryUsageKey));
    if (usagePage != kHIDPage_GenericDesktop ||
        (usage != kHIDUsage_GD_Joystick &&
         usage != kHIDUsage_GD_GamePad &&
         usage != kHIDUsage_GD_MultiAxisController)) {
        goto done;
    }

#ifdef SDL_JOYSTICK_MFI
    {
        extern bool IOS_SupportedHIDDevice(IOHIDDeviceRef device);
        if (!IsControlledBy360ControllerDriver(service) && IOS_SupportedHIDDevice(hidDevice)) {
            // The GameController framework handles this one
            goto done;
        }
    }
#endif

    vendor = GetDeviceIntProperty(hidDevice, CFSTR(kIOHIDVendorIDKey));
    product = GetDeviceIntProperty(hidDevice, CFSTR(kIOHIDProductIDKey));
    version = GetDeviceIntProperty(hidDevice, CFSTR(kIOHIDVersionNumberKey));

    if (!IsControlledBy360ControllerDriver(service) && SDL_IsJoystickXboxOne(vendor, product)) {
        // We can't actually use this API for Xbox controllers without the 360Controller driver
        goto done;
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
    if (!name) {
        *transient_failure = true;
        goto done;
    }

    if (SDL_ShouldIgnoreJoystick(vendor, product, version, name)) {
        SDL_free(name);
        goto done;
    }

    data = (DARWIN_DeviceData *)SDL_calloc(1, sizeof(*data));
    if (data) {
        device = SDL_CreateJoystickDevice(&SDL_DARWIN_JoystickDriver);
    }
    if (!device) {
        SDL_free(data);
        SDL_free(name);
        *transient_failure = true;
        goto done;
    }

    IOObjectRetain(service);
    data->service = service;
    data->has_force_feedback = (FFIsForceFeedback(service) == FF_OK);
    if (vendor == USB_VENDOR_NACON_ALT &&
        product == USB_PRODUCT_NACON_REVOLUTION_X_UNLIMITED_BT) {
        data->nacon_revolution_x_unlimited = true;
    }
    device->driver_data = data;

    guid = SDL_CreateJoystickGUID(SDL_HARDWARE_BUS_USB, (Uint16)vendor, (Uint16)product, (Uint16)version, manufacturer_string, product_string, 0, 0);
    SDL_SetJoystickDeviceDescription(device, name, NULL, guid, GetSteamVirtualGamepadSlot((Uint16)vendor, (Uint16)product, product_string));
    device->vendor_id = (Uint16)vendor;
    device->product_id = (Uint16)product;
    device->type = SDL_GetGamepadTypeFromVIDPID((Uint16)vendor, (Uint16)product, name, false);
    SDL_free(name);

    instance_id = device->driver_instance_id;
    SDL_ReportJoystickDevice(device);

done:
    CFRelease(hidDevice);
    return instance_id;
}

// Find joysticks that were added or removed since the last scan. Called on the device thread.
static void DARWIN_ScanDevices(void)
{
    io_iterator_t iter = IO_OBJECT_NULL;
    io_service_t service;
    DARWIN_KnownDevice *known, *prev, *next;

    for (known = DARWIN_known_devices; known; known = known->next) {
        known->seen = false;
    }

    if (IOServiceGetMatchingServices(kIOMainPortDefault, IOServiceMatching(kIOHIDDeviceKey), &iter) != kIOReturnSuccess) {
        return;
    }
    // IOServiceGetMatchingServices consumes the dictionary

    while ((service = IOIteratorNext(iter)) != IO_OBJECT_NULL) {
        Uint64 entry_id = 0;

        if (SDL_JoystickDiscoveryQuitting()) {
            IOObjectRelease(service);
            IOObjectRelease(iter);
            return;
        }

        if (IORegistryEntryGetRegistryEntryID(service, &entry_id) == KERN_SUCCESS) {
            for (known = DARWIN_known_devices; known; known = known->next) {
                if (known->entry_id == entry_id) {
                    break;
                }
            }
            if (!known) {
                // Remember devices we don't use too, so they're only checked once
                known = (DARWIN_KnownDevice *)SDL_calloc(1, sizeof(*known));
                if (known) {
                    bool transient_failure = false;

                    known->entry_id = entry_id;
                    known->instance_id = DARWIN_AddDevice(service, &transient_failure);
                    if (transient_failure) {
                        // Try again soon
                        DARWIN_retry_needed = true;
                        SDL_free(known);
                        IOObjectRelease(service);
                        continue;
                    }
#ifdef SDL_HAPTIC_IOKIT
                    if (known->instance_id && FFIsForceFeedback(service) == FF_OK) {
                        IOObjectRetain(service);
                        known->ffservice = service;
                    }
#endif
                    known->next = DARWIN_known_devices;
                    DARWIN_known_devices = known;
                }
            }
            if (known) {
                known->seen = true;
            }
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iter);

    prev = NULL;
    for (known = DARWIN_known_devices; known; known = next) {
        next = known->next;
        if (known->seen) {
            prev = known;
            continue;
        }

        if (known->instance_id) {
            SDL_ReportJoystickRemoved(known->instance_id);
        }
        if (known->ffservice) {
            // The haptic list is only used on the app thread, so its device is removed there
            DARWIN_HapticRemoval *message = (DARWIN_HapticRemoval *)SDL_malloc(sizeof(*message));
            if (message) {
                message->service = known->ffservice;
                SDL_PostJoystickDriverMessage(&SDL_DARWIN_JoystickDriver, message);
            } else {
                IOObjectRelease(known->ffservice);
            }
        }
        if (prev) {
            prev->next = next;
        } else {
            DARWIN_known_devices = next;
        }
        SDL_free(known);
    }
}

static void DARWIN_DeviceChangedCallback(void *context, io_iterator_t portIterator)
{
    // Must drain the iterator, or we won't receive new notifications
    io_object_t entry;
    while ((entry = IOIteratorNext(portIterator)) != IO_OBJECT_NULL) {
        IOObjectRelease(entry);
    }
    SDL_AddAtomicInt(&DARWIN_change_count, 1);
    SDL_WakeJoystickDeviceThread();
}

static bool DARWIN_AddNotification(const char *type, io_iterator_t *iter)
{
    io_object_t entry;

    if (IOServiceAddMatchingNotification(DARWIN_notification_port, type, IOServiceMatching(kIOHIDDeviceKey),
                                         DARWIN_DeviceChangedCallback, NULL, iter) != kIOReturnSuccess) {
        return false;
    }

    // Must drain the existing iterator, or we won't receive new notifications
    while ((entry = IOIteratorNext(*iter)) != IO_OBJECT_NULL) {
        IOObjectRelease(entry);
    }
    return true;
}

// Called on the notification queue, so it can't run at the same time as the callback
static void DARWIN_DestroyNotificationPort(void *unused)
{
    if (DARWIN_added_iterator) {
        IOObjectRelease(DARWIN_added_iterator);
        DARWIN_added_iterator = IO_OBJECT_NULL;
    }
    if (DARWIN_removed_iterator) {
        IOObjectRelease(DARWIN_removed_iterator);
        DARWIN_removed_iterator = IO_OBJECT_NULL;
    }
    IONotificationPortDestroy(DARWIN_notification_port);
    DARWIN_notification_port = NULL;
}

static void DARWIN_QuitNotifications(void)
{
    if (DARWIN_notification_queue) {
        if (DARWIN_notification_port) {
            dispatch_sync_f(DARWIN_notification_queue, NULL, DARWIN_DestroyNotificationPort);
        }
        dispatch_release(DARWIN_notification_queue);
        DARWIN_notification_queue = NULL;
    }
}

// Device changes wake the device thread. Notifications are delivered on a private queue, so they don't
// depend on any thread's run loop.
static void DARWIN_InitNotifications(void)
{
    DARWIN_notification_port = IONotificationPortCreate(kIOMainPortDefault);
    if (!DARWIN_notification_port) {
        return;
    }

    DARWIN_notification_queue = dispatch_queue_create("SDL joystick discovery", DISPATCH_QUEUE_SERIAL);
    if (!DARWIN_notification_queue) {
        IONotificationPortDestroy(DARWIN_notification_port);
        DARWIN_notification_port = NULL;
        return;
    }
    IONotificationPortSetDispatchQueue(DARWIN_notification_port, DARWIN_notification_queue);

    if (!DARWIN_AddNotification(kIOFirstMatchNotification, &DARWIN_added_iterator) ||
        !DARWIN_AddNotification(kIOTerminatedNotification, &DARWIN_removed_iterator)) {
        DARWIN_QuitNotifications();
    }
}

static bool DARWIN_JoystickInit(void)
{
    if (!SDL_GetHintBoolean(SDL_HINT_JOYSTICK_IOKIT, true)) {
        return true;
    }

    DARWIN_InitNotifications();

    DARWIN_enabled = true;
    DARWIN_scanned = false;
    return true;
}

// Called on the joystick device thread
static Sint32 DARWIN_JoystickDiscover(void)
{
    // Without device notifications, look for joysticks every 3 seconds
    const Sint32 DARWIN_POLL_INTERVAL_MS = 3000;
    // Devices that failed for a reason that might go away are tried again after a second
    const Sint32 DARWIN_RETRY_INTERVAL_MS = 1000;
    int change_count;

    if (!DARWIN_enabled) {
        return -1;
    }

    change_count = SDL_GetAtomicInt(&DARWIN_change_count);
    if (DARWIN_scanned && DARWIN_notification_port && change_count == DARWIN_last_change_count && !DARWIN_retry_needed) {
        return -1;
    }
    DARWIN_last_change_count = change_count;
    DARWIN_scanned = true;
    DARWIN_retry_needed = false;

    DARWIN_ScanDevices();

    if (DARWIN_retry_needed) {
        return DARWIN_RETRY_INTERVAL_MS;
    }
    return DARWIN_notification_port ? -1 : DARWIN_POLL_INTERVAL_MS;
}

static bool DARWIN_JoystickOpen(SDL_Joystick *joystick, SDL_JoystickDevice *device)
{
    const DARWIN_DeviceData *data = (const DARWIN_DeviceData *)device->driver_data;
    recDevice *hwdata;
    CFArrayRef array;

    hwdata = (recDevice *)SDL_calloc(1, sizeof(*hwdata));
    if (!hwdata) {
        return false;
    }

    hwdata->deviceRef = IOHIDDeviceCreate(kCFAllocatorDefault, data->service);
    if (!hwdata->deviceRef) {
        SDL_free(hwdata);
        return SDL_SetError("Couldn't create HID device");
    }
    if (IOHIDDeviceOpen(hwdata->deviceRef, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        CFRelease(hwdata->deviceRef);
        SDL_free(hwdata);
        return SDL_SetError("Couldn't open HID device");
    }

    array = IOHIDDeviceCopyMatchingElements(hwdata->deviceRef, NULL, kIOHIDOptionsTypeNone);
    if (array) {
        AddHIDElements(array, hwdata);
        CFRelease(array);
    }

    // The haptic subsystem uses this for SDL_OpenHapticFromJoystick()
    if (data->has_force_feedback) {
        IOObjectRetain(data->service);
        hwdata->ffservice = data->service;
    }
    hwdata->nacon_revolution_x_unlimited = data->nacon_revolution_x_unlimited;

    joystick->hwdata = hwdata;
    joystick->naxes = hwdata->axes;
    joystick->nhats = hwdata->hats;
    joystick->nbuttons = hwdata->buttons;

    if (hwdata->ffservice) {
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

    if (!device) {
        return;
    }

    if (device->ffeffect_ref) {
        FFDeviceReleaseEffect(device->ffdevice, device->ffeffect_ref);
    }
    if (device->ffeffect) {
        FreeRumbleEffectData(device->ffeffect);
    }
    if (device->ffdevice) {
        FFReleaseDevice(device->ffdevice);
    }
    if (device->ffservice) {
        IOObjectRelease(device->ffservice);
    }
    if (device->deviceRef) {
        IOHIDDeviceClose(device->deviceRef, kIOHIDOptionsTypeNone);
        CFRelease(device->deviceRef);
    }
    FreeElementList(device->firstAxis);
    FreeElementList(device->firstButton);
    FreeElementList(device->firstHat);
    SDL_free(device);
    joystick->hwdata = NULL;
}

static void DARWIN_JoystickQuit(void)
{
    DARWIN_KnownDevice *known, *next;

    // The device thread has stopped, so nothing else uses these
    DARWIN_QuitNotifications();

    for (known = DARWIN_known_devices; known; known = next) {
        next = known->next;
        if (known->ffservice) {
            IOObjectRelease(known->ffservice);
        }
        SDL_free(known);
    }
    DARWIN_known_devices = NULL;
    DARWIN_enabled = false;
}

static void DARWIN_JoystickHandleMessage(void *data)
{
    DARWIN_HapticRemoval *message = (DARWIN_HapticRemoval *)data;

#ifdef SDL_HAPTIC_IOKIT
    MacHaptic_MaybeRemoveDevice(message->service);
#endif
    IOObjectRelease(message->service);
    SDL_free(message);
}

static void DARWIN_JoystickFreeMessage(void *data)
{
    DARWIN_HapticRemoval *message = (DARWIN_HapticRemoval *)data;

    IOObjectRelease(message->service);
    SDL_free(message);
}

static bool DARWIN_JoystickGetGamepadMapping(SDL_JoystickDevice *device, SDL_GamepadMapping *out)
{
    return false;
}

static void DARWIN_JoystickDevicePublished(SDL_JoystickDevice *device, bool published)
{
#ifdef SDL_HAPTIC_IOKIT
    const DARWIN_DeviceData *data = (const DARWIN_DeviceData *)device->driver_data;

    // Add the haptic device when the joystick is published, like the HID manager callback used to. It's
    // removed when the joystick is unplugged, not when it's unpublished, because the haptic subsystem lists
    // force feedback devices itself too.
    if (published && data->has_force_feedback) {
        MacHaptic_MaybeAddDevice(data->service);
    }
#endif
}

SDL_JoystickDriver SDL_DARWIN_JoystickDriver = {
    .Init = DARWIN_JoystickInit,
    .Discover = DARWIN_JoystickDiscover,
    .HandleMessage = DARWIN_JoystickHandleMessage,
    .FreeMessage = DARWIN_JoystickFreeMessage,
    .DevicePublished = DARWIN_JoystickDevicePublished,
    .FreeDeviceData = DARWIN_FreeDeviceData,
    .GetGamepadMapping = DARWIN_JoystickGetGamepadMapping,
    .Open = DARWIN_JoystickOpen,
    .Rumble = DARWIN_JoystickRumble,
    .Update = DARWIN_JoystickUpdate,
    .Close = DARWIN_JoystickClose,
    .Quit = DARWIN_JoystickQuit,
};

#endif // SDL_JOYSTICK_IOKIT
