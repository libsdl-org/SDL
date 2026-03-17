/*
  Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely.
*/

/*
 * testgpu_spinning_cube_xr.c - SDL3 GPU API OpenXR Spinning Cubes Test
 *
 * This is an XR-enabled version of testgpu_spinning_cube that renders
 * spinning colored cubes in VR using OpenXR and SDL's GPU API.
 *
 * Rendering approach: Multi-pass stereo (one render pass per eye)
 * This is the simplest and most compatible approach, working on all
 * OpenXR-capable platforms (Desktop VR runtimes, Quest, etc.)
 *
 * For more information on stereo rendering techniques, see:
 * - Multi-pass: Traditional, 2 render passes (used here)
 * - Multiview (GL_OVR_multiview): Single pass with texture arrays
 * - Single-pass instanced: GPU instancing to select eye
 */

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

/* Include OpenXR headers BEFORE SDL_openxr.h to get full type definitions */
#ifdef HAVE_OPENXR_H
#include <openxr/openxr.h>
#else
/* SDL includes a copy for building on systems without the OpenXR SDK */
#include "../src/video/khronos/openxr/openxr.h"
#endif

#include <SDL3/SDL_openxr.h>

/* Standard library for exit() */
#include <stdlib.h>

/* Include compiled shader bytecode for all backends */
#include "testgpu/cube.frag.dxil.h"
#include "testgpu/cube.frag.spv.h"
#include "testgpu/cube.vert.dxil.h"
#include "testgpu/cube.vert.spv.h"

#define CHECK_CREATE(var, thing) { if (!(var)) { SDL_Log("Failed to create %s: %s", thing, SDL_GetError()); return false; } }
#define XR_CHECK(result, msg) do { if (XR_FAILED(result)) { SDL_Log("OpenXR Error: %s (result=%d)", msg, (int)(result)); return false; } } while(0)
#define XR_CHECK_QUIT(result, msg) do { if (XR_FAILED(result)) { SDL_Log("OpenXR Error: %s (result=%d)", msg, (int)(result)); quit(2); return; } } while(0)

/* ========================================================================
 * Math Types and Functions
 * ======================================================================== */

typedef struct { float x, y, z; } Vec3;
typedef struct { float m[16]; } Mat4;

static Mat4 Mat4_Multiply(Mat4 a, Mat4 b)
{
    Mat4 result = {{0}};
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            for (int k = 0; k < 4; k++) {
                result.m[i * 4 + j] += a.m[i * 4 + k] * b.m[k * 4 + j];
            }
        }
    }
    return result;
}

static Mat4 Mat4_Translation(float x, float y, float z)
{
    return (Mat4){{ 1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1 }};
}

static Mat4 Mat4_Scale(float s)
{
    return (Mat4){{ s,0,0,0, 0,s,0,0, 0,0,s,0, 0,0,0,1 }};
}

static Mat4 Mat4_RotationY(float rad)
{
    float c = SDL_cosf(rad), s = SDL_sinf(rad);
    return (Mat4){{ c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1 }};
}

static Mat4 Mat4_RotationX(float rad)
{
    float c = SDL_cosf(rad), s = SDL_sinf(rad);
    return (Mat4){{ 1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1 }};
}

/* Convert XrPosef to view matrix (inverted transform) */
static Mat4 Mat4_FromXrPose(XrPosef pose)
{
    float x = pose.orientation.x, y = pose.orientation.y;
    float z = pose.orientation.z, w = pose.orientation.w;

    /* Quaternion to rotation matrix columns */
    Vec3 right = { 1-2*(y*y+z*z), 2*(x*y+w*z), 2*(x*z-w*y) };
    Vec3 up = { 2*(x*y-w*z), 1-2*(x*x+z*z), 2*(y*z+w*x) };
    Vec3 fwd = { 2*(x*z+w*y), 2*(y*z-w*x), 1-2*(x*x+y*y) };
    Vec3 pos = { pose.position.x, pose.position.y, pose.position.z };

    /* Inverted transform for view matrix */
    float dr = -(right.x*pos.x + right.y*pos.y + right.z*pos.z);
    float du = -(up.x*pos.x + up.y*pos.y + up.z*pos.z);
    float df = -(fwd.x*pos.x + fwd.y*pos.y + fwd.z*pos.z);

    return (Mat4){{ right.x,up.x,fwd.x,0, right.y,up.y,fwd.y,0, right.z,up.z,fwd.z,0, dr,du,df,1 }};
}

/* Create asymmetric projection matrix from XR FOV */
static Mat4 Mat4_Projection(XrFovf fov, float nearZ, float farZ)
{
    float tL = SDL_tanf(fov.angleLeft), tR = SDL_tanf(fov.angleRight);
    float tU = SDL_tanf(fov.angleUp), tD = SDL_tanf(fov.angleDown);
    float w = tR - tL, h = tU - tD;

    return (Mat4){{
        2/w, 0, 0, 0,
        0, 2/h, 0, 0,
        (tR+tL)/w, (tU+tD)/h, -farZ/(farZ-nearZ), -1,
        0, 0, -(farZ*nearZ)/(farZ-nearZ), 0
    }};
}

/* ========================================================================
 * Vertex Data
 * ======================================================================== */

typedef struct {
    float x, y, z;
    Uint8 r, g, b, a;
} PositionColorVertex;

/* Cube vertices - 0.25m half-size, each face a different color */
static const float CUBE_HALF_SIZE = 0.25f;

/* ========================================================================
 * OpenXR Function Pointers (loaded dynamically)
 * ======================================================================== */

static PFN_xrGetInstanceProcAddr pfn_xrGetInstanceProcAddr = NULL;
static PFN_xrEnumerateViewConfigurationViews pfn_xrEnumerateViewConfigurationViews = NULL;
static PFN_xrEnumerateSwapchainImages pfn_xrEnumerateSwapchainImages = NULL;
static PFN_xrCreateReferenceSpace pfn_xrCreateReferenceSpace = NULL;
static PFN_xrDestroySpace pfn_xrDestroySpace = NULL;
static PFN_xrDestroySession pfn_xrDestroySession = NULL;
static PFN_xrDestroyInstance pfn_xrDestroyInstance = NULL;
static PFN_xrPollEvent pfn_xrPollEvent = NULL;
static PFN_xrBeginSession pfn_xrBeginSession = NULL;
static PFN_xrEndSession pfn_xrEndSession = NULL;
static PFN_xrWaitFrame pfn_xrWaitFrame = NULL;
static PFN_xrBeginFrame pfn_xrBeginFrame = NULL;
static PFN_xrEndFrame pfn_xrEndFrame = NULL;
static PFN_xrLocateViews pfn_xrLocateViews = NULL;
static PFN_xrAcquireSwapchainImage pfn_xrAcquireSwapchainImage = NULL;
static PFN_xrWaitSwapchainImage pfn_xrWaitSwapchainImage = NULL;
static PFN_xrReleaseSwapchainImage pfn_xrReleaseSwapchainImage = NULL;

/* ========================================================================
 * Global State
 * ======================================================================== */

/* OpenXR state */
static XrInstance xr_instance = XR_NULL_HANDLE;
static XrSystemId xr_system_id = XR_NULL_SYSTEM_ID;
static XrSession xr_session = XR_NULL_HANDLE;
static XrSpace xr_local_space = XR_NULL_HANDLE;
static bool xr_session_running = false;
static bool xr_should_quit = false;

/* Swapchain state */
typedef struct {
    XrSwapchain swapchain;
    SDL_GPUTexture **images;
    SDL_GPUTexture *depth_texture;  /* Local depth buffer for z-ordering */
    XrExtent2Di size;
    SDL_GPUTextureFormat format;
    Uint32 image_count;
} VRSwapchain;

/* Depth buffer format - use D24 for wide compatibility */
static const SDL_GPUTextureFormat DEPTH_FORMAT = SDL_GPU_TEXTUREFORMAT_D24_UNORM;

static VRSwapchain *vr_swapchains = NULL;
static XrView *xr_views = NULL;
static Uint32 view_count = 0;

/* SDL GPU state */
static SDL_GPUDevice *gpu_device = NULL;
static SDL_GPUGraphicsPipeline *pipeline = NULL;
static SDL_GPUBuffer *vertex_buffer = NULL;
static SDL_GPUBuffer *index_buffer = NULL;

/* Animation time */
static float anim_time = 0.0f;
static Uint64 last_ticks = 0;

/* Cube scene configuration */
#define NUM_CUBES 5
static Vec3 cube_positions[NUM_CUBES] = {
    { 0.0f, 0.0f, -2.0f },      /* Center, in front */
    { -1.2f, 0.4f, -2.5f },     /* Upper left */
    { 1.2f, 0.3f, -2.5f },      /* Upper right */
    { -0.6f, -0.4f, -1.8f },    /* Lower left close */
    { 0.6f, -0.3f, -1.8f },     /* Lower right close */
};
static float cube_scales[NUM_CUBES] = { 1.0f, 0.6f, 0.6f, 0.5f, 0.5f };
static float cube_speeds[NUM_CUBES] = { 1.0f, 1.5f, -1.2f, 2.0f, -0.8f };

/* ========================================================================
 * Cleanup and Quit
 * ======================================================================== */

static void quit(int rc)
{
    SDL_Log("Cleaning up...");

    /* CRITICAL: Wait for GPU to finish before destroying resources
     * Per PR #14837 discussion - prevents Vulkan validation errors */
    if (gpu_device) {
        SDL_WaitForGPUIdle(gpu_device);
    }

    /* Release GPU resources first */
    if (pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(gpu_device, pipeline);
        pipeline = NULL;
    }
    if (vertex_buffer) {
        SDL_ReleaseGPUBuffer(gpu_device, vertex_buffer);
        vertex_buffer = NULL;
    }
    if (index_buffer) {
        SDL_ReleaseGPUBuffer(gpu_device, index_buffer);
        index_buffer = NULL;
    }

    /* Release swapchains and depth textures */
    if (vr_swapchains) {
        for (Uint32 i = 0; i < view_count; i++) {
            if (vr_swapchains[i].depth_texture) {
                SDL_ReleaseGPUTexture(gpu_device, vr_swapchains[i].depth_texture);
            }
            if (vr_swapchains[i].swapchain) {
                SDL_DestroyGPUXRSwapchain(gpu_device, vr_swapchains[i].swapchain, vr_swapchains[i].images);
            }
        }
        SDL_free(vr_swapchains);
        vr_swapchains = NULL;
    }

    if (xr_views) {
        SDL_free(xr_views);
        xr_views = NULL;
    }

    /* Destroy OpenXR resources */
    if (xr_local_space && pfn_xrDestroySpace) {
        pfn_xrDestroySpace(xr_local_space);
        xr_local_space = XR_NULL_HANDLE;
    }
    if (xr_session && pfn_xrDestroySession) {
        pfn_xrDestroySession(xr_session);
        xr_session = XR_NULL_HANDLE;
    }

    /* Destroy GPU device (this also handles XR instance cleanup) */
    if (gpu_device) {
        SDL_DestroyGPUDevice(gpu_device);
        gpu_device = NULL;
    }

    SDL_Quit();
    exit(rc);
}

/* ========================================================================
 * Shader Loading
 * ======================================================================== */

static SDL_GPUShader *load_shader(bool is_vertex, Uint32 sampler_count, Uint32 uniform_buffer_count)
{
    SDL_GPUShaderCreateInfo createinfo;
    createinfo.num_samplers = sampler_count;
    createinfo.num_storage_buffers = 0;
    createinfo.num_storage_textures = 0;
    createinfo.num_uniform_buffers = uniform_buffer_count;

    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(gpu_device);
    if (format & SDL_GPU_SHADERFORMAT_DXIL) {
        createinfo.format = SDL_GPU_SHADERFORMAT_DXIL;
        if (is_vertex) {
            createinfo.code = cube_vert_dxil;
            createinfo.code_size = cube_vert_dxil_len;
            createinfo.entrypoint = "main";
        } else {
            createinfo.code = cube_frag_dxil;
            createinfo.code_size = cube_frag_dxil_len;
            createinfo.entrypoint = "main";
        }
    } else if (format & SDL_GPU_SHADERFORMAT_SPIRV) {
        createinfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
        if (is_vertex) {
            createinfo.code = cube_vert_spv;
            createinfo.code_size = cube_vert_spv_len;
            createinfo.entrypoint = "main";
        } else {
            createinfo.code = cube_frag_spv;
            createinfo.code_size = cube_frag_spv_len;
            createinfo.entrypoint = "main";
        }
    } else {
        SDL_Log("No supported shader format found!");
        return NULL;
    }

    createinfo.stage = is_vertex ? SDL_GPU_SHADERSTAGE_VERTEX : SDL_GPU_SHADERSTAGE_FRAGMENT;
    createinfo.props = 0;

    return SDL_CreateGPUShader(gpu_device, &createinfo);
}

/* ========================================================================
 * OpenXR Function Loading
 * ======================================================================== */

static bool load_xr_functions(void)
{
    pfn_xrGetInstanceProcAddr = (PFN_xrGetInstanceProcAddr)SDL_OpenXR_GetXrGetInstanceProcAddr();
    if (!pfn_xrGetInstanceProcAddr) {
        SDL_Log("Failed to get xrGetInstanceProcAddr");
        return false;
    }

#define XR_LOAD(fn) \
    if (XR_FAILED(pfn_xrGetInstanceProcAddr(xr_instance, #fn, (PFN_xrVoidFunction*)&pfn_##fn))) { \
        SDL_Log("Failed to load " #fn); \
        return false; \
    }

    XR_LOAD(xrEnumerateViewConfigurationViews);
    XR_LOAD(xrEnumerateSwapchainImages);
    XR_LOAD(xrCreateReferenceSpace);
    XR_LOAD(xrDestroySpace);
    XR_LOAD(xrDestroySession);
    XR_LOAD(xrDestroyInstance);
    XR_LOAD(xrPollEvent);
    XR_LOAD(xrBeginSession);
    XR_LOAD(xrEndSession);
    XR_LOAD(xrWaitFrame);
    XR_LOAD(xrBeginFrame);
    XR_LOAD(xrEndFrame);
    XR_LOAD(xrLocateViews);
    XR_LOAD(xrAcquireSwapchainImage);
    XR_LOAD(xrWaitSwapchainImage);
    XR_LOAD(xrReleaseSwapchainImage);

#undef XR_LOAD

    SDL_Log("Loaded all XR functions successfully");
    return true;
}

/* ========================================================================
 * Pipeline and Buffer Creation
 * ======================================================================== */

static bool create_pipeline(SDL_GPUTextureFormat color_format)
{
    SDL_GPUShader *vert_shader = load_shader(true, 0, 1);
    SDL_GPUShader *frag_shader = load_shader(false, 0, 0);

    if (!vert_shader || !frag_shader) {
        if (vert_shader) SDL_ReleaseGPUShader(gpu_device, vert_shader);
        if (frag_shader) SDL_ReleaseGPUShader(gpu_device, frag_shader);
        return false;
    }

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vert_shader,
        .fragment_shader = frag_shader,
        .target_info = {
            .num_color_targets = 1,
            .color_target_descriptions = (SDL_GPUColorTargetDescription[]){{
                .format = color_format
            }},
            .has_depth_stencil_target = true,
            .depth_stencil_format = DEPTH_FORMAT
        },
        .depth_stencil_state = {
            .enable_depth_test = true,
            .enable_depth_write = true,
            .compare_op = SDL_GPU_COMPAREOP_LESS_OR_EQUAL
        },
        .rasterizer_state = {
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_CLOCKWISE,  /* Cube indices wind clockwise when viewed from outside */
            .fill_mode = SDL_GPU_FILLMODE_FILL
        },
        .vertex_input_state = {
            .num_vertex_buffers = 1,
            .vertex_buffer_descriptions = (SDL_GPUVertexBufferDescription[]){{
                .slot = 0,
                .pitch = sizeof(PositionColorVertex),
                .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX
            }},
            .num_vertex_attributes = 2,
            .vertex_attributes = (SDL_GPUVertexAttribute[]){{
                .location = 0,
                .buffer_slot = 0,
                .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
                .offset = 0
            }, {
                .location = 1,
                .buffer_slot = 0,
                .format = SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM,
                .offset = sizeof(float) * 3
            }}
        },
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST
    };

    pipeline = SDL_CreateGPUGraphicsPipeline(gpu_device, &pipeline_info);

    SDL_ReleaseGPUShader(gpu_device, vert_shader);
    SDL_ReleaseGPUShader(gpu_device, frag_shader);

    if (!pipeline) {
        SDL_Log("Failed to create pipeline: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Created graphics pipeline for format %d", color_format);
    return true;
}

static bool create_cube_buffers(void)
{
    float s = CUBE_HALF_SIZE;

    PositionColorVertex vertices[24] = {
        /* Front face (red) */
        {-s,-s,-s, 255,0,0,255}, {s,-s,-s, 255,0,0,255}, {s,s,-s, 255,0,0,255}, {-s,s,-s, 255,0,0,255},
        /* Back face (green) */
        {s,-s,s, 0,255,0,255}, {-s,-s,s, 0,255,0,255}, {-s,s,s, 0,255,0,255}, {s,s,s, 0,255,0,255},
        /* Left face (blue) */
        {-s,-s,s, 0,0,255,255}, {-s,-s,-s, 0,0,255,255}, {-s,s,-s, 0,0,255,255}, {-s,s,s, 0,0,255,255},
        /* Right face (yellow) */
        {s,-s,-s, 255,255,0,255}, {s,-s,s, 255,255,0,255}, {s,s,s, 255,255,0,255}, {s,s,-s, 255,255,0,255},
        /* Top face (magenta) */
        {-s,s,-s, 255,0,255,255}, {s,s,-s, 255,0,255,255}, {s,s,s, 255,0,255,255}, {-s,s,s, 255,0,255,255},
        /* Bottom face (cyan) */
        {-s,-s,s, 0,255,255,255}, {s,-s,s, 0,255,255,255}, {s,-s,-s, 0,255,255,255}, {-s,-s,-s, 0,255,255,255}
    };

    Uint16 indices[36] = {
        0,1,2, 0,2,3,       /* Front */
        4,5,6, 4,6,7,       /* Back */
        8,9,10, 8,10,11,    /* Left */
        12,13,14, 12,14,15, /* Right */
        16,17,18, 16,18,19, /* Top */
        20,21,22, 20,22,23  /* Bottom */
    };

    SDL_GPUBufferCreateInfo vertex_buf_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = sizeof(vertices)
    };
    vertex_buffer = SDL_CreateGPUBuffer(gpu_device, &vertex_buf_info);
    CHECK_CREATE(vertex_buffer, "Vertex Buffer");

    SDL_GPUBufferCreateInfo index_buf_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = sizeof(indices)
    };
    index_buffer = SDL_CreateGPUBuffer(gpu_device, &index_buf_info);
    CHECK_CREATE(index_buffer, "Index Buffer");

    /* Create transfer buffer and upload data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = sizeof(vertices) + sizeof(indices)
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(gpu_device, &transfer_info);
    CHECK_CREATE(transfer, "Transfer Buffer");

    void *data = SDL_MapGPUTransferBuffer(gpu_device, transfer, false);
    SDL_memcpy(data, vertices, sizeof(vertices));
    SDL_memcpy((Uint8*)data + sizeof(vertices), indices, sizeof(indices));
    SDL_UnmapGPUTransferBuffer(gpu_device, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu_device);
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation src_vertex = { .transfer_buffer = transfer, .offset = 0 };
    SDL_GPUBufferRegion dst_vertex = { .buffer = vertex_buffer, .offset = 0, .size = sizeof(vertices) };
    SDL_UploadToGPUBuffer(copy_pass, &src_vertex, &dst_vertex, false);

    SDL_GPUTransferBufferLocation src_index = { .transfer_buffer = transfer, .offset = sizeof(vertices) };
    SDL_GPUBufferRegion dst_index = { .buffer = index_buffer, .offset = 0, .size = sizeof(indices) };
    SDL_UploadToGPUBuffer(copy_pass, &src_index, &dst_index, false);

    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(gpu_device, transfer);

    SDL_Log("Created cube vertex (%u bytes) and index (%u bytes) buffers", (unsigned int)sizeof(vertices), (unsigned int)sizeof(indices));
    return true;
}

/* ========================================================================
 * XR Session Initialization
 * ======================================================================== */

static bool init_xr_session(void)
{
    XrResult result;

    /* Create session */
    XrSessionCreateInfo session_info = { XR_TYPE_SESSION_CREATE_INFO };
    result = SDL_CreateGPUXRSession(gpu_device, &session_info, &xr_session);
    XR_CHECK(result, "Failed to create XR session");

    /* Create reference space */
    XrReferenceSpaceCreateInfo space_info = { XR_TYPE_REFERENCE_SPACE_CREATE_INFO };
    space_info.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    space_info.poseInReferenceSpace.orientation.w = 1.0f; /* Identity quaternion */

    result = pfn_xrCreateReferenceSpace(xr_session, &space_info, &xr_local_space);
    XR_CHECK(result, "Failed to create reference space");

    return true;
}

static bool create_swapchains(void)
{
    XrResult result;

    /* Get view configuration */
    result = pfn_xrEnumerateViewConfigurationViews(
        xr_instance, xr_system_id,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        0, &view_count, NULL);
    XR_CHECK(result, "Failed to enumerate view config views (count)");

    SDL_Log("View count: %" SDL_PRIu32, view_count);

    XrViewConfigurationView *view_configs = SDL_calloc(view_count, sizeof(XrViewConfigurationView));
    for (Uint32 i = 0; i < view_count; i++) {
        view_configs[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    }

    result = pfn_xrEnumerateViewConfigurationViews(
        xr_instance, xr_system_id,
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        view_count, &view_count, view_configs);
    if (XR_FAILED(result)) {
        SDL_free(view_configs);
        SDL_Log("Failed to enumerate view config views");
        return false;
    }

    /* Allocate swapchains and views */
    vr_swapchains = SDL_calloc(view_count, sizeof(VRSwapchain));
    xr_views = SDL_calloc(view_count, sizeof(XrView));

    /* Query available swapchain formats
     * Per PR #14837: format arrays are terminated with SDL_GPU_TEXTUREFORMAT_INVALID */
    int num_formats = 0;
    SDL_GPUTextureFormat *formats = SDL_GetGPUXRSwapchainFormats(gpu_device, xr_session, &num_formats);
    if (!formats || num_formats == 0) {
        SDL_Log("Failed to get XR swapchain formats");
        SDL_free(view_configs);
        return false;
    }
    
    /* Use first available format (typically sRGB)
     * Note: Could iterate with: while (formats[i] != SDL_GPU_TEXTUREFORMAT_INVALID) */
    SDL_GPUTextureFormat swapchain_format = formats[0];
    SDL_Log("Using swapchain format: %d (of %d available)", swapchain_format, num_formats);
    
    /* Log all available formats for debugging */
    for (int f = 0; f < num_formats && formats[f] != SDL_GPU_TEXTUREFORMAT_INVALID; f++) {
        SDL_Log("  Available format [%d]: %d", f, formats[f]);
    }
    SDL_free(formats);

    for (Uint32 i = 0; i < view_count; i++) {
        xr_views[i].type = XR_TYPE_VIEW;
        xr_views[i].pose.orientation.w = 1.0f;

        SDL_Log("Eye %" SDL_PRIu32 ": recommended %ux%u", i,
                (unsigned int)view_configs[i].recommendedImageRectWidth,
                (unsigned int)view_configs[i].recommendedImageRectHeight);

        /* Create swapchain using OpenXR's XrSwapchainCreateInfo */
        XrSwapchainCreateInfo swapchain_info = { XR_TYPE_SWAPCHAIN_CREATE_INFO };
        swapchain_info.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swapchain_info.format = 0; /* Ignored - SDL uses the format parameter */
        swapchain_info.sampleCount = 1;
        swapchain_info.width = view_configs[i].recommendedImageRectWidth;
        swapchain_info.height = view_configs[i].recommendedImageRectHeight;
        swapchain_info.faceCount = 1;
        swapchain_info.arraySize = 1;
        swapchain_info.mipCount = 1;

        result = SDL_CreateGPUXRSwapchain(
            gpu_device,
            xr_session,
            &swapchain_info,
            swapchain_format,
            &vr_swapchains[i].swapchain,
            &vr_swapchains[i].images);

        vr_swapchains[i].format = swapchain_format;

        if (XR_FAILED(result)) {
            SDL_Log("Failed to create swapchain %" SDL_PRIu32, i);
            SDL_free(view_configs);
            return false;
        }

        /* Get image count by enumerating swapchain images */
        result = pfn_xrEnumerateSwapchainImages(vr_swapchains[i].swapchain, 0, &vr_swapchains[i].image_count, NULL);
        if (XR_FAILED(result)) {
            vr_swapchains[i].image_count = 3; /* Assume 3 if we can't query */
        }

        vr_swapchains[i].size.width = (int32_t)swapchain_info.width;
        vr_swapchains[i].size.height = (int32_t)swapchain_info.height;

        /* Create local depth texture for this eye
         * Per PR #14837: Depth buffers are "really recommended" for XR apps.
         * Using a local depth texture (not XR-managed) is the simplest approach
         * for proper z-ordering without requiring XR_KHR_composition_layer_depth. */
        SDL_GPUTextureCreateInfo depth_info = {
            .type = SDL_GPU_TEXTURETYPE_2D,
            .format = DEPTH_FORMAT,
            .width = swapchain_info.width,
            .height = swapchain_info.height,
            .layer_count_or_depth = 1,
            .num_levels = 1,
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
            .props = 0
        };
        vr_swapchains[i].depth_texture = SDL_CreateGPUTexture(gpu_device, &depth_info);
        if (!vr_swapchains[i].depth_texture) {
            SDL_Log("Failed to create depth texture for eye %" SDL_PRIu32 ": %s", i, SDL_GetError());
            SDL_free(view_configs);
            return false;
        }

        SDL_Log("Created swapchain %" SDL_PRIu32 ": %" SDL_PRIs32 "x%" SDL_PRIs32 ", %" SDL_PRIu32 " images, with depth buffer",
                i, vr_swapchains[i].size.width, vr_swapchains[i].size.height,
                vr_swapchains[i].image_count);
    }

    SDL_free(view_configs);

    /* Create the pipeline using the swapchain format */
    if (view_count > 0 && pipeline == NULL) {
        if (!create_pipeline(vr_swapchains[0].format)) {
            return false;
        }
        if (!create_cube_buffers()) {
            return false;
        }
    }

    return true;
}

/* ========================================================================
 * XR Event Handling
 * ======================================================================== */

static void handle_xr_events(void)
{
    XrEventDataBuffer event_buffer = { XR_TYPE_EVENT_DATA_BUFFER };

    while (pfn_xrPollEvent(xr_instance, &event_buffer) == XR_SUCCESS) {
        switch (event_buffer.type) {
            case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                XrEventDataSessionStateChanged *state_event =
                    (XrEventDataSessionStateChanged*)&event_buffer;

                SDL_Log("Session state changed: %d", state_event->state);

                switch (state_event->state) {
                    case XR_SESSION_STATE_READY: {
                        XrSessionBeginInfo begin_info = { XR_TYPE_SESSION_BEGIN_INFO };
                        begin_info.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

                        XrResult result = pfn_xrBeginSession(xr_session, &begin_info);
                        if (XR_SUCCEEDED(result)) {
                            SDL_Log("XR Session begun!");
                            xr_session_running = true;

                            /* Create swapchains now that session is ready */
                            if (!create_swapchains()) {
                                SDL_Log("Failed to create swapchains");
                                xr_should_quit = true;
                            }
                        }
                        break;
                    }
                    case XR_SESSION_STATE_STOPPING:
                        pfn_xrEndSession(xr_session);
                        xr_session_running = false;
                        break;
                    case XR_SESSION_STATE_EXITING:
                    case XR_SESSION_STATE_LOSS_PENDING:
                        xr_should_quit = true;
                        break;
                    default:
                        break;
                }
                break;
            }
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                xr_should_quit = true;
                break;
            default:
                break;
        }

        event_buffer.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

/* ========================================================================
 * Rendering
 * ======================================================================== */

static void render_frame(void)
{
    if (!xr_session_running) return;

    XrFrameState frame_state = { XR_TYPE_FRAME_STATE };
    XrFrameWaitInfo wait_info = { XR_TYPE_FRAME_WAIT_INFO };

    XrResult result = pfn_xrWaitFrame(xr_session, &wait_info, &frame_state);
    if (XR_FAILED(result)) return;

    XrFrameBeginInfo begin_info = { XR_TYPE_FRAME_BEGIN_INFO };
    result = pfn_xrBeginFrame(xr_session, &begin_info);
    if (XR_FAILED(result)) return;

    XrCompositionLayerProjectionView *proj_views = NULL;
    XrCompositionLayerProjection layer = { XR_TYPE_COMPOSITION_LAYER_PROJECTION };
    Uint32 layer_count = 0;
    const XrCompositionLayerBaseHeader *layers[1] = {0};

    if (frame_state.shouldRender && view_count > 0 && vr_swapchains != NULL) {
        /* Update animation time */
        Uint64 now = SDL_GetTicks();
        if (last_ticks == 0) last_ticks = now;
        float delta = (float)(now - last_ticks) / 1000.0f;
        last_ticks = now;
        anim_time += delta;

        /* Locate views */
        XrViewState view_state = { XR_TYPE_VIEW_STATE };
        XrViewLocateInfo locate_info = { XR_TYPE_VIEW_LOCATE_INFO };
        locate_info.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locate_info.displayTime = frame_state.predictedDisplayTime;
        locate_info.space = xr_local_space;

        Uint32 view_count_output;
        result = pfn_xrLocateViews(xr_session, &locate_info, &view_state, view_count, &view_count_output, xr_views);
        if (XR_FAILED(result)) {
            SDL_Log("xrLocateViews failed");
            goto endFrame;
        }

        proj_views = SDL_calloc(view_count, sizeof(XrCompositionLayerProjectionView));

        SDL_GPUCommandBuffer *cmd_buf = SDL_AcquireGPUCommandBuffer(gpu_device);

        /* Multi-pass stereo: render each eye separately */
        for (Uint32 i = 0; i < view_count; i++) {
            VRSwapchain *swapchain = &vr_swapchains[i];

            /* Acquire swapchain image */
            Uint32 image_index;
            XrSwapchainImageAcquireInfo acquire_info = { XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
            result = pfn_xrAcquireSwapchainImage(swapchain->swapchain, &acquire_info, &image_index);
            if (XR_FAILED(result)) continue;

            XrSwapchainImageWaitInfo wait_image_info = { XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO };
            wait_image_info.timeout = XR_INFINITE_DURATION;
            result = pfn_xrWaitSwapchainImage(swapchain->swapchain, &wait_image_info);
            if (XR_FAILED(result)) {
                XrSwapchainImageReleaseInfo release_info = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
                pfn_xrReleaseSwapchainImage(swapchain->swapchain, &release_info);
                continue;
            }

            /* Render the scene to this eye */
            SDL_GPUTexture *target_texture = swapchain->images[image_index];

            /* Build view and projection matrices from XR pose/fov */
            Mat4 view_matrix = Mat4_FromXrPose(xr_views[i].pose);
            Mat4 proj_matrix = Mat4_Projection(xr_views[i].fov, 0.05f, 100.0f);

            SDL_GPUColorTargetInfo color_target = {0};
            color_target.texture = target_texture;
            color_target.load_op = SDL_GPU_LOADOP_CLEAR;
            color_target.store_op = SDL_GPU_STOREOP_STORE;
            /* Dark blue background */
            color_target.clear_color.r = 0.05f;
            color_target.clear_color.g = 0.05f;
            color_target.clear_color.b = 0.15f;
            color_target.clear_color.a = 1.0f;

            /* Set up depth target for proper z-ordering */
            SDL_GPUDepthStencilTargetInfo depth_target = {0};
            depth_target.texture = swapchain->depth_texture;
            depth_target.clear_depth = 1.0f;  /* Far plane */
            depth_target.load_op = SDL_GPU_LOADOP_CLEAR;
            depth_target.store_op = SDL_GPU_STOREOP_DONT_CARE;  /* We don't need to preserve depth */
            depth_target.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
            depth_target.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
            depth_target.cycle = true;  /* Allow GPU to cycle the texture for efficiency */

            SDL_GPURenderPass *render_pass = SDL_BeginGPURenderPass(cmd_buf, &color_target, 1, &depth_target);

            if (pipeline && vertex_buffer && index_buffer) {
                SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

                SDL_GPUViewport viewport = {0, 0, (float)swapchain->size.width, (float)swapchain->size.height, 0, 1};
                SDL_SetGPUViewport(render_pass, &viewport);

                SDL_Rect scissor = {0, 0, swapchain->size.width, swapchain->size.height};
                SDL_SetGPUScissor(render_pass, &scissor);

                SDL_GPUBufferBinding vertex_binding = {vertex_buffer, 0};
                SDL_BindGPUVertexBuffers(render_pass, 0, &vertex_binding, 1);

                SDL_GPUBufferBinding index_binding = {index_buffer, 0};
                SDL_BindGPUIndexBuffer(render_pass, &index_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

                /* Draw each cube */
                for (int cube_idx = 0; cube_idx < NUM_CUBES; cube_idx++) {
                    float rot = anim_time * cube_speeds[cube_idx];
                    Vec3 pos = cube_positions[cube_idx];

                    /* Build model matrix: scale -> rotateY -> rotateX -> translate */
                    Mat4 scale = Mat4_Scale(cube_scales[cube_idx]);
                    Mat4 rotY = Mat4_RotationY(rot);
                    Mat4 rotX = Mat4_RotationX(rot * 0.7f);
                    Mat4 trans = Mat4_Translation(pos.x, pos.y, pos.z);

                    Mat4 model = Mat4_Multiply(Mat4_Multiply(Mat4_Multiply(scale, rotY), rotX), trans);
                    Mat4 mv = Mat4_Multiply(model, view_matrix);
                    Mat4 mvp = Mat4_Multiply(mv, proj_matrix);

                    SDL_PushGPUVertexUniformData(cmd_buf, 0, &mvp, sizeof(mvp));
                    SDL_DrawGPUIndexedPrimitives(render_pass, 36, 1, 0, 0, 0);
                }
            }

            SDL_EndGPURenderPass(render_pass);

            /* Release swapchain image */
            XrSwapchainImageReleaseInfo release_info = { XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
            pfn_xrReleaseSwapchainImage(swapchain->swapchain, &release_info);

            /* Set up projection view */
            proj_views[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            proj_views[i].pose = xr_views[i].pose;
            proj_views[i].fov = xr_views[i].fov;
            proj_views[i].subImage.swapchain = swapchain->swapchain;
            proj_views[i].subImage.imageRect.offset.x = 0;
            proj_views[i].subImage.imageRect.offset.y = 0;
            proj_views[i].subImage.imageRect.extent = swapchain->size;
            proj_views[i].subImage.imageArrayIndex = 0;
        }

        SDL_SubmitGPUCommandBuffer(cmd_buf);

        layer.space = xr_local_space;
        layer.viewCount = view_count;
        layer.views = proj_views;
        layers[0] = (XrCompositionLayerBaseHeader*)&layer;
        layer_count = 1;
    }

endFrame:;
    XrFrameEndInfo end_info = { XR_TYPE_FRAME_END_INFO };
    end_info.displayTime = frame_state.predictedDisplayTime;
    end_info.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end_info.layerCount = layer_count;
    end_info.layers = layers;

    pfn_xrEndFrame(xr_session, &end_info);

    if (proj_views) SDL_free(proj_views);
}

/* ========================================================================
 * Main
 * ======================================================================== */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    SDL_Log("SDL GPU OpenXR Spinning Cubes Test starting...");
    SDL_Log("Stereo rendering mode: Multi-pass (one render pass per eye)");

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    SDL_Log("SDL initialized");

    /* Create GPU device with OpenXR enabled */
    SDL_Log("Creating GPU device with OpenXR enabled...");

    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_SPIRV_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_SHADERS_DXIL_BOOLEAN, true);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, true);
    /* Enable XR - SDL will create the OpenXR instance for us */
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_ENABLE_BOOLEAN, true);
    SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_INSTANCE_POINTER, &xr_instance);
    SDL_SetPointerProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_SYSTEM_ID_POINTER, &xr_system_id);
    SDL_SetStringProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_NAME_STRING, "SDL XR Spinning Cubes Test");
    SDL_SetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_XR_APPLICATION_VERSION_NUMBER, 1);

    gpu_device = SDL_CreateGPUDeviceWithProperties(props);
    SDL_DestroyProperties(props);

    if (!gpu_device) {
        SDL_Log("Failed to create GPU device: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    /* Load OpenXR function pointers */
    if (!load_xr_functions()) {
        SDL_Log("Failed to load XR functions");
        quit(1);
    }

    /* Initialize XR session */
    if (!init_xr_session()) {
        SDL_Log("Failed to init XR session");
        quit(1);
    }

    SDL_Log("Entering main loop... Put on your VR headset!");

    /* Main loop */
    while (!xr_should_quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                xr_should_quit = true;
            }
            if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE) {
                xr_should_quit = true;
            }
        }

        handle_xr_events();
        render_frame();
    }

    quit(0);
    return 0;
}
