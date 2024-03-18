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
#include "SDL_sysgpu.h"

// FIXME: This could probably use SDL_ObjectValid
#define CHECK_DEVICE_MAGIC(device, retval)  \
    if (device == NULL) {                   \
        SDL_SetError("Invalid GPU device"); \
        return retval;                      \
    }

#define CHECK_COMMAND_BUFFER                                       \
    if (((CommandBufferCommonHeader *)commandBuffer)->submitted) { \
        SDL_assert_release(!"Command buffer already submitted!");  \
        return;                                                    \
    }

#define CHECK_COMMAND_BUFFER_RETURN_NULL                           \
    if (((CommandBufferCommonHeader *)commandBuffer)->submitted) { \
        SDL_assert_release(!"Command buffer already submitted!");  \
        return NULL;                                               \
    }

#define CHECK_ANY_PASS_IN_PROGRESS(msg, retval)                                 \
    if (                                                                        \
        ((CommandBufferCommonHeader *)commandBuffer)->renderPass.inProgress ||  \
        ((CommandBufferCommonHeader *)commandBuffer)->computePass.inProgress || \
        ((CommandBufferCommonHeader *)commandBuffer)->copyPass.inProgress) {    \
        SDL_assert_release(!msg);                                               \
        return retval;                                                          \
    }

#define CHECK_RENDERPASS                                     \
    if (!((Pass *)renderPass)->inProgress) {                 \
        SDL_assert_release(!"Render pass not in progress!"); \
        return;                                              \
    }

#define CHECK_GRAPHICS_PIPELINE_BOUND                                                       \
    if (!((CommandBufferCommonHeader *)RENDERPASS_COMMAND_BUFFER)->graphicsPipelineBound) { \
        SDL_assert_release(!"Graphics pipeline not bound!");                                \
        return;                                                                             \
    }

#define CHECK_COMPUTEPASS                                     \
    if (!((Pass *)computePass)->inProgress) {                 \
        SDL_assert_release(!"Compute pass not in progress!"); \
        return;                                               \
    }

#define CHECK_COMPUTE_PIPELINE_BOUND                                                        \
    if (!((CommandBufferCommonHeader *)COMPUTEPASS_COMMAND_BUFFER)->computePipelineBound) { \
        SDL_assert_release(!"Compute pipeline not bound!");                                 \
        return;                                                                             \
    }

#define CHECK_COPYPASS                                     \
    if (!((Pass *)copyPass)->inProgress) {                 \
        SDL_assert_release(!"Copy pass not in progress!"); \
        return;                                            \
    }

#define CHECK_TEXTUREFORMAT_ENUM_INVALID(format, retval)     \
    if (format >= SDL_GPU_TEXTUREFORMAT_MAX) {               \
        SDL_assert_release(!"Invalid texture format enum!"); \
        return retval;                                       \
    }

#define CHECK_SWAPCHAINCOMPOSITION_ENUM_INVALID(enumval, retval)    \
    if (enumval >= SDL_GPU_SWAPCHAINCOMPOSITION_MAX) {              \
        SDL_assert_release(!"Invalid swapchain composition enum!"); \
        return retval;                                              \
    }

#define CHECK_PRESENTMODE_ENUM_INVALID(enumval, retval)    \
    if (enumval >= SDL_GPU_PRESENTMODE_MAX) {              \
        SDL_assert_release(!"Invalid present mode enum!"); \
        return retval;                                     \
    }

#define COMMAND_BUFFER_DEVICE \
    ((CommandBufferCommonHeader *)commandBuffer)->device

#define RENDERPASS_COMMAND_BUFFER \
    ((Pass *)renderPass)->commandBuffer

#define RENDERPASS_DEVICE \
    ((CommandBufferCommonHeader *)RENDERPASS_COMMAND_BUFFER)->device

#define COMPUTEPASS_COMMAND_BUFFER \
    ((Pass *)computePass)->commandBuffer

#define COMPUTEPASS_DEVICE \
    ((CommandBufferCommonHeader *)COMPUTEPASS_COMMAND_BUFFER)->device

#define COPYPASS_COMMAND_BUFFER \
    ((Pass *)copyPass)->commandBuffer

#define COPYPASS_DEVICE \
    ((CommandBufferCommonHeader *)COPYPASS_COMMAND_BUFFER)->device

// Drivers

static const SDL_GpuBootstrap *backends[] = {
#ifdef SDL_GPU_METAL
    &MetalDriver,
#endif
#ifdef SDL_GPU_D3D12
    &D3D12Driver,
#endif
#ifdef SDL_GPU_VULKAN
    &VulkanDriver,
#endif
#ifdef SDL_GPU_D3D11
    &D3D11Driver,
#endif
    NULL
};

// Internal Utility Functions

SDL_GpuGraphicsPipeline *SDL_Gpu_FetchBlitPipeline(
    SDL_GpuDevice *device,
    SDL_GpuTextureType sourceTextureType,
    SDL_GpuTextureFormat destinationFormat,
    SDL_GpuShader *blitVertexShader,
    SDL_GpuShader *blitFrom2DShader,
    SDL_GpuShader *blitFrom2DArrayShader,
    SDL_GpuShader *blitFrom3DShader,
    SDL_GpuShader *blitFromCubeShader,
    BlitPipelineCacheEntry **blitPipelines,
    Uint32 *blitPipelineCount,
    Uint32 *blitPipelineCapacity)
{
    SDL_GpuGraphicsPipelineCreateInfo blitPipelineCreateInfo;
    SDL_GpuColorAttachmentDescription colorAttachmentDesc;
    SDL_GpuGraphicsPipeline *pipeline;

    if (blitPipelineCount == NULL) {
        // use pre-created, format-agnostic pipelines
        return (*blitPipelines)[sourceTextureType].pipeline;
    }

    for (Uint32 i = 0; i < *blitPipelineCount; i += 1) {
        if ((*blitPipelines)[i].type == sourceTextureType && (*blitPipelines)[i].format == destinationFormat) {
            return (*blitPipelines)[i].pipeline;
        }
    }

    // No pipeline found, we'll need to make one!
    SDL_zero(blitPipelineCreateInfo);

    SDL_zero(colorAttachmentDesc);
    colorAttachmentDesc.blendState.colorWriteMask = 0xF;
    colorAttachmentDesc.format = destinationFormat;

    blitPipelineCreateInfo.attachmentInfo.colorAttachmentDescriptions = &colorAttachmentDesc;
    blitPipelineCreateInfo.attachmentInfo.colorAttachmentCount = 1;
    blitPipelineCreateInfo.attachmentInfo.depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM; // arbitrary
    blitPipelineCreateInfo.attachmentInfo.hasDepthStencilAttachment = SDL_FALSE;

    blitPipelineCreateInfo.vertexShader = blitVertexShader;
    if (sourceTextureType == SDL_GPU_TEXTURETYPE_CUBE) {
        blitPipelineCreateInfo.fragmentShader = blitFromCubeShader;
    } else if (sourceTextureType == SDL_GPU_TEXTURETYPE_2D_ARRAY) {
        blitPipelineCreateInfo.fragmentShader = blitFrom2DArrayShader;
    } else if (sourceTextureType == SDL_GPU_TEXTURETYPE_3D) {
        blitPipelineCreateInfo.fragmentShader = blitFrom3DShader;
    } else {
        blitPipelineCreateInfo.fragmentShader = blitFrom2DShader;
    }

    blitPipelineCreateInfo.multisampleState.sampleCount = SDL_GPU_SAMPLECOUNT_1;
    blitPipelineCreateInfo.multisampleState.sampleMask = 0xFFFFFFFF;

    blitPipelineCreateInfo.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    blitPipelineCreateInfo.blendConstants[0] = 1.0f;
    blitPipelineCreateInfo.blendConstants[1] = 1.0f;
    blitPipelineCreateInfo.blendConstants[2] = 1.0f;
    blitPipelineCreateInfo.blendConstants[3] = 1.0f;

    pipeline = SDL_CreateGpuGraphicsPipeline(
        device,
        &blitPipelineCreateInfo);

    if (pipeline == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create graphics pipeline for blit");
        return NULL;
    }

    // Cache the new pipeline
    EXPAND_ARRAY_IF_NEEDED(
        (*blitPipelines),
        BlitPipelineCacheEntry,
        *blitPipelineCount + 1,
        *blitPipelineCapacity,
        *blitPipelineCapacity * 2)

    (*blitPipelines)[*blitPipelineCount].pipeline = pipeline;
    (*blitPipelines)[*blitPipelineCount].type = sourceTextureType;
    (*blitPipelines)[*blitPipelineCount].format = destinationFormat;
    *blitPipelineCount += 1;

    return pipeline;
}

void SDL_Gpu_BlitCommon(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBlitRegion *source,
    SDL_GpuBlitRegion *destination,
    SDL_FlipMode flipMode,
    SDL_GpuFilter filterMode,
    SDL_bool cycle,
    SDL_GpuSampler *blitLinearSampler,
    SDL_GpuSampler *blitNearestSampler,
    SDL_GpuShader *blitVertexShader,
    SDL_GpuShader *blitFrom2DShader,
    SDL_GpuShader *blitFrom2DArrayShader,
    SDL_GpuShader *blitFrom3DShader,
    SDL_GpuShader *blitFromCubeShader,
    BlitPipelineCacheEntry **blitPipelines,
    Uint32 *blitPipelineCount,
    Uint32 *blitPipelineCapacity)
{
    CommandBufferCommonHeader *cmdbufHeader = (CommandBufferCommonHeader *)commandBuffer;
    SDL_GpuRenderPass *renderPass;
    TextureCommonHeader *srcHeader = (TextureCommonHeader *)source->texture;
    TextureCommonHeader *dstHeader = (TextureCommonHeader *)destination->texture;
    SDL_GpuGraphicsPipeline *blitPipeline;
    SDL_GpuColorAttachmentInfo colorAttachmentInfo;
    SDL_GpuViewport viewport;
    SDL_GpuTextureSamplerBinding textureSamplerBinding;
    BlitFragmentUniforms blitFragmentUniforms;
    Uint32 layerDivisor;

    blitPipeline = SDL_Gpu_FetchBlitPipeline(
        cmdbufHeader->device,
        srcHeader->info.type,
        dstHeader->info.format,
        blitVertexShader,
        blitFrom2DShader,
        blitFrom2DArrayShader,
        blitFrom3DShader,
        blitFromCubeShader,
        blitPipelines,
        blitPipelineCount,
        blitPipelineCapacity);

    if (blitPipeline == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Could not fetch blit pipeline");
        return;
    }

    // If the entire destination is blitted, we don't have to load
    if (
        dstHeader->info.layerCountOrDepth == 1 &&
        dstHeader->info.levelCount == 1 &&
        dstHeader->info.type != SDL_GPU_TEXTURETYPE_3D &&
        destination->w == dstHeader->info.width &&
        destination->h == dstHeader->info.height) {
        colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_DONT_CARE;
    } else {
        colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_LOAD;
    }

    colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

    colorAttachmentInfo.texture = destination->texture;
    colorAttachmentInfo.mipLevel = destination->mipLevel;
    colorAttachmentInfo.layerOrDepthPlane = destination->layerOrDepthPlane;
    colorAttachmentInfo.cycle = cycle;

    renderPass = SDL_BeginGpuRenderPass(
        commandBuffer,
        &colorAttachmentInfo,
        1,
        NULL);

    viewport.x = (float)destination->x;
    viewport.y = (float)destination->y;
    viewport.w = (float)destination->w;
    viewport.h = (float)destination->h;
    viewport.minDepth = 0;
    viewport.maxDepth = 1;

    SDL_SetGpuViewport(
        renderPass,
        &viewport);

    SDL_BindGpuGraphicsPipeline(
        renderPass,
        blitPipeline);

    textureSamplerBinding.texture = source->texture;
    textureSamplerBinding.sampler =
        filterMode == SDL_GPU_FILTER_NEAREST ? blitNearestSampler : blitLinearSampler;

    SDL_BindGpuFragmentSamplers(
        renderPass,
        0,
        &textureSamplerBinding,
        1);

    blitFragmentUniforms.left = (float)source->x / (srcHeader->info.width >> source->mipLevel);
    blitFragmentUniforms.top = (float)source->y / (srcHeader->info.height >> source->mipLevel);
    blitFragmentUniforms.width = (float)source->w / (srcHeader->info.width >> source->mipLevel);
    blitFragmentUniforms.height = (float)source->h / (srcHeader->info.height >> source->mipLevel);
    blitFragmentUniforms.mipLevel = source->mipLevel;

    layerDivisor = (srcHeader->info.type == SDL_GPU_TEXTURETYPE_3D) ? srcHeader->info.layerCountOrDepth : 1;
    blitFragmentUniforms.layerOrDepth = (float)source->layerOrDepthPlane / layerDivisor;

    if (flipMode & SDL_FLIP_HORIZONTAL) {
        blitFragmentUniforms.left += blitFragmentUniforms.width;
        blitFragmentUniforms.width *= -1;
    }

    if (flipMode & SDL_FLIP_VERTICAL) {
        blitFragmentUniforms.top += blitFragmentUniforms.height;
        blitFragmentUniforms.height *= -1;
    }

    SDL_PushGpuFragmentUniformData(
        commandBuffer,
        0,
        &blitFragmentUniforms,
        sizeof(blitFragmentUniforms));

    SDL_DrawGpuPrimitives(renderPass, 3, 1, 0, 0);
    SDL_EndGpuRenderPass(renderPass);
}

// Driver Functions

static SDL_GpuDriver SDL_GpuSelectBackend(
    SDL_VideoDevice *_this,
    const char *gpudriver,
    SDL_GpuShaderFormat formatFlags)
{
    Uint32 i;

    // Environment/Properties override...
    if (gpudriver != NULL) {
        for (i = 0; backends[i]; i += 1) {
            if (SDL_strcasecmp(gpudriver, backends[i]->Name) == 0) {
                if (!(backends[i]->shaderFormats & formatFlags)) {
                    SDL_LogError(SDL_LOG_CATEGORY_GPU, "Required shader format for backend %s not provided!", gpudriver);
                    return SDL_GPU_DRIVER_INVALID;
                }
                if (backends[i]->PrepareDriver(_this)) {
                    return backends[i]->backendflag;
                }
            }
        }

        SDL_LogError(SDL_LOG_CATEGORY_GPU, "SDL_HINT_GPU_DRIVER %s unsupported!", gpudriver);
        return SDL_GPU_DRIVER_INVALID;
    }

    for (i = 0; backends[i]; i += 1) {
        if ((backends[i]->shaderFormats & formatFlags) == 0) {
            // Don't select a backend which doesn't support the app's shaders.
            continue;
        }
        if (backends[i]->PrepareDriver(_this)) {
            return backends[i]->backendflag;
        }
    }

    SDL_LogError(SDL_LOG_CATEGORY_GPU, "No supported SDL_Gpu backend found!");
    return SDL_GPU_DRIVER_INVALID;
}

SDL_GpuDevice *SDL_CreateGpuDevice(
    SDL_GpuShaderFormat formatFlags,
    SDL_bool debugMode,
    SDL_bool preferLowPower,
    const char *name)
{
    SDL_GpuDevice *result;
    SDL_PropertiesID props = SDL_CreateProperties();
    if (formatFlags & SDL_GPU_SHADERFORMAT_SECRET) {
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_SECRET_BOOL, SDL_TRUE);
    }
    if (formatFlags & SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_SPIRV_BOOL, SDL_TRUE);
    }
    if (formatFlags & SDL_GPU_SHADERFORMAT_DXBC) {
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_DXBC_BOOL, SDL_TRUE);
    }
    if (formatFlags & SDL_GPU_SHADERFORMAT_DXIL) {
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_DXIL_BOOL, SDL_TRUE);
    }
    if (formatFlags & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_MSL_BOOL, SDL_TRUE);
    }
    if (formatFlags & SDL_GPU_SHADERFORMAT_METALLIB) {
        SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_METALLIB_BOOL, SDL_TRUE);
    }
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_DEBUGMODE_BOOL, debugMode);
    SDL_SetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_PREFERLOWPOWER_BOOL, preferLowPower);
    SDL_SetStringProperty(props, SDL_PROP_GPU_CREATEDEVICE_NAME_STRING, name);
    result = SDL_CreateGpuDeviceWithProperties(props);
    SDL_DestroyProperties(props);
    return result;
}

SDL_GpuDevice *SDL_CreateGpuDeviceWithProperties(SDL_PropertiesID props)
{
    SDL_GpuShaderFormat formatFlags = 0;
    SDL_bool debugMode;
    SDL_bool preferLowPower;

    int i;
    const char *gpudriver;
    SDL_GpuDevice *result = NULL;
    SDL_GpuDriver selectedBackend;
    SDL_VideoDevice *_this = SDL_GetVideoDevice();

    if (_this == NULL) {
        SDL_SetError("Video subsystem not initialized");
        return NULL;
    }

    if (SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_SECRET_BOOL, SDL_FALSE)) {
        formatFlags |= SDL_GPU_SHADERFORMAT_SECRET;
    }
    if (SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_SPIRV_BOOL, SDL_FALSE)) {
        formatFlags |= SDL_GPU_SHADERFORMAT_SPIRV;
    }
    if (SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_DXBC_BOOL, SDL_FALSE)) {
        formatFlags |= SDL_GPU_SHADERFORMAT_DXBC;
    }
    if (SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_DXIL_BOOL, SDL_FALSE)) {
        formatFlags |= SDL_GPU_SHADERFORMAT_DXIL;
    }
    if (SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_MSL_BOOL, SDL_FALSE)) {
        formatFlags |= SDL_GPU_SHADERFORMAT_MSL;
    }
    if (SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_SHADERS_METALLIB_BOOL, SDL_FALSE)) {
        formatFlags |= SDL_GPU_SHADERFORMAT_METALLIB;
    }

    debugMode = SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_DEBUGMODE_BOOL, SDL_TRUE);
    preferLowPower = SDL_GetBooleanProperty(props, SDL_PROP_GPU_CREATEDEVICE_PREFERLOWPOWER_BOOL, SDL_TRUE);

    gpudriver = SDL_GetHint(SDL_HINT_GPU_DRIVER);
    if (gpudriver == NULL) {
        gpudriver = SDL_GetStringProperty(props, SDL_PROP_GPU_CREATEDEVICE_NAME_STRING, NULL);
    }

    selectedBackend = SDL_GpuSelectBackend(_this, gpudriver, formatFlags);
    if (selectedBackend != SDL_GPU_DRIVER_INVALID) {
        for (i = 0; backends[i]; i += 1) {
            if (backends[i]->backendflag == selectedBackend) {
                result = backends[i]->CreateDevice(debugMode, preferLowPower, props);
                if (result != NULL) {
                    result->backend = backends[i]->backendflag;
                    result->shaderFormats = backends[i]->shaderFormats;
                    result->debugMode = debugMode;
                    break;
                }
            }
        }
    }
    return result;
}

void SDL_DestroyGpuDevice(SDL_GpuDevice *device)
{
    CHECK_DEVICE_MAGIC(device, );

    device->DestroyDevice(device);
}

SDL_GpuDriver SDL_GetGpuDriver(SDL_GpuDevice *device)
{
    CHECK_DEVICE_MAGIC(device, SDL_GPU_DRIVER_INVALID);

    return device->backend;
}

Uint32 SDL_GpuTextureFormatTexelBlockSize(
    SDL_GpuTextureFormat textureFormat)
{
    switch (textureFormat) {
    case SDL_GPU_TEXTUREFORMAT_BC1_UNORM:
        return 8;
    case SDL_GPU_TEXTUREFORMAT_BC2_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC3_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC7_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC3_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_BC7_UNORM_SRGB:
        return 16;
    case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8_UINT:
        return 1;
    case SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16_UINT:
        return 2;
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
        return 4;
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
        return 8;
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
        return 16;
    default:
        SDL_assert_release(!"Unrecognized TextureFormat!");
        return 0;
    }
}

SDL_bool SDL_SupportsGpuTextureFormat(
    SDL_GpuDevice *device,
    SDL_GpuTextureFormat format,
    SDL_GpuTextureType type,
    SDL_GpuTextureUsageFlags usage)
{
    CHECK_DEVICE_MAGIC(device, SDL_FALSE);

    if (device->debugMode) {
        CHECK_TEXTUREFORMAT_ENUM_INVALID(format, SDL_FALSE)
    }

    return device->SupportsTextureFormat(
        device->driverData,
        format,
        type,
        usage);
}

SDL_bool SDL_SupportsGpuSampleCount(
    SDL_GpuDevice *device,
    SDL_GpuTextureFormat format,
    SDL_GpuSampleCount sampleCount)
{
    CHECK_DEVICE_MAGIC(device, 0);

    if (device->debugMode) {
        CHECK_TEXTUREFORMAT_ENUM_INVALID(format, 0)
    }

    return device->SupportsSampleCount(
        device->driverData,
        format,
        sampleCount);
}

// State Creation

SDL_GpuComputePipeline *SDL_CreateGpuComputePipeline(
    SDL_GpuDevice *device,
    SDL_GpuComputePipelineCreateInfo *computePipelineCreateInfo)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (computePipelineCreateInfo == NULL) {
        SDL_InvalidParamError("computePipelineCreateInfo");
        return NULL;
    }

    if (device->debugMode) {
        if (!(computePipelineCreateInfo->format & device->shaderFormats)) {
            SDL_assert_release(!"Incompatible shader format for GPU backend");
            return NULL;
        }

        if (computePipelineCreateInfo->writeOnlyStorageTextureCount > MAX_COMPUTE_WRITE_TEXTURES) {
            SDL_assert_release(!"Compute pipeline write-only texture count cannot be higher than 8!");
            return NULL;
        }
        if (computePipelineCreateInfo->writeOnlyStorageBufferCount > MAX_COMPUTE_WRITE_BUFFERS) {
            SDL_assert_release(!"Compute pipeline write-only buffer count cannot be higher than 8!");
            return NULL;
        }
        if (computePipelineCreateInfo->threadCountX == 0 ||
            computePipelineCreateInfo->threadCountY == 0 ||
            computePipelineCreateInfo->threadCountZ == 0) {
            SDL_assert_release(!"Compute pipeline threadCount dimensions must be at least 1!");
            return NULL;
        }
    }

    return device->CreateComputePipeline(
        device->driverData,
        computePipelineCreateInfo);
}

SDL_GpuGraphicsPipeline *SDL_CreateGpuGraphicsPipeline(
    SDL_GpuDevice *device,
    SDL_GpuGraphicsPipelineCreateInfo *graphicsPipelineCreateInfo)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (graphicsPipelineCreateInfo == NULL) {
        SDL_InvalidParamError("graphicsPipelineCreateInfo");
        return NULL;
    }

    if (device->debugMode) {
        for (Uint32 i = 0; i < graphicsPipelineCreateInfo->attachmentInfo.colorAttachmentCount; i += 1) {
            CHECK_TEXTUREFORMAT_ENUM_INVALID(graphicsPipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].format, NULL);
            if (IsDepthFormat(graphicsPipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].format)) {
                SDL_assert_release(!"Color attachment formats cannot be a depth format!");
                return NULL;
            }
        }
        if (graphicsPipelineCreateInfo->attachmentInfo.hasDepthStencilAttachment) {
            CHECK_TEXTUREFORMAT_ENUM_INVALID(graphicsPipelineCreateInfo->attachmentInfo.depthStencilFormat, NULL);
            if (!IsDepthFormat(graphicsPipelineCreateInfo->attachmentInfo.depthStencilFormat)) {
                SDL_assert_release(!"Depth-stencil attachment format must be a depth format!");
                return NULL;
            }
        }
    }

    return device->CreateGraphicsPipeline(
        device->driverData,
        graphicsPipelineCreateInfo);
}

SDL_GpuSampler *SDL_CreateGpuSampler(
    SDL_GpuDevice *device,
    SDL_GpuSamplerCreateInfo *samplerCreateInfo)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (samplerCreateInfo == NULL) {
        SDL_InvalidParamError("samplerCreateInfo");
        return NULL;
    }

    return device->CreateSampler(
        device->driverData,
        samplerCreateInfo);
}

SDL_GpuShader *SDL_CreateGpuShader(
    SDL_GpuDevice *device,
    SDL_GpuShaderCreateInfo *shaderCreateInfo)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (shaderCreateInfo == NULL) {
        SDL_InvalidParamError("shaderCreateInfo");
        return NULL;
    }

    if (device->debugMode) {
        if (!(shaderCreateInfo->format & device->shaderFormats)) {
            SDL_assert_release(!"Incompatible shader format for GPU backend");
            return NULL;
        }
    }

    return device->CreateShader(
        device->driverData,
        shaderCreateInfo);
}

SDL_GpuTexture *SDL_CreateGpuTexture(
    SDL_GpuDevice *device,
    SDL_GpuTextureCreateInfo *textureCreateInfo)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (textureCreateInfo == NULL) {
        SDL_InvalidParamError("textureCreateInfo");
        return NULL;
    }

    if (device->debugMode) {
        SDL_bool failed = SDL_FALSE;

        const Uint32 MAX_2D_DIMENSION = 16384;
        const Uint32 MAX_3D_DIMENSION = 2048;

        // Common checks for all texture types
        CHECK_TEXTUREFORMAT_ENUM_INVALID(textureCreateInfo->format, NULL)

        if (textureCreateInfo->width <= 0 || textureCreateInfo->height <= 0 || textureCreateInfo->layerCountOrDepth <= 0) {
            SDL_assert_release(!"For any texture: width, height, and layerCountOrDepth must be >= 1");
            failed = SDL_TRUE;
        }
        if (textureCreateInfo->levelCount <= 0) {
            SDL_assert_release(!"For any texture: levelCount must be >= 1");
            failed = SDL_TRUE;
        }
        if ((textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT) && (textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT)) {
            SDL_assert_release(!"For any texture: usageFlags cannot contain both GRAPHICS_STORAGE_READ_BIT and SAMPLER_BIT");
            failed = SDL_TRUE;
        }
        if (IsDepthFormat(textureCreateInfo->format) && (textureCreateInfo->usageFlags & ~(SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT | SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT))) {
            SDL_assert_release(!"For depth textures: usageFlags cannot contain any flags except for DEPTH_STENCIL_TARGET_BIT and SAMPLER_BIT");
            failed = SDL_TRUE;
        }
        if (IsIntegerFormat(textureCreateInfo->format) && (textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT)) {
            SDL_assert_release(!"For any texture: usageFlags cannot contain SAMPLER_BIT for textures with an integer format");
            failed = SDL_TRUE;
        }

        if (textureCreateInfo->type == SDL_GPU_TEXTURETYPE_CUBE) {
            // Cubemap validation
            if (textureCreateInfo->width != textureCreateInfo->height) {
                SDL_assert_release(!"For cube textures: width and height must be identical");
                failed = SDL_TRUE;
            }
            if (textureCreateInfo->width > MAX_2D_DIMENSION || textureCreateInfo->height > MAX_2D_DIMENSION) {
                SDL_assert_release(!"For cube textures: width and height must be <= 16384");
                failed = SDL_TRUE;
            }
            if (textureCreateInfo->layerCountOrDepth != 6) {
                SDL_assert_release(!"For cube textures: layerCountOrDepth must be 6");
                failed = SDL_TRUE;
            }
            if (textureCreateInfo->sampleCount > SDL_GPU_SAMPLECOUNT_1) {
                SDL_assert_release(!"For cube textures: sampleCount must be SDL_GPU_SAMPLECOUNT_1");
                failed = SDL_TRUE;
            }
            if (!SDL_SupportsGpuTextureFormat(device, textureCreateInfo->format, SDL_GPU_TEXTURETYPE_CUBE, textureCreateInfo->usageFlags)) {
                SDL_assert_release(!"For cube textures: the format is unsupported for the given usageFlags");
                failed = SDL_TRUE;
            }
        } else if (textureCreateInfo->type == SDL_GPU_TEXTURETYPE_3D) {
            // 3D Texture Validation
            if (textureCreateInfo->width > MAX_3D_DIMENSION || textureCreateInfo->height > MAX_3D_DIMENSION || textureCreateInfo->layerCountOrDepth > MAX_3D_DIMENSION) {
                SDL_assert_release(!"For 3D textures: width, height, and layerCountOrDepth must be <= 2048");
                failed = SDL_TRUE;
            }
            if (textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) {
                SDL_assert_release(!"For 3D textures: usageFlags must not contain DEPTH_STENCIL_TARGET_BIT");
                failed = SDL_TRUE;
            }
            if (textureCreateInfo->sampleCount > SDL_GPU_SAMPLECOUNT_1) {
                SDL_assert_release(!"For 3D textures: sampleCount must be SDL_GPU_SAMPLECOUNT_1");
                failed = SDL_TRUE;
            }
            if (!SDL_SupportsGpuTextureFormat(device, textureCreateInfo->format, SDL_GPU_TEXTURETYPE_3D, textureCreateInfo->usageFlags)) {
                SDL_assert_release(!"For 3D textures: the format is unsupported for the given usageFlags");
                failed = SDL_TRUE;
            }
        } else {
            if (textureCreateInfo->type == SDL_GPU_TEXTURETYPE_2D_ARRAY) {
                // Array Texture Validation
                if (textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) {
                    SDL_assert_release(!"For array textures: usageFlags must not contain DEPTH_STENCIL_TARGET_BIT");
                    failed = SDL_TRUE;
                }
                if (textureCreateInfo->sampleCount > SDL_GPU_SAMPLECOUNT_1) {
                    SDL_assert_release(!"For array textures: sampleCount must be SDL_GPU_SAMPLECOUNT_1");
                    failed = SDL_TRUE;
                }
            } else {
                // 2D Texture Validation
                if (textureCreateInfo->sampleCount > SDL_GPU_SAMPLECOUNT_1 && textureCreateInfo->levelCount > 1) {
                    SDL_assert_release(!"For 2D textures: if sampleCount is >= SDL_GPU_SAMPLECOUNT_1, then levelCount must be 1");
                    failed = SDL_TRUE;
                }
            }
            if (!SDL_SupportsGpuTextureFormat(device, textureCreateInfo->format, SDL_GPU_TEXTURETYPE_2D, textureCreateInfo->usageFlags)) {
                SDL_assert_release(!"For 2D textures: the format is unsupported for the given usageFlags");
                failed = SDL_TRUE;
            }
        }

        if (failed) {
            return NULL;
        }
    }

    return device->CreateTexture(
        device->driverData,
        textureCreateInfo);
}

SDL_GpuBuffer *SDL_CreateGpuBuffer(
    SDL_GpuDevice *device,
    SDL_GpuBufferCreateInfo *bufferCreateInfo)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (bufferCreateInfo == NULL) {
        SDL_InvalidParamError("bufferCreateInfo");
        return NULL;
    }

    return device->CreateBuffer(
        device->driverData,
        bufferCreateInfo->usageFlags,
        bufferCreateInfo->sizeInBytes);
}

SDL_GpuTransferBuffer *SDL_CreateGpuTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBufferCreateInfo *transferBufferCreateInfo)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (transferBufferCreateInfo == NULL) {
        SDL_InvalidParamError("transferBufferCreateInfo");
        return NULL;
    }

    return device->CreateTransferBuffer(
        device->driverData,
        transferBufferCreateInfo->usage,
        transferBufferCreateInfo->sizeInBytes);
}

// Debug Naming

void SDL_SetGpuBufferName(
    SDL_GpuDevice *device,
    SDL_GpuBuffer *buffer,
    const char *text)
{
    CHECK_DEVICE_MAGIC(device, );
    if (buffer == NULL) {
        SDL_InvalidParamError("buffer");
        return;
    }
    if (text == NULL) {
        SDL_InvalidParamError("text");
    }

    device->SetBufferName(
        device->driverData,
        buffer,
        text);
}

void SDL_SetGpuTextureName(
    SDL_GpuDevice *device,
    SDL_GpuTexture *texture,
    const char *text)
{
    CHECK_DEVICE_MAGIC(device, );
    if (texture == NULL) {
        SDL_InvalidParamError("texture");
        return;
    }
    if (text == NULL) {
        SDL_InvalidParamError("text");
    }

    device->SetTextureName(
        device->driverData,
        texture,
        text);
}

void SDL_InsertGpuDebugLabel(
    SDL_GpuCommandBuffer *commandBuffer,
    const char *text)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }
    if (text == NULL) {
        SDL_InvalidParamError("text");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
    }

    COMMAND_BUFFER_DEVICE->InsertDebugLabel(
        commandBuffer,
        text);
}

void SDL_PushGpuDebugGroup(
    SDL_GpuCommandBuffer *commandBuffer,
    const char *name)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }
    if (name == NULL) {
        SDL_InvalidParamError("name");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
    }

    COMMAND_BUFFER_DEVICE->PushDebugGroup(
        commandBuffer,
        name);
}

void SDL_PopGpuDebugGroup(
    SDL_GpuCommandBuffer *commandBuffer)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
    }

    COMMAND_BUFFER_DEVICE->PopDebugGroup(
        commandBuffer);
}

// Disposal

void SDL_ReleaseGpuTexture(
    SDL_GpuDevice *device,
    SDL_GpuTexture *texture)
{
    CHECK_DEVICE_MAGIC(device, );
    if (texture == NULL) {
        return;
    }

    device->ReleaseTexture(
        device->driverData,
        texture);
}

void SDL_ReleaseGpuSampler(
    SDL_GpuDevice *device,
    SDL_GpuSampler *sampler)
{
    CHECK_DEVICE_MAGIC(device, );
    if (sampler == NULL) {
        return;
    }

    device->ReleaseSampler(
        device->driverData,
        sampler);
}

void SDL_ReleaseGpuBuffer(
    SDL_GpuDevice *device,
    SDL_GpuBuffer *buffer)
{
    CHECK_DEVICE_MAGIC(device, );
    if (buffer == NULL) {
        return;
    }

    device->ReleaseBuffer(
        device->driverData,
        buffer);
}

void SDL_ReleaseGpuTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBuffer *transferBuffer)
{
    CHECK_DEVICE_MAGIC(device, );
    if (transferBuffer == NULL) {
        return;
    }

    device->ReleaseTransferBuffer(
        device->driverData,
        transferBuffer);
}

void SDL_ReleaseGpuShader(
    SDL_GpuDevice *device,
    SDL_GpuShader *shader)
{
    CHECK_DEVICE_MAGIC(device, );
    if (shader == NULL) {
        return;
    }

    device->ReleaseShader(
        device->driverData,
        shader);
}

void SDL_ReleaseGpuComputePipeline(
    SDL_GpuDevice *device,
    SDL_GpuComputePipeline *computePipeline)
{
    CHECK_DEVICE_MAGIC(device, );
    if (computePipeline == NULL) {
        return;
    }

    device->ReleaseComputePipeline(
        device->driverData,
        computePipeline);
}

void SDL_ReleaseGpuGraphicsPipeline(
    SDL_GpuDevice *device,
    SDL_GpuGraphicsPipeline *graphicsPipeline)
{
    CHECK_DEVICE_MAGIC(device, );
    if (graphicsPipeline == NULL) {
        return;
    }

    device->ReleaseGraphicsPipeline(
        device->driverData,
        graphicsPipeline);
}

// Command Buffer

SDL_GpuCommandBuffer *SDL_AcquireGpuCommandBuffer(
    SDL_GpuDevice *device)
{
    SDL_GpuCommandBuffer *commandBuffer;
    CommandBufferCommonHeader *commandBufferHeader;

    CHECK_DEVICE_MAGIC(device, NULL);

    commandBuffer = device->AcquireCommandBuffer(
        device->driverData);

    if (commandBuffer == NULL) {
        return NULL;
    }

    commandBufferHeader = (CommandBufferCommonHeader *)commandBuffer;
    commandBufferHeader->device = device;
    commandBufferHeader->renderPass.commandBuffer = commandBuffer;
    commandBufferHeader->renderPass.inProgress = SDL_FALSE;
    commandBufferHeader->graphicsPipelineBound = SDL_FALSE;
    commandBufferHeader->computePass.commandBuffer = commandBuffer;
    commandBufferHeader->computePass.inProgress = SDL_FALSE;
    commandBufferHeader->computePipelineBound = SDL_FALSE;
    commandBufferHeader->copyPass.commandBuffer = commandBuffer;
    commandBufferHeader->copyPass.inProgress = SDL_FALSE;
    commandBufferHeader->submitted = SDL_FALSE;

    return commandBuffer;
}

// Uniforms

void SDL_PushGpuVertexUniformData(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }
    if (data == NULL) {
        SDL_InvalidParamError("data");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
    }

    COMMAND_BUFFER_DEVICE->PushVertexUniformData(
        commandBuffer,
        slotIndex,
        data,
        dataLengthInBytes);
}

void SDL_PushGpuFragmentUniformData(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }
    if (data == NULL) {
        SDL_InvalidParamError("data");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
    }

    COMMAND_BUFFER_DEVICE->PushFragmentUniformData(
        commandBuffer,
        slotIndex,
        data,
        dataLengthInBytes);
}

void SDL_PushGpuComputeUniformData(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }
    if (data == NULL) {
        SDL_InvalidParamError("data");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
    }

    COMMAND_BUFFER_DEVICE->PushComputeUniformData(
        commandBuffer,
        slotIndex,
        data,
        dataLengthInBytes);
}

// Render Pass

SDL_GpuRenderPass *SDL_BeginGpuRenderPass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    CommandBufferCommonHeader *commandBufferHeader;

    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return NULL;
    }
    if (colorAttachmentInfos == NULL && colorAttachmentCount > 0) {
        SDL_InvalidParamError("colorAttachmentInfos");
        return NULL;
    }

    if (colorAttachmentCount > MAX_COLOR_TARGET_BINDINGS) {
        SDL_SetError("colorAttachmentCount exceeds MAX_COLOR_TARGET_BINDINGS");
        return NULL;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER_RETURN_NULL
        CHECK_ANY_PASS_IN_PROGRESS("Cannot begin render pass during another pass!", NULL)

        for (Uint32 i = 0; i < colorAttachmentCount; i += 1) {
            if (colorAttachmentInfos[i].cycle && colorAttachmentInfos[i].loadOp == SDL_GPU_LOADOP_LOAD) {
                SDL_assert_release(!"Cannot cycle color attachment when load op is LOAD!");
            }
        }

        if (depthStencilAttachmentInfo != NULL && depthStencilAttachmentInfo->cycle && (depthStencilAttachmentInfo->loadOp == SDL_GPU_LOADOP_LOAD || depthStencilAttachmentInfo->loadOp == SDL_GPU_LOADOP_LOAD)) {
            SDL_assert_release(!"Cannot cycle depth attachment when load op or stencil load op is LOAD!");
        }
    }

    COMMAND_BUFFER_DEVICE->BeginRenderPass(
        commandBuffer,
        colorAttachmentInfos,
        colorAttachmentCount,
        depthStencilAttachmentInfo);

    commandBufferHeader = (CommandBufferCommonHeader *)commandBuffer;
    commandBufferHeader->renderPass.inProgress = SDL_TRUE;
    return (SDL_GpuRenderPass *)&(commandBufferHeader->renderPass);
}

void SDL_BindGpuGraphicsPipeline(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuGraphicsPipeline *graphicsPipeline)
{
    CommandBufferCommonHeader *commandBufferHeader;

    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (graphicsPipeline == NULL) {
        SDL_InvalidParamError("graphicsPipeline");
        return;
    }

    RENDERPASS_DEVICE->BindGraphicsPipeline(
        RENDERPASS_COMMAND_BUFFER,
        graphicsPipeline);

    commandBufferHeader = (CommandBufferCommonHeader *)RENDERPASS_COMMAND_BUFFER;
    commandBufferHeader->graphicsPipelineBound = SDL_TRUE;
}

void SDL_SetGpuViewport(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuViewport *viewport)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (viewport == NULL) {
        SDL_InvalidParamError("viewport");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->SetViewport(
        RENDERPASS_COMMAND_BUFFER,
        viewport);
}

void SDL_SetGpuScissor(
    SDL_GpuRenderPass *renderPass,
    SDL_Rect *scissor)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (scissor == NULL) {
        SDL_InvalidParamError("scissor");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->SetScissor(
        RENDERPASS_COMMAND_BUFFER,
        scissor);
}

void SDL_BindGpuVertexBuffers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstBinding,
    SDL_GpuBufferBinding *pBindings,
    Uint32 bindingCount)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (pBindings == NULL && bindingCount > 0) {
        SDL_InvalidParamError("pBindings");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindVertexBuffers(
        RENDERPASS_COMMAND_BUFFER,
        firstBinding,
        pBindings,
        bindingCount);
}

void SDL_BindGpuIndexBuffer(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuBufferBinding *pBinding,
    SDL_GpuIndexElementSize indexElementSize)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (pBinding == NULL) {
        SDL_InvalidParamError("pBinding");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindIndexBuffer(
        RENDERPASS_COMMAND_BUFFER,
        pBinding,
        indexElementSize);
}

void SDL_BindGpuVertexSamplers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (textureSamplerBindings == NULL && bindingCount > 0) {
        SDL_InvalidParamError("textureSamplerBindings");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindVertexSamplers(
        RENDERPASS_COMMAND_BUFFER,
        firstSlot,
        textureSamplerBindings,
        bindingCount);
}

void SDL_BindGpuVertexStorageTextures(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTexture **storageTextures,
    Uint32 bindingCount)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (storageTextures == NULL && bindingCount > 0) {
        SDL_InvalidParamError("storageTextures");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindVertexStorageTextures(
        RENDERPASS_COMMAND_BUFFER,
        firstSlot,
        storageTextures,
        bindingCount);
}

void SDL_BindGpuVertexStorageBuffers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (storageBuffers == NULL && bindingCount > 0) {
        SDL_InvalidParamError("storageBuffers");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindVertexStorageBuffers(
        RENDERPASS_COMMAND_BUFFER,
        firstSlot,
        storageBuffers,
        bindingCount);
}

void SDL_BindGpuFragmentSamplers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (textureSamplerBindings == NULL && bindingCount > 0) {
        SDL_InvalidParamError("textureSamplerBindings");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindFragmentSamplers(
        RENDERPASS_COMMAND_BUFFER,
        firstSlot,
        textureSamplerBindings,
        bindingCount);
}

void SDL_BindGpuFragmentStorageTextures(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuTexture **storageTextures,
    Uint32 bindingCount)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (storageTextures == NULL && bindingCount > 0) {
        SDL_InvalidParamError("storageTextures");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindFragmentStorageTextures(
        RENDERPASS_COMMAND_BUFFER,
        firstSlot,
        storageTextures,
        bindingCount);
}

void SDL_BindGpuFragmentStorageBuffers(
    SDL_GpuRenderPass *renderPass,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (storageBuffers == NULL && bindingCount > 0) {
        SDL_InvalidParamError("storageBuffers");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->BindFragmentStorageBuffers(
        RENDERPASS_COMMAND_BUFFER,
        firstSlot,
        storageBuffers,
        bindingCount);
}

void SDL_DrawGpuIndexedPrimitives(
    SDL_GpuRenderPass *renderPass,
    Uint32 indexCount,
    Uint32 instanceCount,
    Uint32 firstIndex,
    Sint32 vertexOffset,
    Uint32 firstInstance)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
        CHECK_GRAPHICS_PIPELINE_BOUND
    }

    RENDERPASS_DEVICE->DrawIndexedPrimitives(
        RENDERPASS_COMMAND_BUFFER,
        indexCount,
        instanceCount,
        firstIndex,
        vertexOffset,
        firstInstance);
}

void SDL_DrawGpuPrimitives(
    SDL_GpuRenderPass *renderPass,
    Uint32 vertexCount,
    Uint32 instanceCount,
    Uint32 firstVertex,
    Uint32 firstInstance)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
        CHECK_GRAPHICS_PIPELINE_BOUND
    }

    RENDERPASS_DEVICE->DrawPrimitives(
        RENDERPASS_COMMAND_BUFFER,
        vertexCount,
        instanceCount,
        firstVertex,
        firstInstance);
}

void SDL_DrawGpuPrimitivesIndirect(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (buffer == NULL) {
        SDL_InvalidParamError("buffer");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
        CHECK_GRAPHICS_PIPELINE_BOUND
    }

    RENDERPASS_DEVICE->DrawPrimitivesIndirect(
        RENDERPASS_COMMAND_BUFFER,
        buffer,
        offsetInBytes,
        drawCount,
        stride);
}

void SDL_DrawGpuIndexedPrimitivesIndirect(
    SDL_GpuRenderPass *renderPass,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }
    if (buffer == NULL) {
        SDL_InvalidParamError("buffer");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
        CHECK_GRAPHICS_PIPELINE_BOUND
    }

    RENDERPASS_DEVICE->DrawIndexedPrimitivesIndirect(
        RENDERPASS_COMMAND_BUFFER,
        buffer,
        offsetInBytes,
        drawCount,
        stride);
}

void SDL_EndGpuRenderPass(
    SDL_GpuRenderPass *renderPass)
{
    CommandBufferCommonHeader *commandBufferCommonHeader;

    if (renderPass == NULL) {
        SDL_InvalidParamError("renderPass");
        return;
    }

    if (RENDERPASS_DEVICE->debugMode) {
        CHECK_RENDERPASS
    }

    RENDERPASS_DEVICE->EndRenderPass(
        RENDERPASS_COMMAND_BUFFER);

    commandBufferCommonHeader = (CommandBufferCommonHeader *)RENDERPASS_COMMAND_BUFFER;
    commandBufferCommonHeader->renderPass.inProgress = SDL_FALSE;
    commandBufferCommonHeader->graphicsPipelineBound = SDL_FALSE;
}

// Compute Pass

SDL_GpuComputePass *SDL_BeginGpuComputePass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuStorageTextureWriteOnlyBinding *storageTextureBindings,
    Uint32 storageTextureBindingCount,
    SDL_GpuStorageBufferWriteOnlyBinding *storageBufferBindings,
    Uint32 storageBufferBindingCount)
{
    CommandBufferCommonHeader *commandBufferHeader;

    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return NULL;
    }
    if (storageTextureBindings == NULL && storageTextureBindingCount > 0) {
        SDL_InvalidParamError("storageTextureBindings");
        return NULL;
    }
    if (storageBufferBindings == NULL && storageBufferBindingCount > 0) {
        SDL_InvalidParamError("storageBufferBindings");
        return NULL;
    }
    if (storageTextureBindingCount > MAX_COMPUTE_WRITE_TEXTURES) {
        SDL_InvalidParamError("storageTextureBindingCount");
        return NULL;
    }
    if (storageBufferBindingCount > MAX_COMPUTE_WRITE_BUFFERS) {
        SDL_InvalidParamError("storageBufferBindingCount");
        return NULL;
    }
    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER_RETURN_NULL
        CHECK_ANY_PASS_IN_PROGRESS("Cannot begin compute pass during another pass!", NULL)
    }

    COMMAND_BUFFER_DEVICE->BeginComputePass(
        commandBuffer,
        storageTextureBindings,
        storageTextureBindingCount,
        storageBufferBindings,
        storageBufferBindingCount);

    commandBufferHeader = (CommandBufferCommonHeader *)commandBuffer;
    commandBufferHeader->computePass.inProgress = SDL_TRUE;
    return (SDL_GpuComputePass *)&(commandBufferHeader->computePass);
}

void SDL_BindGpuComputePipeline(
    SDL_GpuComputePass *computePass,
    SDL_GpuComputePipeline *computePipeline)
{
    CommandBufferCommonHeader *commandBufferHeader;

    if (computePass == NULL) {
        SDL_InvalidParamError("computePass");
        return;
    }
    if (computePipeline == NULL) {
        SDL_InvalidParamError("computePipeline");
        return;
    }

    if (COMPUTEPASS_DEVICE->debugMode) {
        CHECK_COMPUTEPASS
    }

    COMPUTEPASS_DEVICE->BindComputePipeline(
        COMPUTEPASS_COMMAND_BUFFER,
        computePipeline);

    commandBufferHeader = (CommandBufferCommonHeader *)COMPUTEPASS_COMMAND_BUFFER;
    commandBufferHeader->computePipelineBound = SDL_TRUE;
}

void SDL_BindGpuComputeStorageTextures(
    SDL_GpuComputePass *computePass,
    Uint32 firstSlot,
    SDL_GpuTexture **storageTextures,
    Uint32 bindingCount)
{
    if (computePass == NULL) {
        SDL_InvalidParamError("computePass");
        return;
    }
    if (storageTextures == NULL && bindingCount > 0) {
        SDL_InvalidParamError("storageTextures");
        return;
    }

    if (COMPUTEPASS_DEVICE->debugMode) {
        CHECK_COMPUTEPASS
    }

    COMPUTEPASS_DEVICE->BindComputeStorageTextures(
        COMPUTEPASS_COMMAND_BUFFER,
        firstSlot,
        storageTextures,
        bindingCount);
}

void SDL_BindGpuComputeStorageBuffers(
    SDL_GpuComputePass *computePass,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount)
{
    if (computePass == NULL) {
        SDL_InvalidParamError("computePass");
        return;
    }
    if (storageBuffers == NULL && bindingCount > 0) {
        SDL_InvalidParamError("storageBuffers");
        return;
    }

    if (COMPUTEPASS_DEVICE->debugMode) {
        CHECK_COMPUTEPASS
    }

    COMPUTEPASS_DEVICE->BindComputeStorageBuffers(
        COMPUTEPASS_COMMAND_BUFFER,
        firstSlot,
        storageBuffers,
        bindingCount);
}

void SDL_DispatchGpuCompute(
    SDL_GpuComputePass *computePass,
    Uint32 groupCountX,
    Uint32 groupCountY,
    Uint32 groupCountZ)
{
    if (computePass == NULL) {
        SDL_InvalidParamError("computePass");
        return;
    }

    if (COMPUTEPASS_DEVICE->debugMode) {
        CHECK_COMPUTEPASS
        CHECK_COMPUTE_PIPELINE_BOUND
    }

    COMPUTEPASS_DEVICE->DispatchCompute(
        COMPUTEPASS_COMMAND_BUFFER,
        groupCountX,
        groupCountY,
        groupCountZ);
}

void SDL_DispatchGpuComputeIndirect(
    SDL_GpuComputePass *computePass,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes)
{
    if (computePass == NULL) {
        SDL_InvalidParamError("computePass");
        return;
    }

    if (COMPUTEPASS_DEVICE->debugMode) {
        CHECK_COMPUTEPASS
        CHECK_COMPUTE_PIPELINE_BOUND
    }

    COMPUTEPASS_DEVICE->DispatchComputeIndirect(
        COMPUTEPASS_COMMAND_BUFFER,
        buffer,
        offsetInBytes);
}

void SDL_EndGpuComputePass(
    SDL_GpuComputePass *computePass)
{
    CommandBufferCommonHeader *commandBufferCommonHeader;

    if (computePass == NULL) {
        SDL_InvalidParamError("computePass");
        return;
    }

    if (COMPUTEPASS_DEVICE->debugMode) {
        CHECK_COMPUTEPASS
    }

    COMPUTEPASS_DEVICE->EndComputePass(
        COMPUTEPASS_COMMAND_BUFFER);

    commandBufferCommonHeader = (CommandBufferCommonHeader *)COMPUTEPASS_COMMAND_BUFFER;
    commandBufferCommonHeader->computePass.inProgress = SDL_FALSE;
    commandBufferCommonHeader->computePipelineBound = SDL_FALSE;
}

// TransferBuffer Data

void *SDL_MapGpuTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBuffer *transferBuffer,
    SDL_bool cycle)
{
    CHECK_DEVICE_MAGIC(device, NULL);
    if (transferBuffer == NULL) {
        SDL_InvalidParamError("transferBuffer");
        return NULL;
    }

    return device->MapTransferBuffer(
        device->driverData,
        transferBuffer,
        cycle);
}

void SDL_UnmapGpuTransferBuffer(
    SDL_GpuDevice *device,
    SDL_GpuTransferBuffer *transferBuffer)
{
    CHECK_DEVICE_MAGIC(device, );
    if (transferBuffer == NULL) {
        SDL_InvalidParamError("transferBuffer");
        return;
    }

    device->UnmapTransferBuffer(
        device->driverData,
        transferBuffer);
}

// Copy Pass

SDL_GpuCopyPass *SDL_BeginGpuCopyPass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    CommandBufferCommonHeader *commandBufferHeader;

    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return NULL;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER_RETURN_NULL
        CHECK_ANY_PASS_IN_PROGRESS("Cannot begin copy pass during another pass!", NULL)
    }

    COMMAND_BUFFER_DEVICE->BeginCopyPass(
        commandBuffer);

    commandBufferHeader = (CommandBufferCommonHeader *)commandBuffer;
    commandBufferHeader->copyPass.inProgress = SDL_TRUE;
    return (SDL_GpuCopyPass *)&(commandBufferHeader->copyPass);
}

void SDL_UploadToGpuTexture(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTextureTransferInfo *source,
    SDL_GpuTextureRegion *destination,
    SDL_bool cycle)
{
    if (copyPass == NULL) {
        SDL_InvalidParamError("copyPass");
        return;
    }
    if (source == NULL) {
        SDL_InvalidParamError("source");
        return;
    }
    if (destination == NULL) {
        SDL_InvalidParamError("destination");
        return;
    }

    if (COPYPASS_DEVICE->debugMode) {
        CHECK_COPYPASS
    }

    COPYPASS_DEVICE->UploadToTexture(
        COPYPASS_COMMAND_BUFFER,
        source,
        destination,
        cycle);
}

void SDL_UploadToGpuBuffer(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTransferBufferLocation *source,
    SDL_GpuBufferRegion *destination,
    SDL_bool cycle)
{
    if (copyPass == NULL) {
        SDL_InvalidParamError("copyPass");
        return;
    }
    if (source == NULL) {
        SDL_InvalidParamError("source");
        return;
    }
    if (destination == NULL) {
        SDL_InvalidParamError("destination");
        return;
    }

    COPYPASS_DEVICE->UploadToBuffer(
        COPYPASS_COMMAND_BUFFER,
        source,
        destination,
        cycle);
}

void SDL_CopyGpuTextureToTexture(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTextureLocation *source,
    SDL_GpuTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    SDL_bool cycle)
{
    if (copyPass == NULL) {
        SDL_InvalidParamError("copyPass");
        return;
    }
    if (source == NULL) {
        SDL_InvalidParamError("source");
        return;
    }
    if (destination == NULL) {
        SDL_InvalidParamError("destination");
        return;
    }

    COPYPASS_DEVICE->CopyTextureToTexture(
        COPYPASS_COMMAND_BUFFER,
        source,
        destination,
        w,
        h,
        d,
        cycle);
}

void SDL_CopyGpuBufferToBuffer(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuBufferLocation *source,
    SDL_GpuBufferLocation *destination,
    Uint32 size,
    SDL_bool cycle)
{
    if (copyPass == NULL) {
        SDL_InvalidParamError("copyPass");
        return;
    }
    if (source == NULL) {
        SDL_InvalidParamError("source");
        return;
    }
    if (destination == NULL) {
        SDL_InvalidParamError("destination");
        return;
    }

    COPYPASS_DEVICE->CopyBufferToBuffer(
        COPYPASS_COMMAND_BUFFER,
        source,
        destination,
        size,
        cycle);
}

void SDL_DownloadFromGpuTexture(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuTextureRegion *source,
    SDL_GpuTextureTransferInfo *destination)
{
    if (copyPass == NULL) {
        SDL_InvalidParamError("copyPass");
        return;
    }
    if (source == NULL) {
        SDL_InvalidParamError("source");
        return;
    }
    if (destination == NULL) {
        SDL_InvalidParamError("destination");
        return;
    }

    COPYPASS_DEVICE->DownloadFromTexture(
        COPYPASS_COMMAND_BUFFER,
        source,
        destination);
}

void SDL_DownloadFromGpuBuffer(
    SDL_GpuCopyPass *copyPass,
    SDL_GpuBufferRegion *source,
    SDL_GpuTransferBufferLocation *destination)
{
    if (copyPass == NULL) {
        SDL_InvalidParamError("copyPass");
        return;
    }
    if (source == NULL) {
        SDL_InvalidParamError("source");
        return;
    }
    if (destination == NULL) {
        SDL_InvalidParamError("destination");
        return;
    }

    COPYPASS_DEVICE->DownloadFromBuffer(
        COPYPASS_COMMAND_BUFFER,
        source,
        destination);
}

void SDL_EndGpuCopyPass(
    SDL_GpuCopyPass *copyPass)
{
    if (copyPass == NULL) {
        SDL_InvalidParamError("copyPass");
        return;
    }

    if (COPYPASS_DEVICE->debugMode) {
        CHECK_COPYPASS
    }

    COPYPASS_DEVICE->EndCopyPass(
        COPYPASS_COMMAND_BUFFER);

    ((CommandBufferCommonHeader *)COPYPASS_COMMAND_BUFFER)->copyPass.inProgress = SDL_FALSE;
}

void SDL_GenerateGpuMipmaps(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTexture *texture)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }
    if (texture == NULL) {
        SDL_InvalidParamError("texture");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
        CHECK_ANY_PASS_IN_PROGRESS("Cannot generate mipmaps during a pass!", )

        TextureCommonHeader *header = (TextureCommonHeader *)texture;
        if (header->info.levelCount <= 1) {
            SDL_assert_release(!"Cannot generate mipmaps for texture with levelCount <= 1!");
            return;
        }

        if (!(header->info.usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT) || !(header->info.usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT)) {
            SDL_assert_release(!"GenerateMipmaps texture must be created with SAMPLER_BIT and COLOR_TARGET_BIT usage flags!");
            return;
        }
    }

    COMMAND_BUFFER_DEVICE->GenerateMipmaps(
        commandBuffer,
        texture);
}

void SDL_BlitGpu(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBlitRegion *source,
    SDL_GpuBlitRegion *destination,
    SDL_FlipMode flipMode,
    SDL_GpuFilter filterMode,
    SDL_bool cycle)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }
    if (source == NULL) {
        SDL_InvalidParamError("source");
        return;
    }
    if (destination == NULL) {
        SDL_InvalidParamError("destination");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
        CHECK_ANY_PASS_IN_PROGRESS("Cannot blit during a pass!", )

        // Validation
        SDL_bool failed = SDL_FALSE;
        TextureCommonHeader *srcHeader = (TextureCommonHeader *)source->texture;
        TextureCommonHeader *dstHeader = (TextureCommonHeader *)destination->texture;

        if (srcHeader == NULL || dstHeader == NULL) {
            SDL_assert_release(!"Blit source and destination textures must be non-NULL");
            return; // attempting to proceed will crash
        }
        if ((srcHeader->info.usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT) == 0) {
            SDL_assert_release(!"Blit source texture must be created with the SAMPLER_BIT usage flag");
            failed = SDL_TRUE;
        }
        if ((dstHeader->info.usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) == 0) {
            SDL_assert_release(!"Blit destination texture must be created with the COLOR_TARGET_BIT usage flag");
            failed = SDL_TRUE;
        }
        if (IsDepthFormat(srcHeader->info.format)) {
            SDL_assert_release(!"Blit source texture cannot have a depth format");
            failed = SDL_TRUE;
        }
        if (source->w == 0 || source->h == 0 || destination->w == 0 || destination->h == 0) {
            SDL_assert_release(!"Blit source/destination regions must have non-zero width, height, and depth");
            failed = SDL_TRUE;
        }

        if (failed) {
            return;
        }
    }

    COMMAND_BUFFER_DEVICE->Blit(
        commandBuffer,
        source,
        destination,
        flipMode,
        filterMode,
        cycle);
}

// Submission/Presentation

SDL_bool SDL_SupportsGpuSwapchainComposition(
    SDL_GpuDevice *device,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition)
{
    CHECK_DEVICE_MAGIC(device, SDL_FALSE);
    if (window == NULL) {
        SDL_InvalidParamError("window");
        return SDL_FALSE;
    }

    if (device->debugMode) {
        CHECK_SWAPCHAINCOMPOSITION_ENUM_INVALID(swapchainComposition, SDL_FALSE)
    }

    return device->SupportsSwapchainComposition(
        device->driverData,
        window,
        swapchainComposition);
}

SDL_bool SDL_SupportsGpuPresentMode(
    SDL_GpuDevice *device,
    SDL_Window *window,
    SDL_GpuPresentMode presentMode)
{
    CHECK_DEVICE_MAGIC(device, SDL_FALSE);
    if (window == NULL) {
        SDL_InvalidParamError("window");
        return SDL_FALSE;
    }

    if (device->debugMode) {
        CHECK_PRESENTMODE_ENUM_INVALID(presentMode, SDL_FALSE)
    }

    return device->SupportsPresentMode(
        device->driverData,
        window,
        presentMode);
}

SDL_bool SDL_ClaimGpuWindow(
    SDL_GpuDevice *device,
    SDL_Window *window)
{
    CHECK_DEVICE_MAGIC(device, SDL_FALSE);
    if (window == NULL) {
        SDL_InvalidParamError("window");
        return SDL_FALSE;
    }

    return device->ClaimWindow(
        device->driverData,
        window);
}

void SDL_UnclaimGpuWindow(
    SDL_GpuDevice *device,
    SDL_Window *window)
{
    CHECK_DEVICE_MAGIC(device, );
    if (window == NULL) {
        SDL_InvalidParamError("window");
        return;
    }

    device->UnclaimWindow(
        device->driverData,
        window);
}

SDL_bool SDL_SetGpuSwapchainParameters(
    SDL_GpuDevice *device,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode)
{
    CHECK_DEVICE_MAGIC(device, SDL_FALSE);
    if (window == NULL) {
        SDL_InvalidParamError("window");
        return SDL_FALSE;
    }

    if (device->debugMode) {
        CHECK_SWAPCHAINCOMPOSITION_ENUM_INVALID(swapchainComposition, SDL_FALSE)
        CHECK_PRESENTMODE_ENUM_INVALID(presentMode, SDL_FALSE)
    }

    return device->SetSwapchainParameters(
        device->driverData,
        window,
        swapchainComposition,
        presentMode);
}

SDL_GpuTextureFormat SDL_GetGpuSwapchainTextureFormat(
    SDL_GpuDevice *device,
    SDL_Window *window)
{
    CHECK_DEVICE_MAGIC(device, SDL_GPU_TEXTUREFORMAT_INVALID);
    if (window == NULL) {
        SDL_InvalidParamError("window");
        return SDL_GPU_TEXTUREFORMAT_INVALID;
    }

    return device->GetSwapchainTextureFormat(
        device->driverData,
        window);
}

SDL_GpuTexture *SDL_AcquireGpuSwapchainTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_Window *window,
    Uint32 *pWidth,
    Uint32 *pHeight)
{
    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return NULL;
    }
    if (window == NULL) {
        SDL_InvalidParamError("window");
        return NULL;
    }
    if (pWidth == NULL) {
        SDL_InvalidParamError("pWidth");
        return NULL;
    }
    if (pHeight == NULL) {
        SDL_InvalidParamError("pHeight");
        return NULL;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER_RETURN_NULL
        CHECK_ANY_PASS_IN_PROGRESS("Cannot acquire a swapchain texture during a pass!", NULL)
    }

    return COMMAND_BUFFER_DEVICE->AcquireSwapchainTexture(
        commandBuffer,
        window,
        pWidth,
        pHeight);
}

void SDL_SubmitGpu(
    SDL_GpuCommandBuffer *commandBuffer)
{
    CommandBufferCommonHeader *commandBufferHeader = (CommandBufferCommonHeader *)commandBuffer;

    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER
        if (
            commandBufferHeader->renderPass.inProgress ||
            commandBufferHeader->computePass.inProgress ||
            commandBufferHeader->copyPass.inProgress) {
            SDL_assert_release(!"Cannot submit command buffer while a pass is in progress!");
            return;
        }
    }

    commandBufferHeader->submitted = SDL_TRUE;

    COMMAND_BUFFER_DEVICE->Submit(
        commandBuffer);
}

SDL_GpuFence *SDL_SubmitGpuAndAcquireFence(
    SDL_GpuCommandBuffer *commandBuffer)
{
    CommandBufferCommonHeader *commandBufferHeader = (CommandBufferCommonHeader *)commandBuffer;

    if (commandBuffer == NULL) {
        SDL_InvalidParamError("commandBuffer");
        return NULL;
    }

    if (COMMAND_BUFFER_DEVICE->debugMode) {
        CHECK_COMMAND_BUFFER_RETURN_NULL
        if (
            commandBufferHeader->renderPass.inProgress ||
            commandBufferHeader->computePass.inProgress ||
            commandBufferHeader->copyPass.inProgress) {
            SDL_assert_release(!"Cannot submit command buffer while a pass is in progress!");
            return NULL;
        }
    }

    commandBufferHeader->submitted = SDL_TRUE;

    return COMMAND_BUFFER_DEVICE->SubmitAndAcquireFence(
        commandBuffer);
}

void SDL_WaitGpu(
    SDL_GpuDevice *device)
{
    CHECK_DEVICE_MAGIC(device, );

    device->Wait(
        device->driverData);
}

void SDL_WaitGpuForFences(
    SDL_GpuDevice *device,
    SDL_bool waitAll,
    SDL_GpuFence **pFences,
    Uint32 fenceCount)
{
    CHECK_DEVICE_MAGIC(device, );
    if (pFences == NULL && fenceCount > 0) {
        SDL_InvalidParamError("pFences");
        return;
    }

    device->WaitForFences(
        device->driverData,
        waitAll,
        pFences,
        fenceCount);
}

SDL_bool SDL_QueryGpuFence(
    SDL_GpuDevice *device,
    SDL_GpuFence *fence)
{
    CHECK_DEVICE_MAGIC(device, SDL_FALSE);
    if (fence == NULL) {
        SDL_InvalidParamError("fence");
        return SDL_FALSE;
    }

    return device->QueryFence(
        device->driverData,
        fence);
}

void SDL_ReleaseGpuFence(
    SDL_GpuDevice *device,
    SDL_GpuFence *fence)
{
    CHECK_DEVICE_MAGIC(device, );
    if (fence == NULL) {
        return;
    }

    device->ReleaseFence(
        device->driverData,
        fence);
}
