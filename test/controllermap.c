/*
  Copyright (C) 1997-2015 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* Game controller mapping generator */
/* Gabriel Jacobo <gabomdq@gmail.com> */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "SDL.h"

#ifndef SDL_JOYSTICK_DISABLED

#ifdef __IPHONEOS__
#define SCREEN_WIDTH    320
#define SCREEN_HEIGHT   480
#else
#define SCREEN_WIDTH    512
#define SCREEN_HEIGHT   317
#endif

#define MAP_WIDTH 512
#define MAP_HEIGHT 317

#define MARKER_BUTTON 1
#define MARKER_AXIS 2

typedef struct MappingStep
{
    int x, y;
    double angle;
    int marker;
    char *field;
    int axis, button, hat, hat_value;
    char mapping[4096];
}MappingStep;


SDL_Texture *
LoadTexture(SDL_Renderer *renderer, char *file, SDL_bool transparent)
{
    SDL_Surface *temp;
    SDL_Texture *texture;

    /* Load the sprite image */
    temp = SDL_LoadBMP(file);
    if (temp == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s", file, SDL_GetError());
        return NULL;
    }

    /* Set transparent pixel as the pixel at (0,0) */
    if (transparent) {
        if (temp->format->palette) {
            SDL_SetColorKey(temp, SDL_TRUE, *(Uint8 *) temp->pixels);
        } else {
            switch (temp->format->BitsPerPixel) {
            case 15:
                SDL_SetColorKey(temp, SDL_TRUE,
                                (*(Uint16 *) temp->pixels) & 0x00007FFF);
                break;
            case 16:
                SDL_SetColorKey(temp, SDL_TRUE, *(Uint16 *) temp->pixels);
                break;
            case 24:
                SDL_SetColorKey(temp, SDL_TRUE,
                                (*(Uint32 *) temp->pixels) & 0x00FFFFFF);
                break;
            case 32:
                SDL_SetColorKey(temp, SDL_TRUE, *(Uint32 *) temp->pixels);
                break;
            }
        }
    }

    /* Create textures from the image */
    texture = SDL_CreateTextureFromSurface(renderer, temp);
    if (!texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture: %s\n", SDL_GetError());
        SDL_FreeSurface(temp);
        return NULL;
    }
    SDL_FreeSurface(temp);

    /* We're ready to roll. :) */
    return texture;
}

static SDL_bool
WatchJoystick(SDL_Joystick * joystick)
{
    SDL_Window *window = NULL;
    SDL_Renderer *screen = NULL;
    SDL_Texture *background, *button, *axis, *marker;
    const char *name = NULL;
    SDL_bool retval = SDL_FALSE;
    SDL_bool done = SDL_FALSE, next=SDL_FALSE;
    SDL_Event event;
    SDL_Rect dst;
    int s, _s;
    Uint8 alpha=200, alpha_step = -1;
    Uint32 alpha_ticks = 0;
    char mapping[4096], temp[4096];
    MappingStep *step, *prev_step;
    MappingStep steps[] = {
        {342, 132,  0.0,  MARKER_BUTTON, "x", -1, -1, -1, -1, ""},
        {387, 167,  0.0,  MARKER_BUTTON, "a", -1, -1, -1, -1, ""},
        {431, 132,  0.0,  MARKER_BUTTON, "b", -1, -1, -1, -1, ""},
        {389, 101,  0.0,  MARKER_BUTTON, "y", -1, -1, -1, -1, ""},
        {174, 132,  0.0,  MARKER_BUTTON, "back", -1, -1, -1, -1, ""},
        {233, 132,  0.0,  MARKER_BUTTON, "guide", -1, -1, -1, -1, ""},
        {289, 132,  0.0,  MARKER_BUTTON, "start", -1, -1, -1, -1, ""},        
        {116, 217,  0.0,  MARKER_BUTTON, "dpleft", -1, -1, -1, -1, ""},
        {154, 249,  0.0,  MARKER_BUTTON, "dpdown", -1, -1, -1, -1, ""},
        {186, 217,  0.0,  MARKER_BUTTON, "dpright", -1, -1, -1, -1, ""},
        {154, 188,  0.0,  MARKER_BUTTON, "dpup", -1, -1, -1, -1, ""},
        {77,  40,   0.0,  MARKER_BUTTON, "leftshoulder", -1, -1, -1, -1, ""},
        {91, 0,    0.0,  MARKER_BUTTON, "lefttrigger", -1, -1, -1, -1, ""},
        {396, 36,   0.0,  MARKER_BUTTON, "rightshoulder", -1, -1, -1, -1, ""},
        {375, 0,    0.0,  MARKER_BUTTON, "righttrigger", -1, -1, -1, -1, ""},
        {75,  154,  0.0,  MARKER_BUTTON, "leftstick", -1, -1, -1, -1, ""},
        {305, 230,  0.0,  MARKER_BUTTON, "rightstick", -1, -1, -1, -1, ""},
        {75,  154,  0.0,  MARKER_AXIS,   "leftx", -1, -1, -1, -1, ""},
        {75,  154,  90.0, MARKER_AXIS,   "lefty", -1, -1, -1, -1, ""},        
        {305, 230,  0.0,  MARKER_AXIS,   "rightx", -1, -1, -1, -1, ""},
        {305, 230,  90.0, MARKER_AXIS,   "righty", -1, -1, -1, -1, ""},
    };

    /* Create a window to display joystick axis position */
    window = SDL_CreateWindow("Game Controller Map", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH,
                              SCREEN_HEIGHT, 0);
    if (window == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window: %s\n", SDL_GetError());
        return SDL_FALSE;
    }

    screen = SDL_CreateRenderer(window, -1, 0);
    if (screen == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create renderer: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        return SDL_FALSE;
    }
    
    background = LoadTexture(screen, "controllermap.bmp", SDL_FALSE);
    button = LoadTexture(screen, "button.bmp", SDL_TRUE);
    axis = LoadTexture(screen, "axis.bmp", SDL_TRUE);
    SDL_RaiseWindow(window);

    /* scale for platforms that don't give you the window size you asked for. */
    SDL_RenderSetLogicalSize(screen, SCREEN_WIDTH, SCREEN_HEIGHT);

    /* Print info about the joystick we are watching */
    name = SDL_JoystickName(joystick);
    SDL_Log("Watching joystick %d: (%s)\n", SDL_JoystickInstanceID(joystick),
           name ? name : "Unknown Joystick");
    SDL_Log("Joystick has %d axes, %d hats, %d balls, and %d buttons\n",
           SDL_JoystickNumAxes(joystick), SDL_JoystickNumHats(joystick),
           SDL_JoystickNumBalls(joystick), SDL_JoystickNumButtons(joystick));
    
    SDL_Log("\n\n\
    ====================================================================================\n\
    Press the buttons on your controller when indicated\n\
    (Your controller may look different than the picture)\n\
    If you want to correct a mistake, press backspace or the back button on your device\n\
    To skip a button, press SPACE or click/touch the screen\n\
    To exit, press ESC\n\
    ====================================================================================\n");
    
    /* Initialize mapping with GUID and name */
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick), temp, SDL_arraysize(temp));
    SDL_snprintf(mapping, SDL_arraysize(mapping), "%s,%s,platform:%s,",
        temp, name ? name : "Unknown Joystick", SDL_GetPlatform());

    /* Loop, getting joystick events! */
    for(s=0; s<SDL_arraysize(steps) && !done;) {
        /* blank screen, set up for drawing this frame. */
        step = &steps[s];
        SDL_strlcpy(step->mapping, mapping, SDL_arraysize(step->mapping));
        step->axis = -1;
        step->button = -1;
        step->hat = -1;
        step->hat_value = -1;
        
        switch(step->marker) {
            case MARKER_AXIS:
                marker = axis;
                break;
            case MARKER_BUTTON:
                marker = button;
                break;
            default:
                break;
        }
        
        dst.x = step->x;
        dst.y = step->y;
        SDL_QueryTexture(marker, NULL, NULL, &dst.w, &dst.h);
        next=SDL_FALSE;

        SDL_SetRenderDrawColor(screen, 0xFF, 0xFF, 0xFF, SDL_ALPHA_OPAQUE);

        while (!done && !next) {
            if (SDL_GetTicks() - alpha_ticks > 5) {
                alpha_ticks = SDL_GetTicks();
                alpha += alpha_step;
                if (alpha == 255) {
                    alpha_step = -1;
                }
                if (alpha < 128) {
                    alpha_step = 1;
                }
            }
            
            SDL_RenderClear(screen);
            SDL_RenderCopy(screen, background, NULL, NULL);
            SDL_SetTextureAlphaMod(marker, alpha);
            SDL_SetTextureColorMod(marker, 10, 255, 21);
            SDL_RenderCopyEx(screen, marker, NULL, &dst, step->angle, NULL, 0);
            SDL_RenderPresent(screen);
            
            if (SDL_PollEvent(&event)) {
                switch (event.type) {
                case SDL_JOYAXISMOTION:
                    if (event.jaxis.value > 20000 || event.jaxis.value < -20000) {
                        for (_s = 0; _s < s; _s++) {
                            if (steps[_s].axis == event.jaxis.axis) {
                                break;
                            }
                        }
                        if (_s == s) {
                            step->axis = event.jaxis.axis;
                            SDL_strlcat(mapping, step->field, SDL_arraysize(mapping));
                            SDL_snprintf(temp, SDL_arraysize(temp), ":a%u,", event.jaxis.axis);
                            SDL_strlcat(mapping, temp, SDL_arraysize(mapping));
                            s++;
                            next=SDL_TRUE;
                        }
                    }
                    
                    break;
                case SDL_JOYHATMOTION:
                        if (event.jhat.value == SDL_HAT_CENTERED) {
                            break;  /* ignore centering, we're probably just coming back to the center from the previous item we set. */
                        }
                        for (_s = 0; _s < s; _s++) {
                            if (steps[_s].hat == event.jhat.hat && steps[_s].hat_value == event.jhat.value) {
                                break;
                            }
                        }
                        if (_s == s) {
                            step->hat = event.jhat.hat;
                            step->hat_value = event.jhat.value;
                            SDL_strlcat(mapping, step->field, SDL_arraysize(mapping));
                            SDL_snprintf(temp, SDL_arraysize(temp), ":h%u.%u,", event.jhat.hat, event.jhat.value );
                            SDL_strlcat(mapping, temp, SDL_arraysize(mapping));
                            s++;
                            next=SDL_TRUE;
                        }
                    break;
                case SDL_JOYBALLMOTION:
                    break;
                case SDL_JOYBUTTONUP:
                    for (_s = 0; _s < s; _s++) {
                        if (steps[_s].button == event.jbutton.button) {
                            break;
                        }
                    }
                    if (_s == s) {
                        step->button = event.jbutton.button;
                        SDL_strlcat(mapping, step->field, SDL_arraysize(mapping));
                        SDL_snprintf(temp, SDL_arraysize(temp), ":b%u,", event.jbutton.button);
                        SDL_strlcat(mapping, temp, SDL_arraysize(mapping));
                        s++;
                        next=SDL_TRUE;
                    }
                    break;
                case SDL_FINGERDOWN:
                case SDL_MOUSEBUTTONDOWN:
                    /* Skip this step */
                    s++;
                    next=SDL_TRUE;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_BACKSPACE || event.key.keysym.sym == SDLK_AC_BACK) {
                        /* Undo! */
                        if (s > 0) {
                            prev_step = &steps[--s];
                            SDL_strlcpy(mapping, prev_step->mapping, SDL_arraysize(prev_step->mapping));
                            next = SDL_TRUE;
                        }
                        break;
                    }
                    if (event.key.keysym.sym == SDLK_SPACE) {
                        /* Skip this step */
                        s++;
                        next=SDL_TRUE;
                        break;
                    }
                    
                    if ((event.key.keysym.sym != SDLK_ESCAPE)) {
                        break;
                    }
                    /* Fall through to signal quit */
                case SDL_QUIT:
                    done = SDL_TRUE;
                    break;
                default:
                    break;
                }
            }
        }

    }

    if (s == SDL_arraysize(steps) ) {
        SDL_Log("Mapping:\n\n%s\n\n", mapping);
        /* Print to stdout as well so the user can cat the output somewhere */
        printf("%s\n", mapping);
    }
    
    while(SDL_PollEvent(&event)) {};
    
    SDL_DestroyRenderer(screen);
    SDL_DestroyWindow(window);
    return retval;
}

int
main(int argc, char *argv[])
{
    const char *name;
    int i;
    SDL_Joystick *joystick;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);	

    /* Initialize SDL (Note: video is required to start event loop) */
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }

    /* Print information about the joysticks */
    SDL_Log("There are %d joysticks attached\n", SDL_NumJoysticks());
    for (i = 0; i < SDL_NumJoysticks(); ++i) {
        name = SDL_JoystickNameForIndex(i);
        SDL_Log("Joystick %d: %s\n", i, name ? name : "Unknown Joystick");
        joystick = SDL_JoystickOpen(i);
        if (joystick == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_JoystickOpen(%d) failed: %s\n", i,
                    SDL_GetError());
        } else {
            char guid[64];
            SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joystick),
                                      guid, sizeof (guid));
            SDL_Log("       axes: %d\n", SDL_JoystickNumAxes(joystick));
            SDL_Log("      balls: %d\n", SDL_JoystickNumBalls(joystick));
            SDL_Log("       hats: %d\n", SDL_JoystickNumHats(joystick));
            SDL_Log("    buttons: %d\n", SDL_JoystickNumButtons(joystick));
            SDL_Log("instance id: %d\n", SDL_JoystickInstanceID(joystick));
            SDL_Log("       guid: %s\n", guid);
            SDL_JoystickClose(joystick);
        }
    }

#ifdef __ANDROID__
    if (SDL_NumJoysticks() > 0) {
#else
    if (argv[1]) {
#endif
        SDL_bool reportederror = SDL_FALSE;
        SDL_bool keepGoing = SDL_TRUE;
        SDL_Event event;
        int device;
#ifdef __ANDROID__
        device = 0;
#else
        device = atoi(argv[1]);
#endif
        joystick = SDL_JoystickOpen(device);

        while ( keepGoing ) {
            if (joystick == NULL) {
                if ( !reportederror ) {
                    SDL_Log("Couldn't open joystick %d: %s\n", device, SDL_GetError());
                    keepGoing = SDL_FALSE;
                    reportederror = SDL_TRUE;
                }
            } else {
                reportederror = SDL_FALSE;
                keepGoing = WatchJoystick(joystick);
                SDL_JoystickClose(joystick);
            }

            joystick = NULL;
            if (keepGoing) {
                SDL_Log("Waiting for attach\n");
            }
            while (keepGoing) {
                SDL_WaitEvent(&event);
                if ((event.type == SDL_QUIT) || (event.type == SDL_FINGERDOWN)
                    || (event.type == SDL_MOUSEBUTTONDOWN)) {
                    keepGoing = SDL_FALSE;
                } else if (event.type == SDL_JOYDEVICEADDED) {
                    joystick = SDL_JoystickOpen(device);
                    break;
                }
            }
        }
    }
    else {
        SDL_Log("\n\nUsage: ./controllermap number\nFor example: ./controllermap 0\nOr: ./controllermap 0 >> gamecontrollerdb.txt");
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);

    return 0;
}

#else

int
main(int argc, char *argv[])
{
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL compiled without Joystick support.\n");
    exit(1);
}

#endif
