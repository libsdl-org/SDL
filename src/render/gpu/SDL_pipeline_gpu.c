/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

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
#include "SDL_internal.h"

#ifdef SDL_VIDEO_RENDER_GPU

#include "SDL_gpu_util.h"
#include "SDL_pipeline_gpu.h"

#include "../SDL_sysrender.h"

struct GPU_PipelineCacheKeyStruct
{
    Uint64 blend_mode : 28;
    Uint64 frag_shader : 4;
    Uint64 vert_shader : 4;
    Uint64 attachment_format : 6;
    Uint64 primitive_type : 3;
};

typedef union GPU_PipelineCacheKey
{
    struct GPU_PipelineCacheKeyStruct as_struct;
    Uint64 as_uint64;
} GPU_PipelineCacheKey;

SDL_COMPILE_TIME_ASSERT(GPU_PipelineCacheKey_Size, sizeof(GPU_PipelineCacheKey) <= sizeof(Uint64));

typedef struct GPU_PipelineCacheEntry
{
    GPU_PipelineCacheKey key;
    SDL_GPUGraphicsPipeline *pipeline;
} GPU_PipelineCacheEntry;

static Uint32 HashPipelineCacheKey(const GPU_PipelineCacheKey *key)
{
    Uint64 x = key->as_uint64;
    // 64-bit uint hash function stolen from taisei (which stole it from somewhere else)
    x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
    x = x ^ (x >> 31);
    return (Uint32)(x & 0xffffffff);
}

static Uint32 HashPassthrough(const void *key, void *data)
{
    // double-cast to silence a clang warning
    return (Uint32)(uintptr_t)key;
}

static bool MatchPipelineCacheKey(const void *a, const void *b, void *data)
{
    return a == b;
}

static void NukePipelineCacheEntry(const void *key, const void *value, void *data)
{
    GPU_PipelineCacheEntry *entry = (GPU_PipelineCacheEntry *)value;
    SDL_GPUDevice *device = data;

    SDL_ReleaseGPUGraphicsPipeline(device, entry->pipeline);
    SDL_free(entry);
}

bool GPU_InitPipelineCache(GPU_PipelineCache *cache, SDL_GPUDevice *device)
{
    // FIXME how many buckets do we need?
    cache->table = SDL_CreateHashTable(device, 32, HashPassthrough, MatchPipelineCacheKey, NukePipelineCacheEntry, true);
    if (!cache->table) {
        return false;
    }
    return true;
}

void GPU_DestroyPipelineCache(GPU_PipelineCache *cache)
{
    SDL_DestroyHashTable(cache->table);
}

static SDL_GPUGraphicsPipeline *MakePipeline(SDL_GPUDevice *device, GPU_Shaders *shaders, const GPU_PipelineParameters *params)
{
    SDL_GPUColorTargetDescription ad;
    SDL_zero(ad);
    ad.format = params->attachment_format;

    SDL_BlendMode blend = params->blend_mode;
    ad.blend_state.enable_blend = blend != 0;
    ad.blend_state.color_write_mask = 0xF;
    ad.blend_state.alpha_blend_op = GPU_ConvertBlendOperation(SDL_GetBlendModeAlphaOperation(blend));
    ad.blend_state.dst_alpha_blendfactor = GPU_ConvertBlendFactor(SDL_GetBlendModeDstAlphaFactor(blend));
    ad.blend_state.src_alpha_blendfactor = GPU_ConvertBlendFactor(SDL_GetBlendModeSrcAlphaFactor(blend));
    ad.blend_state.color_blend_op = GPU_ConvertBlendOperation(SDL_GetBlendModeColorOperation(blend));
    ad.blend_state.dst_color_blendfactor = GPU_ConvertBlendFactor(SDL_GetBlendModeDstColorFactor(blend));
    ad.blend_state.src_color_blendfactor = GPU_ConvertBlendFactor(SDL_GetBlendModeSrcColorFactor(blend));

    SDL_GPUGraphicsPipelineCreateInfo pci;
    SDL_zero(pci);
    pci.target_info.has_depth_stencil_target = false;
    pci.target_info.num_color_targets = 1;
    pci.target_info.color_target_descriptions = &ad;
    pci.vertex_shader = GPU_GetVertexShader(shaders, params->vert_shader);
    pci.fragment_shader = GPU_GetFragmentShader(shaders, params->frag_shader);
    pci.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pci.multisample_state.enable_mask = false;
    pci.primitive_type = params->primitive_type;

    pci.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pci.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pci.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    SDL_GPUVertexBufferDescription vertex_buffer_desc;
    SDL_zero(vertex_buffer_desc);

    Uint32 num_attribs = 0;
    SDL_GPUVertexAttribute attribs[4];
    SDL_zero(attribs);

    bool have_attr_color = false;
    bool have_attr_uv = false;

    switch (params->vert_shader) {
    case VERT_SHADER_TRI_TEXTURE:
        have_attr_uv = true;
        SDL_FALLTHROUGH;
    case VERT_SHADER_TRI_COLOR:
        have_attr_color = true;
        SDL_FALLTHROUGH;
    default:
        break;
    }

    // Position
    attribs[num_attribs].location = num_attribs;
    attribs[num_attribs].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attribs[num_attribs].offset = vertex_buffer_desc.pitch;
    vertex_buffer_desc.pitch += 2 * sizeof(float);
    num_attribs++;

    if (have_attr_color) {
        // Color
        attribs[num_attribs].location = num_attribs;
        attribs[num_attribs].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        attribs[num_attribs].offset = vertex_buffer_desc.pitch;
        vertex_buffer_desc.pitch += 4 * sizeof(float);
        num_attribs++;
    }

    if (have_attr_uv) {
        // UVs
        attribs[num_attribs].location = num_attribs;
        attribs[num_attribs].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attribs[num_attribs].offset = vertex_buffer_desc.pitch;
        vertex_buffer_desc.pitch += 2 * sizeof(float);
        num_attribs++;
    }

    pci.vertex_input_state.num_vertex_attributes = num_attribs;
    pci.vertex_input_state.vertex_attributes = attribs;
    pci.vertex_input_state.num_vertex_buffers = 1;
    pci.vertex_input_state.vertex_buffer_descriptions = &vertex_buffer_desc;

    return SDL_CreateGPUGraphicsPipeline(device, &pci);
}

static GPU_PipelineCacheKey MakePipelineCacheKey(const GPU_PipelineParameters *params)
{
    GPU_PipelineCacheKey key;
    SDL_zero(key);
    key.as_struct.blend_mode = params->blend_mode;
    key.as_struct.frag_shader = params->frag_shader;
    key.as_struct.vert_shader = params->vert_shader;
    key.as_struct.attachment_format = params->attachment_format;
    key.as_struct.primitive_type = params->primitive_type;
    return key;
}

SDL_GPUGraphicsPipeline *GPU_GetPipeline(GPU_PipelineCache *cache, GPU_Shaders *shaders, SDL_GPUDevice *device, const GPU_PipelineParameters *params)
{
    GPU_PipelineCacheKey key = MakePipelineCacheKey(params);
    void *keyval = (void *)(uintptr_t)HashPipelineCacheKey(&key);
    SDL_GPUGraphicsPipeline *pipeline = NULL;

    void *iter = NULL;
    GPU_PipelineCacheEntry *entry = NULL;

    while (SDL_IterateHashTableKey(cache->table, keyval, (const void **)&entry, &iter)) {
        if (entry->key.as_uint64 == key.as_uint64) {
            return entry->pipeline;
        }
    }

    pipeline = MakePipeline(device, shaders, params);

    if (pipeline == NULL) {
        return NULL;
    }

    entry = SDL_malloc(sizeof(*entry));
    entry->key = key;
    entry->pipeline = pipeline;

    SDL_InsertIntoHashTable(cache->table, keyval, entry);

    return pipeline;
}

#endif // SDL_VIDEO_RENDER_GPU
