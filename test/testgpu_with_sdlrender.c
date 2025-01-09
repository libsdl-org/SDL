/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

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
static SDL_Renderer *renderer = NULL;
static SDL_Texture *icon_texture = NULL;
static float icon_pos_x = 0.0f;

SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    const SDL_DisplayMode *mode;
    int dw, dh;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return SDL_APP_FAILURE;
    }

    state->skip_renderer = 1;

    if (!SDLTest_CommonDefaultArgs(state, argc, argv) || !SDLTest_CommonInit(state)) {
        SDLTest_CommonQuit(state);
        return SDL_APP_FAILURE;
    }

    gpu_device = SDL_CreateGPUDevice(TESTGPU_SUPPORTED_FORMATS, true, NULL);
    if (!gpu_device) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_ClaimWindowForGPUDevice(gpu_device, state->windows[0])) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
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

    SDL_PropertiesID props = SDL_CreateProperties();

    SDL_SetStringProperty(props, SDL_PROP_RENDERER_CREATE_NAME_STRING, "gpu");
    SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, state->windows[0]);
    SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_USER_GPU_DEVICE_POINTER, gpu_device);

    renderer = SDL_CreateRendererWithProperties(
        props);

    if (!renderer) {
        SDL_Log("SDL_CreateRendererWithProperties failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_Surface *bmp_surf = SDL_LoadBMP("icon.bmp");

    if (!bmp_surf) {
        SDL_Log("SDL_LoadBMP failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Creation of any resources with manually controlled GPU renderer require a command buffer to be set*/
    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(gpu_device);

    if (cmdbuf == NULL) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (!SDL_SetRenderGPUCommandBuffer(renderer, cmdbuf)) {
        SDL_Log("SDL_SetRenderGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    icon_texture = SDL_CreateTextureFromSurface(renderer, bmp_surf);

    SDL_SubmitGPUCommandBuffer(cmdbuf);

    SDL_DestroySurface(bmp_surf);

    if (!icon_texture) {
        SDL_Log("SDL_LoadBMP failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    then = SDL_GetTicks();

    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_KEY_DOWN) {
        icon_pos_x += 16.0f;
    }

    return SDLTest_CommonEventMainCallbacks(state, event);
}

SDL_AppResult SDL_AppIterate(void *appstate)
{
    SDL_GPUCommandBuffer *cmdbuf = SDL_AcquireGPUCommandBuffer(gpu_device);

    if (cmdbuf == NULL) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    SDL_GPUTexture *swapchainTexture;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmdbuf, state->windows[0], &swapchainTexture, NULL, NULL)) {
        SDL_Log("SDL_AcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    if (swapchainTexture != NULL) {
        const double currentTime = (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency();
        SDL_GPURenderPass *renderPass;
        SDL_GPUColorTargetInfo color_target_info;
        SDL_zero(color_target_info);
        color_target_info.texture = swapchainTexture;
        color_target_info.clear_color.r = (float)(0.5 + 0.5 * SDL_sin(currentTime));
        color_target_info.clear_color.g = (float)(0.5 + 0.5 * SDL_sin(currentTime + SDL_PI_D * 2 / 3));
        color_target_info.clear_color.b = (float)(0.5 + 0.5 * SDL_sin(currentTime + SDL_PI_D * 4 / 3));
        color_target_info.clear_color.a = 1.0f;
        color_target_info.load_op = SDL_GPU_LOADOP_CLEAR;
        color_target_info.store_op = SDL_GPU_STOREOP_STORE;

        /* Custom GPU rendering */
        renderPass = SDL_BeginGPURenderPass(cmdbuf, &color_target_info, 1, NULL);
        /* Render Half-Life 3 or whatever */
        SDL_EndGPURenderPass(renderPass);

        /* 2D rendering with SDL renderer */
        if (!SDL_SetRenderGPUCommandBuffer(renderer, cmdbuf)) {
            SDL_Log("SDL_SetRenderGPUCommandBuffer failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_FRect rect = { 32, 32, 64, 64 };
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        SDL_RenderFillRect(renderer, &rect);
        SDL_FRect tex_rect = { 150 + icon_pos_x, 150, 32, 32 };
        SDL_RenderTexture(renderer, icon_texture, NULL, &tex_rect);

        if (!SDL_RenderPresentToGPUTexture(renderer, swapchainTexture, SDL_GetGPUSwapchainTextureFormat(gpu_device, state->windows[0]))) {
            SDL_Log("SDL_RenderPresentToGPUTexture failed: %s", SDL_GetError());
            return SDL_APP_FAILURE;
        }

        SDL_SubmitGPUCommandBuffer(cmdbuf);
    } else {
        /* Swapchain is unavailable, cancel work */
        SDL_CancelGPUCommandBuffer(cmdbuf);
    }

    frames++;

    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* Print out some timing information */
    const Uint64 now = SDL_GetTicks();
    if (now > then) {
        SDL_Log("%2.2f frames per second\n", ((double)frames * 1000) / (now - then));
    }

    SDL_DestroyTexture(icon_texture);
    SDL_DestroyRenderer(renderer);
    SDL_ReleaseWindowFromGPUDevice(gpu_device, state->windows[0]);
    SDL_DestroyGPUDevice(gpu_device);
    SDLTest_CommonQuit(state);
}
