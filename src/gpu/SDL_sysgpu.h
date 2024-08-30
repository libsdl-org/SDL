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
#include "../video/SDL_sysvideo.h"

#ifndef SDL_GPU_DRIVER_H
#define SDL_GPU_DRIVER_H

// Common Structs

typedef struct Pass
{
    SDL_GPUCommandBuffer *commandBuffer;
    bool inProgress;
} Pass;

typedef struct CommandBufferCommonHeader
{
    SDL_GPUDevice *device;
    Pass renderPass;
    bool graphicsPipelineBound;
    Pass computePass;
    bool computePipelineBound;
    Pass copyPass;
    bool submitted;
} CommandBufferCommonHeader;

typedef struct TextureCommonHeader
{
    SDL_GPUTextureCreateInfo info;
} TextureCommonHeader;

typedef struct BlitFragmentUniforms
{
    // texcoord space
    float left;
    float top;
    float width;
    float height;

    Uint32 mipLevel;
    float layerOrDepth;
} BlitFragmentUniforms;

typedef struct BlitPipelineCacheEntry
{
    SDL_GPUTextureType type;
    SDL_GPUTextureFormat format;
    SDL_GPUGraphicsPipeline *pipeline;
} BlitPipelineCacheEntry;

// Internal Helper Utilities

#define SDL_GPU_TEXTUREFORMAT_MAX        (SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT + 1)
#define SDL_GPU_SWAPCHAINCOMPOSITION_MAX (SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2048 + 1)
#define SDL_GPU_PRESENTMODE_MAX          (SDL_GPU_PRESENTMODE_MAILBOX + 1)

static inline Sint32 Texture_GetBlockSize(
    SDL_GPUTextureFormat format)
{
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_BC1_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC2_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC3_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC7_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC3_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_BC7_UNORM_SRGB:
        return 4;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R16G16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
        return 1;
    default:
        SDL_assert_release(!"Unrecognized TextureFormat!");
        return 0;
    }
}

static inline bool IsDepthFormat(
    SDL_GPUTextureFormat format)
{
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
        return true;

    default:
        return false;
    }
}

static inline bool IsStencilFormat(
    SDL_GPUTextureFormat format)
{
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
        return true;

    default:
        return false;
    }
}

static inline bool IsIntegerFormat(
    SDL_GPUTextureFormat format)
{
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_R8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
        return true;

    default:
        return false;
    }
}

static inline Uint32 IndexSize(SDL_GPUIndexElementSize size)
{
    return (size == SDL_GPU_INDEXELEMENTSIZE_16BIT) ? 2 : 4;
}

static inline Uint32 BytesPerRow(
    Sint32 width,
    SDL_GPUTextureFormat format)
{
    Uint32 blocksPerRow = width;

    if (format == SDL_GPU_TEXTUREFORMAT_BC1_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_BC2_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_BC3_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_BC7_UNORM) {
        blocksPerRow = (width + 3) / 4;
    }

    return blocksPerRow * SDL_GPUTextureFormatTexelBlockSize(format);
}

static inline Sint32 BytesPerImage(
    Uint32 width,
    Uint32 height,
    SDL_GPUTextureFormat format)
{
    Uint32 blocksPerRow = width;
    Uint32 blocksPerColumn = height;

    if (format == SDL_GPU_TEXTUREFORMAT_BC1_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_BC2_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_BC3_UNORM ||
        format == SDL_GPU_TEXTUREFORMAT_BC7_UNORM) {
        blocksPerRow = (width + 3) / 4;
        blocksPerColumn = (height + 3) / 4;
    }

    return blocksPerRow * blocksPerColumn * SDL_GPUTextureFormatTexelBlockSize(format);
}

// GraphicsDevice Limits

#define MAX_TEXTURE_SAMPLERS_PER_STAGE 16
#define MAX_STORAGE_TEXTURES_PER_STAGE 8
#define MAX_STORAGE_BUFFERS_PER_STAGE  8
#define MAX_UNIFORM_BUFFERS_PER_STAGE  4
#define MAX_COMPUTE_WRITE_TEXTURES     8
#define MAX_COMPUTE_WRITE_BUFFERS      8
#define UNIFORM_BUFFER_SIZE            32768
#define MAX_BUFFER_BINDINGS            16
#define MAX_COLOR_TARGET_BINDINGS      4
#define MAX_PRESENT_COUNT              16
#define MAX_FRAMES_IN_FLIGHT           3

// Internal Macros

#define EXPAND_ARRAY_IF_NEEDED(arr, elementType, newCount, capacity, newCapacity) \
    if (newCount >= capacity) {                                                   \
        capacity = newCapacity;                                                   \
        arr = (elementType *)SDL_realloc(                                         \
            arr,                                                                  \
            sizeof(elementType) * capacity);                                      \
    }

// Internal Declarations

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

SDL_GPUGraphicsPipeline *SDL_GPU_FetchBlitPipeline(
    SDL_GPUDevice *device,
    SDL_GPUTextureType sourceTextureType,
    SDL_GPUTextureFormat destinationFormat,
    SDL_GPUShader *blitVertexShader,
    SDL_GPUShader *blitFrom2DShader,
    SDL_GPUShader *blitFrom2DArrayShader,
    SDL_GPUShader *blitFrom3DShader,
    SDL_GPUShader *blitFromCubeShader,
    BlitPipelineCacheEntry **blitPipelines,
    Uint32 *blitPipelineCount,
    Uint32 *blitPipelineCapacity);

void SDL_GPU_BlitCommon(
    SDL_GPUCommandBuffer *commandBuffer,
    SDL_GPUBlitRegion *source,
    SDL_GPUBlitRegion *destination,
    SDL_FlipMode flipMode,
    SDL_GPUFilter filterMode,
    bool cycle,
    SDL_GPUSampler *blitLinearSampler,
    SDL_GPUSampler *blitNearestSampler,
    SDL_GPUShader *blitVertexShader,
    SDL_GPUShader *blitFrom2DShader,
    SDL_GPUShader *blitFrom2DArrayShader,
    SDL_GPUShader *blitFrom3DShader,
    SDL_GPUShader *blitFromCubeShader,
    BlitPipelineCacheEntry **blitPipelines,
    Uint32 *blitPipelineCount,
    Uint32 *blitPipelineCapacity);

#ifdef __cplusplus
}
#endif // __cplusplus

// SDL_GPUDevice Definition

typedef struct SDL_GPURenderer SDL_GPURenderer;

struct SDL_GPUDevice
{
    // Quit

    void (*DestroyDevice)(SDL_GPUDevice *device);

    // State Creation

    SDL_GPUComputePipeline *(*CreateComputePipeline)(
        SDL_GPURenderer *driverData,
        SDL_GPUComputePipelineCreateInfo *pipelineCreateInfo);

    SDL_GPUGraphicsPipeline *(*CreateGraphicsPipeline)(
        SDL_GPURenderer *driverData,
        SDL_GPUGraphicsPipelineCreateInfo *pipelineCreateInfo);

    SDL_GPUSampler *(*CreateSampler)(
        SDL_GPURenderer *driverData,
        SDL_GPUSamplerCreateInfo *samplerCreateInfo);

    SDL_GPUShader *(*CreateShader)(
        SDL_GPURenderer *driverData,
        SDL_GPUShaderCreateInfo *shaderCreateInfo);

    SDL_GPUTexture *(*CreateTexture)(
        SDL_GPURenderer *driverData,
        SDL_GPUTextureCreateInfo *textureCreateInfo);

    SDL_GPUBuffer *(*CreateBuffer)(
        SDL_GPURenderer *driverData,
        SDL_GPUBufferUsageFlags usageFlags,
        Uint32 sizeInBytes);

    SDL_GPUTransferBuffer *(*CreateTransferBuffer)(
        SDL_GPURenderer *driverData,
        SDL_GPUTransferBufferUsage usage,
        Uint32 sizeInBytes);

    // Debug Naming

    void (*SetBufferName)(
        SDL_GPURenderer *driverData,
        SDL_GPUBuffer *buffer,
        const char *text);

    void (*SetTextureName)(
        SDL_GPURenderer *driverData,
        SDL_GPUTexture *texture,
        const char *text);

    void (*InsertDebugLabel)(
        SDL_GPUCommandBuffer *commandBuffer,
        const char *text);

    void (*PushDebugGroup)(
        SDL_GPUCommandBuffer *commandBuffer,
        const char *name);

    void (*PopDebugGroup)(
        SDL_GPUCommandBuffer *commandBuffer);

    // Disposal

    void (*ReleaseTexture)(
        SDL_GPURenderer *driverData,
        SDL_GPUTexture *texture);

    void (*ReleaseSampler)(
        SDL_GPURenderer *driverData,
        SDL_GPUSampler *sampler);

    void (*ReleaseBuffer)(
        SDL_GPURenderer *driverData,
        SDL_GPUBuffer *buffer);

    void (*ReleaseTransferBuffer)(
        SDL_GPURenderer *driverData,
        SDL_GPUTransferBuffer *transferBuffer);

    void (*ReleaseShader)(
        SDL_GPURenderer *driverData,
        SDL_GPUShader *shader);

    void (*ReleaseComputePipeline)(
        SDL_GPURenderer *driverData,
        SDL_GPUComputePipeline *computePipeline);

    void (*ReleaseGraphicsPipeline)(
        SDL_GPURenderer *driverData,
        SDL_GPUGraphicsPipeline *graphicsPipeline);

    // Render Pass

    void (*BeginRenderPass)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUColorAttachmentInfo *colorAttachmentInfos,
        Uint32 colorAttachmentCount,
        SDL_GPUDepthStencilAttachmentInfo *depthStencilAttachmentInfo);

    void (*BindGraphicsPipeline)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUGraphicsPipeline *graphicsPipeline);

    void (*SetViewport)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUViewport *viewport);

    void (*SetScissor)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_Rect *scissor);

    void (*BindVertexBuffers)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstBinding,
        SDL_GPUBufferBinding *pBindings,
        Uint32 bindingCount);

    void (*BindIndexBuffer)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUBufferBinding *pBinding,
        SDL_GPUIndexElementSize indexElementSize);

    void (*BindVertexSamplers)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUTextureSamplerBinding *textureSamplerBindings,
        Uint32 bindingCount);

    void (*BindVertexStorageTextures)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUTexture **storageTextures,
        Uint32 bindingCount);

    void (*BindVertexStorageBuffers)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUBuffer **storageBuffers,
        Uint32 bindingCount);

    void (*BindFragmentSamplers)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUTextureSamplerBinding *textureSamplerBindings,
        Uint32 bindingCount);

    void (*BindFragmentStorageTextures)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUTexture **storageTextures,
        Uint32 bindingCount);

    void (*BindFragmentStorageBuffers)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUBuffer **storageBuffers,
        Uint32 bindingCount);

    void (*PushVertexUniformData)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 slotIndex,
        const void *data,
        Uint32 dataLengthInBytes);

    void (*PushFragmentUniformData)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 slotIndex,
        const void *data,
        Uint32 dataLengthInBytes);

    void (*DrawIndexedPrimitives)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 indexCount,
        Uint32 instanceCount,
        Uint32 firstIndex,
        Sint32 vertexOffset,
        Uint32 firstInstance);

    void (*DrawPrimitives)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 vertexCount,
        Uint32 instanceCount,
        Uint32 firstVertex,
        Uint32 firstInstance);

    void (*DrawPrimitivesIndirect)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUBuffer *buffer,
        Uint32 offsetInBytes,
        Uint32 drawCount,
        Uint32 stride);

    void (*DrawIndexedPrimitivesIndirect)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUBuffer *buffer,
        Uint32 offsetInBytes,
        Uint32 drawCount,
        Uint32 stride);

    void (*EndRenderPass)(
        SDL_GPUCommandBuffer *commandBuffer);

    // Compute Pass

    void (*BeginComputePass)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUStorageTextureWriteOnlyBinding *storageTextureBindings,
        Uint32 storageTextureBindingCount,
        SDL_GPUStorageBufferWriteOnlyBinding *storageBufferBindings,
        Uint32 storageBufferBindingCount);

    void (*BindComputePipeline)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUComputePipeline *computePipeline);

    void (*BindComputeStorageTextures)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUTexture **storageTextures,
        Uint32 bindingCount);

    void (*BindComputeStorageBuffers)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GPUBuffer **storageBuffers,
        Uint32 bindingCount);

    void (*PushComputeUniformData)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 slotIndex,
        const void *data,
        Uint32 dataLengthInBytes);

    void (*DispatchCompute)(
        SDL_GPUCommandBuffer *commandBuffer,
        Uint32 groupCountX,
        Uint32 groupCountY,
        Uint32 groupCountZ);

    void (*DispatchComputeIndirect)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUBuffer *buffer,
        Uint32 offsetInBytes);

    void (*EndComputePass)(
        SDL_GPUCommandBuffer *commandBuffer);

    // TransferBuffer Data

    void *(*MapTransferBuffer)(
        SDL_GPURenderer *device,
        SDL_GPUTransferBuffer *transferBuffer,
        bool cycle);

    void (*UnmapTransferBuffer)(
        SDL_GPURenderer *device,
        SDL_GPUTransferBuffer *transferBuffer);

    // Copy Pass

    void (*BeginCopyPass)(
        SDL_GPUCommandBuffer *commandBuffer);

    void (*UploadToTexture)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUTextureTransferInfo *source,
        SDL_GPUTextureRegion *destination,
        bool cycle);

    void (*UploadToBuffer)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUTransferBufferLocation *source,
        SDL_GPUBufferRegion *destination,
        bool cycle);

    void (*CopyTextureToTexture)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUTextureLocation *source,
        SDL_GPUTextureLocation *destination,
        Uint32 w,
        Uint32 h,
        Uint32 d,
        bool cycle);

    void (*CopyBufferToBuffer)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUBufferLocation *source,
        SDL_GPUBufferLocation *destination,
        Uint32 size,
        bool cycle);

    void (*GenerateMipmaps)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUTexture *texture);

    void (*DownloadFromTexture)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUTextureRegion *source,
        SDL_GPUTextureTransferInfo *destination);

    void (*DownloadFromBuffer)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUBufferRegion *source,
        SDL_GPUTransferBufferLocation *destination);

    void (*EndCopyPass)(
        SDL_GPUCommandBuffer *commandBuffer);

    void (*Blit)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_GPUBlitRegion *source,
        SDL_GPUBlitRegion *destination,
        SDL_FlipMode flipMode,
        SDL_GPUFilter filterMode,
        bool cycle);

    // Submission/Presentation

    bool (*SupportsSwapchainComposition)(
        SDL_GPURenderer *driverData,
        SDL_Window *window,
        SDL_GPUSwapchainComposition swapchainComposition);

    bool (*SupportsPresentMode)(
        SDL_GPURenderer *driverData,
        SDL_Window *window,
        SDL_GPUPresentMode presentMode);

    bool (*ClaimWindow)(
        SDL_GPURenderer *driverData,
        SDL_Window *window);

    void (*ReleaseWindow)(
        SDL_GPURenderer *driverData,
        SDL_Window *window);

    bool (*SetSwapchainParameters)(
        SDL_GPURenderer *driverData,
        SDL_Window *window,
        SDL_GPUSwapchainComposition swapchainComposition,
        SDL_GPUPresentMode presentMode);

    SDL_GPUTextureFormat (*GetSwapchainTextureFormat)(
        SDL_GPURenderer *driverData,
        SDL_Window *window);

    SDL_GPUCommandBuffer *(*AcquireCommandBuffer)(
        SDL_GPURenderer *driverData);

    SDL_GPUTexture *(*AcquireSwapchainTexture)(
        SDL_GPUCommandBuffer *commandBuffer,
        SDL_Window *window,
        Uint32 *pWidth,
        Uint32 *pHeight);

    void (*Submit)(
        SDL_GPUCommandBuffer *commandBuffer);

    SDL_GPUFence *(*SubmitAndAcquireFence)(
        SDL_GPUCommandBuffer *commandBuffer);

    void (*Wait)(
        SDL_GPURenderer *driverData);

    void (*WaitForFences)(
        SDL_GPURenderer *driverData,
        bool waitAll,
        SDL_GPUFence **pFences,
        Uint32 fenceCount);

    bool (*QueryFence)(
        SDL_GPURenderer *driverData,
        SDL_GPUFence *fence);

    void (*ReleaseFence)(
        SDL_GPURenderer *driverData,
        SDL_GPUFence *fence);

    // Feature Queries

    bool (*SupportsTextureFormat)(
        SDL_GPURenderer *driverData,
        SDL_GPUTextureFormat format,
        SDL_GPUTextureType type,
        SDL_GPUTextureUsageFlags usage);

    bool (*SupportsSampleCount)(
        SDL_GPURenderer *driverData,
        SDL_GPUTextureFormat format,
        SDL_GPUSampleCount desiredSampleCount);

    // Opaque pointer for the Driver
    SDL_GPURenderer *driverData;

    // Store this for SDL_GetGPUDriver()
    SDL_GPUDriver backend;

    // Store this for SDL_gpu.c's debug layer
    bool debugMode;
    SDL_GPUShaderFormat shaderFormats;
};

#define ASSIGN_DRIVER_FUNC(func, name) \
    result->func = name##_##func;
#define ASSIGN_DRIVER(name)                                 \
    ASSIGN_DRIVER_FUNC(DestroyDevice, name)                 \
    ASSIGN_DRIVER_FUNC(CreateComputePipeline, name)         \
    ASSIGN_DRIVER_FUNC(CreateGraphicsPipeline, name)        \
    ASSIGN_DRIVER_FUNC(CreateSampler, name)                 \
    ASSIGN_DRIVER_FUNC(CreateShader, name)                  \
    ASSIGN_DRIVER_FUNC(CreateTexture, name)                 \
    ASSIGN_DRIVER_FUNC(CreateBuffer, name)                  \
    ASSIGN_DRIVER_FUNC(CreateTransferBuffer, name)          \
    ASSIGN_DRIVER_FUNC(SetBufferName, name)                 \
    ASSIGN_DRIVER_FUNC(SetTextureName, name)                \
    ASSIGN_DRIVER_FUNC(InsertDebugLabel, name)              \
    ASSIGN_DRIVER_FUNC(PushDebugGroup, name)                \
    ASSIGN_DRIVER_FUNC(PopDebugGroup, name)                 \
    ASSIGN_DRIVER_FUNC(ReleaseTexture, name)                \
    ASSIGN_DRIVER_FUNC(ReleaseSampler, name)                \
    ASSIGN_DRIVER_FUNC(ReleaseBuffer, name)                 \
    ASSIGN_DRIVER_FUNC(ReleaseTransferBuffer, name)         \
    ASSIGN_DRIVER_FUNC(ReleaseShader, name)                 \
    ASSIGN_DRIVER_FUNC(ReleaseComputePipeline, name)        \
    ASSIGN_DRIVER_FUNC(ReleaseGraphicsPipeline, name)       \
    ASSIGN_DRIVER_FUNC(BeginRenderPass, name)               \
    ASSIGN_DRIVER_FUNC(BindGraphicsPipeline, name)          \
    ASSIGN_DRIVER_FUNC(SetViewport, name)                   \
    ASSIGN_DRIVER_FUNC(SetScissor, name)                    \
    ASSIGN_DRIVER_FUNC(BindVertexBuffers, name)             \
    ASSIGN_DRIVER_FUNC(BindIndexBuffer, name)               \
    ASSIGN_DRIVER_FUNC(BindVertexSamplers, name)            \
    ASSIGN_DRIVER_FUNC(BindVertexStorageTextures, name)     \
    ASSIGN_DRIVER_FUNC(BindVertexStorageBuffers, name)      \
    ASSIGN_DRIVER_FUNC(BindFragmentSamplers, name)          \
    ASSIGN_DRIVER_FUNC(BindFragmentStorageTextures, name)   \
    ASSIGN_DRIVER_FUNC(BindFragmentStorageBuffers, name)    \
    ASSIGN_DRIVER_FUNC(PushVertexUniformData, name)         \
    ASSIGN_DRIVER_FUNC(PushFragmentUniformData, name)       \
    ASSIGN_DRIVER_FUNC(DrawIndexedPrimitives, name)         \
    ASSIGN_DRIVER_FUNC(DrawPrimitives, name)                \
    ASSIGN_DRIVER_FUNC(DrawPrimitivesIndirect, name)        \
    ASSIGN_DRIVER_FUNC(DrawIndexedPrimitivesIndirect, name) \
    ASSIGN_DRIVER_FUNC(EndRenderPass, name)                 \
    ASSIGN_DRIVER_FUNC(BeginComputePass, name)              \
    ASSIGN_DRIVER_FUNC(BindComputePipeline, name)           \
    ASSIGN_DRIVER_FUNC(BindComputeStorageTextures, name)    \
    ASSIGN_DRIVER_FUNC(BindComputeStorageBuffers, name)     \
    ASSIGN_DRIVER_FUNC(PushComputeUniformData, name)        \
    ASSIGN_DRIVER_FUNC(DispatchCompute, name)               \
    ASSIGN_DRIVER_FUNC(DispatchComputeIndirect, name)       \
    ASSIGN_DRIVER_FUNC(EndComputePass, name)                \
    ASSIGN_DRIVER_FUNC(MapTransferBuffer, name)             \
    ASSIGN_DRIVER_FUNC(UnmapTransferBuffer, name)           \
    ASSIGN_DRIVER_FUNC(BeginCopyPass, name)                 \
    ASSIGN_DRIVER_FUNC(UploadToTexture, name)               \
    ASSIGN_DRIVER_FUNC(UploadToBuffer, name)                \
    ASSIGN_DRIVER_FUNC(DownloadFromTexture, name)           \
    ASSIGN_DRIVER_FUNC(DownloadFromBuffer, name)            \
    ASSIGN_DRIVER_FUNC(CopyTextureToTexture, name)          \
    ASSIGN_DRIVER_FUNC(CopyBufferToBuffer, name)            \
    ASSIGN_DRIVER_FUNC(GenerateMipmaps, name)               \
    ASSIGN_DRIVER_FUNC(EndCopyPass, name)                   \
    ASSIGN_DRIVER_FUNC(Blit, name)                          \
    ASSIGN_DRIVER_FUNC(SupportsSwapchainComposition, name)  \
    ASSIGN_DRIVER_FUNC(SupportsPresentMode, name)           \
    ASSIGN_DRIVER_FUNC(ClaimWindow, name)                   \
    ASSIGN_DRIVER_FUNC(ReleaseWindow, name)                 \
    ASSIGN_DRIVER_FUNC(SetSwapchainParameters, name)        \
    ASSIGN_DRIVER_FUNC(GetSwapchainTextureFormat, name)     \
    ASSIGN_DRIVER_FUNC(AcquireCommandBuffer, name)          \
    ASSIGN_DRIVER_FUNC(AcquireSwapchainTexture, name)       \
    ASSIGN_DRIVER_FUNC(Submit, name)                        \
    ASSIGN_DRIVER_FUNC(SubmitAndAcquireFence, name)         \
    ASSIGN_DRIVER_FUNC(Wait, name)                          \
    ASSIGN_DRIVER_FUNC(WaitForFences, name)                 \
    ASSIGN_DRIVER_FUNC(QueryFence, name)                    \
    ASSIGN_DRIVER_FUNC(ReleaseFence, name)                  \
    ASSIGN_DRIVER_FUNC(SupportsTextureFormat, name)         \
    ASSIGN_DRIVER_FUNC(SupportsSampleCount, name)

typedef struct SDL_GPUBootstrap
{
    const char *Name;
    const SDL_GPUDriver backendflag;
    const SDL_GPUShaderFormat shaderFormats;
    bool (*PrepareDriver)(SDL_VideoDevice *_this);
    SDL_GPUDevice *(*CreateDevice)(bool debugMode, bool preferLowPower, SDL_PropertiesID props);
} SDL_GPUBootstrap;

#ifdef __cplusplus
extern "C" {
#endif

extern SDL_GPUBootstrap VulkanDriver;
extern SDL_GPUBootstrap D3D11Driver;
extern SDL_GPUBootstrap D3D12Driver;
extern SDL_GPUBootstrap MetalDriver;
extern SDL_GPUBootstrap PS5Driver;

#ifdef __cplusplus
}
#endif

#endif // SDL_GPU_DRIVER_H
