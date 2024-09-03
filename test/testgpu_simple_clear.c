/*
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_test.h>

/* we don't actually use any shaders in this one, so just give us lots of options for backends. */
#define TESTGPU_SUPPORTED_FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB)

static SDLTest_CommonState *state;
static SDL_GPUDevice *gpu_device;
static Uint64 then = 0;
static Uint64 frames = 0;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    const SDL_DisplayMode *mode;
    int dw, dh;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    /* Enable standard application logging */
    SDL_SetLogPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);

    state->skip_renderer = 1;

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return SDL_APP_FAILURE;
    }

	gpu_device = SDL_CreateGPUDevice(TESTGPU_SUPPORTED_FORMATS, SDL_TRUE, NULL);
	if (!gpu_device) {
		SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
		return -1;
	}

	if (!SDL_ClaimWindowForGPUDevice(gpu_device, state->windows[0])) {
		SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
		return -1;
	}

    mode = SDL_GetCurrentDisplayMode(SDL_GetPrimaryDisplay());
    if (mode) {
        SDL_Log("Screen BPP    : %d\n", SDL_BITSPERPIXEL(mode->format));
    }
    SDL_GetWindowSize(state->windows[0], &dw, &dh);
    SDL_Log("Window Size   : %d,%d\n", dw, dh);
    SDL_GetWindowSizeInPixels(state->windows[0], &dw, &dh);
    SDL_Log("Draw Size     : %d,%d\n", dw, dh);
    SDL_Log("\n");

    then = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    return SDLTest_CommonEventMainCallbacks(state, event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
	Uint32 w, h;
	SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(gpu_device);

	if (cmdbuf == NULL) {
		SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
	}

	SDL_GPUTexture *swapchainTexture = SDL_AcquireGPUSwapchainTexture(cmdbuf, state->windows[0], &w, &h);
	if (swapchainTexture != NULL) {
        const double currentTime = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
        SDL_GPURenderPass *renderPass;
		SDL_GPUColorAttachmentInfo colorAttachmentInfo;
        SDL_zero(colorAttachmentInfo);
		colorAttachmentInfo.texture = swapchainTexture;
		colorAttachmentInfo.clearColor.r = (float)(0.5 + 0.5 * SDL_sin(currentTime));
		colorAttachmentInfo.clearColor.g = (float)(0.5 + 0.5 * SDL_sin(currentTime + SDL_PI_D * 2 / 3));
		colorAttachmentInfo.clearColor.b = (float)(0.5 + 0.5 * SDL_sin(currentTime + SDL_PI_D * 4 / 3));;
		colorAttachmentInfo.clearColor.a = 1.0f;
		colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_CLEAR;
		colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

		renderPass = SDL_BeginGPURenderPass(cmdbuf, &colorAttachmentInfo, 1, NULL);
		SDL_EndGPURenderPass(renderPass);
	}

	SDL_SubmitGPUCommandBuffer(cmdbuf);

    frames++;

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate)
{
    /* Print out some timing information */
    const Uint64 now = SDL_GetTicks();
    if (now > then) {
        SDL_Log("%2.2f frames per second\n", ((double)frames * 1000) / (now - then));
    }

	SDL_ReleaseWindowFromGPUDevice(gpu_device, state->windows[0]);
	SDL_DestroyGPUDevice(gpu_device);
    SDLTest_CommonQuit(state);
}

