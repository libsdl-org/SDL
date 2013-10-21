/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2013 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_config.h"

/* This is the game controller API for Simple DirectMedia Layer */

#include "SDL_events.h"
#include "SDL_assert.h"
#include "SDL_sysjoystick.h"
#include "SDL_hints.h"
#include "SDL_gamecontrollerdb.h"

#if !SDL_EVENTS_DISABLED
#include "../events/SDL_events_c.h"
#endif
#define ABS(_x) ((_x) < 0 ? -(_x) : (_x))


/* a list of currently opened game controllers */
static SDL_GameController *SDL_gamecontrollers = NULL;

/* keep track of the hat and mask value that transforms this hat movement into a button/axis press */
struct _SDL_HatMapping
{
    int hat;
    Uint8 mask;
};

#define k_nMaxReverseEntries 20

/**
 * We are encoding the "HAT" as 0xhm. where h == hat ID and m == mask
 * MAX 4 hats supported
 */
#define k_nMaxHatEntries 0x3f + 1

/* our in memory mapping db between joystick objects and controller mappings */
struct _SDL_ControllerMapping
{
    SDL_JoystickGUID guid;
    const char *name;

    /* mapping of axis/button id to controller version */
    int axes[SDL_CONTROLLER_AXIS_MAX];
    int buttonasaxis[SDL_CONTROLLER_AXIS_MAX];

    int buttons[SDL_CONTROLLER_BUTTON_MAX];
    int axesasbutton[SDL_CONTROLLER_BUTTON_MAX];
    struct _SDL_HatMapping hatasbutton[SDL_CONTROLLER_BUTTON_MAX];

    /* reverse mapping, joystick indices to buttons */
    SDL_GameControllerAxis raxes[k_nMaxReverseEntries];
    SDL_GameControllerAxis rbuttonasaxis[k_nMaxReverseEntries];

    SDL_GameControllerButton rbuttons[k_nMaxReverseEntries];
    SDL_GameControllerButton raxesasbutton[k_nMaxReverseEntries];
    SDL_GameControllerButton rhatasbutton[k_nMaxHatEntries];

};


/* our hard coded list of mapping support */
typedef struct _ControllerMapping_t
{
    SDL_JoystickGUID guid;
    char *name;
    char *mapping;
    struct _ControllerMapping_t *next;
} ControllerMapping_t;

static ControllerMapping_t *s_pSupportedControllers = NULL;
#ifdef SDL_JOYSTICK_DINPUT
static ControllerMapping_t *s_pXInputMapping = NULL;
#endif

/* The SDL game controller structure */
struct _SDL_GameController
{
    SDL_Joystick *joystick; /* underlying joystick device */
    int ref_count;
    Uint8 hatState[4]; /* the current hat state for this controller */
    struct _SDL_ControllerMapping mapping; /* the mapping object for this controller */
    struct _SDL_GameController *next; /* pointer to next game controller we have allocated */
};


int SDL_PrivateGameControllerAxis(SDL_GameController * gamecontroller, SDL_GameControllerAxis axis, Sint16 value);
int SDL_PrivateGameControllerButton(SDL_GameController * gamecontroller, SDL_GameControllerButton button, Uint8 state);

/*
 * Event filter to fire controller events from joystick ones
 */
int SDL_GameControllerEventWatcher(void *userdata, SDL_Event * event)
{
    switch( event->type )
    {
    case SDL_JOYAXISMOTION:
        {
            SDL_GameController *controllerlist;

            if ( event->jaxis.axis >= k_nMaxReverseEntries ) break;

            controllerlist = SDL_gamecontrollers;
            while ( controllerlist )
            {
                if ( controllerlist->joystick->instance_id == event->jaxis.which )
                {
                    if ( controllerlist->mapping.raxes[event->jaxis.axis] >= 0 ) /* simple axis to axis, send it through */
                    {
                        SDL_GameControllerAxis axis = controllerlist->mapping.raxes[event->jaxis.axis];
                        Sint16 value = event->jaxis.value;
                        switch (axis)
                        {
                            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                                /* Shift it to be 0 - 32767. */
                                value = value / 2 + 16384;
                            default:
                                break;
                        }
                        SDL_PrivateGameControllerAxis( controllerlist, axis, value );
                    }
                    else if ( controllerlist->mapping.raxesasbutton[event->jaxis.axis] >= 0 ) /* simulate an axis as a button */
                    {
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.raxesasbutton[event->jaxis.axis], ABS(event->jaxis.value) > 32768/2 ? SDL_PRESSED : SDL_RELEASED );
                    }
                    break;
                }
                controllerlist = controllerlist->next;
            }
        }
        break;
    case SDL_JOYBUTTONDOWN:
    case SDL_JOYBUTTONUP:
        {
            SDL_GameController *controllerlist;

            if ( event->jbutton.button >= k_nMaxReverseEntries ) break;

            controllerlist = SDL_gamecontrollers;
            while ( controllerlist )
            {
                if ( controllerlist->joystick->instance_id == event->jbutton.which )
                {
                    if ( controllerlist->mapping.rbuttons[event->jbutton.button] >= 0 ) /* simple button as button */
                    {
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rbuttons[event->jbutton.button], event->jbutton.state );
                    }
                    else if ( controllerlist->mapping.rbuttonasaxis[event->jbutton.button] >= 0 ) /* an button pretending to be an axis */
                    {
                        SDL_PrivateGameControllerAxis( controllerlist, controllerlist->mapping.rbuttonasaxis[event->jbutton.button], event->jbutton.state > 0 ? 32767 : 0 );
                    }
                    break;
                }
                controllerlist = controllerlist->next;
            }
        }
        break;
    case SDL_JOYHATMOTION:
        {
            SDL_GameController *controllerlist;

            if ( event->jhat.hat >= 4 ) break;

            controllerlist = SDL_gamecontrollers;
            while ( controllerlist )
            {
                if ( controllerlist->joystick->instance_id == event->jhat.which )
                {
                    Uint8 bSame = controllerlist->hatState[event->jhat.hat] & event->jhat.value;
                    /* Get list of removed bits (button release) */
                    Uint8 bChanged = controllerlist->hatState[event->jhat.hat] ^ bSame;
                    /* the hat idx in the high nibble */
                    int bHighHat = event->jhat.hat << 4;

                    if ( bChanged & SDL_HAT_DOWN )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_DOWN], SDL_RELEASED );
                    if ( bChanged & SDL_HAT_UP )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_UP], SDL_RELEASED );
                    if ( bChanged & SDL_HAT_LEFT )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_LEFT], SDL_RELEASED );
                    if ( bChanged & SDL_HAT_RIGHT )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_RIGHT], SDL_RELEASED );

                    /* Get list of added bits (button press) */
                    bChanged = event->jhat.value ^ bSame;

                    if ( bChanged & SDL_HAT_DOWN )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_DOWN], SDL_PRESSED );
                    if ( bChanged & SDL_HAT_UP )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_UP], SDL_PRESSED );
                    if ( bChanged & SDL_HAT_LEFT )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_LEFT], SDL_PRESSED );
                    if ( bChanged & SDL_HAT_RIGHT )
                        SDL_PrivateGameControllerButton( controllerlist, controllerlist->mapping.rhatasbutton[bHighHat | SDL_HAT_RIGHT], SDL_PRESSED );

                    /* update our state cache */
                    controllerlist->hatState[event->jhat.hat] = event->jhat.value;

                    break;
                }
                controllerlist = controllerlist->next;
            }
        }
        break;
    case SDL_JOYDEVICEADDED:
        {
            if ( SDL_IsGameController(event->jdevice.which ) )
            {
                SDL_Event deviceevent;
                deviceevent.type = SDL_CONTROLLERDEVICEADDED;
                deviceevent.cdevice.which = event->jdevice.which;
                SDL_PushEvent(&deviceevent);
            }
        }
        break;
    case SDL_JOYDEVICEREMOVED:
        {
            SDL_GameController *controllerlist = SDL_gamecontrollers;
            while ( controllerlist )
            {
                if ( controllerlist->joystick->instance_id == event->jdevice.which )
                {
                    SDL_Event deviceevent;
                    deviceevent.type = SDL_CONTROLLERDEVICEREMOVED;
                    deviceevent.cdevice.which = event->jdevice.which;
                    SDL_PushEvent(&deviceevent);
                    break;
                }
                controllerlist = controllerlist->next;
            }
        }
        break;
    default:
        break;
    }

    return 1;
}

/*
 * Helper function to scan the mappings database for a controller with the specified GUID
 */
ControllerMapping_t *SDL_PrivateGetControllerMappingForGUID(SDL_JoystickGUID *guid)
{
    ControllerMapping_t *pSupportedController = s_pSupportedControllers;
    while ( pSupportedController )
    {
        if ( !SDL_memcmp( guid, &pSupportedController->guid, sizeof(*guid) ) )
        {
            return pSupportedController;
        }
        pSupportedController = pSupportedController->next;
    }
    return NULL;
}

/*
 * Helper function to determine pre-calculated offset to certain joystick mappings
 */
ControllerMapping_t *SDL_PrivateGetControllerMapping(int device_index)
{
#ifdef SDL_JOYSTICK_DINPUT
    if ( SDL_SYS_IsXInputDeviceIndex(device_index) && s_pXInputMapping )
    {
        return s_pXInputMapping;
    }
    else
#endif
    {
        SDL_JoystickGUID jGUID = SDL_JoystickGetDeviceGUID( device_index );
        return SDL_PrivateGetControllerMappingForGUID(&jGUID);
    }

    return NULL;
}

static const char* map_StringForControllerAxis[] = {
    "leftx",
    "lefty",
    "rightx",
    "righty",
    "lefttrigger",
    "righttrigger",
    NULL
};

/*
 * convert a string to its enum equivalent
 */
SDL_GameControllerAxis SDL_GameControllerGetAxisFromString( const char *pchString )
{
    int entry;
    if ( !pchString || !pchString[0] )
        return SDL_CONTROLLER_AXIS_INVALID;

    for ( entry = 0; map_StringForControllerAxis[entry]; ++entry)
    {
        if ( !SDL_strcasecmp( pchString, map_StringForControllerAxis[entry] ) )
            return entry;
    }
    return SDL_CONTROLLER_AXIS_INVALID;
}

/*
 * convert an enum to its string equivalent
 */
const char* SDL_GameControllerGetStringForAxis( SDL_GameControllerAxis axis )
{
    if (axis > SDL_CONTROLLER_AXIS_INVALID && axis < SDL_CONTROLLER_AXIS_MAX)
    {
        return map_StringForControllerAxis[axis];
    }
    return NULL;
}

static const char* map_StringForControllerButton[] = {
    "a",
    "b",
    "x",
    "y",
    "back",
    "guide",
    "start",
    "leftstick",
    "rightstick",
    "leftshoulder",
    "rightshoulder",
    "dpup",
    "dpdown",
    "dpleft",
    "dpright",
    NULL
};

/*
 * convert a string to its enum equivalent
 */
SDL_GameControllerButton SDL_GameControllerGetButtonFromString( const char *pchString )
{
    int entry;
    if ( !pchString || !pchString[0] )
        return SDL_CONTROLLER_BUTTON_INVALID;

    for ( entry = 0; map_StringForControllerButton[entry]; ++entry)
    {
        if ( !SDL_strcasecmp( pchString, map_StringForControllerButton[entry] ) )
        return entry;
    }
    return SDL_CONTROLLER_BUTTON_INVALID;
}

/*
 * convert an enum to its string equivalent
 */
const char* SDL_GameControllerGetStringForButton( SDL_GameControllerButton axis )
{
    if (axis > SDL_CONTROLLER_BUTTON_INVALID && axis < SDL_CONTROLLER_BUTTON_MAX)
    {
        return map_StringForControllerButton[axis];
    }
    return NULL;
}

/*
 * given a controller button name and a joystick name update our mapping structure with it
 */
void SDL_PrivateGameControllerParseButton( const char *szGameButton, const char *szJoystickButton, struct _SDL_ControllerMapping *pMapping )
{
    int iSDLButton = 0;
    SDL_GameControllerButton button;
    SDL_GameControllerAxis axis;
    button = SDL_GameControllerGetButtonFromString( szGameButton );
    axis = SDL_GameControllerGetAxisFromString( szGameButton );
    iSDLButton = SDL_atoi( &szJoystickButton[1] );

    if ( szJoystickButton[0] == 'a' )
    {
        if ( iSDLButton >= k_nMaxReverseEntries )
        {
            SDL_SetError("Axis index too large: %d", iSDLButton );
            return;
        }
        if ( axis != SDL_CONTROLLER_AXIS_INVALID )
        {
            pMapping->axes[ axis ] = iSDLButton;
            pMapping->raxes[ iSDLButton ] = axis;
        }
        else if ( button != SDL_CONTROLLER_BUTTON_INVALID )
        {
            pMapping->axesasbutton[ button ] = iSDLButton;
            pMapping->raxesasbutton[ iSDLButton ] = button;
        }
        else
        {
            SDL_assert( !"How did we get here?" );
        }

    }
    else if ( szJoystickButton[0] == 'b' )
    {
        if ( iSDLButton >= k_nMaxReverseEntries )
        {
            SDL_SetError("Button index too large: %d", iSDLButton );
            return;
        }
        if ( button != SDL_CONTROLLER_BUTTON_INVALID )
        {
            pMapping->buttons[ button ] = iSDLButton;
            pMapping->rbuttons[ iSDLButton ] = button;
        }
        else if ( axis != SDL_CONTROLLER_AXIS_INVALID )
        {
            pMapping->buttonasaxis[ axis ] = iSDLButton;
            pMapping->rbuttonasaxis[ iSDLButton ] = axis;
        }
        else
        {
            SDL_assert( !"How did we get here?" );
        }
    }
    else if ( szJoystickButton[0] == 'h' )
    {
        int hat = SDL_atoi( &szJoystickButton[1] );
        int mask = SDL_atoi( &szJoystickButton[3] );
        if (hat >= 4) {
            SDL_SetError("Hat index too large: %d", iSDLButton );
        }

        if ( button != SDL_CONTROLLER_BUTTON_INVALID )
        {
            int ridx;
            pMapping->hatasbutton[ button ].hat = hat;
            pMapping->hatasbutton[ button ].mask = mask;
            ridx = (hat << 4) | mask;
            pMapping->rhatasbutton[ ridx ] = button;
        }
        else if ( axis != SDL_CONTROLLER_AXIS_INVALID )
        {
            SDL_assert( !"Support hat as axis" );
        }
        else
        {
            SDL_assert( !"How did we get here?" );
        }
    }
}


/*
 * given a controller mapping string update our mapping object
 */
static void
SDL_PrivateGameControllerParseControllerConfigString( struct _SDL_ControllerMapping *pMapping, const char *pchString )
{
    char szGameButton[20];
    char szJoystickButton[20];
    SDL_bool bGameButton = SDL_TRUE;
    int i = 0;
    const char *pchPos = pchString;

    SDL_memset( szGameButton, 0x0, sizeof(szGameButton) );
    SDL_memset( szJoystickButton, 0x0, sizeof(szJoystickButton) );

    while ( pchPos && *pchPos )
    {
        if ( *pchPos == ':' )
        {
            i = 0;
            bGameButton = SDL_FALSE;
        }
        else if ( *pchPos == ' ' )
        {

        }
        else if ( *pchPos == ',' )
        {
            i = 0;
            bGameButton = SDL_TRUE;
            SDL_PrivateGameControllerParseButton( szGameButton, szJoystickButton, pMapping );
            SDL_memset( szGameButton, 0x0, sizeof(szGameButton) );
            SDL_memset( szJoystickButton, 0x0, sizeof(szJoystickButton) );

        }
        else if ( bGameButton )
        {
            if ( i >=  sizeof(szGameButton))
            {
                SDL_SetError( "Button name too large: %s", szGameButton );
                return;
            }
            szGameButton[i] = *pchPos;
            i++;
        }
        else
        {
            if ( i >=  sizeof(szJoystickButton))
            {
                SDL_SetError( "Joystick button name too large: %s", szJoystickButton );
                return;
            }
            szJoystickButton[i] = *pchPos;
            i++;
        }
        pchPos++;
    }

    SDL_PrivateGameControllerParseButton( szGameButton, szJoystickButton, pMapping );

}

/*
 * Make a new button mapping struct
 */
void SDL_PrivateLoadButtonMapping( struct _SDL_ControllerMapping *pMapping, SDL_JoystickGUID guid, const char *pchName, const char *pchMapping )
{
    int j;

    pMapping->guid = guid;
    pMapping->name = pchName;

    /* set all the button mappings to non defaults */
    for ( j = 0; j < SDL_CONTROLLER_AXIS_MAX; j++ )
    {
        pMapping->axes[j] = -1;
        pMapping->buttonasaxis[j] = -1;
    }
    for ( j = 0; j < SDL_CONTROLLER_BUTTON_MAX; j++ )
    {
        pMapping->buttons[j] = -1;
        pMapping->axesasbutton[j] = -1;
        pMapping->hatasbutton[j].hat = -1;
    }

    for ( j = 0; j < k_nMaxReverseEntries; j++ )
    {
        pMapping->raxes[j] = SDL_CONTROLLER_AXIS_INVALID;
        pMapping->rbuttonasaxis[j] = SDL_CONTROLLER_AXIS_INVALID;
        pMapping->rbuttons[j] = SDL_CONTROLLER_BUTTON_INVALID;
        pMapping->raxesasbutton[j] = SDL_CONTROLLER_BUTTON_INVALID;
    }

    for (j = 0; j < k_nMaxHatEntries; j++)
    {
        pMapping->rhatasbutton[j] = SDL_CONTROLLER_BUTTON_INVALID;
    }

    SDL_PrivateGameControllerParseControllerConfigString( pMapping, pchMapping );
}


/*
 * grab the guid string from a mapping string
 */
char *SDL_PrivateGetControllerGUIDFromMappingString( const char *pMapping )
{
    const char *pFirstComma = SDL_strchr( pMapping, ',' );
    if ( pFirstComma )
    {
        char *pchGUID = SDL_malloc( pFirstComma - pMapping + 1 );
        if ( !pchGUID )
        {
            SDL_OutOfMemory();
            return NULL;
        }
        SDL_memcpy( pchGUID, pMapping, pFirstComma - pMapping );
        pchGUID[ pFirstComma - pMapping ] = 0;
        return pchGUID;
    }
    return NULL;
}


/*
 * grab the name string from a mapping string
 */
char *SDL_PrivateGetControllerNameFromMappingString( const char *pMapping )
{
    const char *pFirstComma, *pSecondComma;
    char *pchName;

    pFirstComma = SDL_strchr( pMapping, ',' );
    if ( !pFirstComma )
        return NULL;

    pSecondComma = SDL_strchr( pFirstComma + 1, ',' );
    if ( !pSecondComma )
        return NULL;

    pchName = SDL_malloc( pSecondComma - pFirstComma );
    if ( !pchName )
    {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_memcpy( pchName, pFirstComma + 1, pSecondComma - pFirstComma );
    pchName[ pSecondComma - pFirstComma - 1 ] = 0;
    return pchName;
}


/*
 * grab the button mapping string from a mapping string
 */
char *SDL_PrivateGetControllerMappingFromMappingString( const char *pMapping )
{
    const char *pFirstComma, *pSecondComma;

    pFirstComma = SDL_strchr( pMapping, ',' );
    if ( !pFirstComma )
        return NULL;

    pSecondComma = SDL_strchr( pFirstComma + 1, ',' );
    if ( !pSecondComma )
        return NULL;

    return SDL_strdup(pSecondComma + 1); /* mapping is everything after the 3rd comma */
}

void SDL_PrivateGameControllerRefreshMapping( ControllerMapping_t *pControllerMapping )
{
    SDL_GameController *gamecontrollerlist = SDL_gamecontrollers;
    while ( gamecontrollerlist )
    {
        if ( !SDL_memcmp( &gamecontrollerlist->mapping.guid, &pControllerMapping->guid, sizeof(pControllerMapping->guid) ) )
        {
            SDL_Event event;
            event.type = SDL_CONTROLLERDEVICEREMAPPED;
            event.cdevice.which = gamecontrollerlist->joystick->instance_id;
            SDL_PushEvent(&event);

            /* Not really threadsafe.  Should this lock access within SDL_GameControllerEventWatcher? */
            SDL_PrivateLoadButtonMapping(&gamecontrollerlist->mapping, pControllerMapping->guid, pControllerMapping->name, pControllerMapping->mapping);
        }

        gamecontrollerlist = gamecontrollerlist->next;
    }
}

/*
 * Add or update an entry into the Mappings Database
 */
int
SDL_GameControllerAddMapping( const char *mappingString )
{
    char *pchGUID;
    char *pchName;
    char *pchMapping;
    SDL_JoystickGUID jGUID;
    ControllerMapping_t *pControllerMapping;
#ifdef SDL_JOYSTICK_DINPUT
    SDL_bool is_xinput_mapping = SDL_FALSE;
#endif

    pchGUID = SDL_PrivateGetControllerGUIDFromMappingString( mappingString );
    if (!pchGUID) {
        return -1;
    }
#ifdef SDL_JOYSTICK_DINPUT
    if ( !SDL_strcasecmp( pchGUID, "xinput" ) ) {
        is_xinput_mapping = SDL_TRUE;
    }
#endif
    jGUID = SDL_JoystickGetGUIDFromString(pchGUID);
    SDL_free(pchGUID);

    pControllerMapping = SDL_PrivateGetControllerMappingForGUID(&jGUID);

    pchName = SDL_PrivateGetControllerNameFromMappingString( mappingString );
    if (!pchName) return -1;

    pchMapping = SDL_PrivateGetControllerMappingFromMappingString( mappingString );
    if (!pchMapping) {
        SDL_free( pchName );
        return -1;
    }

    if (pControllerMapping) {
        /* Update existing mapping */
        SDL_free( pControllerMapping->name );
        pControllerMapping->name = pchName;
        SDL_free( pControllerMapping->mapping );
        pControllerMapping->mapping = pchMapping;
        /* refresh open controllers */
        SDL_PrivateGameControllerRefreshMapping( pControllerMapping );
        return 0;
    } else {
        pControllerMapping = SDL_malloc( sizeof(*pControllerMapping) );
        if (!pControllerMapping) {
            SDL_free( pchName );
            SDL_free( pchMapping );
            return SDL_OutOfMemory();
        }
#ifdef SDL_JOYSTICK_DINPUT
        if ( is_xinput_mapping )
        {
            s_pXInputMapping = pControllerMapping;
        }
#endif
        pControllerMapping->guid = jGUID;
        pControllerMapping->name = pchName;
        pControllerMapping->mapping = pchMapping;
        pControllerMapping->next = s_pSupportedControllers;
        s_pSupportedControllers = pControllerMapping;
        return 1;
    }
}

/*
 * Get the mapping string for this GUID
 */
char *
SDL_GameControllerMappingForGUID( SDL_JoystickGUID guid )
{
    char *pMappingString = NULL;
    ControllerMapping_t *mapping = SDL_PrivateGetControllerMappingForGUID(&guid);
    if (mapping) {
        char pchGUID[33];
        size_t needed;
        SDL_JoystickGetGUIDString(guid, pchGUID, sizeof(pchGUID));
        /* allocate enough memory for GUID + ',' + name + ',' + mapping + \0 */
        needed = SDL_strlen(pchGUID) + 1 + SDL_strlen(mapping->name) + 1 + SDL_strlen(mapping->mapping) + 1;
        pMappingString = SDL_malloc( needed );
        SDL_snprintf( pMappingString, needed, "%s,%s,%s", pchGUID, mapping->name, mapping->mapping );
    }
    return pMappingString;
}

/*
 * Get the mapping string for this device
 */
char *
SDL_GameControllerMapping( SDL_GameController * gamecontroller )
{
    return SDL_GameControllerMappingForGUID( gamecontroller->mapping.guid );
}

static void
SDL_GameControllerLoadHints()
{
    const char *hint = SDL_GetHint(SDL_HINT_GAMECONTROLLERCONFIG);
    if ( hint && hint[0] ) {
        size_t nchHints = SDL_strlen( hint );
        char *pUserMappings = SDL_malloc( nchHints + 1 );
        char *pTempMappings = pUserMappings;
        SDL_memcpy( pUserMappings, hint, nchHints );
        while ( pUserMappings ) {
            char *pchNewLine = NULL;

            pchNewLine = SDL_strchr( pUserMappings, '\n' );
            if ( pchNewLine )
                *pchNewLine = '\0';

            SDL_GameControllerAddMapping( pUserMappings );

            if ( pchNewLine )
                pUserMappings = pchNewLine + 1;
            else
                pUserMappings = NULL;
        }
        SDL_free(pTempMappings);
    }
}

/*
 * Initialize the game controller system, mostly load our DB of controller config mappings
 */
int
SDL_GameControllerInit(void)
{
    int i = 0;
    const char *pMappingString = NULL;
    s_pSupportedControllers = NULL;
    pMappingString = s_ControllerMappings[i];
    while ( pMappingString )
    {
        SDL_GameControllerAddMapping( pMappingString );

        i++;
        pMappingString = s_ControllerMappings[i];
    }

    /* load in any user supplied config */
    SDL_GameControllerLoadHints();

    /* watch for joy events and fire controller ones if needed */
    SDL_AddEventWatch( SDL_GameControllerEventWatcher, NULL );

    return (0);
}


/*
 * Get the implementation dependent name of a controller
 */
const char *
SDL_GameControllerNameForIndex(int device_index)
{
    ControllerMapping_t *pSupportedController =  SDL_PrivateGetControllerMapping(device_index);
    if ( pSupportedController )
    {
        return pSupportedController->name;
    }
    return NULL;
}


/*
 * Return 1 if the joystick at this device index is a supported controller
 */
SDL_bool
SDL_IsGameController(int device_index)
{
    ControllerMapping_t *pSupportedController =  SDL_PrivateGetControllerMapping(device_index);
    if ( pSupportedController )
    {
        return SDL_TRUE;
    }

    return SDL_FALSE;
}

/*
 * Open a controller for use - the index passed as an argument refers to
 * the N'th controller on the system.  This index is the value which will
 * identify this controller in future controller events.
 *
 * This function returns a controller identifier, or NULL if an error occurred.
 */
SDL_GameController *
SDL_GameControllerOpen(int device_index)
{
    SDL_GameController *gamecontroller;
    SDL_GameController *gamecontrollerlist;
    ControllerMapping_t *pSupportedController = NULL;

    if ((device_index < 0) || (device_index >= SDL_NumJoysticks())) {
        SDL_SetError("There are %d joysticks available", SDL_NumJoysticks());
        return (NULL);
    }

    gamecontrollerlist = SDL_gamecontrollers;
    /* If the controller is already open, return it */
    while ( gamecontrollerlist )
    {
        if ( SDL_SYS_GetInstanceIdOfDeviceIndex(device_index) == gamecontrollerlist->joystick->instance_id ) {
                gamecontroller = gamecontrollerlist;
                ++gamecontroller->ref_count;
                return (gamecontroller);
        }
        gamecontrollerlist = gamecontrollerlist->next;
    }

    /* Find a controller mapping */
    pSupportedController =  SDL_PrivateGetControllerMapping(device_index);
    if ( !pSupportedController ) {
        SDL_SetError("Couldn't find mapping for device (%d)", device_index );
        return (NULL);
    }

    /* Create and initialize the joystick */
    gamecontroller = (SDL_GameController *) SDL_malloc((sizeof *gamecontroller));
    if (gamecontroller == NULL) {
        SDL_OutOfMemory();
        return NULL;
    }

    SDL_memset(gamecontroller, 0, (sizeof *gamecontroller));
    gamecontroller->joystick = SDL_JoystickOpen(device_index);
    if ( !gamecontroller->joystick ) {
        SDL_free(gamecontroller);
        return NULL;
    }

    SDL_PrivateLoadButtonMapping( &gamecontroller->mapping, pSupportedController->guid, pSupportedController->name, pSupportedController->mapping );

    /* Add joystick to list */
    ++gamecontroller->ref_count;
    /* Link the joystick in the list */
    gamecontroller->next = SDL_gamecontrollers;
    SDL_gamecontrollers = gamecontroller;

    SDL_SYS_JoystickUpdate( gamecontroller->joystick );

    return (gamecontroller);
}

/*
 * Manually pump for controller updates.
 */
void
SDL_GameControllerUpdate(void)
{
    /* Just for API completeness; the joystick API does all the work. */
    SDL_JoystickUpdate();
}


/*
 * Get the current state of an axis control on a controller
 */
Sint16
SDL_GameControllerGetAxis(SDL_GameController * gamecontroller, SDL_GameControllerAxis axis)
{
    if ( !gamecontroller )
        return 0;

    if (gamecontroller->mapping.axes[axis] >= 0 )
    {
        Sint16 value = ( SDL_JoystickGetAxis( gamecontroller->joystick, gamecontroller->mapping.axes[axis]) );
        switch (axis)
        {
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                /* Shift it to be 0 - 32767. */
                value = value / 2 + 16384;
            default:
                break;
        }
        return value;
    }
    else if (gamecontroller->mapping.buttonasaxis[axis] >= 0 )
    {
        Uint8 value;
        value = SDL_JoystickGetButton( gamecontroller->joystick, gamecontroller->mapping.buttonasaxis[axis] );
        if ( value > 0 )
            return 32767;
        return 0;
    }
    return 0;
}


/*
 * Get the current state of a button on a controller
 */
Uint8
SDL_GameControllerGetButton(SDL_GameController * gamecontroller, SDL_GameControllerButton button)
{
    if ( !gamecontroller )
        return 0;

    if ( gamecontroller->mapping.buttons[button] >= 0 )
    {
        return ( SDL_JoystickGetButton( gamecontroller->joystick, gamecontroller->mapping.buttons[button] ) );
    }
    else if ( gamecontroller->mapping.axesasbutton[button] >= 0 )
    {
        Sint16 value;
        value = SDL_JoystickGetAxis( gamecontroller->joystick, gamecontroller->mapping.axesasbutton[button] );
        if ( ABS(value) > 32768/2 )
            return 1;
        return 0;
    }
    else if ( gamecontroller->mapping.hatasbutton[button].hat >= 0 )
    {
        Uint8 value;
        value = SDL_JoystickGetHat( gamecontroller->joystick, gamecontroller->mapping.hatasbutton[button].hat );

        if ( value & gamecontroller->mapping.hatasbutton[button].mask )
            return 1;
        return 0;
    }

    return 0;
}

/*
 * Return if the joystick in question is currently attached to the system,
 *  \return 0 if not plugged in, 1 if still present.
 */
SDL_bool
SDL_GameControllerGetAttached( SDL_GameController * gamecontroller )
{
    if ( !gamecontroller )
        return SDL_FALSE;

    return SDL_JoystickGetAttached(gamecontroller->joystick);
}


/*
 * Get the number of multi-dimensional axis controls on a joystick
 */
const char *
SDL_GameControllerName(SDL_GameController * gamecontroller)
{
    if ( !gamecontroller )
        return NULL;

    return (gamecontroller->mapping.name);
}


/*
 * Get the joystick for this controller
 */
SDL_Joystick *SDL_GameControllerGetJoystick(SDL_GameController * gamecontroller)
{
    if ( !gamecontroller )
        return NULL;

    return gamecontroller->joystick;
}

/**
 * Get the SDL joystick layer binding for this controller axis mapping
 */
SDL_GameControllerButtonBind SDL_GameControllerGetBindForAxis(SDL_GameController * gamecontroller, SDL_GameControllerAxis axis)
{
    SDL_GameControllerButtonBind bind;
    SDL_memset( &bind, 0x0, sizeof(bind) );

    if ( !gamecontroller || axis == SDL_CONTROLLER_AXIS_INVALID )
        return bind;

    if (gamecontroller->mapping.axes[axis] >= 0 )
    {
        bind.bindType = SDL_CONTROLLER_BINDTYPE_AXIS;
        bind.value.button = gamecontroller->mapping.axes[axis];
    }
    else if (gamecontroller->mapping.buttonasaxis[axis] >= 0 )
    {
        bind.bindType = SDL_CONTROLLER_BINDTYPE_BUTTON;
        bind.value.button = gamecontroller->mapping.buttonasaxis[axis];
    }

    return bind;
}


/**
 * Get the SDL joystick layer binding for this controller button mapping
 */
SDL_GameControllerButtonBind SDL_GameControllerGetBindForButton(SDL_GameController * gamecontroller, SDL_GameControllerButton button)
{
    SDL_GameControllerButtonBind bind;
    SDL_memset( &bind, 0x0, sizeof(bind) );

    if ( !gamecontroller || button == SDL_CONTROLLER_BUTTON_INVALID )
        return bind;

    if ( gamecontroller->mapping.buttons[button] >= 0 )
    {
        bind.bindType = SDL_CONTROLLER_BINDTYPE_BUTTON;
        bind.value.button = gamecontroller->mapping.buttons[button];
    }
    else if ( gamecontroller->mapping.axesasbutton[button] >= 0 )
    {
        bind.bindType = SDL_CONTROLLER_BINDTYPE_AXIS;
        bind.value.axis = gamecontroller->mapping.axesasbutton[button];
    }
    else if ( gamecontroller->mapping.hatasbutton[button].hat >= 0 )
    {
        bind.bindType = SDL_CONTROLLER_BINDTYPE_HAT;
        bind.value.hat.hat = gamecontroller->mapping.hatasbutton[button].hat;
        bind.value.hat.hat_mask = gamecontroller->mapping.hatasbutton[button].mask;
    }

    return bind;
}


/*
 * Close a joystick previously opened with SDL_JoystickOpen()
 */
void
SDL_GameControllerClose(SDL_GameController * gamecontroller)
{
    SDL_GameController *gamecontrollerlist, *gamecontrollerlistprev;

    if ( !gamecontroller )
        return;

    /* First decrement ref count */
    if (--gamecontroller->ref_count > 0) {
        return;
    }

    SDL_JoystickClose( gamecontroller->joystick );

    gamecontrollerlist = SDL_gamecontrollers;
    gamecontrollerlistprev = NULL;
    while ( gamecontrollerlist )
    {
        if (gamecontroller == gamecontrollerlist)
        {
            if ( gamecontrollerlistprev )
            {
                /* unlink this entry */
                gamecontrollerlistprev->next = gamecontrollerlist->next;
            }
            else
            {
                SDL_gamecontrollers = gamecontroller->next;
            }

            break;
        }
        gamecontrollerlistprev = gamecontrollerlist;
        gamecontrollerlist = gamecontrollerlist->next;
    }

    SDL_free(gamecontroller);
}


/*
 * Quit the controller subsystem
 */
void
SDL_GameControllerQuit(void)
{
    ControllerMapping_t *pControllerMap;
    while ( SDL_gamecontrollers )
    {
        SDL_gamecontrollers->ref_count = 1;
        SDL_GameControllerClose(SDL_gamecontrollers);
    }

    while ( s_pSupportedControllers )
    {
        pControllerMap = s_pSupportedControllers;
        s_pSupportedControllers = s_pSupportedControllers->next;
        SDL_free( pControllerMap->name );
        SDL_free( pControllerMap );
    }

    SDL_DelEventWatch( SDL_GameControllerEventWatcher, NULL );

}

/*
 * Event filter to transform joystick events into appropriate game controller ones
 */
int
SDL_PrivateGameControllerAxis(SDL_GameController * gamecontroller, SDL_GameControllerAxis axis, Sint16 value)
{
    int posted;

    /* translate the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_GetEventState(SDL_CONTROLLERAXISMOTION) == SDL_ENABLE) {
        SDL_Event event;
        event.type = SDL_CONTROLLERAXISMOTION;
        event.caxis.which = gamecontroller->joystick->instance_id;
        event.caxis.axis = axis;
        event.caxis.value = value;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}


/*
 * Event filter to transform joystick events into appropriate game controller ones
 */
int
SDL_PrivateGameControllerButton(SDL_GameController * gamecontroller, SDL_GameControllerButton button, Uint8 state)
{
    int posted;
#if !SDL_EVENTS_DISABLED
    SDL_Event event;

    if ( button == SDL_CONTROLLER_BUTTON_INVALID )
        return (0);

    switch (state) {
    case SDL_PRESSED:
        event.type = SDL_CONTROLLERBUTTONDOWN;
        break;
    case SDL_RELEASED:
        event.type = SDL_CONTROLLERBUTTONUP;
        break;
    default:
        /* Invalid state -- bail */
        return (0);
    }
#endif /* !SDL_EVENTS_DISABLED */

    /* translate the event, if desired */
    posted = 0;
#if !SDL_EVENTS_DISABLED
    if (SDL_GetEventState(event.type) == SDL_ENABLE) {
        event.cbutton.which = gamecontroller->joystick->instance_id;
        event.cbutton.button = button;
        event.cbutton.state = state;
        posted = SDL_PushEvent(&event) == 1;
    }
#endif /* !SDL_EVENTS_DISABLED */
    return (posted);
}

/*
 * Turn off controller events
 */
int
SDL_GameControllerEventState(int state)
{
#if SDL_EVENTS_DISABLED
    return SDL_IGNORE;
#else
    const Uint32 event_list[] = {
        SDL_CONTROLLERAXISMOTION, SDL_CONTROLLERBUTTONDOWN, SDL_CONTROLLERBUTTONUP,
        SDL_CONTROLLERDEVICEADDED, SDL_CONTROLLERDEVICEREMOVED, SDL_CONTROLLERDEVICEREMAPPED,
    };
    unsigned int i;

    switch (state) {
    case SDL_QUERY:
        state = SDL_IGNORE;
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

/* vi: set ts=4 sw=4 expandtab: */
