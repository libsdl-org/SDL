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

/* This font was created with:
 * ./msdf-atlas-gen.exe -font OpenSans-VariableFont_wdth,wght.ttf -chars '[32,126]' -type msdf -potr -yorigin top -imageout msdf_font.bmp -csv msdf_font.csv
 */

/* This is the distance field range in pixels used when generating the font atlas, defaults to 2 */
#define DISTANCE_FIELD_RANGE 2.0f

/* MSDF shaders */
#include "testgpurender_msdf.frag.dxil.h"
#include "testgpurender_msdf.frag.msl.h"
#include "testgpurender_msdf.frag.spv.h"

typedef struct
{
    float distance_field_range;
    float texture_width;
    float texture_height;
    float padding;
} MSDFShaderUniforms;

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *font_texture = NULL;
static SDL_GPUDevice *device = NULL;
static SDL_GPUShader *shader = NULL;
static SDL_GPURenderState *render_state = NULL;

typedef struct
{
    bool loaded;
    SDL_FRect src;
    SDL_FRect dst;
    float advance;
} GlyphInfo;

static GlyphInfo glyphs[128];

static bool LoadFontTexture(void)
{
    font_texture = LoadTexture(renderer, "msdf_font.bmp", false);
    if (!font_texture) {
        SDL_Log("Failed to create font texture: %s", SDL_GetError());
        return false;
    }
    SDL_SetTextureBlendMode(font_texture, SDL_BLENDMODE_BLEND);

    /* Set the font color, doesn't need to be done every frame */
    SDL_SetTextureColorMod(font_texture, 0, 0, 0);

    return true;
}

static bool LoadFontLayout(void)
{
    const char *file = "msdf_font.csv";
    char *path;
    int offset = 0, len, codepoint;
    float src_left, src_top, src_right, src_bottom;
    float dst_left, dst_top, dst_right, dst_bottom;
    float advance;
    char *font_layout;

    path = GetNearbyFilename(file);
    if (path) {
        font_layout = (char *)SDL_LoadFile(path, NULL);
        SDL_free(path);
    } else {
        font_layout = (char *)SDL_LoadFile(file, NULL);
    }
    if (!font_layout) {
        SDL_Log("Failed to load font layout: %s", SDL_GetError());
        return false;
    }

    while (SDL_sscanf(&font_layout[offset], "%d,%f,%f,%f,%f,%f,%f,%f,%f,%f%n",
                      &codepoint, &advance,
                      &dst_left, &dst_top, &dst_right, &dst_bottom,
                      &src_left, &src_top, &src_right, &src_bottom, &len) == 10) {
        if (codepoint >= 0 && codepoint < SDL_arraysize(glyphs)) {
            GlyphInfo *glyph = &glyphs[codepoint];
            glyph->loaded = true;
            glyph->src.x = src_left;
            glyph->src.y = src_top;
            glyph->src.w = src_right - src_left;
            glyph->src.h = src_bottom - src_top;
            glyph->dst.x = dst_left;
            glyph->dst.y = dst_top;
            glyph->dst.w = dst_right - dst_left;
            glyph->dst.h = dst_bottom - dst_top;
            glyph->advance = advance;
        }
        offset += len;
    }
    SDL_free(font_layout);
    return true;
}

static float MeasureText(const char *text, float font_size)
{
    float width = 0.0f;

    while (*text) {
        GlyphInfo *glyph;
        Uint32 codepoint = SDL_StepUTF8(&text, NULL);
        if (codepoint >= SDL_arraysize(glyphs)) {
            continue;
        }

        glyph = &glyphs[codepoint];
        if (!glyph->loaded) {
            continue;
        }
        width += (glyph->advance * font_size);
    }
    return width;
}

static void RenderText(const char *text, float font_size, float x, float y)
{
    SDL_FRect dst;

    /* The y coordinate is actually the baseline for the text */

    while (*text) {
        GlyphInfo *glyph;
        Uint32 codepoint = SDL_StepUTF8(&text, NULL);
        if (codepoint >= SDL_arraysize(glyphs)) {
            continue;
        }

        glyph = &glyphs[codepoint];
        if (!glyph->loaded) {
            continue;
        }

        dst.x = x + glyph->dst.x * font_size;
        dst.y = y + glyph->dst.y * font_size;
        dst.w = glyph->dst.w * font_size;
        dst.h = glyph->dst.h * font_size;
        SDL_RenderTexture(renderer, font_texture, &glyph->src, &dst);
        x += (glyph->advance * font_size);
    }
}

static bool InitGPURenderState(void)
{
    SDL_GPUShaderFormat formats;
    SDL_GPUShaderCreateInfo info;
    SDL_GPURenderStateDesc desc;
    MSDFShaderUniforms uniforms;

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

    SDL_zero(info);
    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        info.code = testgpurender_msdf_frag_spv;
        info.code_size = testgpurender_msdf_frag_spv_len;
    } else if (formats & SDL_GPU_SHADERFORMAT_DXIL) {
        info.format = SDL_GPU_SHADERFORMAT_DXIL;
        info.code = testgpurender_msdf_frag_dxil;
        info.code_size = testgpurender_msdf_frag_dxil_len;
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        info.format = SDL_GPU_SHADERFORMAT_MSL;
        info.code = testgpurender_msdf_frag_msl;
        info.code_size = testgpurender_msdf_frag_msl_len;
    } else {
        SDL_Log("No supported shader format found");
        return false;
    }
    info.num_samplers = 1;
    info.num_uniform_buffers = 1;
    info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shader = SDL_CreateGPUShader(device, &info);
    if (!shader) {
        SDL_Log("Couldn't create shader: %s", SDL_GetError());
        return false;
    }

    SDL_INIT_INTERFACE(&desc);
    desc.fragment_shader = shader;
    render_state = SDL_CreateGPURenderState(renderer, &desc);
    if (!render_state) {
        SDL_Log("Couldn't create render state: %s", SDL_GetError());
        return false;
    }

    SDL_zero(uniforms);
    uniforms.distance_field_range = DISTANCE_FIELD_RANGE;
    uniforms.texture_width = (float)font_texture->w;
    uniforms.texture_height = (float)font_texture->h;
    if (!SDL_SetGPURenderStateFragmentUniforms(render_state, 0, &uniforms, sizeof(uniforms))) {
        SDL_Log("Couldn't set uniform data: %s", SDL_GetError());
        return false;
    }

    return true;
}

static void QuitGPURenderState(void)
{
    if (render_state) {
        SDL_DestroyGPURenderState(render_state);
        render_state = NULL;
    }
    if (shader) {
        SDL_ReleaseGPUShader(device, shader);
        shader = NULL;
    }
}

/* This function runs once at startup. */
SDL_AppResult SDL_AppInit(void **appstate, int argc, char *argv[])
{
    const char *description = "GPU render MSDF example";

    SDL_SetAppMetadata(description, "1.0", "com.example.testgpurender_msdf");

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("Couldn't initialize SDL: %s", SDL_GetError());
        return SDL_APP_FAILURE;
    }

    window = SDL_CreateWindow(description, 640, 480, 0);
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

    if (!LoadFontTexture() || !LoadFontLayout()) {
        return SDL_APP_FAILURE;
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
    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once per frame, and is the heart of the program. */
SDL_AppResult SDL_AppIterate(void *appstate)
{
    const char *text = "Hello world!";
    float text_width;
    float text_height;
    float x, y;
    int output_width, output_height;

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    text_height = 72.0f;
    text_width = MeasureText(text, text_height);
    SDL_GetCurrentRenderOutputSize(renderer, &output_width, &output_height);
    x = (output_width - text_width) / 2;
    y = (output_height - text_height) / 2;
    SDL_SetRenderGPUState(renderer, render_state);
    RenderText("Hello World!", text_height, x, y);
    SDL_SetRenderGPUState(renderer, NULL);

    SDL_RenderPresent(renderer);

    return SDL_APP_CONTINUE;  /* carry on with the program! */
}

/* This function runs once at shutdown. */
void SDL_AppQuit(void *appstate, SDL_AppResult result)
{
    /* SDL will clean up the window/renderer for us. */
    QuitGPURenderState();
}

