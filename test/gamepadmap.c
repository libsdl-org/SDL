/*
  Copyright (C) 1997-2023 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Gamepad mapping generator */
/* Gabriel Jacobo <gabomdq@gmail.com> */

#include <stdio.h>
#include <stdlib.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "testutils.h"

/* Define this for verbose output while mapping gamepads */
#define DEBUG_GAMEPADMAP

#define SCREEN_WIDTH  512
#define SCREEN_HEIGHT 320

enum marker_type
{
    MARKER_BUTTON,
    MARKER_AXIS,
};

enum
{
    SDL_GAMEPAD_BINDING_AXIS_LEFTX_NEGATIVE,
    SDL_GAMEPAD_BINDING_AXIS_LEFTX_POSITIVE,
    SDL_GAMEPAD_BINDING_AXIS_LEFTY_NEGATIVE,
    SDL_GAMEPAD_BINDING_AXIS_LEFTY_POSITIVE,
    SDL_GAMEPAD_BINDING_AXIS_RIGHTX_NEGATIVE,
    SDL_GAMEPAD_BINDING_AXIS_RIGHTX_POSITIVE,
    SDL_GAMEPAD_BINDING_AXIS_RIGHTY_NEGATIVE,
    SDL_GAMEPAD_BINDING_AXIS_RIGHTY_POSITIVE,
    SDL_GAMEPAD_BINDING_AXIS_TRIGGERLEFT,
    SDL_GAMEPAD_BINDING_AXIS_TRIGGERRIGHT,
    SDL_GAMEPAD_BINDING_AXIS_MAX,
};

#define BINDING_COUNT (SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_MAX)

static struct
{
    int x, y;
    double angle;
    enum marker_type marker;

} s_arrBindingDisplay[] = {
    { 387, 167, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_A */
    { 431, 132, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_B */
    { 342, 132, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_X */
    { 389, 101, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_Y */
    { 174, 132, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_BACK */
    { 232, 128, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_GUIDE */
    { 289, 132, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_START */
    { 75, 154, 0.0, MARKER_BUTTON },  /* SDL_GAMEPAD_BUTTON_LEFT_STICK */
    { 305, 230, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_RIGHT_STICK */
    { 77, 40, 0.0, MARKER_BUTTON },   /* SDL_GAMEPAD_BUTTON_LEFT_SHOULDER */
    { 396, 36, 0.0, MARKER_BUTTON },  /* SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER */
    { 154, 188, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_DPAD_UP */
    { 154, 249, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_DPAD_DOWN */
    { 116, 217, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_DPAD_LEFT */
    { 186, 217, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_DPAD_RIGHT */
    { 232, 174, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_MISC1 */
    { 132, 135, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_PADDLE1 */
    { 330, 135, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_PADDLE2 */
    { 132, 175, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_PADDLE3 */
    { 330, 175, 0.0, MARKER_BUTTON }, /* SDL_GAMEPAD_BUTTON_PADDLE4 */
    { 0, 0, 0.0, MARKER_BUTTON },     /* SDL_GAMEPAD_BUTTON_TOUCHPAD */
    { 74, 153, 270.0, MARKER_AXIS },  /* SDL_GAMEPAD_BINDING_AXIS_LEFTX_NEGATIVE */
    { 74, 153, 90.0, MARKER_AXIS },   /* SDL_GAMEPAD_BINDING_AXIS_LEFTX_POSITIVE */
    { 74, 153, 0.0, MARKER_AXIS },    /* SDL_GAMEPAD_BINDING_AXIS_LEFTY_NEGATIVE */
    { 74, 153, 180.0, MARKER_AXIS },  /* SDL_GAMEPAD_BINDING_AXIS_LEFTY_POSITIVE */
    { 306, 231, 270.0, MARKER_AXIS }, /* SDL_GAMEPAD_BINDING_AXIS_RIGHTX_NEGATIVE */
    { 306, 231, 90.0, MARKER_AXIS },  /* SDL_GAMEPAD_BINDING_AXIS_RIGHTX_POSITIVE */
    { 306, 231, 0.0, MARKER_AXIS },   /* SDL_GAMEPAD_BINDING_AXIS_RIGHTY_NEGATIVE */
    { 306, 231, 180.0, MARKER_AXIS }, /* SDL_GAMEPAD_BINDING_AXIS_RIGHTY_POSITIVE */
    { 91, -20, 180.0, MARKER_AXIS },  /* SDL_GAMEPAD_BINDING_AXIS_TRIGGERLEFT */
    { 375, -20, 180.0, MARKER_AXIS }, /* SDL_GAMEPAD_BINDING_AXIS_TRIGGERRIGHT */
};
SDL_COMPILE_TIME_ASSERT(s_arrBindingDisplay, SDL_arraysize(s_arrBindingDisplay) == BINDING_COUNT);

static int s_arrBindingOrder[] = {
    SDL_GAMEPAD_BUTTON_A,
    SDL_GAMEPAD_BUTTON_B,
    SDL_GAMEPAD_BUTTON_Y,
    SDL_GAMEPAD_BUTTON_X,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_LEFTX_NEGATIVE,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_LEFTX_POSITIVE,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_LEFTY_NEGATIVE,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_LEFTY_POSITIVE,
    SDL_GAMEPAD_BUTTON_LEFT_STICK,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_RIGHTX_NEGATIVE,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_RIGHTX_POSITIVE,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_RIGHTY_NEGATIVE,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_RIGHTY_POSITIVE,
    SDL_GAMEPAD_BUTTON_RIGHT_STICK,
    SDL_GAMEPAD_BUTTON_LEFT_SHOULDER,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_TRIGGERLEFT,
    SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER,
    SDL_GAMEPAD_BUTTON_MAX + SDL_GAMEPAD_BINDING_AXIS_TRIGGERRIGHT,
    SDL_GAMEPAD_BUTTON_DPAD_UP,
    SDL_GAMEPAD_BUTTON_DPAD_RIGHT,
    SDL_GAMEPAD_BUTTON_DPAD_DOWN,
    SDL_GAMEPAD_BUTTON_DPAD_LEFT,
    SDL_GAMEPAD_BUTTON_BACK,
    SDL_GAMEPAD_BUTTON_GUIDE,
    SDL_GAMEPAD_BUTTON_START,
    SDL_GAMEPAD_BUTTON_MISC1,
    SDL_GAMEPAD_BUTTON_PADDLE1,
    SDL_GAMEPAD_BUTTON_PADDLE2,
    SDL_GAMEPAD_BUTTON_PADDLE3,
    SDL_GAMEPAD_BUTTON_PADDLE4,
    SDL_GAMEPAD_BUTTON_TOUCHPAD,
};
SDL_COMPILE_TIME_ASSERT(s_arrBindingOrder, SDL_arraysize(s_arrBindingOrder) == BINDING_COUNT);

typedef struct
{
    SDL_GamepadBindingType bindType;
    union
    {
        int button;

        struct
        {
            int axis;
            int axis_min;
            int axis_max;
        } axis;

        struct
        {
            int hat;
            int hat_mask;
        } hat;

    } value;

    SDL_bool committed;

} SDL_GameControllerExtendedBind;

static SDL_GameControllerExtendedBind s_arrBindings[BINDING_COUNT];

typedef struct
{
    SDL_bool m_bMoving;
    int m_nLastValue;
    int m_nStartingValue;
    int m_nFarthestValue;
} AxisState;

static int s_nNumAxes;
static AxisState *s_arrAxisState;

static int s_iCurrentBinding;
static Uint64 s_unPendingAdvanceTime;
static SDL_bool s_bBindingComplete;

static SDL_Window *window;
static SDL_Renderer *screen;
static SDL_bool done = SDL_FALSE;
static SDL_bool bind_touchpad = SDL_FALSE;

static int
StandardizeAxisValue(int nValue)
{
    if (nValue > SDL_JOYSTICK_AXIS_MAX / 2) {
        return SDL_JOYSTICK_AXIS_MAX;
    } else if (nValue < SDL_JOYSTICK_AXIS_MIN / 2) {
        return SDL_JOYSTICK_AXIS_MIN;
    } else {
        return 0;
    }
}

static void
SetCurrentBinding(int iBinding)
{
    int iIndex;
    SDL_GameControllerExtendedBind *pBinding;

    if (iBinding < 0) {
        return;
    }

    if (iBinding == BINDING_COUNT) {
        s_bBindingComplete = SDL_TRUE;
        return;
    }

    if (s_arrBindingOrder[iBinding] == -1) {
        SetCurrentBinding(iBinding + 1);
        return;
    }

    if (s_arrBindingOrder[iBinding] == SDL_GAMEPAD_BUTTON_TOUCHPAD &&
        !bind_touchpad) {
        SetCurrentBinding(iBinding + 1);
        return;
    }

    s_iCurrentBinding = iBinding;

    pBinding = &s_arrBindings[s_arrBindingOrder[s_iCurrentBinding]];
    SDL_zerop(pBinding);

    for (iIndex = 0; iIndex < s_nNumAxes; ++iIndex) {
        s_arrAxisState[iIndex].m_nFarthestValue = s_arrAxisState[iIndex].m_nStartingValue;
    }

    s_unPendingAdvanceTime = 0;
}

static SDL_bool
BBindingContainsBinding(const SDL_GameControllerExtendedBind *pBindingA, const SDL_GameControllerExtendedBind *pBindingB)
{
    if (pBindingA->bindType != pBindingB->bindType) {
        return SDL_FALSE;
    }
    switch (pBindingA->bindType) {
    case SDL_GAMEPAD_BINDTYPE_AXIS:
        if (pBindingA->value.axis.axis != pBindingB->value.axis.axis) {
            return SDL_FALSE;
        }
        if (!pBindingA->committed) {
            return SDL_FALSE;
        }
        {
            int minA = SDL_min(pBindingA->value.axis.axis_min, pBindingA->value.axis.axis_max);
            int maxA = SDL_max(pBindingA->value.axis.axis_min, pBindingA->value.axis.axis_max);
            int minB = SDL_min(pBindingB->value.axis.axis_min, pBindingB->value.axis.axis_max);
            int maxB = SDL_max(pBindingB->value.axis.axis_min, pBindingB->value.axis.axis_max);
            return minA <= minB && maxA >= maxB;
        }
        /* Not reached */
    default:
        return SDL_memcmp(pBindingA, pBindingB, sizeof(*pBindingA)) == 0;
    }
}

static void
ConfigureBinding(const SDL_GameControllerExtendedBind *pBinding)
{
    SDL_GameControllerExtendedBind *pCurrent;
    int iIndex;
    int iCurrentElement = s_arrBindingOrder[s_iCurrentBinding];

    /* Do we already have this binding? */
    for (iIndex = 0; iIndex < SDL_arraysize(s_arrBindings); ++iIndex) {
        pCurrent = &s_arrBindings[iIndex];
        if (BBindingContainsBinding(pCurrent, pBinding)) {
            if (iIndex == SDL_GAMEPAD_BUTTON_A && iCurrentElement != SDL_GAMEPAD_BUTTON_B) {
                /* Skip to the next binding */
                SetCurrentBinding(s_iCurrentBinding + 1);
                return;
            }

            if (iIndex == SDL_GAMEPAD_BUTTON_B) {
                /* Go back to the previous binding */
                SetCurrentBinding(s_iCurrentBinding - 1);
                return;
            }

            /* Already have this binding, ignore it */
            return;
        }
    }

#ifdef DEBUG_GAMEPADMAP
    switch (pBinding->bindType) {
    case SDL_GAMEPAD_BINDTYPE_NONE:
        break;
    case SDL_GAMEPAD_BINDTYPE_BUTTON:
        SDL_Log("Configuring button binding for button %d\n", pBinding->value.button);
        break;
    case SDL_GAMEPAD_BINDTYPE_AXIS:
        SDL_Log("Configuring axis binding for axis %d %d/%d committed = %s\n", pBinding->value.axis.axis, pBinding->value.axis.axis_min, pBinding->value.axis.axis_max, pBinding->committed ? "true" : "false");
        break;
    case SDL_GAMEPAD_BINDTYPE_HAT:
        SDL_Log("Configuring hat binding for hat %d %d\n", pBinding->value.hat.hat, pBinding->value.hat.hat_mask);
        break;
    }
#endif /* DEBUG_GAMEPADMAP */

    /* Should the new binding override the existing one? */
    pCurrent = &s_arrBindings[iCurrentElement];
    if (pCurrent->bindType != SDL_GAMEPAD_BINDTYPE_NONE) {
        SDL_bool bNativeDPad, bCurrentDPad;
        SDL_bool bNativeAxis, bCurrentAxis;

        bNativeDPad = (iCurrentElement == SDL_GAMEPAD_BUTTON_DPAD_UP ||
                       iCurrentElement == SDL_GAMEPAD_BUTTON_DPAD_DOWN ||
                       iCurrentElement == SDL_GAMEPAD_BUTTON_DPAD_LEFT ||
                       iCurrentElement == SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
        bCurrentDPad = (pCurrent->bindType == SDL_GAMEPAD_BINDTYPE_HAT);
        if (bNativeDPad && bCurrentDPad) {
            /* We already have a binding of the type we want, ignore the new one */
            return;
        }

        bNativeAxis = (iCurrentElement >= SDL_GAMEPAD_BUTTON_MAX);
        bCurrentAxis = (pCurrent->bindType == SDL_GAMEPAD_BINDTYPE_AXIS);
        if (bNativeAxis == bCurrentAxis &&
            (pBinding->bindType != SDL_GAMEPAD_BINDTYPE_AXIS ||
             pBinding->value.axis.axis != pCurrent->value.axis.axis)) {
            /* We already have a binding of the type we want, ignore the new one */
            return;
        }
    }

    *pCurrent = *pBinding;

    if (pBinding->committed) {
        s_unPendingAdvanceTime = SDL_GetTicks();
    } else {
        s_unPendingAdvanceTime = 0;
    }
}

static SDL_bool
BMergeAxisBindings(int iIndex)
{
    SDL_GameControllerExtendedBind *pBindingA = &s_arrBindings[iIndex];
    SDL_GameControllerExtendedBind *pBindingB = &s_arrBindings[iIndex + 1];
    if (pBindingA->bindType == SDL_GAMEPAD_BINDTYPE_AXIS &&
        pBindingB->bindType == SDL_GAMEPAD_BINDTYPE_AXIS &&
        pBindingA->value.axis.axis == pBindingB->value.axis.axis) {
        if (pBindingA->value.axis.axis_min == pBindingB->value.axis.axis_min) {
            pBindingA->value.axis.axis_min = pBindingA->value.axis.axis_max;
            pBindingA->value.axis.axis_max = pBindingB->value.axis.axis_max;
            pBindingB->bindType = SDL_GAMEPAD_BINDTYPE_NONE;
            return SDL_TRUE;
        }
    }
    return SDL_FALSE;
}

static void
WatchJoystick(SDL_Joystick *joystick)
{
    SDL_Texture *background_front, *background_back, *button, *axis, *marker = NULL;
    const char *name = NULL;
    SDL_Event event;
    SDL_FRect dst;
    int texture_w, texture_h;
    Uint8 alpha = 200, alpha_step = -1;
    Uint64 alpha_ticks = 0;
    SDL_JoystickID nJoystickID;

    background_front = LoadTexture(screen, "gamepadmap.bmp", SDL_FALSE, NULL, NULL);
    background_back = LoadTexture(screen, "gamepadmap_back.bmp", SDL_FALSE, NULL, NULL);
    button = LoadTexture(screen, "button.bmp", SDL_TRUE, NULL, NULL);
    axis = LoadTexture(screen, "axis.bmp", SDL_TRUE, NULL, NULL);
    SDL_RaiseWindow(window);

    /* scale for platforms that don't give you the window size you asked for. */
    SDL_SetRenderLogicalPresentation(screen, SCREEN_WIDTH, SCREEN_HEIGHT,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX,
                                     SDL_SCALEMODE_LINEAR);

    /* Print info about the joystick we are watching */
    name = SDL_GetJoystickName(joystick);
    SDL_Log("Watching joystick %" SDL_PRIs32 ": (%s)\n", SDL_GetJoystickInstanceID(joystick),
            name ? name : "Unknown Joystick");
    SDL_Log("Joystick has %d axes, %d hats, and %d buttons\n",
            SDL_GetNumJoystickAxes(joystick), SDL_GetNumJoystickHats(joystick), SDL_GetNumJoystickButtons(joystick));

    SDL_Log("\n\n\
    ====================================================================================\n\
    Press the buttons on your gamepad when indicated\n\
    (Your gamepad may look different than the picture)\n\
    If you want to correct a mistake, press backspace or the back button on your device\n\
    To skip a button, press SPACE or click/touch the screen\n\
    To exit, press ESC\n\
    ====================================================================================\n");

    nJoystickID = SDL_GetJoystickInstanceID(joystick);

    s_nNumAxes = SDL_GetNumJoystickAxes(joystick);
    s_arrAxisState = (AxisState *)SDL_calloc(s_nNumAxes, sizeof(*s_arrAxisState));

    /* Skip any spurious events at start */
    while (SDL_PollEvent(&event) > 0) {
    }

    /* Loop, getting joystick events! */
    while (!done && !s_bBindingComplete) {
        int iElement = s_arrBindingOrder[s_iCurrentBinding];

        switch (s_arrBindingDisplay[iElement].marker) {
        case MARKER_AXIS:
            marker = axis;
            break;
        case MARKER_BUTTON:
            marker = button;
            break;
        }

        SDL_QueryTexture(marker, NULL, NULL, &texture_w, &texture_h);
        dst.x = (float)s_arrBindingDisplay[iElement].x;
        dst.y = (float)s_arrBindingDisplay[iElement].y;
        dst.w = (float)texture_w;
        dst.h = (float)texture_h;

        if (SDL_GetTicks() >= (alpha_ticks + 5)) {
            alpha_ticks = SDL_GetTicks();
            alpha += alpha_step;
            if (alpha == 255) {
                alpha_step = -1;
            }
            if (alpha < 128) {
                alpha_step = 1;
            }
        }

        SDL_SetRenderDrawColor(screen, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(screen);
        if (s_arrBindingOrder[s_iCurrentBinding] >= SDL_GAMEPAD_BUTTON_PADDLE1 &&
            s_arrBindingOrder[s_iCurrentBinding] <= SDL_GAMEPAD_BUTTON_PADDLE4) {
            SDL_RenderTexture(screen, background_back, NULL, NULL);
        } else {
            SDL_RenderTexture(screen, background_front, NULL, NULL);
        }
        SDL_SetTextureAlphaMod(marker, alpha);
        SDL_SetTextureColorMod(marker, 10, 255, 21);
        SDL_RenderTextureRotated(screen, marker, NULL, &dst, s_arrBindingDisplay[iElement].angle, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(screen);

        while (SDL_PollEvent(&event) > 0) {
            switch (event.type) {
            case SDL_EVENT_JOYSTICK_REMOVED:
                if (event.jaxis.which == nJoystickID) {
                    done = SDL_TRUE;
                }
                break;
            case SDL_EVENT_JOYSTICK_AXIS_MOTION:
                if (event.jaxis.which == nJoystickID) {
                    const int MAX_ALLOWED_JITTER = SDL_JOYSTICK_AXIS_MAX / 80; /* ShanWan PS3 gamepad needed 96 */
                    AxisState *pAxisState = &s_arrAxisState[event.jaxis.axis];
                    int nValue = event.jaxis.value;
                    int nCurrentDistance, nFarthestDistance;
                    if (!pAxisState->m_bMoving) {
                        Sint16 nInitialValue;
                        pAxisState->m_bMoving = SDL_GetJoystickAxisInitialState(joystick, event.jaxis.axis, &nInitialValue);
                        pAxisState->m_nLastValue = nValue;
                        pAxisState->m_nStartingValue = nInitialValue;
                        pAxisState->m_nFarthestValue = nInitialValue;
                    } else if (SDL_abs(nValue - pAxisState->m_nLastValue) <= MAX_ALLOWED_JITTER) {
                        break;
                    } else {
                        pAxisState->m_nLastValue = nValue;
                    }
                    nCurrentDistance = SDL_abs(nValue - pAxisState->m_nStartingValue);
                    nFarthestDistance = SDL_abs(pAxisState->m_nFarthestValue - pAxisState->m_nStartingValue);
                    if (nCurrentDistance > nFarthestDistance) {
                        pAxisState->m_nFarthestValue = nValue;
                        nFarthestDistance = SDL_abs(pAxisState->m_nFarthestValue - pAxisState->m_nStartingValue);
                    }

#ifdef DEBUG_GAMEPADMAP
                    SDL_Log("AXIS %d nValue %d nCurrentDistance %d nFarthestDistance %d\n", event.jaxis.axis, nValue, nCurrentDistance, nFarthestDistance);
#endif
                    if (nFarthestDistance >= 16000) {
                        /* If we've gone out far enough and started to come back, let's bind this axis */
                        SDL_bool bCommitBinding = (nCurrentDistance <= 10000) ? SDL_TRUE : SDL_FALSE;
                        SDL_GameControllerExtendedBind binding;
                        SDL_zero(binding);
                        binding.bindType = SDL_GAMEPAD_BINDTYPE_AXIS;
                        binding.value.axis.axis = event.jaxis.axis;
                        binding.value.axis.axis_min = StandardizeAxisValue(pAxisState->m_nStartingValue);
                        binding.value.axis.axis_max = StandardizeAxisValue(pAxisState->m_nFarthestValue);
                        binding.committed = bCommitBinding;
                        ConfigureBinding(&binding);
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_HAT_MOTION:
                if (event.jhat.which == nJoystickID) {
                    if (event.jhat.value != SDL_HAT_CENTERED) {
                        SDL_GameControllerExtendedBind binding;

#ifdef DEBUG_GAMEPADMAP
                        SDL_Log("HAT %d %d\n", event.jhat.hat, event.jhat.value);
#endif
                        SDL_zero(binding);
                        binding.bindType = SDL_GAMEPAD_BINDTYPE_HAT;
                        binding.value.hat.hat = event.jhat.hat;
                        binding.value.hat.hat_mask = event.jhat.value;
                        binding.committed = SDL_TRUE;
                        ConfigureBinding(&binding);
                    }
                }
                break;
            case SDL_EVENT_JOYSTICK_BUTTON_UP:
                if (event.jbutton.which == nJoystickID) {
                    SDL_GameControllerExtendedBind binding;

#ifdef DEBUG_GAMEPADMAP
                    SDL_Log("BUTTON %d\n", event.jbutton.button);
#endif
                    SDL_zero(binding);
                    binding.bindType = SDL_GAMEPAD_BINDTYPE_BUTTON;
                    binding.value.button = event.jbutton.button;
                    binding.committed = SDL_TRUE;
                    ConfigureBinding(&binding);
                }
                break;
            case SDL_EVENT_FINGER_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                /* Skip this step */
                SetCurrentBinding(s_iCurrentBinding + 1);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.keysym.sym == SDLK_BACKSPACE || event.key.keysym.sym == SDLK_AC_BACK) {
                    SetCurrentBinding(s_iCurrentBinding - 1);
                    break;
                }
                if (event.key.keysym.sym == SDLK_SPACE) {
                    SetCurrentBinding(s_iCurrentBinding + 1);
                    break;
                }

                if (event.key.keysym.sym != SDLK_ESCAPE) {
                    break;
                }
                SDL_FALLTHROUGH;
            case SDL_EVENT_QUIT:
                done = SDL_TRUE;
                break;
            default:
                break;
            }
        }

        SDL_Delay(15);

        /* Wait 30 ms for joystick events to stop coming in,
           in case a gamepad sends multiple events for a single control (e.g. axis and button for trigger)
        */
        if (s_unPendingAdvanceTime && SDL_GetTicks() - s_unPendingAdvanceTime >= 30) {
            SetCurrentBinding(s_iCurrentBinding + 1);
        }
    }

    if (s_bBindingComplete) {
        SDL_JoystickGUID guid;
        Uint16 crc;
        char mapping[1024];
        char trimmed_name[128];
        char *spot;
        int iIndex;
        char pszElement[12];

        SDL_strlcpy(trimmed_name, name, SDL_arraysize(trimmed_name));
        while (SDL_isspace(trimmed_name[0])) {
            SDL_memmove(&trimmed_name[0], &trimmed_name[1], SDL_strlen(trimmed_name));
        }
        while (trimmed_name[0] && SDL_isspace(trimmed_name[SDL_strlen(trimmed_name) - 1])) {
            trimmed_name[SDL_strlen(trimmed_name) - 1] = '\0';
        }
        while ((spot = SDL_strchr(trimmed_name, ',')) != NULL) {
            SDL_memmove(spot, spot + 1, SDL_strlen(spot));
        }

        /* Initialize mapping with GUID and name */
        guid = SDL_GetJoystickGUID(joystick);
        SDL_GetJoystickGUIDInfo(guid, NULL, NULL, NULL, &crc);
        if (crc) {
            /* Clear the CRC from the GUID for the mapping */
            guid.data[2] = 0;
            guid.data[3] = 0;
        }
        SDL_GetJoystickGUIDString(guid, mapping, SDL_arraysize(mapping));
        SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
        SDL_strlcat(mapping, trimmed_name, SDL_arraysize(mapping));
        SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
        SDL_strlcat(mapping, "platform:", SDL_arraysize(mapping));
        SDL_strlcat(mapping, SDL_GetPlatform(), SDL_arraysize(mapping));
        SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
        if (crc) {
            char crc_string[5];

            SDL_strlcat(mapping, "crc:", SDL_arraysize(mapping));
            (void)SDL_snprintf(crc_string, sizeof crc_string, "%.4x", crc);
            SDL_strlcat(mapping, crc_string, SDL_arraysize(mapping));
            SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
        }

        for (iIndex = 0; iIndex < SDL_arraysize(s_arrBindings); ++iIndex) {
            SDL_GameControllerExtendedBind *pBinding = &s_arrBindings[iIndex];
            if (pBinding->bindType == SDL_GAMEPAD_BINDTYPE_NONE) {
                continue;
            }

            if (iIndex < SDL_GAMEPAD_BUTTON_MAX) {
                SDL_GamepadButton eButton = (SDL_GamepadButton)iIndex;
                SDL_strlcat(mapping, SDL_GetGamepadStringForButton(eButton), SDL_arraysize(mapping));
            } else {
                const char *pszAxisName = NULL;
                switch (iIndex - SDL_GAMEPAD_BUTTON_MAX) {
                case SDL_GAMEPAD_BINDING_AXIS_LEFTX_NEGATIVE:
                    if (!BMergeAxisBindings(iIndex)) {
                        SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                    }
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTX);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_LEFTX_POSITIVE:
                    SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTX);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_LEFTY_NEGATIVE:
                    if (!BMergeAxisBindings(iIndex)) {
                        SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                    }
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTY);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_LEFTY_POSITIVE:
                    SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFTY);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_RIGHTX_NEGATIVE:
                    if (!BMergeAxisBindings(iIndex)) {
                        SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                    }
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTX);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_RIGHTX_POSITIVE:
                    SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTX);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_RIGHTY_NEGATIVE:
                    if (!BMergeAxisBindings(iIndex)) {
                        SDL_strlcat(mapping, "-", SDL_arraysize(mapping));
                    }
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTY);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_RIGHTY_POSITIVE:
                    SDL_strlcat(mapping, "+", SDL_arraysize(mapping));
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHTY);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_TRIGGERLEFT:
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
                    break;
                case SDL_GAMEPAD_BINDING_AXIS_TRIGGERRIGHT:
                    pszAxisName = SDL_GetGamepadStringForAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
                    break;
                }
                if (pszAxisName) {
                    SDL_strlcat(mapping, pszAxisName, SDL_arraysize(mapping));
                }
            }
            SDL_strlcat(mapping, ":", SDL_arraysize(mapping));

            pszElement[0] = '\0';
            switch (pBinding->bindType) {
            case SDL_GAMEPAD_BINDTYPE_BUTTON:
                (void)SDL_snprintf(pszElement, sizeof pszElement, "b%d", pBinding->value.button);
                break;
            case SDL_GAMEPAD_BINDTYPE_AXIS:
                if (pBinding->value.axis.axis_min == 0 && pBinding->value.axis.axis_max == SDL_JOYSTICK_AXIS_MIN) {
                    /* The negative half axis */
                    (void)SDL_snprintf(pszElement, sizeof pszElement, "-a%d", pBinding->value.axis.axis);
                } else if (pBinding->value.axis.axis_min == 0 && pBinding->value.axis.axis_max == SDL_JOYSTICK_AXIS_MAX) {
                    /* The positive half axis */
                    (void)SDL_snprintf(pszElement, sizeof pszElement, "+a%d", pBinding->value.axis.axis);
                } else {
                    (void)SDL_snprintf(pszElement, sizeof pszElement, "a%d", pBinding->value.axis.axis);
                    if (pBinding->value.axis.axis_min > pBinding->value.axis.axis_max) {
                        /* Invert the axis */
                        SDL_strlcat(pszElement, "~", SDL_arraysize(pszElement));
                    }
                }
                break;
            case SDL_GAMEPAD_BINDTYPE_HAT:
                (void)SDL_snprintf(pszElement, sizeof pszElement, "h%d.%d", pBinding->value.hat.hat, pBinding->value.hat.hat_mask);
                break;
            default:
                SDL_assert(!"Unknown bind type");
                break;
            }
            SDL_strlcat(mapping, pszElement, SDL_arraysize(mapping));
            SDL_strlcat(mapping, ",", SDL_arraysize(mapping));
        }

        SDL_Log("Mapping:\n\n%s\n\n", mapping);
        /* Print to stdout as well so the user can cat the output somewhere */
        printf("%s\n", mapping);
    }

    SDL_free(s_arrAxisState);
    s_arrAxisState = NULL;

    SDL_DestroyRenderer(screen);
}

static SDL_bool HasJoysticks()
{
    int num_joysticks = 0;
    SDL_JoystickID *joysticks;

    joysticks = SDL_GetJoysticks(&num_joysticks);
    if (joysticks) {
        SDL_free(joysticks);
    }
    if (num_joysticks > 0) {
        return SDL_TRUE;
    } else {
        return SDL_FALSE;
    }
}

int main(int argc, char *argv[])
{
    const char *name;
    int i;
    int num_joysticks = 0;
    SDL_JoystickID *joysticks;
    int joystick_index;
    SDL_Joystick *joystick = NULL;

    SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, "0");

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    if (argv[1] && SDL_strcmp(argv[1], "--bind-touchpad") == 0) {
        bind_touchpad = SDL_TRUE;
    }

    /* Create a window to display joystick axis position */
    window = SDL_CreateWindow("Game Controller Map", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                              SCREEN_HEIGHT, 0);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return 2;
    }

    screen = SDL_CreateRenderer(window, NULL, 0);
    if (screen == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        return 2;
    }

    while (!done && !HasJoysticks()) {
        SDL_Event event;

        while (SDL_PollEvent(&event) > 0) {
            switch (event.type) {
            case SDL_EVENT_KEY_DOWN:
                if (event.key.keysym.sym != SDLK_ESCAPE) {
                    break;
                }
                SDL_FALLTHROUGH;
            case SDL_EVENT_QUIT:
                done = SDL_TRUE;
                break;
            default:
                break;
            }
        }
        SDL_RenderPresent(screen);
    }

    /* Print information about the joysticks */
    joysticks = SDL_GetJoysticks(&num_joysticks);
    if (joysticks) {
        SDL_Log("There are %d joysticks attached\n", num_joysticks);
        for (i = 0; joysticks[i]; ++i) {
            SDL_JoystickID instance_id = joysticks[i];

            name = SDL_GetJoystickInstanceName(instance_id);
            SDL_Log("Joystick %" SDL_PRIu32 ": %s\n", instance_id, name ? name : "Unknown Joystick");
            joystick = SDL_OpenJoystick(instance_id);
            if (joystick == NULL) {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_OpenJoystick(%" SDL_PRIu32 ") failed: %s\n", instance_id,
                             SDL_GetError());
            } else {
                char guid[64];
                SDL_GetJoystickGUIDString(SDL_GetJoystickGUID(joystick),
                                          guid, sizeof(guid));
                SDL_Log("       axes: %d\n", SDL_GetNumJoystickAxes(joystick));
                SDL_Log("       hats: %d\n", SDL_GetNumJoystickHats(joystick));
                SDL_Log("    buttons: %d\n", SDL_GetNumJoystickButtons(joystick));
                SDL_Log("instance id: %" SDL_PRIu32 "\n", instance_id);
                SDL_Log("       guid: %s\n", guid);
                SDL_Log("    VID/PID: 0x%.4x/0x%.4x\n", SDL_GetJoystickVendor(joystick), SDL_GetJoystickProduct(joystick));
                SDL_CloseJoystick(joystick);
            }
        }
    }

    joystick_index = 0;
    for (i = 1; i < argc; ++i) {
        if (argv[i] && *argv[i] != '-') {
            joystick_index = SDL_atoi(argv[i]);
            break;
        }
    }
    if (joysticks && joystick_index < num_joysticks) {
        joystick = SDL_OpenJoystick(joysticks[joystick_index]);
        if (joystick == NULL) {
            SDL_Log("Couldn't open joystick %d: %s\n", joystick_index, SDL_GetError());
        }
    }
    if (joystick != NULL) {
        WatchJoystick(joystick);
        SDL_CloseJoystick(joystick);
    }

    SDL_free(joysticks);

    SDL_DestroyWindow(window);

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);

    return 0;
}
