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

extern const SDL_GpuDriver DUMMY_GpuDriver;

static const SDL_GpuDriver *gpu_drivers[] = {
    &DUMMY_GpuDriver
};

Uint32
SDL_GpuGetNumDrivers(void)
{
    return (Uint32) SDL_arraysize(gpu_drivers);
}

const char *
SDL_GpuGetDriverName(Uint32 index)
{
    const Uint32 numdrivers = (Uint32) SDL_arraysize(gpu_drivers);
    if (index >= numdrivers) {
        SDL_SetError("index must be in the range of 0 - %u", (unsigned int) (numdrivers ? (numdrivers - 1) : 0));
        return NULL;
    }
    return gpu_drivers[index]->name;
}

/* helper function since lots of things need an object and a label allocated. */
static void *allocate_obj_and_string(const size_t objlen, const char *str, char **allocatedstr)
{
    void *retval;

    SDL_assert(str != NULL);
    SDL_assert(allocatedstr != NULL);
    SDL_assert(objlen > 0);

    *allocatedstr = NULL;
    retval = SDL_calloc(1, objlen);
    if (!retval) {
        SDL_OutOfMemory();
        return NULL;
    }

    if (str) {
        *allocatedstr = SDL_strdup(str);
        if (!*allocatedstr) {
            SDL_free(retval);
            SDL_OutOfMemory();
            return NULL;
        }
    }

    return retval;
}

#define ALLOC_OBJ_WITH_LABEL(typ, var, str) { \
    char *cpystr; \
    var = (typ *) allocate_obj_and_string(sizeof (typ), str, &cpystr); \
    if (var != NULL) { \
        var->label = cpystr; \
    } \
}

#define FREE_AND_NULL_OBJ_WITH_LABEL(obj) { \
    SDL_free((void *) obj->label); \
    SDL_free(obj); \
    obj = NULL; \
}

#define ALLOC_OBJ_WITH_DESC(typ, var, dsc) { \
    char *cpystr; \
    var = (typ *) allocate_obj_and_string(sizeof (typ), dsc->label, &cpystr); \
    if (var != NULL) { \
        SDL_memcpy(&var->desc, dsc, sizeof (*dsc));\
        var->desc.label = cpystr; \
    } \
}

#define FREE_AND_NULL_OBJ_WITH_DESC(obj) { \
    SDL_free((void *) obj->desc.label); \
    SDL_free(obj); \
    obj = NULL; \
}


/* !!! FIXME: change this API to allow selection of a specific GPU? */
static int
GpuCreateDeviceInternal(SDL_GpuDevice *device, const char *driver)
{
    size_t i;

    if (driver) {  /* if a specific driver requested, succeed or fail without trying others. */
        for (i = 0; i < SDL_arraysize(gpu_drivers); i++) {
            const SDL_GpuDriver *thisdriver = gpu_drivers[i];
            if (SDL_strcasecmp(driver, thisdriver->name) == 0) {
                return thisdriver->CreateDevice(device);
            }
        }
        return SDL_SetError("GPU driver '%s' not found", driver);  /* possibly misnamed, possibly not built in */
    }


    /* !!! FIXME: add a hint to SDL_hints.h later, but that will make merging later harder if done now. */
    driver = SDL_GetHint(/*SDL_HINT_GPU_DRIVER*/ "SDL_GPU_DRIVER");
    if (driver) {
        for (i = 0; i < SDL_arraysize(gpu_drivers); i++) {
            const SDL_GpuDriver *thisdriver = gpu_drivers[i];
            if (SDL_strcasecmp(driver, thisdriver->name) == 0) {
                if (thisdriver->CreateDevice(device) == 0) {
                    return 0;
                }
            }
        }
    }

    /* Still here? Take the first one that works. */
    for (i = 0; i < SDL_arraysize(gpu_drivers); i++) {
        const SDL_GpuDriver *thisdriver = gpu_drivers[i];
        if (!driver || (SDL_strcasecmp(driver, thisdriver->name) != 0)) {
            if (thisdriver->CreateDevice(device) == 0) {
                return 0;
            }
        }
    }

    return SDL_SetError("Couldn't find an available GPU driver");
}

SDL_GpuDevice *
SDL_GpuCreateDevice(const char *label, const char *driver)
{
    SDL_GpuDevice *device;
    ALLOC_OBJ_WITH_LABEL(SDL_GpuDevice, device, label);

    if (device != NULL) {
        if (GpuCreateDeviceInternal(device, driver) == -1) {
            FREE_AND_NULL_OBJ_WITH_LABEL(device);
        }
    }
    return device;
}

void
SDL_GpuDestroyDevice(SDL_GpuDevice *device)
{
    if (device) {
        device->DestroyDevice(device);
        FREE_AND_NULL_OBJ_WITH_LABEL(device);
    }
}

SDL_GpuCpuBuffer *
SDL_GpuCreateCpuBuffer(const char *label, SDL_GpuDevice *device, const Uint32 buflen, const void *data)
{
    SDL_GpuCpuBuffer *buffer = NULL;
    if (!device) {
        SDL_InvalidParamError("device");
    } else if (buflen == 0) {
        SDL_InvalidParamError("buflen");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuCpuBuffer, buffer, label);
        if (buffer != NULL) {
            buffer->device = device;
            buffer->buflen = buflen;
            if (device->CreateCpuBuffer(buffer, data) == -1) {
                FREE_AND_NULL_OBJ_WITH_LABEL(buffer);
            }
        }
    }
    return buffer;
}

void
SDL_GpuDestroyCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    if (buffer) {
        buffer->device->DestroyCpuBuffer(buffer);
        FREE_AND_NULL_OBJ_WITH_LABEL(buffer);
    }
}

void *
SDL_GpuLockCpuBuffer(SDL_GpuCpuBuffer *buffer, Uint32 *_buflen)
{
    void *retval = NULL;
    if (!buffer) {
        SDL_InvalidParamError("buffer");
    } else {
        retval = buffer->device->LockCpuBuffer(buffer);
    }

    if (_buflen) {
        *_buflen = retval ? buffer->buflen : 0;
    }
    return retval;
}

int
SDL_GpuUnlockCpuBuffer(SDL_GpuCpuBuffer *buffer)
{
    if (!buffer) {
        return SDL_InvalidParamError("buffer");
    }
    return buffer->device->UnlockCpuBuffer(buffer);
}

SDL_GpuBuffer *
SDL_GpuCreateBuffer(const char *label, SDL_GpuDevice *device, const Uint32 buflen)
{
    SDL_GpuBuffer *buffer = NULL;

    if (!device) {
        SDL_InvalidParamError("device");
    } else if (buflen == 0) {
        SDL_InvalidParamError("buflen");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuBuffer, buffer, label);
        if (buffer != NULL) {
            buffer->device = device;
            buffer->buflen = buflen;
            if (device->CreateBuffer(buffer) == -1) {
                FREE_AND_NULL_OBJ_WITH_LABEL(buffer);
            }
        }
    }
    return buffer;
}

void
SDL_GpuDestroyBuffer(SDL_GpuBuffer *buffer)
{
    if (buffer) {
        buffer->device->DestroyBuffer(buffer);
        FREE_AND_NULL_OBJ_WITH_LABEL(buffer);
    }
}

SDL_GpuTexture *
SDL_GpuCreateTexture(SDL_GpuDevice *device, const SDL_GpuTextureDescription *desc)
{
    SDL_GpuTexture *texture = NULL;

    if (!device) {
        SDL_InvalidParamError("device");
    } else if (!desc) {
        SDL_InvalidParamError("desc");
    } else {
        ALLOC_OBJ_WITH_DESC(SDL_GpuTexture, texture, desc);
        if (texture != NULL) {
            texture->device = device;
            if (device->CreateTexture(texture) == -1) {
                FREE_AND_NULL_OBJ_WITH_DESC(texture);
            }
        }
    }
    return texture;
}

int
SDL_GpuGetTextureDescription(SDL_GpuTexture *texture, SDL_GpuTextureDescription *desc)
{
    if (!texture) {
        return SDL_InvalidParamError("pipeline");
    } else if (!desc) {
        return SDL_InvalidParamError("desc");
    }
    SDL_memcpy(desc, &texture->desc, sizeof (*desc));
    return 0;
}

void
SDL_GpuDestroyTexture(SDL_GpuTexture *texture)
{
    if (texture) {
        texture->device->DestroyTexture(texture);
        FREE_AND_NULL_OBJ_WITH_DESC(texture);
    }
}

SDL_GpuShader *
SDL_GpuCreateShader(const char *label, SDL_GpuDevice *device, const Uint8 *bytecode, const Uint32 bytecodelen)
{
    SDL_GpuShader *shader = NULL;

    if (!device) {
        SDL_InvalidParamError("device");
    } else if (!bytecode) {
        SDL_InvalidParamError("bytecode");
    } else if (bytecodelen == 0) {
        SDL_InvalidParamError("bytecodelen");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuShader, shader, label);
        if (shader != NULL) {
            shader->device = device;
            SDL_AtomicSet(&shader->refcount, 1);
            if (device->CreateShader(shader, bytecode, bytecodelen) == -1) {
                FREE_AND_NULL_OBJ_WITH_LABEL(shader);
            }
        }
    }
    return shader;
}

void
SDL_GpuDestroyShader(SDL_GpuShader *shader)
{
    if (shader) {
        if (SDL_AtomicDecRef(&shader->refcount)) {
            shader->device->DestroyShader(shader);
            FREE_AND_NULL_OBJ_WITH_LABEL(shader);
        }
    }
}

SDL_GpuTexture *
SDL_GpuGetBackbuffer(SDL_GpuDevice *device, SDL_Window *window)
{
    if (!device) {
        SDL_InvalidParamError("device");
        return NULL;
    } else if (!window) {
        SDL_InvalidParamError("window");
        return NULL;
    }
    return device->GetBackbuffer(device, window);
}

SDL_GpuPipeline *
SDL_GpuCreatePipeline(SDL_GpuDevice *device, const SDL_GpuPipelineDescription *desc)
{
    SDL_GpuPipeline *pipeline = NULL;
    if (!device) {
        SDL_InvalidParamError("device");
    } else if (!desc) {
        SDL_InvalidParamError("desc");
    } else if (desc->vertex_shader && (desc->vertex_shader->device != device)) {
        SDL_SetError("vertex shader is not from this device");
    } else if (desc->fragment_shader && (desc->fragment_shader->device != device)) {
        SDL_SetError("fragment shader is not from this device");
    } else {
        ALLOC_OBJ_WITH_DESC(SDL_GpuPipeline, pipeline, desc);
        if (pipeline != NULL) {
            pipeline->device = device;
            if (device->CreatePipeline(pipeline) == -1) {
                FREE_AND_NULL_OBJ_WITH_DESC(pipeline);
            } else {
                if (pipeline->desc.vertex_shader) {
                    SDL_AtomicIncRef(&pipeline->desc.vertex_shader->refcount);
                }
                if (pipeline->desc.fragment_shader) {
                    SDL_AtomicIncRef(&pipeline->desc.fragment_shader->refcount);
                }
            }
        }
    }
    return pipeline;
}

void
SDL_GpuDestroyPipeline(SDL_GpuPipeline *pipeline)
{
    if (pipeline) {
        SDL_GpuShader *vshader = pipeline->desc.vertex_shader;
        SDL_GpuShader *fshader = pipeline->desc.fragment_shader;

        pipeline->device->DestroyPipeline(pipeline);
        FREE_AND_NULL_OBJ_WITH_DESC(pipeline);

        /* decrement reference counts (and possibly destroy) the shaders. */
        SDL_GpuDestroyShader(vshader);
        SDL_GpuDestroyShader(fshader);
    }
}

void
SDL_GpuDefaultPipelineDescription(SDL_GpuPipelineDescription *desc)
{
    /* !!! FIXME: decide if these are reasonable defaults. */
    SDL_zerop(desc);
    desc->primitive = SDL_GPUPRIM_TRIANGLESTRIP;
    desc->num_vertex_attributes = 1;
    desc->vertices[0].format = SDL_GPUVERTFMT_FLOAT4;
    desc->num_color_attachments = 1;
    desc->color_attachments[0].pixel_format = SDL_GPUPIXELFMT_RGBA8;
    desc->color_attachments[0].writemask_enabled_red = SDL_TRUE;
    desc->color_attachments[0].writemask_enabled_blue = SDL_TRUE;
    desc->color_attachments[0].writemask_enabled_green = SDL_TRUE;
    desc->color_attachments[0].writemask_enabled_alpha = SDL_TRUE;
    desc->depth_format = SDL_GPUPIXELFMT_Depth24_Stencil8;
    desc->stencil_format = SDL_GPUPIXELFMT_Depth24_Stencil8;
    desc->depth_write_enabled = SDL_TRUE;
    desc->stencil_read_mask = 0xFFFFFFFF;
    desc->stencil_write_mask = 0xFFFFFFFF;
    desc->depth_function = SDL_GPUCMPFUNC_LESS;
    desc->stencil_function = SDL_GPUCMPFUNC_ALWAYS;
    desc->stencil_fail = SDL_GPUSTENCILOP_KEEP;
    desc->depth_fail = SDL_GPUSTENCILOP_KEEP;
    desc->depth_and_stencil_pass = SDL_GPUSTENCILOP_KEEP;
    desc->fill_mode = SDL_GPUFILL_FILL;
    desc->front_face = SDL_GPUFRONTFACE_COUNTER_CLOCKWISE;
    desc->cull_face = SDL_GPUCULLFACE_BACK;
}

int
SDL_GpuGetPipelineDescription(SDL_GpuPipeline *pipeline, SDL_GpuPipelineDescription *desc)
{
    if (!pipeline) {
        return SDL_InvalidParamError("pipeline");
    } else if (!desc) {
        return SDL_InvalidParamError("desc");
    }
    SDL_memcpy(desc, &pipeline->desc, sizeof (*desc));
    return 0;
}

SDL_GpuSampler *
SDL_GpuCreateSampler(SDL_GpuDevice *device, const SDL_GpuSamplerDescription *desc)
{
    SDL_GpuSampler *sampler = NULL;
    if (!device) {
        SDL_InvalidParamError("device");
    } else if (!desc) {
        SDL_InvalidParamError("desc");
    } else {
        ALLOC_OBJ_WITH_DESC(SDL_GpuSampler, sampler, desc);
        if (sampler != NULL) {
            sampler->device = device;
            if (device->CreateSampler(sampler) == -1) {
                FREE_AND_NULL_OBJ_WITH_DESC(sampler);
            }
        }
    }
    return sampler;
}

void
SDL_GpuDestroySampler(SDL_GpuSampler *sampler)
{
    if (sampler) {
        sampler->device->DestroySampler(sampler);
        FREE_AND_NULL_OBJ_WITH_DESC(sampler);
    }
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
    SDL_GpuStateCache *cache = NULL;
    if (!device) {
        SDL_InvalidParamError("device");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuStateCache, cache, label);
        if (cache != NULL) {
            /* !!! FIXME: adjust hash table bucket counts? */
            cache->device = device;
            cache->pipeline_mutex = SDL_CreateMutex();
            cache->sampler_mutex = SDL_CreateMutex();
            cache->pipeline_cache = SDL_NewHashTable(NULL, 128, hash_pipeline, keymatch_pipeline, nuke_pipeline, SDL_FALSE);
            cache->sampler_cache = SDL_NewHashTable(NULL, 16, hash_sampler, keymatch_sampler, nuke_sampler, SDL_FALSE);
            if (!cache->pipeline_mutex || !cache->sampler_mutex || !cache->pipeline_cache || !cache->sampler_cache) {
                SDL_GpuDestroyStateCache(cache);  /* can clean up half-created objects. */
                cache = NULL;
            }
        }
    }

    return cache;
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
        FREE_AND_NULL_OBJ_WITH_LABEL(cache);
    }
}

SDL_GpuCommandBuffer *
SDL_GpuCreateCommandBuffer(const char *label, SDL_GpuDevice *device)
{
    SDL_GpuCommandBuffer *cmdbuf = NULL;
    if (!device) {
        SDL_InvalidParamError("device");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuCommandBuffer, cmdbuf, label);
        if (cmdbuf != NULL) {
            cmdbuf->device = device;
            if (device->CreateCommandBuffer(cmdbuf) == -1) {
                FREE_AND_NULL_OBJ_WITH_LABEL(cmdbuf);
            }
        }
    }
    return cmdbuf;
}

SDL_GpuRenderPass *
SDL_GpuStartRenderPass(const char *label, SDL_GpuCommandBuffer *cmdbuf,
                       Uint32 num_color_attachments,
                       const SDL_GpuColorAttachmentDescription *color_attachments,
                       const SDL_GpuDepthAttachmentDescription *depth_attachment,
                       const SDL_GpuStencilAttachmentDescription *stencil_attachment)
{
    SDL_GpuRenderPass *pass = NULL;
    if (!cmdbuf) {
        SDL_InvalidParamError("cmdbuf");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuRenderPass, pass, label);
        if (pass != NULL) {
            pass->device = cmdbuf->device;
            pass->cmdbuf = cmdbuf;
            if (pass->device->StartRenderPass(pass, num_color_attachments, color_attachments, depth_attachment, stencil_attachment) == -1) {
                FREE_AND_NULL_OBJ_WITH_LABEL(pass);
            }
        }
    }
    return pass;
}

int
SDL_GpuSetRenderPassPipeline(SDL_GpuRenderPass *pass, SDL_GpuPipeline *pipeline)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    }
    /* !!! FIXME: can we set a NULL pipeline? */
    return pass->device->SetRenderPassPipeline(pass, pipeline);
}

int
SDL_GpuSetRenderPassViewport(SDL_GpuRenderPass *pass, const double x, const double y, const double width, const double height, const double znear, const double zfar)
{
    return pass ? pass->device->SetRenderPassViewport(pass, x, y, width, height, znear, zfar) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassScissor(SDL_GpuRenderPass *pass, const Uint32 x, const Uint32 y, const Uint32 width, const Uint32 height)
{
    return pass ? pass->device->SetRenderPassScissor(pass, x, y, width, height) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassBlendConstant(SDL_GpuRenderPass *pass, const double red, const double green, const double blue, const double alpha)
{
    return pass ? pass->device->SetRenderPassBlendConstant(pass, red, green, blue, alpha) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassVertexBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, const Uint32 offset, const Uint32 index)
{
    return pass ? pass->device->SetRenderPassVertexBuffer(pass, buffer, offset, index) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassVertexSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, const Uint32 index)
{
    return pass ? pass->device->SetRenderPassVertexSampler(pass, sampler, index) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassVertexTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, const Uint32 index)
{
    return pass ? pass->device->SetRenderPassVertexTexture(pass, texture, index) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassFragmentBuffer(SDL_GpuRenderPass *pass, SDL_GpuBuffer *buffer, const Uint32 offset, const Uint32 index)
{
    return pass ? pass->device->SetRenderPassFragmentBuffer(pass, buffer, offset, index) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassFragmentSampler(SDL_GpuRenderPass *pass, SDL_GpuSampler *sampler, const Uint32 index)
{
    return pass ? pass->device->SetRenderPassFragmentSampler(pass, sampler, index) : SDL_InvalidParamError("pass");
}

int
SDL_GpuSetRenderPassFragmentTexture(SDL_GpuRenderPass *pass, SDL_GpuTexture *texture, const Uint32 index)
{
    return pass ? pass->device->SetRenderPassFragmentTexture(pass, texture, index) : SDL_InvalidParamError("pass");
}

int
SDL_GpuDraw(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count)
{
    return pass ? pass->device->Draw(pass, vertex_start, vertex_count) : SDL_InvalidParamError("pass");
}

int
SDL_GpuDrawIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset)
{
    return pass ? pass->device->DrawIndexed(pass, index_count, index_type, index_buffer, index_offset) : SDL_InvalidParamError("pass");
}

int
SDL_GpuDrawInstanced(SDL_GpuRenderPass *pass, Uint32 vertex_start, Uint32 vertex_count, Uint32 instance_count, Uint32 base_instance)
{
    return pass ? pass->device->DrawInstanced(pass, vertex_start, vertex_count, instance_count, base_instance) : SDL_InvalidParamError("pass");
}

int
SDL_GpuDrawInstancedIndexed(SDL_GpuRenderPass *pass, Uint32 index_count, SDL_GpuIndexType index_type, SDL_GpuBuffer *index_buffer, Uint32 index_offset, Uint32 instance_count, Uint32 base_instance)
{
    return pass ? pass->device->DrawInstancedIndexed(pass, index_count, index_type, index_buffer, index_offset, instance_count, base_instance) : SDL_InvalidParamError("pass");
}

int
SDL_GpuEndRenderPass(SDL_GpuRenderPass *pass)
{
    int retval;
    if (!pass) {
        return SDL_InvalidParamError("pass");
    }

    retval = pass->device->EndRenderPass(pass);
    if (retval == 0) {
        FREE_AND_NULL_OBJ_WITH_LABEL(pass);
    }
    return retval;
}


SDL_GpuBlitPass *
SDL_GpuStartBlitPass(const char *label, SDL_GpuCommandBuffer *cmdbuf)
{
    SDL_GpuBlitPass *pass = NULL;
    if (!cmdbuf) {
        SDL_InvalidParamError("cmdbuf");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuBlitPass, pass, label);
        if (pass != NULL) {
            pass->device = cmdbuf->device;
            pass->cmdbuf = cmdbuf;
            if (pass->device->StartBlitPass(pass) == -1) {
                FREE_AND_NULL_OBJ_WITH_LABEL(pass);
            }
        }
    }
    return pass;
}

int
SDL_GpuCopyBetweenTextures(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                           Uint32 srcx, Uint32 srcy, Uint32 srcz,
                           Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                           SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                           Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!srctex) {
        return SDL_InvalidParamError("srctex");
    } else if (!dsttex) {
        return SDL_InvalidParamError("dsttex");
    }
    /* !!! FIXME: check levels, slices, etc. */
    return pass->device->CopyBetweenTextures(pass, srctex, srcslice, srclevel, srcx, srcy, srcz, srcw, srch, srcdepth, dsttex, dstslice, dstlevel, dstx, dsty, dstz);
}

int
SDL_GpuFillBuffer(SDL_GpuBlitPass *pass, SDL_GpuBuffer *buffer, Uint32 offset, Uint32 length, Uint8 value)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!buffer) {
        return SDL_InvalidParamError("buffer");
    } else if ((offset+length) > buffer->buflen) {
        return SDL_SetError("offset+length overflows the buffer");  /* !!! FIXME: should we clamp instead so you can fully initialize without knowing the size? */
    }
    return pass ? pass->device->FillBuffer(pass, buffer, offset, length, value) : SDL_InvalidParamError("pass");
}

int
SDL_GpuGenerateMipmaps(SDL_GpuBlitPass *pass, SDL_GpuTexture *texture)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!texture) {
        return SDL_InvalidParamError("texture");
    }
    return pass->device->GenerateMipmaps(pass, texture);
}

int
SDL_GpuCopyBufferCpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuCpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!srcbuf) {
        return SDL_InvalidParamError("srcbuf");
    } else if (!dstbuf) {
        return SDL_InvalidParamError("dstbuf");
    } else if ((srcoffset+length) > srcbuf->buflen) {
        return SDL_SetError("srcoffset+length overflows the source buffer");
    } else if ((dstoffset+length) > dstbuf->buflen) {
        return SDL_SetError("dstoffset+length overflows the destination buffer");
    }
    return pass->device->CopyBufferCpuToGpu(pass, srcbuf, srcoffset, dstbuf, dstoffset, length);
}

int
SDL_GpuCopyBufferGpuToCpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuCpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!srcbuf) {
        return SDL_InvalidParamError("srcbuf");
    } else if (!dstbuf) {
        return SDL_InvalidParamError("dstbuf");
    } else if ((srcoffset+length) > srcbuf->buflen) {
        return SDL_SetError("srcoffset+length overflows the source buffer");
    } else if ((dstoffset+length) > dstbuf->buflen) {
        return SDL_SetError("dstoffset+length overflows the destination buffer");
    }
    return pass->device->CopyBufferGpuToCpu(pass, srcbuf, srcoffset, dstbuf, dstoffset, length);
}

int
SDL_GpuCopyBufferGpuToGpu(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset, SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 length)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!srcbuf) {
        return SDL_InvalidParamError("srcbuf");
    } else if (!dstbuf) {
        return SDL_InvalidParamError("dstbuf");
    } else if ((srcoffset+length) > srcbuf->buflen) {
        return SDL_SetError("srcoffset+length overflows the source buffer");
    } else if ((dstoffset+length) > dstbuf->buflen) {
        return SDL_SetError("dstoffset+length overflows the destination buffer");
    }
    return pass->device->CopyBufferGpuToGpu(pass, srcbuf, srcoffset, dstbuf, dstoffset, length);
}

int
SDL_GpuCopyFromBufferToTexture(SDL_GpuBlitPass *pass, SDL_GpuBuffer *srcbuf, Uint32 srcoffset,
                               Uint32 srcpitch, Uint32 srcimgpitch,
                               Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                               SDL_GpuTexture *dsttex, Uint32 dstslice, Uint32 dstlevel,
                               Uint32 dstx, Uint32 dsty, Uint32 dstz)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!srcbuf) {
        return SDL_InvalidParamError("srcbuf");
    } else if (!dsttex) {
        return SDL_InvalidParamError("dsttex");
    }
    /* !!! FIXME: check other param ranges */
    return pass->device->CopyFromBufferToTexture(pass, srcbuf, srcoffset, srcpitch, srcimgpitch, srcw, srch, srcdepth, dsttex, dstslice, dstlevel, dstx, dsty, dstz);
}

int
SDL_GpuCopyFromTextureToBuffer(SDL_GpuBlitPass *pass, SDL_GpuTexture *srctex, Uint32 srcslice, Uint32 srclevel,
                               Uint32 srcx, Uint32 srcy, Uint32 srcz,
                               Uint32 srcw, Uint32 srch, Uint32 srcdepth,
                               SDL_GpuBuffer *dstbuf, Uint32 dstoffset, Uint32 dstpitch, Uint32 dstimgpitch)
{
    if (!pass) {
        return SDL_InvalidParamError("pass");
    } else if (!srctex) {
        return SDL_InvalidParamError("srctex");
    } else if (!dstbuf) {
        return SDL_InvalidParamError("dstbuf");
    }
    /* !!! FIXME: check other param ranges */
    return pass->device->CopyFromTextureToBuffer(pass, srctex, srcslice, srclevel, srcx, srcy, srcz, srcw, srch, srcdepth, dstbuf, dstoffset, dstpitch, dstimgpitch);
}

int
SDL_GpuEndBlitPass(SDL_GpuBlitPass *pass)
{
    int retval;
    if (!pass) {
        return SDL_InvalidParamError("pass");
    }

    retval = pass->device->EndBlitPass(pass);
    if (retval == 0) {
        FREE_AND_NULL_OBJ_WITH_LABEL(pass);
    }
    return retval;
}

SDL_GpuFence *
SDL_GpuCreateFence(const char *label, SDL_GpuDevice *device)
{
    SDL_GpuFence *fence = NULL;
    if (!device) {
        SDL_InvalidParamError("device");
    } else {
        ALLOC_OBJ_WITH_LABEL(SDL_GpuFence, fence, label);
        if (fence != NULL) {
            fence->device = device;
            if (device->CreateFence(fence) == -1) {
                FREE_AND_NULL_OBJ_WITH_LABEL(fence);
            }
        }
    }
    return fence;
}

void
SDL_GpuDestroyFence(SDL_GpuFence *fence)
{
    if (fence) {
        fence->device->DestroyFence(fence);
        FREE_AND_NULL_OBJ_WITH_LABEL(fence);
    }
}

int
SDL_GpuQueryFence(SDL_GpuFence *fence)
{
    return fence ? fence->device->QueryFence(fence) : SDL_InvalidParamError("fence");
}

int
SDL_GpuResetFence(SDL_GpuFence *fence)
{
    return fence ? fence->device->ResetFence(fence) : SDL_InvalidParamError("fence");
}

int
SDL_GpuWaitFence(SDL_GpuFence *fence)
{
    return fence ? fence->device->WaitFence(fence) : SDL_InvalidParamError("fence");
}

int
SDL_GpuSubmitCommandBuffers(SDL_GpuDevice *device, SDL_GpuCommandBuffer **buffers, const Uint32 numcmdbufs, SDL_GpuPresentType presenttype, SDL_GpuFence *fence)
{
    int retval;
    Uint32 i;

    if (!device) {
        return SDL_InvalidParamError("device");
    } else if (fence && (fence->device != device)) {
        return SDL_SetError("Fence is not from this device");
    }

    for (i = 0; i < numcmdbufs; i++) {
        if (!buffers[i]) {
            return SDL_SetError("Can't submit a NULL command buffer");
        } else if (buffers[i]->device != device) {
            return SDL_SetError("Command buffer is not from this device");
        }
    }

    retval = device->SubmitCommandBuffers(device, buffers, numcmdbufs, presenttype, fence);

    if (retval == 0) {
        for (i = 0; i < numcmdbufs; i++) {
            FREE_AND_NULL_OBJ_WITH_LABEL(buffers[i]);
        }
    }

    return retval;
}

void
SDL_GpuAbandonCommandBuffers(SDL_GpuCommandBuffer **buffers, const Uint32 numcmdbufs)
{
    if (buffers) {
        Uint32 i;
        for (i = 0; i < numcmdbufs; i++) {
            if (buffers[i]) {
                buffers[i]->device->AbandonCommandBuffer(buffers[i]);
                FREE_AND_NULL_OBJ_WITH_LABEL(buffers[i]);
            }
        }
    }
}

SDL_GpuBuffer *SDL_GpuCreateAndInitBuffer(const char *label, SDL_GpuDevice *device, const Uint32 buflen, const void *data)
{
    SDL_GpuFence *fence = NULL;
    SDL_GpuCpuBuffer *staging = NULL;
    SDL_GpuBuffer *gpubuf = NULL;
    SDL_GpuBuffer *retval = NULL;
    SDL_GpuCommandBuffer *cmd = NULL;
    SDL_GpuBlitPass *blit = NULL;

    if (device == NULL) {
        SDL_InvalidParamError("device");
        return NULL;
    } else if (data == NULL) {
        SDL_InvalidParamError("data");
        return NULL;
    }

    if ( ((fence = SDL_GpuCreateFence("Temporary fence for SDL_GpuCreateAndInitBuffer", device)) != NULL) &&
         ((staging = SDL_GpuCreateCpuBuffer("Staging buffer for SDL_GpuCreateAndInitBuffer", device, buflen, data)) != NULL) &&
         ((gpubuf = SDL_GpuCreateBuffer(label, device, buflen)) != NULL) &&
         ((cmd = SDL_GpuCreateCommandBuffer("Command buffer for SDL_GpuCreateAndInitBuffer", device)) != NULL) &&
         ((blit = SDL_GpuStartBlitPass("Blit pass for SDL_GpuCreateAndInitBuffer", cmd)) != NULL) ) {
        SDL_GpuCopyBufferCpuToGpu(blit, staging, 0, gpubuf, 0, buflen);
        SDL_GpuEndBlitPass(blit);
        SDL_GpuSubmitCommandBuffers(device, &cmd, 1, SDL_GPUPRESENT_NONE, fence);
        SDL_GpuWaitFence(fence);  /* so we know it's definitely uploaded */
        retval = gpubuf;
    }

    if (!retval) {
        SDL_GpuEndBlitPass(blit);   /* assume this might be un-ended. */
        SDL_GpuAbandonCommandBuffers(&cmd, 1);
        SDL_GpuDestroyBuffer(gpubuf);
    }
    SDL_GpuDestroyCpuBuffer(staging);
    SDL_GpuDestroyFence(fence);
    return retval;
}

/* !!! FIXME: SDL_GpuCreateAndInitTexture */

SDL_GpuTexture *
SDL_GpuMatchingDepthTexture(const char *label, SDL_GpuDevice *device, SDL_GpuTexture *backbuffer, SDL_GpuTexture **depthtex)
{
    SDL_GpuTextureDescription bbtexdesc, depthtexdesc;

    if (!device) {
        SDL_InvalidParamError("device");
        return NULL;
    } else if (!backbuffer) {
        SDL_InvalidParamError("backbuffer");
        return NULL;
    } else if (!depthtex) {
        SDL_InvalidParamError("depthtex");
        return NULL;
    }

    SDL_GpuGetTextureDescription(backbuffer, &bbtexdesc);

    if (*depthtex) {
        SDL_GpuGetTextureDescription(*depthtex, &depthtexdesc);
    }

    /* !!! FIXME: check texture_type, pixel_format, etc? */
    if (!*depthtex || (depthtexdesc.width != bbtexdesc.width) || (depthtexdesc.height != bbtexdesc.height)) {
        SDL_zero(depthtexdesc);
        depthtexdesc.label = label;
        depthtexdesc.texture_type = SDL_GPUTEXTYPE_2D;
        depthtexdesc.pixel_format = SDL_GPUPIXELFMT_Depth24_Stencil8;
        depthtexdesc.usage = SDL_GPUTEXUSAGE_RENDER_TARGET;  /* !!! FIXME: does this need shader read or write to be the depth buffer? */
        depthtexdesc.width = bbtexdesc.width;
        depthtexdesc.height = bbtexdesc.width;
        SDL_GpuDestroyTexture(*depthtex);
        *depthtex = SDL_GpuCreateTexture(device, &depthtexdesc);
    }

    return *depthtex;
}

/* various object cycle APIs ... */
#define SDL_GPUCYCLETYPE SDL_GpuCpuBufferCycle
#define SDL_GPUCYCLEITEMTYPE SDL_GpuCpuBuffer
#define SDL_GPUCYCLECREATEFNSIG SDL_GpuCreateCpuBufferCycle(const char *label, SDL_GpuDevice *device, const Uint32 bufsize, const void *data, const Uint32 numitems)
#define SDL_GPUCYCLENEXTFNNAME SDL_GpuNextCpuBufferCycle
#define SDL_GPUCYCLENEXTPTRFNNAME SDL_GpuNextCpuBufferPtrCycle
#define SDL_GPUCYCLEDESTROYFNNAME SDL_GpuDestroyCpuBufferCycle
#define SDL_GPUCYCLECREATE(lbl, failvar, itemvar) { itemvar = SDL_GpuCreateCpuBuffer(lbl, device, bufsize, data); failvar = (itemvar == NULL); }
#define SDL_GPUCYCLEDESTROY SDL_GpuDestroyCpuBuffer
#include "SDL_gpu_cycle_impl.h"

#define SDL_GPUCYCLETYPE SDL_GpuBufferCycle
#define SDL_GPUCYCLEITEMTYPE SDL_GpuBuffer
#define SDL_GPUCYCLECREATEFNSIG SDL_GpuCreateBufferCycle(const char *label, SDL_GpuDevice *device, const Uint32 bufsize, const Uint32 numitems)
#define SDL_GPUCYCLENEXTFNNAME SDL_GpuNextBufferCycle
#define SDL_GPUCYCLENEXTPTRFNNAME SDL_GpuNextBufferPtrCycle
#define SDL_GPUCYCLEDESTROYFNNAME SDL_GpuDestroyBufferCycle
#define SDL_GPUCYCLECREATE(lbl, failvar, itemvar) { itemvar = SDL_GpuCreateBuffer(lbl, device, bufsize); failvar = (itemvar == NULL); }
#define SDL_GPUCYCLEDESTROY SDL_GpuDestroyBuffer
#include "SDL_gpu_cycle_impl.h"

#define SDL_GPUCYCLETYPE SDL_GpuTextureCycle
#define SDL_GPUCYCLEITEMTYPE SDL_GpuTexture
#define SDL_GPUCYCLECREATEFNSIG SDL_GpuCreateTextureCycle(const char *label, SDL_GpuDevice *device, const SDL_GpuTextureDescription *texdesc, const Uint32 numitems)
#define SDL_GPUCYCLENEXTFNNAME SDL_GpuNextTextureCycle
#define SDL_GPUCYCLENEXTPTRFNNAME SDL_GpuNextTexturePtrCycle
#define SDL_GPUCYCLEDESTROYFNNAME SDL_GpuDestroyTextureCycle
#define SDL_GPUCYCLECREATE(lbl, failvar, itemvar) { if (texdesc) { SDL_GpuTextureDescription td; SDL_memcpy(&td, texdesc, sizeof (td)); td.label = lbl; itemvar = SDL_GpuCreateTexture(device, &td); failvar = (itemvar == NULL); } else { itemvar = NULL; failvar = SDL_FALSE; } }
#define SDL_GPUCYCLEDESTROY SDL_GpuDestroyTexture
#include "SDL_gpu_cycle_impl.h"

#define SDL_GPUCYCLETYPE SDL_GpuFenceCycle
#define SDL_GPUCYCLEITEMTYPE SDL_GpuFence
#define SDL_GPUCYCLECREATEFNSIG SDL_GpuCreateFenceCycle(const char *label, SDL_GpuDevice *device, const Uint32 numitems)
#define SDL_GPUCYCLENEXTFNNAME SDL_GpuNextFenceCycle
#define SDL_GPUCYCLENEXTPTRFNNAME SDL_GpuNextFencePtrCycle
#define SDL_GPUCYCLEDESTROYFNNAME SDL_GpuDestroyFenceCycle
#define SDL_GPUCYCLECREATE(lbl, failvar, itemvar) { itemvar = SDL_GpuCreateFence(lbl, device); failvar = (itemvar == NULL); }
#define SDL_GPUCYCLEDESTROY SDL_GpuDestroyFence
#include "SDL_gpu_cycle_impl.h"

/* vi: set ts=4 sw=4 expandtab: */
