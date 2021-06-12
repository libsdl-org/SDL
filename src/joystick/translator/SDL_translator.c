/*
  Simple DirectMedia Layer
  Copyright (C) 2021 Paul Cercueil <paul@crapouillou.net>

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

#ifdef SDL_JOYSTICK_TRANSLATOR

#include "SDL_joystick.h"
#include "SDL_keyboard.h"
#include "../SDL_sysjoystick.h"
#include "../SDL_joystick_c.h"

static int num_translator_joysticks;

enum SDL_translator_item_type
{
    SDL_TRANSLATOR_AXIS,
    SDL_TRANSLATOR_HAT,
    SDL_TRANSLATOR_BUTTON,
};

enum SDL_translator_item_source
{
    SDL_TRANSLATOR_SOURCE_JOYSTICK,
    SDL_TRANSLATOR_SOURCE_KEYBOARD,
};

typedef struct SDL_translator_item
{
    enum SDL_translator_item_type type;
    enum SDL_translator_item_source source;

    SDL_Joystick *joystick;
    int device_index;
    int srccode[4];
    int dstcode;

    struct SDL_translator_item *next;
} SDL_translator_item;

typedef struct SDL_translator_joystick
{
    unsigned int naxes, nbuttons, nhats;
    int device_index;
    SDL_JoystickID joystickid;
    char *name;

    struct SDL_translator_item *items;
    struct SDL_translator_joystick *next;
} SDL_translator_joystick;

static SDL_translator_joystick *SDL_joylist;

static int
ProcessMap(char *map, char delimiter, int (*cb)(char *, void *), void *d)
{
    char *delim;
    int ret;

    while (map != NULL) {
        delim = SDL_strchr(map, delimiter);
        if (delim != NULL) {
            *delim++ = '\0';
        }
        ret = (*cb)(map, d);
        if (ret < 0) {
            return ret;
        }

        map = delim;
    }

    return 0;
}

static int
ProcessDeviceItem(char *map, void *d)
{
    SDL_translator_joystick *joystick = d;
    enum SDL_translator_item_type type;
    enum SDL_translator_item_source source;
    SDL_translator_item *item;
    SDL_JoystickGUID guid, guid2;
    int numjoysticks, i, device_index = 0;
    int dstcode, nb_codes;
    char *end;

    if (!SDL_strncmp(map, "axis:", sizeof("axis:") - 1)) {
        dstcode = joystick->naxes++;
        map += sizeof("axis:") - 1;
        type = SDL_TRANSLATOR_AXIS;
    } else if (!SDL_strncmp(map, "hat:", sizeof("hat:") - 1)) {
        dstcode = joystick->nhats++;
        map += sizeof("hat:") - 1;
        type = SDL_TRANSLATOR_HAT;
    } else if (!SDL_strncmp(map, "btn:", sizeof("btn:") - 1)) {
        dstcode = joystick->nbuttons++;
        map += sizeof("btn:") - 1;
        type = SDL_TRANSLATOR_BUTTON;
    } else {
        return SDL_SetError("Invalid device map");
    }

    if (!SDL_strncmp(map, "kb:", sizeof("kb:") - 1)) {
        if (type == SDL_TRANSLATOR_AXIS) {
            return SDL_SetError("Cannot use keyboard key for axis");
        }

        source = SDL_TRANSLATOR_SOURCE_KEYBOARD;
    } else {
        guid = SDL_JoystickGetGUIDFromString(map);
        numjoysticks = SDL_NumJoysticks();

        for (i = 0; i < numjoysticks; i++) {
            guid2 = SDL_JoystickGetDeviceGUID(i);

            if (!SDL_memcmp(&guid, &guid2, sizeof(guid))) {
                break;
            }
        }

        if (i == numjoysticks) {
            return SDL_SetError("Invalid device in map");
        }

        device_index = i;
        source = SDL_TRANSLATOR_SOURCE_JOYSTICK;
    }

    map = SDL_strchr(map, ':');
    if (!map) {
        return SDL_SetError("Invalid device map");
    }

    map += 1;

    if (type == SDL_TRANSLATOR_HAT && source == SDL_TRANSLATOR_SOURCE_KEYBOARD)
        nb_codes = 4;
    else
        nb_codes = 1;

    item = SDL_calloc(1, sizeof(*item));
    if (item == NULL)
        return SDL_OutOfMemory();

    item->device_index = device_index;
    item->dstcode = dstcode;
    item->type = type;
    item->source = source;

    for (i = 0; i < nb_codes; i++) {
        item->srccode[i] = (int) SDL_strtol(map, &end, 10);

        if (end == map ||
            (i == nb_codes - 1 && *end != '\0') ||
            (i < nb_codes - 1 && *end != ':')) {
            SDL_free(item);
            return SDL_SetError("Invalid device map");
        }

        map = end + 1;
    }

    item->next = joystick->items;
    joystick->items = item;

    return 0;
}

static void
FreeJoystick(SDL_translator_joystick *joystick)
{
    SDL_translator_item *item, *next;

    for (item = joystick->items; item; item = next) {
        next = item->next;
        SDL_free(item);
    }

    SDL_free(joystick->name);
    SDL_free(joystick);
}

static int
ProcessDevice(char *map, void *d)
{
    SDL_translator_joystick *joystick;
    char *comma, *name;
    int ret;

    comma = SDL_strchr(map, ',');
    if (comma == NULL) {
        return SDL_SetError("Invalid device map");
    }

    *comma = '\0';

    joystick = SDL_calloc(1, sizeof(*joystick));
    name = SDL_strdup(map);
    if (joystick == NULL || name == NULL) {
        SDL_free(joystick);
        SDL_free(name);
        return SDL_OutOfMemory();
    }

    joystick->name = name;
    map = comma + 1;

    ret = ProcessMap(map, ',', ProcessDeviceItem, joystick);
    if (ret) {
        FreeJoystick(joystick);
        return ret;
    }

    joystick->joystickid = SDL_GetNextJoystickInstanceID();

    joystick->next = SDL_joylist;
    SDL_joylist = joystick;
    num_translator_joysticks++;

    SDL_PrivateJoystickAdded(joystick->joystickid);

    return 0;
}

static int
TRANSLATOR_JoystickInit(void)
{
    char *map;
    int ret;

    map = SDL_getenv("SDL_JOYSTICK_TRANSLATOR_MAP");

    if (map == NULL) {
        return 0;
    }

    map = SDL_strdup(map);
    if (map == NULL) {
        return SDL_OutOfMemory();
    }

    ret = ProcessMap(map, ';', ProcessDevice, NULL);

    SDL_free(map);

    return ret;
}

static int
TRANSLATOR_JoystickGetCount(void)
{
    return num_translator_joysticks;
}

static void
TRANSLATOR_JoystickDetect(void)
{
}

static const char *
TRANSLATOR_JoystickGetDeviceName(int device_index)
{
    SDL_translator_joystick *js;

    for (js = SDL_joylist; js; js = js->next) {
        if (js->device_index == device_index) {
            return js->name;
        }
    }

    return "Translated joystick";
}

static int
TRANSLATOR_JoystickGetDevicePlayerIndex(int device_index)
{
    return -1;
}

static void
TRANSLATOR_JoystickSetDevicePlayerIndex(int device_index, int player_index)
{
}

static SDL_JoystickGUID TRANSLATOR_JoystickGetDeviceGUID(int device_index)
{
    SDL_JoystickGUID guid;
    /* the GUID is just the first 16 chars of the name for now */
    const char *name = TRANSLATOR_JoystickGetDeviceName(device_index);
    SDL_zero(guid);
    SDL_memcpy(&guid, name, SDL_min(sizeof(guid), SDL_strlen(name)));
    return guid;
}

/* Function to perform the mapping from device index to the instance id for this index */
static SDL_JoystickID TRANSLATOR_JoystickGetDeviceInstanceID(int device_index)
{
    SDL_translator_joystick *js;

    for (js = SDL_joylist; js; js = js->next) {
        if (js->device_index == device_index) {
            return js->joystickid;
        }
    }

    return -1;
}

static int
TRANSLATOR_JoystickRumble(SDL_Joystick *joystick, Uint16 low_frequency_rumble,
                          Uint16 high_frequency_rumble)
{
    return SDL_Unsupported();
}

static int
TRANSLATOR_JoystickRumbleTriggers(SDL_Joystick *joystick,
                                  Uint16 left_rumble, Uint16 right_rumble)
{
    return SDL_Unsupported();
}

static SDL_bool TRANSLATOR_JoystickHasLED(SDL_Joystick *joystick)
{
    return SDL_FALSE;
}

static int
TRANSLATOR_JoystickSetLED(SDL_Joystick *joystick, Uint8 red,
                          Uint8 green, Uint8 blue)
{
    return SDL_Unsupported();
}

static int TRANSLATOR_JoystickSetSensorsEnabled(SDL_Joystick *joystick,
                                                SDL_bool enabled)
{
    return SDL_Unsupported();
}

static void TRANSLATOR_JoystickUpdate(SDL_Joystick *joystick)
{
    SDL_translator_joystick *js = (SDL_translator_joystick *) joystick->hwdata;
    SDL_translator_item *item;
    const Uint8 *kbstate = SDL_GetKeyboardState(NULL);
    Uint8 value;
    int i;

    for (item = js->items; item; item = item->next) {
        if (item->type == SDL_TRANSLATOR_AXIS) {
            Sint16 axis_value = SDL_JoystickGetAxis(item->joystick, item->srccode[0]);
            SDL_PrivateJoystickAxis(joystick, item->dstcode, axis_value);
        } else if (item->type == SDL_TRANSLATOR_HAT) {
            if (item->source == SDL_TRANSLATOR_SOURCE_KEYBOARD) {
                for (value = 0, i = 0; i < 4; i++) {
                    if (kbstate[item->srccode[i]]) {
                        value |= 1 << i;
                    }
                }
            } else {
                value = SDL_JoystickGetHat(item->joystick, item->srccode[0]);
            }

            SDL_PrivateJoystickHat(joystick, item->dstcode, value);
        } else {
            if (item->source == SDL_TRANSLATOR_SOURCE_KEYBOARD) {
                value = kbstate[item->srccode[0]];
            } else {
                value = SDL_JoystickGetButton(item->joystick, item->srccode[0]);
            }

            SDL_PrivateJoystickButton(joystick, item->dstcode, value);
        }
    }
}

static int TRANSLATOR_JoystickOpen(SDL_Joystick *joystick, int device_index)
{
    SDL_translator_joystick *js;
    SDL_translator_item *item, *item2;

    for (js = SDL_joylist; js; js = js->next) {
        if (js->device_index == device_index) {
            break;
        }
    }

    if (js == NULL) {
        return SDL_SetError("No such device");
    }

    joystick->naxes = js->naxes;
    joystick->nbuttons = js->nbuttons;
    joystick->nhats = js->nhats;

    for (item = js->items; item; item = item->next) {
        item->joystick = SDL_JoystickOpen(item->device_index);
        if (!item->joystick) {
            for (item2 = js->items; item2 != item; item2 = item2->next) {
                SDL_JoystickClose(item2->joystick);
                item2->joystick = NULL;
            }

            return SDL_SetError("Unable to open joystick");
        }
    }

    joystick->hwdata = (struct joystick_hwdata *) js;

    return 0;
}

static void TRANSLATOR_JoystickClose(SDL_Joystick *joystick)
{
    SDL_translator_joystick *js = (SDL_translator_joystick *) joystick->hwdata;
    SDL_translator_item *item;

    for (item = js->items; item; item = item->next) {
        SDL_JoystickClose(item->joystick);
        item->joystick = NULL;
    }
}

void
TRANSLATOR_JoystickQuit(void)
{
    SDL_translator_joystick *next;

    while (SDL_joylist) {
        next = SDL_joylist->next;

        FreeJoystick(SDL_joylist);
        SDL_joylist = next;
    }
}

static SDL_bool
TRANSLATOR_JoystickGetGamepadMapping(int device_index, SDL_GamepadMapping *out)
{
    return SDL_FALSE;
}

SDL_JoystickDriver SDL_TRANSLATOR_JoystickDriver =
{
    TRANSLATOR_JoystickInit,
    TRANSLATOR_JoystickGetCount,
    TRANSLATOR_JoystickDetect,
    TRANSLATOR_JoystickGetDeviceName,
    TRANSLATOR_JoystickGetDevicePlayerIndex,
    TRANSLATOR_JoystickSetDevicePlayerIndex,
    TRANSLATOR_JoystickGetDeviceGUID,
    TRANSLATOR_JoystickGetDeviceInstanceID,
    TRANSLATOR_JoystickOpen,
    TRANSLATOR_JoystickRumble,
    TRANSLATOR_JoystickRumbleTriggers,
    TRANSLATOR_JoystickHasLED,
    TRANSLATOR_JoystickSetLED,
    TRANSLATOR_JoystickSetSensorsEnabled,
    TRANSLATOR_JoystickUpdate,
    TRANSLATOR_JoystickClose,
    TRANSLATOR_JoystickQuit,
    TRANSLATOR_JoystickGetGamepadMapping
};

#endif /* SDL_JOYSTICK_TRANSLATOR */

/* vi: set ts=4 sw=4 expandtab: */
