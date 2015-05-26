/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

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

/* This is the joystick API for Simple DirectMedia Layer */

#include "SDL.h"
#include "SDL_events.h"
#include "SDL_sysjoystick.h"
#include "SDL_assert.h"
#include "SDL_hints.h"

#if !SDL_EVENTS_DISABLED
#include "../events/SDL_events_c.h"
#endif

static SDL_bool SDL_joystick_allows_background_events = SDL_FALSE;
static SDL_Joystick *SDL_joysticks = NULL;
static SDL_Joystick *SDL_updating_joystick = NULL;

static void
SDL_JoystickAllowBackgroundEventsChanged(void *userdata, const char *name, const char *oldValue, const char *hint)
{
    if (hint && *hint == '1') {
        SDL_joystick_allows_background_events = SDL_TRUE;
    } else {
        SDL_joystick_allows_background_events = SDL_FALSE;
    }
}

int
SDL_JoystickInit(void)
{
    int status;

    /* See if we should allow joystick events while in the background */
    SDL_AddHintCallback(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
                        SDL_JoystickAllowBackgroundEventsChanged, NULL);

#if !SDL_EVENTS_DISABLED
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) < 0) {
        return -1;
    }
#endif /* !SDL_EVENTS_DISABLED */

    status = SDL_SYS_JoystickInit();
    if (status >= 0) {
        status = 0;
    }
    return (status);
}

/*
 * Count the number of joysticks attached to the system
 */
int
SDL_NumJoysticks(void)
{
    return SDL_SYS_NumJoysticks();
}

/*
 * Get the implementation dependent name of a joystick
 */
const char *
SDL_JoystickNameForIndex(int device_index)
{
    if ((device_index < 0) || (device_index >= SDL_NumJoysticks())) {
        SDL_SetError("There are %d joysticks available", SDL_NumJoysticks());
        return (NULL);
    }
    return (SDL_SYS_JoystickNameForDeviceIndex(device_index));
}

/*
 * Open a joystick for use - the index passed as an argument refers to
 * the N'th joystick on the system.  This index is the value which will
 * identify this joystick in future joystick events.
 *
 * This function returns a joystick identifier, or NULL if an error occurred.
 */
SDL_Joystick *
SDL_JoystickOpen(int device_index)
{
    SDL_Joystick *joystick;
    SDL_Joystick *joysticklist;
    const char *joystickname = NULL;

    if ((device_index < 0) || (device_index >= SDL_NumJoysticks())) {
        SDL_SetError("There are %d joysticks available", SDL_NumJoysticks());
        return (NULL);
    }

    joysticklist = SDL_joysticks;
    /* If the joystick is already open, return it
    * it is important that we have a single joystick * for each instance id
    */
    while (joysticklist) {
        if (SDL_SYS_GetInstanceIdOfDeviceIndex(device_index) == joysticklist->instance_id) {
                joystick = joysticklist;
                ++joystick->ref_count;
                return (joystick);
        }
        joysticklist = joysticklist->next;
    }

    /* Create and initialize the joystick */
    joystick = (SDL_Joystick *) SDL_malloc((sizeof *joystick));
    if (joystick == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    SDL_memset(joystick, 0, (sizeof *joystick));
    if (SDL_SYS_JoystickOpen(joystick, device_index) < 0) {
        SDL_free(joystick);
        return NULL;
    }

    joystickname = SDL_SYS_JoystickNameForDeviceIndex(device_index);
    if (joystickname)
        joystick->name = SDL_strdup(joystickname);
    else
        joystick->name = NULL;

    if (joystick->naxes > 0) {
        joystick->axes = (Sint16 *) SDL_malloc
            (joystick->naxes * sizeof(Sint16));
    }
    if (joystick->nhats > 0) {
        joystick->hats = (Uint8 *) SDL_malloc
            (joystick->nhats * sizeof(Uint8));
    }
    if (joystick->nballs > 0) {
        joystick->balls = (struct balldelta *) SDL_malloc
            (joystick->nballs * sizeof(*joystick->balls));
    }
    if (joystick->nbuttons > 0) {
        joystick->buttons = (Uint8 *) SDL_malloc
            (joystick->nbuttons * sizeof(Uint8));
    }
    if (((joystick->naxes > 0) && !joystick->axes)
        || ((joystick->nhats > 0) && !joystick->hats)
        || ((joystick->nballs > 0) && !joystick->balls)
        || ((joystick->nbuttons > 0) && !joystick->buttons)) {
        SDL_OutOfMemory();
        SDL_JoystickClose(joystick);
        return NULL;
    }
    if (joystick->axes) {
        SDL_memset(joystick->axes, 0, joystick->naxes * sizeof(Sint16));
    }
    if (joystick->hats) {
        SDL_memset(joystick->hats, 0, joystick->nhats * sizeof(Uint8));
    }
    if (joystick->balls) {
        SDL_memset(joystick->balls, 0,
            joystick->nballs * sizeof(*joystick->balls));
    }
    if (joystick->buttons) {
        SDL_memset(joystick->buttons, 0, joystick->nbuttons * sizeof(Uint8));
    }

    /* Add joystick to list */
    ++joystick->ref_count;
    /* Link the joystick in the list */
    joystick->next = SDL_joysticks;
    SDL_joysticks = joystick;

    SDL_SYS_JoystickUpdate(joystick);

    return (joystick);
}


/*
 * Checks to make sure the joystick is valid.
 */
int
SDL_PrivateJoystickValid(SDL_Joystick * joystick)
{
    int valid;

    if (joystick == NULL) {
        SDL_SetError("Joystick hasn't been opened yet");
        valid = 0;
    } else {
        valid = 1;
    }

    return valid;
}

/*
 * Get the number of multi-dimensional axis controls on a joystick
 */
int
SDL_JoystickNumAxes(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return (-1);
    }
    return (joystick->naxes);
}

/*
 * Get the number of hats on a joystick
 */
int
SDL_JoystickNumHats(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return (-1);
    }
    return (joystick->nhats);
}

/*
 * Get the number of trackballs on a joystick
 */
int
SDL_JoystickNumBalls(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return (-1);
    }
    return (joystick->nballs);
}

/*
 * Get the number of buttons on a joystick
 */
int
SDL_JoystickNumButtons(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return (-1);
    }
    return (joystick->nbuttons);
}

/*
 * Get the current state of an axis control on a joystick
 */
Sint16
SDL_JoystickGetAxis(SDL_Joystick * joystick, int axis)
{
    Sint16 state;

    if (!SDL_PrivateJoystickValid(joystick)) {
        return (0);
    }
    if (axis < joystick->naxes) {
        state = joystick->axes[axis];
    } else {
        SDL_SetError("Joystick only has %d axes", joystick->naxes);
        state = 0;
    }
    return (state);
}

/*
 * Get the current state of a hat on a joystick
 */
Uint8
SDL_JoystickGetHat(SDL_Joystick * joystick, int hat)
{
    Uint8 state;

    if (!SDL_PrivateJoystickValid(joystick)) {
        return (0);
    }
    if (hat < joystick->nhats) {
        state = joystick->hats[hat];
    } else {
        SDL_SetError("Joystick only has %d hats", joystick->nhats);
        state = 0;
    }
    return (state);
}

/*
 * Get the ball axis change since the last poll
 */
int
SDL_JoystickGetBall(SDL_Joystick * joystick, int ball, int *dx, int *dy)
{
    int retval;

    if (!SDL_PrivateJoystickValid(joystick)) {
        return (-1);
    }

    retval = 0;
    if (ball < joystick->nballs) {
        if (dx) {
            *dx = joystick->balls[ball].dx;
        }
        if (dy) {
            *dy = joystick->balls[ball].dy;
        }
        joystick->balls[ball].dx = 0;
        joystick->balls[ball].dy = 0;
    } else {
        return SDL_SetError("Joystick only has %d balls", joystick->nballs);
    }
    return (retval);
}

/*
 * Get the current state of a button on a joystick
 */
Uint8
SDL_JoystickGetButton(SDL_Joystick * joystick, int button)
{
    Uint8 state;

    if (!SDL_PrivateJoystickValid(joystick)) {
        return (0);
    }
    if (button < joystick->nbuttons) {
        state = joystick->buttons[button];
    } else {
        SDL_SetError("Joystick only has %d buttons", joystick->nbuttons);
        state = 0;
    }
    return (state);
}

/*
 * Return if the joystick in question is currently attached to the system,
 *  \return SDL_FALSE if not plugged in, SDL_TRUE if still present.
 */
SDL_bool
SDL_JoystickGetAttached(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return SDL_FALSE;
    }

    return SDL_SYS_JoystickAttached(joystick);
}

/*
 * Get the instance id for this opened joystick
 */
SDL_JoystickID
SDL_JoystickInstanceID(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return (-1);
    }

    return (joystick->instance_id);
}

/*
 * Get the friendly name of this joystick
 */
const char *
SDL_JoystickName(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        return (NULL);
    }

    return (joystick->name);
}

/*
 * Close a joystick previously opened with SDL_JoystickOpen()
 */
void
SDL_JoystickClose(SDL_Joystick * joystick)
{
    SDL_Joystick *joysticklist;
    SDL_Joystick *joysticklistprev;

    if (!joystick) {
        return;
    }

    /* First decrement ref count */
    if (--joystick->ref_count > 0) {
        return;
    }

    if (joystick == SDL_updating_joystick) {
        return;
    }

    SDL_SYS_JoystickClose(joystick);
    joystick->hwdata = NULL;

    joysticklist = SDL_joysticks;
    joysticklistprev = NULL;
    while (joysticklist) {
        if (joystick == joysticklist) {
            if (joysticklistprev) {
                /* unlink this entry */
                joysticklistprev->next = joysticklist->next;
            } else {
                SDL_joysticks = joystick->next;
            }
            break;
        }
        joysticklistprev = joysticklist;
        joysticklist = joysticklist->next;
    }

    SDL_free(joystick->name);

    /* Free the data associated with this joystick */
    SDL_free(joystick->axes);
    SDL_free(joystick->hats);
    SDL_free(joystick->balls);
    SDL_free(joystick->buttons);
    SDL_free(joystick);
}

void
SDL_JoystickQuit(void)
{
    /* Make sure we're not getting called in the middle of updating joysticks */
    SDL_assert(!SDL_updating_joystick);

    /* Stop the event polling */
    while (SDL_joysticks) {
        SDL_joysticks->ref_count = 1;
        SDL_JoystickClose(SDL_joysticks);
    }

    /* Quit the joystick setup */
    SDL_SYS_JoystickQuit();

#if !SDL_EVENTS_DISABLED
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
#endif
}


static SDL_bool
SDL_PrivateJoystickShouldIgnoreEvent()
{
    if (SDL_joystick_allows_background_events) {
        return SDL_FALSE;
    }

    if (SDL_WasInit(SDL_INIT_VIDEO)) {
        if (SDL_GetKeyboardFocus() == NULL) {
            /* Video is initialized and we don't have focus, ignore the event. */
            return SDL_TRUE;
        } else {
            return SDL_FALSE;
        }
    }

    /* Video subsystem wasn't initialized, always allow the event */
    return SDL_FALSE;
}

/* These are global for SDL_sysjoystick.c and SDL_events.c */

int
SDL_PrivateJoystickAxis(SDL_Joystick * joystick, Uint8 axis, Sint16 value)
{
    int posted;

    /* Make sure we're not getting garbage or duplicate events */
    if (axis >= joystick->naxes) {
        return 0;
    }
    if (value == joystick->axes[axis]) {
        return 0;
    }

    /* We ignore events if we don't have keyboard focus, except for centering
     * events.
     */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        if ((value > 0 && value >= joystick->axes[axis]) ||
            (value < 0 && value <= joystick->axes[axis])) {
            return 0;
        }
    }

    /* Update internal joystick state */
    joystick->axes[axis] = value;

    /* Post the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_GetEventState(SDL_JOYAXISMOTION) == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_JOYAXISMOTION;
        event.jaxis.which = joystick->instance_id;
        event.jaxis.axis = axis;
        event.jaxis.value = value;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}

int
SDL_PrivateJoystickHat(SDL_Joystick * joystick, Uint8 hat, Uint8 value)
{
    int posted;

    /* Make sure we're not getting garbage or duplicate events */
    if (hat >= joystick->nhats) {
        return 0;
    }
    if (value == joystick->hats[hat]) {
        return 0;
    }

    /* We ignore events if we don't have keyboard focus, except for centering
     * events.
     */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        if (value != SDL_HAT_CENTERED) {
            return 0;
        }
    }

    /* Update internal joystick state */
    joystick->hats[hat] = value;

    /* Post the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_GetEventState(SDL_JOYHATMOTION) == SDL_ENABLE) {
        SDL_Event event;
        event.jhat.type = SDL_JOYHATMOTION;
        event.jhat.which = joystick->instance_id;
        event.jhat.hat = hat;
        event.jhat.value = value;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}

int
SDL_PrivateJoystickBall(SDL_Joystick * joystick, Uint8 ball,
                        Sint16 xrel, Sint16 yrel)
{
    int posted;

    /* Make sure we're not getting garbage events */
    if (ball >= joystick->nballs) {
        return 0;
    }

    /* We ignore events if we don't have keyboard focus. */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        return 0;
    }

    /* Update internal mouse state */
    joystick->balls[ball].dx += xrel;
    joystick->balls[ball].dy += yrel;

    /* Post the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_GetEventState(SDL_JOYBALLMOTION) == SDL_ENABLE) {
        SDL_Event event;
        event.jball.type = SDL_JOYBALLMOTION;
        event.jball.which = joystick->instance_id;
        event.jball.ball = ball;
        event.jball.xrel = xrel;
        event.jball.yrel = yrel;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}

int
SDL_PrivateJoystickButton(SDL_Joystick * joystick, Uint8 button, Uint8 state)
{
    int posted;
#if !SDL_EVENTS_DISABLED
    SDL_Event event;

    switch (state) {
    case SDL_PRESSED:
        event.type = SDL_JOYBUTTONDOWN;
        break;
    case SDL_RELEASED:
        event.type = SDL_JOYBUTTONUP;
        break;
    default:
        /* Invalid state -- bail */
        return (0);
    }
#endif /* !SDL_EVENTS_DISABLED */

    /* Make sure we're not getting garbage or duplicate events */
    if (button >= joystick->nbuttons) {
        return 0;
	}
	if (state == joystick->buttons[button]) {
		return 0;
	}

    /* We ignore events if we don't have keyboard focus, except for button
     * release. */
    if (SDL_PrivateJoystickShouldIgnoreEvent()) {
        if (state == SDL_PRESSED) {
            return 0;
        }
    }

    /* Update internal joystick state */
    joystick->buttons[button] = state;

    /* Post the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_GetEventState(event.type) == SDL_ENABLE) {
        event.jbutton.which = joystick->instance_id;
        event.jbutton.button = button;
        event.jbutton.state = state;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}

void
SDL_JoystickUpdate(void)
{
    SDL_Joystick *joystick;

    joystick = SDL_joysticks;
    while (joystick) {
        SDL_Joystick *joysticknext;
        /* save off the next pointer, the Update call may cause a joystick removed event
         * and cause our joystick pointer to be freed
         */
        joysticknext = joystick->next;

        SDL_updating_joystick = joystick;

        SDL_SYS_JoystickUpdate(joystick);

        if (joystick->force_recentering) {
            int i;

            /* Tell the app that everything is centered/unpressed...  */
            for (i = 0; i < joystick->naxes; i++)
                SDL_PrivateJoystickAxis(joystick, i, 0);

            for (i = 0; i < joystick->nbuttons; i++)
                SDL_PrivateJoystickButton(joystick, i, 0);

            for (i = 0; i < joystick->nhats; i++)
                SDL_PrivateJoystickHat(joystick, i, SDL_HAT_CENTERED);

            joystick->force_recentering = SDL_FALSE;
        }

        SDL_updating_joystick = NULL;

        /* If the joystick was closed while updating, free it here */
        if (joystick->ref_count <= 0) {
            SDL_JoystickClose(joystick);
        }

        joystick = joysticknext;
    }

    /* this needs to happen AFTER walking the joystick list above, so that any
       dangling hardware data from removed devices can be free'd
     */
    SDL_SYS_JoystickDetect();
}

int
SDL_JoystickEventState(int state)
{
#if SDL_EVENTS_DISABLED
    return SDL_DISABLE;
#else
    const Uint32 event_list[] = {
        SDL_JOYAXISMOTION, SDL_JOYBALLMOTION, SDL_JOYHATMOTION,
        SDL_JOYBUTTONDOWN, SDL_JOYBUTTONUP, SDL_JOYDEVICEADDED, SDL_JOYDEVICEREMOVED
    };
    unsigned int i;

    switch (state) {
    case SDL_QUERY:
        state = SDL_DISABLE;
        for (i = 0; i < SDL_arraysize(event_list); ++i) {
            state = SDL_EventState(event_list[i], SDL_QUERY);
            if (state == SDL_ENABLE) {
                break;
            }
        }
        break;
    default:
        for (i = 0; i < SDL_arraysize(event_list); ++i) {
            SDL_EventState(event_list[i], state);
        }
        break;
    }
    return (state);
#endif /* SDL_EVENTS_DISABLED */
}

/* return the guid for this index */
SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int device_index)
{
    if ((device_index < 0) || (device_index >= SDL_NumJoysticks())) {
        SDL_JoystickGUID emptyGUID;
        SDL_SetError("There are %d joysticks available", SDL_NumJoysticks());
        SDL_zero(emptyGUID);
        return emptyGUID;
    }
    return SDL_SYS_JoystickGetDeviceGUID(device_index);
}

/* return the guid for this opened device */
SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick * joystick)
{
    if (!SDL_PrivateJoystickValid(joystick)) {
        SDL_JoystickGUID emptyGUID;
        SDL_zero(emptyGUID);
        return emptyGUID;
    }
    return SDL_SYS_JoystickGetGUID(joystick);
}

/* convert the guid to a printable string */
void SDL_JoystickGetGUIDString(SDL_JoystickGUID guid, char *pszGUID, int cbGUID)
{
    static const char k_rgchHexToASCII[] = "0123456789abcdef";
    int i;

    if ((pszGUID == NULL) || (cbGUID <= 0)) {
        return;
    }

    for (i = 0; i < sizeof(guid.data) && i < (cbGUID-1)/2; i++) {
        /* each input byte writes 2 ascii chars, and might write a null byte. */
        /* If we don't have room for next input byte, stop */
        unsigned char c = guid.data[i];

        *pszGUID++ = k_rgchHexToASCII[c >> 4];
        *pszGUID++ = k_rgchHexToASCII[c & 0x0F];
    }
    *pszGUID = '\0';
}


/*-----------------------------------------------------------------------------
 * Purpose: Returns the 4 bit nibble for a hex character
 * Input  : c -
 * Output : unsigned char
 *-----------------------------------------------------------------------------*/
static unsigned char nibble(char c)
{
    if ((c >= '0') && (c <= '9')) {
        return (unsigned char)(c - '0');
    }

    if ((c >= 'A') && (c <= 'F')) {
        return (unsigned char)(c - 'A' + 0x0a);
    }

    if ((c >= 'a') && (c <= 'f')) {
        return (unsigned char)(c - 'a' + 0x0a);
    }

    /* received an invalid character, and no real way to return an error */
    /* AssertMsg1(false, "Q_nibble invalid hex character '%c' ", c); */
    return 0;
}


/* convert the string version of a joystick guid to the struct */
SDL_JoystickGUID SDL_JoystickGetGUIDFromString(const char *pchGUID)
{
    SDL_JoystickGUID guid;
    int maxoutputbytes= sizeof(guid);
    size_t len = SDL_strlen(pchGUID);
    Uint8 *p;
    size_t i;

    /* Make sure it's even */
    len = (len) & ~0x1;

    SDL_memset(&guid, 0x00, sizeof(guid));

    p = (Uint8 *)&guid;
    for (i = 0; (i < len) && ((p - (Uint8 *)&guid) < maxoutputbytes); i+=2, p++) {
        *p = (nibble(pchGUID[i]) << 4) | nibble(pchGUID[i+1]);
    }

    return guid;
}


/* vi: set ts=4 sw=4 expandtab: */
