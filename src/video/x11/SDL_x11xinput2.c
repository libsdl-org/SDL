/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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

#ifdef SDL_VIDEO_DRIVER_X11

#include "SDL_x11pen.h"
#include "SDL_x11video.h"
#include "SDL_x11xinput2.h"
#include "../../events/SDL_events_c.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_pen_c.h"
#include "../../events/SDL_touch_c.h"

#define MAX_AXIS 16

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
static int xinput2_initialized = 0;

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
static int xinput2_multitouch_supported = 0;
#endif

/* Opcode returned X11_XQueryExtension
 * It will be used in event processing
 * to know that the event came from
 * this extension */
static int xinput2_opcode;

static void parse_valuators(const double *input_values, const unsigned char *mask, int mask_len,
                            double *output_values, int output_values_len)
{
    int i = 0, z = 0;
    int top = mask_len * 8;
    if (top > MAX_AXIS) {
        top = MAX_AXIS;
    }

    SDL_memset(output_values, 0, output_values_len * sizeof(double));
    for (; i < top && z < output_values_len; i++) {
        if (XIMaskIsSet(mask, i)) {
            const int value = (int)*input_values;
            output_values[z] = value;
            input_values++;
        }
        z++;
    }
}

static int query_xinput2_version(Display *display, int major, int minor)
{
    /* We don't care if this fails, so long as it sets major/minor on it's way out the door. */
    X11_XIQueryVersion(display, &major, &minor);
    return (major * 1000) + minor;
}

static SDL_bool xinput2_version_atleast(const int version, const int wantmajor, const int wantminor)
{
    return version >= ((wantmajor * 1000) + wantminor);
}

static SDL_WindowData *xinput2_get_sdlwindowdata(SDL_VideoData *videodata, Window window)
{
    int i;
    for (i = 0; i < videodata->numwindows; i++) {
        SDL_WindowData *d = videodata->windowlist[i];
        if (d->xwindow == window) {
            return d;
        }
    }
    return NULL;
}

static SDL_Window *xinput2_get_sdlwindow(SDL_VideoData *videodata, Window window)
{
    const SDL_WindowData *windowdata = xinput2_get_sdlwindowdata(videodata, window);
    return windowdata ? windowdata->window : NULL;
}

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
static void xinput2_normalize_touch_coordinates(SDL_Window *window, double in_x, double in_y, float *out_x, float *out_y)
{
    if (window) {
        if (window->w == 1) {
            *out_x = 0.5f;
        } else {
            *out_x = in_x / (window->w - 1);
        }
        if (window->h == 1) {
            *out_y = 0.5f;
        } else {
            *out_y = in_y / (window->h - 1);
        }
    } else {
        // couldn't find the window...
        *out_x = in_x;
        *out_y = in_y;
    }
}
#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH */

#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2 */

SDL_bool X11_InitXinput2(SDL_VideoDevice *_this)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
    SDL_VideoData *data = _this->driverdata;

    int version = 0;
    XIEventMask eventmask;
    unsigned char mask[4] = { 0, 0, 0, 0 };
    int event, err;

    /*
     * Initialize XInput 2
     * According to http://who-t.blogspot.com/2009/05/xi2-recipes-part-1.html its better
     * to inform Xserver what version of Xinput we support.The server will store the version we support.
     * "As XI2 progresses it becomes important that you use this call as the server may treat the client
     * differently depending on the supported version".
     *
     * FIXME:event and err are not needed but if not passed X11_XQueryExtension returns SegmentationFault
     */
    if (!SDL_X11_HAVE_XINPUT2 ||
        !X11_XQueryExtension(data->display, "XInputExtension", &xinput2_opcode, &event, &err)) {
        return SDL_FALSE; /* X server does not have XInput at all */
    }

    /* We need at least 2.2 for Multitouch, 2.0 otherwise. */
    version = query_xinput2_version(data->display, 2, 2);
    if (!xinput2_version_atleast(version, 2, 0)) {
        return SDL_FALSE; /* X server does not support the version we want at all. */
    }

    xinput2_initialized = 1;

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH /* Multitouch needs XInput 2.2 */
    xinput2_multitouch_supported = xinput2_version_atleast(version, 2, 2);
#endif

    /* Enable raw motion events for this display */
    SDL_zero(eventmask);
    SDL_zeroa(mask);
    eventmask.deviceid = XIAllMasterDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    XISetMask(mask, XI_RawMotion);
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawButtonRelease);

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    /* Enable raw touch events if supported */
    if (X11_Xinput2IsMultitouchSupported()) {
        XISetMask(mask, XI_RawTouchBegin);
        XISetMask(mask, XI_RawTouchUpdate);
        XISetMask(mask, XI_RawTouchEnd);
    }
#endif

    X11_XISelectEvents(data->display, DefaultRootWindow(data->display), &eventmask, 1);

    SDL_zero(eventmask);
    SDL_zeroa(mask);
    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    XISetMask(mask, XI_HierarchyChanged);
    X11_XISelectEvents(data->display, DefaultRootWindow(data->display), &eventmask, 1);

    X11_Xinput2UpdateDevices(_this, SDL_TRUE);

    return SDL_TRUE;
#else
    return SDL_FALSE;
#endif
}

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
/* xi2 device went away? take it out of the list. */
static void xinput2_remove_device_info(SDL_VideoData *videodata, const int device_id)
{
    SDL_XInput2DeviceInfo *prev = NULL;
    SDL_XInput2DeviceInfo *devinfo;

    for (devinfo = videodata->mouse_device_info; devinfo; devinfo = devinfo->next) {
        if (devinfo->device_id == device_id) {
            SDL_assert((devinfo == videodata->mouse_device_info) == (prev == NULL));
            if (!prev) {
                videodata->mouse_device_info = devinfo->next;
            } else {
                prev->next = devinfo->next;
            }
            SDL_free(devinfo);
            return;
        }
        prev = devinfo;
    }
}

static SDL_XInput2DeviceInfo *xinput2_get_device_info(SDL_VideoData *videodata, const int device_id)
{
    /* cache device info as we see new devices. */
    SDL_XInput2DeviceInfo *prev = NULL;
    SDL_XInput2DeviceInfo *devinfo;
    XIDeviceInfo *xidevinfo;
    int axis = 0;
    int i;

    for (devinfo = videodata->mouse_device_info; devinfo; devinfo = devinfo->next) {
        if (devinfo->device_id == device_id) {
            SDL_assert((devinfo == videodata->mouse_device_info) == (prev == NULL));
            if (prev) { /* move this to the front of the list, assuming we'll get more from this one. */
                prev->next = devinfo->next;
                devinfo->next = videodata->mouse_device_info;
                videodata->mouse_device_info = devinfo;
            }
            return devinfo;
        }
        prev = devinfo;
    }

    /* don't know about this device yet, query and cache it. */
    devinfo = (SDL_XInput2DeviceInfo *)SDL_calloc(1, sizeof(SDL_XInput2DeviceInfo));
    if (!devinfo) {
        return NULL;
    }

    xidevinfo = X11_XIQueryDevice(videodata->display, device_id, &i);
    if (!xidevinfo) {
        SDL_free(devinfo);
        return NULL;
    }

    devinfo->device_id = device_id;

    /* !!! FIXME: this is sort of hacky because we only care about the first two axes we see, but any given
       !!! FIXME:  axis could be relative or absolute, and they might not even be the X and Y axes!
       !!! FIXME:  But we go on, for now. Maybe we need a more robust mouse API in SDL3... */
    for (i = 0; i < xidevinfo->num_classes; i++) {
        const XIValuatorClassInfo *v = (const XIValuatorClassInfo *)xidevinfo->classes[i];
        if (v->type == XIValuatorClass) {
            devinfo->relative[axis] = (v->mode == XIModeRelative);
            devinfo->minval[axis] = v->min;
            devinfo->maxval[axis] = v->max;
            if (++axis >= 2) {
                break;
            }
        }
    }

    X11_XIFreeDeviceInfo(xidevinfo);

    devinfo->next = videodata->mouse_device_info;
    videodata->mouse_device_info = devinfo;

    return devinfo;
}

static void xinput2_pen_ensure_window(SDL_VideoDevice *_this, const SDL_Pen *pen, Window window)
{
    /* When "flipping" a Wacom eraser pen, we get an XI_DeviceChanged event
     * with the newly-activated pen, but this event is global for the display.
     * We won't get a window until the pen starts triggering motion or
     * button events, so we instead hook the pen to its window at that point. */
    const SDL_WindowData *windowdata = X11_FindWindow(_this, window);
    if (windowdata) {
        SDL_SendPenWindowEvent(0, pen->header.id, windowdata->window);
    }
}
#endif

void X11_HandleXinput2Event(SDL_VideoDevice *_this, XGenericEventCookie *cookie)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
    SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;

    if (cookie->extension != xinput2_opcode) {
        return;
    }

    switch (cookie->evtype) {
    case XI_HierarchyChanged:
    {
        const XIHierarchyEvent *hierev = (const XIHierarchyEvent *)cookie->data;
        int i;
        for (i = 0; i < hierev->num_info; i++) {
            if (hierev->info[i].flags & XISlaveRemoved) {
                xinput2_remove_device_info(videodata, hierev->info[i].deviceid);
            }
        }
        videodata->xinput_hierarchy_changed = SDL_TRUE;
    } break;

    case XI_PropertyEvent:
    case XI_DeviceChanged:
    {
        // FIXME: We shouldn't rescan all devices for pen changes every time a property or active slave changes
        X11_InitPen(_this);
    } break;

    case XI_Enter:
    case XI_Leave:
    {
        const XIEnterEvent *enterev = (const XIEnterEvent *)cookie->data;
        const SDL_WindowData *windowdata = X11_FindWindow(_this, enterev->event);
        const SDL_Pen *pen = SDL_GetPenPtr(X11_PenIDFromDeviceID(enterev->sourceid));
        SDL_Window *window = (windowdata && (cookie->evtype == XI_Enter)) ? windowdata->window : NULL;
        if (pen) {
            SDL_SendPenWindowEvent(0, pen->header.id, window);
        }
    } break;

    case XI_RawMotion:
    {
        const XIRawEvent *rawev = (const XIRawEvent *)cookie->data;
        const SDL_bool is_pen = X11_PenIDFromDeviceID(rawev->sourceid) != SDL_PEN_INVALID;
        SDL_Mouse *mouse = SDL_GetMouse();
        SDL_XInput2DeviceInfo *devinfo;
        double coords[2];
        double processed_coords[2];
        int i;

        videodata->global_mouse_changed = SDL_TRUE;
        if (is_pen) {
            break; /* Pens check for XI_Motion instead */
        }

        /* Non-pen: */

        if (!mouse->relative_mode || mouse->relative_mode_warp) {
            break;
        }

        /* Relative mouse motion is delivered to the window with keyboard focus */
        if (!SDL_GetKeyboardFocus()) {
            break;
        }

        devinfo = xinput2_get_device_info(videodata, rawev->deviceid);
        if (!devinfo) {
            break; /* oh well. */
        }

        parse_valuators(rawev->raw_values, rawev->valuators.mask,
                        rawev->valuators.mask_len, coords, 2);

        for (i = 0; i < 2; i++) {
            if (devinfo->relative[i]) {
                processed_coords[i] = coords[i];
            } else {
                processed_coords[i] = devinfo->prev_coords[i] - coords[i]; /* convert absolute to relative */
            }
        }

        SDL_SendMouseMotion(0, mouse->focus, (SDL_MouseID)rawev->sourceid, SDL_TRUE, (float)processed_coords[0], (float)processed_coords[1]);
        devinfo->prev_coords[0] = coords[0];
        devinfo->prev_coords[1] = coords[1];
    } break;

    case XI_KeyPress:
    case XI_KeyRelease:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        SDL_WindowData *windowdata = X11_FindWindow(_this, xev->event);
        XEvent xevent;

        if (xev->deviceid != xev->sourceid) {
            /* Discard events from "Master" devices to avoid duplicates. */
            break;
        }

        if (cookie->evtype == XI_KeyPress) {
            xevent.type = KeyPress;
        } else {
            xevent.type = KeyRelease;
        }
        xevent.xkey.serial = xev->serial;
        xevent.xkey.send_event = xev->send_event;
        xevent.xkey.display = xev->display;
        xevent.xkey.window = xev->event;
        xevent.xkey.root = xev->root;
        xevent.xkey.subwindow = xev->child;
        xevent.xkey.time = xev->time;
        xevent.xkey.x = xev->event_x;
        xevent.xkey.y = xev->event_y;
        xevent.xkey.x_root = xev->root_x;
        xevent.xkey.y_root = xev->root_y;
        xevent.xkey.state = xev->mods.effective;
        xevent.xkey.keycode = xev->detail;
        xevent.xkey.same_screen = 1;

        X11_HandleKeyEvent(_this, windowdata, (SDL_KeyboardID)xev->sourceid, &xevent);
    } break;

    case XI_RawButtonPress:
    case XI_RawButtonRelease:
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    case XI_RawTouchBegin:
    case XI_RawTouchUpdate:
    case XI_RawTouchEnd:
#endif
    {
        videodata->global_mouse_changed = SDL_TRUE;
    } break;

    case XI_ButtonPress:
    case XI_ButtonRelease:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        const SDL_Pen *pen = SDL_GetPenPtr(X11_PenIDFromDeviceID(xev->deviceid));
        const int button = xev->detail;
        const SDL_bool pressed = (cookie->evtype == XI_ButtonPress) ? SDL_TRUE : SDL_FALSE;

        if (pen) {
            xinput2_pen_ensure_window(_this, pen, xev->event);

            /* Only report button event; if there was also pen movement / pressure changes, we expect
               an XI_Motion event first anyway */
            if (button == 1) {
                /* button 1 is the pen tip */
                if (pressed && SDL_PenPerformHitTest()) {
                    /* Check whether we should handle window resize / move events */
                    SDL_WindowData *windowdata = X11_FindWindow(_this, xev->event);
                    if (windowdata && X11_TriggerHitTestAction(_this, windowdata, pen->last.x, pen->last.y)) {
                        SDL_SendWindowEvent(windowdata->window, SDL_EVENT_WINDOW_HIT_TEST, 0, 0);
                        break; /* Don't pass on this event */
                    }
                }
                SDL_SendPenTipEvent(0, pen->header.id,
                                    pressed ? SDL_PRESSED : SDL_RELEASED);
            } else {
                SDL_SendPenButton(0, pen->header.id,
                                  pressed ? SDL_PRESSED : SDL_RELEASED,
                                  button - 1);
            }
        } else {
            /* Otherwise assume a regular mouse */
            SDL_WindowData *windowdata = xinput2_get_sdlwindowdata(videodata, xev->event);

            if (xev->deviceid != xev->sourceid) {
                /* Discard events from "Master" devices to avoid duplicates. */
                break;
            }

            if (pressed) {
                X11_HandleButtonPress(_this, windowdata, (SDL_MouseID)xev->sourceid, button,
                                      xev->event_x, xev->event_y, xev->time);
            } else {
                X11_HandleButtonRelease(_this, windowdata, (SDL_MouseID)xev->sourceid, button);
            }
        }
    } break;

    /* Register to receive XI_Motion (which deactivates MotionNotify), so that we can distinguish
       real mouse motions from synthetic ones, for multitouch and pen support. */
    case XI_Motion:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        const SDL_Pen *pen = SDL_GetPenPtr(X11_PenIDFromDeviceID(xev->deviceid));
#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
        SDL_bool pointer_emulated = ((xev->flags & XIPointerEmulated) != 0);
#else
        SDL_bool pointer_emulated = SDL_FALSE;
#endif

        videodata->global_mouse_changed = SDL_TRUE;

        if (xev->deviceid != xev->sourceid) {
            /* Discard events from "Master" devices to avoid duplicates. */
            break;
        }

        if (pen) {
            SDL_PenStatusInfo pen_status;

            pen_status.x = xev->event_x;
            pen_status.y = xev->event_y;

            X11_PenAxesFromValuators(pen,
                                     xev->valuators.values, xev->valuators.mask, xev->valuators.mask_len,
                                     &pen_status.axes[0]);

            xinput2_pen_ensure_window(_this, pen, xev->event);

            SDL_SendPenMotion(0, pen->header.id,
                              SDL_TRUE,
                              &pen_status);
            break;
        }

        if (!pointer_emulated) {
            SDL_Mouse *mouse = SDL_GetMouse();
            if (!mouse->relative_mode || mouse->relative_mode_warp) {
                SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
                if (window) {
                    X11_ProcessHitTest(_this, window->driverdata, (float)xev->event_x, (float)xev->event_y, SDL_FALSE);
                    SDL_SendMouseMotion(0, window, (SDL_MouseID)xev->sourceid, SDL_FALSE, (float)xev->event_x, (float)xev->event_y);
                }
            }
        }
    } break;

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    case XI_TouchBegin:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        float x, y;
        SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
        xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
        SDL_SendTouch(0, xev->sourceid, xev->detail, window, SDL_TRUE, x, y, 1.0);
    } break;

    case XI_TouchEnd:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        float x, y;
        SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
        xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
        SDL_SendTouch(0, xev->sourceid, xev->detail, window, SDL_FALSE, x, y, 1.0);
    } break;

    case XI_TouchUpdate:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        float x, y;
        SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
        xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
        SDL_SendTouchMotion(0, xev->sourceid, xev->detail, window, x, y, 1.0);
    } break;
#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH */
    }
#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2 */
}

void X11_InitXinput2Multitouch(SDL_VideoDevice *_this)
{
}

void X11_Xinput2SelectTouch(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_VideoData *data = NULL;
    XIEventMask eventmask;
    unsigned char mask[4] = { 0, 0, 0, 0 };
    SDL_WindowData *window_data = NULL;

    if (!X11_Xinput2IsMultitouchSupported()) {
        return;
    }

    data = _this->driverdata;
    window_data = window->driverdata;

    eventmask.deviceid = XIAllMasterDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    XISetMask(mask, XI_TouchBegin);
    XISetMask(mask, XI_TouchUpdate);
    XISetMask(mask, XI_TouchEnd);
    XISetMask(mask, XI_Motion);

    X11_XISelectEvents(data->display, window_data->xwindow, &eventmask, 1);
#endif
}

int X11_Xinput2IsInitialized(void)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
    return xinput2_initialized;
#else
    return 0;
#endif
}

SDL_bool X11_Xinput2SelectMouseAndKeyboard(SDL_VideoDevice *_this, SDL_Window *window)
{
    SDL_WindowData *windowdata = (SDL_WindowData *)window->driverdata;

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
    const SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;

    if (X11_Xinput2IsInitialized()) {
        XIEventMask eventmask;
        unsigned char mask[4] = { 0, 0, 0, 0 };

        eventmask.mask_len = sizeof(mask);
        eventmask.mask = mask;
        eventmask.deviceid = XIAllDevices;

/* This is not enabled by default because these events are only delivered to the window with mouse focus, not keyboard focus */
#ifdef USE_XINPUT2_KEYBOARD
        XISetMask(mask, XI_KeyPress);
        XISetMask(mask, XI_KeyRelease);
        windowdata->xinput2_keyboard_enabled = SDL_TRUE;
#endif

        XISetMask(mask, XI_ButtonPress);
        XISetMask(mask, XI_ButtonRelease);
        XISetMask(mask, XI_Motion);
        windowdata->xinput2_mouse_enabled = SDL_TRUE;

        XISetMask(mask, XI_Enter);
        XISetMask(mask, XI_Leave);

        /* Hotplugging: */
        XISetMask(mask, XI_DeviceChanged);
        XISetMask(mask, XI_HierarchyChanged);
        XISetMask(mask, XI_PropertyEvent); /* E.g., when swapping tablet pens */

        if (X11_XISelectEvents(data->display, windowdata->xwindow, &eventmask, 1) != Success) {
            SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Could not enable XInput2 event handling\n");
            windowdata->xinput2_keyboard_enabled = SDL_FALSE;
            windowdata->xinput2_mouse_enabled = SDL_FALSE;
        }
    }
#endif

    if (windowdata->xinput2_keyboard_enabled || windowdata->xinput2_mouse_enabled) {
        return SDL_TRUE;
    }
    return SDL_FALSE;
}

int X11_Xinput2IsMultitouchSupported(void)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    return xinput2_initialized && xinput2_multitouch_supported;
#else
    return 0;
#endif
}

void X11_Xinput2GrabTouch(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;

    unsigned char mask[4] = { 0, 0, 0, 0 };
    XIGrabModifiers mods;
    XIEventMask eventmask;

    if (!X11_Xinput2IsMultitouchSupported()) {
        return;
    }

    mods.modifiers = XIAnyModifier;
    mods.status = 0;

    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    XISetMask(eventmask.mask, XI_TouchBegin);
    XISetMask(eventmask.mask, XI_TouchUpdate);
    XISetMask(eventmask.mask, XI_TouchEnd);
    XISetMask(eventmask.mask, XI_Motion);

    X11_XIGrabTouchBegin(display, XIAllDevices, data->xwindow, True, &eventmask, 1, &mods);
#endif
}

void X11_Xinput2UngrabTouch(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_WindowData *data = window->driverdata;
    Display *display = data->videodata->display;

    XIGrabModifiers mods;

    if (!X11_Xinput2IsMultitouchSupported()) {
        return;
    }

    mods.modifiers = XIAnyModifier;
    mods.status = 0;

    X11_XIUngrabTouchBegin(display, XIAllDevices, data->xwindow, 1, &mods);
#endif
}

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2

static void AddDeviceID(Uint32 deviceID, Uint32 **list, int *count)
{
    int new_count = (*count + 1);
    Uint32 *new_list = (Uint32 *)SDL_realloc(*list, new_count * sizeof(*new_list));
    if (!new_list) {
        /* Oh well, we'll drop this one */
        return;
    }
    new_list[new_count - 1] = deviceID;

    *count = new_count;
    *list = new_list;
}

static SDL_bool HasDeviceID(Uint32 deviceID, Uint32 *list, int count)
{
    for (int i = 0; i < count; ++i) {
        if (deviceID == list[i]) {
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

#endif // SDL_VIDEO_DRIVER_X11_XINPUT2

void X11_Xinput2UpdateDevices(SDL_VideoDevice *_this, SDL_bool initial_check)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
    SDL_VideoData *data = _this->driverdata;
    XIDeviceInfo *info;
    int ndevices;
    int old_keyboard_count = 0;
    SDL_KeyboardID *old_keyboards = NULL;
    int new_keyboard_count = 0;
    SDL_KeyboardID *new_keyboards = NULL;
    int old_mouse_count = 0;
    SDL_MouseID *old_mice = NULL;
    int new_mouse_count = 0;
    SDL_MouseID *new_mice = NULL;
    int old_touch_count = 0;
    SDL_TouchID *old_touch_devices64 = NULL;
    Uint32 *old_touch_devices = NULL;
    int new_touch_count = 0;
    Uint32 *new_touch_devices = NULL;
    SDL_bool send_event = !initial_check;

    SDL_assert(X11_Xinput2IsInitialized());

    info = X11_XIQueryDevice(data->display, XIAllDevices, &ndevices);

    old_keyboards = SDL_GetKeyboards(&old_keyboard_count);
    old_mice = SDL_GetMice(&old_mouse_count);

    /* SDL_TouchID is 64-bit, but our helper functions take Uint32 */
    old_touch_devices64 = SDL_GetTouchDevices(&old_touch_count);
    if (old_touch_count > 0) {
        old_touch_devices = (Uint32 *)SDL_malloc(old_touch_count * sizeof(*old_touch_devices));
        if (old_touch_devices) {
            for (int i = 0; i < old_touch_count; ++i) {
                old_touch_devices[i] = (Uint32)old_touch_devices64[i];
            }
        }
    }
    SDL_free(old_touch_devices64);

    for (int i = 0; i < ndevices; i++) {
        XIDeviceInfo *dev = &info[i];

        switch (dev->use) {
        case XIMasterKeyboard:
        case XISlaveKeyboard:
            {
                SDL_KeyboardID keyboardID = (SDL_KeyboardID)dev->deviceid;
                AddDeviceID(keyboardID, &new_keyboards, &new_keyboard_count);
                if (!HasDeviceID(keyboardID, old_keyboards, old_keyboard_count)) {
                    SDL_AddKeyboard(keyboardID, dev->name, send_event);
                }
            }
            break;
        case XIMasterPointer:
        case XISlavePointer:
            {
                SDL_MouseID mouseID = (SDL_MouseID)dev->deviceid;
                AddDeviceID(mouseID, &new_mice, &new_mouse_count);
                if (!HasDeviceID(mouseID, old_mice, old_mouse_count)) {
                    SDL_AddMouse(mouseID, dev->name, send_event);
                }
            }
            break;
        default:
            break;
        }

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
        for (int j = 0; j < dev->num_classes; j++) {
            Uint32 touchID;
            SDL_TouchDeviceType touchType;
            XIAnyClassInfo *class = dev->classes[j];
            XITouchClassInfo *t = (XITouchClassInfo *)class;

            /* Only touch devices */
            if (class->type != XITouchClass) {
                continue;
            }

            touchID = (Uint32)t->sourceid;
            AddDeviceID(touchID, &new_touch_devices, &new_touch_count);
            if (!HasDeviceID(touchID, old_touch_devices, old_touch_count)) {
                if (t->mode == XIDependentTouch) {
                    touchType = SDL_TOUCH_DEVICE_INDIRECT_RELATIVE;
                } else { /* XIDirectTouch */
                    touchType = SDL_TOUCH_DEVICE_DIRECT;
                }
                SDL_AddTouch(touchID, touchType, dev->name);
            }
        }
#endif // SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    }

    for (int i = old_keyboard_count; i--;) {
        if (!HasDeviceID(old_keyboards[i], new_keyboards, new_keyboard_count)) {
            SDL_RemoveKeyboard(old_keyboards[i], send_event);
        }
    }

    for (int i = old_mouse_count; i--;) {
        if (!HasDeviceID(old_mice[i], new_mice, new_mouse_count)) {
            SDL_RemoveMouse(old_mice[i], send_event);
        }
    }

    for (int i = old_touch_count; i--;) {
        if (!HasDeviceID(old_touch_devices[i], new_touch_devices, new_touch_count)) {
            SDL_DelTouch(old_touch_devices[i]);
        }
    }

    SDL_free(old_keyboards);
    SDL_free(new_keyboards);
    SDL_free(old_mice);
    SDL_free(new_mice);
    SDL_free(old_touch_devices);
    SDL_free(new_touch_devices);

    X11_XIFreeDeviceInfo(info);

#endif // SDL_VIDEO_DRIVER_X11_XINPUT2
}

#endif /* SDL_VIDEO_DRIVER_X11 */
