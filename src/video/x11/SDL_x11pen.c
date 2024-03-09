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
#include "../../SDL_internal.h"

#ifdef SDL_VIDEO_DRIVER_X11_XINPUT2

#include "../../events/SDL_pen_c.h"
#include "../SDL_sysvideo.h"
#include "SDL_x11pen.h"
#include "SDL_x11video.h"
#include "SDL_x11xinput2.h"

#define PEN_ERASER_ID_MAXLEN 256      /* Max # characters of device name to scan */
#define PEN_ERASER_NAME_TAG  "eraser" /* String constant to identify erasers */

#define DEBUG_PEN (SDL_PEN_DEBUG_NOID | SDL_PEN_DEBUG_NONWACOM | SDL_PEN_DEBUG_UNKNOWN_WACOM | SDL_PEN_DEBUG_NOSERIAL_WACOM)

#define SDL_PEN_AXIS_VALUATOR_MISSING -1

/* X11-specific information attached to each pen */
typedef struct xinput2_pen
{
    float axis_min[SDL_PEN_NUM_AXES];
    float axis_max[SDL_PEN_NUM_AXES];
    float slider_bias;                         /* shift value to add to PEN_AXIS_SLIDER (before normalisation) */
    float rotation_bias;                       /* rotation to add to PEN_AXIS_ROTATION  (after normalisation) */
    Sint8 valuator_for_axis[SDL_PEN_NUM_AXES]; /* SDL_PEN_AXIS_VALUATOR_MISSING if not supported */
} xinput2_pen;

/* X11 atoms */
static struct
{
    int initialized; /* initialised to 0 */
    Atom device_product_id;
    Atom abs_pressure;
    Atom abs_tilt_x;
    Atom abs_tilt_y;
    Atom wacom_serial_ids;
    Atom wacom_tool_type;
} pen_atoms;

/*
 * Mapping from X11 device IDs to pen IDs
 *
 * In X11, the same device ID may represent any number of pens.  We
 * thus cannot directly use device IDs as pen IDs.
 */
static struct
{
    int num_pens_known; /* Number of currently known pens (based on their GUID); used to give pen ID to new pens */
    int num_entries;    /* Number of X11 device IDs that correspond to pens */

    struct pen_device_id_mapping
    {
        Uint32 deviceid;
        Uint32 pen_id;
    } * entries; /* Current pen to device ID mappings */
} pen_map;

typedef enum
{
    SDL_PEN_VENDOR_UNKNOWN = 0,
    SDL_PEN_VENDOR_WACOM
} sdl_pen_vendor;

/* Information to identify pens during discovery */
typedef struct
{
    sdl_pen_vendor vendor;
    SDL_GUID guid;
    SDL_PenSubtype heuristic_type; /* Distinguish pen+eraser devices with shared bus ID */
    Uint32 devicetype_id, serial;  /* used by PEN_VENDOR_WACOM */
    Uint32 deviceid;
} pen_identity;

int X11_PenIDFromDeviceID(int deviceid)
{
    int i;
    for (i = 0; i < pen_map.num_entries; ++i) {
        if (pen_map.entries[i].deviceid == deviceid) {
            return pen_map.entries[i].pen_id;
        }
    }
    return SDL_PEN_INVALID;
}

static void pen_atoms_ensure_initialized(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;

    if (pen_atoms.initialized) {
        return;
    }
    /* Create atoms if they don't exist yet to pre-empt hotplugging updates */
    pen_atoms.device_product_id = X11_XInternAtom(data->display, "Device Product ID", False);
    pen_atoms.wacom_serial_ids = X11_XInternAtom(data->display, "Wacom Serial IDs", False);
    pen_atoms.wacom_tool_type = X11_XInternAtom(data->display, "Wacom Tool Type", False);
    pen_atoms.abs_pressure = X11_XInternAtom(data->display, "Abs Pressure", False);
    pen_atoms.abs_tilt_x = X11_XInternAtom(data->display, "Abs Tilt X", False);
    pen_atoms.abs_tilt_y = X11_XInternAtom(data->display, "Abs Tilt Y", False);

    pen_atoms.initialized = 1;
}

/* Read out an integer property and store into a preallocated Sint32 array, extending 8 and 16 bit values suitably.
   Returns number of Sint32s written (<= max_words), or 0 on error. */
static size_t xinput2_pen_get_int_property(SDL_VideoDevice *_this, int deviceid, Atom property, Sint32 *dest, size_t max_words)
{
    const SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;
    Atom type_return;
    int format_return;
    unsigned long num_items_return;
    unsigned long bytes_after_return;
    unsigned char *output;

    if (property == None) {
        return 0;
    }

    if (Success != X11_XIGetProperty(data->display, deviceid,
                                     property,
                                     0, max_words, False,
                                     XA_INTEGER, &type_return, &format_return,
                                     &num_items_return, &bytes_after_return,
                                     &output) ||
        num_items_return == 0 || output == NULL) {
        return 0;
    }

    if (type_return == XA_INTEGER) {
        int k;
        const int to_copy = SDL_min(max_words, num_items_return);

        if (format_return == 8) {
            Sint8 *numdata = (Sint8 *)output;
            for (k = 0; k < to_copy; ++k) {
                dest[k] = numdata[k];
            }
        } else if (format_return == 16) {
            Sint16 *numdata = (Sint16 *)output;
            for (k = 0; k < to_copy; ++k) {
                dest[k] = numdata[k];
            }
        } else {
            SDL_memcpy(dest, output, sizeof(Sint32) * to_copy);
        }
        X11_XFree(output);
        return to_copy;
    }
    return 0; /* type mismatch */
}

/* 32 bit vendor + device ID from evdev */
static Uint32 xinput2_pen_evdevid(SDL_VideoDevice *_this, int deviceid)
{
#if !(SDL_PEN_DEBUG_NOID)
    Sint32 ids[2];

    pen_atoms_ensure_initialized(_this);

    if (2 != xinput2_pen_get_int_property(_this, deviceid, pen_atoms.device_product_id, ids, 2)) {
        return 0;
    }
    return ((ids[0] << 16) | (ids[1] & 0xffff));
#else /* Testing: pretend that we have no ID (not sure if this can happen IRL) */
    return 0;
#endif
}

/* Gets reasonably-unique GUID for the device */
static void xinput2_pen_update_generic_guid(SDL_VideoDevice *_this, pen_identity *pident, int deviceid)
{
    Uint32 evdevid = xinput2_pen_evdevid(_this, deviceid); /* also initialises pen_atoms  */

    if (!evdevid) {
        /* Fallback: if no evdevid is available; try to at least distinguish devices within the
           current session.  This is a poor GUID and our last resort. */
        evdevid = deviceid;
    }
    SDL_PenUpdateGUIDForGeneric(&pident->guid, 0, evdevid);
}

/* Identify Wacom devices (if SDL_TRUE is returned) and extract their device type and serial IDs */
static SDL_bool xinput2_wacom_deviceid(SDL_VideoDevice *_this, int deviceid, Uint32 *wacom_devicetype_id, Uint32 *wacom_serial)
{
#if !(SDL_PEN_DEBUG_NONWACOM) /* Can be disabled for testing */
    Sint32 serial_id_buf[3];
    int result;

    pen_atoms_ensure_initialized(_this);

    if ((result = xinput2_pen_get_int_property(_this, deviceid, pen_atoms.wacom_serial_ids, serial_id_buf, 3)) == 3) {
        *wacom_devicetype_id = serial_id_buf[2];
        *wacom_serial = serial_id_buf[1];
#if SDL_PEN_DEBUG_NOSERIAL_WACOM /* Disabled for testing? */
        *wacom_serial = 0;
#endif
        return SDL_TRUE;
    }
#endif
    return SDL_FALSE;
}

/* Heuristically determines if device is an eraser */
static SDL_bool xinput2_pen_is_eraser(SDL_VideoDevice *_this, int deviceid, char *devicename)
{
    SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;
    char dev_name[PEN_ERASER_ID_MAXLEN];
    int k;

    pen_atoms_ensure_initialized(_this);

    if (pen_atoms.wacom_tool_type != None) {
        Atom type_return;
        int format_return;
        unsigned long num_items_return;
        unsigned long bytes_after_return;
        unsigned char *tooltype_name_info = NULL;

        /* Try Wacom-specific method */
        if (Success == X11_XIGetProperty(data->display, deviceid,
                                         pen_atoms.wacom_tool_type,
                                         0, 32, False,
                                         AnyPropertyType, &type_return, &format_return,
                                         &num_items_return, &bytes_after_return,
                                         &tooltype_name_info) &&
            tooltype_name_info != NULL && num_items_return > 0) {

            SDL_bool result = SDL_FALSE;
            char *tooltype_name = NULL;

            if (type_return == XA_ATOM) {
                /* Atom instead of string?  Un-intern */
                Atom atom = *((Atom *)tooltype_name_info);
                if (atom != None) {
                    tooltype_name = X11_XGetAtomName(data->display, atom);
                }
            } else if (type_return == XA_STRING && format_return == 8) {
                tooltype_name = (char *)tooltype_name_info;
            }

            if (tooltype_name) {
                if (0 == SDL_strcasecmp(tooltype_name, PEN_ERASER_NAME_TAG)) {
                    result = SDL_TRUE;
                }
                X11_XFree(tooltype_name_info);

                return result;
            }
        }
    }
    /* Non-Wacom device? */

    /* We assume that a device is an eraser if its name contains the string "eraser".
     * Unfortunately there doesn't seem to be a clean way to distinguish these cases (as of 2022-03). */

    SDL_strlcpy(dev_name, devicename, PEN_ERASER_ID_MAXLEN);
    /* lowercase device name string so we can use strstr() */
    for (k = 0; dev_name[k]; ++k) {
        dev_name[k] = SDL_tolower(dev_name[k]);
    }

    return (SDL_strstr(dev_name, PEN_ERASER_NAME_TAG)) ? SDL_TRUE : SDL_FALSE;
}

/* Gets GUID and other identifying information for the device using the best known method */
static pen_identity xinput2_identify_pen(SDL_VideoDevice *_this, int deviceid, char *name)
{
    pen_identity pident;

    pident.devicetype_id = 0ul;
    pident.serial = 0ul;
    pident.deviceid = deviceid;
    pident.heuristic_type = SDL_PEN_TYPE_PEN;
    SDL_memset(pident.guid.data, 0, sizeof(pident.guid.data));

    if (xinput2_pen_is_eraser(_this, deviceid, name)) {
        pident.heuristic_type = SDL_PEN_TYPE_ERASER;
    }

    if (xinput2_wacom_deviceid(_this, deviceid, &pident.devicetype_id, &pident.serial)) {
        pident.vendor = SDL_PEN_VENDOR_WACOM;
        SDL_PenUpdateGUIDForWacom(&pident.guid, pident.devicetype_id, pident.serial);

#if DEBUG_PEN
        printf("[pen] Pen %d reports Wacom device_id %x\n",
               deviceid, pident.devicetype_id);
#endif

    } else {
        pident.vendor = SDL_PEN_VENDOR_UNKNOWN;
    }
    if (!pident.serial) {
        /* If the pen has a serial number, we can move it across tablets and retain its identity.
           Otherwise, we use the evdev ID as part of its GUID, which may mean that we identify it with the tablet. */
        xinput2_pen_update_generic_guid(_this, &pident, deviceid);
    }
    SDL_PenUpdateGUIDForType(&pident.guid, pident.heuristic_type);
    return pident;
}

static void xinput2_pen_free_deviceinfo(Uint32 deviceid, void *x11_peninfo, void *context)
{
    SDL_free(x11_peninfo);
}

static void xinput2_merge_deviceinfo(xinput2_pen *dest, xinput2_pen *src)
{
    *dest = *src;
}

/**
 * Fill in vendor-specific device information, if available
 *
 * For Wacom pens: identify number of buttons and extra axis (if present)
 *
 * \param _this global state
 * \param dev The device to analyse
 * \param pen The pen to initialise
 * \param pident Pen identity information
 * \param[out] valuator_5 Meaning of the valuator with offset 5, if any
 *   (written only if known and if the device has a 6th axis,
 *   e.g., for the Wacom Art Pen and Wacom Airbrush Pen)
 * \param[out] axes Bitmask of all possibly supported axes
 *
 * This function identifies Wacom device types through a Wacom-specific device ID.
 * It then fills in pen details from an internal database.
 * If the device seems to be a Wacom pen/eraser but can't be identified, the function
 * leaves "axes" untouched and sets the other outputs to common defaults.
 *
 * There is no explicit support for other vendors, though vendors that
 * emulate the Wacom API might be supported.
 *
 * Unsupported devices will keep the default settings.
 */
static void xinput2_vendor_peninfo(SDL_VideoDevice *_this, const XIDeviceInfo *dev, SDL_Pen *pen, pen_identity pident, int *valuator_5, Uint32 *axes)
{
    switch (pident.vendor) {
    case SDL_PEN_VENDOR_WACOM:
    {
        if (SDL_PenModifyForWacomID(pen, pident.devicetype_id, axes)) {
            if (*axes & SDL_PEN_AXIS_SLIDER_MASK) {
                /* Air Brush Pen or eraser */
                *valuator_5 = SDL_PEN_AXIS_SLIDER;
            } else if (*axes & SDL_PEN_AXIS_ROTATION_MASK) {
                /* Art Pen or eraser, or 6D Art Pen */
                *valuator_5 = SDL_PEN_AXIS_ROTATION;
            }
            return;
        } else {
#if DEBUG_PEN
            printf("[pen] Could not identify wacom pen %d with device id %x, using default settings\n",
                   pident.deviceid, pident.devicetype_id);
#endif
            break;
        }
    }

    default:
#if DEBUG_PEN
        printf("[pen] Pen %d is not from a known vendor\n", pident.deviceid);
#endif
        break;
    }

    /* Fall back to default heuristics for identifying device type */

    SDL_strlcpy(pen->name, dev->name, SDL_PEN_MAX_NAME);

    pen->type = pident.heuristic_type;
}

/* Does this device have a valuator for pressure sensitivity? */
static SDL_bool xinput2_device_is_pen(SDL_VideoDevice *_this, const XIDeviceInfo *dev)
{
    int classct;

    pen_atoms_ensure_initialized(_this);

    for (classct = 0; classct < dev->num_classes; ++classct) {
        const XIAnyClassInfo *classinfo = dev->classes[classct];

        switch (classinfo->type) {
        case XIValuatorClass:
        {
            XIValuatorClassInfo *val_classinfo = (XIValuatorClassInfo *)classinfo;
            Atom vname = val_classinfo->label;

            if (vname == pen_atoms.abs_pressure) {
                return SDL_TRUE;
            }
        }
        }
    }
    return SDL_FALSE;
}

void X11_InitPen(SDL_VideoDevice *_this)
{
    SDL_VideoData *data = (SDL_VideoData *)_this->driverdata;
    int i;
    XIDeviceInfo *device_info;
    int num_device_info;

    device_info = X11_XIQueryDevice(data->display, XIAllDevices, &num_device_info);
    if (!device_info) {
        return;
    }

    /* Reset the device id -> pen map */
    if (pen_map.entries) {
        SDL_free(pen_map.entries);
        pen_map.entries = NULL;
        pen_map.num_entries = 0;
    }

    SDL_PenGCMark();

    for (i = 0; i < num_device_info; ++i) {
        const XIDeviceInfo *dev = &device_info[i];
        int classct;
        xinput2_pen pen_device;
        Uint32 capabilities = 0;
        Uint32 axis_mask = ~0;    /* Permitted axes (default: all) */
        int valuator_5_axis = -1; /* For Wacom devices, the 6th valuator (offset 5) has a model-specific meaning */
        pen_identity pident;
        SDL_PenID pen_id;
        SDL_Pen *pen;
        int old_num_pens_known = pen_map.num_pens_known;
        int k;

        /* Only track physical devices that are enabled and look like pens */
        if (dev->use != XISlavePointer || dev->enabled == 0 || !xinput2_device_is_pen(_this, dev)) {
            continue;
        }

        pen_device.slider_bias = 0.0f;
        pen_device.rotation_bias = 0.0f;
        for (k = 0; k < SDL_PEN_NUM_AXES; ++k) {
            pen_device.valuator_for_axis[k] = SDL_PEN_AXIS_VALUATOR_MISSING;
        }

        pident = xinput2_identify_pen(_this, dev->deviceid, dev->name);

        pen_id = SDL_GetPenFromGUID(pident.guid);
        if (pen_id == SDL_PEN_INVALID) {
            /* We have never met this pen */
            pen_id = ++pen_map.num_pens_known; /* start at 1 */
        }
        pen = SDL_PenModifyBegin(pen_id);

        /* Complement XF86 driver information with vendor-specific details */
        xinput2_vendor_peninfo(_this, dev, pen, pident, &valuator_5_axis, &axis_mask);

        for (classct = 0; classct < dev->num_classes; ++classct) {
            const XIAnyClassInfo *classinfo = dev->classes[classct];

            switch (classinfo->type) {
            case XIValuatorClass:
            {
                XIValuatorClassInfo *val_classinfo = (XIValuatorClassInfo *)classinfo;
                Sint8 valuator_nr = val_classinfo->number;
                Atom vname = val_classinfo->label;
                int axis = -1;

                float min = val_classinfo->min;
                float max = val_classinfo->max;

                if (vname == pen_atoms.abs_pressure) {
                    axis = SDL_PEN_AXIS_PRESSURE;
                } else if (vname == pen_atoms.abs_tilt_x) {
                    axis = SDL_PEN_AXIS_XTILT;
                } else if (vname == pen_atoms.abs_tilt_y) {
                    axis = SDL_PEN_AXIS_YTILT;
                }

                if (axis == -1 && valuator_nr == 5) {
                    /* Wacom model-specific axis support */
                    /* The meaning of the various axes is highly underspecitied in Xinput2.
                     * As of 2023-08-26, Wacom seems to be the only vendor to support these axes, so the code below
                     * captures the de-facto standard. */
                    axis = valuator_5_axis;

                    switch (axis) {
                    case SDL_PEN_AXIS_SLIDER:
                        /* cf. xinput2_wacom_peninfo for how this axis is used.
                           In all current cases, our API wants this value in 0..1, but the xf86 driver
                           starts at a negative offset, so we normalise here. */
                        pen_device.slider_bias = -min;
                        max -= min;
                        min = 0.0f;
                        break;

                    case SDL_PEN_AXIS_ROTATION:
                        /* The "0" value points to the left, rather than up, so we must
                           rotate 90 degrees counter-clockwise to have 0 point to the top. */

                        pen_device.rotation_bias = -90.0f;
                        break;

                    default:
                        break;
                    }
                }

                if (axis >= 0) {
                    capabilities |= SDL_PEN_AXIS_CAPABILITY(axis);

                    pen_device.valuator_for_axis[axis] = valuator_nr;
                    pen_device.axis_min[axis] = min;
                    pen_device.axis_max[axis] = max;
                }
                break;
            }
            default:
                break;
            }
        }

        /* We have a pen if and only if the device measures pressure */
        if (capabilities & SDL_PEN_AXIS_PRESSURE_MASK) {
            xinput2_pen *xinput2_deviceinfo;
            Uint64 guid_a, guid_b;

            /* Done collecting data, write to pen */
            SDL_PenModifyAddCapabilities(pen, capabilities);
            pen->guid = pident.guid;

            if (pen->deviceinfo) {
                /* Updating a known pen */
                xinput2_deviceinfo = (xinput2_pen *)pen->deviceinfo;
                xinput2_merge_deviceinfo(xinput2_deviceinfo, &pen_device);
            } else {
                /* Registering a new pen */
                xinput2_deviceinfo = SDL_malloc(sizeof(xinput2_pen));
                SDL_memcpy(xinput2_deviceinfo, &pen_device, sizeof(xinput2_pen));
            }
            pen->deviceinfo = xinput2_deviceinfo;

#if DEBUG_PEN
            printf("[pen] pen %d [%04x] valuators pressure=%d, xtilt=%d, ytilt=%d [%s]\n",
                   pen->header.id, pen->header.flags,
                   pen_device.valuator_for_axis[SDL_PEN_AXIS_PRESSURE],
                   pen_device.valuator_for_axis[SDL_PEN_AXIS_XTILT],
                   pen_device.valuator_for_axis[SDL_PEN_AXIS_YTILT],
                   pen->name);
#endif
            SDL_memcpy(&guid_a, &pen->guid.data[0], 8);
            SDL_memcpy(&guid_b, &pen->guid.data[8], 8);
            if (!(guid_a | guid_b)) {
#if DEBUG_PEN
                printf("[pen]    (pen eliminated due to zero GUID)\n");
#endif
                pen->type = SDL_PEN_TYPE_NONE;
            }

        } else {
            /* Not a pen, mark for deletion */
            pen->type = SDL_PEN_TYPE_NONE;
        }
        SDL_PenModifyEnd(pen, SDL_TRUE);

        if (pen->type != SDL_PEN_TYPE_NONE) {
            const int map_pos = pen_map.num_entries;

            /* We found a pen: add mapping */
            if (pen_map.entries == NULL) {
                pen_map.entries = SDL_calloc(sizeof(struct pen_device_id_mapping), 1);
                pen_map.num_entries = 1;
            } else {
                pen_map.num_entries += 1;
                pen_map.entries = SDL_realloc(pen_map.entries,
                                              pen_map.num_entries * (sizeof(struct pen_device_id_mapping)));
            }
            pen_map.entries[map_pos].deviceid = dev->deviceid;
            pen_map.entries[map_pos].pen_id = pen_id;
        } else {
            /* Revert pen number allocation */
            pen_map.num_pens_known = old_num_pens_known;
        }
    }
    X11_XIFreeDeviceInfo(device_info);

    SDL_PenGCSweep(NULL, xinput2_pen_free_deviceinfo);
}

static void xinput2_normalize_pen_axes(const SDL_Pen *peninfo,
                                       const xinput2_pen *xpen,
                                       /* inout-mode paramters: */
                                       float *coords)
{
    int axis;

    /* Normalise axes */
    for (axis = 0; axis < SDL_PEN_NUM_AXES; ++axis) {
        int valuator = xpen->valuator_for_axis[axis];
        if (valuator != SDL_PEN_AXIS_VALUATOR_MISSING) {
            float value = coords[axis];
            float min = xpen->axis_min[axis];
            float max = xpen->axis_max[axis];

            if (axis == SDL_PEN_AXIS_SLIDER) {
                value += xpen->slider_bias;
            }

            /* min ... 0 ... max */
            if (min < 0.0) {
                /* Normalise so that 0 remains 0.0 */
                if (value < 0) {
                    value = value / (-min);
                } else {
                    if (max == 0.0) {
                        value = 0.0f;
                    } else {
                        value = value / max;
                    }
                }
            } else {
                /* 0 ... min ... max */
                /* including 0.0 = min */
                if (max == 0.0) {
                    value = 0.0f;
                } else {
                    value = (value - min) / max;
                }
            }

            switch (axis) {
            case SDL_PEN_AXIS_XTILT:
            case SDL_PEN_AXIS_YTILT:
                if (peninfo->info.max_tilt > 0.0f) {
                    value *= peninfo->info.max_tilt; /* normalise to physical max */
                }
                break;

            case SDL_PEN_AXIS_ROTATION:
                /* normalised to -1..1, so let's convert to degrees */
                value *= 180.0;
                value += xpen->rotation_bias;

                /* handle simple over/underflow */
                if (value >= 180.0f) {
                    value -= 360.0f;
                } else if (value < -180.0f) {
                    value += 360.0f;
                }
                break;

            default:
                break;
            }
            coords[axis] = value;
        }
    }
}

void X11_PenAxesFromValuators(const SDL_Pen *peninfo,
                              const double *input_values, const unsigned char *mask, const int mask_len,
                              /* out-mode parameters: */
                              float axis_values[SDL_PEN_NUM_AXES])
{
    const xinput2_pen *pen = (xinput2_pen *)peninfo->deviceinfo;
    int i;

    for (i = 0; i < SDL_PEN_NUM_AXES; ++i) {
        const int valuator = pen->valuator_for_axis[i];
        if (valuator == SDL_PEN_AXIS_VALUATOR_MISSING || valuator >= mask_len * 8 || !(XIMaskIsSet(mask, valuator))) {
            axis_values[i] = 0.0f;
        } else {
            axis_values[i] = input_values[valuator];
        }
    }
    xinput2_normalize_pen_axes(peninfo, pen, axis_values);
}

#endif /* SDL_VIDEO_DRIVER_X11_XINPUT2 */

/* vi: set ts=4 sw=4 expandtab: */
