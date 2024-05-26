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
 * # CategoryPen
 *
 * SDL pen event handling.
 *
 * SDL provides an API for pressure-sensitive pen (stylus and/or
 * eraser) handling, e.g., for input and drawing tablets or suitably equipped
 * mobile / tablet devices.
 *
 * To get started with pens:
 *
 * - Listen to SDL_PenMotionEvent and SDL_PenButtonEvent
 * - To avoid treating pen events as mouse events, ignore SDL_MouseMotionEvent
 *   and SDL_MouseButtonEvent whenever `which` == SDL_PEN_MOUSEID.
 *
 * We primarily identify pens by SDL_PenID. The implementation makes a best
 * effort to relate each SDL_PenID to the same physical device during a
 * session. Formerly valid SDL_PenID values remain valid even if a device
 * disappears.
 *
 * For identifying pens across sessions, the API provides a device-specific
 * SDL_GUID. This is a best-effort attempt to identify hardware across runs,
 * and is not perfect.
 */

#ifndef SDL_pen_h_
#define SDL_pen_h_

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_guid.h>

/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

/**
 * SDL pen instance IDs.
 *
 * Zero is used to signify an invalid/null device.
 *
 * \since This datatype is available since SDL 3.0.0.
 */
typedef Uint32 SDL_PenID;

/**
 * Used as the device ID for mouse events simulated with pen input
 *
 * \since This macro is available since SDL 3.0.0.
 */
#define SDL_PEN_MOUSEID ((SDL_MouseID)-2) /**< Device ID for mouse events triggered by pen events */


/**
 * Pen axis indices.
 *
 * These are the valid indices to the `axis` array from SDL_PenMotionEvent and
 * SDL_PenButtonEvent. The axis indices form a contiguous range of ints from 0
 * to SDL_PEN_AXIS_LAST, inclusive. All "axis[]" entries are either normalised
 * to 0..1 or report a (positive or negative) angle in degrees, with 0.0
 * representing the centre. Not all pens/backends support all axes:
 * unsupported entries are always zero.
 *
 * To convert angles for tilt and rotation into vector representation, use
 * SDL_sinf on the XTILT, YTILT, or ROTATION component, for example:
 *
 * `SDL_sinf(xtilt * SDL_PI_F / 180.0)`.
 *
 * \since This enum is available since SDL 3.0.0
 */
typedef enum SDL_PenAxis
{
    SDL_PEN_AXIS_PRESSURE,  /**< Pen pressure.  Unidirectional: 0 to 1.0 */
    SDL_PEN_AXIS_XTILT,     /**< Pen horizontal tilt angle.  Bidirectional: -90.0 to 90.0 (left-to-right).
                                 The physical max/min tilt may be smaller than -90.0 / 90.0, check SDL_PenCapabilityInfo */
    SDL_PEN_AXIS_YTILT,     /**< Pen vertical tilt angle.  Bidirectional: -90.0 to 90.0 (top-to-down).
                                 The physical max/min tilt may be smaller than -90.0 / 90.0 check SDL_PenCapabilityInfo */
    SDL_PEN_AXIS_DISTANCE,  /**< Pen distance to drawing surface.  Unidirectional: 0.0 to 1.0 */
    SDL_PEN_AXIS_ROTATION,  /**< Pen barrel rotation.  Bidirectional: -180 to 179.9 (clockwise, 0 is facing up, -180.0 is facing down). */
    SDL_PEN_AXIS_SLIDER,    /**< Pen finger wheel or slider (e.g., Airbrush Pen).  Unidirectional: 0 to 1.0 */
    SDL_PEN_NUM_AXES        /**< Total known pen axis types in this version of SDL. This number may grow in future releases! */
} SDL_PenAxis;


/**
 * Pen input flags, as reported by SDL_GetPenStatus and various events.
 *
 * The `SDL_PEN_INPUT_BUTTON_*` defines happen to match SDL_MouseButtonFlags.
 *
 * \since This datatype is available since SDL 3.0.0.
 */
typedef Uint32 SDL_PenInputFlags;
#define SDL_PEN_INPUT_BUTTON_1     1   /**< Bit index for pen button 1 is pressed */
#define SDL_PEN_INPUT_BUTTON_2     2   /**< Bit index for pen button 2 is pressed */
#define SDL_PEN_INPUT_BUTTON_3     3   /**< Bit index for pen button 3 is pressed */
#define SDL_PEN_INPUT_BUTTON_4     4   /**< Bit index for pen button 4 is pressed */
#define SDL_PEN_INPUT_BUTTON_5     5   /**< Bit index for pen button 5 is pressed */
#define SDL_PEN_INPUT_ERASER_TIP  30   /**< Bit index for pen is using eraser tip, not regular drawing tip */
#define SDL_PEN_INPUT_DOWN        29   /**< Bit index for pen is touching the surface */

#define SDL_PEN_INPUT(X)              (1u << ((X)-1))
#define SDL_PEN_INPUT_BUTTON_1_MASK   SDL_PEN_INPUT(SDL_PEN_INPUT_BUTTON_1)   /**< & to see if button 1 is pressed */
#define SDL_PEN_INPUT_BUTTON_2_MASK   SDL_PEN_INPUT(SDL_PEN_INPUT_BUTTON_2)   /**< & to see if button 2 is pressed */
#define SDL_PEN_INPUT_BUTTON_3_MASK   SDL_PEN_INPUT(SDL_PEN_INPUT_BUTTON_3)   /**< & to see if button 3 is pressed */
#define SDL_PEN_INPUT_BUTTON_4_MASK   SDL_PEN_INPUT(SDL_PEN_INPUT_BUTTON_4)   /**< & to see if button 4 is pressed */
#define SDL_PEN_INPUT_BUTTON_5_MASK   SDL_PEN_INPUT(SDL_PEN_INPUT_BUTTON_5)   /**< & to see if button 5 is pressed */
#define SDL_PEN_INPUT_ERASER_TIP_MASK SDL_PEN_INPUT(SDL_PEN_INPUT_ERASER_TIP) /**< & to see if eraser tip is used */
#define SDL_PEN_INPUT_DOWN_MASK       SDL_PEN_INPUT(SDL_PEN_INPUT_DOWN)       /**< & to see if pen is down */


/**
 * SDL_PenInfo capability flags.
 *
 * \since This datatype is available since SDL 3.0.0.
 */
typedef Uint32 SDL_PenCapabilityFlags;

#define SDL_PEN_CAPABILITY_PRESSURE  0x00000001u  /**< Pen provides pressure information in axis SDL_PEN_AXIS_PRESSURE */
#define SDL_PEN_CAPABILITY_XTILT     0x00000002u  /**< Pen provides horizontal tilt information in axis SDL_PEN_AXIS_XTILT */
#define SDL_PEN_CAPABILITY_YTILT     0x00000004u  /**< Pen provides vertical tilt information in axis SDL_PEN_AXIS_YTILT */
#define SDL_PEN_CAPABILITY_DISTANCE  0x00000008u  /**< Pen provides distance to drawing tablet in axis SDL_PEN_AXIS_DISTANCE */
#define SDL_PEN_CAPABILITY_ROTATION  0x00000010u  /**< Pen provides barrel rotation info in axis SDL_PEN_AXIS_ROTATION */
#define SDL_PEN_CAPABILITY_SLIDER    0x00000020u  /**< Pen provides slider/finger wheel/etc in axis SDL_PEN_AXIS_SLIDER */
#define SDL_PEN_CAPABILITY_ERASER    0x00000040u  /**< Pen also has an eraser tip */

#define SDL_PEN_CAPABILITY_BIDIRECTIONAL (SDL_PEN_CAPABILITY_XTILT | SDL_PEN_CAPABILITY_YTILT)  /**< for convenience */


/**
 * Specific subtypes of pen devices.
 *
 * Some pens identify as a particular type of drawing device (for example,
 * an airbrush or a pencil).
 *
 * Note that pen subtypes do not dictate whether the pen tip is
 * _ink_ or an _eraser_; to determine whether a pen is being used in
 * eraser mode, check `SDL_PenInputFlags & SDL_PEN_INPUT_ERASER_TIP_MASK`,
 * in SDL_EVENT_PEN_DOWN events, or in the return value of SDL_GetPenStatus().
 *
 * \since This enum is available since SDL 3.0.0
 */
typedef enum SDL_PenSubtype
{
    SDL_PEN_TYPE_UNKNOWN,   /**< Unknown pen device */
    SDL_PEN_TYPE_ERASER,    /**< Eraser */
    SDL_PEN_TYPE_PEN,       /**< Generic pen; this is the default. */
    SDL_PEN_TYPE_PENCIL,    /**< Pencil */
    SDL_PEN_TYPE_BRUSH,     /**< Brush-like device */
    SDL_PEN_TYPE_AIRBRUSH   /**< Airbrush device that "sprays" ink */
} SDL_PenSubtype;


/**
 * Pen hardware information, as reported by SDL_GetPenInfo()
 *
 * \since This struct is available since SDL 3.0.0.
 *
 * \sa SDL_GetPenInfo
 */
typedef struct SDL_PenInfo
{
    SDL_GUID guid;      /**< a (best-attempt) unique identifier for this hardware. */
    SDL_PenCapabilityFlags capabilities;  /**< bitflags of device capabilities */
    float max_tilt;    /**< Physical maximum tilt angle, for XTILT and YTILT, or -1.0f if unknown.  Pens cannot typically tilt all the way to 90 degrees, so this value is usually less than 90.0. */
    Uint32 wacom_id;   /**< For Wacom devices: wacom tool type ID, otherwise 0 (useful e.g. with libwacom) */
    int num_buttons; /**< Number of pen buttons (not counting the pen tip), or -1 if unknown. */
    SDL_PenSubtype subtype;  /**< type of pen device */
} SDL_PenInfo;


/**
 * Retrieves all pens that are connected to the system.
 *
 * Yields an array of SDL_PenID values. These identify and track pens
 * throughout a session. To track pens across sessions (program restart), use
 * the SDL_GUID reported by SDL_GetPenInfo().
 *
 * The returned array must be freed with SDL_free().
 *
 * \param count The number of pens in the array (number of array elements
 *              minus 1, i.e., not counting the terminator 0). Can be NULL.
 * \returns A 0 terminated array of SDL_PenID values, or NULL on error.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_PenID *SDLCALL SDL_GetPens(int *count);

/**
 * Retrieves a human-readable description for a SDL_PenID.
 *
 * The returned string is intended for human consumption. It might or
 * might not be localised, depending on platform settings. It is not
 * guaranteed to be unique; use the SDL_GUID reported by SDL_GetPenInfo()
 * for (best-effort) unique identifiers.
 *
 * The pointer is managed by the SDL pen subsystem and must not be
 * deallocated by the caller. The pointer remains valid at least until the
 * next call to SDL_PumpEvents and is definitely deallocated during SDL_Quit().
 *
 * \param instance_id The pen to query.
 * \returns A string that contains the name of the pen. Returns NULL on error.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC const char *SDLCALL SDL_GetPenName(SDL_PenID instance_id);

/**
 * Retrieves hardware details for a given SDL_PenID.
 *
 * \param instance_id The pen to query.
 * \param info Filled in with information about pen details. Can be NULL.
 * \returns zero on success, -1 on error.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC int SDLCALL SDL_GetPenInfo(SDL_PenID instance_id, SDL_PenInfo *info);

/**
 * Retrieves the pen's current status.
 *
 * If the pen is not connected, this operation may return default values.
 *
 * If `num_axes` is larger than the number of axes known to SDL, it will
 * set the extra elements to 0.0f.
 *
 * \param x Out-mode parameter for pen x coordinate. May be NULL.
 * \param y Out-mode parameter for pen y coordinate. May be NULL.
 * \param axes Out-mode parameter for axis information. May be NULL. The axes
 *             are in the same order as SDL_PenAxis.
 * \param num_axes Maximum number of axes to write to "axes".
 * \returns a SDL_PenInputFlags bitmask of the current pen input states, or 0 on error.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_PenInputFlags SDLCALL SDL_GetPenStatus(SDL_PenID instance_id, float *x, float *y, float *axes, size_t num_axes);

/**
 * Retrieves an SDL_PenID for the given SDL_GUID.
 *
 * \param guid A pen GUID.
 * \returns A valid SDL_PenID, or 0 if there is no matching SDL_PenID.
 *
 * \since This function is available since SDL 3.0.0.
 *
 * \sa SDL_GetPenInfo
 */
extern SDL_DECLSPEC SDL_PenID SDLCALL SDL_GetPenFromGUID(SDL_GUID guid);

/**
 * Checks whether a pen is still attached.
 *
 * If a pen is detached, it will not show up for SDL_GetPens(). Other
 * operations may still be available but return default values.
 *
 * \param instance_id A pen ID.
 * \returns SDL_TRUE if `instance_id` is valid and the corresponding pen is
 *          attached, or SDL_FALSE otherwise.
 *
 * \since This function is available since SDL 3.0.0.
 */
extern SDL_DECLSPEC SDL_bool SDLCALL SDL_PenConnected(SDL_PenID instance_id);

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif

#endif /* SDL_pen_h_ */

/* vi: set ts=4 sw=4 expandtab: */
