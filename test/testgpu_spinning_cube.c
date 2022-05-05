/*
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include "SDL_test_common.h"
#include "SDL_gpu.h"
#include "SDL_gpu_compiler.h"

typedef struct RenderState
{
    SDL_GpuBuffer *gpubuf_static;
    SDL_GpuPipeline *pipeline;
} RenderState;

typedef struct WindowState
{
    int angle_x, angle_y, angle_z;
    SDL_GpuTextureCycle *texcycle_depth;
    SDL_GpuCpuBufferCycle *cpubufcycle_uniforms;
    SDL_GpuBufferCycle *gpubufcycle_uniforms;
} WindowState;

static SDL_GpuDevice *gpu_device = NULL;
static RenderState render_state;
static SDLTest_CommonState *state = NULL;
static WindowState *window_states = NULL;

static void shutdownGpu(void)
{
    /* !!! FIXME: We need a WaitIdle API */
    if (window_states) {
        int i;
        for (i = 0; i < state->num_windows; i++) {
            WindowState *winstate = &window_states[i];
            SDL_GpuDestroyTextureCycle(winstate->texcycle_depth);
            SDL_GpuDestroyCpuBufferCycle(winstate->cpubufcycle_uniforms);
            SDL_GpuDestroyBufferCycle(winstate->gpubufcycle_uniforms);
        }
        SDL_free(window_states);
        window_states = NULL;
    }

    SDL_GpuDestroyBuffer(render_state.gpubuf_static);
    SDL_GpuDestroyPipeline(render_state.pipeline);
    SDL_GpuDestroyDevice(gpu_device);

    SDL_zero(render_state);
    gpu_device = NULL;
}


/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void
quit(int rc)
{
    shutdownGpu();
    SDLTest_CommonQuit(state);
    exit(rc);
}

/* 
 * Simulates desktop's glRotatef. The matrix is returned in column-major 
 * order. 
 */
static void
rotate_matrix(float angle, float x, float y, float z, float *r)
{
    float radians, c, s, c1, u[3], length;
    int i, j;

    radians = (float)(angle * M_PI) / 180.0f;

    c = SDL_cosf(radians);
    s = SDL_sinf(radians);

    c1 = 1.0f - SDL_cosf(radians);

    length = (float)SDL_sqrt(x * x + y * y + z * z);

    u[0] = x / length;
    u[1] = y / length;
    u[2] = z / length;

    for (i = 0; i < 16; i++) {
        r[i] = 0.0;
    }

    r[15] = 1.0;

    for (i = 0; i < 3; i++) {
        r[i * 4 + (i + 1) % 3] = u[(i + 2) % 3] * s;
        r[i * 4 + (i + 2) % 3] = -u[(i + 1) % 3] * s;
    }

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) {
            r[i * 4 + j] += c1 * u[i] * u[j] + (i == j ? c : 0.0f);
        }
    }
}

/* 
 * Simulates gluPerspectiveMatrix 
 */
static void 
perspective_matrix(float fovy, float aspect, float znear, float zfar, float *r)
{
    int i;
    float f;

    f = 1.0f/SDL_tanf(fovy * 0.5f);

    for (i = 0; i < 16; i++) {
        r[i] = 0.0;
    }

    r[0] = f / aspect;
    r[5] = f;
    r[10] = (znear + zfar) / (znear - zfar);
    r[11] = -1.0f;
    r[14] = (2.0f * znear * zfar) / (znear - zfar);
    r[15] = 0.0f;
}

/* 
 * Multiplies lhs by rhs and writes out to r. All matrices are 4x4 and column
 * major. In-place multiplication is supported.
 */
static void
multiply_matrix(float *lhs, float *rhs, float *r)
{
    int i, j, k;
    float tmp[16];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 4; j++) {
            tmp[j * 4 + i] = 0.0;

            for (k = 0; k < 4; k++) {
                tmp[j * 4 + i] += lhs[k * 4 + i] * rhs[j * 4 + k];
            }
        }
    }

    for (i = 0; i < 16; i++) {
        r[i] = tmp[i];
    }
}

typedef struct VertexData
{
    float x, y, z; /* 3D data. Vertex range -0.5..0.5 in all axes. Z -0.5 is near, 0.5 is far. */
    float red, green, blue;  /* intensity 0 to 1 (alpha is always 1). */
} VertexData;

static const VertexData vertex_data[] = {
    /* Front face. */
    /* Bottom left */
    { -0.5,  0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
    {  0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */

    /* Top right */
    { -0.5, 0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { 0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */

    /* Left face */
    /* Bottom left */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

    /* Top right */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    { -0.5,  0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */

    /* Top face */
    /* Bottom left */
    { -0.5, 0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    {  0.5, 0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { -0.5, 0.5, -0.5, 1.0, 0.0, 0.0 }, /* red */

    /* Top right */
    { -0.5, 0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    {  0.5, 0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
    {  0.5, 0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */

    /* Right face */
    /* Bottom left */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { 0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */
    { 0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */

    /* Top right */
    { 0.5,  0.5, -0.5, 1.0, 1.0, 0.0 }, /* yellow */
    { 0.5,  0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
    { 0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */

    /* Back face */
    /* Bottom left */
    {  0.5,  0.5, 0.5, 0.0, 0.0, 0.0 }, /* black */
    { -0.5, -0.5, 0.5, 0.0, 1.0, 1.0 }, /* cyan */
    {  0.5, -0.5, 0.5, 1.0, 0.0, 1.0 }, /* magenta */

    /* Top right */
    {  0.5,  0.5,  0.5, 0.0, 0.0, 0.0 }, /* black */
    { -0.5,  0.5,  0.5, 1.0, 1.0, 1.0 }, /* white */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

    /* Bottom face */
    /* Bottom left */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
    {  0.5, -0.5,  0.5, 1.0, 0.0, 1.0 }, /* magenta */
    { -0.5, -0.5,  0.5, 0.0, 1.0, 1.0 }, /* cyan */

    /* Top right */
    { -0.5, -0.5, -0.5, 0.0, 1.0, 0.0 }, /* green */
    {  0.5, -0.5, -0.5, 0.0, 0.0, 1.0 }, /* blue */
    {  0.5, -0.5,  0.5, 1.0, 0.0, 1.0 } /* magenta */
};

/* !!! FIXME: these shaders need to change. This is just the GLES2 shaders right now. */
static const char* shader_vert_src =
" attribute vec4 av4position; "
" attribute vec3 av3color; "
" uniform mat4 mvp; "
" varying vec3 vv3color; "
" void main() { "
"    vv3color = av3color; "
"    gl_Position = mvp * av4position; "
" } ";

static const char* shader_frag_src =
" precision lowp float; "
" varying vec3 vv3color; "
" void main() { "
"    gl_FragColor = vec4(vv3color, 1.0); "
" } ";

static void
Render(SDL_Window *window, const int windownum)
{
    WindowState *winstate = &window_states[windownum];
    SDL_GpuTexture *backbuffer = SDL_GpuGetBackbuffer(gpu_device, window);
    const SDL_GpuPresentType presenttype = (state->render_flags & SDL_RENDERER_PRESENTVSYNC) ? SDL_GPUPRESENT_VSYNC : SDL_GPUPRESENT_IMMEDIATE;
    SDL_GpuColorAttachmentDescription color_attachment;
    SDL_GpuDepthAttachmentDescription depth_attachment;
    SDL_GpuTexture **depth_texture_ptr;
    SDL_GpuCpuBuffer *cpubuf_uniforms = SDL_GpuNextCpuBufferCycle(winstate->cpubufcycle_uniforms);
    SDL_GpuBuffer *gpubuf_uniforms = SDL_GpuNextBufferCycle(winstate->gpubufcycle_uniforms);
    SDL_GpuTextureDescription texdesc;
    float matrix_rotate[16], matrix_modelview[16], matrix_perspective[16];
    Uint32 drawablew, drawableh;
    SDL_GpuCommandBuffer *cmd;
    SDL_GpuRenderPass *render;
    SDL_GpuBlitPass *blit;
    char label[64];

    if (!backbuffer) {
        SDL_Log("Uhoh, no backbuffer for window #%d!\n", windownum);
        return;
    }

    SDL_GpuGetTextureDescription(backbuffer, &texdesc);
    drawablew = texdesc.width;
    drawableh = texdesc.height;

    /* 
    * Do some rotation with Euler angles. It is not a fixed axis as
    * quaterions would be, but the effect is cool. 
    */
    rotate_matrix((float)winstate->angle_x, 1.0f, 0.0f, 0.0f, matrix_modelview);
    rotate_matrix((float)winstate->angle_y, 0.0f, 1.0f, 0.0f, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    rotate_matrix((float)winstate->angle_z, 0.0f, 1.0f, 0.0f, matrix_rotate);

    multiply_matrix(matrix_rotate, matrix_modelview, matrix_modelview);

    /* Pull the camera back from the cube */
    matrix_modelview[14] -= 2.5;

    perspective_matrix(45.0f, (float)drawablew/drawableh, 0.01f, 100.0f, matrix_perspective);

    multiply_matrix(matrix_perspective, matrix_modelview, (float *) SDL_GpuLockCpuBuffer(cpubuf_uniforms, NULL));
    SDL_GpuUnlockCpuBuffer(cpubuf_uniforms);

    winstate->angle_x += 3;
    winstate->angle_y += 2;
    winstate->angle_z += 1;

    if(winstate->angle_x >= 360) winstate->angle_x -= 360;
    if(winstate->angle_x < 0) winstate->angle_x += 360;
    if(winstate->angle_y >= 360) winstate->angle_y -= 360;
    if(winstate->angle_y < 0) winstate->angle_y += 360;
    if(winstate->angle_z >= 360) winstate->angle_z -= 360;
    if(winstate->angle_z < 0) winstate->angle_z += 360;

    /* Copy the new uniform data to the GPU */
    cmd = SDL_GpuCreateCommandBuffer("Render new frame", gpu_device);
    if (!cmd) {
        SDL_Log("Failed to create command buffer: %s\n", SDL_GetError());
        quit(2);
    }

    blit = SDL_GpuStartBlitPass("Copy mvp matrix to GPU pass", cmd);
    if (!blit) {
        SDL_Log("Failed to create blit pass: %s\n", SDL_GetError());
        quit(2);
    }

    SDL_GpuCopyBufferCpuToGpu(blit, cpubuf_uniforms, 0, gpubuf_uniforms, 0, sizeof (float) * 16);
    SDL_GpuEndBlitPass(blit);

    SDL_zero(color_attachment);
    color_attachment.texture = backbuffer;
    color_attachment.color_init = SDL_GPUPASSINIT_CLEAR;
    color_attachment.clear_alpha = 1.0f;

    /* resize the depth texture if the window size changed */
    SDL_snprintf(label, sizeof (label), "Depth buffer for window #%d", windownum);
    depth_texture_ptr = SDL_GpuNextTexturePtrCycle(winstate->texcycle_depth);
    if (SDL_GpuMatchingDepthTexture(label, gpu_device, color_attachment.texture, depth_texture_ptr) == NULL) {
        SDL_Log("Failed to prepare depth buffer for window #%d: %s\n", windownum, SDL_GetError());
        quit(2);
    }

    SDL_zero(depth_attachment);
    depth_attachment.texture = *depth_texture_ptr;
    depth_attachment.depth_init = SDL_GPUPASSINIT_CLEAR;
    depth_attachment.clear_depth = 0.0;


    /* Draw the cube! */

    /* !!! FIXME: does viewport/scissor default to the texture size? Because that would be nice. */
    render = SDL_GpuStartRenderPass("Spinning cube render pass", cmd, 1, &color_attachment, &depth_attachment, NULL);
    SDL_GpuSetRenderPassPipeline(render, render_state.pipeline);
    SDL_GpuSetRenderPassViewport(render, 0.0, 0.0, (double) drawablew, (double) drawableh, 0.0, 1.0);  /* !!! FIXME near and far are wrong */
    SDL_GpuSetRenderPassScissor(render, 0.0, 0.0, (double) drawablew, (double) drawableh);
    SDL_GpuSetRenderPassVertexBuffer(render, render_state.gpubuf_static, 0, 0);
    SDL_GpuSetRenderPassVertexBuffer(render, gpubuf_uniforms, 0, 1);
    SDL_GpuDraw(render, 0, SDL_arraysize(vertex_data));
    SDL_GpuEndRenderPass(render);

    SDL_GpuSubmitCommandBuffers(gpu_device, &cmd, 1, presenttype, NULL);  /* push work to the GPU and tell it to present to the window when done. */
}

static SDL_GpuShader *load_shader(const char *label, const char *src, const char *type)
{
    SDL_GpuShader *retval = NULL;
    Uint8 *bytecode = NULL;
    Uint32 bytecodelen = 0;
    if (SDL_GpuCompileShader(src, -1, type, "main", &bytecode, &bytecodelen) == -1) {
        SDL_Log("Failed to compile %s shader: %s", type, SDL_GetError());
        quit(2);
    }
    retval = SDL_GpuCreateShader(label, gpu_device, bytecode, bytecodelen);
    if (!retval) {
        SDL_Log("Failed to load %s shader bytecode: %s", type, SDL_GetError());
        quit(2);
    }

    SDL_free(bytecode);

    return retval;
}

static void
init_render_state(void)
{
    SDL_GpuCommandBuffer *cmd;
    SDL_GpuPipelineDescription pipelinedesc;
    SDL_GpuTextureDescription texdesc;
    SDL_GpuShader *vertex_shader;
    SDL_GpuShader *fragment_shader;
    void *ptr;
    int i;

    #define CHECK_CREATE(var, thing) { if (!(var)) { SDL_Log("Failed to create %s: %s\n", thing, SDL_GetError()); quit(2); } }

    gpu_device = SDL_GpuCreateDevice("The GPU device", NULL);
    CHECK_CREATE(gpu_device, "GPU device");

    vertex_shader = load_shader("Spinning cube vertex shader", shader_vert_src, "vertex");
    fragment_shader = load_shader("Spinning cube fragment shader", shader_frag_src, "fragment");

    /* We just need to upload the static data once. */
    render_state.gpubuf_static = SDL_GpuCreateAndInitBuffer("Static vertex data GPU buffer", gpu_device, sizeof (vertex_data), vertex_data);
    CHECK_CREATE(render_state.gpubuf_static, "static vertex GPU buffer");

    SDL_GpuDefaultPipelineDescription(&pipelinedesc);
    pipelinedesc.label = "The spinning cube pipeline";
    pipelinedesc.primitive = SDL_GPUPRIM_TRIANGLESTRIP;
    pipelinedesc.vertex_shader = vertex_shader;
    pipelinedesc.fragment_shader = fragment_shader;
    pipelinedesc.num_vertex_attributes = 2;
    pipelinedesc.vertices[0].format = SDL_GPUVERTFMT_FLOAT3;
    pipelinedesc.vertices[1].format = SDL_GPUVERTFMT_FLOAT3;
    pipelinedesc.vertices[1].index = 1;
    pipelinedesc.num_color_attachments = 1;
    pipelinedesc.color_attachments[0].pixel_format = SDL_GPUPIXELFMT_RGBA8;
    pipelinedesc.color_attachments[0].blending_enabled = SDL_FALSE;
    pipelinedesc.depth_format = SDL_GPUPIXELFMT_Depth24_Stencil8;

    render_state.pipeline = SDL_GpuCreatePipeline(gpu_device, &pipelinedesc);
    if (!render_state.pipeline) {
        SDL_Log("Failed to create render pipeline: %s\n", SDL_GetError());
        quit(2);
    }

    /* These are reference-counted; once the pipeline is created, you don't need to keep these. */
    SDL_GpuDestroyShader(vertex_shader);
    SDL_GpuDestroyShader(fragment_shader);

    window_states = (WindowState *) SDL_calloc(state->num_windows, sizeof (WindowState));
    if (!window_states) {
        SDL_Log("Out of memory!\n");
        quit(2);
    }

    for (i = 0; i < state->num_windows; i++) {
        /* each window gets a cycle of buffers and depth textures, so we don't have to wait for them
           to finish; by the time they come around again in the cycle, they're available to use again. */
        WindowState *winstate = &window_states[i];
        char label[32];

        SDL_snprintf(label, sizeof (label), "Window #%d uniform staging buffer", i);
        winstate->cpubufcycle_uniforms = SDL_GpuCreateCpuBufferCycle(label, gpu_device, sizeof (float) * 16, NULL, 3);
        CHECK_CREATE(winstate->cpubufcycle_uniforms, label);

        SDL_snprintf(label, sizeof (label), "Window #%d uniform GPU buffer", i);
        winstate->gpubufcycle_uniforms = SDL_GpuCreateBufferCycle(label, gpu_device, sizeof (float) * 16, 3);
        CHECK_CREATE(winstate->gpubufcycle_uniforms, label);

        SDL_snprintf(label, sizeof (label), "Window #%d depth texture", i);  /* NULL texdesc, so we'll build them as we need them. */
        winstate->texcycle_depth = SDL_GpuCreateTextureCycle(label, gpu_device, NULL, 3);
        CHECK_CREATE(winstate->texcycle_depth, label);

        /* make each window different */
        winstate->angle_x = (i * 10) % 360;
        winstate->angle_y = (i * 20) % 360;
        winstate->angle_z = (i * 30) % 360;
    }
}


static int done = 0;
static Uint32 frames = 0;

void loop()
{
    SDL_Event event;
    int i;
    int status;

    /* Check for events */
    ++frames;
    while (SDL_PollEvent(&event) && !done) {
        SDLTest_CommonEvent(state, &event, &done);
    }
    if (!done) {
        for (i = 0; i < state->num_windows; ++i) {
            Render(state->windows[i], i);
        }
    }
#ifdef __EMSCRIPTEN__
    else {
        emscripten_cancel_main_loop();
    }
#endif
}

int
main(int argc, char *argv[])
{
    int fsaa;
    int value;
    int i;
    SDL_DisplayMode mode;
    Uint32 then, now;
    int status;

    /* Initialize parameters */
    fsaa = 0;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }

    state->skip_renderer = 1;

    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            if (SDL_strcasecmp(argv[i], "--fsaa") == 0) {
                ++fsaa;
                consumed = 1;
            } else {
                consumed = -1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--fsaa]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }
        i += consumed;
    }

    state->window_flags |= SDL_WINDOW_RESIZABLE;

    if (!SDLTest_CommonInit(state)) {
        quit(2);
        return 0;
    }

    SDL_GetCurrentDisplayMode(0, &mode);
    SDL_Log("Screen bpp: %d\n", SDL_BITSPERPIXEL(mode.format));

    #if 0  // !!! FIXME: report any of this through the Gpu API?
    SDL_Log("\n");
    SDL_Log("Vendor     : %s\n", ctx.glGetString(GL_VENDOR));
    SDL_Log("Renderer   : %s\n", ctx.glGetString(GL_RENDERER));
    SDL_Log("Version    : %s\n", ctx.glGetString(GL_VERSION));
    SDL_Log("Extensions : %s\n", ctx.glGetString(GL_EXTENSIONS));
    SDL_Log("\n");
    #endif

    /* !!! FIXME: use fsaa once multisample support is wired into the API */

    init_render_state();

    /* Main render loop */
    frames = 0;
    then = SDL_GetTicks();
    done = 0;

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!done) {
        loop();
    }
#endif

    /* Print out some timing information */
    now = SDL_GetTicks();
    if (now > then) {
        SDL_Log("%2.2f frames per second\n",
               ((double) frames * 1000) / (now - then));
    }
#if !defined(__ANDROID__) && !defined(__NACL__)  
    quit(0);
#endif    
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
