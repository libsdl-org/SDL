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

#ifdef SDL_JOYSTICK_EMSCRIPTEN

#include <stdio.h>              /* For the definition of NULL */
#include "SDL_error.h"
#include "SDL_events.h"

#if !SDL_EVENTS_DISABLED
#include "../../events/SDL_events_c.h"
#endif

#include "SDL_joystick.h"
#include "SDL_hints.h"
#include "SDL_assert.h"
#include "SDL_timer.h"
#include "SDL_log.h"
#include "SDL_sysjoystick_c.h"
#include "../SDL_joystick_c.h"

static SDL_joylist_item * JoystickByIndex(int index);

static SDL_joylist_item *SDL_joylist = NULL;
static SDL_joylist_item *SDL_joylist_tail = NULL;
static int numjoysticks = 0;
static int instance_counter = 0;

EM_BOOL
Emscripten_JoyStickConnected(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData)
{
    int i;

    SDL_joylist_item *item;

    if (JoystickByIndex(gamepadEvent->index) != NULL) {
      return 1;
    }

#if !SDL_EVENTS_DISABLED
    SDL_Event event;
#endif

    item = (SDL_joylist_item *) SDL_malloc(sizeof (SDL_joylist_item));
    if (item == NULL) {
        return 1;
    }

    SDL_zerop(item);
    item->index = gamepadEvent->index;

    item->name = SDL_strdup(gamepadEvent->id);
    if ( item->name == NULL ) {
        SDL_free(item);
        return 1;
    }

    item->mapping = SDL_strdup(gamepadEvent->mapping);
    if ( item->mapping == NULL ) {
        SDL_free(item->name);
        SDL_free(item);
        return 1;
    }

    item->naxes = gamepadEvent->numAxes;
    item->nbuttons = gamepadEvent->numButtons;
    item->device_instance = instance_counter++;

    item->timestamp = gamepadEvent->timestamp;

    for( i = 0; i < item->naxes; i++) {
        item->axis[i] = gamepadEvent->axis[i];
    }

    for( i = 0; i < item->nbuttons; i++) {
        item->analogButton[i] = gamepadEvent->analogButton[i];
        item->digitalButton[i] = gamepadEvent->digitalButton[i];
    }

    if (SDL_joylist_tail == NULL) {
        SDL_joylist = SDL_joylist_tail = item;
    } else {
        SDL_joylist_tail->next = item;
        SDL_joylist_tail = item;
    }

    ++numjoysticks;
#ifdef DEBUG_JOYSTICK
    SDL_Log("Number of joysticks is %d", numjoysticks);
#endif
#if !SDL_EVENTS_DISABLED
    event.type = SDL_JOYDEVICEADDED;

    if (SDL_GetEventState(event.type) == SDL_ENABLE) {
        event.jdevice.which = numjoysticks - 1;
        if ( (SDL_EventOK == NULL) ||
             (*SDL_EventOK) (SDL_EventOKParam, &event) ) {
            SDL_PushEvent(&event);
        }
    }
#endif /* !SDL_EVENTS_DISABLED */

#ifdef DEBUG_JOYSTICK
    SDL_Log("Added joystick with index %d", item->index);
#endif

    return 1;
}

EM_BOOL
Emscripten_JoyStickDisconnected(int eventType, const EmscriptenGamepadEvent *gamepadEvent, void *userData)
{
    SDL_joylist_item *item = SDL_joylist;
    SDL_joylist_item *prev = NULL;
#if !SDL_EVENTS_DISABLED
    SDL_Event event;
#endif

    while (item != NULL) {
        if (item->index == gamepadEvent->index) {
            break;
        }
        prev = item;
        item = item->next;
    }

    if (item == NULL) {
        return 1;
    }

    if (item->joystick) {
        item->joystick->hwdata = NULL;
    }

    if (prev != NULL) {
        prev->next = item->next;
    } else {
        SDL_assert(SDL_joylist == item);
        SDL_joylist = item->next;
    }
    if (item == SDL_joylist_tail) {
        SDL_joylist_tail = prev;
    }

    /* Need to decrement the joystick count before we post the event */
    --numjoysticks;

#if !SDL_EVENTS_DISABLED
    event.type = SDL_JOYDEVICEREMOVED;

    if (SDL_GetEventState(event.type) == SDL_ENABLE) {
        event.jdevice.which = item->device_instance;
        if ( (SDL_EventOK == NULL) ||
             (*SDL_EventOK) (SDL_EventOKParam, &event) ) {
            SDL_PushEvent(&event);
        }
    }
#endif /* !SDL_EVENTS_DISABLED */

#ifdef DEBUG_JOYSTICK
    SDL_Log("Removed joystick with id %d", item->device_instance);
#endif
    SDL_free(item->name);
    SDL_free(item->mapping);
    SDL_free(item);
    return 1;
}

/* Function to scan the system for joysticks.
 * It should return 0, or -1 on an unrecoverable fatal error.
 */
int
SDL_SYS_JoystickInit(void)
{
    int retval, i, numjs;
    EmscriptenGamepadEvent gamepadState;

    numjoysticks = 0;
    numjs = emscripten_get_num_gamepads();

    /* Check if gamepad is supported by browser */
    if (numjs == EMSCRIPTEN_RESULT_NOT_SUPPORTED) {
        return -1;
    }

    /* handle already connected gamepads */
    if (numjs > 0) {
        for(i = 0; i < numjs; i++) {
            retval = emscripten_get_gamepad_status(i, &gamepadState);
            if (retval == EMSCRIPTEN_RESULT_SUCCESS) {
                Emscripten_JoyStickConnected(EMSCRIPTEN_EVENT_GAMEPADCONNECTED,
                                             &gamepadState,
                                             NULL);
            }
        }
    }

    retval = emscripten_set_gamepadconnected_callback(NULL,
                                                      0,
                                                      Emscripten_JoyStickConnected);

    if(retval != EMSCRIPTEN_RESULT_SUCCESS) {
        SDL_SYS_JoystickQuit();
        return -1;
    }

    retval = emscripten_set_gamepaddisconnected_callback(NULL,
                                                         0,
                                                         Emscripten_JoyStickDisconnected);
    if(retval != EMSCRIPTEN_RESULT_SUCCESS) {
        SDL_SYS_JoystickQuit();
        return -1;
    }

    return 0;
}

/* Returns item matching given SDL device index. */
static SDL_joylist_item *
JoystickByDeviceIndex(int device_index)
{
    SDL_joylist_item *item = SDL_joylist;

    while (0 < device_index) {
        --device_index;
        item = item->next;
    }

    return item;
}

/* Returns item matching given HTML gamepad index. */
static SDL_joylist_item *
JoystickByIndex(int index)
{
    SDL_joylist_item *item = SDL_joylist;

    if (index < 0) {
        return NULL;
    }

    while (item != NULL) {
        if (item->index == index) {
            break;
        }
        item = item->next;
    }

    return item;
}

int SDL_SYS_NumJoysticks()
{
    return numjoysticks;
}

void SDL_SYS_JoystickDetect()
{
}

/* Function to get the device-dependent name of a joystick */
const char *
SDL_SYS_JoystickNameForDeviceIndex(int device_index)
{
    return JoystickByDeviceIndex(device_index)->name;
}

/* Function to perform the mapping from device index to the instance id for this index */
SDL_JoystickID SDL_SYS_GetInstanceIdOfDeviceIndex(int device_index)
{
    return JoystickByDeviceIndex(device_index)->device_instance;
}

/* Function to open a joystick for use.
   The joystick to open is specified by the device index.
   This should fill the nbuttons and naxes fields of the joystick structure.
   It returns 0, or -1 if there is an error.
 */
int
SDL_SYS_JoystickOpen(SDL_Joystick * joystick, int device_index)
{
    SDL_joylist_item *item = JoystickByDeviceIndex(device_index);

    if (item == NULL ) {
        return SDL_SetError("No such device");
    }

    if (item->joystick != NULL) {
        return SDL_SetError("Joystick already opened");
    }

    joystick->instance_id = item->device_instance;
    joystick->hwdata = (struct joystick_hwdata *) item;
    item->joystick = joystick;

    /* HTML5 Gamepad API doesn't say anything about these */
    joystick->nhats = 0;
    joystick->nballs = 0;

    joystick->nbuttons = item->nbuttons;
    joystick->naxes = item->naxes;

    return (0);
}

/* Function to determine is this joystick is attached to the system right now */
SDL_bool SDL_SYS_JoystickAttached(SDL_Joystick *joystick)
{
    return !joystick->closed && (joystick->hwdata != NULL);
}

/* Function to update the state of a joystick - called as a device poll.
 * This function shouldn't update the joystick structure directly,
 * but instead should call SDL_PrivateJoystick*() to deliver events
 * and update joystick device state.
 */
void
SDL_SYS_JoystickUpdate(SDL_Joystick * joystick)
{
    EmscriptenGamepadEvent gamepadState;
    SDL_joylist_item *item = (SDL_joylist_item *) joystick->hwdata;
    int i, result, buttonState;

    if (item) {
        result = emscripten_get_gamepad_status(item->index, &gamepadState);
        if( result == EMSCRIPTEN_RESULT_SUCCESS) {
            if(gamepadState.timestamp == 0 || gamepadState.timestamp != item->timestamp) {
                for(i = 0; i < item->nbuttons; i++) {
                    if(item->digitalButton[i] != gamepadState.digitalButton[i]) {
                        buttonState = gamepadState.digitalButton[i]? SDL_PRESSED: SDL_RELEASED;
                        SDL_PrivateJoystickButton(item->joystick, i, buttonState);
                    }
                }

                for(i = 0; i < item->naxes; i++) {
                    if(item->axis[i] != gamepadState.axis[i]) {
                        // do we need to do conversion?
                        SDL_PrivateJoystickAxis(item->joystick, i,
                                                  (Sint16) (32767.*gamepadState.axis[i]));
                    }
                }

                item->timestamp = gamepadState.timestamp;
                for( i = 0; i < item->naxes; i++) {
                    item->axis[i] = gamepadState.axis[i];
                }

                for( i = 0; i < item->nbuttons; i++) {
                    item->analogButton[i] = gamepadState.analogButton[i];
                    item->digitalButton[i] = gamepadState.digitalButton[i];
                }
            }
        }
    }
}

/* Function to close a joystick after use */
void
SDL_SYS_JoystickClose(SDL_Joystick * joystick)
{
    if (joystick->hwdata) {
        ((SDL_joylist_item*)joystick->hwdata)->joystick = NULL;
        joystick->hwdata = NULL;
    }
    joystick->closed = 1;
}

/* Function to perform any system-specific joystick related cleanup */
void
SDL_SYS_JoystickQuit(void)
{
    SDL_joylist_item *item = NULL;
    SDL_joylist_item *next = NULL;

    for (item = SDL_joylist; item; item = next) {
        next = item->next;
        SDL_free(item->mapping);
        SDL_free(item->name);
        SDL_free(item);
    }

    SDL_joylist = SDL_joylist_tail = NULL;

    numjoysticks = 0;
    instance_counter = 0;

    emscripten_set_gamepadconnected_callback(NULL, 0, NULL);
    emscripten_set_gamepaddisconnected_callback(NULL, 0, NULL);
}

SDL_JoystickGUID
SDL_SYS_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = SDL_SYS_JoystickNameForDeviceIndex(device_index);
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

SDL_JoystickGUID
SDL_SYS_JoystickGetGUID(SDL_Joystick * joystick)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = joystick->name;
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

#endif /* SDL_JOYSTICK_EMSCRIPTEN */
