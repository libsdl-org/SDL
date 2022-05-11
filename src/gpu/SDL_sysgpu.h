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
#include "../SDL_internal.h"

#ifndef SDL_sysgpu_h_
#define SDL_sysgpu_h_

#define SDL_SUPPRESS_GPU_API_UNSTABLE_WARNING 1  /* !!! FIXME: remove later */
#include "SDL_gpu.h"
#include "../SDL_hashtable.h"


struct SDL_GpuCpuBuffer
{
    SDL_GpuDevice *device;
    const char *label;
    Uint32 buflen;
    void *driverdata;
};

struct SDL_GpuBuffer
{
    SDL_GpuDevice *device;
    const char *label;
    Uint32 buflen;
    void *driverdata;
};

struct SDL_GpuTexture
{
    SDL_GpuDevice *device;
    SDL_GpuTextureDescription desc;
    void *driverdata;
};

struct SDL_GpuShader
{
    SDL_GpuDevice *device;
    const char *label;
    SDL_atomic_t refcount;
    void *driverdata;
};

struct SDL_GpuPipeline
{
    SDL_GpuDevice *device;
    SDL_GpuPipelineDescription desc;
    void *driverdata;
};

struct SDL_GpuSampler
{
    SDL_GpuDevice *device;
    SDL_GpuSamplerDescription desc;
    void *driverdata;
};

struct SDL_GpuCommandBuffer
{
    SDL_GpuDevice *device;
    const char *label;
    SDL_bool currently_encoding;
    void *driverdata;
};

struct SDL_GpuRenderPass
{
    SDL_GpuDevice *device;
    const char *label;
    SDL_GpuCommandBuffer *cmdbuf;
    void *driverdata;
};

struct SDL_GpuBlitPass
{
    SDL_GpuDevice *device;
    const char *label;
    SDL_GpuCommandBuffer *cmdbuf;
    void *driverdata;
};

struct SDL_GpuFence
{
    SDL_GpuDevice *device;
    const char *label;
    void *driverdata;
};

struct SDL_GpuDevice
{
    const char *label;

    void *driverdata;

    void (*DestroyDevice)(SDL_GpuDevice *device);

    /* !!! FIXME: we need an UnclaimWindow for when the device (or window!) is being destroyed */
    int (*ClaimWindow)(SDL_GpuDevice *device, SDL_Window *window);

    int (*CreateCpuBuffer)(SDL_GpuCpuBuffer *buffer, const void *data);
    void (*DestroyCpuBuffer)(SDL_GpuCpuBuffer *buffer);
    void *(*LockCpuBuffer)(SDL_GpuCpuBuffer *buffer);
    int (*UnlockCpuBuffer)(SDL_GpuCpuBuffer *buffer);

    int (*CreateBuffer)(SDL_GpuBuffer *buffer);
    void (*DestroyBuffer)(SDL_GpuBuffer *buffer);

    int (*CreateTexture)(SDL_GpuTexture *texture);
    void (*DestroyTexture)(SDL_GpuTexture *texture);

    int (*CreateShader)(SDL_GpuShader *shader, const Uint8 *bytecode, const Uint32 bytecodelen);
    void (*DestroyShader)(SDL_GpuShader *shader);

    int (*CreatePipeline)(SDL_GpuPipeline *pipeline);
    void (*DestroyPipeline)(SDL_GpuPipeline *pipeline);

    int (*CreateSampler)(SDL_GpuSampler *sampler);
    void (*DestroySampler)(SDL_GpuSampler *sampler);

    int (*CreateCommandBuffer)(SDL_GpuCommandBuffer *cmdbuf);
    int (*SubmitCommandBuffers)(SDL_GpuDevice *device, SDL_GpuCommandBuffer **buffers, const Uint32 numcmdbufs, SDL_GpuFence *fence);
    void (*AbandonCommandBuffer)(SDL_GpuCommandBuffer *buffer);

    int (*StartRenderPass)(SDL_GpuRenderPass *pass, Uint32 num_color_attachments, const SDL_GpuColorAttachmentDescription *color_attachments, const SDL_GpuDepthAttachmentDescription *depth_attachment, const SDL_GpuStencilAttachmentDescription *stencil_attachment);
    int (*SetRenderPassPipeline)(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline);
    int (*SetRenderPassViewport)(SDL_GpuRenderPass *pass, double x, double y, double width, double height, double znear, double zfar);
    int (*SetRenderPassScissor)(SDL_GpuRenderPass *pass, Uint32 x, Uint32 y, Uint32 width, Uint32 height);
    int (*SetRenderPassBlendConstant)(SDL_GpuRenderPass *pass, double red, double green, double blue, double alpha);
    int (*SetRenderPassVertexBuffer)(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index);
    int (*SetRenderPassVertexSampler)(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index);
    int (*SetRenderPassVertexTexture)(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index);
    int (*SetRenderPassFragmentBuffer)(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index);
    int (*SetRenderPassFragmentSampler)(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index);
    int (*SetRenderPassFragmentTexture)(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index);
    int (*Draw)(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count);
    int (*DrawIndexed)(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset);
    int (*DrawInstanced)(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count, Uint32 instance_count, Uint32 base_instance);
    int (*DrawInstancedIndexed)(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset, Uint32 instance_count, Uint32 base_vertex, Uint32 base_instance);
    int (*EndRenderPass)(SDL_GpuRenderPass *pass);

    int (*StartBlitPass)(SDL_GpuBlitPass *pass);
    int (*CopyBetweenTextures)(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel, Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz);
    int (*FillBuffer)(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, Uint8 value);
    int (*GenerateMipmaps)(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture);
    int (*CopyBufferCpuToGpu)(SDL_GpuBlitPass *pass, SDL_GpuCpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length);
    int (*CopyBufferGpuToCpu)(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuCpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length);
    int (*CopyBufferGpuToGpu)(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length);
    int (*CopyFromBufferToTexture)(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, Uint32 srcpitch, Uint32 srcimgpitch, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz);
    int (*CopyFromTextureToBuffer)(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel, Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch);
    int (*EndBlitPass)(SDL_GpuBlitPass *pass);

    int (*GetBackbuffer)(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *texture);
    int (*Present)(SDL_GpuDevice *device, SDL_Window *window, SDL_GpuTexture *backbuffer, int swapinterval);

    int (*CreateFence)(SDL_GpuFence *fence);
    void (*DestroyFence)(SDL_GpuFence *fence);
    int (*QueryFence)(SDL_GpuFence *fence);
    int (*ResetFence)(SDL_GpuFence *fence);
    int (*WaitFence)(SDL_GpuFence *fence);
};

/* Multiple mutexes might be overkill, but there's no reason to
   block all caches when one is being accessed. */
struct SDL_GpuStateCache
{
    const char *label;
    SDL_GpuDevice *device;
    SDL_mutex *pipeline_mutex;
    SDL_HashTable *pipeline_cache;
    SDL_mutex *sampler_mutex;
    SDL_HashTable *sampler_cache;
};

typedef struct SDL_GpuDriver
{
    const char *name;
    int (*CreateDevice)(SDL_GpuDevice *device);
} SDL_GpuDriver;

#endif /* SDL_sysgpu_h_ */

/* vi: set ts=4 sw=4 expandtab: */
