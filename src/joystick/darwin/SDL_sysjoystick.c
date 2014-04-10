/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2014 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_JOYSTICK_IOKIT

#include <IOKit/hid/IOHIDLib.h>

/* For force feedback testing. */
#include <ForceFeedback/ForceFeedback.h>
#include <ForceFeedback/ForceFeedbackConstants.h>

#include "SDL_joystick.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"
#include "SDL_sysjoystick_c.h"
#include "SDL_events.h"
#include "../../haptic/darwin/SDL_syshaptic_c.h"    /* For haptic hot plugging */
#if !SDL_EVENTS_DISABLED
#include "../../events/SDL_events_c.h"
#endif

/* The base object of the HID Manager API */
static IOHIDManagerRef hidman = NULL;

/* Linked list of all available devices */
static recDevice *gpDeviceList = NULL;

/* if SDL_TRUE then a device was added since the last update call */
static SDL_bool s_bDeviceAdded = SDL_FALSE;
static SDL_bool s_bDeviceRemoved = SDL_FALSE;

/* static incrementing counter for new joystick devices seen on the system. Devices should start with index 0 */
static int s_joystick_instance_id = -1;


static void
FreeElementList(recElement *pElement)
{
    while (pElement) {
        recElement *pElementNext = pElement->pNext;
        SDL_free(pElement);
        pElement = pElementNext;
    }
}

static recDevice *
FreeDevice(recDevice *removeDevice)
{
    recDevice *pDeviceNext = NULL;
    if (removeDevice) {
        /* save next device prior to disposing of this device */
        pDeviceNext = removeDevice->pNext;

        if ( gpDeviceList == removeDevice ) {
            gpDeviceList = pDeviceNext;
        } else {
            recDevice *device = gpDeviceList;
            while (device->pNext != removeDevice) {
                device = device->pNext;
            }
            device->pNext = pDeviceNext;
        }
        removeDevice->pNext = NULL;

        /* free element lists */
        FreeElementList(removeDevice->firstAxis);
        FreeElementList(removeDevice->firstButton);
        FreeElementList(removeDevice->firstHat);

        SDL_free(removeDevice);
    }
    return pDeviceNext;
}

static SInt32
GetHIDElementState(recDevice *pDevice, recElement *pElement)
{
    SInt32 value = 0;

    if (pDevice && pElement) {
        IOHIDValueRef valueRef;
        if (IOHIDDeviceGetValue(pDevice->deviceRef, pElement->elementRef, &valueRef) == kIOReturnSuccess) {
            value = (SInt32) IOHIDValueGetIntegerValue(valueRef);

            /* record min and max for auto calibration */
            if (value < pElement->minReport) {
                pElement->minReport = value;
            }
            if (value > pElement->maxReport) {
                pElement->maxReport = value;
            }
        }
    }

    return value;
}

static SInt32
GetHIDScaledCalibratedState(recDevice * pDevice, recElement * pElement, SInt32 min, SInt32 max)
{
    const float deviceScale = max - min;
    const float readScale = pElement->maxReport - pElement->minReport;
    const SInt32 value = GetHIDElementState(pDevice, pElement);
    if (readScale == 0) {
        return value;           /* no scaling at all */
    }
    return ((value - pElement->minReport) * deviceScale / readScale) + min;
}


static void
JoystickDeviceWasRemovedCallback(void *ctx, IOReturn result, void *sender)
{
    recDevice *device = (recDevice *) ctx;
    device->removed = 1;
#if SDL_HAPTIC_IOKIT
    MacHaptic_MaybeRemoveDevice(device->ffservice);
#endif
    s_bDeviceRemoved = SDL_TRUE;
}


static void AddHIDElement(const void *value, void *parameter);

/* Call AddHIDElement() on all elements in an array of IOHIDElementRefs */
static void
AddHIDElements(CFArrayRef array, recDevice *pDevice)
{
    const CFRange range = { 0, CFArrayGetCount(array) };
    CFArrayApplyFunction(array, range, AddHIDElement, pDevice);
}

static SDL_bool
ElementAlreadyAdded(const IOHIDElementCookie cookie, const recElement *listitem) {
    while (listitem) {
        if (listitem->cookie == cookie) {
            return SDL_TRUE;
        }
        listitem = listitem->pNext;
    }
    return SDL_FALSE;
}

/* See if we care about this HID element, and if so, note it in our recDevice. */
static void
AddHIDElement(const void *value, void *parameter)
{
    recDevice *pDevice = (recDevice *) parameter;
    IOHIDElementRef refElement = (IOHIDElementRef) value;
    const CFTypeID elementTypeID = refElement ? CFGetTypeID(refElement) : 0;

    if (refElement && (elementTypeID == IOHIDElementGetTypeID())) {
        const IOHIDElementCookie cookie = IOHIDElementGetCookie(refElement);
        const uint32_t usagePage = IOHIDElementGetUsagePage(refElement);
        const uint32_t usage = IOHIDElementGetUsage(refElement);
        recElement *element = NULL;
        recElement **headElement = NULL;

        /* look at types of interest */
        switch (IOHIDElementGetType(refElement)) {
            case kIOHIDElementTypeInput_Misc:
            case kIOHIDElementTypeInput_Button:
            case kIOHIDElementTypeInput_Axis: {
                switch (usagePage) {    /* only interested in kHIDPage_GenericDesktop and kHIDPage_Button */
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
                                    element = (recElement *) SDL_calloc(1, sizeof (recElement));
                                    if (element) {
                                        pDevice->axes++;
                                        headElement = &(pDevice->firstAxis);
                                    }
                                }
                                break;

                            case kHIDUsage_GD_Hatswitch:
                                if (!ElementAlreadyAdded(cookie, pDevice->firstHat)) {
                                    element = (recElement *) SDL_calloc(1, sizeof (recElement));
                                    if (element) {
                                        pDevice->hats++;
                                        headElement = &(pDevice->firstHat);
                                    }
                                }
                                break;
                        }
                        break;

                    case kHIDPage_Simulation:
                        switch (usage) {
                            case kHIDUsage_Sim_Rudder:
                            case kHIDUsage_Sim_Throttle:
                                if (!ElementAlreadyAdded(cookie, pDevice->firstAxis)) {
                                    element = (recElement *) SDL_calloc(1, sizeof (recElement));
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
                        if (!ElementAlreadyAdded(cookie, pDevice->firstButton)) {
                            element = (recElement *) SDL_calloc(1, sizeof (recElement));
                            if (element) {
                                pDevice->buttons++;
                                headElement = &(pDevice->firstButton);
                            }
                        }
                        break;

                    default:
                        break;
                }
            }
            break;

            case kIOHIDElementTypeCollection: {
                CFArrayRef array = IOHIDElementGetChildren(refElement);
                if (array) {
                    AddHIDElements(array, pDevice);
                }
            }
            break;

            default:
                break;
        }

        if (element && headElement) {       /* add to list */
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

            element->minReport = element->min = (SInt32) IOHIDElementGetLogicalMin(refElement);
            element->maxReport = element->max = (SInt32) IOHIDElementGetLogicalMax(refElement);
            element->cookie = IOHIDElementGetCookie(refElement);

            pDevice->elements++;
        }
    }
}

static SDL_bool
GetDeviceInfo(IOHIDDeviceRef hidDevice, recDevice *pDevice)
{
    Uint32 *guid32 = NULL;
    CFTypeRef refCF = NULL;
    CFArrayRef array = NULL;

    /* get usage page and usage */
    refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDPrimaryUsagePageKey));
    if (refCF) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &pDevice->usagePage);
    }
    if (pDevice->usagePage != kHIDPage_GenericDesktop) {
        return SDL_FALSE; /* Filter device list to non-keyboard/mouse stuff */
    }

    refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDPrimaryUsageKey));
    if (refCF) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &pDevice->usage);
    }

    if ((pDevice->usage != kHIDUsage_GD_Joystick &&
         pDevice->usage != kHIDUsage_GD_GamePad &&
         pDevice->usage != kHIDUsage_GD_MultiAxisController)) {
        return SDL_FALSE; /* Filter device list to non-keyboard/mouse stuff */
    }

    pDevice->deviceRef = hidDevice;

    /* get device name */
    refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDProductKey));
    if (!refCF) {
        /* Maybe we can't get "AwesomeJoystick2000", but we can get "Logitech"? */
        refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDManufacturerKey));
    }
    if ((!refCF) || (!CFStringGetCString(refCF, pDevice->product, sizeof (pDevice->product), kCFStringEncodingUTF8))) {
        SDL_strlcpy(pDevice->product, "Unidentified joystick", sizeof (pDevice->product));
    }

    refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDVendorIDKey));
    if (refCF) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &pDevice->guid.data[0]);
    }

    refCF = IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDProductIDKey));
    if (refCF) {
        CFNumberGetValue(refCF, kCFNumberSInt32Type, &pDevice->guid.data[8]);
    }

    /* Check to make sure we have a vendor and product ID
       If we don't, use the same algorithm as the Linux code for Bluetooth devices */
    guid32 = (Uint32*)pDevice->guid.data;
    if (!guid32[0] && !guid32[1]) {
        /* If we don't have a vendor and product ID this is probably a Bluetooth device */
        const Uint16 BUS_BLUETOOTH = 0x05;
        Uint16 *guid16 = (Uint16 *)guid32;
        *guid16++ = BUS_BLUETOOTH;
        *guid16++ = 0;
        SDL_strlcpy((char*)guid16, pDevice->product, sizeof(pDevice->guid.data) - 4);
    }

    array = IOHIDDeviceCopyMatchingElements(hidDevice, NULL, kIOHIDOptionsTypeNone);
    if (array) {
        AddHIDElements(array, pDevice);
        CFRelease(array);
    }

    return SDL_TRUE;
}


static void
JoystickDeviceWasAddedCallback(void *ctx, IOReturn res, void *sender, IOHIDDeviceRef ioHIDDeviceObject)
{
    if (res != kIOReturnSuccess) {
        return;
    }

    recDevice *device = (recDevice *) SDL_calloc(1, sizeof(recDevice));

    if (!device) {
        SDL_OutOfMemory();
        return;
    }

    if (!GetDeviceInfo(ioHIDDeviceObject, device)) {
        SDL_free(device);
        return;   /* not a device we care about, probably. */
    }

    /* Get notified when this device is disconnected. */
    IOHIDDeviceRegisterRemovalCallback(ioHIDDeviceObject, JoystickDeviceWasRemovedCallback, device);
    IOHIDDeviceScheduleWithRunLoop(ioHIDDeviceObject, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);

    /* Allocate an instance ID for this device */
    device->instance_id = ++s_joystick_instance_id;

    /* We have to do some storage of the io_service_t for SDL_HapticOpenFromJoystick */
    if (IOHIDDeviceGetService != NULL) {  /* weak reference: available in 10.6 and later. */
        const io_service_t ioservice = IOHIDDeviceGetService(ioHIDDeviceObject);
        if ((ioservice) && (FFIsForceFeedback(ioservice) == FF_OK)) {
            device->ffservice = ioservice;
#if SDL_HAPTIC_IOKIT
            MacHaptic_MaybeAddDevice(ioservice);
#endif
        }
    }

    device->send_open_event = 1;
    s_bDeviceAdded = SDL_TRUE;

    /* Add device to the end of the list */
    if ( !gpDeviceList )
    {
        gpDeviceList = device;
    }
    else
    {
        recDevice *curdevice;

        curdevice = gpDeviceList;
        while ( curdevice->pNext )
        {
            curdevice = curdevice->pNext;
        }
        curdevice->pNext = device;
    }
}

static SDL_bool
ConfigHIDManager(CFArrayRef matchingArray)
{
    CFRunLoopRef runloop = CFRunLoopGetCurrent();

    /* Run in a custom RunLoop mode just while initializing,
       so we can detect sticks without messing with everything else. */
    CFStringRef tempRunLoopMode = CFSTR("SDLJoystickInit");

    if (IOHIDManagerOpen(hidman, kIOHIDOptionsTypeNone) != kIOReturnSuccess) {
        return SDL_FALSE;
    }

    IOHIDManagerRegisterDeviceMatchingCallback(hidman, JoystickDeviceWasAddedCallback, NULL);
    IOHIDManagerScheduleWithRunLoop(hidman, runloop, tempRunLoopMode);
    IOHIDManagerSetDeviceMatchingMultiple(hidman, matchingArray);

    while (CFRunLoopRunInMode(tempRunLoopMode,0,TRUE)==kCFRunLoopRunHandledSource) {
        /* no-op. Callback fires once per existing device. */
    }

    /* Put this in the normal RunLoop mode now, for future hotplug events. */
    IOHIDManagerUnscheduleFromRunLoop(hidman, runloop, tempRunLoopMode);
    IOHIDManagerScheduleWithRunLoop(hidman, runloop, kCFRunLoopDefaultMode);

    return SDL_TRUE;  /* good to go. */
}


static CFDictionaryRef
CreateHIDDeviceMatchDictionary(const UInt32 page, const UInt32 usage, int *okay)
{
    CFDictionaryRef retval = NULL;
    CFNumberRef pageNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &page);
    CFNumberRef usageNumRef = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &usage);
    const void *keys[2] = { (void *) CFSTR(kIOHIDDeviceUsagePageKey), (void *) CFSTR(kIOHIDDeviceUsageKey) };
    const void *vals[2] = { (void *) pageNumRef, (void *) usageNumRef };

    if (pageNumRef && usageNumRef) {
        retval = CFDictionaryCreate(kCFAllocatorDefault, keys, vals, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    }

    if (pageNumRef) {
        CFRelease(pageNumRef);
    }
    if (usageNumRef) {
        CFRelease(usageNumRef);
    }

    if (!retval) {
        *okay = 0;
    }

    return retval;
}

static SDL_bool
CreateHIDManager(void)
{
    SDL_bool retval = SDL_FALSE;
    int okay = 1;
    const void *vals[] = {
        (void *) CreateHIDDeviceMatchDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_Joystick, &okay),
        (void *) CreateHIDDeviceMatchDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_GamePad, &okay),
        (void *) CreateHIDDeviceMatchDictionary(kHIDPage_GenericDesktop, kHIDUsage_GD_MultiAxisController, &okay),
    };
    const size_t numElements = SDL_arraysize(vals);
    CFArrayRef array = okay ? CFArrayCreate(kCFAllocatorDefault, vals, numElements, &kCFTypeArrayCallBacks) : NULL;
    size_t i;

    for (i = 0; i < numElements; i++) {
        if (vals[i]) {
            CFRelease((CFTypeRef) vals[i]);
        }
    }

    if (array) {
        hidman = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
        if (hidman != NULL) {
            retval = ConfigHIDManager(array);
        }
        CFRelease(array);
    }

    return retval;
}


/* Function to scan the system for joysticks.
 * Joystick 0 should be the system default joystick.
 * This function should return the number of available joysticks, or -1
 * on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
    if (gpDeviceList) {
        return SDL_SetError("Joystick: Device list already inited.");
    }

    if (!CreateHIDManager()) {
        return SDL_SetError("Joystick: Couldn't initialize HID Manager");
    }

    return SDL_SYS_NumJoysticks();
}

/* Function to return the number of joystick devices plugged in right now */
int
SDL_SYS_NumJoysticks()
{
    recDevice *device = gpDeviceList;
    int nJoySticks = 0;

    while (device) {
        if (!device->removed) {
            nJoySticks++;
        }
        device = device->pNext;
    }

    return nJoySticks;
}

/* Function to cause any queued joystick insertions to be processed
 */
void
SDL_SYS_JoystickDetect()
{
    if (s_bDeviceAdded || s_bDeviceRemoved) {
        recDevice *device = gpDeviceList;
        s_bDeviceAdded = SDL_FALSE;
        s_bDeviceRemoved = SDL_FALSE;
        int device_index = 0;
        /* send notifications */
        while (device) {
            if (device->send_open_event) {
                device->send_open_event = 0;
/* !!! FIXME: why isn't there an SDL_PrivateJoyDeviceAdded()? */
#if !SDL_EVENTS_DISABLED
                SDL_Event event;
                event.type = SDL_JOYDEVICEADDED;

                if (SDL_GetEventState(event.type) == SDL_ENABLE) {
                    event.jdevice.which = device_index;
                    if ((SDL_EventOK == NULL)
                        || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
                        SDL_PushEvent(&event);
                    }
                }
#endif /* !SDL_EVENTS_DISABLED */

            }

            if (device->removed) {
                const int instance_id = device->instance_id;
                device = FreeDevice(device);

/* !!! FIXME: why isn't there an SDL_PrivateJoyDeviceRemoved()? */
#if !SDL_EVENTS_DISABLED
                SDL_Event event;
                event.type = SDL_JOYDEVICEREMOVED;

                if (SDL_GetEventState(event.type) == SDL_ENABLE) {
                    event.jdevice.which = instance_id;
                    if ((SDL_EventOK == NULL)
                        || (*SDL_EventOK) (SDL_EventOKParam, &event)) {
                        SDL_PushEvent(&event);
                    }
                }
#endif /* !SDL_EVENTS_DISABLED */

            } else {
                device = device->pNext;
                device_index++;
            }
        }
    }
}

SDL_bool
SDL_SYS_JoystickNeedsPolling()
{
    return s_bDeviceAdded || s_bDeviceRemoved;
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    recDevice *device = gpDeviceList;

    while (device_index-- > 0) {
        device = device->pNext;
    }

    return device->product;
}

/* Function to return the instance id of the joystick at device_index
 */
SDL_JoystickID
SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
    recDevice *device = gpDeviceList;
    int index;

    for (index = device_index; index > 0; index--) {
        device = device->pNext;
    }

    return device->instance_id;
}

/* Function to open a joystick for use.
 * The joystick to open is specified by the index field of the joystick.
 * This should fill the nbuttons and naxes fields of the joystick structure.
 * It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    recDevice *device = gpDeviceList;
    int index;

    for (index = device_index; index > 0; index--) {
        device = device->pNext;
    }

    joystick->instance_id = device->instance_id;
    joystick->hwdata = device;
    joystick->name = device->product;

    joystick->naxes = device->axes;
    joystick->nhats = device->hats;
    joystick->nballs = 0;
    joystick->nbuttons = device->buttons;
    return 0;
}

/* Function to query if the joystick is currently attached
 *   It returns 1 if attached, 0 otherwise.
 */
SDL_bool
SDL_SYS_JoystickAttached(SDL_Joystick * joystick)
{
    recDevice *device = gpDeviceList;

    while (device) {
        if (joystick->instance_id == device->instance_id) {
            return SDL_TRUE;
        }
        device = device->pNext;
    }

    return SDL_FALSE;
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
    recDevice *device = joystick->hwdata;
    recElement *element;
    SInt32 value, range;
    int i;

    if (!device) {
        return;
    }

    if (device->removed) {      /* device was unplugged; ignore it. */
        joystick->closed = 1;
        joystick->uncentered = 1;
        joystick->hwdata = NULL;
        return;
    }

    element = device->firstAxis;
    i = 0;
    while (element) {
        value = GetHIDScaledCalibratedState(device, element, -32768, 32767);
        if (value != joystick->axes[i]) {
            SDL_PrivateJoystickAxis(joystick, i, value);
        }
        element = element->pNext;
        ++i;
    }

    element = device->firstButton;
    i = 0;
    while (element) {
        value = GetHIDElementState(device, element);
        if (value > 1) {          /* handle pressure-sensitive buttons */
            value = 1;
        }
        if (value != joystick->buttons[i]) {
            SDL_PrivateJoystickButton(joystick, i, value);
        }
        element = element->pNext;
        ++i;
    }

    element = device->firstHat;
    i = 0;
    while (element) {
        Uint8 pos = 0;

        range = (element->max - element->min + 1);
        value = GetHIDElementState(device, element) - element->min;
        if (range == 4) {         /* 4 position hatswitch - scale up value */
            value *= 2;
        } else if (range != 8) {    /* Neither a 4 nor 8 positions - fall back to default position (centered) */
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

        if (pos != joystick->hats[i]) {
            SDL_PrivateJoystickHat(joystick, i, pos);
        }

        element = element->pNext;
        ++i;
    }
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
    joystick->closed = 1;
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
    while (FreeDevice(gpDeviceList)) {
        /* spin */
    }

    if (hidman) {
        IOHIDManagerClose(hidman, kIOHIDOptionsTypeNone);
        CFRelease(hidman);
        hidman = NULL;
    }

    s_bDeviceAdded = s_bDeviceRemoved = SDL_FALSE;
}


SDL_JoystickGUID SDL_SYS_JoystickGetDeviceGUID( int device_index )
{
    recDevice *device = gpDeviceList;
    int index;

    for (index = device_index; index > 0; index--) {
        device = device->pNext;
    }

    return device->guid;
}

SDL_JoystickGUID SDL_SYS_JoystickGetGUID(SDL_Joystick *joystick)
{
    return joystick->hwdata->guid;
}

#endif /* SDL_JOYSTICK_IOKIT */

/* vi: set ts=4 sw=4 expandtab: */
