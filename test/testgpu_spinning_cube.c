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

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

#include <SDL3/SDL_test_common.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3/SDL_main.h>

/* Regenerate the shaders with testgpu/build-shaders.sh */
#include "testgpu/testgpu_spirv.h"
#include "testgpu/testgpu_dxbc.h"
#include "testgpu/testgpu_dxil.h"
#include "testgpu/testgpu_metallib.h"

#define TESTGPU_SUPPORTED_FORMATS (SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXBC | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB)

#define CHECK_CREATE(var, thing) { if (!(var)) { SDL_Log("Failed to create %s: %s\n", thing, SDL_GetError()); quit(2); } }

static Uint32 frames = 0;

typedef struct RenderState
{
    SDL_GpuBuffer *buf_vertex;
    SDL_GpuGraphicsPipeline *pipeline;
    SDL_GpuSampleCount sample_count;
} RenderState;

typedef struct WindowState
{
    int angle_x, angle_y, angle_z;
    SDL_GpuTexture *tex_depth, *tex_msaa;
    Uint32 prev_drawablew, prev_drawableh;
} WindowState;

static SDL_GpuDevice *gpu_device = NULL;
static RenderState render_state;
static SDLTest_CommonState *state = NULL;
static WindowState *window_states = NULL;

static void shutdownGpu(void)
{
    if (window_states) {
        int i;
        for (i = 0; i < state->num_windows; i++) {
            WindowState *winstate = &window_states[i];
            SDL_ReleaseGpuTexture(gpu_device, winstate->tex_depth);
            SDL_ReleaseGpuTexture(gpu_device, winstate->tex_msaa);
            SDL_UnclaimGpuWindow(gpu_device, state->windows[i]);
        }
        SDL_free(window_states);
        window_states = NULL;
    }

    SDL_ReleaseGpuBuffer(gpu_device, render_state.buf_vertex);
    SDL_ReleaseGpuGraphicsPipeline(gpu_device, render_state.pipeline);
    SDL_DestroyGpuDevice(gpu_device);

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

    radians = angle * SDL_PI_F / 180.0f;

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

static SDL_GpuTexture*
CreateDepthTexture(Uint32 drawablew, Uint32 drawableh)
{
    SDL_GpuTextureCreateInfo depthtex_createinfo;
    SDL_GpuTexture *result;

    depthtex_createinfo.type = SDL_GPU_TEXTURETYPE_2D;
    depthtex_createinfo.format = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    depthtex_createinfo.width = drawablew;
    depthtex_createinfo.height = drawableh;
    depthtex_createinfo.layerCountOrDepth = 1;
    depthtex_createinfo.levelCount = 1;
    depthtex_createinfo.sampleCount = render_state.sample_count;
    depthtex_createinfo.usageFlags = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT;
    depthtex_createinfo.props = 0;

    result = SDL_CreateGpuTexture(gpu_device, &depthtex_createinfo);
    CHECK_CREATE(result, "Depth Texture")

    return result;
}

static SDL_GpuTexture*
CreateMSAATexture(Uint32 drawablew, Uint32 drawableh)
{
    SDL_GpuTextureCreateInfo msaatex_createinfo;
    SDL_GpuTexture *result;

    if (render_state.sample_count == SDL_GPU_SAMPLECOUNT_1) {
        return NULL;
    }

    msaatex_createinfo.type = SDL_GPU_TEXTURETYPE_2D;
    msaatex_createinfo.format = SDL_GetGpuSwapchainTextureFormat(gpu_device, state->windows[0]);
    msaatex_createinfo.width = drawablew;
    msaatex_createinfo.height = drawableh;
    msaatex_createinfo.layerCountOrDepth = 1;
    msaatex_createinfo.levelCount = 1;
    msaatex_createinfo.sampleCount = render_state.sample_count;
    msaatex_createinfo.usageFlags = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT;
    msaatex_createinfo.props = 0;

    result = SDL_CreateGpuTexture(gpu_device, &msaatex_createinfo);
    CHECK_CREATE(result, "MSAA Texture")

    return result;
}

static void
Render(SDL_Window *window, const int windownum)
{
    WindowState *winstate = &window_states[windownum];
    SDL_GpuTexture *swapchain;
    SDL_GpuColorAttachmentInfo color_attachment;
    SDL_GpuDepthStencilAttachmentInfo depth_attachment;
    float matrix_rotate[16], matrix_modelview[16], matrix_perspective[16], matrix_final[16];
    Uint32 drawablew, drawableh;
    SDL_GpuCommandBuffer *cmd;
    SDL_GpuRenderPass *pass;
    SDL_GpuBufferBinding vertex_binding;
    SDL_GpuBlitRegion src_region;
    SDL_GpuBlitRegion dst_region;

    /* Acquire the swapchain texture */

    cmd = SDL_AcquireGpuCommandBuffer(gpu_device);
    swapchain = SDL_AcquireGpuSwapchainTexture(cmd, state->windows[windownum], &drawablew, &drawableh);

    if (!swapchain) {
        /* No swapchain was acquired, probably too many frames in flight */
        SDL_SubmitGpu(cmd);
        return;
    }

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
    matrix_modelview[14] -= 2.5f;

    perspective_matrix(45.0f, (float)drawablew/drawableh, 0.01f, 100.0f, matrix_perspective);
    multiply_matrix(matrix_perspective, matrix_modelview, (float*) &matrix_final);

    winstate->angle_x += 3;
    winstate->angle_y += 2;
    winstate->angle_z += 1;

    if(winstate->angle_x >= 360) winstate->angle_x -= 360;
    if(winstate->angle_x < 0) winstate->angle_x += 360;
    if(winstate->angle_y >= 360) winstate->angle_y -= 360;
    if(winstate->angle_y < 0) winstate->angle_y += 360;
    if(winstate->angle_z >= 360) winstate->angle_z -= 360;
    if(winstate->angle_z < 0) winstate->angle_z += 360;

    /* Resize the depth buffer if the window size changed */

    if (winstate->prev_drawablew != drawablew || winstate->prev_drawableh != drawableh) {
        SDL_ReleaseGpuTexture(gpu_device, winstate->tex_depth);
        SDL_ReleaseGpuTexture(gpu_device, winstate->tex_msaa);
        winstate->tex_depth = CreateDepthTexture(drawablew, drawableh);
        winstate->tex_msaa = CreateMSAATexture(drawablew, drawableh);
    }
    winstate->prev_drawablew = drawablew;
    winstate->prev_drawableh = drawableh;

    /* Set up the pass */

    SDL_zero(color_attachment);
    color_attachment.clearColor.a = 1.0f;
    color_attachment.loadOp = SDL_GPU_LOADOP_CLEAR;
    color_attachment.storeOp = SDL_GPU_STOREOP_STORE;
    color_attachment.texture = winstate->tex_msaa ? winstate->tex_msaa : swapchain;

    SDL_zero(depth_attachment);
    depth_attachment.depthStencilClearValue.depth = 1.0f;
    depth_attachment.loadOp = SDL_GPU_LOADOP_CLEAR;
    depth_attachment.storeOp = SDL_GPU_STOREOP_DONT_CARE;
    depth_attachment.texture = winstate->tex_depth;
    depth_attachment.cycle = SDL_TRUE;

    /* Set up the bindings */

    vertex_binding.buffer = render_state.buf_vertex;
    vertex_binding.offset = 0;

    /* Draw the cube! */

    SDL_PushGpuVertexUniformData(cmd, 0, matrix_final, sizeof(matrix_final));

    pass = SDL_BeginGpuRenderPass(cmd, &color_attachment, 1, &depth_attachment);
    SDL_BindGpuGraphicsPipeline(pass, render_state.pipeline);
    SDL_BindGpuVertexBuffers(pass, 0, &vertex_binding, 1);
    SDL_DrawGpuPrimitives(pass, 36, 1, 0, 0);
    SDL_EndGpuRenderPass(pass);

    /* Blit MSAA to swapchain, if needed */
    if (render_state.sample_count > SDL_GPU_SAMPLECOUNT_1) {
        SDL_zero(src_region);
        src_region.texture = winstate->tex_msaa;
        src_region.w = drawablew;
        src_region.h = drawableh;

        dst_region = src_region;
        dst_region.texture = swapchain;

        SDL_BlitGpu(cmd, &src_region, &dst_region, SDL_FLIP_NONE, SDL_GPU_FILTER_LINEAR, SDL_FALSE);
    }

    /* Submit the command buffer! */
    SDL_SubmitGpu(cmd);

    ++frames;
}

static SDL_GpuShader*
load_shader(SDL_bool is_vertex)
{
    SDL_GpuShaderCreateInfo createinfo;
    createinfo.samplerCount = 0;
    createinfo.storageBufferCount = 0;
    createinfo.storageTextureCount = 0;
    createinfo.uniformBufferCount = is_vertex ? 1 : 0;
    createinfo.props = 0;

    SDL_GpuDriver backend = SDL_GetGpuDriver(gpu_device);
    if (backend == SDL_GPU_DRIVER_D3D11) {
        createinfo.format = SDL_GPU_SHADERFORMAT_DXBC;
        createinfo.code = is_vertex ? D3D11_CubeVert : D3D11_CubeFrag;
        createinfo.codeSize = is_vertex ? SDL_arraysize(D3D11_CubeVert) : SDL_arraysize(D3D11_CubeFrag);
        createinfo.entryPointName = is_vertex ? "VSMain" : "PSMain";
    } else if (backend == SDL_GPU_DRIVER_D3D12) {
        createinfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        createinfo.code = is_vertex ? D3D12_CubeVert : D3D12_CubeFrag;
        createinfo.codeSize = is_vertex ? SDL_arraysize(D3D12_CubeVert) : SDL_arraysize(D3D12_CubeFrag);
        createinfo.entryPointName = is_vertex ? "VSMain" : "PSMain";
    } else if (backend == SDL_GPU_DRIVER_METAL) {
        createinfo.format = SDL_GPU_SHADERFORMAT_METALLIB;
        createinfo.code = is_vertex ? cube_vert_metallib : cube_frag_metallib;
        createinfo.codeSize = is_vertex ? cube_vert_metallib_len : cube_frag_metallib_len;
        createinfo.entryPointName = is_vertex ? "vs_main" : "fs_main";
    } else {
        createinfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
        createinfo.code = is_vertex ? cube_vert_spv : cube_frag_spv;
        createinfo.codeSize = is_vertex ? cube_vert_spv_len : cube_frag_spv_len;
        createinfo.entryPointName = "main";
    }

    createinfo.stage = is_vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    return SDL_CreateGpuShader(gpu_device, &createinfo);
}

static void
init_render_state(int msaa)
{
    SDL_GpuCommandBuffer *cmd;
    SDL_GpuTransferBuffer *buf_transfer;
    void *map;
    SDL_GpuTransferBufferLocation buf_location;
    SDL_GpuBufferRegion dst_region;
    SDL_GpuCopyPass *copy_pass;
    SDL_GpuBufferCreateInfo buffer_desc;
    SDL_GpuTransferBufferCreateInfo transfer_buffer_desc;
    SDL_GpuGraphicsPipelineCreateInfo pipelinedesc;
    SDL_GpuColorAttachmentDescription color_attachment_desc;
    Uint32 drawablew, drawableh;
    SDL_GpuVertexAttribute vertex_attributes[2];
    SDL_GpuVertexBinding vertex_binding;
    SDL_GpuShader *vertex_shader;
    SDL_GpuShader *fragment_shader;
    int i;

    gpu_device = SDL_CreateGpuDevice(
        TESTGPU_SUPPORTED_FORMATS,
        1,
        0,
        NULL
    );
    CHECK_CREATE(gpu_device, "GPU device");

    /* Claim the windows */

    for (i = 0; i < state->num_windows; i++) {
        SDL_ClaimGpuWindow(
            gpu_device,
            state->windows[i]
        );
    }

    /* Create shaders */

    vertex_shader = load_shader(SDL_TRUE);
    CHECK_CREATE(vertex_shader, "Vertex Shader")
    fragment_shader = load_shader(SDL_FALSE);
    CHECK_CREATE(fragment_shader, "Fragment Shader")

    /* Create buffers */

    buffer_desc.usageFlags = SDL_GPU_BUFFERUSAGE_VERTEX_BIT;
    buffer_desc.sizeInBytes = sizeof(vertex_data);
    buffer_desc.props = 0;
    render_state.buf_vertex = SDL_CreateGpuBuffer(
        gpu_device,
        &buffer_desc
    );
    CHECK_CREATE(render_state.buf_vertex, "Static vertex buffer")

    SDL_SetGpuBufferName(gpu_device, render_state.buf_vertex, "космонавт");

    transfer_buffer_desc.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_buffer_desc.sizeInBytes = sizeof(vertex_data);
    transfer_buffer_desc.props = 0;
    buf_transfer = SDL_CreateGpuTransferBuffer(
        gpu_device,
        &transfer_buffer_desc
    );
    CHECK_CREATE(buf_transfer, "Vertex transfer buffer")

    /* We just need to upload the static data once. */
    map = SDL_MapGpuTransferBuffer(gpu_device, buf_transfer, SDL_FALSE);
    SDL_memcpy(map, vertex_data, sizeof(vertex_data));
    SDL_UnmapGpuTransferBuffer(gpu_device, buf_transfer);

    cmd = SDL_AcquireGpuCommandBuffer(gpu_device);
    copy_pass = SDL_BeginGpuCopyPass(cmd);
    buf_location.transferBuffer = buf_transfer;
    buf_location.offset = 0;
    dst_region.buffer = render_state.buf_vertex;
    dst_region.offset = 0;
    dst_region.size = sizeof(vertex_data);
    SDL_UploadToGpuBuffer(copy_pass, &buf_location, &dst_region, SDL_FALSE);
    SDL_EndGpuCopyPass(copy_pass);
    SDL_SubmitGpu(cmd);

    SDL_ReleaseGpuTransferBuffer(gpu_device, buf_transfer);

    /* Determine which sample count to use */
    render_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    if (msaa && SDL_SupportsGpuSampleCount(
        gpu_device,
        SDL_GetGpuSwapchainTextureFormat(gpu_device, state->windows[0]),
        SDL_GPU_SAMPLECOUNT_4)) {
        render_state.sample_count = SDL_GPU_SAMPLECOUNT_4;
    }

    /* Set up the graphics pipeline */

    SDL_zero(pipelinedesc);

    color_attachment_desc.format = SDL_GetGpuSwapchainTextureFormat(gpu_device, state->windows[0]);

    color_attachment_desc.blendState.blendEnable = 0;
    color_attachment_desc.blendState.alphaBlendOp = SDL_GPU_BLENDOP_ADD;
    color_attachment_desc.blendState.colorBlendOp = SDL_GPU_BLENDOP_ADD;
    color_attachment_desc.blendState.colorWriteMask = 0xF;
    color_attachment_desc.blendState.srcAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ONE;
    color_attachment_desc.blendState.dstAlphaBlendFactor = SDL_GPU_BLENDFACTOR_ZERO;
    color_attachment_desc.blendState.srcColorBlendFactor = SDL_GPU_BLENDFACTOR_ONE;
    color_attachment_desc.blendState.dstColorBlendFactor = SDL_GPU_BLENDFACTOR_ZERO;

    pipelinedesc.attachmentInfo.colorAttachmentCount = 1;
    pipelinedesc.attachmentInfo.colorAttachmentDescriptions = &color_attachment_desc;
    pipelinedesc.attachmentInfo.depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM;
    pipelinedesc.attachmentInfo.hasDepthStencilAttachment = SDL_TRUE;

    pipelinedesc.depthStencilState.depthTestEnable = 1;
    pipelinedesc.depthStencilState.depthWriteEnable = 1;
    pipelinedesc.depthStencilState.compareOp = SDL_GPU_COMPAREOP_LESS_OR_EQUAL;

    pipelinedesc.multisampleState.sampleCount = render_state.sample_count;
    pipelinedesc.multisampleState.sampleMask = 0xF;

    pipelinedesc.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    pipelinedesc.vertexShader = vertex_shader;
    pipelinedesc.fragmentShader = fragment_shader;

    vertex_binding.binding = 0;
    vertex_binding.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vertex_binding.instanceStepRate = 0;
    vertex_binding.stride = sizeof(VertexData);

    vertex_attributes[0].binding = 0;
    vertex_attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[0].location = 0;
    vertex_attributes[0].offset = 0;

    vertex_attributes[1].binding = 0;
    vertex_attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    vertex_attributes[1].location = 1;
    vertex_attributes[1].offset = sizeof(float) * 3;

    pipelinedesc.vertexInputState.vertexBindingCount = 1;
    pipelinedesc.vertexInputState.vertexBindings = &vertex_binding;
    pipelinedesc.vertexInputState.vertexAttributeCount = 2;
    pipelinedesc.vertexInputState.vertexAttributes = (SDL_GpuVertexAttribute*) &vertex_attributes;

    pipelinedesc.props = 0;

    render_state.pipeline = SDL_CreateGpuGraphicsPipeline(gpu_device, &pipelinedesc);
    CHECK_CREATE(render_state.pipeline, "Render Pipeline")

    /* These are reference-counted; once the pipeline is created, you don't need to keep these. */
    SDL_ReleaseGpuShader(gpu_device, vertex_shader);
    SDL_ReleaseGpuShader(gpu_device, fragment_shader);

    /* Set up per-window state */

    window_states = (WindowState *) SDL_calloc(state->num_windows, sizeof (WindowState));
    if (!window_states) {
        SDL_Log("Out of memory!\n");
        quit(2);
    }

    for (i = 0; i < state->num_windows; i++) {
        WindowState *winstate = &window_states[i];

        /* create a depth texture for the window */
        SDL_GetWindowSizeInPixels(state->windows[i], (int*) &drawablew, (int*) &drawableh);
        winstate->tex_depth = CreateDepthTexture(drawablew, drawableh);
        winstate->tex_msaa = CreateMSAATexture(drawablew, drawableh);

        /* make each window different */
        winstate->angle_x = (i * 10) % 360;
        winstate->angle_y = (i * 20) % 360;
        winstate->angle_z = (i * 30) % 360;
    }
}

static int done = 0;

void loop()
{
    SDL_Event event;
    int i;

    /* Check for events */
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
    int msaa;
    int i;
    const SDL_DisplayMode *mode;
    Uint64 then, now;

    /* Initialize params */
    msaa = 0;

    /* Initialize test framework */
    state = SDLTest_CommonCreateState(argv, SDL_INIT_VIDEO);
    if (!state) {
        return 1;
    }
    for (i = 1; i < argc;) {
        int consumed;

        consumed = SDLTest_CommonArg(state, i);
        if (consumed == 0) {
            if (SDL_strcasecmp(argv[i], "--msaa") == 0) {
                ++msaa;
                consumed = 1;
            } else {
                consumed = -1;
            }
        }
        if (consumed < 0) {
            static const char *options[] = { "[--msaa]", NULL };
            SDLTest_CommonLogUsage(state, argv[0], options);
            quit(1);
        }
        i += consumed;
    }

    state->skip_renderer = 1;
    state->window_flags |= SDL_WINDOW_RESIZABLE;

    if (!SDLTest_CommonInit(state)) {
        quit(2);
        return 0;
    }

    mode = SDL_GetCurrentDisplayMode(SDL_GetDisplayForWindow(state->windows[0]));
    SDL_Log("Screen bpp: %d\n", SDL_BITSPERPIXEL(mode->format));

    init_render_state(msaa);

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
#if !defined(__ANDROID__)
    quit(0);
#endif
    return 0;
}

/* vi: set ts=4 sw=4 expandtab: */
