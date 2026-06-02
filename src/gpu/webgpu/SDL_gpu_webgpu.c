// NOTE:
// This was developed using the Vulkan backend as a guide.
// Also, to slouken, cosmonaut, or anybody else who's gonna have to read through this code: I'm sorry.
// I've seen ancient Egyptian hieroglyphics that were easier to read than this code.

// TODO:
// Buffer cycling!

// Current implementation checklist
//
// FUNCTIONS:
// SDL_AcquireGPUCommandBuffer [x]
// SDL_AcquireGPUSwapchainTexture [x]
// SDL_BeginGPUComputePass []
// SDL_BeginGPUCopyPass [x]
// SDL_BeginGPURenderPass []
// SDL_BindGPUComputePipeline []
// SDL_BindGPUComputeSamplers []
// SDL_BindGPUComputeStorageBuffers []
// SDL_BindGPUComputeStorageTextures []
// SDL_BindGPUFragmentSamplers []
// SDL_BindGPUFragmentStorageBuffers []
// SDL_BindGPUFragmentStorageTextures []
// SDL_BindGPUGraphicsPipeline []
// SDL_BindGPUIndexBuffer []
// SDL_BindGPUVertexBuffers []
// SDL_BindGPUVertexSamplers []
// SDL_BindGPUVertexStorageBuffers []
// SDL_BindGPUVertexStorageTextures []
// SDL_BlitGPUTexture []
// SDL_CancelGPUCommandBuffer [] (See the comment in WEBGPU_AcquireGPUCommandBuffer.)
// SDL_ClaimWindowForGPUDevice [x]
// SDL_CopyGPUBufferToBuffer [x]
// SDL_CopyGPUTextureToTexture []
// SDL_CreateGPUBuffer [x]
// SDL_CreateGPUComputePipeline []
// SDL_CreateGPUDevice [x]
// SDL_CreateGPUDeviceWithProperties []
// SDL_CreateGPUGraphicsPipeline []
// SDL_CreateGPUSampler []
// SDL_CreateGPUShader []
// SDL_CreateGPUTexture [x] (Maybe. I'm so confused)
// SDL_CreateGPUTransferBuffer [x]
// SDL_DestroyGPUDevice []
// SDL_DispatchGPUCompute []
// SDL_DispatchGPUComputeIndirect []
// SDL_DownloadFromGPUBuffer []
// SDL_DownloadFromGPUTexture []
// SDL_DrawGPUIndexedPrimitives []
// SDL_DrawGPUIndexedPrimitivesIndirect []
// SDL_DrawGPUPrimitives []
// SDL_DrawGPUPrimitivesIndirect []
// SDL_EndGPUComputePass []
// SDL_EndGPUCopyPass [x]
// SDL_EndGPURenderPass []
// SDL_GDKResumeGPU [] (Don't even think GDK'll work with WebGPU)
// SDL_GDKSuspendGPU []
// SDL_GenerateMipmapsForGPUTexture []
// SDL_GetGPUDeviceProperties [x] (Sorta, kinda; The only property that's set is the device name)
// SDL_GetGPUDriver [x]
// SDL_GetGPUShaderFormats []
// SDL_GetGPUSwapchainTextureFormat []
// SDL_GetGPUTextureFormatFromPixelFormat []
// SDL_GetNumGPUDrivers [x]
// SDL_GetPixelFormatFromGPUTextureFormat []
// SDL_GPUSupportsProperties []
// SDL_GPUSupportsShaderFormats []
// SDL_GPUTextureFormatTexelBlockSize []
// SDL_GPUTextureSupportsFormat []
// SDL_GPUTextureSupportsSampleCount []
// SDL_InsertGPUDebugLabel []
// SDL_MapGPUTransferBuffer [x]
// SDL_PopGPUDebugGroup []
// SDL_PushGPUComputeUniformData []
// SDL_PushGPUDebugGroup []
// SDL_PushGPUFragmentUniformData []
// SDL_PushGPUVertexUniformData []
// SDL_QueryGPUFence [x]
// SDL_ReleaseGPUBuffer [x]
// SDL_ReleaseGPUComputePipeline []
// SDL_ReleaseGPUFence [x]
// SDL_ReleaseGPUGraphicsPipeline []
// SDL_ReleaseGPUSampler []
// SDL_ReleaseGPUShader []
// SDL_ReleaseGPUTexture [x]
// SDL_ReleaseGPUTransferBuffer [x]
// SDL_ReleaseWindowFromGPUDevice []
// SDL_SetGPUAllowedFramesInFlight []
// SDL_SetGPUBlendConstants []
// SDL_SetGPUBufferName []
// SDL_SetGPUScissor []
// SDL_SetGPUStencilReference []
// SDL_SetGPUSwapchainParameters []
// SDL_SetGPUTextureName []
// SDL_SetGPUViewport []
// SDL_SubmitGPUCommandBuffer [x]
// SDL_SubmitGPUCommandBufferAndAcquireFence [x]
// SDL_UnmapGPUTransferBuffer [x]
// SDL_UploadToGPUBuffer []
// SDL_UploadToGPUTexture []
// SDL_WaitAndAcquireGPUSwapchainTexture []
// SDL_WaitForGPUFences []
// SDL_WaitForGPUIdle []
// SDL_WaitForGPUSwapchain []
// SDL_WindowSupportsGPUPresentMode []
// SDL_WindowSupportsGPUSwapchainComposition []

#include "SDL_internal.h"

// FIXME: Temporarily here so that I'm not coding blind
#define SDL_GPU_WEBGPU 1

#ifdef SDL_GPU_WEBGPU

#include "../SDL_sysgpu.h"
#include "webgpu.h"

// FIXME: WebGPU supports more modes than SDL_GPU does.
// More specifically, it supports "FifoRelaxed" on AMD cards.
// We won't be using that.
static WGPUPresentMode SDLToWebGPU_PresentMode[] = {
    WGPUPresentMode_Fifo,
    WGPUPresentMode_Immediate,
    WGPUPresentMode_Mailbox,
};

static WGPUTextureFormat SDLToWebGPU_TextureFormat[] = {
    WGPUTextureFormat_Undefined,            // INVALID
    WGPUTextureFormat_Undefined,            // A8_UNORM, no such format in webgpu.h (i think?)
    WGPUTextureFormat_R8Unorm,              // R8_UNORM
    WGPUTextureFormat_RG8Unorm,             // R8G8_UNORM
    WGPUTextureFormat_RGBA8Unorm,           // R8G8B8A8_UNORM
    WGPUTextureFormat_R16Unorm,             // R16_UNORM
    WGPUTextureFormat_RG16Unorm,            // R16G16_UNORM
    WGPUTextureFormat_RGBA16Unorm,          // R16G16B16A16_UNORM
    WGPUTextureFormat_RGB10A2Unorm,         // R10G10B10A2_UNORM
    WGPUTextureFormat_Undefined,            // B5G6R5_UNORM (?)
    WGPUTextureFormat_Undefined,            // B5G5R5A1_UNORM (?)
    WGPUTextureFormat_Undefined,            // B4G4R4A4_UNORM (?)
    WGPUTextureFormat_BGRA8Unorm,           // B8G8R8A8_UNORM
    WGPUTextureFormat_BC1RGBAUnorm,         // BC1_UNORM
    WGPUTextureFormat_BC2RGBAUnorm,         // BC2_UNORM
    WGPUTextureFormat_BC3RGBAUnorm,         // BC3_UNORM
    WGPUTextureFormat_BC4RUnorm,            // BC4_UNORM (?)
    WGPUTextureFormat_BC5RGUnorm,           // BC5_UNORM
    WGPUTextureFormat_BC7RGBAUnorm,         // BC7_UNORM
    WGPUTextureFormat_BC6HRGBFloat,         // BC6H_FLOAT
    WGPUTextureFormat_BC6HRGBUfloat,        // BC6H_UFLOAT
    WGPUTextureFormat_R8Snorm,              // R8_SNORM
    WGPUTextureFormat_RG8Snorm,             // R8G8_SNORM
    WGPUTextureFormat_RGBA8Snorm,           // R8G8B8A8_SNORM
    WGPUTextureFormat_R16Snorm,             // R16_SNORM
    WGPUTextureFormat_RG16Snorm,            // R16G16_SNORM
    WGPUTextureFormat_RGBA16Snorm,          // R16G16B16A16_SNORM
    WGPUTextureFormat_R16Float,             // R16_FLOAT
    WGPUTextureFormat_RG16Float,            // R16G16_FLOAT
    WGPUTextureFormat_RGBA16Float,          // R16G16B16A16_FLOAT
    WGPUTextureFormat_R32Float,             // R32_FLOAT
    WGPUTextureFormat_RG32Float,            // R32G32_FLOAT
    WGPUTextureFormat_RGBA32Float,          // R32G32B32A32_FLOAT
    WGPUTextureFormat_RG11B10Ufloat,        // R11G11B10_UFLOAT
    WGPUTextureFormat_R8Uint,               // R8_UINT
    WGPUTextureFormat_RG8Uint,              // R8G8_UINT
    WGPUTextureFormat_RGBA8Uint,            // R8G8B8A8_UINT
    WGPUTextureFormat_R16Uint,              // R16_UINT
    WGPUTextureFormat_RG16Uint,             // R16G16_UINT
    WGPUTextureFormat_RGBA16Uint,           // R16G16B16A16_UINT
    WGPUTextureFormat_R32Sint,              // R32_UINT
    WGPUTextureFormat_RG32Uint,             // R32G32_UINT
    WGPUTextureFormat_RGBA32Uint,           // R32G32B32A32_UINT
    WGPUTextureFormat_R8Sint,               // R8_INT
    WGPUTextureFormat_RG8Sint,              // R8G8_INT
    WGPUTextureFormat_RGBA8Sint,            // R8G8B8A8_INT
    WGPUTextureFormat_R16Sint,              // R16_INT
    WGPUTextureFormat_RG16Sint,             // R16G16_INT
    WGPUTextureFormat_RGBA16Sint,           // R16G16B16A16_INT
    WGPUTextureFormat_R32Sint,              // R32_INT
    WGPUTextureFormat_RG32Sint,             // R32G32_INT
    WGPUTextureFormat_RGBA32Sint,           // R32G32B32A32_INT
    WGPUTextureFormat_RGBA8UnormSrgb,       // R8G8B8A8_UNORM_SRGB
    WGPUTextureFormat_BGRA8UnormSrgb,       // B8G8R8A8_UNORM_SRGB
    WGPUTextureFormat_BC1RGBAUnormSrgb,     // BC1_UNORM_SRGB
    WGPUTextureFormat_BC2RGBAUnormSrgb,     // BC2_UNORM_SRGB
    WGPUTextureFormat_BC3RGBAUnormSrgb,     // BC3_UNORM_SRGB
    WGPUTextureFormat_BC7RGBAUnormSrgb,     // BC7_UNORM_SRGB
    WGPUTextureFormat_Depth16Unorm,         // D16_UNORM
    WGPUTextureFormat_Depth24Plus,          // D24_UNORM, pretty sure this requires a feature
    WGPUTextureFormat_Depth32Float,         // D32_FLOAT
    WGPUTextureFormat_Depth24PlusStencil8,  // D24_UNORM_S8_UINT
    WGPUTextureFormat_Depth32FloatStencil8, // D32_FLOAT_S8_UINT, needs WGPUTextureFeature_Depth32FloatStencil8
    WGPUTextureFormat_ASTC4x4Unorm,         // ASTC_4x4_UNORM
    WGPUTextureFormat_ASTC5x4Unorm,         // ASTC_5x4_UNORM
    WGPUTextureFormat_ASTC5x5Unorm,         // ASTC_5x5_UNORM
    WGPUTextureFormat_ASTC6x5Unorm,         // ASTC_6x5_UNORM
    WGPUTextureFormat_ASTC6x6Unorm,         // ASTC_6x6_UNORM
    WGPUTextureFormat_ASTC8x5Unorm,         // ASTC_8x5_UNORM
    WGPUTextureFormat_ASTC8x6Unorm,         // ASTC_8x6_UNORM
    WGPUTextureFormat_ASTC8x8Unorm,         // ASTC_8x8_UNORM
    WGPUTextureFormat_ASTC10x5Unorm,        // ASTC_10x5_UNORM
    WGPUTextureFormat_ASTC10x6Unorm,        // ASTC_10x6_UNORM
    WGPUTextureFormat_ASTC10x8Unorm,        // ASTC_10x8_UNORM
    WGPUTextureFormat_ASTC10x10Unorm,       // ASTC_10x10_UNORM
    WGPUTextureFormat_ASTC12x10Unorm,       // ASTC_12x10_UNORM
    WGPUTextureFormat_ASTC12x12Unorm,       // ASTC_12x12_UNORM
    WGPUTextureFormat_ASTC4x4UnormSrgb,     // ASTC_4x4_UNORM_SRGB
    WGPUTextureFormat_ASTC5x4UnormSrgb,     // ASTC_5x4_UNORM_SRGB
    WGPUTextureFormat_ASTC5x5UnormSrgb,     // ASTC_5x5_UNORM_SRGB
    WGPUTextureFormat_ASTC6x5UnormSrgb,     // ASTC_6x5_UNORM_SRGB
    WGPUTextureFormat_ASTC6x6UnormSrgb,     // ASTC_6x6_UNORM_SRGB
    WGPUTextureFormat_ASTC8x5UnormSrgb,     // ASTC_8x5_UNORM_SRGB
    WGPUTextureFormat_ASTC8x6UnormSrgb,     // ASTC_8x6_UNORM_SRGB
    WGPUTextureFormat_ASTC8x8UnormSrgb,     // ASTC_8x8_UNORM_SRGB
    WGPUTextureFormat_ASTC10x5UnormSrgb,    // ASTC_10x5_UNORM_SRGB
    WGPUTextureFormat_ASTC10x6UnormSrgb,    // ASTC_10x6_UNORM_SRGB
    WGPUTextureFormat_ASTC10x8UnormSrgb,    // ASTC_10x8_UNORM_SRGB
    WGPUTextureFormat_ASTC10x10UnormSrgb,   // ASTC_10x10_UNORM_SRGB
    WGPUTextureFormat_ASTC12x10UnormSrgb,   // ASTC_12x10_UNORM_SRGB
    WGPUTextureFormat_ASTC12x12UnormSrgb,   // ASTC_12x12_UNORM_SRGB
    WGPUTextureFormat_Undefined,            // ASTC_4x4_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_5x4_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_5x5_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_6x5_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_6x6_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_8x5_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_8x6_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_8x8_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_10x5_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_10x6_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_10x8_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_10x10_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_12x10_FLOAT
    WGPUTextureFormat_Undefined,            // ASTC_12x12_FLOAT
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_TextureFormat, SDL_arraysize(SDLToWebGPU_TextureFormat) == SDL_GPU_TEXTUREFORMAT_MAX_ENUM_VALUE);

static WGPUTextureComponentSwizzle SwizzleForSDLFormat(SDL_GPUTextureFormat format)
{
    // TODO: I have never swizzled a texture in my life. -- TheStickmahn
    // *insert moe catgirl mleming here or something*
    // (↑ oh god what am I doing with my life)
}

static WGPUTextureFormat SwapchainCompositionToFormat[] = {
    WGPUTextureFormat_BGRA8Unorm,     // SDR
    WGPUTextureFormat_BGRA8UnormSrgb, // SDR_LINEAR
    WGPUTextureFormat_RGBA16Float,    // HDR_EXTENDED_LINEAR
    WGPUTextureFormat_RGB10A2Unorm    // HDR10_ST2084
};

static WGPUTextureFormat SwapchainCompositionToFallbackFormat[] = {
    // NOTE: If I remember correctly, B8G8R8A8_UNORM and it's SRGB counterpart are always supported in WebGPU.
    // Maybe. I have bad memory.
    WGPUTextureFormat_BGRA8Unorm,     // SDR
    WGPUTextureFormat_BGRA8UnormSrgb, // SDR_LINEAR
    WGPUTextureFormat_Undefined,      // HDR_EXTENDED_LINEAR (no fallback)
    WGPUTextureFormat_Undefined       // HDR10_ST2084 (no fallback)
};

static SDL_GPUTextureFormat SwapchainCompositionToSDLFormat(
    SDL_GPUSwapchainComposition composition,
    bool usingFallback)
{
    switch (composition) {
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR:
        return usingFallback ? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR:
        return usingFallback ? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB;
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084:
        return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
    default:
        return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

// FIXME: I have no idea what these "NORM" values are?
static WGPUVertexFormat SDLToWebGPU_VertexFormat[] = {
    0,                          // INVALID
    WGPUVertexFormat_Sint32,    // INT
    WGPUVertexFormat_Sint32x2,  // INT2
    WGPUVertexFormat_Sint32x3,  // INT3
    WGPUVertexFormat_Sint32x4,  // INT4
    WGPUVertexFormat_Uint32,    // UINT
    WGPUVertexFormat_Uint32x2,  // UINT2
    WGPUVertexFormat_Uint32x3,  // UINT3
    WGPUVertexFormat_Uint32x4,  // UINT4
    WGPUVertexFormat_Float32,   // FLOAT
    WGPUVertexFormat_Float32x2, // FLOAT2
    WGPUVertexFormat_Float32x3, // FLOAT3
    WGPUVertexFormat_Float32x4, // FLOAT4
    WGPUVertexFormat_Sint8x2,   // BYTE2
    WGPUVertexFormat_Sint8x4,   // BYTE4
    WGPUVertexFormat_Uint8x2,   // UBYTE2
    WGPUVertexFormat_Uint8x4,   // UBYTE4
    0,                          // BYTE2_NORM
    0,                          // BYTE4_NORM
    0,                          // UBYTE2_NORM
    0,                          // UBYTE4_NORM
    WGPUVertexFormat_Sint16x2,  // SHORT2
    WGPUVertexFormat_Sint16x4,  // SHORT4
    WGPUVertexFormat_Uint16x2,  // USHORT2
    WGPUVertexFormat_Uint16x4,  // USHORT4
    0,                          // SHORT2_NORM
    0,                          // SHORT4_NORM
    0,                          // USHORT2_NORM
    0,                          // USHORT4_NORM
    WGPUVertexFormat_Float16x2, // HALF2
    WGPUVertexFormat_Float16x4, // HALF4
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_VertexFormat, SDL_arraysize(SDLToWebGPU_VertexFormat) == SDL_GPU_VERTEXELEMENTFORMAT_MAX_ENUM_VALUE);

static WGPUIndexFormat SDLToWebGPU_IndexFormat[] = {
    WGPUIndexFormat_Uint16,
    WGPUIndexFormat_Uint32,
};

// NOTE: Line and Point requires some features I'm pretty sure.
static WGPUPrimitiveTopology SDLToWebGPU_PrimitiveType[] = {
    WGPUPrimitiveTopology_TriangleList,
    WGPUPrimitiveTopology_TriangleStrip,
    WGPUPrimitiveTopology_LineList,
    WGPUPrimitiveTopology_LineStrip,
    WGPUPrimitiveTopology_PointList,
};

static WGPUCullMode SDLToWebGPU_CullMode[] = {
    WGPUCullMode_None,
    WGPUCullMode_Front,
    WGPUCullMode_Back,
    // Back and front cull isn't supported.
    // Who uses that anyways?
    WGPUCullMode_Undefined,
};

static WGPUFrontFace SDLToWebGPU_FrontFace[] = {
    WGPUFrontFace_CCW,
    WGPUFrontFace_CW
};

static WGPUBlendFactor SDLToWebGPU_BlendFactor[] = {
    WGPUBlendFactor_Undefined,
    WGPUBlendFactor_Zero,
    WGPUBlendFactor_One,
    WGPUBlendFactor_Src,
    WGPUBlendFactor_OneMinusSrc,
    WGPUBlendFactor_Dst,
    WGPUBlendFactor_OneMinusDst,
    WGPUBlendFactor_SrcAlpha,
    WGPUBlendFactor_OneMinusSrcAlpha,
    WGPUBlendFactor_DstAlpha,
    WGPUBlendFactor_OneMinusDstAlpha,
    WGPUBlendFactor_Constant,
    WGPUBlendFactor_OneMinusConstant,
    WGPUBlendFactor_SrcAlphaSaturated,
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_BlendFactor, SDL_arraysize(SDLToWebGPU_BlendFactor) == SDL_GPU_BLENDFACTOR_MAX_ENUM_VALUE);

static WGPUBlendOperation SDLToWebGPU_BlendOp[] = {
    WGPUBlendOperation_Add, // INVALID
    WGPUBlendOperation_Add,
    WGPUBlendOperation_Subtract,
    WGPUBlendOperation_ReverseSubtract,
    WGPUBlendOperation_Min,
    WGPUBlendOperation_Max,
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_BlendOp, SDL_arraysize(SDLToWebGPU_BlendOp) == SDL_GPU_BLENDOP_MAX_ENUM_VALUE);

static WGPUCompareFunction SDLToWebGPU_CompareFunc[] = {
    WGPUCompareFunction_Never, // INVALID
    WGPUCompareFunction_Never,
    WGPUCompareFunction_Less,
    WGPUCompareFunction_Equal,
    WGPUCompareFunction_LessEqual,
    WGPUCompareFunction_Greater,
    WGPUCompareFunction_NotEqual,
    WGPUCompareFunction_GreaterEqual,
    WGPUCompareFunction_Always,
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_CompareFunc, SDL_arraysize(SDLToWebGPU_CompareFunc) == SDL_GPU_COMPAREOP_MAX_ENUM_VALUE);

static WGPUStencilOperation SDLToWebGPU_StencilOp[] = {
    WGPUStencilOperation_Keep, // INVALID
    WGPUStencilOperation_Keep,
    WGPUStencilOperation_Zero,
    WGPUStencilOperation_Replace,
    WGPUStencilOperation_IncrementClamp,
    WGPUStencilOperation_DecrementClamp,
    WGPUStencilOperation_Invert,
    WGPUStencilOperation_IncrementWrap,
    WGPUStencilOperation_DecrementWrap,
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_StencilOp, SDL_arraysize(SDLToWebGPU_StencilOp) == SDL_GPU_STENCILOP_MAX_ENUM_VALUE);

/// FIXME: WebGPU doesn't have a "Don't Care" load/store op.
/// Defaulting to Clear, although I don't know if that's smart...
static WGPULoadOp SDLToWebGPU_LoadOp[] = {
    WGPULoadOp_Load,
    WGPULoadOp_Clear,
    WGPULoadOp_Clear,
};

static WGPUStoreOp SDLToWebGPU_StoreOp[] = {
    WGPUStoreOp_Store,
    WGPUStoreOp_Discard,
    WGPUStoreOp_Discard,
    WGPUStoreOp_Store,
};

static WGPUVertexStepMode SDLToWebGPU_StepMode[] = {
    WGPUVertexStepMode_Vertex,
    WGPUVertexStepMode_Instance,
};

static WGPUFilterMode SDLToWebGPU_FilterMode[] = {
    WGPUFilterMode_Nearest,
    WGPUFilterMode_Linear
};

static WGPUMipmapFilterMode SDLToWebGPU_MipmapFilterMode[] = {
    WGPUMipmapFilterMode_Nearest,
    WGPUMipmapFilterMode_Linear,
};

static WGPUAddressMode SDLToWebGPU_AddressMode[] = {
    WGPUAddressMode_Repeat,
    WGPUAddressMode_MirrorRepeat,
    WGPUAddressMode_ClampToEdge,
};

typedef struct WebGPUTexture WebGPUTexture;
typedef struct WebGPUTextureContainer WebGPUTextureContainer;
typedef struct WebGPUMemoryAllocation WebGPUMemoryAllocation;
typedef struct WebGPUBuffer WebGPUBuffer;
typedef struct WebGPUUniformBuffer WebGPUUniformBuffer;
typedef struct WebGPUBufferContainer WebGPUBufferContainer;
typedef struct WebGPUWindowData WebGPUWindowData;

typedef struct WebGPURenderer
{
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;
    WGPUCommandEncoder commandEncoder;

    WebGPUWindowData **capturedWindows;

    // TODO: Properties
    SDL_PropertiesID props;

    bool debugMode;
    bool preferLowPower;
    bool shouldRecreateLostDevice;
    bool hasCapturedWindow;
} WebGPURenderer;

struct WebGPUWindowData
{
    SDL_Window *window;
    WebGPURenderer *renderer;

    int refcount;
    SDL_GPUSwapchainComposition swapchainComposition;
    SDL_GPUPresentMode presentMode;

    bool surfaceSuboptimal;

    // Window surface
    WGPUSurface surface;
};

typedef struct WebGPUMemoryFreeRegion
{
    WebGPUMemoryAllocation *allocation;
    uint64_t offset;
    uint64_t size;
    Uint32 allocationIndex;
    Uint32 sortedIndex;
} WebGPUMemoryFreeRegion;

typedef struct WebGPUMemoryUsedRegion
{
    WebGPUMemoryAllocation *allocation;
    uint64_t offset;
    uint64_t size;
    uint64_t resourceOffset; // differs from offset based on alignment
    uint64_t resourceSize;   // differs from size based on alignment
    uint64_t alignment;
    Uint8 isBuffer;
    union
    {
        WebGPUBuffer *webGPUBuffer;
        WebGPUTexture *webGPUTexture;
    };
} WebGPUMemoryUsedRegion;

typedef struct WebGPUMemorySubAllocator
{
    Uint32 memoryTypeIndex;
    WebGPUMemoryAllocation **allocations;
    Uint32 allocationCount;
    WebGPUMemoryFreeRegion **sortedFreeRegions;
    Uint32 sortedFreeRegionCount;
    Uint32 sortedFreeRegionCapacity;
} WebGPUMemorySubAllocator;

struct WebGPUMemoryAllocation
{
    // NOTE: There is no analogy to VkDeviceMemory in WebGPU.
    // It'll be ignored, and the memory will instead be mapped at runtime.
    // VkDeviceMemory memory;

    WebGPUMemorySubAllocator *allocator;
    uint64_t size;
    WebGPUMemoryUsedRegion **usedRegions;
    Uint32 usedRegionCount;
    Uint32 usedRegionCapacity;
    WebGPUMemoryFreeRegion **freeRegions;
    Uint32 freeRegionCount;
    Uint32 freeRegionCapacity;
    Uint8 availableForAllocation;
    uint64_t freeSpace;
    uint64_t usedSpace;
    Uint8 *mapPointer;
    SDL_Mutex *memoryLock;
    SDL_AtomicInt referenceCount; // Used to avoid defrag races
};

typedef enum WebGPUBufferType
{
    WEBGPU_BUFFER_TYPE_GPU,
    WEBGPU_BUFFER_TYPE_UNIFORM,
    WEBGPU_BUFFER_TYPE_TRANSFER
} WebGPUBufferType;

struct WebGPUBuffer
{
    WebGPUBufferContainer *container;
    Uint32 containerIndex;

    WGPUBuffer buffer;
    WebGPUMemoryUsedRegion *usedRegion;

    // Needed for uniforms and defrag
    WebGPUBufferType type;
    SDL_GPUBufferUsageFlags usage;
    uint64_t size;

    SDL_AtomicInt referenceCount;
    bool transitioned;
    bool markedForDestroy; // so that defrag doesn't double-free
    WebGPUUniformBuffer *uniformBufferForDefrag;
};

struct WebGPUBufferContainer
{
    WebGPUBuffer *activeBuffer;

    WebGPUBuffer **buffers;
    Uint32 bufferCapacity;
    Uint32 bufferCount;

    bool dedicated;
    char *debugName;
};

typedef struct WebGPUTextureSubresource
{
    WebGPUTexture *parent;
    Uint32 layer;
    Uint32 level;

    // FIXME: I am so confused.
    // Why can a subresource have multiple render target views but only one depth and compute view?
    WGPUTextureView *renderTargetViews; // One render target view per depth slice
    WGPUTextureView computeWriteView;
    WGPUTextureView depthStencilView;
} WebGPUTextureSubresource;

struct WebGPUTexture
{
    WebGPUTextureContainer *container;
    Uint32 containerIndex;

    // NOTE:
    // WebGPU really doesn't like persistently mapped memory.
    // We'll instead just map the memory OTF during R/W operations.
    //
    // WebGPUMemoryUsedRegion *usedRegion;

    WGPUTexture texture;
    WGPUTextureView fullView; // used for samplers and storage reads
    Uint32 depth;             // used for cleanup only

    WGPUTextureAspect aspect;

    // used to avoid indirection on barriers
    //
    // NOTE: Pretty sure these are redundant?
    Uint32 levelCount;
    Uint32 layerCount;
    SDL_GPUTextureType type;

    // FIXME: It'd be nice if we didn't have to have this on the texture...
    SDL_GPUTextureUsageFlags usage; // used for defrag transitions only.

    Uint32 subresourceCount;
    WebGPUTextureSubresource *subresources;

    bool markedForDestroy;  // so that defrag doesn't double-free
    bool externallyManaged; // true for XR swapchain images
    SDL_AtomicInt referenceCount;
};

struct WebGPUTextureContainer
{
    TextureCommonHeader header;

    WebGPUTexture *activeTexture;

    Uint32 textureCapacity;
    Uint32 textureCount;
    WebGPUTexture **textures;

    char *debugName;
    bool canBeCycled;
};

/// SDL_GPU's command buffers have no real counterpart in WebGPU, so I had to do this.
typedef struct WebGPUCommandBufferWrapper
{
    WGPUCommandEncoder *encoder;
    WGPUQueue *queue;
    WebGPUWindowData *windowDataGrossHack;
} WebGPUCommandBufferWrapper;

/// There are no fences in WebGPU, so this is a solution around it.
typedef struct WebGPUFence
{
    bool status;
} WebGPUFence;

static void WEBGPU_FenceCallback(WGPUQueueWorkDoneStatus status, WGPUStringView message, void *fence, void *unused)
{
    if (fence != NULL) {
        ((WebGPUFence *)fence)->status = true;
    }
}

static void
WEBGPU_UncapturedErrorCallback(WGPUDevice const *device, WGPUErrorType type, WGPUStringView message, void *debugMode, void *unused)
{
    if (debugMode) {
        // FIXME: Again, not sure if this is how I should report errors.
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU uncaptured error!\n'%s'\n(Type %X)", message.data, type);
    }
}

static SDL_PropertiesID WEBGPU_GetDeviceProperties(
    SDL_GPUDevice *device)
{
    WebGPURenderer *renderer = (WebGPURenderer *)device->driverData;
    return renderer->props;
}

// forward decl
static void WEBGPU_RequestDevice(WebGPURenderer *renderer, bool *success);

static void WEBGPU_DeviceLostCallback(WGPUDevice const *device, WGPUDeviceLostReason reason, WGPUStringView message, void *renderer, void *unused)
{
    bool debugMode = ((WebGPURenderer *)renderer)->debugMode;

    if (debugMode) {
        SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Device has been lost.");
    }

    if (((WebGPURenderer *)renderer)->shouldRecreateLostDevice) {
        // Since the device has been lost, there might be some larger issues within WebGPU.
        // We'll double check that everything's in order.

        if (((WebGPURenderer *)renderer)->instance == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU instance has been lost. It's so joever.");
            return;
        }
        if (((WebGPURenderer *)renderer)->adapter == NULL) {
            // TODO: We should just recreate the adapter if it's lost.
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Adapter is lost. It's so joever");
        }

        if (debugMode) {
            SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Attempting to recreate WebGPU device.");
        }

        WEBGPU_RequestDevice(renderer, NULL);
    } else {
        return;
    }
}

static void WEBGPU_RequestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *renderer, void *success)
{
    if (status == WGPURequestAdapterStatus_Success) {
        ((WebGPURenderer *)renderer)->adapter = adapter;
        if (((WebGPURenderer *)renderer)->debugMode) {
            SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Acquired WebGPU adapter!");

            if (success != NULL) {
                *((bool *)success) = true;
            }
        }
    } else {
        // FIXME: I'm not entirely sure what the SDL conventions about error handling are so I'm just gonna return false and log an error.
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Requesting WebGPU adapter failed!\n'%s'", message.data);
        if (success != NULL) {
            *((bool *)success) = false;
        }
    }
}

static void WEBGPU_RequestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *renderer, void *success)
{
    if (status == WGPURequestDeviceStatus_Success) {
        ((WebGPURenderer *)renderer)->device = device;
        if (((WebGPURenderer *)renderer)->debugMode) {
            SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Acquired WebGPU device!");
            if (success != NULL) {
                *((bool *)success) = true;
            }
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Requesting WebGPU device failed!\n'%s'", message.data);
        if (success != NULL) {
            *((bool *)success) = false;
        }
    }
}

static void WEBGPU_RequestAdapter(WebGPURenderer *renderer, bool *success)
{
    WGPURequestAdapterOptions adapterReqOptions;
    adapterReqOptions.powerPreference = renderer->preferLowPower ? WGPUPowerPreference_LowPower : WGPUPowerPreference_HighPerformance;

    wgpuInstanceRequestAdapter(renderer->instance, &adapterReqOptions, (WGPURequestAdapterCallbackInfo){ .callback = WEBGPU_RequestAdapterCallback, .mode = WGPUCallbackMode_AllowSpontaneous, .nextInChain = NULL, .userdata1 = &renderer, .userdata2 = success });
#ifdef __EMSCRIPTEN_
    // HACK: When using Emscripten, wgpuInstanceRequestAdapter/Device only triggers its callback after waiting a bit.
    // So, we'll just.... sleep for a millisecond-
    // OW OW OW STOP THROWING TOMATOES AT ME (loud booing in background)
    emscripten_sleep(1);
#endif
}

static void WEBGPU_RequestDevice(WebGPURenderer *renderer, bool *success)
{
    WGPUDeviceDescriptor deviceDesc;
    deviceDesc.deviceLostCallbackInfo = (WGPUDeviceLostCallbackInfo){ .callback = WEBGPU_DeviceLostCallback, .mode = WGPUCallbackMode_AllowSpontaneous, .nextInChain = NULL, .userdata1 = renderer, .userdata2 = NULL };
    deviceDesc.uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo){ .callback = WEBGPU_UncapturedErrorCallback, .nextInChain = NULL, .userdata1 = &renderer->debugMode, .userdata2 = NULL };

    wgpuAdapterRequestDevice(renderer->adapter, &deviceDesc, (WGPURequestDeviceCallbackInfo){ .callback = WEBGPU_RequestDeviceCallback, .mode = WGPUCallbackMode_AllowSpontaneous, .nextInChain = NULL, .userdata1 = renderer, .userdata2 = success });

#ifdef __EMSCRIPTEN_
    // HACK: See WEBGPU_RequestAdapter.
    emscripten_sleep(1);
#endif
}

static bool WEBGPU_PrepareDriver(SDL_VideoDevice *_this, SDL_PropertiesID props)
{
    bool result = false;
    // FIXME: We're gonna cheat and statically link the library for now.
    // We'll make it dynamically loaded eventually™,

    // The renderer's heap allocated since I don't wanna risk a stack overflow (OMG HE SAID THE THING!!!!!!)
    WebGPURenderer *renderer;

    if (_this->WGPU_CreateSurface == NULL) {
        return false;
    }

    renderer = (WebGPURenderer *)SDL_calloc(1, sizeof(*renderer));

    renderer->debugMode = SDL_GetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_DEBUGMODE_BOOLEAN, false);
    renderer->preferLowPower = SDL_GetBooleanProperty(props, SDL_PROP_GPU_DEVICE_CREATE_PREFERLOWPOWER_BOOLEAN, false);
    renderer->shouldRecreateLostDevice = true;

    renderer->instance = wgpuCreateInstance(&WGPU_INSTANCE_DESCRIPTOR_INIT);

    if (!renderer->instance) {
        result = false;
        goto finished;
    }

    bool getAdapterSucceeded = false;
    bool getDeviceSucceeded = false;
    int loops = 0;

    WEBGPU_RequestAdapter(renderer, &getAdapterSucceeded);

    while (!getAdapterSucceeded) {
        // Simple timeout functionality.
        if (loops < 2500) {
            loops++;
        } else {
            result = false;
            goto finished;
        }
#ifdef __EMSCRIPTEN_
        // HACK: If we don't sleep we'll just eat up 100% CPU time and the program'll freeze.
        emscripten_sleep(1);
#endif
    }

    WEBGPU_RequestDevice(renderer, &getDeviceSucceeded);

    while (!getDeviceSucceeded) {
#ifdef __EMSCRIPTEN_
        emscripten_sleep(1);
#endif
    }

    renderer->queue = wgpuDeviceGetQueue(renderer->device);

    if (!renderer->queue) {
        result = false;
        goto finished;
    }

    renderer->commandEncoder = wgpuDeviceCreateCommandEncoder(renderer->device, &WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT);

    if (!renderer->commandEncoder) {
        result = false;
        goto finished;
    }

    // Alright, if we've reached this logic everything went well!
    // Let's free all resources and return a success!
    result = true;
    goto finished;

finished:
    // NOTE: Not entirely sure if this is safe or if it'll crash the program.
    // This is the first time I've ever used C's memory allocation stuff. -- TheStickmahn
    renderer->shouldRecreateLostDevice = false;
    renderer->debugMode = false; // shut up

    if (renderer->commandEncoder) {
        wgpuCommandEncoderRelease(renderer->commandEncoder);
    }
    if (renderer->queue) {
        wgpuQueueRelease(renderer->queue);
    }
    if (renderer->device) {
        wgpuDeviceRelease(renderer->device);
    }
    if (renderer->adapter) {
        wgpuAdapterRelease(renderer->adapter);
    }
    if (renderer->instance) {
        wgpuInstanceRelease(renderer->instance);
    }

    SDL_free(renderer);
    return result;
}

static SDL_GPUDevice *WEBGPU_CreateGPUDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)
{
    WebGPURenderer *renderer;
    SDL_GPUDevice *result;

    renderer = (WebGPURenderer *)SDL_calloc(1, sizeof(*renderer));

    if (renderer == NULL) {
        return NULL;
    }

    renderer->debugMode = debugMode;
    renderer->preferLowPower = preferLowPower;
    renderer->shouldRecreateLostDevice = true;
    renderer->props = SDL_CreateProperties();

    renderer->instance = wgpuCreateInstance(&WGPU_INSTANCE_DESCRIPTOR_INIT);

    if (!renderer->instance) {
        SDL_free(renderer);
        return NULL;
    }

    bool getAdapterSucceeded = false;
    bool getDeviceSucceeded = false;

    WEBGPU_RequestAdapter(renderer, &getAdapterSucceeded);
    while (!getAdapterSucceeded) {
#ifdef __EMSCRIPTEN_
        emscripten_sleep(1);
#endif
    }

    if (!renderer->adapter) {
        wgpuInstanceRelease(renderer->instance);

        SDL_free(renderer);
        return NULL;
    }

    WEBGPU_RequestDevice(renderer, &getDeviceSucceeded);
    while (!getDeviceSucceeded) {
#ifdef __EMSCRIPTEN_
        emscripten_sleep(1);
#endif
    }

    if (!renderer->device) {
        wgpuAdapterRelease(renderer->adapter);
        wgpuInstanceRelease(renderer->instance);

        SDL_free(renderer);
        return NULL;
    }

    WGPUAdapterInfo adapterInfo;
    wgpuAdapterGetInfo(renderer->adapter, &adapterInfo);

    SDL_SetStringProperty(
        renderer->props,
        SDL_PROP_GPU_DEVICE_NAME_STRING,
        adapterInfo.device.data);

    // FIXME: Not sure this is how it's supposed to work
    SDL_SetStringProperty(
        renderer->props,
        SDL_PROP_GPU_DEVICE_DRIVER_NAME_STRING,
        "WebGPU");

    result = (SDL_GPUDevice *)SDL_calloc(1, sizeof(SDL_GPUDevice));

    result->driverData = (SDL_GPURenderer *)renderer;
    result->shader_formats = SDL_GPU_SHADERFORMAT_WGSL;

    // FIXME: Uncomment this when everything's been implemented
    //
    // ASSIGN_DRIVER(WEBGPU)

    return result;
}

static SDL_GPUCopyPass *WEBGPU_BeginCopyPass(
    SDL_GPUCommandBuffer *commandBuffer)
{
    // Every copy pass is just a CommandBuffer in a large trench coat.
    // Damn you Tim Apple.
    return (SDL_GPUCopyPass *)commandBuffer;
}

static void WEBGPU_EndCopyPass(SDL_GPUCopyPass *copyPass)
{
    // Unfortunately, this function will never be implemented.
    //
    // We simply do not, as a human race, have enough compute on this Earth
    // to do the incredibly computationally expensive task, called "doing nothing"
}

static SDL_GPUCommandBuffer *WEBGPU_AcquireGPUCommandBuffer(SDL_GPUDevice *device)
{
    // HACK:
    // WebGPU doesn't really have "command buffers" like SDL_GPU does.
    // It does have a "command buffer", but that's something you get after saying "Hey, I'm not gonna do anything more with this.",
    // which is the exact opposite of what a command buffer is in SDL_GPU.
    // So, I had to do this gross hack. God, please forgive me.
    WebGPUCommandBufferWrapper *wrapper = (WebGPUCommandBufferWrapper *)SDL_calloc(1, sizeof(WebGPUCommandBufferWrapper));

    wrapper->queue = &((WebGPURenderer *)device->driverData)->queue;
    wrapper->queue = &((WebGPURenderer *)device->driverData)->queue;

    // HACK:
    // Yet another gross hack.
    // Because WebGPU abstracts away swapchain handling from the user, you can only access the swapchain through the WGPUSurface.
    // So, I added a WindowData pointer to the WebGPUCommandBufferWrapper.
    // It'll point to the first captured window. (if there is one)
    wrapper->windowDataGrossHack = ((WebGPURenderer *)device->driverData)->capturedWindows[0];
}

static bool WEBGPU_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commandBuffer)
{
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish((*((WebGPUCommandBufferWrapper *)commandBuffer)->encoder), &WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT);

    if (!cmdBuf) {
        SDL_free(commandBuffer);
        return false;
    }

    wgpuQueueSubmit(*((WebGPUCommandBufferWrapper *)commandBuffer)->queue, 1, &cmdBuf);

    // We'll be freeing the "command buffer", so any usage of it will be undefined behaviour.
    // Don't. The docs tell you not to.
    SDL_free(commandBuffer);
    return true;
}

static SDL_GPUFence *WEBGPU_SubmitGPUCommandBufferAndAcquireFence(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUFence *fence;
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish((*((WebGPUCommandBufferWrapper *)commandBuffer)->encoder), &WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT);
    if (!cmdBuf) {
        SDL_SetError("wgpuCommandEncoderFinish failed!");
        SDL_free(commandBuffer);
        return NULL;
    }

    fence = (WebGPUFence *)SDL_calloc(1, sizeof(*fence));

    wgpuQueueOnSubmittedWorkDone((*((WebGPUCommandBufferWrapper *)commandBuffer)->queue), (WGPUQueueWorkDoneCallbackInfo){ .callback = WEBGPU_FenceCallback, .mode = WGPUCallbackMode_AllowSpontaneous, .nextInChain = NULL, .userdata1 = fence, .userdata2 = NULL });
    wgpuQueueSubmit(*((WebGPUCommandBufferWrapper *)commandBuffer)->queue, 1, &cmdBuf);

    SDL_free(commandBuffer);
    return true;
}

static bool WEBGPU_QueryGPUFence(SDL_GPUDevice *device, SDL_GPUFence *fence)
{
    if (fence != NULL) {
        return ((WebGPUFence *)fence)->status;
    }
}

static WGPUTextureView WEBGPU_INTERNAL_CreateTextureView(WebGPUTexture *texture, uint32_t depth)
{
    // FIXME: This should probably be heap-allocated.
    WGPUTextureView view;

    uint32_t mipLevelCount = wgpuTextureGetMipLevelCount(texture->texture);
    uint32_t arrayLayerCount = wgpuTextureGetDepthOrArrayLayers(texture->texture);
    WGPUTextureFormat format = wgpuTextureGetFormat(texture->texture);
    WGPUTextureUsage usages = wgpuTextureGetUsage(texture->texture);
    WGPUTextureViewDimension dimension;

    switch (texture->type) {
    case SDL_GPU_TEXTURETYPE_2D:
        dimension = WGPUTextureViewDimension_2D;
        break;
    case SDL_GPU_TEXTURETYPE_2D_ARRAY:
        dimension = WGPUTextureViewDimension_2DArray;
        break;
    case SDL_GPU_TEXTURETYPE_3D:
        dimension = WGPUTextureViewDimension_3D;
        break;
    case SDL_GPU_TEXTURETYPE_CUBE:
        dimension = WGPUTextureViewDimension_Cube;
        break;
    case SDL_GPU_TEXTURETYPE_CUBE_ARRAY:
        dimension = WGPUTextureViewDimension_CubeArray;
        break;
    }

    view = wgpuTextureCreateView(texture->texture, &((WGPUTextureViewDescriptor){
                                                       .arrayLayerCount = arrayLayerCount,
                                                       .baseArrayLayer = depth,
                                                       .baseMipLevel = 0,
                                                       .mipLevelCount = mipLevelCount,
                                                       .aspect = texture->aspect,
                                                       .dimension = dimension,
                                                       .format = format,
                                                       .label = NULL,
                                                       .usage = usages,
                                                   }));

    return view;
}

static Uint32 WEBGPU_INTERNAL_GetTextureSubresourceIndex(
    Uint32 mipLevel,
    Uint32 layer,
    Uint32 numLevels)
{
    return mipLevel + (layer * numLevels);
}

static WebGPUTextureSubresource *VULKAN_INTERNAL_FetchTextureSubresource(
    WebGPUTextureContainer *textureContainer,
    Uint32 layer,
    Uint32 level)
{
    Uint32 index = WEBGPU_INTERNAL_GetTextureSubresourceIndex(
        level,
        layer,
        textureContainer->header.info.num_levels);

    return &textureContainer->activeTexture->subresources[index];
}

static WebGPUTexture *WEBGPU_INTERNAL_CreateTexture(WebGPURenderer *renderer, const SDL_GPUTextureCreateInfo *createInfo)
{
    WebGPUTexture *texture;

    texture = (WebGPUTexture *)SDL_calloc(1, sizeof(*texture));
    WGPUTextureDescriptor desc;

    if (createInfo->type == SDL_GPU_TEXTURETYPE_2D) {
        desc.dimension = WGPUTextureDimension_2D;
    } else if (createInfo->type == SDL_GPU_TEXTURETYPE_3D) {
        desc.dimension = WGPUTextureDimension_3D;
    } else {
        SDL_free(texture);
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Texture format unsupported!");
    }
    desc.usage = 0;

    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_SAMPLER) {
        desc.usage |= WGPUTextureUsage_TextureBinding;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) {
        desc.usage |= WGPUTextureUsage_RenderAttachment;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET) {
        desc.usage |= WGPUTextureUsage_RenderAttachment;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ || createInfo->usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ || createInfo->usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE) {
        desc.usage |= WGPUTextureUsage_StorageBinding;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Texture usage unsupported!");
        SDL_free(texture);
        return NULL;
    }

    desc.format = SDLToWebGPU_TextureFormat[createInfo->format];
    desc.mipLevelCount = 1;

    desc.size = (WGPUExtent3D){ .width = createInfo->width, .height = createInfo->height, .depthOrArrayLayers = 1 };

    if (SDL_HasProperty(createInfo->props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING)) {
        char *debugName = SDL_strdup(SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, NULL));
        desc.label = (WGPUStringView){ .data = debugName, .length = SDL_strlen(debugName) };
    }

    texture->usage = createInfo->usage;
    texture->levelCount = createInfo->num_levels;
    texture->layerCount = (createInfo->type == SDL_GPU_TEXTURETYPE_3D) ? 1 : createInfo->layer_count_or_depth;
    texture->depth = (createInfo->type == SDL_GPU_TEXTURETYPE_3D) ? createInfo->layer_count_or_depth : 1;
    texture->type = createInfo->type;
    SDL_SetAtomicInt(&texture->referenceCount, 0);

    // excellent naming by yours truly
    texture->texture = wgpuDeviceCreateTexture(renderer->device, &desc);

    texture->fullView = WEBGPU_INTERNAL_CreateTextureView(texture, 0);
    texture->subresourceCount = texture->layerCount * createInfo->num_levels;
    texture->subresources = SDL_calloc(
        texture->subresourceCount,
        sizeof(WebGPUTextureSubresource));

    for (Uint32 i = 0; i < texture->layerCount; i += 1) {
        // NOTE: WebGPU allows us to retrieve a texture's usages after it's been created, so we just have one "CreateTextureView" function,
        // which works for all kinds of texture views.
        // This does however rely on CreateTexture working.
        for (Uint32 j = 0; j < createInfo->num_levels; j += 1) {
            Uint32 subresourceIndex = WEBGPU_INTERNAL_GetTextureSubresourceIndex(
                j,
                i,
                createInfo->num_levels);

            if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) {
                texture->subresources[subresourceIndex].renderTargetViews = SDL_malloc(
                    texture->depth * sizeof(WGPUTextureView));

                if (texture->depth > 1) {
                    for (Uint32 k = 0; k < texture->depth; k += 1) {
                        texture->subresources[subresourceIndex].renderTargetViews[k] = WEBGPU_INTERNAL_CreateTextureView(texture, k);
                    }
                } else {
                    texture->subresources[subresourceIndex].renderTargetViews[0] = WEBGPU_INTERNAL_CreateTextureView(texture, i);
                }
            }

            if ((createInfo->usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE)) {
                texture->subresources[subresourceIndex].computeWriteView = WEBGPU_INTERNAL_CreateTextureView(texture, i);
            }

            if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET) {
                texture->subresources[subresourceIndex].depthStencilView = WEBGPU_INTERNAL_CreateTextureView(texture, i);
            }

            texture->subresources[subresourceIndex].parent = texture;
            texture->subresources[subresourceIndex].layer = i;
            texture->subresources[subresourceIndex].level = j;
        }
    }
}

static SDL_GPUTexture *WEBGPU_CreateGPUTexture(
    SDL_GPURenderer *driverData,
    const SDL_GPUTextureCreateInfo *createInfo)
{
    WebGPURenderer *renderer = (WebGPURenderer *)driverData;
    WebGPUTexture *texture;
    WebGPUTextureContainer *container;

    texture = WEBGPU_INTERNAL_CreateTexture(
        renderer,
        createInfo);

    if (texture == NULL) {
        return NULL;
    }

    container = SDL_malloc(sizeof(WebGPUTextureContainer));

    container->header.info = *createInfo;
    container->header.info.props = SDL_CreateProperties();
    if (createInfo->props) {
        SDL_CopyProperties(createInfo->props, container->header.info.props);
    }

    container->canBeCycled = true;
    container->activeTexture = texture;
    container->textureCapacity = 1;
    container->textureCount = 1;
    container->textures = SDL_malloc(
        container->textureCapacity * sizeof(WebGPUTexture *));
    container->textures[0] = container->activeTexture;
    container->debugName = NULL;

    if (SDL_HasProperty(createInfo->props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING)) {
        container->debugName = SDL_strdup(SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, NULL));
    }

    texture->container = container;
    texture->containerIndex = 0;

    return (SDL_GPUTexture *)container;
}

static bool WEBGPU_ClaimWindowForGPUDevice(SDL_GPUDevice *device, SDL_Window *window)
{
    WebGPUWindowData *windowData;

    windowData = (WebGPUWindowData *)SDL_calloc(1, sizeof(*windowData));

    windowData->window = window;
    windowData->presentMode = SDL_GPU_PRESENTMODE_VSYNC,
    windowData->swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    windowData->refcount = 1;
    windowData->renderer = (WebGPURenderer *)device->driverData;
    windowData->surface = SDL_WGPU_CreateSurface(window, windowData->renderer->instance);
    windowData->surfaceSuboptimal = false;

    WGPUSurfaceCapabilities caps = { 0 };
    wgpuSurfaceGetCapabilities(windowData->surface, windowData->renderer->adapter, &caps);

    // TODO: Check that caps.formats[0] is SDR.

    int w = 0;
    int h = 0;

    SDL_GetWindowSizeInPixels(window, &w, &h);

    wgpuSurfaceConfigure(windowData->surface, &(WGPUSurfaceConfiguration){
                                                  .device = windowData->renderer->device,
                                                  .usage = WGPUTextureUsage_RenderAttachment,
                                                  .format = caps.formats[0],
                                                  .presentMode = WGPUPresentMode_Fifo,
                                                  .alphaMode = caps.alphaModes[0],
                                                  .width = w,
                                                  .height = h });

    if (windowData->surface == NULL) {
        // TODO: I can't be bothered freeing everything
        return false;
    }

    windowData->renderer->hasCapturedWindow = true;
    return true;
}

static bool WEBGPU_AcquireGPUSwapchainTexture(SDL_GPUCommandBuffer *command_buffer, SDL_Window *window, SDL_GPUTexture **swapchain_texture, Uint32 *width, Uint32 *height)
{
    WGPUSurfaceTexture surfaceTexture;
    wgpuSurfaceGetCurrentTexture(((WebGPUCommandBufferWrapper *)command_buffer)->windowDataGrossHack->surface, &surfaceTexture);

    if (surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
        // FIXME: This is GROSS AND HORRIBLE
        ((WebGPUTextureContainer *)swapchain_texture)->activeTexture->texture = surfaceTexture.texture;
    } else if (surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        // TODO: Should reconfigure the surface if it's suboptimal
        ((WebGPUTextureContainer *)swapchain_texture)->activeTexture->texture = surfaceTexture.texture;
    } else {
        return false;
    }

    return true;
}

// NOTE: Small issue here, if we were to release a gpu "fence" which hasn't yet been called,
// the callback would still attempt to write into that pointer (which is now free memory).
// So, we're just not gonna do that.
static void WEBGPU_ReleaseGPUFence(SDL_GPUDevice *device, SDL_GPUFence *fence)
{
    if (fence != NULL) {
        if (((WebGPUFence *)fence)->status) {
            SDL_free(fence);
        } else {
            // Fence hasn't been called yet, we can't free it.

            if (device->debug_mode) {
                SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Attempted to free non-called fence! This would cause a use-after-free!");
            }
        }
    }
}

static void WEBGPU_WaitForGPUFences(SDL_GPUDevice *device, bool waitAll, SDL_GPUFence const *fences, uint32_t num_fences)
{
    // TODO: I'm not entirely how I'm supposed to implement this, so I'll just..... not.
    SDL_LogError(SDL_LOG_CATEGORY_GPU, "WEBGPU_WaitForGPUFences called! This hasn't been implemented!");
}

static WebGPUBuffer *WEBGPU_INTERNAL_CreateBuffer(WebGPURenderer *renderer, uint64_t size, SDL_GPUBufferUsageFlags usageFlags, WebGPUBufferType bufferType, const char *debugName)
{
    WGPUBufferUsage usages = 0;

    if (bufferType == WEBGPU_BUFFER_TYPE_GPU) {
        if (usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX) {
            usages |= WGPUBufferUsage_Vertex;
        }

        if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX) {
            usages |= WGPUBufferUsage_Index;
        }

        if (usageFlags & (SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ |
                          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE)) {
            usages |= WGPUBufferUsage_Storage;
        }

        if (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT) {
            usages |= WGPUBufferUsage_Indirect;
        }
    } else if (bufferType == WEBGPU_BUFFER_TYPE_TRANSFER) {
    } else if (bufferType == WEBGPU_BUFFER_TYPE_UNIFORM) {
        usages |= WGPUBufferUsage_Uniform;
    }

    // These are always needed!
    usages |= WGPUBufferUsage_CopySrc;
    usages |= WGPUBufferUsage_CopyDst;

    WebGPUBuffer *buf = (WebGPUBuffer *)SDL_calloc(1, sizeof(*buf));

    buf->size = size;
    buf->usage = usages;
    buf->type = bufferType;
    buf->markedForDestroy = false;
    buf->transitioned = false;

    WGPUBufferDescriptor desc = { .label = { debugName, SDL_strlen(debugName) }, .size = size, .usage = usages, .mappedAtCreation = false, .nextInChain = NULL };

    buf->buffer = wgpuDeviceCreateBuffer(renderer->device, &desc);
    buf->usedRegion->webGPUBuffer = buf;

    return buf;
}

static WebGPUBufferContainer *WEBGPU_INTERNAL_CreateBufferContainer(
    WebGPURenderer *renderer,
    uint64_t size,
    SDL_GPUBufferUsageFlags usageFlags,
    WebGPUBufferType bufferType,
    bool dedicated,
    const char *debugName)
{
    WebGPUBufferContainer *bufferContainer;
    WebGPUBuffer *buffer;

    buffer = WEBGPU_INTERNAL_CreateBuffer(
        renderer,
        size,
        usageFlags,
        bufferType,
        debugName);

    if (buffer == NULL) {
        return NULL;
    }

    bufferContainer = SDL_calloc(1, sizeof(WebGPUBufferContainer));

    bufferContainer->activeBuffer = buffer;
    buffer->container = bufferContainer;
    buffer->containerIndex = 0;

    bufferContainer->bufferCapacity = 1;
    bufferContainer->bufferCount = 1;
    bufferContainer->buffers = SDL_calloc(bufferContainer->bufferCapacity, sizeof(WebGPUBuffer *));
    bufferContainer->buffers[0] = bufferContainer->activeBuffer;
    bufferContainer->dedicated = dedicated;
    bufferContainer->debugName = NULL;

    if (debugName != NULL) {
        bufferContainer->debugName = SDL_strdup(debugName);
    }

    return bufferContainer;
}

static void WEBGPU_INTERNAL_ReleaseBuffer(WebGPUBuffer *buffer)
{
    // FIXME: This is almost certainly unsafe
    wgpuBufferRelease(buffer->buffer);
}

static void WEBGPU_INTERNAL_ReleaseTexture(WebGPUTexture *texture)
{
    wgpuTextureRelease(texture->texture);
}

static void WEBGPU_INTERNAL_ReleaseBufferContainer(WebGPUBufferContainer *container)
{
    for (int i = 0; i < container->bufferCount; i++) {
        WEBGPU_INTERNAL_ReleaseBuffer(container->buffers[i]);
    }

    if (container->debugName != NULL) {
        SDL_free(container->debugName);
        container->debugName = NULL;
    }
    SDL_free(container->buffers);
    SDL_free(container);
}

static SDL_GPUBuffer *WEBGPU_CreateGPUBuffer(SDL_GPUDevice *device, const SDL_GPUBufferCreateInfo *createInfo)
{
    char *debugName = NULL;

    if (SDL_HasProperty(createInfo->props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING)) {
        SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, debugName);
    }

    return (SDL_GPUBuffer *)WEBGPU_INTERNAL_CreateBufferContainer(
        (WebGPURenderer *)device->driverData,
        createInfo->size,
        createInfo->usage,
        WEBGPU_BUFFER_TYPE_GPU,
        false,
        debugName);
}

static SDL_GPUTransferBuffer *WEBGPU_CreateGPUTransferBuffer(SDL_GPUDevice *device, const SDL_GPUBufferCreateInfo *createInfo)
{
    char *debugName = NULL;

    if (SDL_HasProperty(createInfo->props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING)) {
        SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_BUFFER_CREATE_NAME_STRING, debugName);
    }

    return (SDL_GPUTransferBuffer *)WEBGPU_INTERNAL_CreateBufferContainer(
        (WebGPURenderer *)device->driverData,
        createInfo->size,
        createInfo->usage,
        WEBGPU_BUFFER_TYPE_TRANSFER,
        false,
        debugName);
}

static void WEBGPU_ReleaseGPUBuffer(SDL_GPUDevice *device, SDL_GPUBuffer *buffer)
{
    if (device != NULL && buffer != NULL) {
        WEBGPU_INTERNAL_ReleaseBufferContainer((WebGPUBufferContainer *)buffer);
    }
}

static void WEBGPU_ReleaseGPUTransferBuffer(SDL_GPUDevice *device, SDL_GPUTransferBuffer *buffer)
{
    if (device != NULL && buffer != NULL) {
        WEBGPU_INTERNAL_ReleaseBufferContainer((WebGPUBufferContainer *)buffer);
    }
}

static void *WEBGPU_MapGPUTransferBuffer(SDL_GPUDevice *device, SDL_GPUTransferBuffer *transferBuffer, bool cycle)
{
    if (cycle) {
        // TODO: Buffer cycling!
    }

    // NOTE: WebGPU allows you to map a "slice" of a buffer. Maybe we should implement the same into SDL_GPU?
    // I think Vulkan also supports it.
    return wgpuBufferGetMappedRange(((WebGPUBufferContainer *)transferBuffer)->activeBuffer->buffer, 0, ((WebGPUBufferContainer *)transferBuffer)->activeBuffer->size);
}

static void WEBGPU_UnmapGPUTransferBuffer(SDL_GPUDevice *device, SDL_GPUTransferBuffer *transferBuffer)
{
    wgpuBufferUnmap(((WebGPUBufferContainer *)transferBuffer)->activeBuffer->buffer);
}

static void WEBGPU_INTERNAL_CopyBufferToBuffer(WGPUCommandEncoder *encoder, WebGPUBufferContainer *sourceBuf,
                                               uint32_t sourceBufOffset, WebGPUBufferContainer *destBuf,
                                               uint32_t destBufOffset, uint32_t size)
{
    wgpuCommandEncoderCopyBufferToBuffer(*encoder, sourceBuf->activeBuffer->buffer, sourceBufOffset, destBuf->activeBuffer->buffer, destBufOffset, size);
}

static void WEBGPU_CopyGPUBufferToBuffer(SDL_GPUCopyPass *copyPass, const SDL_GPUBufferLocation *source, const SDL_GPUBufferLocation *dest, uint32_t size, bool cycle)
{
    if (cycle) {
        // TODO: Buffer cycling!
    }

    WEBGPU_INTERNAL_CopyBufferToBuffer((WGPUCommandEncoder *)copyPass, (WebGPUBufferContainer *)source, source->offset, (WebGPUBufferContainer *)dest, dest->offset, size);
}

SDL_GPUBootstrap WebGPUDriver = {
    "webgpu",
    WEBGPU_PrepareDriver,
    WEBGPU_CreateGPUDevice
};

#endif
