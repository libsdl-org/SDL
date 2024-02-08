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

void X11_InitXinput2(SDL_VideoDevice *_this)
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
        return; /* X server does not have XInput at all */
    }

    /* We need at least 2.2 for Multitouch, 2.0 otherwise. */
    version = query_xinput2_version(data->display, 2, 2);
    if (!xinput2_version_atleast(version, 2, 0)) {
        return; /* X server does not support the version we want at all. */
    }

    xinput2_initialized = 1;

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH /* Multitouch needs XInput 2.2 */
    xinput2_multitouch_supported = xinput2_version_atleast(version, 2, 2);
#endif

    /* Enable raw motion events for this display */
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

    if (X11_XISelectEvents(data->display, DefaultRootWindow(data->display), &eventmask, 1) != Success) {
        return;
    }

    SDL_zero(eventmask);
    SDL_zeroa(mask);
    eventmask.deviceid = XIAllDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    XISetMask(mask, XI_HierarchyChanged);
    if (X11_XISelectEvents(data->display, DefaultRootWindow(data->display), &eventmask, 1) != Success) {
        return;
    }
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

int X11_HandleXinput2Event(SDL_VideoDevice *_this, XGenericEventCookie *cookie)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
    SDL_VideoData *videodata = (SDL_VideoData *)_this->driverdata;

    if (cookie->extension != xinput2_opcode) {
        return 0;
    }
    switch (cookie->evtype) {
    case XI_PropertyEvent:
    case XI_DeviceChanged:
    {
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
            return 0; /* Pens check for XI_Motion instead */
        }

        /* Non-pen: */

        if (!mouse->relative_mode || mouse->relative_mode_warp) {
            return 0;
        }

        devinfo = xinput2_get_device_info(videodata, rawev->deviceid);
        if (!devinfo) {
            return 0; /* oh well. */
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

        SDL_SendMouseMotion(0, mouse->focus, mouse->mouseID, 1, (float)processed_coords[0], (float)processed_coords[1]);
        devinfo->prev_coords[0] = coords[0];
        devinfo->prev_coords[1] = coords[1];
        return 1;
    } break;

    case XI_HierarchyChanged:
    {
        const XIHierarchyEvent *hierev = (const XIHierarchyEvent *)cookie->data;
        int i;
        for (i = 0; i < hierev->num_info; i++) {
            if (hierev->info[i].flags & XISlaveRemoved) {
                xinput2_remove_device_info(videodata, hierev->info[i].deviceid);
            }
        }
        X11_InitPen(_this);
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
                        return 1; /* Don't pass on this event */
                    }
                }
                SDL_SendPenTipEvent(0, pen->header.id,
                                    pressed ? SDL_PRESSED : SDL_RELEASED);
            } else {
                SDL_SendPenButton(0, pen->header.id,
                                  pressed ? SDL_PRESSED : SDL_RELEASED,
                                  button - 1);
            }
            return 1;
        } else {
            /* Otherwise assume a regular mouse */
            SDL_WindowData *windowdata = xinput2_get_sdlwindowdata(videodata, xev->event);

            if (xev->deviceid != xev->sourceid) {
                /* Discard events from "Master" devices to avoid duplicates. */
                return 1;
            }

            if (pressed) {
                X11_HandleButtonPress(_this, windowdata, button,
                                      xev->event_x, xev->event_y, xev->time);
            } else {
                X11_HandleButtonRelease(_this, windowdata, button);
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
        int pointer_emulated = (xev->flags & XIPointerEmulated);
#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH */

        videodata->global_mouse_changed = SDL_TRUE;

        if (xev->deviceid != xev->sourceid) {
            /* Discard events from "Master" devices to avoid duplicates. */
            return 1;
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
            return 1;
        }

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
        if (!pointer_emulated) {
            SDL_Mouse *mouse = SDL_GetMouse();
            if (!mouse->relative_mode || mouse->relative_mode_warp) {
                SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
                if (window) {
                    X11_ProcessHitTest(_this, window->driverdata, (float)xev->event_x, (float)xev->event_y, SDL_FALSE);
                    SDL_SendMouseMotion(0, window, 0, 0, (float)xev->event_x, (float)xev->event_y);
                }
            }
        }
        return 1;
#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH */
    } break;

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    case XI_TouchBegin:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        float x, y;
        SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
        xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
        SDL_SendTouch(0, xev->sourceid, xev->detail, window, SDL_TRUE, x, y, 1.0);
        return 1;
    } break;
    case XI_TouchEnd:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        float x, y;
        SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
        xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
        SDL_SendTouch(0, xev->sourceid, xev->detail, window, SDL_FALSE, x, y, 1.0);
        return 1;
    } break;
    case XI_TouchUpdate:
    {
        const XIDeviceEvent *xev = (const XIDeviceEvent *)cookie->data;
        float x, y;
        SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
        xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
        SDL_SendTouchMotion(0, xev->sourceid, xev->detail, window, x, y, 1.0);
        return 1;
    } break;
#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH */
    }
#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2 */
    return 0;
}

void X11_InitXinput2Multitouch(SDL_VideoDevice *_this)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_VideoData *data = _this->driverdata;
    XIDeviceInfo *info;
    int ndevices, i, j;

    if (!X11_Xinput2IsMultitouchSupported()) {
        return;
    }

    info = X11_XIQueryDevice(data->display, XIAllDevices, &ndevices);

    for (i = 0; i < ndevices; i++) {
        XIDeviceInfo *dev = &info[i];
        for (j = 0; j < dev->num_classes; j++) {
            SDL_TouchID touchId;
            SDL_TouchDeviceType touchType;
            XIAnyClassInfo *class = dev->classes[j];
            XITouchClassInfo *t = (XITouchClassInfo *)class;

            /* Only touch devices */
            if (class->type != XITouchClass) {
                continue;
            }

            if (t->mode == XIDependentTouch) {
                touchType = SDL_TOUCH_DEVICE_INDIRECT_RELATIVE;
            } else { /* XIDirectTouch */
                touchType = SDL_TOUCH_DEVICE_DIRECT;
            }

            touchId = t->sourceid;
            SDL_AddTouch(touchId, touchType, dev->name);
        }
    }
    X11_XIFreeDeviceInfo(info);
#endif
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

SDL_bool X11_Xinput2SelectMouse(SDL_VideoDevice *_this, SDL_Window *window)
{
#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2
    const SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;
    XIEventMask eventmask;
    unsigned char mask[4] = { 0, 0, 0, 0 };
    SDL_WindowData *window_data = (SDL_WindowData *)window->driverdata;

    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;
    eventmask.deviceid = XIAllDevices;

    XISetMask(mask, XI_ButtonPress);
    XISetMask(mask, XI_ButtonRelease);
    XISetMask(mask, XI_Motion);
    XISetMask(mask, XI_Enter);
    XISetMask(mask, XI_Leave);
    /* Hotplugging: */
    XISetMask(mask, XI_DeviceChanged);
    XISetMask(mask, XI_HierarchyChanged);
    XISetMask(mask, XI_PropertyEvent); /* E.g., when swapping tablet pens */

    if (X11_XISelectEvents(data->display,
                           window_data->xwindow,
                           &eventmask, 1) == Success) {
        return SDL_TRUE;
    }
    SDL_LogWarn(SDL_LOG_CATEGORY_INPUT, "Could not enable XInput2 mouse event handling\n");
#endif
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

#endif /* SDL_VIDEO_DRIVER_X11 */
