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

/* The high-level gpu subsystem */

#include "SDL.h"
#include "SDL_sysgpu.h"

/* !!! FIXME: change this API to allow selection of a specific GPU? */
SDL_GpuDevice *
SDL_GpuCreateDevice(const char *label)
{
}

void
SDL_GpuDestroyDevice(SDL_GpuDevice *device)
{
}

SDL_GpuCpuBuffer *
SDL_GpuCreateCpuBuffer(const char *label, SDL_GpuDevice *device, const Uint32 buflen, const void *data)
{
}

void
SDL_GpuDestroyCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
}

void *
SDL_GpuLockCpuBuffer(SDL_GpuCpuBuffer *buffer, Uint32 *_buflen)
{
}

void
SDL_GpuUnlockCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
}

SDL_GpuBuffer *
SDL_GpuCreateBuffer(const char *label, SDL_GpuDevice *device, const Uint32 length)
{
}

void
SDL_GpuDestroyBuffer(SDL_GpuBuffer *buffer)
{
}


SDL_GpuTexture *
SDL_GpuCreateTexture(SDL_GpuDevice *device, const SDL_GpuTextureDescription *desc)
{
}

void
SDL_GpuGetTextureDescription(SDL_GpuTexture *texture, SDL_GpuTextureDescription *desc)
{
}

void
SDL_GpuDestroyTexture(SDL_GpuTexture *texture)
{
}

SDL_GpuShader *
SDL_GpuLoadShader(SDL_GpuDevice *device, const Uint8 *bytecode, const Uint32 bytecodelen)
{
}

void
SDL_GpuDestroyShader(SDL_GpuShader *shader)
{
}

SDL_GpuTexture *
SDL_GpuGetBackbuffer(SDL_GpuDevice *device, SDL_Window *window)
{
}

SDL_GpuPipeline *
SDL_GpuCreatePipeline(SDL_GpuDevice *device, const SDL_GpuPipelineDescription *desc)
{
}

void
SDL_GpuDestroyPipeline(SDL_GpuPipeline *pipeline)
{
}

void
SDL_GpuDefaultPipelineDescription(SDL_GpuPipelineDescription *desc)
{
}

void
SDL_GpuGetPipelineDescription(SDL_GpuPipeline *pipeline, SDL_GpuPipelineDescription *desc)
{
}

SDL_GpuSampler *
SDL_GpuCreateSampler(SDL_GpuDevice *device, const SDL_GpuSamplerDescription *desc)
{
}

void
SDL_GpuDestroySampler(SDL_GpuSampler *sampler)
{
}


/* GpuStateCache hashtable implementations... */

/* !!! FIXME: crc32 algorithm is probably overkill here. */
#define CRC32_INIT_VALUE 0xFFFFFFFF
#define CRC32_APPEND_VAR(crc, var) crc = crc32_append(crc, &var, sizeof (var))
#define CRC32_FINISH(crc) crc ^= 0xFFFFFFFF

static Uint32
crc32_append(Uint32 crc, const void *_buf, const size_t buflen)
{
    const Uint8 *buf = (const Uint8 *) _buf;
    size_t i;
    for (i = 0; i < buflen; i++) {
        Uint32 xorval = (Uint32) ((crc ^ *(buf++)) & 0xFF);
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        xorval = ((xorval & 1) ? (0xEDB88320 ^ (xorval >> 1)) : (xorval >> 1));
        crc = xorval ^ (crc >> 8);
    } // for

    return crc;
}

static Uint32 hash_pipeline(const void *key, void *data)
{
    /* this hashes most pointers; this hash is meant to be unique and contained in this process. As such, it also doesn't care about enum size or byte order. */
    /* However, it _does_ care about uninitialized packing bytes, so it doesn't just hash the sizeof (object). */
    const SDL_GpuPipelineDescription *desc = (const SDL_GpuPipelineDescription *) key;
    Uint32 crc = CRC32_INIT_VALUE;
    Uint32 i;

    if (desc->label) { crc = crc32_append(crc, desc->label, SDL_strlen(desc->label)); }  /* NULL means less bytes hashed to keep it unique vs "". */

    CRC32_APPEND_VAR(crc, desc->primitive);
    CRC32_APPEND_VAR(crc, desc->vertex_shader);
    CRC32_APPEND_VAR(crc, desc->fragment_shader);

    CRC32_APPEND_VAR(crc, desc->num_vertex_attributes);
    for (i = 0; i < desc->num_vertex_attributes; i++) {
        CRC32_APPEND_VAR(crc, desc->vertices[i].format);
        CRC32_APPEND_VAR(crc, desc->vertices[i].offset);
        CRC32_APPEND_VAR(crc, desc->vertices[i].stride);
        CRC32_APPEND_VAR(crc, desc->vertices[i].index);
    }

    CRC32_APPEND_VAR(crc, desc->num_color_attachments);
    for (i = 0; i < desc->num_color_attachments; i++) {
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].pixel_format);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].writemask_enabled_red);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].writemask_enabled_blue);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].writemask_enabled_green);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].writemask_enabled_alpha);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].blending_enabled);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].alpha_blend_op);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].alpha_src_blend_factor);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].alpha_dst_blend_factor);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].rgb_blend_op);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].rgb_src_blend_factor);
        CRC32_APPEND_VAR(crc, desc->color_attachments[i].rgb_dst_blend_factor);
    }

    CRC32_APPEND_VAR(crc, desc->depth_format);
    CRC32_APPEND_VAR(crc, desc->stencil_format);
    CRC32_APPEND_VAR(crc, desc->depth_write_enabled);
    CRC32_APPEND_VAR(crc, desc->stencil_read_mask);
    CRC32_APPEND_VAR(crc, desc->stencil_write_mask);
    CRC32_APPEND_VAR(crc, desc->stencil_reference_front);
    CRC32_APPEND_VAR(crc, desc->stencil_reference_back);
    CRC32_APPEND_VAR(crc, desc->depth_function);
    CRC32_APPEND_VAR(crc, desc->stencil_function);
    CRC32_APPEND_VAR(crc, desc->stencil_fail);
    CRC32_APPEND_VAR(crc, desc->depth_fail);
    CRC32_APPEND_VAR(crc, desc->depth_and_stencil_pass);
    CRC32_APPEND_VAR(crc, desc->fill_mode);
    CRC32_APPEND_VAR(crc, desc->front_face);
    CRC32_APPEND_VAR(crc, desc->cull_face);
    CRC32_APPEND_VAR(crc, desc->depth_bias);
    CRC32_APPEND_VAR(crc, desc->depth_bias_scale);
    CRC32_APPEND_VAR(crc, desc->depth_bias_clamp);

    return CRC32_FINISH(crc);
}

static SDL_bool keymatch_pipeline(const void *_a, const void *_b, void *data)
{
    const SDL_GpuPipelineDescription *a = (const SDL_GpuPipelineDescription *) _a;
    const SDL_GpuPipelineDescription *b = (const SDL_GpuPipelineDescription *) _b;
    Uint32 i;

    if ( (!SDL_KeyMatchString(a->label, b->label, NULL)) ||
         (a->primitive != b->primitive) ||
         (a->vertex_shader != b->vertex_shader) ||
         (a->fragment_shader != b->fragment_shader) ||
         (a->num_vertex_attributes != b->num_vertex_attributes) ||
         (a->num_color_attachments != b->num_color_attachments) ||
         (a->depth_format != b->depth_format) ||
         (a->stencil_format != b->stencil_format) ||
         (a->depth_write_enabled != b->depth_write_enabled) ||
         (a->stencil_read_mask != b->stencil_read_mask) ||
         (a->stencil_write_mask != b->stencil_write_mask) ||
         (a->stencil_reference_front != b->stencil_reference_front) ||
         (a->stencil_reference_back != b->stencil_reference_back) ||
         (a->depth_function != b->depth_function) ||
         (a->stencil_function != b->stencil_function) ||
         (a->stencil_fail != b->stencil_fail) ||
         (a->depth_fail != b->depth_fail) ||
         (a->depth_and_stencil_pass != b->depth_and_stencil_pass) ||
         (a->fill_mode != b->fill_mode) ||
         (a->front_face != b->front_face) ||
         (a->cull_face != b->cull_face) ||
         (a->depth_bias != b->depth_bias) ||
         (a->depth_bias_scale != b->depth_bias_scale) ||
         (a->depth_bias_clamp != b->depth_bias_clamp) ) {
        return SDL_FALSE;
    }

    /* still here? Compare the arrays */
    for (i = 0; i < a->num_vertex_attributes; i++) {
        const SDL_GpuVertexAttributeDescription *av = &a->vertices[i];
        const SDL_GpuVertexAttributeDescription *bv = &b->vertices[i];
        if ( (av->format != bv->format) ||
             (av->offset != bv->offset) ||
             (av->stride != bv->stride) ||
             (av->index != bv->index) ) {
            return SDL_FALSE;
        }
    }

    for (i = 0; i < a->num_color_attachments; i++) {
        const SDL_GpuPipelineColorAttachmentDescription *ac = &a->color_attachments[i];
        const SDL_GpuPipelineColorAttachmentDescription *bc = &b->color_attachments[i];
        if ( (ac->pixel_format != bc->pixel_format) ||
             (ac->writemask_enabled_red != bc->writemask_enabled_red) ||
             (ac->writemask_enabled_blue != bc->writemask_enabled_blue) ||
             (ac->writemask_enabled_green != bc->writemask_enabled_green) ||
             (ac->writemask_enabled_alpha != bc->writemask_enabled_alpha) ||
             (ac->blending_enabled != bc->blending_enabled) ||
             (ac->alpha_blend_op != bc->alpha_blend_op) ||
             (ac->alpha_src_blend_factor != bc->alpha_src_blend_factor) ||
             (ac->alpha_dst_blend_factor != bc->alpha_dst_blend_factor) ||
             (ac->rgb_blend_op != bc->rgb_blend_op) ||
             (ac->rgb_src_blend_factor != bc->rgb_src_blend_factor) ||
             (ac->rgb_dst_blend_factor != bc->rgb_dst_blend_factor) ) {
            return SDL_FALSE;
        }
    }

    return SDL_TRUE;
}

void nuke_pipeline(const void *key, const void *value, void *data)
{
    SDL_GpuPipelineDescription *desc = (SDL_GpuPipelineDescription *) key;
    SDL_free((void *) desc->label);
    SDL_free(desc);
    SDL_GpuDestroyPipeline((SDL_GpuPipeline *) value);
}


static Uint32 hash_sampler(const void *key, void *data)
{
    /* this hashes most pointers; this hash is meant to be unique and contained in this process. As such, it also doesn't care about enum size or byte order. */
    /* However, it _does_ care about uninitialized packing bytes, so it doesn't just hash the sizeof (object). */
    const SDL_GpuSamplerDescription *desc = (const SDL_GpuSamplerDescription *) key;
    Uint32 crc = CRC32_INIT_VALUE;

    if (desc->label) { crc = crc32_append(crc, desc->label, SDL_strlen(desc->label)); }  /* NULL means less bytes hashed to keep it unique vs "". */
    CRC32_APPEND_VAR(crc, desc->addrmode_u);
    CRC32_APPEND_VAR(crc, desc->addrmode_v);
    CRC32_APPEND_VAR(crc, desc->addrmode_r);
    CRC32_APPEND_VAR(crc, desc->border_color);
    CRC32_APPEND_VAR(crc, desc->min_filter);
    CRC32_APPEND_VAR(crc, desc->mag_filter);
    CRC32_APPEND_VAR(crc, desc->mip_filter);
    return CRC32_FINISH(crc);
}

static SDL_bool keymatch_sampler(const void *_a, const void *_b, void *data)
{
    const SDL_GpuSamplerDescription *a = (const SDL_GpuSamplerDescription *) _a;
    const SDL_GpuSamplerDescription *b = (const SDL_GpuSamplerDescription *) _b;
    return ( (SDL_KeyMatchString(a->label, b->label, NULL)) &&
             (a->addrmode_u == b->addrmode_u) &&
             (a->addrmode_v == b->addrmode_v) &&
             (a->addrmode_r == b->addrmode_r) &&
             (a->min_filter == b->min_filter) &&
             (a->mag_filter == b->mag_filter) &&
             (a->mip_filter == b->mip_filter) ) ? SDL_TRUE : SDL_FALSE;
}

void nuke_sampler(const void *key, const void *value, void *data)
{
    SDL_GpuSamplerDescription *desc = (SDL_GpuSamplerDescription *) key;
    SDL_free((void *) desc->label);
    SDL_free(desc);
    SDL_GpuDestroySampler((SDL_GpuSampler *) value);
}

SDL_GpuStateCache *
SDL_GpuCreateStateCache(const char *label, SDL_GpuDevice *device)
{
    SDL_GpuStateCache *cache = (SDL_GpuStateCache *) SDL_calloc(1, sizeof (SDL_GpuStateCache));
    if (!cache) {
        SDL_OutOfMemory();
        return NULL;
    }

    cache->pipeline_mutex = SDL_CreateMutex();
    if (!cache->pipeline_mutex) {
        goto failed;
    }

    cache->sampler_mutex = SDL_CreateMutex();
    if (!cache->sampler_mutex) {
        goto failed;
    }

    if (label) {
        cache->label = SDL_strdup(label);
        if (!cache->label) {
            SDL_OutOfMemory();
            goto failed;
        }
    }

    /* !!! FIXME: adjust hash table bucket counts? */

    cache->pipeline_cache = SDL_NewHashTable(NULL, 128, hash_pipeline, keymatch_pipeline, nuke_pipeline, SDL_FALSE);
    if (!cache->pipeline_cache) {
        goto failed;
    }

    cache->sampler_cache = SDL_NewHashTable(NULL, 16, hash_sampler, keymatch_sampler, nuke_sampler, SDL_FALSE);
    if (!cache->sampler_cache) {
        goto failed;
    }

    cache->device = device;
    return cache;

failed:
    SDL_GpuDestroyStateCache(cache);  /* can clean up half-created objects. */
    return NULL;
}

#define GETCACHEDOBJIMPL(ctyp, typ) \
    SDL_Gpu##ctyp *retval; \
    const void *val; \
    \
    if (!cache) { \
        SDL_InvalidParamError("cache"); \
        return NULL; \
    } \
    \
    SDL_LockMutex(cache->typ##_mutex); \
    if (SDL_FindInHashTable(cache->typ##_cache, desc, &val)) { \
        retval = (SDL_Gpu##ctyp *) val; \
    } else {  /* not cached yet, make a new one and cache it. */ \
        retval = SDL_GpuCreate##ctyp(cache->device, desc); \
        if (retval) { \
            if (!SDL_InsertIntoHashTable(cache->typ##_cache, &retval->desc, retval)) { \
                SDL_GpuDestroy##ctyp(retval); \
                retval = NULL; \
            } \
        } \
    } \
    SDL_UnlockMutex(cache->typ##_mutex); \
    return retval

SDL_GpuPipeline *
SDL_GpuGetCachedPipeline(SDL_GpuStateCache *cache, const SDL_GpuPipelineDescription *desc)
{
    GETCACHEDOBJIMPL(Pipeline, pipeline);
}

SDL_GpuSampler *
SDL_GpuGetCachedSampler(SDL_GpuStateCache *cache, const SDL_GpuSamplerDescription *desc)
{
    GETCACHEDOBJIMPL(Sampler, sampler);
}

void
SDL_GpuDestroyStateCache(SDL_GpuStateCache *cache)
{
    if (cache) {
        SDL_DestroyMutex(cache->pipeline_mutex);
        SDL_FreeHashTable(cache->pipeline_cache);
        SDL_DestroyMutex(cache->sampler_mutex);
        SDL_FreeHashTable(cache->sampler_cache);
        SDL_free((void *) cache->label);
        SDL_free(cache);
    }
}

SDL_GpuCommandBuffer *
SDL_GpuCreateCommandBuffer(const char *label, SDL_GpuDevice *device)
{
}


SDL_GpuRenderPass *
SDL_GpuStartRenderPass(const char *label, SDL_GpuCommandBuffer *cmdbuf,
                       Uint32 num_color_attachments,
                       const SDL_GpuColorAttachmentDescription *color_attachments,
                       const SDL_GpuDepthAttachmentDescription *depth_attachment,
                       const SDL_GpuStencilAttachmentDescription *stencil_attachment)
{
}


void
SDL_GpuSetRenderPassPipeline(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline)
{
}

void
SDL_GpuSetRenderPassViewport(SDL_GpuRenderPass *pass, const double x, const double y, const double width, const double height, const double znear, const double zfar)
{
}

void
SDL_GpuSetRenderPassScissor(SDL_GpuRenderPass *pass, const Uint32 x, const Uint32 y, const Uint32 width, const Uint32 height)
{
}

void
SDL_GpuSetRenderBlendConstant(SDL_GpuRenderPass *pass, const double red, const double green, const double blue, const double alpha)
{
}

void
SDL_GpuSetRenderPassVertexBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, const Uint32 offset, const Uint32 index)
{
}

void
SDL_GpuSetRenderPassVertexSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, const Uint32 index)
{
}

void
SDL_GpuSetRenderPassVertexTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, const Uint32 index)
{
}

void
SDL_GpuSetRenderPassFragmentBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, const Uint32 offset, const Uint32 index)
{
}

void
SDL_GpuSetRenderPassFragmentSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, const Uint32 index)
{
}

void
SDL_GpuSetRenderPassFragmentTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, const Uint32 index)
{
}

void
SDL_GpuDraw(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count)
{
}

void
SDL_GpuDrawIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset)
{
}

void
SDL_GpuDrawInstanced(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count, Uint32 instance_count, Uint32 base_instance)
{
}

void
SDL_GpuDrawInstancedIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset, Uint32 instance_count, Uint32 base_instance)
{
}


void
SDL_GpuEndRenderPass(SDL_GpuRenderPass *pass)
{
}


SDL_GpuBlitPass *
SDL_GpuStartBlitPass(const char *label, SDL_GpuCommandBuffer *cmdbuf)
{
}

void
SDL_GpuCopyBetweenTextures(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                                 Uint32 srcx, Uint32 srcy, Uint32 srcz,
                                 Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                 SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                                 Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
}

void
SDL_GpuFillBuffer(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, unsigned char value)
{
}

void
SDL_GpuGenerateMipmaps(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture)
{
}

void
SDL_GpuCopyBufferCpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuCpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
}

void
SDL_GpuCopyBufferGpuToCpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuCpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
}

void
SDL_GpuCopyFromBufferToTexture(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset,
                                     Uint32 srcpitch, Uint32 srcimgpitch,
                                     Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                     SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                                     Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
}

void
SDL_GpuCopyFromTextureToBuffer(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                                     Uint32 srcx, Uint32 srcy, Uint32 srcz,
                                     Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                                     SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch)
{
}

void
SDL_GpuEndBlitPass(SDL_GpuBlitPass *pass)
{
}

SDL_GpuFence *
SDL_GpuCreateFence(const char *label, SDL_GpuDevice *device)
{
}

void
SDL_GpuDestroyFence(SDL_GpuFence *fence)
{
}

int
SDL_GpuQueryFence(SDL_GpuFence *fence)
{
}

int
SDL_GpuResetFence(SDL_GpuFence *fence)
{
}

int
SDL_GpuWaitFence(SDL_GpuFence *fence)
{
}

void
SDL_GpuSubmitCommandBuffers(SDL_GpuCommandBuffer **buffers, const Uint32 numcmdbufs, SDL_GpuPresentType presenttype, SDL_GpuFence *fence)
{
}

void
SDL_GpuAbandonCommandBuffers(SDL_GpuCommandBuffer **buffers, const Uint32 numcmdbufs)
{
}

/* vi: set ts=4 sw=4 expandtab: */
