/*
  Copyright (C) 1997-2025 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

#define SDL_MAIN_USE_CALLBACKS 1  /* use the callbacks instead of main() */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include "testutils.h"

#include "testgpurender_effects_grayscale.frag.dxil.h"
#include "testgpurender_effects_grayscale.frag.msl.h"
#include "testgpurender_effects_grayscale.frag.spv.h"
#include "testgpurender_effects_CRT.frag.dxil.h"
#include "testgpurender_effects_CRT.frag.msl.h"
#include "testgpurender_effects_CRT.frag.spv.h"

/* The window is twice the size of the background */
#define WINDOW_WIDTH  (408 * 2)
#define WINDOW_HEIGHT (167 * 2)
#define NUM_SPRITES   15
#define MAX_SPEED     1

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *target = NULL;
static SDL_GPUDevice *device = NULL;
static SDL_Texture *background;
static SDL_Texture *sprite;
static SDL_FRect positions[NUM_SPRITES];
static SDL_FRect velocities[NUM_SPRITES];

typedef enum
{
    EFFECT_NONE,
    EFFECT_GRAYSCALE,
    EFFECT_CRT,
    NUM_EFFECTS
} FullscreenEffect;

typedef struct 
{
    const char *name;
    const unsigned char *dxil_shader_source;
    unsigned int dxil_shader_source_len;
    const unsigned char *msl_shader_source;
    unsigned int msl_shader_source_len;
    const unsigned char *spirv_shader_source;
    unsigned int spirv_shader_source_len;
    int num_samplers;
    int num_uniform_buffers;
    SDL_GPUShader *shader;
    SDL_GPURenderState *state;
} FullscreenEffectData;

typedef struct  
{
    float texture_width;
    float texture_height;
} CRTEffectUniforms;

static FullscreenEffectData effects[] = {
    {
        "NONE",
        NULL,
        0,
        NULL,
        0,
        NULL,
        0,
        0,
        0,
        NULL,
        NULL
    },
    {
        "Grayscale",
        testgpu_effects_grayscale_frag_dxil,
        sizeof(testgpu_effects_grayscale_frag_dxil),
        testgpu_effects_grayscale_frag_msl,
        sizeof(testgpu_effects_grayscale_frag_msl),
        testgpu_effects_grayscale_frag_spv,
        sizeof(testgpu_effects_grayscale_frag_spv),
        1,
        0,
        NULL,
        NULL
    },
    {
        "CRT monitor",
        testgpu_effects_CRT_frag_dxil,
        sizeof(testgpu_effects_CRT_frag_dxil),
        testgpu_effects_CRT_frag_msl,
        sizeof(testgpu_effects_CRT_frag_msl),
        testgpu_effects_CRT_frag_spv,
        sizeof(testgpu_effects_CRT_frag_spv),
        1,
        1,
        NULL,
        NULL
    }
};
SDL_COMPILE_TIME_ASSERT(effects, SDL_arraysize(effects) == NUM_EFFECTS);

static int current_effect = 0;

static void DrawScene(void)
{
    int i;
    int window_w = WINDOW_WIDTH;
    int window_h = WINDOW_HEIGHT;
    SDL_FRect *position, *velocity;

    SDL_RenderTexture(renderer, background, NULL, NULL);

    /* Move the sprite, bounce at the wall, and draw */
    for (i = 0; i < NUM_SPRITES; ++i) {
        position = &positions[i];
        velocity = &velocities[i];
        position->x += velocity->x;
        if ((position->x < 0) || (position->x >= (window_w - sprite->w))) {
            velocity->x = -velocity->x;
            position->x += velocity->x;
        }
        position->y += velocity->y;
        if ((position->y < 0) || (position->y >= (window_h - sprite->h))) {
            velocity->y = -velocity->y;
            position->y += velocity->y;
        }

        /* Blit the sprite onto the screen */
        SDL_RenderTexture(renderer, sprite, NULL, position);
    }
}

static bool InitGPURenderState(void)
{
    SDL_GPUShaderFormat formats;
    SDL_GPUShaderCreateInfo info;
    SDL_GPURenderStateDesc desc;
    int i;

    device = (SDL_GPUDevice *)SDL_GetPointerProperty(SDL_GetRendererProperties(renderer), SDL_PROP_RENDERER_GPU_DEVICE_POINTER, NULL);
    if (!device) {
        SDL_Log("Couldn't get GPU device");
        return false;
    }

    formats = SDL_GetGPUShaderFormats(device);
    if (formats == SDL_GPU_SHADERFORMAT_INVALID) {
        SDL_Log("Couldn't get supported shader formats: %s", SDL_GetError());
        return false;
    }

    for (i = 0; i < SDL_arraysize(effects); ++i) {
        FullscreenEffectData *data = &effects[i];

        if (i == EFFECT_NONE) {
            continue;
        }

        SDL_zero(info);
        if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
            info.format = SDL_GPU_SHADERFORMAT_SPIRV;
            info.code = data->spirv_shader_source;
            info.code_size = data->spirv_shader_source_len;
        } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
            info.format = SDL_GPU_SHADERFORMAT_DXIL;
            info.code = data->dxil_shader_source;
            info.code_size = data->dxil_shader_source_len;
        } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
            info.format = SDL_GPU_SHADERFORMAT_MSL;
            info.code = data->msl_shader_source;
            info.code_size = data->msl_shader_source_len;
        } else {
            SDL_Log("No supported shader format found");
            return false;
        }
        info.num_samplers = data->num_samplers;
        info.num_uniform_buffers = data->num_uniform_buffers;
        info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        data->shader = SDL_CreateGPUShader(device, &info);
        if (!data->shader) {
            SDL_Log("Couldn't create shader: %s", SDL_GetError());
            return false;
        }

        SDL_INIT_INTERFACE(&desc);
        desc.fragment_shader = data->shader;
        data->state = SDL_CreateGPURenderState(renderer, &desc);
        if (!data->state) {
            SDL_Log("Couldn't create render state: %s", SDL_GetError());
            return false;
        }

        if (i == EFFECT_CRT) {
            CRTEffectUniforms uniforms;
            SDL_zero(uniforms);
            uniforms.texture_width = (float)target->w;
            uniforms.texture_height = (float)target->h;
            if (!SDL_SetGPURenderStateFragmentUniforms(data->state, 0, &uniforms, sizeof(uniforms))) {
                SDL_Log("Couldn't set uniform data: %s", SDL_GetError());
                return false;
            }
        }
    }
    return true;
}

static void QuitGPURenderState(void)
{
    int i;

    for (i = 0; i < SDL_arraysize(effects); ++i) {
        FullscreenEffectData *data = &effects[i];

        if (i == EFFECT_NONE) {
            continue;
        }

        SDL_DestroyGPURenderState(data->state);
        SDL_ReleaseGPUShader(device, data->shader);
    }
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    const char *description = "GPU render effects example";
    int i;

    SDL_SetAppMetadata(description, "1.0", "com.example.testgpurender_effects");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window = SDL_CreateWindow(description, WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    if (!window) {
        SDL_Log("Couldn't create window: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    renderer = SDL_CreateRenderer(window, "gpu");
    if (!renderer) {
        SDL_Log("Couldn't create renderer: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }
    SDL_SetRenderVSync(renderer, 1);

    target = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!target) {
        SDL_Log("Couldn't create target texture: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    background = LoadTexture(renderer, "sample.bmp", false);
    if (!background) {
        SDL_Log("Couldn't create background: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    sprite = LoadTexture(renderer, "icon.bmp", true);
    if (!sprite) {
        SDL_Log("Couldn't create sprite: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    /* Initialize the sprite positions */
    for (i = 0; i < NUM_SPRITES; ++i) {
        positions[i].x = (float)SDL_rand(WINDOW_WIDTH - sprite->w);
        positions[i].y = (float)SDL_rand(WINDOW_HEIGHT - sprite->h);
        positions[i].w = (float)sprite->w;
        positions[i].h = (float)sprite->h;
        velocities[i].x = 0.0f;
        velocities[i].y = 0.0f;
        while (velocities[i].x == 0.f && velocities[i].y == 0.f) {
            velocities[i].x = (float)(SDL_rand(MAX_SPEED * 2 + 1) - MAX_SPEED);
            velocities[i].y = (float)(SDL_rand(MAX_SPEED * 2 + 1) - MAX_SPEED);
        }
    }

    if (!InitGPURenderState()) {
        return SDL_APP_FAILURE;
    }

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs when a new event (mouse input, keypresses, etc) occurs. */
SDL_AppResult SDL_AppEvent(void *appstate, SDL_Event *event)
{
    if (event->type == SDL_EVENT_QUIT ||
        (event->type == SDL_EVENT_KEY_DOWN && event->key.key == SDLK_ESCAPE)) {
        return SDL_APP_SUCCESS;  /* end the program, reporting success to the OS. */
    }
    if (event->type == SDL_EVENT_KEY_DOWN) {
        if (event->key.key == SDLK_SPACE) {
            current_effect = (current_effect + 1) % NUM_EFFECTS;
        }
    }
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    FullscreenEffectData *effect = &effects[current_effect];

    /* Draw the scene to the render target */
    SDL_SetRenderTarget(renderer, target);
    DrawScene();
    SDL_SetRenderTarget(renderer, NULL);

    /* Display the render target with the fullscreen effect */
    SDL_SetRenderGPUState(renderer, effect->state);
    SDL_RenderTexture(renderer, target, NULL, NULL);
    SDL_SetRenderGPUState(renderer, NULL);

    /* Draw some help text */
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderDebugTextFormat(renderer, 4.0f, WINDOW_HEIGHT - SDL_DEBUG_TEXT_FONT_CHARACTER_SIZE - 4.0f,
        "Current effect: %s, press SPACE to cycle", effect->name);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
    QuitGPURenderState();
}

