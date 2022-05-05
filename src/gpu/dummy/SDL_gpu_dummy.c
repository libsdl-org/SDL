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
#include "../../SDL_internal.h"

/* The high-level gpu subsystem */

#include "SDL.h"
#include "../SDL_sysgpu.h"

static void DUMMY_GpuDestroyDevice(SDL_GpuDevice *device) { /* no-op */ }

static int DUMMY_GpuCreateCpuBuffer(SDL_GpuCpuBuffer *buffer, const void *data)
{
    /* have to save off buffer data so we can provide it for locking, etc. */
    buffer->driverdata = SDL_calloc(1, buffer->buflen);
    if (!buffer->driverdata) {
        return SDL_OutOfMemory();
    }
    if (data) {
        SDL_memcpy(buffer->driverdata, data, buffer->buflen);
    }
    return 0;
}

static void DUMMY_GpuDestroyCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    SDL_free(buffer->driverdata);
}

static void *DUMMY_GpuLockCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    return buffer->driverdata;
}

/* we could get fancier and manage imaginary GPU buffers and textures, but I don't think it's worth it atm. */

static int DUMMY_GpuUnlockCpuBuffer(SDL_GpuCpuBuffer *buffer) { return 0; }
static int DUMMY_GpuCreateBuffer(SDL_GpuBuffer *buffer) { return 0; }
static void DUMMY_GpuDestroyBuffer(SDL_GpuBuffer *buffer) {}
static int DUMMY_GpuCreateTexture(SDL_GpuTexture *texture) { return 0; }
static void DUMMY_GpuDestroyTexture(SDL_GpuTexture *texture) {}
static int DUMMY_GpuCreateShader(SDL_GpuShader *shader, const Uint8 *bytecode, const Uint32 bytecodelen) { return 0; }
static void DUMMY_GpuDestroyShader(SDL_GpuShader *shader) {}
static SDL_GpuTexture *DUMMY_GpuGetBackbuffer(SDL_GpuDevice *device, SDL_Window *window) { return NULL; /* !!! FIXME */ }
static int DUMMY_GpuCreatePipeline(SDL_GpuPipeline *pipeline) { return 0; }
static void DUMMY_GpuDestroyPipeline(SDL_GpuPipeline *pipeline) {}
static int DUMMY_GpuCreateSampler(SDL_GpuSampler *sampler) { return 0; }
static void DUMMY_GpuDestroySampler(SDL_GpuSampler *sampler) {}
static int DUMMY_GpuCreateCommandBuffer(SDL_GpuCommandBuffer *cmdbuf) { return 0; }
static int DUMMY_GpuSubmitCommandBuffers(SDL_GpuDevice *device, SDL_GpuCommandBuffer **buffers, const Uint32 numcmdbufs, SDL_GpuPresentType presenttype, SDL_GpuFence *fence) { return 0; }
static void DUMMY_GpuAbandonCommandBuffer(SDL_GpuCommandBuffer *buffer) {}
static int DUMMY_GpuStartRenderPass(SDL_GpuRenderPass *pass, Uint32 num_color_attachments, const SDL_GpuColorAttachmentDescription *color_attachments, const SDL_GpuDepthAttachmentDescription *depth_attachment, const SDL_GpuStencilAttachmentDescription *stencil_attachment) { return 0; }
static int DUMMY_GpuSetRenderPassPipeline(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline) { return 0; }
static int DUMMY_GpuSetRenderPassViewport(SDL_GpuRenderPass *pass, double x, double y, double width, double height, double znear, double zfar) { return 0; }
static int DUMMY_GpuSetRenderPassScissor(SDL_GpuRenderPass *pass, double x, double y, double width, double height) { return 0; }
static int DUMMY_GpuSetRenderPassBlendConstant(SDL_GpuRenderPass *pass, double red, double green, double blue, double alpha) { return 0; }
static int DUMMY_GpuSetRenderPassVertexBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index) { return 0; }
static int DUMMY_GpuSetRenderPassVertexSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index) { return 0; }
static int DUMMY_GpuSetRenderPassVertexTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index) { return 0; }
static int DUMMY_GpuSetRenderPassFragmentBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 index) { return 0; }
static int DUMMY_GpuSetRenderPassFragmentSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, Uint32 index) { return 0; }
static int DUMMY_GpuSetRenderPassFragmentTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, Uint32 index) { return 0; }
static int DUMMY_GpuDraw(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count) { return 0; }
static int DUMMY_GpuDrawIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset) { return 0; }
static int DUMMY_GpuDrawInstanced(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count, Uint32 instance_count, Uint32 base_instance) { return 0; }
static int DUMMY_GpuDrawInstancedIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset, Uint32 instance_count, Uint32 base_instance) { return 0; }
static int DUMMY_GpuEndRenderPass(SDL_GpuRenderPass *pass) { return 0; }
static int DUMMY_GpuStartBlitPass(SDL_GpuBlitPass *pass) { return 0; }
static int DUMMY_GpuCopyBetweenTextures(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel, Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz) { return 0; }
static int DUMMY_GpuFillBuffer(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, Uint8 value) { return 0; }
static int DUMMY_GpuGenerateMipmaps(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture) { return 0; }
static int DUMMY_GpuCopyBufferCpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuCpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length) { return 0; }
static int DUMMY_GpuCopyBufferGpuToCpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuCpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length) { return 0; }
static int DUMMY_GpuCopyBufferGpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length) { return 0; }
static int DUMMY_GpuCopyFromBufferToTexture(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, Uint32 srcpitch, Uint32 srcimgpitch, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel, Uint32 dstx, Uint32 dsty, Uint32 dstz) { return 0; }
static int DUMMY_GpuCopyFromTextureToBuffer(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel, Uint32 srcx, Uint32 srcy, Uint32 srcz, Uint32 srcw, Uint32 srch, Uint32 srcdepth, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch) { return 0; }
static int DUMMY_GpuEndBlitPass(SDL_GpuBlitPass *pass) { return 0; }
static int DUMMY_GpuCreateFence(SDL_GpuFence *fence) { return 0; }
static void DUMMY_GpuDestroyFence(SDL_GpuFence *fence) {}
static int DUMMY_GpuQueryFence(SDL_GpuFence *fence) { return 1; }
static int DUMMY_GpuResetFence(SDL_GpuFence *fence) { return 0; }
static int DUMMY_GpuWaitFence(SDL_GpuFence *fence) { return 0; }

static int
DUMMY_GpuCreateDevice(SDL_GpuDevice *device)
{
    device->DestroyDevice = DUMMY_GpuDestroyDevice;
    device->CreateCpuBuffer = DUMMY_GpuCreateCpuBuffer;
    device->DestroyCpuBuffer = DUMMY_GpuDestroyCpuBuffer;
    device->LockCpuBuffer = DUMMY_GpuLockCpuBuffer;
    device->UnlockCpuBuffer = DUMMY_GpuUnlockCpuBuffer;
    device->CreateBuffer = DUMMY_GpuCreateBuffer;
    device->DestroyBuffer = DUMMY_GpuDestroyBuffer;
    device->CreateTexture = DUMMY_GpuCreateTexture;
    device->DestroyTexture = DUMMY_GpuDestroyTexture;
    device->CreateShader = DUMMY_GpuCreateShader;
    device->DestroyShader = DUMMY_GpuDestroyShader;
    device->GetBackbuffer = DUMMY_GpuGetBackbuffer;
    device->CreatePipeline = DUMMY_GpuCreatePipeline;
    device->DestroyPipeline = DUMMY_GpuDestroyPipeline;
    device->CreateSampler = DUMMY_GpuCreateSampler;
    device->DestroySampler = DUMMY_GpuDestroySampler;
    device->CreateCommandBuffer = DUMMY_GpuCreateCommandBuffer;
    device->SubmitCommandBuffers = DUMMY_GpuSubmitCommandBuffers;
    device->AbandonCommandBuffer = DUMMY_GpuAbandonCommandBuffer;
    device->StartRenderPass = DUMMY_GpuStartRenderPass;
    device->SetRenderPassPipeline = DUMMY_GpuSetRenderPassPipeline;
    device->SetRenderPassViewport = DUMMY_GpuSetRenderPassViewport;
    device->SetRenderPassScissor = DUMMY_GpuSetRenderPassScissor;
    device->SetRenderPassBlendConstant = DUMMY_GpuSetRenderPassBlendConstant;
    device->SetRenderPassVertexBuffer = DUMMY_GpuSetRenderPassVertexBuffer;
    device->SetRenderPassVertexSampler = DUMMY_GpuSetRenderPassVertexSampler;
    device->SetRenderPassVertexTexture = DUMMY_GpuSetRenderPassVertexTexture;
    device->SetRenderPassFragmentBuffer = DUMMY_GpuSetRenderPassFragmentBuffer;
    device->SetRenderPassFragmentSampler = DUMMY_GpuSetRenderPassFragmentSampler;
    device->SetRenderPassFragmentTexture = DUMMY_GpuSetRenderPassFragmentTexture;
    device->Draw = DUMMY_GpuDraw;
    device->DrawIndexed = DUMMY_GpuDrawIndexed;
    device->DrawInstanced = DUMMY_GpuDrawInstanced;
    device->DrawInstancedIndexed = DUMMY_GpuDrawInstancedIndexed;
    device->EndRenderPass = DUMMY_GpuEndRenderPass;
    device->StartBlitPass = DUMMY_GpuStartBlitPass;
    device->CopyBetweenTextures = DUMMY_GpuCopyBetweenTextures;
    device->FillBuffer = DUMMY_GpuFillBuffer;
    device->GenerateMipmaps = DUMMY_GpuGenerateMipmaps;
    device->CopyBufferCpuToGpu = DUMMY_GpuCopyBufferCpuToGpu;
    device->CopyBufferGpuToCpu = DUMMY_GpuCopyBufferGpuToCpu;
    device->CopyBufferGpuToGpu = DUMMY_GpuCopyBufferGpuToGpu;
    device->CopyFromBufferToTexture = DUMMY_GpuCopyFromBufferToTexture;
    device->CopyFromTextureToBuffer = DUMMY_GpuCopyFromTextureToBuffer;
    device->EndBlitPass = DUMMY_GpuEndBlitPass;
    device->CreateFence = DUMMY_GpuCreateFence;
    device->DestroyFence = DUMMY_GpuDestroyFence;
    device->QueryFence = DUMMY_GpuQueryFence;
    device->ResetFence = DUMMY_GpuResetFence;
    device->WaitFence = DUMMY_GpuWaitFence;

    return 0;  /* okay, always succeeds. */
}

const SDL_GpuDriver DUMMY_GpuDriver = {
    "dummy", DUMMY_GpuCreateDevice
};

/* vi: set ts=4 sw=4 expandtab: */
