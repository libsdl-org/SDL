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

/* Common Struct */

typedef struct Pass
{
    SDL_GpuCommandBuffer *commandBuffer;
    SDL_bool inProgress;
} Pass;

typedef struct CommandBufferCommonHeader
{
    SDL_GpuDevice *device;
    Pass renderPass;
    SDL_bool graphicsPipelineBound;
    Pass computePass;
    SDL_bool computePipelineBound;
    Pass copyPass;
    SDL_bool submitted;
} CommandBufferCommonHeader;

/* Internal Helper Utilities */

static inline Sint32 Texture_GetBlockSize(
    SDL_GpuTextureFormat format)
{
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_BC1:
    case SDL_GPU_TEXTUREFORMAT_BC2:
    case SDL_GPU_TEXTUREFORMAT_BC3:
    case SDL_GPU_TEXTUREFORMAT_BC7:
    case SDL_GPU_TEXTUREFORMAT_BC3_SRGB:
    case SDL_GPU_TEXTUREFORMAT_BC7_SRGB:
        return 4;
    case SDL_GPU_TEXTUREFORMAT_R8:
    case SDL_GPU_TEXTUREFORMAT_A8:
    case SDL_GPU_TEXTUREFORMAT_R8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R5G6B5:
    case SDL_GPU_TEXTUREFORMAT_B4G4R4A4:
    case SDL_GPU_TEXTUREFORMAT_A1R5G5B5:
    case SDL_GPU_TEXTUREFORMAT_R16_SFLOAT:
    case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8:
    case SDL_GPU_TEXTUREFORMAT_R32_SFLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_SFLOAT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SRGB:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_SRGB:
    case SDL_GPU_TEXTUREFORMAT_A2R10G10B10:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16:
    case SDL_GPU_TEXTUREFORMAT_R32G32_SFLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_SFLOAT:
        return 1;
    default:
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Unrecognized TextureFormat!");
        return 0;
    }
}

static inline SDL_bool IsDepthFormat(
    SDL_GpuTextureFormat format)
{
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D32_SFLOAT:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
    case SDL_GPU_TEXTUREFORMAT_D32_SFLOAT_S8_UINT:
        return SDL_TRUE;

    default:
        return SDL_FALSE;
    }
}

static inline SDL_bool IsStencilFormat(
    SDL_GpuTextureFormat format)
{
    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
    case SDL_GPU_TEXTUREFORMAT_D32_SFLOAT_S8_UINT:
        return SDL_TRUE;

    default:
        return SDL_FALSE;
    }
}

static inline Uint32 PrimitiveVerts(
    SDL_GpuPrimitiveType primitiveType,
    Uint32 primitiveCount)
{
    switch (primitiveType) {
    case SDL_GPU_PRIMITIVETYPE_TRIANGLELIST:
        return primitiveCount * 3;
    case SDL_GPU_PRIMITIVETYPE_TRIANGLESTRIP:
        return primitiveCount + 2;
    case SDL_GPU_PRIMITIVETYPE_LINELIST:
        return primitiveCount * 2;
    case SDL_GPU_PRIMITIVETYPE_LINESTRIP:
        return primitiveCount + 1;
    case SDL_GPU_PRIMITIVETYPE_POINTLIST:
        return primitiveCount;
    default:
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "Unrecognized primitive type!");
        return 0;
    }
}

static inline Uint32 IndexSize(SDL_GpuIndexElementSize size)
{
    return (size == SDL_GPU_INDEXELEMENTSIZE_16BIT) ? 2 : 4;
}

static inline Uint32 BytesPerRow(
    Sint32 width,
    SDL_GpuTextureFormat format)
{
    Uint32 blocksPerRow = width;

    if (format == SDL_GPU_TEXTUREFORMAT_BC1 ||
        format == SDL_GPU_TEXTUREFORMAT_BC2 ||
        format == SDL_GPU_TEXTUREFORMAT_BC3 ||
        format == SDL_GPU_TEXTUREFORMAT_BC7) {
        blocksPerRow = (width + 3) / 4;
    }

    return blocksPerRow * SDL_GpuTextureFormatTexelBlockSize(format);
}

static inline Sint32 BytesPerImage(
    Uint32 width,
    Uint32 height,
    SDL_GpuTextureFormat format)
{
    Uint32 blocksPerRow = width;
    Uint32 blocksPerColumn = height;

    if (format == SDL_GPU_TEXTUREFORMAT_BC1 ||
        format == SDL_GPU_TEXTUREFORMAT_BC2 ||
        format == SDL_GPU_TEXTUREFORMAT_BC3 ||
        format == SDL_GPU_TEXTUREFORMAT_BC7) {
        blocksPerRow = (width + 3) / 4;
        blocksPerColumn = (height + 3) / 4;
    }

    return blocksPerRow * blocksPerColumn * SDL_GpuTextureFormatTexelBlockSize(format);
}

/* GraphicsDevice Limits */

#define MAX_TEXTURE_SAMPLERS_PER_STAGE 16
#define MAX_STORAGE_TEXTURES_PER_STAGE 8
#define MAX_STORAGE_BUFFERS_PER_STAGE  8
#define MAX_UNIFORM_BUFFERS_PER_STAGE  14
#define MAX_BUFFER_BINDINGS            16
#define MAX_COLOR_TARGET_BINDINGS      4
#define MAX_PRESENT_COUNT              16
#define MAX_FRAMES_IN_FLIGHT           3

/* SDL_GpuDevice Definition */

typedef struct SDL_GpuRenderer SDL_GpuRenderer;

struct SDL_GpuDevice
{
    /* Quit */

    void (*DestroyDevice)(SDL_GpuDevice *device);

    /* State Creation */

    SDL_GpuComputePipeline *(*CreateComputePipeline)(
        SDL_GpuRenderer *driverData,
        SDL_GpuComputePipelineCreateInfo *pipelineCreateInfo);

    SDL_GpuGraphicsPipeline *(*CreateGraphicsPipeline)(
        SDL_GpuRenderer *driverData,
        SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo);

    SDL_GpuSampler *(*CreateSampler)(
        SDL_GpuRenderer *driverData,
        SDL_GpuSamplerCreateInfo *samplerCreateInfo);

    SDL_GpuShader *(*CreateShader)(
        SDL_GpuRenderer *driverData,
        SDL_GpuShaderCreateInfo *shaderCreateInfo);

    SDL_GpuTexture *(*CreateTexture)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTextureCreateInfo *textureCreateInfo);

    SDL_GpuBuffer *(*CreateBuffer)(
        SDL_GpuRenderer *driverData,
        SDL_GpuBufferUsageFlags usageFlags,
        Uint32 sizeInBytes);

    SDL_GpuTransferBuffer *(*CreateTransferBuffer)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTransferBufferUsage usage,
        Uint32 sizeInBytes);

    /* Debug Naming */

    void (*SetBufferName)(
        SDL_GpuRenderer *driverData,
        SDL_GpuBuffer *buffer,
        const char *text);

    void (*SetTextureName)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTexture *texture,
        const char *text);

    void (*InsertDebugLabel)(
        SDL_GpuCommandBuffer *commandBuffer,
        const char *text);

    void (*PushDebugGroup)(
        SDL_GpuCommandBuffer *commandBuffer,
        const char *name);

    void (*PopDebugGroup)(
        SDL_GpuCommandBuffer *commandBuffer);

    /* Disposal */

    void (*ReleaseTexture)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTexture *texture);

    void (*ReleaseSampler)(
        SDL_GpuRenderer *driverData,
        SDL_GpuSampler *sampler);

    void (*ReleaseBuffer)(
        SDL_GpuRenderer *driverData,
        SDL_GpuBuffer *buffer);

    void (*ReleaseTransferBuffer)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTransferBuffer *transferBuffer);

    void (*ReleaseShader)(
        SDL_GpuRenderer *driverData,
        SDL_GpuShader *shader);

    void (*ReleaseComputePipeline)(
        SDL_GpuRenderer *driverData,
        SDL_GpuComputePipeline *computePipeline);

    void (*ReleaseGraphicsPipeline)(
        SDL_GpuRenderer *driverData,
        SDL_GpuGraphicsPipeline *graphicsPipeline);

    /* Render Pass */

    void (*BeginRenderPass)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
        Uint32 colorAttachmentCount,
        SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo);

    void (*BindGraphicsPipeline)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuGraphicsPipeline *graphicsPipeline);

    void (*SetViewport)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuViewport *viewport);

    void (*SetScissor)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuRect *scissor);

    void (*BindVertexBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstBinding,
        SDL_GpuBufferBinding *pBindings,
        Uint32 bindingCount);

    void (*BindIndexBuffer)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuBufferBinding *pBinding,
        SDL_GpuIndexElementSize indexElementSize);

    void (*BindVertexSamplers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSamplerBinding *textureSamplerBindings,
        Uint32 bindingCount);

    void (*BindVertexStorageTextures)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSlice *storageTextureSlices,
        Uint32 bindingCount);

    void (*BindVertexStorageBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuBuffer **storageBuffers,
        Uint32 bindingCount);

    void (*BindFragmentSamplers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSamplerBinding *textureSamplerBindings,
        Uint32 bindingCount);

    void (*BindFragmentStorageTextures)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSlice *storageTextureSlices,
        Uint32 bindingCount);

    void (*BindFragmentStorageBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuBuffer **storageBuffers,
        Uint32 bindingCount);

    void (*PushVertexUniformData)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 slotIndex,
        const void *data,
        Uint32 dataLengthInBytes);

    void (*PushFragmentUniformData)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 slotIndex,
        const void *data,
        Uint32 dataLengthInBytes);

    void (*DrawIndexedPrimitives)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 baseVertex,
        Uint32 startIndex,
        Uint32 primitiveCount,
        Uint32 instanceCount);

    void (*DrawPrimitives)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 vertexStart,
        Uint32 primitiveCount);

    void (*DrawPrimitivesIndirect)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuBuffer *buffer,
        Uint32 offsetInBytes,
        Uint32 drawCount,
        Uint32 stride);

    void (*DrawIndexedPrimitivesIndirect)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuBuffer *buffer,
        Uint32 offsetInBytes,
        Uint32 drawCount,
        Uint32 stride);

    void (*EndRenderPass)(
        SDL_GpuCommandBuffer *commandBuffer);

    /* Compute Pass */

    void (*BeginComputePass)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuStorageTextureReadWriteBinding *storageTextureBindings,
        Uint32 storageTextureBindingCount,
        SDL_GpuStorageBufferReadWriteBinding *storageBufferBindings,
        Uint32 storageBufferBindingCount);

    void (*BindComputePipeline)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuComputePipeline *computePipeline);

    void (*BindComputeStorageTextures)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuTextureSlice *storageTextureSlices,
        Uint32 bindingCount);

    void (*BindComputeStorageBuffers)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 firstSlot,
        SDL_GpuBuffer **storageBuffers,
        Uint32 bindingCount);

    void (*PushComputeUniformData)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 slotIndex,
        const void *data,
        Uint32 dataLengthInBytes);

    void (*DispatchCompute)(
        SDL_GpuCommandBuffer *commandBuffer,
        Uint32 groupCountX,
        Uint32 groupCountY,
        Uint32 groupCountZ);

    void (*EndComputePass)(
        SDL_GpuCommandBuffer *commandBuffer);

    /* TransferBuffer Data */

    void (*MapTransferBuffer)(
        SDL_GpuRenderer *device,
        SDL_GpuTransferBuffer *transferBuffer,
        SDL_bool cycle,
        void **ppData);

    void (*UnmapTransferBuffer)(
        SDL_GpuRenderer *device,
        SDL_GpuTransferBuffer *transferBuffer);

    void (*SetTransferData)(
        SDL_GpuRenderer *driverData,
        const void *data,
        SDL_GpuTransferBuffer *transferBuffer,
        SDL_GpuBufferCopy *copyParams,
        SDL_bool cycle);

    void (*GetTransferData)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTransferBuffer *transferBuffer,
        void *data,
        SDL_GpuBufferCopy *copyParams);

    /* Copy Pass */

    void (*BeginCopyPass)(
        SDL_GpuCommandBuffer *commandBuffer);

    void (*UploadToTexture)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuTransferBuffer *transferBuffer,
        SDL_GpuTextureRegion *textureSlice,
        SDL_GpuBufferImageCopy *copyParams,
        SDL_bool cycle);

    void (*UploadToBuffer)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuTransferBuffer *transferBuffer,
        SDL_GpuBuffer *buffer,
        SDL_GpuBufferCopy *copyParams,
        SDL_bool cycle);

    void (*CopyTextureToTexture)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuTextureRegion *source,
        SDL_GpuTextureRegion *destination,
        SDL_bool cycle);

    void (*CopyBufferToBuffer)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuBuffer *source,
        SDL_GpuBuffer *destination,
        SDL_GpuBufferCopy *copyParams,
        SDL_bool cycle);

    void (*GenerateMipmaps)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuTexture *texture);

    void (*DownloadFromTexture)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuTextureRegion *textureSlice,
        SDL_GpuTransferBuffer *transferBuffer,
        SDL_GpuBufferImageCopy *copyParams);

    void (*DownloadFromBuffer)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuBuffer *buffer,
        SDL_GpuTransferBuffer *transferBuffer,
        SDL_GpuBufferCopy *copyParams);

    void (*EndCopyPass)(
        SDL_GpuCommandBuffer *commandBuffer);

    void (*Blit)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_GpuTextureRegion *source,
        SDL_GpuTextureRegion *destination,
        SDL_GpuFilter filterMode,
        SDL_bool cycle);

    /* Submission/Presentation */

    SDL_bool (*SupportsSwapchainComposition)(
        SDL_GpuRenderer *driverData,
        SDL_Window *window,
        SDL_GpuSwapchainComposition swapchainComposition);

    SDL_bool (*SupportsPresentMode)(
        SDL_GpuRenderer *driverData,
        SDL_Window *window,
        SDL_GpuPresentMode presentMode);

    SDL_bool (*ClaimWindow)(
        SDL_GpuRenderer *driverData,
        SDL_Window *window,
        SDL_GpuSwapchainComposition swapchainComposition,
        SDL_GpuPresentMode presentMode);

    void (*UnclaimWindow)(
        SDL_GpuRenderer *driverData,
        SDL_Window *window);

    void (*SetSwapchainParameters)(
        SDL_GpuRenderer *driverData,
        SDL_Window *window,
        SDL_GpuSwapchainComposition swapchainComposition,
        SDL_GpuPresentMode presentMode);

    SDL_GpuTextureFormat (*GetSwapchainTextureFormat)(
        SDL_GpuRenderer *driverData,
        SDL_Window *window);

    SDL_GpuCommandBuffer *(*AcquireCommandBuffer)(
        SDL_GpuRenderer *driverData);

    SDL_GpuTexture *(*AcquireSwapchainTexture)(
        SDL_GpuCommandBuffer *commandBuffer,
        SDL_Window *window,
        Uint32 *pWidth,
        Uint32 *pHeight);

    void (*Submit)(
        SDL_GpuCommandBuffer *commandBuffer);

    SDL_GpuFence *(*SubmitAndAcquireFence)(
        SDL_GpuCommandBuffer *commandBuffer);

    void (*Wait)(
        SDL_GpuRenderer *driverData);

    void (*WaitForFences)(
        SDL_GpuRenderer *driverData,
        SDL_bool waitAll,
        SDL_GpuFence **pFences,
        Uint32 fenceCount);

    SDL_bool (*QueryFence)(
        SDL_GpuRenderer *driverData,
        SDL_GpuFence *fence);

    void (*ReleaseFence)(
        SDL_GpuRenderer *driverData,
        SDL_GpuFence *fence);

    /* Feature Queries */

    SDL_bool (*IsTextureFormatSupported)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTextureFormat format,
        SDL_GpuTextureType type,
        SDL_GpuTextureUsageFlags usage);

    SDL_GpuSampleCount (*GetBestSampleCount)(
        SDL_GpuRenderer *driverData,
        SDL_GpuTextureFormat format,
        SDL_GpuSampleCount desiredSampleCount);

    /* Opaque pointer for the Driver */
    SDL_GpuRenderer *driverData;

    /* Store this for SDL_GpuGetBackend() */
    SDL_GpuBackend backend;
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
    ASSIGN_DRIVER_FUNC(EndComputePass, name)                \
    ASSIGN_DRIVER_FUNC(MapTransferBuffer, name)             \
    ASSIGN_DRIVER_FUNC(UnmapTransferBuffer, name)           \
    ASSIGN_DRIVER_FUNC(SetTransferData, name)               \
    ASSIGN_DRIVER_FUNC(GetTransferData, name)               \
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
    ASSIGN_DRIVER_FUNC(UnclaimWindow, name)                 \
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
    ASSIGN_DRIVER_FUNC(IsTextureFormatSupported, name)      \
    ASSIGN_DRIVER_FUNC(GetBestSampleCount, name)

typedef struct SDL_GpuDriver
{
    const char *Name;
    const SDL_GpuBackend backendflag;
    Uint8 (*PrepareDriver)(SDL_VideoDevice *_this);
    SDL_GpuDevice *(*CreateDevice)(SDL_bool debugMode);
} SDL_GpuDriver;

extern SDL_GpuDriver VulkanDriver;
extern SDL_GpuDriver D3D11Driver;
extern SDL_GpuDriver MetalDriver;
extern SDL_GpuDriver PS5Driver;

#endif /* SDL_GPU_DRIVER_H */
