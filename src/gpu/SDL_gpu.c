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
#include "SDL_gpu_driver.h"

#define NULL_RETURN(name) if (name == NULL) { return; }
#define NULL_RETURN_NULL(name) if (name == NULL) { return NULL; }

/* Drivers */

#if SDL_GPU_VULKAN
	#define SDL_GPU_VULKAN_DRIVER &VulkanDriver
#else
	#define SDL_GPU_VULKAN_DRIVER NULL
#endif

#if SDL_GPU_D3D11
	#define SDL_GPU_D3D11_DRIVER &D3D11Driver
#else
	#define SDL_GPU_D3D11_DRIVER NULL
#endif

#if SDL_GPU_METAL
	#define SDL_GPU_METAL_DRIVER &MetalDriver
#else
	#define SDL_GPU_METAL_DRIVER NULL
#endif

static const SDL_GpuDriver *backends[] = {
	SDL_GPU_VULKAN_DRIVER,
	SDL_GPU_D3D11_DRIVER,
    SDL_GPU_METAL_DRIVER,
	NULL
};

/* Driver Functions */

static SDL_GpuBackend selectedBackend = SDL_GPU_BACKEND_INVALID;

SDL_GpuBackend SDL_GpuSelectBackend(
	SDL_GpuBackend *preferredBackends,
	Uint32 preferredBackendCount,
	Uint32 *flags
) {
	Uint32 i;
	SDL_GpuBackend currentPreferredBackend;

	/* Iterate the array and return if a backend successfully prepares. */

	for (i = 0; i < preferredBackendCount; i += 1)
	{
		currentPreferredBackend = preferredBackends[i];
		if (backends[currentPreferredBackend] != NULL && backends[currentPreferredBackend]->PrepareDriver(flags))
		{
			selectedBackend = currentPreferredBackend;
			return currentPreferredBackend;
		}
	}

	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No supported SDL_Gpu backend found!");
	return SDL_GPU_BACKEND_INVALID;
}

SDL_GpuDevice* SDL_GpuCreateDevice(
	Uint8 debugMode
) {
	if (selectedBackend == SDL_GPU_BACKEND_INVALID)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Invalid backend selection. Did you call SDL_GpuSelectBackend?");
		return NULL;
	}

	return backends[selectedBackend]->CreateDevice(
		debugMode
	);
}

void SDL_GpuDestroyDevice(SDL_GpuDevice *device)
{
	NULL_RETURN(device);
	device->DestroyDevice(device);
}

/* State Creation */

SDL_GpuComputePipeline* SDL_GpuCreateComputePipeline(
	SDL_GpuDevice *device,
	SDL_GpuComputeShaderInfo *computeShaderInfo
) {
	NULL_RETURN_NULL(device);
	return device->CreateComputePipeline(
		device->driverData,
		computeShaderInfo
	);
}

SDL_GpuGraphicsPipeline* SDL_GpuCreateGraphicsPipeline(
	SDL_GpuDevice *device,
	SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo
) {
	NULL_RETURN_NULL(device);
	return device->CreateGraphicsPipeline(
		device->driverData,
		pipelineCreateInfo
	);
}

SDL_GpuSampler* SDL_GpuCreateSampler(
	SDL_GpuDevice *device,
	SDL_GpuSamplerStateCreateInfo *samplerStateCreateInfo
) {
	NULL_RETURN_NULL(device);
	return device->CreateSampler(
		device->driverData,
		samplerStateCreateInfo
	);
}

SDL_GpuShaderModule* SDL_GpuCreateShaderModule(
    SDL_GpuDevice *device,
    SDL_GpuShaderModuleCreateInfo *shaderModuleCreateInfo
) {
    return device->CreateShaderModule(
        device->driverData,
        shaderModuleCreateInfo
    );
}

SDL_GpuTexture* SDL_GpuCreateTexture(
	SDL_GpuDevice *device,
	SDL_GpuTextureCreateInfo *textureCreateInfo
) {
	NULL_RETURN_NULL(device);
	return device->CreateTexture(
		device->driverData,
		textureCreateInfo
	);
}

SDL_GpuBuffer* SDL_GpuCreateGpuBuffer(
	SDL_GpuDevice *device,
	SDL_GpuBufferUsageFlags usageFlags,
	Uint32 sizeInBytes
) {
	NULL_RETURN_NULL(device);
	return device->CreateGpuBuffer(
		device->driverData,
		usageFlags,
		sizeInBytes
	);
}

SDL_GpuTransferBuffer* SDL_GpuCreateTransferBuffer(
	SDL_GpuDevice *device,
	SDL_GpuTransferUsage usage,
	Uint32 sizeInBytes
) {
	NULL_RETURN_NULL(device);
	return device->CreateTransferBuffer(
		device->driverData,
		usage,
		sizeInBytes
	);
}

/* Debug Naming */

void SDL_GpuSetGpuBufferName(
	SDL_GpuDevice *device,
	SDL_GpuBuffer *buffer,
	const char *text
) {
	NULL_RETURN(device);
	NULL_RETURN(buffer);

	device->SetGpuBufferName(
		device->driverData,
		buffer,
		text
	);
}

void SDL_GpuSetTextureName(
	SDL_GpuDevice *device,
	SDL_GpuTexture *texture,
	const char *text
) {
	NULL_RETURN(device);
	NULL_RETURN(texture);

	device->SetTextureName(
		device->driverData,
		texture,
		text
	);
}

/* Disposal */

void SDL_GpuQueueDestroyTexture(
	SDL_GpuDevice *device,
	SDL_GpuTexture *texture
) {
	NULL_RETURN(device);
	device->QueueDestroyTexture(
		device->driverData,
		texture
	);
}

void SDL_GpuQueueDestroySampler(
	SDL_GpuDevice *device,
	SDL_GpuSampler *sampler
) {
	NULL_RETURN(device);
	device->QueueDestroySampler(
		device->driverData,
		sampler
	);
}

void SDL_GpuQueueDestroyGpuBuffer(
	SDL_GpuDevice *device,
	SDL_GpuBuffer *gpuBuffer
) {
	NULL_RETURN(device);
	device->QueueDestroyGpuBuffer(
		device->driverData,
		gpuBuffer
	);
}

void SDL_GpuQueueDestroyTransferBuffer(
	SDL_GpuDevice *device,
	SDL_GpuTransferBuffer *transferBuffer
) {
	NULL_RETURN(device);
	device->QueueDestroyTransferBuffer(
		device->driverData,
		transferBuffer
	);
}

void SDL_GpuQueueDestroyShaderModule(
	SDL_GpuDevice *device,
	SDL_GpuShaderModule *shaderModule
) {
	NULL_RETURN(device);
	device->QueueDestroyShaderModule(
		device->driverData,
		shaderModule
	);
}

void SDL_GpuQueueDestroyComputePipeline(
	SDL_GpuDevice *device,
	SDL_GpuComputePipeline *computePipeline
) {
	NULL_RETURN(device);
	device->QueueDestroyComputePipeline(
		device->driverData,
		computePipeline
	);
}

void SDL_GpuQueueDestroyGraphicsPipeline(
	SDL_GpuDevice *device,
	SDL_GpuGraphicsPipeline *graphicsPipeline
) {
	NULL_RETURN(device);
	device->QueueDestroyGraphicsPipeline(
		device->driverData,
		graphicsPipeline
	);
}

/* Render Pass */

void SDL_GpuBeginRenderPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
	Uint32 colorAttachmentCount,
	SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo
) {
	NULL_RETURN(device);
	device->BeginRenderPass(
		device->driverData,
		commandBuffer,
		colorAttachmentInfos,
		colorAttachmentCount,
		depthStencilAttachmentInfo
	);
}

void SDL_GpuBindGraphicsPipeline(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuGraphicsPipeline *graphicsPipeline
) {
	NULL_RETURN(device);
	device->BindGraphicsPipeline(
		device->driverData,
		commandBuffer,
		graphicsPipeline
	);
}

void SDL_GpuSetViewport(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuViewport *viewport
) {
	NULL_RETURN(device)
	device->SetViewport(
		device->driverData,
		commandBuffer,
		viewport
	);
}

void SDL_GpuSetScissor(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuRect *scissor
) {
	NULL_RETURN(device)
	device->SetScissor(
		device->driverData,
		commandBuffer,
		scissor
	);
}

void SDL_GpuBindVertexBuffers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 firstBinding,
	Uint32 bindingCount,
	SDL_GpuBufferBinding *pBindings
) {
	NULL_RETURN(device);
	device->BindVertexBuffers(
		device->driverData,
		commandBuffer,
		firstBinding,
		bindingCount,
		pBindings
	);
}

void SDL_GpuBindIndexBuffer(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBufferBinding *pBinding,
	SDL_GpuIndexElementSize indexElementSize
) {
	NULL_RETURN(device);
	device->BindIndexBuffer(
		device->driverData,
		commandBuffer,
		pBinding,
		indexElementSize
	);
}

void SDL_GpuBindVertexSamplers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
) {
	NULL_RETURN(device);
	device->BindVertexSamplers(
		device->driverData,
		commandBuffer,
		pBindings
	);
}

void SDL_GpuBindFragmentSamplers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
) {
	NULL_RETURN(device);
	device->BindFragmentSamplers(
		device->driverData,
		commandBuffer,
		pBindings
	);
}

void SDL_GpuPushVertexShaderUniforms(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	NULL_RETURN(device);
	device->PushVertexShaderUniforms(
		device->driverData,
		commandBuffer,
		data,
		dataLengthInBytes
	);
}

void SDL_GpuPushFragmentShaderUniforms(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	NULL_RETURN(device);
	device->PushFragmentShaderUniforms(
		device->driverData,
		commandBuffer,
		data,
		dataLengthInBytes
	);
}

void SDL_GpuDrawInstancedPrimitives(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 baseVertex,
	Uint32 startIndex,
	Uint32 primitiveCount,
	Uint32 instanceCount
) {
	NULL_RETURN(device);
	device->DrawInstancedPrimitives(
		device->driverData,
		commandBuffer,
		baseVertex,
		startIndex,
		primitiveCount,
		instanceCount
	);
}

void SDL_GpuDrawPrimitives(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 vertexStart,
	Uint32 primitiveCount
) {
	NULL_RETURN(device);
	device->DrawPrimitives(
		device->driverData,
		commandBuffer,
		vertexStart,
		primitiveCount
	);
}

void SDL_GpuDrawPrimitivesIndirect(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *gpuBuffer,
	Uint32 offsetInBytes,
	Uint32 drawCount,
	Uint32 stride
) {
	NULL_RETURN(device);
	device->DrawPrimitivesIndirect(
		device->driverData,
		commandBuffer,
		gpuBuffer,
		offsetInBytes,
		drawCount,
		stride
	);
}

void SDL_GpuEndRenderPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NULL_RETURN(device);
	device->EndRenderPass(
		device->driverData,
		commandBuffer
	);
}

/* Compute Pass */

void SDL_GpuBeginComputePass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NULL_RETURN(device);
	device->BeginComputePass(
		device->driverData,
		commandBuffer
	);
}

void SDL_GpuBindComputePipeline(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputePipeline *computePipeline
) {
	NULL_RETURN(device);
	device->BindComputePipeline(
		device->driverData,
		commandBuffer,
		computePipeline
	);
}

void SDL_GpuBindComputeBuffers(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeBufferBinding *pBindings
) {
	NULL_RETURN(device);
	device->BindComputeBuffers(
		device->driverData,
		commandBuffer,
		pBindings
	);
}

void SDL_GpuBindComputeTextures(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeTextureBinding *pBindings
) {
	NULL_RETURN(device);
	device->BindComputeTextures(
		device->driverData,
		commandBuffer,
		pBindings
	);
}

void SDL_GpuPushComputeShaderUniforms(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	NULL_RETURN(device);
	device->PushComputeShaderUniforms(
		device->driverData,
		commandBuffer,
		data,
		dataLengthInBytes
	);
}

void SDL_GpuDispatchCompute(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 groupCountX,
	Uint32 groupCountY,
	Uint32 groupCountZ
) {
	NULL_RETURN(device);
	device->DispatchCompute(
		device->driverData,
		commandBuffer,
		groupCountX,
		groupCountY,
		groupCountZ
	);
}

void SDL_GpuEndComputePass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NULL_RETURN(device);
	device->EndComputePass(
		device->driverData,
		commandBuffer
	);
}

/* TransferBuffer Set/Get */

void SDL_GpuSetTransferData(
	SDL_GpuDevice *device,
	void* data,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuTransferOptions transferOption
) {
	NULL_RETURN(device);
	device->SetTransferData(
		device->driverData,
		data,
		transferBuffer,
		copyParams,
		transferOption
	);
}

void SDL_GpuGetTransferData(
	SDL_GpuDevice *device,
	SDL_GpuTransferBuffer *transferBuffer,
	void* data,
	SDL_GpuBufferCopy *copyParams
) {
	NULL_RETURN(device);
	device->GetTransferData(
		device->driverData,
		transferBuffer,
		data,
		copyParams
	);
}

/* Copy Pass */

void SDL_GpuBeginCopyPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NULL_RETURN(device);
	device->BeginCopyPass(
		device->driverData,
		commandBuffer
	);
}

void SDL_GpuUploadToTexture(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuTextureRegion *textureRegion,
	SDL_GpuBufferImageCopy *copyParams,
	SDL_GpuTextureWriteOptions writeOption
) {
	NULL_RETURN(device);
	device->UploadToTexture(
		device->driverData,
		commandBuffer,
		transferBuffer,
		textureRegion,
		copyParams,
		writeOption
	);
}

void SDL_GpuUploadToBuffer(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBuffer *gpuBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions writeOption
) {
	NULL_RETURN(device);
	device->UploadToBuffer(
		device->driverData,
		commandBuffer,
		transferBuffer,
		gpuBuffer,
		copyParams,
		writeOption
	);
}

void SDL_GpuCopyTextureToTexture(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureRegion *source,
	SDL_GpuTextureRegion *destination,
	SDL_GpuTextureWriteOptions writeOption
) {
	NULL_RETURN(device);
	device->CopyTextureToTexture(
		device->driverData,
		commandBuffer,
		source,
		destination,
		writeOption
	);
}

void SDL_GpuCopyBufferToBuffer(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *source,
	SDL_GpuBuffer *destination,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions writeOption
) {
	NULL_RETURN(device);
	device->CopyBufferToBuffer(
		device->driverData,
		commandBuffer,
		source,
		destination,
		copyParams,
		writeOption
	);
}

void SDL_GpuGenerateMipmaps(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTexture *texture
) {
	NULL_RETURN(device);
	device->GenerateMipmaps(
		device->driverData,
		commandBuffer,
		texture
	);
}

void SDL_GpuEndCopyPass(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NULL_RETURN(device);
	device->EndCopyPass(
		device->driverData,
		commandBuffer
	);
}

/* Submission/Presentation */

Uint8 SDL_GpuClaimWindow(
	SDL_GpuDevice *device,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
) {
	if (device == NULL) { return 0; }
	return device->ClaimWindow(
		device->driverData,
		windowHandle,
		presentMode
	);
}

void SDL_GpuUnclaimWindow(
	SDL_GpuDevice *device,
	void *windowHandle
) {
	NULL_RETURN(device);
	device->UnclaimWindow(
		device->driverData,
		windowHandle
	);
}

void SDL_GpuSetSwapchainPresentMode(
	SDL_GpuDevice *device,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
) {
	NULL_RETURN(device);
	device->SetSwapchainPresentMode(
		device->driverData,
		windowHandle,
		presentMode
	);
}

SDL_GpuTextureFormat SDL_GpuGetSwapchainFormat(
	SDL_GpuDevice *device,
	void *windowHandle
) {
	if (device == NULL) { return 0; }
	return device->GetSwapchainFormat(
		device->driverData,
		windowHandle
	);
}

SDL_GpuCommandBuffer* SDL_GpuAcquireCommandBuffer(
	SDL_GpuDevice *device
) {
	NULL_RETURN_NULL(device);
	return device->AcquireCommandBuffer(
		device->driverData
	);
}

SDL_GpuTexture* SDL_GpuAcquireSwapchainTexture(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer,
	void *windowHandle,
	Uint32 *pWidth,
	Uint32 *pHeight
) {
	NULL_RETURN_NULL(device);
	return device->AcquireSwapchainTexture(
		device->driverData,
		commandBuffer,
		windowHandle,
		pWidth,
		pHeight
	);
}

void SDL_GpuSubmit(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NULL_RETURN(device);
	device->Submit(
		device->driverData,
		commandBuffer
	);
}

SDL_GpuFence* SDL_GpuSubmitAndAcquireFence(
	SDL_GpuDevice *device,
	SDL_GpuCommandBuffer *commandBuffer
) {
	NULL_RETURN_NULL(device);
	return device->SubmitAndAcquireFence(
		device->driverData,
		commandBuffer
	);
}

void SDL_GpuWait(
	SDL_GpuDevice *device
) {
	NULL_RETURN(device);
	device->Wait(
		device->driverData
	);
}

void SDL_GpuWaitForFences(
	SDL_GpuDevice *device,
	Uint8 waitAll,
	Uint32 fenceCount,
	SDL_GpuFence **pFences
) {
	NULL_RETURN(device);
	device->WaitForFences(
		device->driverData,
		waitAll,
		fenceCount,
		pFences
	);
}

int SDL_GpuQueryFence(
	SDL_GpuDevice *device,
	SDL_GpuFence *fence
) {
	if (device == NULL) { return 0; }

	return device->QueryFence(
		device->driverData,
		fence
	);
}

void SDL_GpuReleaseFence(
	SDL_GpuDevice *device,
	SDL_GpuFence *fence
) {
	NULL_RETURN(device);
	device->ReleaseFence(
		device->driverData,
		fence
	);
}

void SDL_GpuDownloadFromTexture(
	SDL_GpuDevice *device,
	SDL_GpuTextureRegion *textureRegion,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferImageCopy *copyParams,
	SDL_GpuTransferOptions transferOption
) {
	NULL_RETURN(device);
	device->DownloadFromTexture(
		device->driverData,
		textureRegion,
		transferBuffer,
		copyParams,
		transferOption
	);
}

void SDL_GpuDownloadFromBuffer(
	SDL_GpuDevice *device,
	SDL_GpuBuffer *gpuBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuTransferOptions transferOption
) {
	NULL_RETURN(device);
	device->DownloadFromBuffer(
		device->driverData,
		gpuBuffer,
		transferBuffer,
		copyParams,
		transferOption
	);
}
