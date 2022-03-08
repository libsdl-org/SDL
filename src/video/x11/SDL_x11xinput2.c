/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

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

#if SDL_VIDEO_DRIVER_X11

#include "SDL_x11video.h"
#include "SDL_x11xinput2.h"
#include "../../events/SDL_mouse_c.h"
#include "../../events/SDL_touch_c.h"

#define MAX_AXIS 16

#if SDL_VIDEO_DRIVER_X11_XINPUT2
static int xinput2_initialized = 0;

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
static int xinput2_multitouch_supported = 0;
#endif

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO
typedef struct {
    /* -1 if not present */
    int source_id;
    /* 0 = horizontal, 1 = vertical */
    int axis_id[2];
    double prev_coord[2];
    /* specifies the amount of coordinate change to scroll one line */
    double line_unit[2];
} scrollable_device;

#define MAX_SCROLLABLE_DEVICES 8
static scrollable_device scrollable_devices[MAX_SCROLLABLE_DEVICES];
#endif

/* Opcode returned X11_XQueryExtension
 * It will be used in event processing
 * to know that the event came from
 * this extension */
static int xinput2_opcode;

static void parse_valuators(const double *input_values, const unsigned char *mask,int mask_len,
                            double *output_values,int output_values_len) {
    int i = 0,z = 0;
    int top = mask_len * 8;
    if (top > MAX_AXIS)
        top = MAX_AXIS;

    SDL_memset(output_values,0,output_values_len * sizeof(double));
    for (; i < top && z < output_values_len; i++) {
        if (XIMaskIsSet(mask, i)) {
            const int value = (int) *input_values;
            output_values[z] = value;
            input_values++;
        }
        z++;
    }
}

static int
query_xinput2_version(Display *display, int major, int minor)
{
    /* We don't care if this fails, so long as it sets major/minor on it's way out the door. */
    X11_XIQueryVersion(display, &major, &minor);
    return ((major * 1000) + minor);
}

static SDL_bool
xinput2_version_atleast(const int version, const int wantmajor, const int wantminor)
{
    return ( version >= ((wantmajor * 1000) + wantminor) );
}

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO
static void
xinput2_enumerate_scrollable_devices(Display *display, SDL_bool supported)
{
    XIDeviceInfo *info;
    int ndevices,i,j,k,dev_i=0;

    SDL_memset(scrollable_devices, 0, sizeof(scrollable_devices));
    for (i = 0; i<MAX_SCROLLABLE_DEVICES; i++) {
        scrollable_devices[i].source_id = -1;
    }

    if (!supported)
        return;

    info = X11_XIQueryDevice(display, XIAllDevices, &ndevices);

    /* find scroll classes */
    for (i = 0; i < ndevices && dev_i < MAX_SCROLLABLE_DEVICES; i++) {
        scrollable_device *sd = &scrollable_devices[dev_i];
        XIDeviceInfo *dev = &info[i];
        for (j = 0; j < dev->num_classes; j++) {
            int dir;
            XIAnyClassInfo *class = dev->classes[j];
            XIScrollClassInfo *s = (XIScrollClassInfo*)class;

            if (class->type != XIScrollClass)
                continue;

            dir = s->scroll_type == XIScrollTypeVertical;
            sd->source_id = s->sourceid;
            sd->axis_id[dir] = s->number;
            sd->line_unit[dir] = s->increment;
        }

        if (scrollable_devices[dev_i].source_id == -1)
            continue;

        /* find valuator info for each scroll class */
        for (j = 0; j < dev->num_classes; j++) {
            XIAnyClassInfo *class = dev->classes[j];
            XIValuatorClassInfo *v = (XIValuatorClassInfo*)class;

            if (class->type != XIValuatorClass)
                continue;

            for (k=0; k<2; k++) {
                if (sd->axis_id[k] == v->number) {
                    sd->prev_coord[k] = v->value;
                }
            }
        }

        dev_i++;
    }

    X11_XIFreeDeviceInfo(info);
}

static void
xinput2_parse_scrollable_valuators(const XIDeviceEvent *xev)
{
    SDL_Mouse *mouse = SDL_GetMouse();            
    int i,j,k;

    for (i=0; i<MAX_SCROLLABLE_DEVICES; i++) {
        scrollable_device *sd = &scrollable_devices[i];
        if (xev->sourceid == sd->source_id) {
            int values_i=0;
            for (j=0; j<xev->valuators.mask_len*8; j++) {
                if (!XIMaskIsSet(xev->valuators.mask, j))
                    continue;

                for (k = 0; k<2; k++) {
                    if (sd->axis_id[k] == j) {
                        double current_val = xev->valuators.values[values_i];
                        double delta = (sd->prev_coord[k] - current_val)/sd->line_unit[k];
                        double x = k == 0 ? delta : 0;
                        double y = k == 1 ? delta : 0;
                        SDL_SendMouseWheel(mouse->focus,mouse->mouseID, x, y, SDL_MOUSEWHEEL_NORMAL);
                        sd->prev_coord[k] = current_val;
                    }
                }

                values_i++;
            }
        }
    }
}

#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO */

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
static SDL_Window *
xinput2_get_sdlwindow(SDL_VideoData *videodata, Window window)
{
    int i;
    for (i = 0; i < videodata->numwindows; i++) {
        SDL_WindowData *d = videodata->windowlist[i];
        if (d->xwindow == window) {
            return d->window;
        }
    }
    return NULL;
}

static void
xinput2_normalize_touch_coordinates(SDL_Window *window, double in_x, double in_y, float *out_x, float *out_y)
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

void
X11_InitXinput2(_THIS)
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;

    int version = 0;
    XIEventMask eventmask;
    unsigned char mask[3] = { 0,0,0 };
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

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO
    /* Xinput 2.1 is required to provide precision scrolling info */
    data->xinput_scrolling = xinput2_version_atleast(version, 2, 1);

    xinput2_enumerate_scrollable_devices(data->display, data->xinput_scrolling);
#endif

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH  /* Multitouch needs XInput 2.2 */
    xinput2_multitouch_supported = xinput2_version_atleast(version, 2, 2);
#endif

    /* Enable  Raw motion events for this display */
    eventmask.deviceid = XIAllMasterDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    XISetMask(mask, XI_RawMotion);
    XISetMask(mask, XI_Motion);
    XISetMask(mask, XI_RawButtonPress);
    XISetMask(mask, XI_RawButtonRelease);

    if (X11_XISelectEvents(data->display,DefaultRootWindow(data->display),&eventmask,1) != Success) {
        return;
    }
#endif
}

int
X11_HandleXinput2Event(SDL_VideoData *videodata,XGenericEventCookie *cookie)
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2
    if(cookie->extension != xinput2_opcode) {
        return 0;
    }
    switch(cookie->evtype) {
        case XI_RawMotion: {
            const XIRawEvent *rawev = (const XIRawEvent*)cookie->data;
            SDL_Mouse *mouse = SDL_GetMouse();
            double relative_coords[2];
            static Time prev_time = 0;
            static double prev_rel_coords[2];

            videodata->global_mouse_changed = SDL_TRUE;

            if (!mouse->relative_mode || mouse->relative_mode_warp) {
                return 0;
            }

            parse_valuators(rawev->raw_values,rawev->valuators.mask,
                            rawev->valuators.mask_len,relative_coords,2);

            if ((rawev->time == prev_time) && (relative_coords[0] == prev_rel_coords[0]) && (relative_coords[1] == prev_rel_coords[1])) {
                return 0;  /* duplicate event, drop it. */
            }

            SDL_SendMouseMotion(mouse->focus,mouse->mouseID,1,(int)relative_coords[0],(int)relative_coords[1]);
            prev_rel_coords[0] = relative_coords[0];
            prev_rel_coords[1] = relative_coords[1];
            prev_time = rawev->time;
            return 1;
            }
            break;

        case XI_RawButtonPress:
        case XI_RawButtonRelease:
            videodata->global_mouse_changed = SDL_TRUE;
            break;

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO || SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
        case XI_Motion: {
            const XIDeviceEvent *xev = (const XIDeviceEvent *) cookie->data;
            
#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_SCROLLINFO
            xinput2_parse_scrollable_valuators(xev);
#endif

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
            /* With multitouch, register to receive XI_Motion (which desctivates MotionNotify),
            * so that we can distinguish real mouse motions from synthetic one.  */
            if (!(xev->flags & XIPointerEmulated)) {
                SDL_Mouse *mouse = SDL_GetMouse();
                if(!mouse->relative_mode || mouse->relative_mode_warp) {
                    SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
                    if (window) {
                        SDL_SendMouseMotion(window, 0, 0, xev->event_x, xev->event_y);
                    }
                }
            }
#endif

            return 1;
            }
            break;
#endif

#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
        case XI_TouchBegin: {
            const XIDeviceEvent *xev = (const XIDeviceEvent *) cookie->data;
            float x, y;
            SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
            xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
            SDL_SendTouch(xev->sourceid, xev->detail, window, SDL_TRUE, x, y, 1.0);
            return 1;
            }
            break;
        case XI_TouchEnd: {
            const XIDeviceEvent *xev = (const XIDeviceEvent *) cookie->data;
            float x, y;
            SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
            xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
            SDL_SendTouch(xev->sourceid, xev->detail, window, SDL_FALSE, x, y, 1.0);
            return 1;
            }
            break;
        case XI_TouchUpdate: {
            const XIDeviceEvent *xev = (const XIDeviceEvent *) cookie->data;
            float x, y;
            SDL_Window *window = xinput2_get_sdlwindow(videodata, xev->event);
            xinput2_normalize_touch_coordinates(window, xev->event_x, xev->event_y, &x, &y);
            SDL_SendTouchMotion(xev->sourceid, xev->detail, window, x, y, 1.0);
            return 1;
            }
            break;
#endif
    }
#endif
    return 0;
}

void
X11_InitXinput2Multitouch(_THIS)
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_VideoData *data = (SDL_VideoData *) _this->driverdata;
    XIDeviceInfo *info;
    int ndevices,i,j;
    info = X11_XIQueryDevice(data->display, XIAllDevices, &ndevices);

    for (i = 0; i < ndevices; i++) {
        XIDeviceInfo *dev = &info[i];
        for (j = 0; j < dev->num_classes; j++) {
            SDL_TouchID touchId;
            SDL_TouchDeviceType touchType;
            XIAnyClassInfo *class = dev->classes[j];
            XITouchClassInfo *t = (XITouchClassInfo*)class;

            /* Only touch devices */
            if (class->type != XITouchClass)
                continue;

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

void
X11_Xinput2SelectTouch(_THIS, SDL_Window *window)
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_VideoData *data = NULL;
    XIEventMask eventmask;
    unsigned char mask[4] = { 0, 0, 0, 0 };
    SDL_WindowData *window_data = NULL;

    if (!X11_Xinput2IsMultitouchSupported()) {
        return;
    }

    data = (SDL_VideoData *) _this->driverdata;
    window_data = (SDL_WindowData*)window->driverdata;

    eventmask.deviceid = XIAllMasterDevices;
    eventmask.mask_len = sizeof(mask);
    eventmask.mask = mask;

    XISetMask(mask, XI_TouchBegin);
    XISetMask(mask, XI_TouchUpdate);
    XISetMask(mask, XI_TouchEnd);
    XISetMask(mask, XI_Motion);

    X11_XISelectEvents(data->display,window_data->xwindow,&eventmask,1);
#endif
}


int
X11_Xinput2IsInitialized()
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2
    return xinput2_initialized;
#else
    return 0;
#endif
}

int
X11_Xinput2IsMultitouchSupported()
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    return xinput2_initialized && xinput2_multitouch_supported;
#else
    return 0;
#endif
}

void
X11_Xinput2GrabTouch(_THIS, SDL_Window *window)
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    unsigned char mask[4] = { 0, 0, 0, 0 };
    XIGrabModifiers mods;
    XIEventMask eventmask;

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

void
X11_Xinput2UngrabTouch(_THIS, SDL_Window *window)
{
#if SDL_VIDEO_DRIVER_X11_XINPUT2_SUPPORTS_MULTITOUCH
    SDL_WindowData *data = (SDL_WindowData *) window->driverdata;
    Display *display = data->videodata->display;

    XIGrabModifiers mods;

    mods.modifiers = XIAnyModifier;
    mods.status = 0;

    X11_XIUngrabTouchBegin(display, XIAllDevices, data->xwindow, 1, &mods);
#endif
}


#endif /* SDL_VIDEO_DRIVER_X11 */

/* vi: set ts=4 sw=4 expandtab: */
