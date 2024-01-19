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

#include "SDL_error.h"
#include "SDL_guid.h"
#include "SDL_stdinc.h"

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

typedef Uint32 SDL_PenID; /**< SDL_PenIDs identify pens uniquely within a session */

#define SDL_PEN_INVALID ((Uint32)0) /**< Reserved invalid ::SDL_PenID is valid */

#define SDL_PEN_MOUSEID ((Uint32)-2) /**< Device ID for mouse events triggered by pen events */

#define SDL_PEN_INFO_UNKNOWN (-1) /**< Marks unknown information when querying the pen */

/**
 * Pen axis indices
 *
 * Below are the valid indices to the "axis" array from ::SDL_PenMotionEvent and ::SDL_PenButtonEvent.
 * The axis indices form a contiguous range of ints from 0 to ::SDL_PEN_AXIS_LAST, inclusive.
 * All "axis[]" entries are either normalised to  0..1 or report a (positive or negative)
 * angle in degrees, with 0.0 representing the centre.
 * Not all pens/backends support all axes: unsupported entries are always "0.0f".
 *
 * To convert angles for tilt and rotation into vector representation, use
 * SDL_sinf on the XTILT, YTILT, or ROTATION component, e.g., "SDL_sinf(xtilt * SDL_PI_F / 180.0)".
 */
typedef enum
{
    SDL_PEN_AXIS_PRESSURE = 0,               /**< Pen pressure.  Unidirectional: 0..1.0 */
    SDL_PEN_AXIS_XTILT,                      /**< Pen horizontal tilt angle.  Bidirectional: -90.0..90.0 (left-to-right).
						The physical max/min tilt may be smaller than -90.0 / 90.0, cf. SDL_PenCapabilityInfo */
    SDL_PEN_AXIS_YTILT,                      /**< Pen vertical tilt angle.  Bidirectional: -90.0..90.0 (top-to-down).
						The physical max/min tilt may be smaller than -90.0 / 90.0, cf. SDL_PenCapabilityInfo */
    SDL_PEN_AXIS_DISTANCE,                   /**< Pen distance to drawing surface.  Unidirectional: 0.0..1.0 */
    SDL_PEN_AXIS_ROTATION,                   /**< Pen barrel rotation.  Bidirectional: -180..179.9 (clockwise, 0 is facing up, -180.0 is facing down). */
    SDL_PEN_AXIS_SLIDER,                     /**< Pen finger wheel or slider (e.g., Airbrush Pen).  Unidirectional: 0..1.0 */
    SDL_PEN_NUM_AXES,                        /**< Last valid axis index */
    SDL_PEN_AXIS_LAST = SDL_PEN_NUM_AXES - 1 /**< Last axis index plus 1 */
} SDL_PenAxis;

/* Pen flags.  These share a bitmask space with ::SDL_BUTTON_LEFT and friends. */
#define SDL_PEN_FLAG_DOWN_BIT_INDEX   13 /* Bit for storing that pen is touching the surface */
#define SDL_PEN_FLAG_INK_BIT_INDEX    14 /* Bit for storing has-non-eraser-capability status */
#define SDL_PEN_FLAG_ERASER_BIT_INDEX 15 /* Bit for storing is-eraser or has-eraser-capability property */
#define SDL_PEN_FLAG_AXIS_BIT_OFFSET  16 /* Bit for storing has-axis-0 property */

#define SDL_PEN_CAPABILITY(capbit)    (1ul << (capbit))
#define SDL_PEN_AXIS_CAPABILITY(axis) SDL_PEN_CAPABILITY((axis) + SDL_PEN_FLAG_AXIS_BIT_OFFSET)

/**
 * Pen tips
 * @{
 */
#define SDL_PEN_TIP_INK    SDL_PEN_FLAG_INK_BIT_INDEX     /**< Regular pen tip (for drawing) touched the surface */
#define SDL_PEN_TIP_ERASER SDL_PEN_FLAG_ERASER_BIT_INDEX  /**< Eraser pen tip touched the surface */
/** @} */


/**
 * \defgroup SDL_PEN_CAPABILITIES Pen capabilities
 * Pen capabilities reported by ::SDL_GetPenCapabilities
 * @{
 */
#define SDL_PEN_DOWN_MASK          SDL_PEN_CAPABILITY(SDL_PEN_FLAG_DOWN_BIT_INDEX)   /**< Pen tip is currently touching the drawing surface. */
#define SDL_PEN_INK_MASK           SDL_PEN_CAPABILITY(SDL_PEN_FLAG_INK_BIT_INDEX)    /**< Pen has a regular drawing tip (::SDL_GetPenCapabilities).  For events (::SDL_PenButtonEvent, ::SDL_PenMotionEvent, ::SDL_GetPenStatus) this flag is mutually exclusive with ::SDL_PEN_ERASER_MASK .  */
#define SDL_PEN_ERASER_MASK        SDL_PEN_CAPABILITY(SDL_PEN_FLAG_ERASER_BIT_INDEX) /**< Pen has an eraser tip (::SDL_GetPenCapabilities) or is being used as eraser (::SDL_PenButtonEvent , ::SDL_PenMotionEvent , ::SDL_GetPenStatus)  */
#define SDL_PEN_AXIS_PRESSURE_MASK SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_PRESSURE)    /**< Pen provides pressure information in axis ::SDL_PEN_AXIS_PRESSURE */
#define SDL_PEN_AXIS_XTILT_MASK    SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_XTILT)       /**< Pen provides horizontal tilt information in axis ::SDL_PEN_AXIS_XTILT */
#define SDL_PEN_AXIS_YTILT_MASK    SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_YTILT)       /**< Pen provides vertical tilt information in axis ::SDL_PEN_AXIS_YTILT */
#define SDL_PEN_AXIS_DISTANCE_MASK SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_DISTANCE)    /**< Pen provides distance to drawing tablet in ::SDL_PEN_AXIS_DISTANCE */
#define SDL_PEN_AXIS_ROTATION_MASK SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_ROTATION)    /**< Pen provides barrel rotation information in axis ::SDL_PEN_AXIS_ROTATION */
#define SDL_PEN_AXIS_SLIDER_MASK   SDL_PEN_AXIS_CAPABILITY(SDL_PEN_AXIS_SLIDER)      /**< Pen provides slider / finger wheel or similar in axis ::SDL_PEN_AXIS_SLIDER */

#define SDL_PEN_AXIS_BIDIRECTIONAL_MASKS (SDL_PEN_AXIS_XTILT_MASK | SDL_PEN_AXIS_YTILT_MASK)
/**< Masks for all axes that may be bidirectional */
/** @} */

/**
 * Pen types
 *
 * Some pens identify as a particular type of drawing device (e.g., an airbrush or a pencil).
 *
 */
typedef enum
{
    SDL_PEN_TYPE_ERASER = 1,                  /**< Eraser */
    SDL_PEN_TYPE_PEN,                         /**< Generic pen; this is the default. */
    SDL_PEN_TYPE_PENCIL,                      /**< Pencil */
    SDL_PEN_TYPE_BRUSH,                       /**< Brush-like device */
    SDL_PEN_TYPE_AIRBRUSH,                    /**< Airbrush device that "sprays" ink */
    SDL_PEN_TYPE_LAST = SDL_PEN_TYPE_AIRBRUSH /**< Last valid pen type */
} SDL_PenSubtype;


/* Function prototypes */

/**
 * Retrieves all pens that are connected to the system.
 *
 * Yields an array of ::SDL_PenID values. These identify and track pens
 * throughout a session. To track pens across sessions (program restart), use
 * ::SDL_GUID .
 *
 * \param count The number of pens in the array (number of array elements
 *              minus 1, i.e., not counting the terminator 0).
 * \returns A 0 terminated array of ::SDL_PenID values, or NULL on error. The
 *          array must be freed with ::SDL_free(). On a NULL return,
 *          ::SDL_GetError() is set.
 *
 * \since This function is available since SDL 3.0.0
 */
extern DECLSPEC SDL_PenID *SDLCALL SDL_GetPens(int *count);

/**
 * Retrieves the pen's current status.
 *
 * If the pen is detached (cf. ::SDL_PenConnected), this operation may return
 * default values.
 *
 * \param instance_id The pen to query.
 * \param x Out-mode parameter for pen x coordinate. May be NULL.
 * \param y Out-mode parameter for pen y coordinate. May be NULL.
 * \param axes Out-mode parameter for axis information. May be null. The axes
 *             are in the same order as ::SDL_PenAxis.
 * \param num_axes Maximum number of axes to write to "axes".
 * \returns a bit mask with the current pen button states (::SDL_BUTTON_LMASK
 *          etc.), possibly ::SDL_PEN_DOWN_MASK, and exactly one of
 *          ::SDL_PEN_INK_MASK or ::SDL_PEN_ERASER_MASK , or 0 on error (see
 *          ::SDL_GetError()).
 *
 * \since This function is available since SDL 3.0.0
 */
extern DECLSPEC Uint32 SDLCALL SDL_GetPenStatus(SDL_PenID instance_id, float *x, float *y, float *axes, size_t num_axes);

/**
 * Retrieves an ::SDL_PenID for the given ::SDL_GUID.
 *
 * \param guid A pen GUID.
 * \returns A valid ::SDL_PenID, or ::SDL_PEN_INVALID if there is no matching
 *          SDL_PenID.
 *
 * \since This function is available since SDL 3.0.0
 *
 * \sa SDL_GUID
 */
extern DECLSPEC SDL_PenID SDLCALL SDL_GetPenFromGUID(SDL_GUID guid);

/**
 * Retrieves the ::SDL_GUID for a given ::SDL_PenID.
 *
 * \param instance_id The pen to query.
 * \returns The corresponding pen GUID; persistent across multiple sessions.
 *          If "instance_id" is ::SDL_PEN_INVALID, returns an all-zeroes GUID.
 *
 * \since This function is available since SDL 3.0.0
 *
 * \sa SDL_PenForID
 */
extern DECLSPEC SDL_GUID SDLCALL SDL_GetPenGUID(SDL_PenID instance_id);

/**
 * Checks whether a pen is still attached.
 *
 * If a pen is detached, it will not show up for ::SDL_GetPens(). Other
 * operations will still be available but may return default values.
 *
 * \param instance_id A pen ID.
 * \returns SDL_TRUE if "instance_id" is valid and the corresponding pen is
 *          attached, or SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0
 */
extern DECLSPEC SDL_bool SDLCALL SDL_PenConnected(SDL_PenID instance_id);

/**
 * Retrieves a human-readable description for a ::SDL_PenID.
 *
 * \param instance_id The pen to query.
 * \returns A string that contains the name of the pen, intended for human
 *          consumption. The string might or might not be localised, depending
 *          on platform settings. It is not guaranteed to be unique; use
 *          ::SDL_GetPenGUID() for (best-effort) unique identifiers. The
 *          pointer is managed by the SDL pen subsystem and must not be
 *          deallocated. The pointer remains valid until SDL is shut down.
 *          Returns NULL on error (cf. ::SDL_GetError())
 *
 * \since This function is available since SDL 3.0.0
 */
extern DECLSPEC const char *SDLCALL SDL_GetPenName(SDL_PenID instance_id);

/**
 * Pen capabilities, as reported by ::SDL_GetPenCapabilities()
 */
typedef struct SDL_PenCapabilityInfo
{
    float max_tilt;    /**< Physical maximum tilt angle, for XTILT and YTILT, or SDL_PEN_INFO_UNKNOWN .  Pens cannot typically tilt all the way to 90 degrees, so this value is usually less than 90.0. */
    Uint32 wacom_id;   /**< For Wacom devices: wacom tool type ID, otherwise 0 (useful e.g. with libwacom) */
    Sint8 num_buttons; /**< Number of pen buttons (not counting the pen tip), or SDL_PEN_INFO_UNKNOWN */
} SDL_PenCapabilityInfo;

/**
 * Retrieves capability flags for a given ::SDL_PenID.
 *
 * \param instance_id The pen to query.
 * \param capabilities Detail information about pen capabilities, such as the
 *                     number of buttons
 * \returns a set of capability flags, cf. SDL_PEN_CAPABILITIES
 *
 * \since This function is available since SDL 3.0.0
 */
extern DECLSPEC Uint32 SDLCALL SDL_GetPenCapabilities(SDL_PenID instance_id, SDL_PenCapabilityInfo *capabilities);

/**
 * Retrieves the pen type for a given ::SDL_PenID.
 *
 * \param instance_id The pen to query.
 * \returns The corresponding pen type (cf. ::SDL_PenSubtype) or 0 on error.
 *          Note that the pen type does not dictate whether the pen tip is
 *          ::SDL_PEN_TIP_INK or ::SDL_PEN_TIP_ERASER; to determine whether a
 *          pen is being used for drawing or in eraser mode, check either the
 *          pen tip on ::SDL_EVENT_PEN_DOWN, or the flag ::SDL_PEN_ERASER_MASK
 *          in the pen state.
 *
 * \since This function is available since SDL 3.0.0
 */
extern DECLSPEC SDL_PenSubtype SDLCALL SDL_GetPenType(SDL_PenID instance_id);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* SDL_pen_h_ */

/* vi: set ts=4 sw=4 expandtab: */
