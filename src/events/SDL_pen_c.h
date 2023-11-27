/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

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

#include "../SDL_internal.h"

#ifndef SDL_pen_c_h_
#define SDL_pen_c_h_

#include "../../include/SDL3/SDL_pen.h"
#include "SDL_mouse_c.h"

/* For testing alternate code paths: */
#define SDL_PEN_DEBUG_NOID 0           /* Pretend that pen device does not supply ID / ID is some default value \
                                          affects: SDL_x11pen.c                                                 \
                                                   SDL_waylandevents.c  */
#define SDL_PEN_DEBUG_NONWACOM 0       /* Pretend that no attached device is a Wacom device \
                                          affects: SDL_x11pen.c                             \
                                                   SDL_waylandevents.c  */
#define SDL_PEN_DEBUG_UNKNOWN_WACOM 0  /* Pretend that any attached Wacom device is of an unknown make \
                                          affects: SDL_PenModifyFromWacomID() */
#define SDL_PEN_DEBUG_NOSERIAL_WACOM 0 /* Pretend that any attached Wacom device has serial number 0 \
                                          affects: SDL_x11pen.c                                      \
                                                   SDL_waylandevents.c  */

#define SDL_PEN_TYPE_NONE 0 /**< Pen type for non-pens (use to cancel pen registration) */

#define SDL_PEN_MAX_NAME 64

#define SDL_PEN_FLAG_ERROR    (1ul << 28) /* Printed an internal API usage error about this pen (used to prevent spamming) */
#define SDL_PEN_FLAG_NEW      (1ul << 29) /* Pen was registered in most recent call to SDL_PenRegisterBegin() */
#define SDL_PEN_FLAG_DETACHED (1ul << 30) /* Detached (not re-registered before last SDL_PenGCSweep()) */
#define SDL_PEN_FLAG_STALE    (1ul << 31) /* Not re-registered since last SDL_PenGCMark() */

typedef struct SDL_PenStatusInfo
{
    float x, y;
    float axes[SDL_PEN_NUM_AXES];
    Uint32 buttons; /* SDL_BUTTON(1) | SDL_BUTTON(2) | ... | SDL_PEN_DOWN_MASK */
} SDL_PenStatusInfo;

/**
 * Internal (backend driver-independent) pen representation
 *
 * Implementation-specific backend drivers may read and write most of this structure, and
 * are expected to initialise parts of it when registering a new pen.  They must not write
 * to the "header" section.
 */
typedef struct SDL_Pen
{
    /* Backend driver MUST NOT not write to: */
    struct SDL_Pen_header
    {
        SDL_PenID id;       /* id determines sort order unless SDL_PEN_FLAG_DETACHED is set */
        Uint32 flags;       /* SDL_PEN_FLAG_* | SDK_PEN_DOWN_MASK | SDL_PEN_INK_MASK | SDL_PEN_ERASER_MASK | SDL_PEN_AXIS_* */
        SDL_Window *window; /* Current SDL window for this pen, or NULL */
    } header;

    SDL_PenStatusInfo last; /* Last reported status, normally read-only for backend */

    /* Backend: MUST initialise this block when pen is first registered: */
    SDL_GUID guid; /* GUID, MUST be set by backend.
                         MUST be unique (no other pen ID with same GUID).
                         SHOULD be persistent across sessions. */

    /* Backend: SHOULD initialise this block when pen is first registered if it can
       Otherwise: Set to sane default values during SDL_PenModifyEnd() */
    SDL_PenCapabilityInfo info; /* Detail information about the pen (buttons, tilt) */
    SDL_PenSubtype type;
    Uint8 last_mouse_button;    /* For mouse button emulation: last emulated button */
    char *name;                 /* Preallocated; set via SDL_strlcpy(pen->name, src, SDL_PEN_MAX_NAME) */
                                /* We hand this exact pointer to client code, so it must not be modified after
                                   creation. */

    void *deviceinfo; /* implementation-specific information */
} SDL_Pen;

/* ---- API for backend driver only ---- */

/**
 * (Only for backend driver) Look up a pen by pen ID
 *
 * \param instance_id A Uint32 pen identifier (driver-dependent meaning).  Must not be 0 = SDL_PEN_INVALID.
 * The same ID is exposed to clients as SDL_PenID.
 *
 * The pen pointer is only valid until the next call to SDL_PenModifyEnd() or SDL_PenGCSweep()
 *
 * \return pen, if it exists, or NULL
 */
extern SDL_Pen *SDL_GetPenPtr(Uint32 instance_id);

/**
 * (Only for backend driver) Start registering a new pen or updating an existing pen.
 *
 * Acquires the pen mutex, which is held until the next call to SDL_PenModifyEnd() .
 *
 * If the PenID already exists, returns the existing entry.  Otherwise initialise fresh SDL_Pen.
 * For new pens, sets SDL_PEN_FLAG_NEW.
 *
 * Usage:
 * - SDL_PenModifyStart()
 * - update pen object, in any order:
 *     - SDL_PenModifyAddCapabilities()
 *     - pen->guid (MUST be set for new pens, e.g. via ::SDL_PenUpdateGUIDForGeneric and related operations)
 *     - pen->info.num_buttons
 *     - pen->info.max_tilt
 *     - pen->type
 *     - pen->name
 *     - pen->deviceinfo (backend-specific)
 * - SDL_PenModifyEnd()
 *
 * For new pens, sets defaults for:
 *   - num_buttons (SDL_PEN_INFO_UNKNOWN)
 *   - max_tilt (SDL_PEN_INFO_UNKNOWN)
 *   - pen_type (SDL_PEN_TYPE_PEN)
 *   - Zeroes all other (non-header) fields
 *
 * \param instance_id Pen ID to allocate (must not be 0 = SDL_PEN_ID_INVALID)
 * \returns SDL_Pen pointer; only valid until the call to SDL_PenModifyEnd()
 */
extern SDL_Pen *SDL_PenModifyBegin(Uint32 instance_id);

/**
 * (Only for backend driver) Add capabilities to a pen (cf. SDL_PenModifyBegin()).
 *
 * Adds capabilities to a pen obtained via SDL_PenModifyBegin().  Can be called more than once.
 *
 * \param pen The pen to update
 * \param capabilities Capabilities flags, out of: SDL_PEN_AXIS_*, SDL_PEN_ERASER_MASK, SDL_PEN_INK_MASK
 *     Setting SDL_PEN_ERASER_MASK will clear SDL_PEN_INK_MASK, and vice versa.
 */
extern void SDL_PenModifyAddCapabilities(SDL_Pen *pen, Uint32 capabilities);

/**
 * Set up a pen structure for a Wacom device.
 *
 * Some backends (e.g., XInput2, Wayland) can only partially identify the capabilities of a given
 * pen but can identify Wacom pens and obtain their Wacom-specific device type identifiers.
 * This function partly automates device setup in those cases.
 *
 * This function does NOT set up the pen's GUID.  Use ::SD_PenModifyGUIDForWacom instead.
 *
 * This function does NOT call SDL_PenModifyAddCapabilities() ifself, since some backends may
 * not have access to all pen axes (e.g., Xinput2).
 *
 * \param pen The pen to initialise
 * \param wacom_devicetype_id The Wacom-specific device type identifier
 * \param[out] axis_flags The set of physically supported axes for this pen, suitable for passing to
 *    SDL_PenModifyAddCapabilities()
 *
 * \returns SDL_TRUE if the device ID could be identified, otherwise SDL_FALSE
 */
extern int SDL_PenModifyForWacomID(SDL_Pen *pen, Uint32 wacom_devicetype_id, Uint32 *axis_flags);

/**
 * Updates a GUID for a generic pen device.
 *
 * Assumes that the GUID has been pre-initialised (typically to 0).
 * Idempotent, and commutative with ::SDL_PenUpdateGUIDForWacom and ::SDL_PenUpdateGUIDForType
 *
 * \param[out] guid The GUID to update
 * \param upper Upper half of the device ID (assume lower entropy than "lower"; pass 0 if not available)
 * \param lower Lower half of the device ID (assume higher entropy than "upper")
 */
extern void SDL_PenUpdateGUIDForGeneric(SDL_GUID *guid, Uint32 upper, Uint32 lower);

/**
 * Updates a GUID based on a pen type
 *
 * Assumes that the GUID has been pre-initialised (typically to 0).
 * Idempotent, and commutative with ::SDL_PenUpdateGUIDForWacom and ::SDL_PenUpdateGUIDForGeneric
 *
 * \param[out] guid The GUID to update
 * \param pentype The pen type to insert
 */
extern void SDL_PenUpdateGUIDForType(SDL_GUID *guid, SDL_PenSubtype pentype);

/**
 * Updates a GUID for a Wacom pen device.
 *
 * Assumes that the GUID has been pre-initialised (typically to 0).
 * Idempotent, and commutative with ::SDL_PenUpdateGUIDForType and ::SDL_PenUpdateGUIDForGeneric
 *
 * This update is identical to the one written by ::SDL_PenModifyFromWacomID .
 *
 * \param[out] guid The GUID to update
 * \param wacom_devicetype_id The Wacom-specific device type identifier
 * \param wacom_serial_id The Wacom-specific serial number
 */
extern void SDL_PenUpdateGUIDForWacom(SDL_GUID *guid, Uint32 wacom_devicetype_id, Uint32 wacom_serial_id);

/**
 * (Only for backend driver) Finish updating a pen.
 *
 * Releases the pen mutex acquired by SDL_PenModifyBegin() .
 *
 * If pen->type == SDL_PEN_TYPE_NONE, removes the pen entirely (only
 * for new pens).  This allows backends to start registering a
 * potential pen device and to abort if the device turns out to not be
 * a pen.
 *
 * For new pens, this call will also set the following:
 *   - name (default name, if not yet set)
 *
 * \param pen The pen to register.  That pointer is no longer valid after this call.
 * \param attach Whether the pen should be attached (SDL_TRUE) or detached (SDL_FALSE).
 *
 * If the pen is detached or removed, it is the caller's responsibility to free
 * and null "deviceinfo".
 */
extern void SDL_PenModifyEnd(SDL_Pen *pen, SDL_bool attach);

/**
 * (Only for backend driver) Mark all current pens for garbage collection.
 *
 * Must not be called while the pen mutex is held (by SDL_PenModifyBegin() ).
 *
 * SDL_PenGCMark() / SDL_PenGCSweep() provide a simple mechanism for
 * detaching all known pens that are not discoverable.  This allows
 * backends to use the same code for pen discovery and for
 * hotplugging:
 *
 *  - SDL_PenGCMark() and start backend-specific discovery
 *  - for each discovered pen: SDL_PenModifyBegin() + SDL_PenModifyEnd() (this will retain existing state)
 *  - SDL_PenGCSweep()  (will now detach all pens that were not re-registered).
 */
extern void SDL_PenGCMark(void);

/**
 * (Only for backend driver) Detach pens that haven't been reported attached since the last call to SDL_PenGCMark().
 *
 * Must not be called while the pen mutex is held (by SDL_PenModifyBegin() ).
 *
 * See SDL_PenGCMark() for details.
 *
 * \param context Extra parameter to pass through to "free_deviceinfo"
 * \param free_deviceinfo Operation to call on any non-NULL "backend.deviceinfo".
 *
 * \sa SDL_PenGCMark()
 */
extern void SDL_PenGCSweep(void *context, void (*free_deviceinfo)(Uint32 instance_id, void *deviceinfo, void *context));

/**
 * (Only for backend driver) Send a pen motion event.
 *
 * Suppresses pen motion events that do not change the current pen state.
 * May also send a mouse motion event, if mouse emulation is enabled and the pen position has
 * changed sufficiently for the motion to be visible to mouse event listeners.
 *
 * \param timestamp Event timestamp in nanoseconds, or 0 to ask SDL to use SDL_GetTicksNS() .
 *        While 0 is safe to report, your backends may be able to  report more precise
 *        timing information.
 *        Keep in mind that you should never report timestamps that are greater than
 *        SDL_GetTicksNS() . In particular, SDL_GetTicksNS() reports nanoseconds since the start
 *        of the SDL session, and your backend may use a different starting point as "timestamp zero".
 * \param instance_id Pen
 * \param window_relative Coordinates are already window-relative
 * \param status Coordinates and axes (buttons are ignored)
 *
 * \returns SDL_TRUE if at least one event was sent
 */
extern int SDL_SendPenMotion(Uint64 timestamp, SDL_PenID instance_id, SDL_bool window_relative, const SDL_PenStatusInfo *status);

/**
 * (Only for backend driver) Send a pen button event
 *
 * \param timestamp Event timestamp in nanoseconds, or 0 to ask SDL to use SDL_GetTicksNS() .
 *        See SDL_SendPenMotion() for a discussion about how to handle timestamps.
 * \param instance_id Pen
 * \param state SDL_PRESSED or SDL_RELEASED
 * \param button Button number: 1 (first physical button) etc.
 *
 * \returns SDL_TRUE if at least one event was sent
 */
extern int SDL_SendPenButton(Uint64 timestamp, SDL_PenID instance_id, Uint8 state, Uint8 button);

/**
 * (Only for backend driver) Send a pen tip event (touching or no longer touching the surface)
 *
 * Note: the backend should perform hit testing on window decoration elements to allow the pen
 * to e.g. resize/move the window, just as for mouse events, unless ::SDL_SendPenTipEvent is false.
 *
 * \param timestamp Event timestamp in nanoseconds, or 0 to ask SDL to use SDL_GetTicksNS() .
 *        See SDL_SendPenMotion() for a discussion about how to handle timestamps.
 * \param instance_id Pen
 * \param state SDL_PRESSED (for PEN_DOWN) or SDL_RELEASED (for PEN_UP)
 *
 * \returns SDL_TRUE if at least one event was sent
 */
extern int SDL_SendPenTipEvent(Uint64 timestamp, SDL_PenID instance_id, Uint8 state);

/**
 * (Only for backend driver) Check if a PEN_DOWN event should perform hit box testing.
 *
 * \returns SDL_TRUE if and only if the backend should perform hit testing
 */
extern SDL_bool SDL_PenPerformHitTest(void);

/**
 * (Only for backend driver) Send a pen window event.
 *
 * Tracks when a pen has entered/left a window.
 * Don't call this when reporting new pens or removing known pens; those cases are handled automatically.
 *
 * \param timestamp Event timestamp in nanoseconds, or 0 to ask SDL to use SDL_GetTicksNS() .
 *        See SDL_SendPenMotion() for a discussion about how to handle timestamps.
 * \param instance_id Pen
 * \param window Window to enter, or NULL to exit
 */
extern int SDL_SendPenWindowEvent(Uint64 timestamp, SDL_PenID instance_id, SDL_Window *window);

/**
 * Initialises the pen subsystem.
 */
extern void SDL_PenInit(void);

/**
 * De-initialises the pen subsystem.
 */
extern void SDL_PenQuit(void);

#endif /* SDL_pen_c_h_ */

/* vi: set ts=4 sw=4 expandtab: */
