/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2022 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef SDL_gpu_h_
#define SDL_gpu_h_

/**
 *  \file SDL_gpu.h
 *
 *  Header for the SDL GPU routines.
 */

#include "SDL_stdinc.h"
#include "SDL_error.h"

#include "begin_code.h"
/* Set up for C function definitions, even when using C++ */
#ifdef __cplusplus
extern "C" {
#endif

#ifndef SDL_SUPPRESS_GPU_API_UNSTABLE_WARNING
#warning The SDL GPU API is still in development. Expect things to change!
#warning DO NOT SHIP BUILDS OF SDL TO THE PUBLIC WITH THIS CODE IN IT.
#warning DO NOT SHIP _ANYTHING_ THAT USES THIS API.
#warning This warning will be removed when the API stabilizes.
#endif

/* !!! FIXME: this all needs formal (and significantly more robust) documentation. */

/*
 * The basic sizzle reel:
 *
 *  - You work in terms of modern GPU APIs without having to bog down
 *    in their specific minutiae.
 *  - It works on several APIs behind the scenes.
 *  - It's about _removing existing limitations_ without giving up
 *    most comfort, portability, or performance.
 *  - You write shaders in a simple language once, and either ship
 *    shader source code or bytecode. At runtime, we figure out how to
 *    make it work.
 *  - You work in one coordinate system and we deal with the
 *    differences for you.
 *
 *  !!! FIXME: explain shader policy (it's all a simple C-like thing,
 *  !!! FIXME: meant to be easy-ish to parse and write in. It is not
 *  !!! FIXME: GLSL or HLSL or MSL, because while being able to reuse
 *  !!! FIXME: shaders would be nice, the language specs are huge and
 *  !!! FIXME: carry various API-specific quirks, making them bad fits
 *  !!! FIXME: for SDL. It's quite possible to build external tools
 *  !!! FIXME: that will convert from existing shader languages to
 *  !!! FIXME: SDL_gpu. The same ideas apply to shader bytecode.)
 *
 *  !!! FIXME: document coordinate systems (it's the same as Direct3D 12
 *  !!! FIXME:  and Metal, which is what WebGPU landed on, too. We'll
 *  !!! FIXME:  convert behind the scenes for OpenGL and Vulkan where
 *  !!! FIXME:  appropriate).
 *
 *  Some rules and limitations:
 *  - There is no software renderer, and this API will not make heroic
 *    efforts to work on ancient GPUs and APIs.
 *  - This doesn't expose all of Metal/Vulkan/DX12. We are trying to
 *    drastically improve on SDL's render API functionality while
 *    keeping it simple-ish. Modern APIs put most of the heavy lifting
 *    into shaders, command queues, and precooked state objects, and
 *    we are exposing that specific set, which is powerful enough for
 *    almost anything you want to build outside of the highest of
 *    high end triple-AAA titles.
 *  - This exposes a feature set that the underlying API probably can't
 *    entirely lift before OpenGL 4 or Direct3D 11. For example, it allows
 *    vertex shaders to use samplers, which wasn't available in
 *    Direct3D 10. D3D11 was available in the retail release of
 *    Windows 7, though--and backported to Vista!--which is probably
 *    reasonable. It also means ancient, now-garbage GPUs are not and
 *    will not be supported. Hypothetically we could make _most_ of this
 *    work on OpenGL 2 and Direct3D 9, but why bother in modern times?
 *    (then again: maybe we can support enough of this to make many
 *    reasonable apps run on older GL/D3D, and just fail in
 *    SDL_GpuLoadShader on unsupported stuff).
 *  - Modern GPUs expect you to draw triangles, lines, or points.
 *    There are no quads or complex polygons. You can build them out of
 *    triangles yourself when you need them.
 *  - Modern APIs expose an enormous amount of fine-grained resource
 *    management, but I've opted for something simpler: there are GPU
 *    buffers and CPU buffers, and you have to queue a blit command to
 *    transfer between them. All the other stuff about what type of
 *    memory a buffer should be in, or CPU cache modes, etc, is mostly
 *    hidden here. GPU does fast things with GPU buffers, CPU does fast
 *    things with CPU buffers, transferring between them is slow, done.
 *  - You are NOT allowed to call into the underlying API directly.
 *    You can not force this to use OpenGL so you can intermingle
 *    your own OpenGL calls, etc. There is no compatibility functions
 *    to pull lowlevel API handles out of this to use in your own app.
 *    If you want to do this: just copy the source code out of here
 *    into your app, do what you like with it, and don't file a bug report.
 *    (!!! FIXME: it's been pointed out to me that there's a value in
 *    getting the lowlevel handles so you can plug them into OpenXR for
 *    rendering in a VR headset, and this seems worthwhile.)
 *  - The shader compiler is meant to be fast and lightweight. It does
 *    not do heavy optimizations of your code. It's meant to let you
 *    deal with source code at runtime, if you need to generate it on
 *    the fly for various reasons.
 *  - The shader bytecode is also meant to be fast and lightweight. Its
 *    primary goal is to convert quickly to whatever the underlying API
 *    needs. It's possible the underlying API might do an optimization
 *    pass, though.
 *  - There's no reason an offline compiler can't optimize the bytecode
 *    passed in here, but this doesn't currently exist and will not
 *    be implemented as a standard piece of the runtime.
 *
 *
 *  some things that modern GPU APIs offer that we aren't exposing
 *  (but that does not mean we will _never_ expose)...
 * 
 *  - compute
 *  - geometry shaders
 *  - tesselation
 *  - ray tracing
 *  - device enumeration/selection
 *  - multiple command queues (you can encode multiple command buffers, from multiple threads, though)
 *  - Most of the wild list of uncompressed texture formats.
 *  - texture slices (with the exception of cubemap faces)
 *
 *  some things I said no to originally that I was later convinced to support:
 *
 *  - multisample
 *  - texture arrays
 *  - compressed texture formats
 *  - instancing
 */

/*
 *  !!! FIXME: enumerate lowlevel APIs? In theory a Windows machine
 *   could offer all of Direct3D 9-12, Vulkan, OpenGL, GLES, etc...
 */

/* !!! FIXME: Enumerate physical devices. Right now this API doesn't allow it. */

typedef struct SDL_GpuDevice *SDL_GpuDevice;
SDL_GpuDevice *SDL_GpuCreateDevice(const char *name);  /* `name` is for debugging, not a specific device name to access. */
void SDL_GpuDestroyDevice(SDL_GpuDevice *device);

/* CPU buffers live in RAM and can be accessed by the CPU. */
SDL_GpuBuffer *SDL_GpuCreateCPUBuffer(SDL_GpuDevice *device, const Uint32 buflen);
void *SDL_GpuLockCPUBuffer(SDL_GpuBuffer *buffer, Uint32 *_buflen);
void SDL_GpuUnlockCPUBuffer(SDL_GpuBuffer *buffer);

/*
 * GPU buffers live in GPU-specific memory and can not be accessed by the CPU.
 *  If you need to get data to/from a GPU buffer, encode a blit operation
 *  to move it from/to a CPU buffer. Once in a CPU buffer, you can lock it to access data in your code.
 */
SDL_GpuBuffer *SDL_GpuCreateBuffer(SDL_GpuDevice *device, const Uint32 length);
void SDL_GpuDestroyBuffer(SDL_GpuBuffer *buffer);


typedef enum SDL_GpuTextureType
{
    SDL_GPUTEXTYPE_1D,
    SDL_GPUTEXTYPE_2D,
    SDL_GPUTEXTYPE_CUBE,
    SDL_GPUTEXTYPE_3D,
    SDL_GPUTEXTYPE_2D_ARRAY,
    SDL_GPUTEXTYPE_CUBE_ARRAY
} SDL_GpuTextureType;

typedef enum SDL_GpuPixelFormat
{
    SDL_GPUPIXELFMT_B5G6R5,
    SDL_GPUPIXELFMT_BGR5A1,
    SDL_GPUPIXELFMT_RGBA8,
    SDL_GPUPIXELFMT_RGBA8_sRGB,
    SDL_GPUPIXELFMT_BGRA8,
    SDL_GPUPIXELFMT_BGRA8_sRGB,
    SDL_GPUPIXELFMT_Depth24_Stencil8
    /* !!! FIXME: some sort of YUV format to let movies stream efficiently? */
}   /* !!! FIXME: s3tc? pvrtc? other compressed formats? We'll need a query for what's supported, and/or guarantee it with a software fallback...? */
} SDL_GpuPixelFormat;

/* you can specify multiple values OR'd together for texture usage, for example if you are going to render to it and then later
   sample the rendered-to texture's contents in a shader, you'd want RENDER_TARGET|SHADER_READ */
typedef enum SDL_GpuTextureUsage
{
    SDL_GPUTEXUSAGE_SHADER_READ = (1 << 0),    /* If you sample from a texture, you need this flag. */
    SDL_GPUTEXUSAGE_SHADER_WRITE = (1 << 1),
    SDL_GPUTEXUSAGE_RENDER_TARGET = (1 << 2),   /* Draw to this texture! You don't need to set SHADER_WRITE to use this flag! */
    SDL_GPUTEXUSAGE_NO_SAMPLE = (1 << 3)   /* You won't sample from this texture at all, just read or write it. */
} SDL_GpuTextureUsage;

typedef struct SDL_GpuTextureDescription
{
    const char *name;
    SDL_GpuTextureType texture_type;
    SDL_GpuPixelFormat pixel_format;
    SDL_GpuTextureUsage usage;  /* OR SDL_GpuTextureUsage values together */
    Uint32 width;
    Uint32 height;
    Uint32 depth_or_slices;
    Uint32 mipmap_levels;
} SDL_GpuTextureDescription;

SDL_GpuTexture *SDL_GpuCreateTexture(SDL_GpuDevice *device, const SDL_GpuTextureDescription *desc);
void SDL_GpuDestroyTexture(SDL_GpuTexture *texture);

/* compiling shaders is a different (and optional at runtime) piece, in SDL_gpu_compiler.h */
SDL_GpuShader *SDL_GpuLoadShader(SDL_GpuDevice *device, const Uint8 *bytecode, const Uint32 bytecodelen);
void SDL_GpuDestroyShader(SDL_GpuShader *shader);


/* !!! FIXME: I don't know what this is going to look like yet, this is a placeholder. */
SDL_GpuTexture *SDL_GpuGetBackbuffer(SDL_GpuDevice *device);


/* PRECOOKED STATE OBJECTS... */

typedef enum SDL_GpuBlendOperation
{
    SDL_GPUBLENDOP_ADD,
    SDL_GPUBLENDOP_SUBTRACT,
    SDL_GPUBLENDOP_REVERSESUBTRACT,
    SDL_GPUBLENDOP_MIN,
    SDL_GPUBLENDOP_MAX
} SDL_GpuBlendOperation;

typedef enum SDL_GpuBlendFactor
{
    SDL_GPUBLENDFACTOR_ZERO,
    SDL_GPUBLENDFACTOR_ONE,
    SDL_GPUBLENDFACTOR_SOURCECOLOR,
    SDL_GPUBLENDFACTOR_ONEMINUSSOURCECOLOR,
    SDL_GPUBLENDFACTOR_SOURCEALPHA,
    SDL_GPUBLENDFACTOR_ONEMINUSSOURCEALPHA,
    SDL_GPUBLENDFACTOR_DESTINATIONCOLOR,
    SDL_GPUBLENDFACTOR_ONEMINUSDESTINATIONCOLOR,
    SDL_GPUBLENDFACTOR_DESTINATIONALPHA,
    SDL_GPUBLENDFACTOR_ONEMINUSDESTINATIONALPHA,
    SDL_GPUBLENDFACTOR_SOURCEALPHASATURATED,
    SDL_GPUBLENDFACTOR_BLENDCOLOR,
    SDL_GPUBLENDFACTOR_ONEMINUSBLENDCOLOR,
    SDL_GPUBLENDFACTOR_BLENDALPHA,
    SDL_GPUBLENDFACTOR_ONEMINUSBLENDALPHA,
    SDL_GPUBLENDFACTOR_SOURCE1COLOR,
    SDL_GPUBLENDFACTOR_ONEMINUSSOURCE1COLOR,
    SDL_GPUBLENDFACTOR_SOURCE1ALPHA,
    SDL_GPUBLENDFACTOR_ONEMINUSSOURCE1ALPHA
} SDL_GpuBlendFactor;

typedef struct SDL_GpuPipelineColorAttachmentDescription
{
    SDL_GpuPixelFormat pixel_format;
    SDL_bool writemask_enabled_red;
    SDL_bool writemask_enabled_blue;
    SDL_bool writemask_enabled_green;
    SDL_bool writemask_enabled_alpha;
    SDL_bool blending_enabled;
    SDL_GpuBlendOperation alpha_blend_op;
    SDL_GpuBlendFactor alpha_src_blend_factor;
    SDL_GpuBlendFactor alpha_dst_blend_factor;
    SDL_GpuBlendOperation rgb_blend_op;
    SDL_GpuBlendFactor rgb_src_blend_factor;
    SDL_GpuBlendFactor rgb_dst_blend_factor;
} SDL_GpuColorAttachmentDescription;

typedef enum SDL_GpuVertexFormat
{
    SDL_GPUVERTFMT_UCHAR2,
    SDL_GPUVERTFMT_UCHAR4,
    SDL_GPUVERTFMT_CHAR2,
    SDL_GPUVERTFMT_CHAR4,
    SDL_GPUVERTFMT_UCHAR2_NORMALIZED,
    SDL_GPUVERTFMT_UCHAR4_NORMALIZED,
    SDL_GPUVERTFMT_CHAR2_NORMALIZED,
    SDL_GPUVERTFMT_CHAR4_NORMALIZED,
    SDL_GPUVERTFMT_USHORT,
    SDL_GPUVERTFMT_USHORT2,
    SDL_GPUVERTFMT_USHORT4,
    SDL_GPUVERTFMT_SHORT,
    SDL_GPUVERTFMT_SHORT2,
    SDL_GPUVERTFMT_SHORT4,
    SDL_GPUVERTFMT_USHORT_NORMALIZED,
    SDL_GPUVERTFMT_USHORT2_NORMALIZED,
    SDL_GPUVERTFMT_USHORT4_NORMALIZED,
    SDL_GPUVERTFMT_SHORT_NORMALIZED,
    SDL_GPUVERTFMT_SHORT2_NORMALIZED,
    SDL_GPUVERTFMT_SHORT4_NORMALIZED,
    SDL_GPUVERTFMT_HALF,
    SDL_GPUVERTFMT_HALF2,
    SDL_GPUVERTFMT_HALF4,
    SDL_GPUVERTFMT_FLOAT,
    SDL_GPUVERTFMT_FLOAT2,
    SDL_GPUVERTFMT_FLOAT3,
    SDL_GPUVERTFMT_FLOAT4,
    SDL_GPUVERTFMT_UINT,
    SDL_GPUVERTFMT_UINT2,
    SDL_GPUVERTFMT_UINT3,
    SDL_GPUVERTFMT_UINT4,
    SDL_GPUVERTFMT_INT,
    SDL_GPUVERTFMT_INT2,
    SDL_GPUVERTFMT_INT3,
    SDL_GPUVERTFMT_INT4
} SDL_GpuVertexFormat;

typedef struct SDL_GpuVertexAttributeDescription
{
    SDL_GpuVertexFormat format;
    Uint32 offset;
    Uint32 stride;
    Uint32 index;
} SDL_GpuVertexAttributeDescription;

typedef enum SDL_GpuCompareFunction
{
    SDL_GPUCMPFUNC_NEVER,
    SDL_GPUCMPFUNC_LESS,
    SDL_GPUCMPFUNC_EQUAL,
    SDL_GPUCMPFUNC_LESSEQUAL,
    SDL_GPUCMPFUNC_GREATER,
    SDL_GPUCMPFUNC_NOTEQUAL,
    SDL_GPUCMPFUNC_GREATEREQUAL,
    SDL_GPUCMPFUNC_ALWAYS
} SDL_GpuCompareFunction;

typedef enum SDL_GpuStencilOperation
{
    SDL_GPUSTENCILOP_KEEP,
    SDL_GPUSTENCILOP_ZERO,
    SDL_GPUSTENCILOP_REPLACE,
    SDL_GPUSTENCILOP_INCREMENTCLAMP,
    SDL_GPUSTENCILOP_DECREMENTCLAMP,
    SDL_GPUSTENCILOP_INVERT,
    SDL_GPUSTENCILOP_INCREMENTWRAP,
    SDL_GPUSTENCILOP_DECREMENTWRAP
} SDL_GpuStencilOperation;

typedef enum SDL_GpuPrimitive
{
    SDL_GPUPRIM_POINT,
    SDL_GPUPRIM_LINE,
    SDL_GPUPRIM_LINESTRIP,
    SDL_GPUPRIM_TRIANGLE,
    SDL_GPUPRIM_TRIANGLESTRIP
} SDL_GpuPrimitive;

typedef enum SDL_GpuFillMode
{
    SDL_GPUFILL_FILL,  /* fill polygons */
    SDL_GPUFILL_LINE  /* wireframe mode */
    /* !!! FIXME: Vulkan has POINT and FILL_RECTANGLE_NV here, but Metal and D3D12 do not. */
} SDL_GpuFillMode;

typedef enum SDL_GpuFrontFace
{
    SDL_GPUFRONTFACE_COUNTER_CLOCKWISE,
    SDL_GPUFRONTFACE_CLOCKWISE
} SDL_GpuFrontFace;

typedef enum SDL_GpuCullFace
{
    SDL_GPUCULLFACE_BACK,
    SDL_GPUCULLFACE_FRONT,
    SDL_GPUCULLFACE_NONE
    /* !!! FIXME: Vulkan lets you cull front-and-back (i.e. - everything) */
} SDL_GpuCullFace;

#define SDL_GPU_MAX_COLOR_ATTACHMENTS 4   /* !!! FIXME: what's a sane number here? */
#define SDL_GPU_MAX_VERTEX_ATTRIBUTES 32   /* !!! FIXME: what's a sane number here? */
typedef struct SDL_GpuPipelineDescription
{
    const char *name;
    SDL_GpuPrimitive primitive;
    SDL_GpuShader *vertex_shader;
    SDL_GpuShader *fragment_shader;
    Uint32 num_vertex_attributes;
    SDL_GpuVertexAttributeDescription[SDL_GPU_MAX_VERTEX_ATTRIBUTES];  /* !!! FIXME: maybe don't hardcode a static array? */
    Uint32 num_color_attachments;
    SDL_GpuPipelineColorAttachmentDescription[SDL_GPU_MAX_COLOR_ATTACHMENTS];  /* !!! FIXME: maybe don't hardcode a static array? */
    SDL_GpuPixelFormat depth_format;
    SDL_GpuPixelFormat stencil_format;
    SDL_bool depth_write_enabled;
    Uint32 stencil_read_mask;
    Uint32 stencil_write_mask;
    Uint32 stencil_reference_front;
    Uint32 stencil_reference_back;
    SDL_GpuCompareFunction depth_function;
    SDL_GpuCompareFunction stencil_function;
    SDL_GpuStencilOperation stencil_fail;
    SDL_GpuStencilOperation depth_fail;
    SDL_GpuStencilOperation depth_and_stencil_pass;
    SDL_GpuFillMode fill_mode;
    SDL_GpuFrontFace front_face;
    SDL_GpuCullFace cull_face;
    float depth_bias;
    float depth_bias_scale;
    float depth_bias_clamp;
} SDL_GpuPipelineDescription;

typedef struct SDL_GpuPipeline SDL_GpuPipeline;
SDL_GpuPipeline *SDL_GpuCreatePipeline(SDL_GpuDevice *device, const SDL_GpuPipelineDescription *desc);
void SDL_GpuDestroyPipeline(SDL_GpuPipeline *pipeline);

/* these make it easier to set up a Pipeline description; set the defaults (or
   start with an existing pipeline's state) then change what you like. */
void SDL_GpuDefaultPipelineDescription(SDL_GpuPipelineDescription *desc);
void SDL_GpuGetPipelineDescription(SDL_GpuPipeline *pipeline, SDL_GpuPipelineDescription *desc);



typedef enum SDL_GpuSamplerAddressMode
{
    SDL_GPUSAMPADDR_CLAMPTOEDGE,
    SDL_GPUSAMPADDR_MIRRORCLAMPTOEDGE,
    SDL_GPUSAMPADDR_REPEAT,
    SDL_GPUSAMPADDR_MIRRORREPEAT,
    SDL_GPUSAMPADDR_CLAMPTOZERO,
    SDL_GPUSAMPADDR_CLAMPTOBORDERCOLOR
} SDL_GpuSamplerAddressMode;

typedef enum SDL_GpuSamplerBorderColor
{
    SDL_GPUSAMPBORDER_TRANSPARENT_BLACK,
    SDL_GPUSAMPBORDER_OPAQUE_BLACK,
    SDL_GPUSAMPBORDER_OPAQUE_WHITE
} SDL_GpuSamplerBorderColor;

typedef enum SDL_GpuSamplerMinMagFilter
{
    SDL_GPUMINMAGFILTER_NEAREST,
    SDL_GPUMINMAGFILTER_LINEAR
} SDL_GpuSamplerMinMagFilter;

typedef enum SDL_GpuSamplerMipFilter
{
    SDL_GPUMIPFILTER_NOTMIPMAPPED,
    SDL_GPUMIPFILTER_NEAREST,
    SDL_GPUMIPFILTER_LINEAR
} SDL_GpuSamplerMipFilter;

typedef struct SDL_GpuSamplerDescription
{
    const char *name;
    SDL_GpuSamplerAddressMode addrmode_u;
    SDL_GpuSamplerAddressMode addrmode_v;
    SDL_GpuSamplerAddressMode addrmode_r;
    SDL_GpuSamplerBorderColor border_color;
    SDL_GpuSamplerMinMagFilter min_filter;
    SDL_GpuSamplerMinMagFilter mag_filter;
    SDL_GpuSamplerMipFilter mip_filter;
} SDL_GpuSamplerDescription;

typedef struct SDL_GpuSampler SDL_GpuSampler;
SDL_GpuSampler *SDL_GpuCreateSampler(SDL_GpuDevice *device, const SDL_GpuSamplerDescription *desc);
void SDL_GpuDestroySampler(SDL_GpuSampler *sampler);



/*
 * STATE CACHE CONVENIENCE FUNCTIONS...
 *
 * If you have only a few pipeline/etc states, or you have a system to manage
 *  them already, you don't need to use a StateCache. But if you're
 *  planning to make a bunch of states, we can manage them for you so you
 *  just tell us what you need and we either give you a previously-made object
 *  or create/cache a new one as needed. You can have multiple caches, so as
 *  to group related states together. You can then dump all the states at
 *  once, perhaps on level load, by deleting a specific cache.
 */
typedef struct SDL_GpuStateCache SDL_GpuStateCache;
SDL_GpuStateCache *SDL_GpuCreateStateCache(const char *name, SDL_GpuDevice *device);
SDL_GpuPipeline *SDL_GpuGetCachedPipeline(SDL_GpuStateCache *cache, const SDL_GpuPipelineDescription *desc);
SDL_GpuSampler *SDL_GpuGetCachedSampler(SDL_GpuStateCache *cache, const SDL_GpuSamplerDescription *desc);
void SDL_GpuDestroyStateCache(SDL_GpuStateCache *cache);

// !!! FIXME: read/write state caches to disk?


/*
 * COMMAND BUFFERS...
 *
 *  Commands to send to the GPU are encoded into command buffers.
 *   You make a command buffer, you encode commands into it, then you
 *   commit the buffer. Once committed, the buffer is sent to the GPU.
 *   The GPU processes these buffers in the order they were committed,
 *   running the buffer's commands in the order they were encoded.
 *  Command buffers may be encoded and committed from any thread, but
 *   only one thread may encode to a given command buffer at a time.
 *  You can only have one encoder for a command buffer at a time, but
 *   you can encode different types of commands (rendering and blitting,
 *   etc) into the same command buffer.
 */
typedef struct SDL_GpuCommandBuffer SDL_GpuCommandBuffer;
SDL_GpuCommandBuffer *SDL_GpuCreateCommandBuffer(const char *name, SDL_GpuDevice *device);


/* RENDERING PASSES... */

typedef enum SDL_GpuPassInit
{
    SDL_GPUPASSINIT_UNDEFINED,
    SDL_GPUPASSINIT_LOAD,
    SDL_GPUPASSINIT_CLEAR
} SDL_GpuPassInit;

typedef struct SDL_GpuColorAttachmentDescription
{
    SDL_GpuTexture *texture;   /* MUST be created with render target support! */
    SDL_GpuPassInit color_init;
    double clear_red;
    double clear_green;
    double clear_blue;
    double clear_alpha;
} SDL_GpuColorAttachmentDescription;

typedef struct SDL_GpuDepthAttachmentDescription
{
    SDL_GpuTexture *texture;   /* MUST be created with render target support! */
    SDL_GpuPassInit depth_init;
    double clear_depth;
} SDL_GpuDepthAttachmentDescription;

typedef struct SDL_GpuStencilAttachmentDescription
{
    SDL_GpuTexture *texture;   /* MUST be created with render target support! */
    SDL_GpuPassInit stencil_init;
    Uint32 clear_stencil;
} SDL_GpuDepthAttachmentDescription;

/* start encoding a render pass to a command buffer. You can only encode one type of pass to a command buffer at a time. End this pass to start encoding another. */
SDL_GpuRenderPass *SDL_GpuStartRenderPass(const char *name, SDL_GpuCommandBuffer *cmdbuf,
                            SDL_GpuPipeline *initial_pipeline,
                            Uint32 num_color_attachments,
                            const SDL_GpuColorAttachmentDescription *color_attachments,
                            const SDL_GpuDepthAttachmentDescription *depth_attachment,
                            const SDL_GpuStencilAttachmentDescription *stencil_attachment);


/*
 * These functions encode commands into the render pass...
 * 
 *  New states can be encoded into the render pass with these function and future encoded
 *   commands will use it. Previously encoded commands use whatever the current state
 *   was set to at the time. Try not to encode redundant state changes into a render pass
 *   as they will take resources to do nothing.
 */
void SDL_GpuSetRenderPassPipeline(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline);

void SDL_GpuSetRenderPassViewport(SDL_GpuRenderPass *pass, const double x, const double y, const double width, const double height, const double znear, const double zfar);
void SDL_GpuSetRenderPassScissor(SDL_GpuRenderPass *pass, const Uint32 x, const Uint32 y, const Uint32 width, const Uint32 height);
void SDL_GpuSetRenderBlendConstant(SDL_GpuRenderPass *pass, const double red, const double green, const double blue, const double alpha);

void SDL_GpuSetRenderPassVertexBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, const Uint32 offset, const Uint32 index);
void SDL_GpuSetRenderPassVertexSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, const Uint32 index);
void SDL_GpuSetRenderPassVertexTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, const Uint32 index);

void SDL_GpuSetRenderPassFragmentBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, const Uint32 offset, const Uint32 index);
void SDL_GpuSetRenderPassFragmentSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, const Uint32 index);
void SDL_GpuSetRenderPassFragmentTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, const Uint32 index);


/* Drawing! */

typedef enum SDL_GpuIndexType
{
    SDL_GPUINDEXTYPE_UINT16,
    SDL_GPUINDEXTYPE_UINT32
} SDL_GpuIndexType;

void SDL_GpuDraw(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count);
void SDL_GpuDrawIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset);
void SDL_GpuDrawInstanced(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count, Uint32 instance_count, Uint32 base_instance);
void SDL_GpuDrawInstancedIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset, Uint32 instance_count, Uint32 base_instance);

/* Done encoding this render pass into the command buffer. You can now commit the command buffer or start a new render (or whatever) pass. This `pass` pointer becomes invalid. */
void SDL_GpuEndRenderPass(SDL_GpuRenderPass *pass);

/* start encoding a blit pass to a command buffer. You can only encode one type of pass to a command buffer at a time.  End this pass to start encoding another. */
SDL_GpuBlitPass *SDL_GpuStartBlitPass(const char *name, SDL_GpuCommandBuffer *cmdbuf);
void SDL_GpuCopyBetweenTextures(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                                 Uint32 srcx, Uint32 srcy, Uint32 srcz,
                                 Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                 SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                                 Uint32 dstx, Uint32 dsty, Uint32 dstz);

void SDL_GpuFillBuffer(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, unsigned char value);

void SDL_GpuGenerateMipmaps(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture);

void SDL_GpuCopyBetweenBuffers(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length);

void SDL_GpuCopyFromBufferToTexture(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset,
                                     Uint32 srcpitch, Uint32 srcimgpitch,
                                     Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                     SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                                     Uint32 dstx, Uint32 dsty, Uint32 dstz);

void SDL_GpuCopyFromTextureToBuffer(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                                     Uint32 srcx, Uint32 srcy, Uint32 srcz,
                                     Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                     SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch);

/* Done encoding this blit pass into the command buffer. You can now commit the command buffer or start a new render (or whatever) pass. This `pass` pointer becomes invalid. */
void SDL_GpuEndBlitPass(SDL_GpuBlitPass *pass);


/*
 * COMMAND BUFFER SUBMISSION ...
 *
 * When submitting command buffers, you can optionally specify a fence. This fence object is used to tell you when
 *  the GPU has completed the work submitted in this batch, so your program can tell when it's completed some effort
 *  and if it's safe to touch resources that are no longer in-flight.
 */
typedef struct SDL_GpuFence SDL_GpuFence;
SDL_GpuFence *SDL_GpuCreateFence(SDL_GpuDevice *device);
void SDL_GpuDestroyFence(SDL_GpuFence *fence);
int SDL_GpuQueryFence(SDL_GpuFence *fence);
int SDL_GpuResetFence(SDL_GpuFence *fence);
int SDL_GpuWaitFence(SDL_GpuFence *fence);

/*
 * Once you've encoded your command buffer(s), you can submit them to the GPU for executing.
 * Command buffers are executed in the order they are submitted, and the commands in those buffers are executed in the order they were encoded.
 * Once a command buffer is submitted, its pointer becomes invalid. Create a new one for the next set of commands.
 */
void SDL_GpuSubmitCommandBuffers(SDL_GpuCommandBuffer **buffers, const Uint32 numcmdbufs, const SDL_bool also_present, SDL_GpuFence *fence);

/* !!! FIXME: add a SDL_GpuAbandonCommandBuffer() function for freeing a buffer without submitting it? */

/* Ends C function definitions when using C++ */
#ifdef __cplusplus
}
#endif
#include "close_code.h"

#endif /* SDL_gpu_h_ */

/* vi: set ts=4 sw=4 expandtab: */
