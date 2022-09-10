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

/**
 *  \file SDL_pen.h
 *
 *  Include file for SDL pen event handling.
 *
 *  This file describes operations for pressure-sensitive pen (stylus and/or eraser) handling, e.g., for input
 *    and drawing tablets or suitably equipped mobile / tablet devices.
 *
 *  To get started with pens:
 *  - Listen to ::SDL_PenMotionEvent and ::SDL_PenButtonEvent
 *  - To avoid treating pen events as mouse events, ignore  ::SDL_MouseMotionEvent and ::SDL_MouseButtonEvent
 *    whenever "which" == ::SDL_PEN_MOUSEID.
 *
 *  This header file describes advanced functionality that can be useful for managing user configuration
 *    and understanding the capabilities of the attached pens.
 *
 *  We primarily identify pens by ::SDL_PenID.  The implementation makes a best effort to relate each :SDL_PenID
 *    to the same physical device during a session.  Formerly valid ::SDL_PenID values remain valid
 *    even if a device disappears.
 *
 *  For identifying pens across sessions, the API provides the type ::SDL_GUID .
 */

#ifndef SDL_pen_h_
#define SDL_pen_h_

#include "SDL_stdinc.h"
#include "SDL_error.h"
#include "SDL_guid.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef Uint32 SDL_PenID;               /**< SDL_PenIDs identify pens uniquely within a session */

#define SDL_PENID_INVALID ((Uint32)0)   /**< Reserved invalid ::SDL_PenID is valid */

#define SDL_PEN_MOUSEID ((Uint32)-2)    /**< Device ID for mouse events triggered by pen events */

#define SDL_PEN_INFO_UNKNOWN (-1)       /**< Marks unknown information when querying the pen */

/**
 * Pen axis indices
 *
 * Below are the valid indices to the "axis" array from ::SDL_PenMotionEvent and ::SDL_PenButtonEvent.
 * The axis indices form a contiguous range of ints from 0 to ::SDL_PEN_AXIS_LAST, inclusive.
 * All "axis[]" entries are normalised to either 0..1 (unidirectional axes) or between -1..1 (bidirectional axes).
 * Unsupported entries are always "0.0f".
 *
 */
typedef enum {
    SDL_PEN_AXIS_PRESSURE = 0,                /**< Pen pressure.  Unidirectional: 0..1.0 */
    SDL_PEN_AXIS_XTILT,                       /**< Pen horizontal tilt fraction.  Bidirectional: -1.0..1.0 (left-to-right).  This is NOT the angle, but the tilt vector x component.  The physical max/min tilt may be smaller than -1.0 / 1.0, cf. \link SDL_PenCapabilityInfo \endlink .  */
    SDL_PEN_AXIS_YTILT,                       /**< Pen vertical tilt fraction.  Bidirectional: -1.0..1.0 (top-to-bottom).  This is NOT the angle, but the tilt vector y component.  */
    SDL_PEN_AXIS_DISTANCE,                    /**< Pen distance to drawing surface.  Unidirectional: 0.0..1.0 */
    SDL_PEN_AXIS_ROTATION,                    /**< Pen barrel rotation.  Bidirectional: -1.0..1.0 (clockwise, 0 is facing up).  Multiply with "2.0 * M_PI" to get radians, or "360.0" for degrees. */
    SDL_PEN_AXIS_SLIDER,                      /**< Pen finger wheel or slider (e.g., Airbrush Pen).  Unidirectional: 0..1.0 */
    SDL_PEN_NUM_AXES,                         /**< Last valid axis index */
    SDL_PEN_AXIS_LAST = SDL_PEN_NUM_AXES - 1  /**< Last axis index plus 1 */
} SDL_PenAxis;

/* Pen flags.  THese share a bitmask space with ::SDL_BUTTON_LEFT and friends. */
#define SDL_PEN_FLAG_INK_BIT_INDEX          14  /* Bit for storing has-non-eraser capability status */
#define SDL_PEN_FLAG_ERASER_BIT_INDEX       15  /* Bit for storing is-eraser or has-eraser property */
#define SDL_PEN_FLAG_AXIS_BIT_OFFSET        16  /* Bit for storing has-axis-0 property */

#define SDL_PEN_CAPABILITY(capbit)    (1ul << (capbit))
#define SDL_PEN_AXIS_CAPABILITY(axis) SDL_PEN_CAPABILITY((axis) + SDL_PEN_FLAG_AXIS_BIT_OFFSET)

/**
 * \defgroup SDL_PEN_CAPABILITIES Pen capabilities
 * Pen capabilities reported by ::SDL_PenCapabilities
 * @{
 */
#define SDL_PEN_INK_MASK            SDL_PEN_CAPABILITY(SDL_PEN_FLAG_INK_BIT_INDEX)    /**< Pen has a regular drawing tip (::SDL_PenCapabilities).  For events (::SDL_PenButtonEvent, ::SDL_PenMotionEvent, ::SDL_PenStatus) this flag is mutually exclusive with ::SDL_PEN_ERASER_MASK .  */
#define SDL_PEN_ERASER_MASK         SDL_PEN_CAPABILITY(SDL_PEN_FLAG_ERASER_BIT_INDEX) /**< Pen has an eraser tip (::SDL_PenCapabilities) or is being used as eraser (::SDL_PenButtonEvent , ::SDL_PenMotionEvent , ::SDL_PenStatus)  */
#define SDL_PEN_AXIS_PRESSURE_MASK  SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_PRESSURE)    /**< Pen provides pressure information in axis ::SDL_PEN_AXIS_PRESSURE */
#define SDL_PEN_AXIS_XTILT_MASK     SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_XTILT)       /**< Pen provides horizontal tilt information in axis ::SDL_PEN_AXIS_XTILT */
#define SDL_PEN_AXIS_YTILT_MASK     SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_YTILT)       /**< Pen provides vertical tilt information in axis ::SDL_PEN_AXIS_YTILT */
#define SDL_PEN_AXIS_DISTANCE_MASK  SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_DISTANCE)    /**< Pen provides distance to drawing tablet in ::SDL_PEN_AXIS_DISTANCE */
#define SDL_PEN_AXIS_ROTATION_MASK  SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_ROTATION)    /**< Pen provides barrel rotation information in axis ::SDL_PEN_AXIS_ROTATION */
#define SDL_PEN_AXIS_SLIDER_MASK    SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_SLIDER)      /**< Pen provides slider / finger wheel or similar in axis ::SDL_PEN_AXIS_SLIDER */

#define SDL_PEN_AXIS_BIDIRECTIONAL_MASKS (SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK)
        /**< Masks for all axes that may be bidirectional */
/** @} */


/**
 * Pen types
 *
 * Some pens identify as a particular type of drawing device (e.g., an airbrush or a pencil).
 * Clients can use this information e.g. to select default behaviour.
 *
 */
typedef enum {
    SDL_PEN_TYPE_ERASER = 1,                    /**< Eraser */
    SDL_PEN_TYPE_PEN,                           /**< Generic pen; this is the default. */
    SDL_PEN_TYPE_PENCIL,                        /**< Pencil */
    SDL_PEN_TYPE_BRUSH,                         /**< Brush-like device */
    SDL_PEN_TYPE_AIRBRUSH,                      /**< Airbrush device that "sprays" ink */
    SDL_PEN_TYPE_LAST = SDL_PEN_TYPE_AIRBRUSH   /**< Last valid pen type */
} SDL_PenSubtype;


/* Function prototypes */

/**
 * Counts the number of pens attached to the system.
 *
 * \returns the number of known attached pens.  This number may change
 *          whenever the client processes events (::SDL_PollEvent()
 *          etc.) or calls the event or windowing subsystems, due to
 *          hot-plugging.
 *
 * \since This function is available since SDL 2.TBD
 *
 * \sa SDL_PenIDForIndex()
 */
extern DECLSPEC int SDLCALL SDL_NumPens(void);

/**
 * Retrieves a pen attached to the system while iterating over all pens.
 *
 * This function is intended for enumerating all currently attached pens.
 * The iteration order and number of valid indices may change after
 * any call to event processing (e.g., ::SDL_PollEvent()) or to the graphics, or windowing subsystems.
 *
 * Use ::SDL_PenID to track pens throughout a session, or ::SDL_GUID for
 * tracking across sessions.
 *
 * \param device_index An index between 0 and ::SDL_NumPens() - 1 (inclusive).
 *
 * \returns A valid ::SDL_PenID, or ::SDL_PENID_INVALID iff the "device_index" was outside the valid range.
 *     In the latter case, ::SDL_GetError() is set.
 *
 * \since This function is available since SDL 2.TBD
 *
 * \sa SDL_NumPens()
 */
extern DECLSPEC SDL_PenID SDLCALL SDL_PenIDForIndex(int device_index);

/**
 * Retrieves the pen's current status.
 *
 * If the pen is detached (cf. ::SDL_PenAttached), this operation may return
 * default values.
 *
 * \param pen The pen to query.
 * \param[out] x Out-mode parameter for pen x coordinate.  May be NULL.
 * \param[out] y Out-mode parameter for pen y coordinate.  May be NULL.
 * \param[out] axes Out-mode parameter for axis information.  May be null.  The axes are in the same order as for
 *     ::SDL_PenAxis .
 * \param num_axes Maximum number of axes to write to "axes".
 *
 * \returns a bit mask with the current pen button states (::SDL_BUTTON_LMASK etc.) and exactly one of
 *     ::SDL_PEN_INK_MASK or ::SDL_PEN_ERASER_MASK , or 0 on error (see ::SDL_GetError()).
 *
 * \since This function is available since SDL 2.TBD
 */
extern DECLSPEC Uint32 SDLCALL SDL_PenStatus(SDL_PenID pen, float * x, float * y, float * axes, size_t num_axes);

/**
 * Retrieves an ::SDL_PenID for the given ::SDL_GUID.
 *
 * \param guid A pen GUID.
 *
 * \returns A valid ::SDL_PenID, or ::SDL_PENID_INVALID if there is no matching SDL_PenID.
 *
 * \since This function is available since SDL 2.TBD
 *
 * \sa SDL_GUID()
 */
extern DECLSPEC SDL_PenID SDLCALL SDL_PenIDForGUID(SDL_GUID guid);

/**
 * Retrieves the ::SDL_GUID for a given ::SDL_PenID.
 *
 * \param penid The pen to query.
 *
 * \returns The corresponding pen GUID; persistent across multiple sessions.
 *     If "penid" is ::SDL_PENID_INVALID, returns an all-zeroes GUID.
 *
 * \since This function is available since SDL 2.TBD
 *
 * \sa SDL_PenForID()
 */
extern DECLSPEC SDL_GUID SDLCALL SDL_PenGUIDForPenID(SDL_PenID penid);

/**
 * Checks whether a pen is still attached.
 *
 * If a pen is detached, it will not show up for ::SDL_NumPens() and ::SDL_PenIDForIndex().
 * Other operations will still be available but may return default values.
 *
 * \param penid A pen ID.
 * \returns SDL_TRUE if "penid" is valid and the corresponding pen is attached, or
 *     SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 2.TBD
 */
extern DECLSPEC SDL_bool SDLCALL SDL_PenAttached(SDL_PenID penid);

/**
 * Retrieves a human-readable description for a ::SDL_PenID.
 *
 * \param pen The pen to query.
 *
 * \returns A statically allocated string that contains the name of the pen,
 *     intended for human consumption.  The string might or might not
 *     be localised, depending on platform settings.  The string is valid
 *     until the next call to the event, graphics, or windowing subsystems.
 *     Returns NULL on error (cf. ::SDL_GetError())
 *
 * \since This function is available since SDL 2.TBD
 */
extern DECLSPEC const char * SDLCALL SDL_PenName(SDL_PenID pen);

/**
 * Pen capabilities, as reported by ::SDL_PenCapabilities()
 */
typedef struct SDL_PenCapabilityInfo {
    float max_tilt;           /**< Physical maximum tilt values, or SDL_PEN_INFO_UNKNOWN .  Pens can't typically tilt all the way to 90 degrees, so this value is usually less than 1.0. */
    Uint32 wacom_id;          /**< For Wacom devices: wacom tool type ID, otherwise 0 (useful e.g. with libwacom) */
    Sint8 num_buttons;        /**< Number of pen buttons (not counting the pen tip), or SDL_PEN_INFO_UNKNOWN */
} SDL_PenCapabilityInfo;

/**
 * Retrieves capability flags for a given ::SDL_PenID.
 *
 * \param pen The pen to query.
 * \param[out] capabilities Detail information about pen capabilities, such as the number of buttons
 *
 * \returns a set of capability flags, cf. \link SDL_PEN_CAPABILITIES \endlink.  Returns 0 on error
 *     (cf. ::SDL_GetError())
 *
 * \since This function is available since SDL 2.TBD
 */
extern DECLSPEC Uint32 SDLCALL SDL_PenCapabilities(SDL_PenID pen, SDL_PenCapabilityInfo * capabilities);

/**
 * Retrieves the pen type for a given ::SDL_PenID.
 *
 * \param pen The pen to query.
 * \returns The corresponding pen type (cf. ::SDL_PenSubtype) or 0 on error.
 */
extern DECLSPEC SDL_PenSubtype SDLCALL SDL_PenType(SDL_PenID pen);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_pen_h_ */

/* vi: set ts=4 sw=4 expandtab: */
