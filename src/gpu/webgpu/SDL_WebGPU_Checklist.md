# SDL WebGPU Driver Roadmap / Checklist

These methods are tested by the example suite:
- https://github.com/klukaszek/SDL3-WebGPU-Examples

Getting my fork of the example suite merged with the original shouldn't be too much work but it needs to tested first.

The original example suite can be found at:
- https://github.com/TheSpydog/SDL_gpu_examples/

Currently, the WebGPU backend only supports Emscripten as the compilation target, though I've added Elie Michel's cross-platform surface configuration logic to the backend. I have not tested it with any native implementations however. 

# Checklist

## General
- [x] DestroyDevice
- [x] SupportsPresentMode
- [x] ClaimWindow
- [x] ReleaseWindow

## Swapchains
- [x] SetSwapchainParameters
- [x] SupportsTextureFormat
- [x] SupportsSampleCount
- [x] SupportsSwapchainComposition

## Command Buffers and Fences
- [x] AcquireCommandBuffer
- [x] AcquireSwapchainTexture
- [x] GetSwapchainTextureFormat
- [x] Submit
- [x] SubmitAndAcquireFence (Should just call Submit)
- [x] Cancel (Should be no-op for WebGPU)
- [x] Wait (Should be no-op for WebGPU)
- [x] WaitForFences (Should be no-op for WebGPU)
- [x] QueryFence (Should be no-op for WebGPU)
- [x] ReleaseFence (Should be no-op for WebGPU)

Note: WebGPU has no exposed fence API.

## Buffers
- [x] CreateBuffer
- [x] ReleaseBuffer
- [x] SetBufferName
- [x] CreateTransferBuffer
- [x] ReleaseTransferBuffer
- [x] MapTransferBuffer
- [x] UnmapTransferBuffer
- [x] UploadToBuffer
- [x] DownloadFromBuffer
- [x] CopyBufferToBuffer

## Textures
- [x] CreateTexture
- [x] ReleaseTexture
- [x] SetTextureName
- [x] UploadToTexture
- [x] DownloadFromTexture (needs to be tested)
- [x] CopyTextureToTexture (needs to be tested)
- [ ] GenerateMipmaps
    - Requires custom compute pipeline implementation.
    - https://eliemichel.github.io/LearnWebGPU/basic-compute/image-processing/mipmap-generation.html

## Samplers
- [x] CreateSampler
- [x] ReleaseSampler

## Debugging
- [x] InsertDebugLabel
- [x] PushDebugGroup
- [x] PopDebugGroup

## Graphics Pipelines
- [x] CreateGraphicsPipeline
- [x] BindGraphicsPipeline
- [x] ReleaseGraphicsPipeline

## Compute Pipelines
- [ ] CreateComputePipeline
- [ ] BindComputePipeline
- [ ] ReleaseComputePipeline

## Shaders
- [x] CreateShader
- [x] ReleaseShader

## Rendering
- [x] BeginRenderPass
- [x] EndRenderPass
- [x] DrawPrimitivesIndirect
- [x] DrawPrimitives
- [x] DrawIndexedPrimitives
- [x] DrawIndexedPrimitivesIndirect

## Copy Passes
- [x] BeginCopyPass
- [x] EndCopyPass

## Compute Passes
- [ ] BeginComputePass
- [ ] EndComputePass
- [ ] DispatchCompute
- [ ] DispatchComputeIndirect
- [ ] BindComputeSamplers
- [ ] BindComputeStorageTextures
- [ ] BindComputeStorageBuffers
- [ ] PushComputeUniformData

## Fragment Stage
- [x] BindFragmentSamplers
- [ ] BindFragmentStorageTextures
- [ ] BindFragmentStorageBuffers
- [ ] PushFragmentUniformData
    - Needs to be rewritten.

## Vertex Stage
- [x] BindVertexBuffers
- [x] BindIndexBuffer
- [x] BindVertexSamplers
- [ ] BindVertexStorageTextures
- [ ] BindVertexStorageBuffers
- [ ] PushVertexUniformData
    - Needs to be rewritten.

## Rendering States
- [x] SetViewport
- [x] SetScissor
- [x] SetBlendConstants
- [x] SetStencilReference

## Composition
- [ ] Blit 
    - Mostly functional.
    - Bug: Example "Blit2DArray.c" has a sampler issue where the RHS is not downsampled.
    - Bug: Example "TriangleMSAA.c" does not cycle between different sample counts.
    
