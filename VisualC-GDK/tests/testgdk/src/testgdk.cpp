/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
/* testgdk:  Basic tests of using task queue/xbl (with simple drawing) in GDK.
 * NOTE: As of June 2022 GDK, login will only work if MicrosoftGame.config is
 * configured properly. See README-gdk.md.
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "SDL_test.h"
#include "SDL_test_common.h"
#include "../src/core/windows/SDL_windows.h"

extern "C" {
#include "../test/testutils.h"
}

#include <XGameRuntime.h>

#define NUM_SPRITES    100
#define MAX_SPEED     1

static SDLTest_CommonState *state;
static int num_sprites;
static SDL_Texture **sprites;
static SDL_bool cycle_color;
static SDL_bool cycle_alpha;
static int cycle_direction = 1;
static int current_alpha = 0;
static int current_color = 0;
static int sprite_w, sprite_h;
static SDL_BlendMode blendMode = SDL_BLENDMODE_BLEND;

int done;

static struct
{
    SDL_AudioSpec spec;
    Uint8 *sound;    /* Pointer to wave data */
    Uint32 soundlen; /* Length of wave data */
    int soundpos;    /* Current play position */
} wave;

static SDL_AudioDeviceID device;

static void
close_audio()
{
    if (device != 0) {
        SDL_CloseAudioDevice(device);
        device = 0;
    }
}

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    SDL_free(sprites);
    close_audio();
    SDL_FreeWAV(wave.sound);
    SDLTest_CommonQuit(state);
    /* If rc is 0, just let main return normally rather than calling exit.
     * This allows testing of platforms where SDL_main is required and does meaningful cleanup.
     */
    if (rc != 0) {
        exit(rc);
    }
}

static void
open_audio()
{
    /* Initialize fillerup() variables */
    device = SDL_OpenAudioDevice(NULL, SDL_FALSE, &wave.spec, NULL, 0);
    if (!device) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open audio: %s\n", SDL_GetError());
        SDL_FreeWAV(wave.sound);
        quit(2);
    }

    /* Let the audio run */
    SDL_PauseAudioDevice(device, SDL_FALSE);
}

static void
reopen_audio()
{
    close_audio();
    open_audio();
}

void SDLCALL
fillerup(void *unused, Uint8 *stream, int len)
{
    Uint8 *waveptr;
    int waveleft;

    /* Set up the pointers */
    waveptr = wave.sound + wave.soundpos;
    waveleft = wave.soundlen - wave.soundpos;

    /* Go! */
    while (waveleft <= len) {
        SDL_memcpy(stream, waveptr, waveleft);
        stream += waveleft;
        len -= waveleft;
        waveptr = wave.sound;
        waveleft = wave.soundlen;
        wave.soundpos = 0;
    }
    SDL_memcpy(stream, waveptr, len);
    wave.soundpos += len;
}

void
UserLoggedIn(XUserHandle user)
{
    HRESULT hr;
    char gamertag[128];
    hr = XUserGetGamertag(user, XUserGamertagComponent::UniqueModern, sizeof(gamertag), gamertag, NULL);

    if (SUCCEEDED(hr)) {
        SDL_Log("User logged in: %s", gamertag);
    } else {
        SDL_Log("[GDK] UserLoggedIn -- XUserGetGamertag failed: 0x%08x.", hr);
    }

    XUserCloseHandle(user);
}

void
AddUserUICallback(XAsyncBlock *asyncBlock)
{
    HRESULT hr;
    XUserHandle user = NULL;

    hr = XUserAddResult(asyncBlock, &user);
    if (SUCCEEDED(hr)) {
        uint64_t userId;

        hr = XUserGetId(user, &userId);
        if (FAILED(hr)) {
            /* If unable to get the user ID, it means the account is banned, etc. */
            SDL_Log("[GDK] AddUserSilentCallback -- XUserGetId failed: 0x%08x.", hr);
            XUserCloseHandle(user);

            /* Per the docs, likely should call XUserResolveIssueWithUiAsync here. */
        } else {
            UserLoggedIn(user);
        }
    } else {
        SDL_Log("[GDK] AddUserUICallback -- XUserAddAsync failed: 0x%08x.", hr);
    }

    delete asyncBlock;
}

void
AddUserUI()
{
    HRESULT hr;
    XAsyncBlock *asyncBlock = new XAsyncBlock;

    asyncBlock->context = NULL;
    asyncBlock->queue = NULL; /* A null queue will use the global process task queue */
    asyncBlock->callback = &AddUserUICallback;

    hr = XUserAddAsync(XUserAddOptions::None, asyncBlock);

    if (FAILED(hr)) {
        delete asyncBlock;
        SDL_Log("[GDK] AddUserSilent -- failed: 0x%08x", hr);
    }
}

void
AddUserSilentCallback(XAsyncBlock *asyncBlock)
{
    HRESULT hr;
    XUserHandle user = NULL;

    hr = XUserAddResult(asyncBlock, &user);
    if (SUCCEEDED(hr)) {
        uint64_t userId;

        hr = XUserGetId(user, &userId);
        if (FAILED(hr)) {
            /* If unable to get the user ID, it means the account is banned, etc. */
            SDL_Log("[GDK] AddUserSilentCallback -- XUserGetId failed: 0x%08x. Trying with UI.", hr);
            XUserCloseHandle(user);
            AddUserUI();
        } else {
            UserLoggedIn(user);
        }
    } else {
        SDL_Log("[GDK] AddUserSilentCallback -- XUserAddAsync failed: 0x%08x. Trying with UI.", hr);
        AddUserUI();
    }

    delete asyncBlock;
}

void
AddUserSilent()
{
    HRESULT hr;
    XAsyncBlock *asyncBlock = new XAsyncBlock;

    asyncBlock->context = NULL;
    asyncBlock->queue = NULL; /* A null queue will use the global process task queue */
    asyncBlock->callback = &AddUserSilentCallback;

    hr = XUserAddAsync(XUserAddOptions::AddDefaultUserSilently, asyncBlock);

    if (FAILED(hr)) {
        delete asyncBlock;
        SDL_Log("[GDK] AddUserSilent -- failed: 0x%08x", hr);
    }
}

int
LoadSprite(const char *file)
{
    int i;

    for (i = 0; i < state->num_windows; ++i) {
        /* This does the SDL_LoadBMP step repeatedly, but that's OK for test code. */
        sprites[i] = LoadTexture(state->renderers[i], file, SDL_TRUE, &sprite_w, &sprite_h);
        if (!sprites[i]) {
            return -1;
        }
        if (SDL_SetTextureBlendMode(sprites[i], blendMode) < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't set blend mode: %s\n", SDL_GetError());
            SDL_DestroyTexture(sprites[i]);
            return -1;
        }
    }

    /* We're ready to roll. :) */
    return 0;
}

void
DrawSprites(SDL_Renderer * renderer, SDL_Texture * sprite)
{
    SDL_Rect viewport, temp;

    /* Query the sizes */
    SDL_RenderGetViewport(renderer, &viewport);

    /* Cycle the color and alpha, if desired */
    if (cycle_color) {
        current_color += cycle_direction;
        if (current_color < 0) {
            current_color = 0;
            cycle_direction = -cycle_direction;
        }
        if (current_color > 255) {
            current_color = 255;
            cycle_direction = -cycle_direction;
        }
        SDL_SetTextureColorMod(sprite, 255, (Uint8) current_color,
                               (Uint8) current_color);
    }
    if (cycle_alpha) {
        current_alpha += cycle_direction;
        if (current_alpha < 0) {
            current_alpha = 0;
            cycle_direction = -cycle_direction;
        }
        if (current_alpha > 255) {
            current_alpha = 255;
            cycle_direction = -cycle_direction;
        }
        SDL_SetTextureAlphaMod(sprite, (Uint8) current_alpha);
    }

    /* Draw a gray background */
    SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
    SDL_RenderClear(renderer);

    /* Test points */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0xFF);
    SDL_RenderDrawPoint(renderer, 0, 0);
    SDL_RenderDrawPoint(renderer, viewport.w-1, 0);
    SDL_RenderDrawPoint(renderer, 0, viewport.h-1);
    SDL_RenderDrawPoint(renderer, viewport.w-1, viewport.h-1);

    /* Test horizontal and vertical lines */
    SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
    SDL_RenderDrawLine(renderer, 1, 0, viewport.w-2, 0);
    SDL_RenderDrawLine(renderer, 1, viewport.h-1, viewport.w-2, viewport.h-1);
    SDL_RenderDrawLine(renderer, 0, 1, 0, viewport.h-2);
    SDL_RenderDrawLine(renderer, viewport.w-1, 1, viewport.w-1, viewport.h-2);

    /* Test fill and copy */
    SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0xFF, 0xFF);
    temp.x = 1;
    temp.y = 1;
    temp.w = sprite_w;
    temp.h = sprite_h;
    SDL_RenderFillRect(renderer, &temp);
    SDL_RenderCopy(renderer, sprite, NULL, &temp);
    temp.x = viewport.w-sprite_w-1;
    temp.y = 1;
    temp.w = sprite_w;
    temp.h = sprite_h;
    SDL_RenderFillRect(renderer, &temp);
    SDL_RenderCopy(renderer, sprite, NULL, &temp);
    temp.x = 1;
    temp.y = viewport.h-sprite_h-1;
    temp.w = sprite_w;
    temp.h = sprite_h;
    SDL_RenderFillRect(renderer, &temp);
    SDL_RenderCopy(renderer, sprite, NULL, &temp);
    temp.x = viewport.w-sprite_w-1;
    temp.y = viewport.h-sprite_h-1;
    temp.w = sprite_w;
    temp.h = sprite_h;
    SDL_RenderFillRect(renderer, &temp);
    SDL_RenderCopy(renderer, sprite, NULL, &temp);

    /* Test diagonal lines */
    SDL_SetRenderDrawColor(renderer, 0x00, 0xFF, 0x00, 0xFF);
    SDL_RenderDrawLine(renderer, sprite_w, sprite_h,
                       viewport.w-sprite_w-2, viewport.h-sprite_h-2);
    SDL_RenderDrawLine(renderer, viewport.w-sprite_w-2, sprite_h,
                       sprite_w, viewport.h-sprite_h-2);

    /* Update the screen! */
    SDL_RenderPresent(renderer);
}

void
loop()
{
    int i;
    SDL_Event event;

    /* Check for events */
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN && !event.key.repeat) {
            SDL_Log("Initial SDL_KEYDOWN: %s", SDL_GetScancodeName(event.key.keysym.scancode));
        }
#if defined(__XBOXONE__) || defined(__XBOXSERIES__)
        /* On Xbox, ignore the keydown event because the features aren't supported */
        if (event.type != SDL_KEYDOWN) {
            SDLTest_CommonEvent(state, &event, &done);
        }
#else
        SDLTest_CommonEvent(state, &event, &done);
#endif
    }
    for (i = 0; i < state->num_windows; ++i) {
        if (state->windows[i] == NULL) {
            continue;
        }
        DrawSprites(state->renderers[i], sprites[i]);
    }
}

int
main(int argc, char *argv[])
{
    int i;
    const char *icon = "icon.bmp";
    char *soundname = NULL;

    /* Initialize parameters */
    num_sprites = NUM_SPRITES;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (!state) {
        return 1;
    }

    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            consumed = -1;
            if (SDL_strcasecmp(argv[i], "--blend") == 0) {
                if (argv[i + 1]) {
                    if (SDL_strcasecmp(argv[i + 1], "none") == 0) {
                        blendMode = SDL_BLENDMODE_NONE;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "blend") == 0) {
                        blendMode = SDL_BLENDMODE_BLEND;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "add") == 0) {
                        blendMode = SDL_BLENDMODE_ADD;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "mod") == 0) {
                        blendMode = SDL_BLENDMODE_MOD;
                        consumed = 2;
                    } else if (SDL_strcasecmp(argv[i + 1], "sub") == 0) {
                        blendMode = SDL_ComposeCustomBlendMode(SDL_BLENDFACTOR_SRC_ALPHA, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_SUBTRACT, SDL_BLENDFACTOR_ZERO, SDL_BLENDFACTOR_ONE, SDL_BLENDOPERATION_SUBTRACT);
                        consumed = 2;
                    }
                }
            } else if (SDL_strcasecmp(argv[i], "--cyclecolor") == 0) {
                cycle_color = SDL_TRUE;
                consumed = 1;
            } else if (SDL_strcasecmp(argv[i], "--cyclealpha") == 0) {
                cycle_alpha = SDL_TRUE;
                consumed = 1;
            } else if (SDL_isdigit(*argv[i])) {
                num_sprites = SDL_atoi(argv[i]);
                consumed = 1;
            } else if (argv[i][0] != '-') {
                icon = argv[i];
                consumed = 1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = {
                "[--blend none|blend|add|mod]",
                "[--cyclecolor]",
                "[--cyclealpha]",
                "[num_sprites]",
                "[icon.bmp]",
                NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }
        i += consumed;
    }
    if (!SDLTest_CommonInit(state)) {
        quit(2);
    }

    /* Create the windows, initialize the renderers, and load the textures */
    sprites =
        (SDL_Texture **) SDL_malloc(state->num_windows * sizeof(*sprites));
    if (!sprites) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!\n");
        quit(2);
    }
    for (i = 0; i < state->num_windows; ++i) {
        SDL_Renderer *renderer = state->renderers[i];
        SDL_SetRenderDrawColor(renderer, 0xA0, 0xA0, 0xA0, 0xFF);
        SDL_RenderClear(renderer);
    }
    if (LoadSprite(icon) < 0) {
        quit(2);
    }

    soundname = GetResourceFilename(argc > 1 ? argv[1] : NULL, "sample.wav");

    if (!soundname) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s\n", SDL_GetError());
        quit(1);
    }

    /* Load the wave file into memory */
    if (SDL_LoadWAV(soundname, &wave.spec, &wave.sound, &wave.soundlen) == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't load %s: %s\n", soundname, SDL_GetError());
        quit(1);
    }

    wave.spec.callback = fillerup;

    /* Show the list of available drivers */
    SDL_Log("Available audio drivers:");
    for (i = 0; i < SDL_GetNumAudioDrivers(); ++i) {
        SDL_Log("%i: %s", i, SDL_GetAudioDriver(i));
    }

    SDL_Log("Using audio driver: %s\n", SDL_GetCurrentAudioDriver());

    open_audio();

    /* Main render loop */
    done = 0;

    /* Try to add the default user silently */
    AddUserSilent();

    while (!done) {
        loop();
    }

    quit(0);

    SDL_free(soundname);
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
