/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/* This is the equivalent of testvulkan.c (just clears the screen, fading colors). */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "SDL_test_common.h"
#include "SDL_gpu.h"

typedef struct GpuContext
{
    SDL_Window *window;
    SDL_GpuDevice *device;
} GpuContext;

static SDLTest_CommonState *state;
static GpuContext *gpuContexts = NULL;  // an array of state->num_windows items
static GpuContext *gpuContext = NULL;  // for the currently-rendering window

static void shutdownGpu(void);

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc)
{
    shutdownGpu();
    SDLTest_CommonQuit(state);
    exit(rc);
}

static void initGpu(void)
{
    int i;

    gpuContexts = (GpuContext *) SDL_calloc(state->num_windows, sizeof (GpuContext));
    if (!gpuContexts) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Out of memory!");
        quit(2);
    }

    for (i = 0; i < state->num_windows; ++i) {
        char label[64];
        gpuContext = &gpuContexts[i];
        gpuContext->window = state->windows[i];
        SDL_snprintf(label, sizeof (label), "Window #%d", i);
        gpuContext->device = SDL_GpuCreateDevice(label, state->windows[i]);
    }
}

static void shutdownGpu(void)
{
    if (gpuContexts) {
        int i;
        for (i = 0; i < state->num_windows; ++i) {
            gpuContext = &gpuContexts[i];
            SDL_GpuDestroyDevice(gpuContext->device);
        }
        SDL_free(gpuContexts);
        gpuContexts = NULL;
    }
}

static SDL_bool render(void)
{
    double currentTime;
    SDL_GpuColorAttachmentDescription color_desc;
    SDL_GpuCommandBuffer *cmd;
    SDL_GpuRenderPass *pass;

    cmd = SDL_GpuCreateCommandBuffer("empty command buffer", gpuContext->device);
    if (!cmd) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GpuCreateCommandBuffer(): %s\n", SDL_GetError());
        quit(2);
    }

    currentTime = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();

    SDL_zero(color_desc);
    color_desc.texture = SDL_GpuGetBackbuffer(gpuContext->device);
    color_desc.color_init = SDL_GPUPASSINIT_CLEAR;
    color_desc.clear_red = (float)(0.5 + 0.5 * SDL_sin(currentTime));
    color_desc.clear_green = (float)(0.5 + 0.5 * SDL_sin(currentTime + M_PI * 2 / 3));
    color_desc.clear_blue = (float)(0.5 + 0.5 * SDL_sin(currentTime + M_PI * 4 / 3));
    color_desc.clear_alpha = 1.0f;

    pass = SDL_GpuStartRenderPass("just-clear-the-screen render pass", cmd, 1, &color_desc, NULL, NULL);
    if (!pass) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GpuStartRenderPass(): %s\n", SDL_GetError());
        quit(2);
    }

    /* literally nothing to do, we just start a pass to say "clear the framebuffer to this color," present, and we're done. */
    SDL_GpuSubmitCommandBuffers(&cmd, 1, SDL_TRUE, NULL);

    return SDL_TRUE;
}

int main(int argc, char **argv)
{
    int done;
    SDL_DisplayMode mode;
    SDL_Event event;
    Uint32 then, now, frames;
    SDL_GpuTextureDescription texdesc;
    int dw, dh;

    /* Enable standard application logging */
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if(!state) {
        return 1;
    }

    state->skip_renderer = 1;

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return 1;
    }

    initGpu();

    SDL_GetCurrentDisplayMode(0, &mode);
    SDL_Log("Screen BPP    : %d\n", SDL_BITSPERPIXEL(mode.format));
    SDL_GetWindowSize(state->windows[0], &dw, &dh);
    SDL_Log("Window Size   : %d,%d\n", dw, dh);
    SDL_GpuGetTextureDescription(SDL_GpuGetBackbuffer(gpuContext->device), &texdesc);
    SDL_Log("Draw Size     : %d,%d\n", (int) texdesc.width, (int) texdesc.height);
    SDL_Log("\n");

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;
    while (!done) {
        /* Check for events */
        frames++;
        while(SDL_PollEvent(&event)) {
            SDLTest_CommonEvent(state, &event, &done);
        }

        if (!done) {
            int i;
            for (i = 0; i < state->num_windows; ++i) {
                if (state->windows[i]) {
                    gpuContext = &gpuContexts[i];
                    render();
                }
            }
        }
    }

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        SDL_Log("%2.2f frames per second\n", ((double)frames * 1000) / (now - then));
    }

    shutdownGpu();
    SDLTest_CommonQuit(state);
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */

