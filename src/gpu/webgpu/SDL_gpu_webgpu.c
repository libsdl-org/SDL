// TODO:
// - Clean up this code base.
//  - This code base is miserable to read through. I've placed functions in random places without care in the world.
//    Searching for functions manually in this is a pain I wouldn't wish upon my worst enemy.
// - Documentation, documentation.
//  - I have documented practically NOTHING, simply because documenting something means that I expect other people
//    to use it, and I am afraid of commitment.
// - Dynamically load WebGPU instead of statically linking it to SDL.

// FIXME: We create a WebGPU surface through the video backend, but we destroy it through the GPU backend?
// FIXME: I'm pretty sure 2D array mipmap generation is broken?

#include "SDL_internal.h"

#ifdef SDL_GPU_WEBGPU

#include "../SDL_sysgpu.h"
#include "webgpu.h"

#define WINDOW_PROPERTY_DATA                           "SDL.internal.gpu.webgpu.data"
#define DEFAULT_BINDGROUP_EXPIRY                       10000
#define FORCIBLY_DESTROY_QUEUED_DESTROY_AFTER_N_FAILED 10000

// Disable pseudo-mapping. Useful for debugging upload errors.
#define DEV_DISABLE_TRANSFER_BUFFER_PSEUDO_MAPPING false

// map states
#define MAP_STATE_UNMAPPED 0
// This means that the buffer was mapped from the GPU.
#define MAP_STATE_MAPPED_GPU 1
// This means that the buffer is "pseudo-mapped". See BufferContainer.pseudoMappedRange for more.
#define MAP_STATE_MAPPED_CPU 2

static WGPUPresentMode SDLToWebGPU_PresentMode[] = {
    WGPUPresentMode_Fifo,
    WGPUPresentMode_Immediate,
    WGPUPresentMode_Mailbox,
};

// NOTE: All of these required features are subject to change as we test it on different browsers and operating systems.
// Also, note that while Firefox's documentation *claims* that a lot of features are unsupported,
// I've been able to use a lot of them just fine (on Linux nonetheless), so I'm not sure what's up with that.

const WGPUFeatureName WEBGPU_INTERNAL_RequiredFeatures[6] = {
    // These three all have 99.98% coverage on WebGPU devices.
    WGPUFeatureName_Depth32FloatStencil8,
    WGPUFeatureName_RG11B10UfloatRenderable,
    WGPUFeatureName_IndirectFirstInstance,
    // This one has 98%, and only because of Samsung's weird browser.
    WGPUFeatureName_DepthClipControl,
    // This one has 95%, blame Samsung.
    WGPUFeatureName_BGRA8UnormStorage,
    // This one has 90%.
    WGPUFeatureName_Float32Filterable,
};

// These are features which we don't *need*, but they'd be really nice to have.
const WGPUFeatureName WEBGPU_INTERNAL_OptionalFeatures[7] = {
    // This one has 85%.
    WGPUFeatureName_Float32Blendable,

    // ASTC has 44%, while BC has 86%.
    WGPUFeatureName_TextureCompressionASTC,
    WGPUFeatureName_TextureCompressionASTCSliced3D,
    WGPUFeatureName_TextureCompressionBC,
    WGPUFeatureName_TextureCompressionBCSliced3D,

    // I have no idea what the support for these looks like.
    WGPUFeatureName_TextureFormatsTier1,
    WGPUFeatureName_TextureFormatsTier2,
};

static const char *WEBGPU_FeatureNameToString(WGPUFeatureName name)
{
    switch (name) {
    case WGPUFeatureName_CoreFeaturesAndLimits:
        return "WGPUFeatureName_CoreFeaturesAndLimits";
    case WGPUFeatureName_DepthClipControl:
        return "WGPUFeatureName_DepthClipControl";
    case WGPUFeatureName_Depth32FloatStencil8:
        return "WGPUFeatureName_Depth32FloatStencil8";
    case WGPUFeatureName_TextureCompressionBC:
        return "WGPUFeatureName_TextureCompressionBC";
    case WGPUFeatureName_TextureCompressionBCSliced3D:
        return "WGPUFeatureName_TextureCompressionBCSliced3D";
    case WGPUFeatureName_TextureCompressionETC2:
        return "WGPUFeatureName_TextureCompressionETC2";
    case WGPUFeatureName_TextureCompressionASTC:
        return "WGPUFeatureName_TextureCompressionASTC";
    case WGPUFeatureName_TextureCompressionASTCSliced3D:
        return "WGPUFeatureName_TextureCompressionASTCSliced3D";
    case WGPUFeatureName_TimestampQuery:
        return "WGPUFeatureName_TimestampQuery";
    case WGPUFeatureName_IndirectFirstInstance:
        return "WGPUFeatureName_IndirectFirstInstance";
    case WGPUFeatureName_ShaderF16:
        return "WGPUFeatureName_ShaderF16";
    case WGPUFeatureName_RG11B10UfloatRenderable:
        return "WGPUFeatureName_RG11B10UfloatRenderable";
    case WGPUFeatureName_BGRA8UnormStorage:
        return "WGPUFeatureName_BGRA8UnormStorage";
    case WGPUFeatureName_Float32Filterable:
        return "WGPUFeatureName_Float32Filterable";
    case WGPUFeatureName_Float32Blendable:
        return "WGPUFeatureName_Float32Blendable";
    case WGPUFeatureName_ClipDistances:
        return "WGPUFeatureName_ClipDistances";
    case WGPUFeatureName_DualSourceBlending:
        return "WGPUFeatureName_DualSourceBlending";
    case WGPUFeatureName_Subgroups:
        return "WGPUFeatureName_Subgroups";
    case WGPUFeatureName_TextureFormatsTier1:
        return "WGPUFeatureName_TextureFormatsTier1";
    case WGPUFeatureName_TextureFormatsTier2:
        return "WGPUFeatureName_TextureFormatsTier2";
    case WGPUFeatureName_PrimitiveIndex:
        return "WGPUFeatureName_PrimitiveIndex";
    case WGPUFeatureName_TextureComponentSwizzle:
        return "WGPUFeatureName_TextureComponentSwizzle";
    case WGPUFeatureName_SubgroupSizeControl:
        return "WGPUFeatureName_SubgroupSizeControl";
    case WGPUFeatureName_Force32:
        return "WGPUFeatureName_Force32";
    default:
        SDL_assert(!"Unsupported WGPUFeatureName");
        return "Unknown WGPUFeatureName";
    }
}

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
    WGPUTextureFormat_R32Uint,              // R32_UINT
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
    WGPUTextureFormat_Depth24Plus,          // D24_UNORM,
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

static WGPUTextureFormat SwapchainCompositionToFormat[] = {
    WGPUTextureFormat_RGBA8Unorm,     // SDR
    WGPUTextureFormat_RGBA8UnormSrgb, // SDR_LINEAR
    WGPUTextureFormat_RGBA16Float,    // HDR_EXTENDED_LINEAR
    WGPUTextureFormat_RGB10A2Unorm    // HDR10_ST2084
};

static WGPUTextureFormat SwapchainCompositionToFallbackFormat[] = {
    WGPUTextureFormat_BGRA8Unorm,     // SDR
    WGPUTextureFormat_BGRA8UnormSrgb, // SDR_LINEAR
    WGPUTextureFormat_RGBA16Float,    // HDR_EXTENDED_LINEAR (no fallback)
    WGPUTextureFormat_RGB10A2Unorm,   // HDR10_ST2084 (no fallback)
};

static SDL_GPUTextureFormat SwapchainCompositionToSDLFormat(
    SDL_GPUSwapchainComposition composition,
    bool usingFallback)
{
    switch (composition) {
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR:
        return usingFallback ? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    case SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR:
        return usingFallback ? SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB : SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB;
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR_EXTENDED_LINEAR:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
    case SDL_GPU_SWAPCHAINCOMPOSITION_HDR10_ST2084:
        return SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM;
    default:
        return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

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
    WGPUVertexFormat_Snorm8x2,  // BYTE2_NORM
    WGPUVertexFormat_Snorm8x4,  // BYTE4_NORM
    WGPUVertexFormat_Unorm8x2,  // UBYTE2_NORM
    WGPUVertexFormat_Unorm8x4,  // UBYTE4_NORM
    WGPUVertexFormat_Sint16x2,  // SHORT2
    WGPUVertexFormat_Sint16x4,  // SHORT4
    WGPUVertexFormat_Uint16x2,  // USHORT2
    WGPUVertexFormat_Uint16x4,  // USHORT4
    WGPUVertexFormat_Snorm16x2, // SHORT2_NORM
    WGPUVertexFormat_Snorm16x4, // SHORT4_NORM
    WGPUVertexFormat_Unorm16x2, // USHORT2_NORM
    WGPUVertexFormat_Unorm16x4, // USHORT4_NORM
    WGPUVertexFormat_Float16x2, // HALF2
    WGPUVertexFormat_Float16x4, // HALF4
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_VertexFormat, SDL_arraysize(SDLToWebGPU_VertexFormat) == SDL_GPU_VERTEXELEMENTFORMAT_MAX_ENUM_VALUE);

static WGPUIndexFormat SDLToWebGPU_IndexFormat[] = {
    WGPUIndexFormat_Uint16,
    WGPUIndexFormat_Uint32,
};

static WGPUVertexStepMode SDLToWebGPU_VertexInputRate[] = {
    WGPUVertexStepMode_Vertex,
    WGPUVertexStepMode_Instance,
};

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
    WGPUBlendOperation_Undefined, // INVALID
    WGPUBlendOperation_Add,
    WGPUBlendOperation_Subtract,
    WGPUBlendOperation_ReverseSubtract,
    WGPUBlendOperation_Min,
    WGPUBlendOperation_Max,
};
SDL_COMPILE_TIME_ASSERT(SDLToWebGPU_BlendOp, SDL_arraysize(SDLToWebGPU_BlendOp) == SDL_GPU_BLENDOP_MAX_ENUM_VALUE);

static WGPUCompareFunction SDLToWebGPU_CompareFunc[] = {
    WGPUCompareFunction_Undefined, // SDL_GPU_COMPAREOP_INVALID
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
    WGPUStencilOperation_Undefined, // INVALID
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

static WGPUTextureSampleType WebGPUTextureFormatToSampleType(WGPUTextureFormat format)
{
    switch (format) {
    case WGPUTextureFormat_R8Uint:
    case WGPUTextureFormat_RG8Uint:
    case WGPUTextureFormat_RGBA8Uint:
    case WGPUTextureFormat_R16Uint:
    case WGPUTextureFormat_RG16Uint:
    case WGPUTextureFormat_RGBA16Uint:
    case WGPUTextureFormat_R32Uint:
    case WGPUTextureFormat_RG32Uint:
    case WGPUTextureFormat_RGBA32Uint:
        return WGPUTextureSampleType_Uint;

    case WGPUTextureFormat_R8Sint:
    case WGPUTextureFormat_RG8Sint:
    case WGPUTextureFormat_RGBA8Sint:
    case WGPUTextureFormat_R16Sint:
    case WGPUTextureFormat_RG16Sint:
    case WGPUTextureFormat_RGBA16Sint:
    case WGPUTextureFormat_R32Sint:
    case WGPUTextureFormat_RG32Sint:
    case WGPUTextureFormat_RGBA32Sint:
        return WGPUTextureSampleType_Sint;

    case WGPUTextureFormat_Depth16Unorm:
    case WGPUTextureFormat_Depth24Plus:
    case WGPUTextureFormat_Depth32Float:
        return WGPUTextureSampleType_Depth;

    case WGPUTextureFormat_R32Float:
    case WGPUTextureFormat_RG32Float:
    case WGPUTextureFormat_RGBA32Float:
        return WGPUTextureSampleType_UnfilterableFloat;

    default:
        return WGPUTextureSampleType_Float;
    }
}

static bool WebGPUTextureFormatIsBlendable(WGPUTextureFormat format, bool blendableFloat32FeatureEnabled)
{
    // TODO: I couldn't find a list of which formats support blending so we'll just add them when we find them
    switch (format) {
    case WGPUTextureFormat_RGBA32Float:
        return blendableFloat32FeatureEnabled;
    default:
        return true;
    }
}

// These WGSL identifiers were developed using Naga's wgsl keywords as a reference.
// Thank you WGPU developers!
// TODO: Make sure all of these license notices:
// 1: Exist
// 2: Are legally sound
// I don't want my kneecaps broken please

static char *WGSLTextureFormatIdentifiers[43] = {
    "f32", // Undefined
    "u32", // Undefined
    "i32", // Undefined
    "rgba8unorm",
    "rgba8snorm",
    "rgba8uint",
    "rgba8sint",
    "rgba16unorm",
    "rgba16snorm",
    "rgba16uint",
    "rgba16sint",
    "rgba16float",
    "rg8unorm",
    "rg8snorm",
    "rg8uint",
    "rg8sint",
    "rg16unorm",
    "rg16snorm",
    "rg16uint",
    "rg16sint",
    "rg16float",
    "r32uint",
    "r32sint",
    "r32float",
    "rg32uint",
    "rg32sint",
    "rg32float",
    "rgba32uint",
    "rgba32sint",
    "rgba32float",
    "bgra8unorm",
    "r8unorm",
    "r8snorm",
    "r8uint",
    "r8sint",
    "r16unorm",
    "r16snorm",
    "r16uint",
    "r16sint",
    "r16float",
    "rgb10a2unorm",
    "rgb10a2uint",
    "rg11b10ufloat",
};

static char *WGSLTextureViewDimensionIdentifiers[17] = {
    "texture_1d",
    "texture_2d_array",
    "texture_2d",
    "texture_3d",
    "texture_cube_array",
    "texture_cube",
    "texture_multisampled_2d",
    "texture_depth_multisampled_2d",
    "texture_external",
    "texture_storage_1d",
    "texture_storage_2d_array",
    "texture_storage_2d",
    "texture_storage_3d",
    "texture_depth_2d_array",
    "texture_depth_2d",
    "texture_depth_cube_array",
    "texture_depth_cube",
};

static char *WGSLTextureIdentifiers[13] = {
    "texture_1d",
    "texture_2d_array",
    "texture_2d",
    "texture_3d",
    "texture_cube_array",
    "texture_cube",
    "texture_multisampled_2d",
    "texture_depth_multisampled_2d",
    "texture_external",
    "texture_depth_2d_array",
    "texture_depth_2d",
    "texture_depth_cube_array",
    "texture_depth_cube",
};

static char *WGSLStorageTextureIdentifiers[4] = {
    "texture_storage_1d",
    "texture_storage_2d_array",
    "texture_storage_2d",
    "texture_storage_3d",
};

static char *WGSLStorageTextureAccessIdentifiers[3] = {
    "read_write",
    "write",
    "read",
};

static WGPUTextureFormat WGSLTextureFormatIdentifiersIndexThingamabob[43] = {
    WGPUTextureFormat_Undefined,
    WGPUTextureFormat_Undefined,
    WGPUTextureFormat_Undefined,
    WGPUTextureFormat_RGBA8Unorm,
    WGPUTextureFormat_RGBA8Snorm,
    WGPUTextureFormat_RGBA8Uint,
    WGPUTextureFormat_RGBA8Sint,
    WGPUTextureFormat_RGBA16Unorm,
    WGPUTextureFormat_RGBA16Snorm,
    WGPUTextureFormat_RGBA16Uint,
    WGPUTextureFormat_RGBA16Sint,
    WGPUTextureFormat_RGBA16Float,
    WGPUTextureFormat_RG8Unorm,
    WGPUTextureFormat_RG8Snorm,
    WGPUTextureFormat_RG8Uint,
    WGPUTextureFormat_RG8Sint,
    WGPUTextureFormat_RG16Unorm,
    WGPUTextureFormat_RG16Snorm,
    WGPUTextureFormat_RG16Uint,
    WGPUTextureFormat_RG16Sint,
    WGPUTextureFormat_RG16Float,
    WGPUTextureFormat_R32Uint,
    WGPUTextureFormat_R32Sint,
    WGPUTextureFormat_R32Float,
    WGPUTextureFormat_RG32Uint,
    WGPUTextureFormat_RG32Sint,
    WGPUTextureFormat_RG32Float,
    WGPUTextureFormat_RGBA32Uint,
    WGPUTextureFormat_RGBA32Sint,
    WGPUTextureFormat_RGBA32Float,
    WGPUTextureFormat_BGRA8Unorm,
    WGPUTextureFormat_R8Unorm,
    WGPUTextureFormat_R8Snorm,
    WGPUTextureFormat_R8Uint,
    WGPUTextureFormat_R8Sint,
    WGPUTextureFormat_R16Unorm,
    WGPUTextureFormat_R16Snorm,
    WGPUTextureFormat_R16Uint,
    WGPUTextureFormat_R16Sint,
    WGPUTextureFormat_R16Float,
    WGPUTextureFormat_RGB10A2Unorm,
    WGPUTextureFormat_RGB10A2Uint,
    WGPUTextureFormat_RG11B10Ufloat,
};

static WGPUTextureViewDimension WGSLTextureViewDimensionIdentifiersIndexIHateNamingVariables[17] = {
    WGPUTextureViewDimension_1D,
    WGPUTextureViewDimension_2DArray,
    WGPUTextureViewDimension_2D,
    WGPUTextureViewDimension_3D,
    WGPUTextureViewDimension_CubeArray,
    WGPUTextureViewDimension_Cube,
    WGPUTextureViewDimension_2D,
    WGPUTextureViewDimension_2D,
    WGPUTextureViewDimension_Undefined,
    WGPUTextureViewDimension_1D,
    WGPUTextureViewDimension_2DArray,
    WGPUTextureViewDimension_2D,
    WGPUTextureViewDimension_3D,
    WGPUTextureViewDimension_2DArray,
    WGPUTextureViewDimension_2D,
    WGPUTextureViewDimension_CubeArray,
    WGPUTextureViewDimension_Cube,
};

static WGPUStorageTextureAccess WGSLStorageTextureAccessIdentifiersIndexWowISuckAtNamingThings[3] = {
    WGPUStorageTextureAccess_ReadWrite,
    WGPUStorageTextureAccess_WriteOnly,
    WGPUStorageTextureAccess_ReadOnly,
};

// Bltting shaders kindly borrowed (stolen) from klukaszek's SDLGPU WebGPU implementation.
// Thank you very much, I hate writing shaders. -- TheStickmahn
// https://github.com/klukaszek/SDL/blob/main/src/gpu/webgpu/SDL_gpu_webgpu.c

const char *blitVert = "\n\
struct VertexOutput {\n\
    @builtin(position) pos: vec4<f32>,\n\
    @location(0) tex: vec2<f32>\n\
};\n\
@vertex\n\
fn main(@builtin(vertex_index) vertexIndex: u32) -> VertexOutput {\n\
    var output: VertexOutput;\n\
    let tex = vec2<f32>(\n\
        f32((vertexIndex << 1u) & 2u),\n\
        f32(vertexIndex & 2u)\n\
    );\n\
    output.tex = tex;\n\
    output.pos = vec4<f32>(\n\
        tex * vec2<f32>(2.0, -2.0) + vec2<f32>(-1.0, 1.0),\n\
        0.0,\n\
        1.0\n\
    );\n\
    return output;\n\
}";

// TODO: We should just generate the RGBA32 compat shaders at runtime.
const char *blit2DShader = "\n\
struct SourceRegionBuffer {\n\
    uvLeftTop: vec2<f32>,\n\
    uvDimensions: vec2<f32>,\n\
    mipLevel: u32,\n\
    layerOrDepth: f32\n\
}\n\
@group(2) @binding(0) var sourceTexture2D: texture_2d<f32>;\n\
@group(2) @binding(1) var sourceSampler: sampler;\n\
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;\n\
@fragment\n\
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {\n\
    let newCoord = sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex;\n\
    return textureSampleLevel(sourceTexture2D, sourceSampler, newCoord, f32(sourceRegion.mipLevel));\n\
}";

const char *blit2DArrayShader = "\n\
struct SourceRegionBuffer {\n\
    uvLeftTop: vec2<f32>,\n\
    uvDimensions: vec2<f32>,\n\
    mipLevel: u32,\n\
    layerOrDepth: f32\n\
}\n\
@group(2) @binding(0) var sourceTexture2DArray: texture_2d_array<f32>;\n\
@group(2) @binding(1) var sourceSampler: sampler;\n\
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;\n\
@fragment\n\
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {\n\
    let newCoord = vec2<f32>(\n\
        sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex\n\
    );\n\
    return textureSampleLevel(sourceTexture2DArray, sourceSampler, newCoord, u32(sourceRegion.layerOrDepth), f32(sourceRegion.mipLevel));\n\
}";

const char *blit3DShader = "\n\
struct SourceRegionBuffer {\n\
    uvLeftTop: vec2<f32>,\n\
    uvDimensions: vec2<f32>,\n\
    mipLevel: u32,\n\
    layerOrDepth: f32\n\
}\n\
@group(2) @binding(0) var sourceTexture3D: texture_3d<f32>;\n\
@group(2) @binding(1) var sourceSampler: sampler;\n\
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;\n\
@fragment\n\
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {\n\
    let newCoord = vec3<f32>(\n\
        sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex,\n\
        sourceRegion.layerOrDepth\n\
    );\n\
    return textureSampleLevel(sourceTexture3D, sourceSampler, newCoord, f32(sourceRegion.mipLevel));\n\
}";

const char *blitCubeShader = "\n\
struct SourceRegionBuffer {\n\
    uvLeftTop: vec2<f32>,\n\
    uvDimensions: vec2<f32>,\n\
    mipLevel: u32,\n\
    layerOrDepth: f32\n\
}\n\
@group(2) @binding(0) var sourceTextureCube: texture_cube<f32>;\n\
@group(2) @binding(1) var sourceSampler: sampler;\n\
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;\n\
@fragment\n\
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {\n\
    let scaledUV = sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex;\n\
    let u = 2.0 * scaledUV.x - 1.0;\n\
    let v = 2.0 * scaledUV.y - 1.0;\n\
    var newCoord: vec3<f32>;\n\
    switch(u32(sourceRegion.layerOrDepth)) {\n\
        case 0u: { newCoord = vec3<f32>(1.0, -v, -u); }\n\
        case 1u: { newCoord = vec3<f32>(-1.0, -v, u); }\n\
        case 2u: { newCoord = vec3<f32>(u, 1.0, -v); }\n\
        case 3u: { newCoord = vec3<f32>(u, -1.0, v); }\n\
        case 4u: { newCoord = vec3<f32>(u, -v, 1.0); }\n\
        case 5u: { newCoord = vec3<f32>(-u, -v, -1.0); }\n\
        default: { newCoord = vec3<f32>(0.0, 0.0, 0.0); }\n\
    }\n\
\n\
    return textureSampleLevel(sourceTextureCube, sourceSampler, newCoord, f32(sourceRegion.mipLevel));\n\
}";

const char *blitCubeArrayShader = "\n\
struct SourceRegionBuffer {\n\
    uvLeftTop: vec2<f32>,\n\
    uvDimensions: vec2<f32>,\n\
    mipLevel: u32,\n\
    layerOrDepth: f32\n\
}\n\
@group(2) @binding(0) var sourceTextureCubeArray: texture_cube_array<f32>;\n\
@group(2) @binding(1) var sourceSampler: sampler;\n\
@group(3) @binding(0) var<uniform> sourceRegion: SourceRegionBuffer;\n\
@fragment\n\
fn main(@location(0) tex: vec2<f32>) -> @location(0) vec4<f32> {\n\
    let scaledUV = sourceRegion.uvLeftTop + sourceRegion.uvDimensions * tex;\n\
    let u = 2.0 * scaledUV.x - 1.0;\n\
    let v = 2.0 * scaledUV.y - 1.0;\n\
    let arrayIndex = u32(sourceRegion.layerOrDepth) / 6u;\n\
    var newCoord: vec3<f32>;\n\
    \n\
    switch(u32(sourceRegion.layerOrDepth) % 6u) {\n\
        case 0u: { newCoord = vec3<f32>(1.0, -v, -u); }\n\
        case 1u: { newCoord = vec3<f32>(-1.0, -v, u); }\n\
        case 2u: { newCoord = vec3<f32>(u, 1.0, -v); }\n\
        case 3u: { newCoord = vec3<f32>(u, -1.0, v); }\n\
        case 4u: { newCoord = vec3<f32>(u, -v, 1.0); }\n\
        case 5u: { newCoord = vec3<f32>(-u, -v, -1.0); }\n\
        default: { newCoord = vec3<f32>(0.0, 0.0, 0.0); }\n\
    }\n\
    \n\
    return textureSampleLevel(sourceTextureCubeArray, sourceSampler, newCoord, arrayIndex, f32(sourceRegion.mipLevel));\n\
}";

// I hate manual memory management so much I'm just reinventing the dynamic array but worse
#define WEBGPU_INTERNAL_InsertElementIntoArray(array, arrayCapacity, arrayElementCount, elementType, element)    \
    do {                                                                                                         \
        EXPAND_ARRAY_IF_NEEDED(array, elementType, arrayElementCount + 1, arrayCapacity, arrayElementCount + 1); \
        ((elementType *)array)[arrayElementCount++] = element;                                                   \
    } while (0)

// WebGPU is bad and stinky
#define ALIGN_VALUE(value, alignment) (value % alignment != 0 ? value + (alignment - (value % alignment)) : value)

typedef struct WebGPUTexture WebGPUTexture;
typedef struct WebGPUTextureContainer WebGPUTextureContainer;
typedef struct WebGPUBuffer WebGPUBuffer;
typedef struct WebGPUBufferContainer WebGPUBufferContainer;
typedef struct WebGPUWindowData WebGPUWindowData;
typedef struct WebGPUFence WebGPUFence;
typedef struct WebGPUSubmittedCommandBuffer WebGPUSubmittedCommandBuffer;
typedef struct WebGPUQueuedDestroy WebGPUQueuedDestroy;

typedef struct WebGPURenderer
{
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;

    // 0-3: Vertex
    // 4-7: Fragment
    // 8-11: Compute
    WebGPUBufferContainer *uniformBuffers[12];
    // Pre-made bind groups for the uniforms.
    // 0: Vertex, 1: Fragment, 2: Compute
    WGPUBindGroup uniformBufferBindGroups[3];

    WebGPUSubmittedCommandBuffer **submittedCommandBuffers;
    Uint32 submittedCommandBufferCapacity;
    Uint32 submittedCommandBufferCount;

    WebGPUQueuedDestroy **queuedDestroys;
    Uint32 queuedDestroyCount;
    Uint32 queuedDestroyCapacity;

    BlitPipelineCacheEntry *blitPipelines;

    struct blitResources
    {
        SDL_GPUShader *blitVertexShader;
        SDL_GPUShader *blit2DShader;
        SDL_GPUShader *blit2DArrayShader;
        SDL_GPUShader *blit3DShader;
        SDL_GPUShader *blitCubeShader;
        SDL_GPUShader *blitCubeArrayShader;

        SDL_GPUSampler *blitNearestSampler;
        SDL_GPUSampler *blitLinearSampler;
    } blitResources;

    SDL_PropertiesID props;
    SDL_HashTable *bindGroupHashTable; // A cache of the renderer's bind groups.

    SDL_Mutex *queryingFenceLock;
    SDL_Mutex *destroyingSelfLock;
    SDL_Mutex *registeringQueuedDestroyLock;
    SDL_Mutex *submittingCommandBufferLock;
    SDL_Mutex *creatingWebGPUResourceLock;

    // The ID of the thread which created this renderer.
    // This is currently only used for upload transfer buffer mapping / writing.
    // Since wgpuQueueWriteBuffer isn't thread-safe, we can't use pseudo-mapping
    // and instead have to fall back to regular (slow) maps and unmaps
    Uint64 createdByThreadID;
    Uint64 numSubmissions;

    Uint32 maxFramesInFlight;
    Uint32 blitPipelineCount;
    Uint32 blitPipelineCapacity;

    Uint32 nextBindableResourceID;

    // For how many submissions can a bind group be unused until it's automatically freed?
    // Set to -1 to disable pruning, and 0 to instantly free it.
    int bindGroupsExpireAfter;

    WebGPUFence *queueDoneFence;

    bool debugMode;
    bool destroyingSelf;
    bool preferLowPower;
    bool shouldRecreateLostDevice;
} WebGPURenderer;

struct WebGPUWindowData
{
    SDL_Window *window;
    WebGPURenderer *renderer;

    SDL_GPUSwapchainComposition swapchainComposition;
    SDL_GPUPresentMode presentMode;

    bool surfaceDirty;
    bool shouldUseFallbackFormat;

    WGPUSurface surface;
    WGPUSurfaceConfiguration surfaceConfig;
};

typedef enum WebGPUBufferType
{
    WEBGPU_BUFFER_TYPE_GPU,
    WEBGPU_BUFFER_TYPE_UNIFORM,
    WEBGPU_BUFFER_TYPE_TRANSFER_UPLOAD,
    WEBGPU_BUFFER_TYPE_TRANSFER_DOWNLOAD,
    // A transfer buffer which can only be accessed on the GPU.
    // This is used for texture copies where the user has not properly padded the data.
    // The GPUOnly transfer buffer acts as an intermediate.
    WEBGPU_BUFFER_TYPE_TRANSFER_GPUONLY,
} WebGPUBufferType;

// FIXME: This has a really bad name.
typedef enum WebGPUBindGroupType
{
    // Vertex stage, sampler, sampled textures, and storage textures / buffers
    // Corresponds to group 0 in WGSL.
    WEBGPU_BINDGROUP_VERTEXSAMPLERSTORAGE,
    // Vertex stage, uniform buffers.
    // Corresponds to group 1 in WGSL.
    WEBGPU_BINDGROUP_VERTEXUNIFORMS,
    // Fragment stage, samplers & storage textures / buffers
    // Corresponds to group 2 in WGSL.
    WEBGPU_BINDGROUP_FRAGMENTSAMPLERSTORAGE,
    // Fragment stage, uniform buffers.
    // Corresponds to group 3 in WGSL.
    WEBGPU_BINDGROUP_FRAGMENTUNIFORMS,
    // Compute stage, samplers, sampled textures, and readonly storage textures / buffers
    // Corresponds to group 0 in WGSL.
    WEBGPU_BINDGROUP_COMPUTESAMPLERSTORAGE,
    // Compute stage, readwrite storage textures and buffers.
    // Corresponds to group 1 in WGSL.
    WEBGPU_BINDGROUP_COMPUTEREADWRITESTORAGE,
    // Compute stage, uniform buffers.
    // Corresponds to group 2 in WGSL.
    WEBGPU_BINDGROUP_COMPUTEUNIFORMS,
} WebGPUBindGroupType;

typedef enum WebGPUQueuedDestroyType
{
    WEBGPU_QUEUED_DESTROY_INVALID,
    WEBGPU_QUEUED_DESTROY_TEXTURE_CONTAINER,
    WEBGPU_QUEUED_DESTROY_TEXTURE,
    WEBGPU_QUEUED_DESTROY_SAMPLER,
    WEBGPU_QUEUED_DESTROY_BUFFER_CONTAINER,
    WEBGPU_QUEUED_DESTROY_BUFFER,
    WEBGPU_QUEUED_DESTROY_FENCE,
    WEBGPU_QUEUED_DESTROY_SUBMITTED_COMMAND_BUFFER,
    WEBGPU_QUEUED_DESTROY_BIND_GROUP,
} WebGPUQueuedDestroyType;

typedef struct WebGPUBindGroupCacheKey
{
    Uint64 boundSamplersIDs[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    Uint64 boundTexturesIDs[MAX_TEXTURE_SAMPLERS_PER_STAGE];

    // Storage textures/buffers in a render pass are always read-only, so they don't need their own fields.
    Uint64 boundReadWriteStorageTexturesIDs[MAX_STORAGE_TEXTURES_PER_STAGE];
    Uint64 boundReadOnlyStorageTexturesIDs[MAX_STORAGE_TEXTURES_PER_STAGE];

    Uint64 boundReadWriteStorageBuffersIDs[MAX_STORAGE_BUFFERS_PER_STAGE];
    Uint64 boundReadOnlyStorageBuffersIDs[MAX_STORAGE_BUFFERS_PER_STAGE];
} WebGPUBindGroupCacheKey;

typedef enum WebGPUBindGroupEntryType
{
    WEBGPU_BIND_GROUP_ENTRY_TYPE_UNKNOWN,
    WEBGPU_BIND_GROUP_ENTRY_TYPE_SAMPLER,
    WEBGPU_BIND_GROUP_ENTRY_TYPE_TEXTURE,
    WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_BUFFER,
    WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_TEXTURE,
} WebGPUBindGroupEntryType;

typedef struct WebGPUInferredBindGroupLayoutEntry
{
    union
    {
        struct sampler
        {
            WGPUSamplerBindingType bindType;
        } sampler;

        struct texture
        {
            WGPUTextureFormat format;
            WGPUTextureViewDimension dimension;

            bool isDepth;                // Is the texture a depth type? (E.g: texture_depth_2d)
            bool isMultisampled;         // Is the texture a multisampled type? (E.g: texture_multisampled_2d)
            bool isForciblyUnfilterable; // Is this texture unfilterable?
        } texture;

        struct storageTexture
        {
            WGPUTextureFormat format;
            WGPUTextureViewDimension dimension;
            WGPUStorageTextureAccess access;
        } storageTexture;

        struct storageBuffer
        {
            bool canRead;
            bool canWrite;
        } storageBuffer;
    };

    WebGPUBindGroupEntryType type;

    Uint32 group;   // The group this binding belongs to.
    Uint32 binding; // The binding slot this binding.... binds to. (Take a shot each time I say "bind", and you'll be dead in a minute.)
} WebGPUInferredBindGroupLayoutEntry;

typedef struct WebGPUBindGroup
{
    WGPUBindGroup bindGroup;

    WebGPUBindGroupCacheKey key;

    // The renderer's numSubmissions when this bind groups was last bound.
    // This is used to automatically send old, redunant bind groups to the shadow realm.
    Uint64 lastUsedAtSubmission;

    // if this is true, nuke this bind group from orbit ASAP
    bool invalid;
} WebGPUBindGroup;

struct WebGPUBuffer
{
    WebGPUBufferContainer *container;
    WGPUBuffer buffer;

    WebGPUBufferType type;
    SDL_GPUBufferUsageFlags usage;

    Uint64 size;

    // The resource's identifier. (Excellent documentation by yours truly, give me that pulitzer now)
    Uint64 identifier;

    // the bind groups which depend on this resource
    // if this resource is freed, invalidate all of the dependents
    char **dependants;

    SDL_AtomicInt referenceCount;

    Uint32 numDependants;
    Uint32 dependantsCapacity;
};

struct WebGPUBufferContainer
{
    WebGPUBuffer *activeBuffer;

    WebGPUBuffer **buffers;
    Uint32 bufferCapacity;
    Uint32 bufferCount;

    SDL_GPUBufferUsageFlags usageFlags;
    WebGPUBufferType bufferType;
    Uint32 size;

    // Actually mapping stuff in WebGPU's really slow. So, we don't. Instead we allocate some memory,
    // say that's the mapped buffer, and then when you use it to copy something we call wgpuQueueWriteBuffer.
    // Now, this is only a thing for upload buffers. We obviously can't do this trick with download buffers.
    // However, this does increase performance SIGNIFIGANTLY if you're uploading data regularly.
    void *pseudoMappedRange;

    // The map state of this buffer.
    // 0 = Not mapped.
    // 1 = Mapped from GPU (Accessing GPU memory directly)
    // 2 = Mapped on CPU (Pseudo-mapping, see above)
    Uint16 mapState;

    bool dedicated;
    char *debugName;
};

typedef struct WebGPUTextureView
{
    WGPUTextureView view;

    Uint64 identifier;
    char **dependants;

    Uint32 numDependants;
    Uint32 dependantsCapacity;
} WebGPUTextureView;

struct WebGPUTexture
{
    WebGPUTextureContainer *container;

    WGPUTexture texture;
    WGPUTextureAspect aspect;
    WebGPUTextureView *fullTextureView;
    WebGPUTextureView **textureViews;

    SDL_GPUTextureType type;
    SDL_GPUTextureFormat format;
    SDL_AtomicInt referenceCount;

    Uint32 textureViewCount;
    Uint32 textureViewCapacity;

    bool markedForDestroy;  // so that defrag doesn't double-free
    bool externallyManaged; // true for XR swapchain images
};

struct WebGPUTextureContainer
{
    TextureCommonHeader header;

    WebGPUTexture *activeTexture;

    Uint32 textureCapacity;
    Uint32 textureCount;
    Uint32 activeTextureIndex;
    WebGPUTexture **textures;

    bool canBeCycled;
};

typedef struct WebGPUSampler
{
    WGPUSampler sampler;

    Uint64 identifier;

    char **dependants;

    Uint32 numDependants;
    Uint32 dependantsCapacity;
} WebGPUSampler;

typedef struct WebGPUFence
{
    WGPUFutureWaitInfo future;
    SDL_AtomicInt status;
} WebGPUFence;

typedef struct WebGPUQueuedResourceBindSampler
{
    WGPUShaderStage visibleTo;
    WebGPUSampler *sampler;
    WebGPUTextureView *textureView;

    Uint64 samplerIdentifier;
    Uint64 textureIdentifier;

    Uint16 slot;
} WebGPUQueuedResourceBindSampler;

typedef struct WebGPUQueuedResourceBindStorageTexture
{
    WGPUShaderStage visibleTo;
    WebGPUTextureView *storageTextureView;

    Uint64 identifier;
    Uint16 slot;
    // if false, storage texture is read-only
    bool canWrite;
} WebGPUQueuedResourceBindStorageTexture;

typedef struct WebGPUQueuedResourceBindStorageBuffer
{
    WGPUShaderStage visibleTo;
    WebGPUBuffer *storageBuffer;

    Uint64 identifier;
    Uint16 slot;
    // if false, storage buffer is read-only
    bool canWrite;
} WebGPUQueuedResourceBindStorageBuffer;

typedef struct WebGPUQueuedResourceBindUniformBuffer
{
    Uint32 length;
    // Stage-relative
    // (they're cousins)
    Uint32 slot;
} WebGPUQueuedResourceBindUniformBuffer;

typedef struct WebGPUQueuedResourceBindVertexBuffer
{
    WebGPUBuffer *buffer;
    Uint32 offset;

    const char *identifier;
    Uint16 slot;
} WebGPUQueuedResourceBindVertexBuffer;

typedef struct WebGPUQueuedResourceBindIndexBuffer
{
    WebGPUBuffer *buffer;

    const char *identifier;
    Uint32 offset;
} WebGPUQueuedResourceBindIndexBuffer;

typedef struct WebGPUQueuedUniformDataUpload
{
    void *data;

    Uint64 offset;
    Uint32 length;
    // This doesn't care about stage visibility. It's the raw index of the uniform buffer it'll write to.
    Uint32 slot;
} WebGPUQueuedUniformDataUpload;

typedef struct WebGPUQueuedDestroy
{
    WebGPUQueuedDestroyType type;
    union
    {
        WebGPUTextureContainer *textureContainer;
        WebGPUTexture *texture;
        WebGPUSampler *sampler;

        WebGPUBufferContainer *bufferContainer;
        WebGPUBuffer *buffer;

        WebGPUFence *fence;
        WebGPUSubmittedCommandBuffer *submittedCommandBuffer;
        WebGPUBindGroup *bindGroup;
    } resource;

    // Just used for debugging. If this is above 100 it'll log to the console that this resource is very stubborn.
    Uint32 destroysAttempted;
} WebGPUQueuedDestroy;

// TODO: Prevent the user from shooting off their foot if they bound an invalid resource

typedef struct WebGPUShaderBindGroupLayouts
{
    WGPUBindGroupLayout samplerStorageBindGroupLayout;
    WGPUBindGroupLayout uniformBindGroupLayout;
} WebGPUShaderBindGroupLayouts;

typedef struct WebGPUComputeShaderBindGroupLayouts
{
    WGPUBindGroupLayout samplerStorageBindGroupLayout;
    WGPUBindGroupLayout readWriteStorageBindGroupLayout;
    WGPUBindGroupLayout uniformBindGroupLayout;
} WebGPUComputeShaderBindGroupLayouts;

// TODO: Maybe this should be called something like "WebGPUGraphicsShader" to specify that it's only for graphics and not compute?
typedef struct WebGPUShader
{
    WGPUShaderModule shader;

    SDL_GPUShaderStage stage;
    char *entrypoint;
    WebGPUShaderBindGroupLayouts *bindGroupLayouts;

    Uint32 refCount;
} WebGPUShader;

typedef struct WebGPUGraphicsPipeline
{
    GraphicsPipelineCommonHeader header;

    WGPURenderPipeline pipeline;

    WebGPUShaderBindGroupLayouts vertexBindGroupLayouts;
    WebGPUShaderBindGroupLayouts fragmentBindGroupLayouts;
} WebGPUGraphicsPipeline;

typedef struct WebGPUComputePipeline
{
    ComputePipelineCommonHeader header;
    WGPUShaderModule computeShader;
    WGPUComputePipeline pipeline;

    WebGPUComputeShaderBindGroupLayouts *bindGroupLayouts;

} WebGPUComputePipeline;

typedef struct WebGPUOffsetBufferBinding
{
    WebGPUBuffer *buffer;
    Uint32 offset;
} WebGPUOffsetBufferBinding;

struct WebGPUSubmittedCommandBuffer
{
    WebGPUFence *fence;

    WebGPUBuffer **usedBuffers;
    Uint32 usedBufferCount;
    Uint32 usedBufferCapacity;

    WebGPUTexture **usedTextures;
    Uint32 usedTextureCount;
    Uint32 usedTextureCapacity;
};

typedef struct WebGPUCommandBuffer
{
    CommandBufferCommonHeader common;

    // gross, i don't like this
    WebGPURenderer *renderer;

    WGPUCommandEncoder encoder;
    WGPUQueue queue;
    WGPUDevice device;
    WGPUInstance instance;

    WGPURenderPassEncoder renderPassEncoder;
    WGPUComputePassEncoder computePassEncoder;

    WebGPUGraphicsPipeline *boundGraphicsPipeline;
    WebGPUComputePipeline *boundComputePipeline;

    WebGPUSubmittedCommandBuffer submitted;

    // These'll run right before the command buffer is submitted.
    WebGPUQueuedUniformDataUpload *queuedUniformUploads;

    Uint32 numQueuedUniformUploads;
    Uint32 queuedUniformUploadCapacity;

    size_t totalUniformDataLen;

    bool hasBoundGraphicsPipeline;
    bool hasBoundComputePipeline;

    struct vertexStageBinds
    {
        WebGPUSampler *boundSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
        WebGPUTextureView *boundTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
        WebGPUTextureView *boundStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
        WebGPUBuffer *boundStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];
        WebGPUOffsetBufferBinding boundVertexBuffers[MAX_VERTEX_BUFFERS];
        WebGPUOffsetBufferBinding boundIndexBuffer;

        WGPUIndexFormat indexFormat;

        Uint32 currentUniformWriteOffsets[4];
        Uint32 currentUniformReadOffsets[4];

        bool shouldBindVertexBuffers;
        bool shouldBindIndexBuffer;
        bool samplerStorageBindGroupOutdated;
        bool uniformBindGroupOutdated; // The bind group itself can never become outdated, but the read offsets can.
    } vertexStageBinds;

    struct fragmentStageBinds
    {
        WebGPUSampler *boundSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
        WebGPUTextureView *boundTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
        WebGPUTextureView *boundStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
        WebGPUBuffer *boundStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

        Uint32 currentUniformWriteOffsets[4];
        Uint32 currentUniformReadOffsets[4];

        bool samplerStorageBindGroupOutdated;
        bool uniformBindGroupOutdated; // The bind group itself can never become outdated, but the read offsets can.
    } fragmentStageBinds;

    struct computeStageBinds
    {
        WebGPUSampler *boundSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
        WebGPUTextureView *boundTextures[MAX_TEXTURE_SAMPLERS_PER_STAGE];
        WebGPUTextureView *boundReadOnlyStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
        WebGPUTextureView *boundReadWriteStorageTextures[MAX_STORAGE_TEXTURES_PER_STAGE];
        WebGPUBuffer *boundReadOnlyStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];
        WebGPUBuffer *boundReadWriteStorageBuffers[MAX_STORAGE_BUFFERS_PER_STAGE];

        Uint32 currentUniformWriteOffsets[4];
        Uint32 currentUniformReadOffsets[4];

        bool samplerStorageBindGroupOutdated;
        bool readWriteStorageBindGroupOutdated;
        bool uniformBindGroupOutdated;
    } computeStageBinds;

    Uint32 surfaceCount;
    Uint32 surfaceCapacity;
    WGPUSurface *surfaces;

    Uint32 swapchainTextureCount;
    Uint32 swapchainTextureCapacity;
    WebGPUTextureContainer **acquiredSwapchainTextures;
} WebGPUCommandBuffer;

static void
WEBGPU_INTERNAL_FenceCallback(WGPUQueueWorkDoneStatus status, WGPUStringView message, void *fence, void *unused)
{
    if (fence != NULL) {
        SDL_SetAtomicInt(&((WebGPUFence *)fence)->status, 1);
    }
}

static void
WEBGPU_INTERNAL_UncapturedErrorCallback(WGPUDevice const *device, WGPUErrorType type, WGPUStringView message, void *renderer, void *unused)
{
    SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU uncaptured error!\n%s", message.data);

    if (((WebGPURenderer *)renderer)->debugMode) {
        SDL_assert_release(!"Uncaptured WebGPU error! SDL won't let me format this message though so check the console or smth");
    }
}

static inline Uint32 WEBGPU_INTERNAL_GetTextureViewIndex(Uint32 layer, Uint32 level, Uint32 numMips)
{
    return (layer * numMips) + level;
}

static SDL_PropertiesID WEBGPU_GetDeviceProperties(
    SDL_GPUDevice *device)
{
    WebGPURenderer *renderer = (WebGPURenderer *)device;
    return renderer->props;
}

// forward decl
static void WEBGPU_INTERNAL_RequestDevice(WebGPURenderer *renderer, bool *success);

static void WEBGPU_INTERNAL_DeviceLostCallback(WGPUDevice const *device, WGPUDeviceLostReason reason, WGPUStringView message, void *renderer, void *unused)
{
    bool debugMode = ((WebGPURenderer *)renderer)->debugMode;

    if (debugMode) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Device has been lost.");
    }

    if (((WebGPURenderer *)renderer)->shouldRecreateLostDevice && !((WebGPURenderer *)renderer)->destroyingSelf) {
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

        WEBGPU_INTERNAL_RequestDevice(renderer, NULL);
    } else {
        return;
    }
}

static void WEBGPU_INTERNAL_RequestAdapterCallback(WGPURequestAdapterStatus status, WGPUAdapter adapter, WGPUStringView message, void *renderer, void *success)
{
    if (status == WGPURequestAdapterStatus_Success) {
        ((WebGPURenderer *)renderer)->adapter = adapter;
        if (((WebGPURenderer *)renderer)->debugMode) {
            SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Acquired WebGPU adapter!");
        }
        if (success != NULL) {
            *((bool *)success) = true;
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Requesting WebGPU adapter failed!\n'%s'", message.data);
        if (success != NULL) {
            *((bool *)success) = false;
        }
    }
}

// Bind group hashmap functions

// This hash function is based on Landon Curt Noll's hash_32a.c FNV-1a implementation.
// https://github.com/lcn2/fnv/blob/master/hash_32a.c
static Uint32 WEBGPU_INTERNAL_HashBindGroupKey(void *unused, const void *key)
{
    Uint32 bufferSize = sizeof(WebGPUBindGroupCacheKey);
    Uint32 hash = 0xAAA1ABEE; // There are no rules forcing this to be any specific number so I will take this rare opportunity to shitpost

    char *keyDataBuf = (char *)key; // Is this void* -> char* cast a bad idea? Probably.
    char *endOfBuf = keyDataBuf + bufferSize;

    while (keyDataBuf < endOfBuf) {
        hash *= 0x01000193;
        hash ^= (Uint32)*keyDataBuf++;
    }

    return hash;
}

static void WEBGPU_INTERNAL_DestroyCachedBindGroupAndKey(void *unused, const void *key, const void *value)
{
    WebGPUBindGroup *bindGroup = (WebGPUBindGroup *)value; // "Bu-bu-but that's a const pointe-" shh my child, segmentation faults don't happen if you're pure of heart
    if (bindGroup == NULL) {
        return;
    }

    wgpuBindGroupRelease(bindGroup->bindGroup);
    SDL_free(bindGroup);
}

static bool WEBGPU_INTERNAL_MatchHashedBindGroupKey(void *unused, const void *a, const void *b)
{
    return SDL_memcmp(a, b, sizeof(WebGPUBindGroupCacheKey)) == 0;
}

static void
WEBGPU_INTERNAL_RequestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, WGPUStringView message, void *renderer, void *success)
{
    if (status == WGPURequestDeviceStatus_Success) {
        ((WebGPURenderer *)renderer)->device = device;
        if (((WebGPURenderer *)renderer)->debugMode) {
            SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "Acquired WebGPU device!");
        }
        if (success != NULL) {
            *((bool *)success) = true;
        }
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Requesting WebGPU device failed!\n'%s'", message.data);
        if (success != NULL) {
            *((bool *)success) = false;
        }
    }
}

static inline void WEBGPU_INTERNAL_RegisterQueuedDestroy(WebGPURenderer *renderer, WebGPUQueuedDestroy *destroy)
{
    WEBGPU_INTERNAL_InsertElementIntoArray(renderer->queuedDestroys, renderer->queuedDestroyCapacity,
                                           renderer->queuedDestroyCount, WebGPUQueuedDestroy *, destroy);
}

static void WEBGPU_INTERNAL_QueueTextureContainerForRelease(WebGPURenderer *renderer, WebGPUTextureContainer *container)
{
    WebGPUQueuedDestroy *destroy = NULL;
    if (container == NULL) {
        return;
    }

    destroy = SDL_calloc(1, sizeof(*destroy));
    destroy->type = WEBGPU_QUEUED_DESTROY_TEXTURE_CONTAINER;
    destroy->resource.textureContainer = container;

    WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
}

// static void WEBGPU_INTERNAL_QueueTextureForRelease(WebGPURenderer *renderer, WebGPUTexture *texture)
// {
//     WebGPUQueuedDestroy *destroy = NULL;
//     if (texture == NULL) {
//         return;
//     }
//
//     destroy = SDL_calloc(1, sizeof(*destroy));
//     destroy->type = WEBGPU_QUEUED_DESTROY_TEXTURE;
//     destroy->resource.texture = texture;
//
//     WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
// }

static void WEBGPU_INTERNAL_QueueSamplerForRelease(WebGPURenderer *renderer, WebGPUSampler *sampler)
{
    WebGPUQueuedDestroy *destroy = NULL;
    if (sampler == NULL) {
        return;
    }

    destroy = SDL_calloc(1, sizeof(*destroy));
    destroy->type = WEBGPU_QUEUED_DESTROY_SAMPLER;
    destroy->resource.sampler = sampler;

    WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
}

static void WEBGPU_INTERNAL_QueueBufferContainerForRelease(WebGPURenderer *renderer, WebGPUBufferContainer *container)
{
    WebGPUQueuedDestroy *destroy = NULL;
    if (container == NULL) {
        return;
    }

    destroy = SDL_calloc(1, sizeof(*destroy));
    destroy->type = WEBGPU_QUEUED_DESTROY_BUFFER_CONTAINER;
    destroy->resource.bufferContainer = container;

    WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
}

static void WEBGPU_INTERNAL_QueueBufferForRelease(WebGPURenderer *renderer, WebGPUBuffer *buffer)
{
    WebGPUQueuedDestroy *destroy = NULL;
    if (buffer == NULL) {
        return;
    }

    destroy = SDL_calloc(1, sizeof(*destroy));
    destroy->type = WEBGPU_QUEUED_DESTROY_BUFFER;
    destroy->resource.buffer = buffer;

    WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
}

static void WEBGPU_INTERNAL_QueueFenceForRelease(WebGPURenderer *renderer, WebGPUFence *fence)
{
    WebGPUQueuedDestroy *destroy = NULL;
    if (fence == NULL) {
        return;
    }

    destroy = SDL_calloc(1, sizeof(*destroy));
    destroy->type = WEBGPU_QUEUED_DESTROY_FENCE;
    destroy->resource.fence = fence;

    WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
}

static void WEBGPU_INTERNAL_QueueSubmittedCommandBufferForRelease(WebGPURenderer *renderer, WebGPUSubmittedCommandBuffer *commandBuffer)
{
    WebGPUQueuedDestroy *destroy = NULL;
    if (commandBuffer == NULL) {
        return;
    }

    destroy = SDL_calloc(1, sizeof(*destroy));
    destroy->type = WEBGPU_QUEUED_DESTROY_SUBMITTED_COMMAND_BUFFER;
    destroy->resource.submittedCommandBuffer = commandBuffer;

    WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
}

static void WEBGPU_INTERNAL_QueueBindGroupForRelease(WebGPURenderer *renderer, WebGPUBindGroup *bindGroup)
{
    WebGPUQueuedDestroy *destroy = NULL;
    if (bindGroup == NULL) {
        return;
    }

    destroy = SDL_calloc(1, sizeof(*destroy));
    destroy->type = WEBGPU_QUEUED_DESTROY_BIND_GROUP;
    destroy->resource.bindGroup = bindGroup;

    WEBGPU_INTERNAL_RegisterQueuedDestroy(renderer, destroy);
}

static void WEBGPU_INTERNAL_RequestAdapter(WebGPURenderer *renderer, bool *success)
{
    WGPURequestAdapterOptions adapterReqOptions = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;

    adapterReqOptions.powerPreference = renderer->preferLowPower ? WGPUPowerPreference_LowPower : WGPUPowerPreference_HighPerformance;

    WGPUFuture future = wgpuInstanceRequestAdapter(renderer->instance, &adapterReqOptions, (WGPURequestAdapterCallbackInfo){
                                                                                               .callback = WEBGPU_INTERNAL_RequestAdapterCallback,
                                                                                               .mode = WGPUCallbackMode_WaitAnyOnly,
                                                                                               .nextInChain = NULL,
                                                                                               .userdata1 = renderer,
                                                                                               .userdata2 = success,
                                                                                           });

    WGPUFutureWaitInfo waitInfo = {
        future,
        false,
    };

    while (!waitInfo.completed) {
        wgpuInstanceWaitAny(renderer->instance, 1, &waitInfo, 0);
        SDL_DelayNS(100);
    }
}

static void WEBGPU_INTERNAL_RequestDevice(WebGPURenderer *renderer, bool *success)
{
    WGPUDeviceDescriptor deviceDesc = WGPU_DEVICE_DESCRIPTOR_INIT;

    bool clipDistance = SDL_GetBooleanProperty(renderer->props, SDL_PROP_GPU_DEVICE_CREATE_FEATURE_CLIP_DISTANCE_BOOLEAN, false);
    Uint32 numFeaturesEnabled = 0;
    Uint32 featureCapacity = SDL_arraysize(WEBGPU_INTERNAL_RequiredFeatures) + SDL_arraysize(WEBGPU_INTERNAL_OptionalFeatures) + 1;

    WGPUFeatureName *features = SDL_calloc(featureCapacity, sizeof(WGPUFeatureName));
    WGPUSupportedFeatures supportedFeatures = { 0 };

    wgpuAdapterGetFeatures(renderer->adapter, &supportedFeatures);

    if (clipDistance) {
        WEBGPU_INTERNAL_InsertElementIntoArray(features, featureCapacity, numFeaturesEnabled, WGPUFeatureName, WGPUFeatureName_ClipDistances);
    }

    // This code is horrible.
    for (int i = 0; i < SDL_arraysize(WEBGPU_INTERNAL_RequiredFeatures); i++) {
        bool supported = false;
        for (int j = 0; j < supportedFeatures.featureCount; j++) {
            if (WEBGPU_INTERNAL_RequiredFeatures[i] == supportedFeatures.features[j]) {
                WEBGPU_INTERNAL_InsertElementIntoArray(features, featureCapacity, numFeaturesEnabled, WGPUFeatureName, WEBGPU_INTERNAL_RequiredFeatures[i]);
                supported = true;
                break;
            }
        }
        if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU adapter does not support required feature \"%s\"!", WEBGPU_FeatureNameToString(WEBGPU_INTERNAL_RequiredFeatures[i]));
            SDL_assert_release(!"WebGPU adapter does not support all required features!");
        }
    }

    for (int i = 0; i < SDL_arraysize(WEBGPU_INTERNAL_RequiredFeatures); i++) {
        bool supported = false;
        for (int j = 0; j < supportedFeatures.featureCount; j++) {
            if (WEBGPU_INTERNAL_RequiredFeatures[i] == supportedFeatures.features[j]) {
                WEBGPU_INTERNAL_InsertElementIntoArray(features, featureCapacity, numFeaturesEnabled, WGPUFeatureName, WEBGPU_INTERNAL_RequiredFeatures[i]);
                supported = true;
                break;
            }
        }
        if (!supported) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU adapter does not support required feature \"%s\"!", WEBGPU_FeatureNameToString(WEBGPU_INTERNAL_RequiredFeatures[i]));
        }
    }

    for (int i = 0; i < SDL_arraysize(WEBGPU_INTERNAL_OptionalFeatures); i++) {
        bool supported = false;

        for (int j = 0; j < supportedFeatures.featureCount; j++) {
            if (WEBGPU_INTERNAL_OptionalFeatures[i] == supportedFeatures.features[j]) {
                WEBGPU_INTERNAL_InsertElementIntoArray(features, featureCapacity, numFeaturesEnabled, WGPUFeatureName, WEBGPU_INTERNAL_OptionalFeatures[i]);
                supported = true;
                break;
            }
        }

        if (!supported && renderer->debugMode) {
            SDL_LogDebug(SDL_LOG_CATEGORY_GPU, "WebGPU adapter does not support optional feature \"%s\".", WEBGPU_FeatureNameToString(WEBGPU_INTERNAL_OptionalFeatures[i]));
        }
    }

    deviceDesc.deviceLostCallbackInfo = (WGPUDeviceLostCallbackInfo){
        .callback = WEBGPU_INTERNAL_DeviceLostCallback,
        .mode = WGPUCallbackMode_AllowSpontaneous,
        .nextInChain = NULL,
        .userdata1 = renderer,
        .userdata2 = NULL,
    };

    deviceDesc.uncapturedErrorCallbackInfo = (WGPUUncapturedErrorCallbackInfo){
        .callback = WEBGPU_INTERNAL_UncapturedErrorCallback,
        .nextInChain = NULL,
        .userdata1 = renderer,
        .userdata2 = NULL,
    };

    deviceDesc.requiredFeatureCount = numFeaturesEnabled;
    deviceDesc.requiredFeatures = features;

    WGPUFuture future = wgpuAdapterRequestDevice(renderer->adapter, &deviceDesc, (WGPURequestDeviceCallbackInfo){
                                                                                     .callback = WEBGPU_INTERNAL_RequestDeviceCallback,
                                                                                     .mode = WGPUCallbackMode_WaitAnyOnly,
                                                                                     .nextInChain = NULL,
                                                                                     .userdata1 = renderer,
                                                                                     .userdata2 = success,
                                                                                 });

    WGPUFutureWaitInfo waitInfo = {
        future,
        false,
    };

    while (!waitInfo.completed) {
        wgpuInstanceWaitAny(renderer->instance, 1, &waitInfo, 0);
        SDL_DelayNS(100);
    }

    wgpuSupportedFeaturesFreeMembers(supportedFeatures);
    SDL_free(features);
}

static void WEBGPU_BeginCopyPass(
    SDL_GPUCommandBuffer *commandBuffer)
{
    // No-op.
}

static void WEBGPU_EndCopyPass(SDL_GPUCommandBuffer *copyPass)
{
    // Unfortunately, this function will never be implemented.
    //
    // We simply do not, as a human race, have enough compute on this Earth
    // to do the incredibly computationally expensive task, called "doing nothing"
}

static SDL_GPUCommandBuffer *WEBGPU_AcquireCommandBuffer(SDL_GPURenderer *device)
{
    // HACK:
    // WebGPU doesn't really have "command buffers" like SDL_GPU does.
    // It does have a "command buffer", but that's something you get after saying "Hey, I'm not gonna do anything more with this.",
    // which is the exact opposite of what a command buffer is in SDL_GPU.
    // So, I had to do this gross hack. God, please forgive me.
    WebGPUCommandBuffer *wrapper;
    wrapper = (WebGPUCommandBuffer *)SDL_calloc(1, sizeof(*wrapper));

    wrapper->renderer = (WebGPURenderer *)device;

    wrapper->encoder = wgpuDeviceCreateCommandEncoder(((WebGPURenderer *)device)->device, NULL);
    wrapper->queue = ((WebGPURenderer *)device)->queue;
    wrapper->device = ((WebGPURenderer *)device)->device;
    wrapper->instance = ((WebGPURenderer *)device)->instance;

    WGPUSurface *surfaces;
    WebGPUTextureContainer **acquiredSwapchainTextures;
    surfaces = SDL_calloc(2, sizeof(WGPUSurface));
    acquiredSwapchainTextures = SDL_calloc(2, sizeof(*acquiredSwapchainTextures));

    // Allocating enough space for two since XR and stuff
    // It'll automatically resize if needed but that's slow so eeeh
    wrapper->surfaces = surfaces;
    wrapper->surfaceCount = 0;
    wrapper->surfaceCapacity = 2;

    wrapper->acquiredSwapchainTextures = acquiredSwapchainTextures;
    wrapper->swapchainTextureCount = 0;
    wrapper->swapchainTextureCapacity = 2;

    return (SDL_GPUCommandBuffer *)wrapper;
}

/**
 * Set all of the render pass's bound resources to NULL.
 */
static void WEBGPU_INTERNAL_ClearRenderPassBindings(WebGPUCommandBuffer *cmdBuf)
{
    for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
        cmdBuf->vertexStageBinds.boundTextures[i] = NULL;
        cmdBuf->vertexStageBinds.boundSamplers[i] = NULL;
        cmdBuf->fragmentStageBinds.boundTextures[i] = NULL;
        cmdBuf->fragmentStageBinds.boundSamplers[i] = NULL;
    }

    for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
        cmdBuf->vertexStageBinds.boundStorageTextures[i] = NULL;
        cmdBuf->fragmentStageBinds.boundStorageTextures[i] = NULL;
    }

    for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
        cmdBuf->vertexStageBinds.boundStorageBuffers[i] = NULL;
        cmdBuf->fragmentStageBinds.boundStorageBuffers[i] = NULL;
    }

    // for (int i = 0; i < MAX_VERTEX_BUFFERS; i++) {
    //     cmdBuf->vertexStageBinds.boundVertexBuffers[i].buffer = NULL;
    //     cmdBuf->vertexStageBinds.boundVertexBuffers[i].offset = 0;
    // }

    cmdBuf->vertexStageBinds.samplerStorageBindGroupOutdated = true;
    cmdBuf->vertexStageBinds.uniformBindGroupOutdated = true;

    cmdBuf->fragmentStageBinds.samplerStorageBindGroupOutdated = true;
    cmdBuf->fragmentStageBinds.uniformBindGroupOutdated = true;

    cmdBuf->vertexStageBinds.shouldBindVertexBuffers = true;
    cmdBuf->vertexStageBinds.shouldBindIndexBuffer = true;
}

/**
 * Set all of the compute pass's bound resources to NULL.
 * clearReadWriteBindings prevents... clearing read write bindings. Since they're set at the beginning of the compute pass, clearing them when the pipeline's bound is A Bad Thing!
 */
static void WEBGPU_INTERNAL_ClearComputePassBindings(WebGPUCommandBuffer *cmdBuf, bool clearReadWriteBindings)
{
    for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
        cmdBuf->computeStageBinds.boundSamplers[i] = NULL;
        cmdBuf->computeStageBinds.boundTextures[i] = NULL;
    }

    for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
        cmdBuf->computeStageBinds.boundReadOnlyStorageTextures[i] = NULL;
        if (clearReadWriteBindings) {
            cmdBuf->computeStageBinds.boundReadWriteStorageTextures[i] = NULL;
        }
    }

    for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
        cmdBuf->computeStageBinds.boundReadOnlyStorageBuffers[i] = NULL;
        if (clearReadWriteBindings) {
            cmdBuf->computeStageBinds.boundReadWriteStorageBuffers[i] = NULL;
        }
    }

    cmdBuf->computeStageBinds.uniformBindGroupOutdated = true;
    cmdBuf->computeStageBinds.samplerStorageBindGroupOutdated = true;
    cmdBuf->computeStageBinds.readWriteStorageBindGroupOutdated = true;
}

static void WEBGPU_INTERNAL_FreeCommandBuffer(WebGPUCommandBuffer *cmdBuf)
{
    SDL_free(cmdBuf->surfaces);
    SDL_free(cmdBuf->acquiredSwapchainTextures);
    SDL_free(cmdBuf->queuedUniformUploads);
    SDL_free(cmdBuf);
}

// Create a new fence which will be called by wgpuQueueOnSubmittedWorkDone
static WebGPUFence *WEBGPU_INTERNAL_CreateFence(WGPUQueue queue)
{
    WebGPUFence *fence = SDL_calloc(1, sizeof(*fence));
    SDL_SetAtomicInt(&fence->status, 0);

    fence->future.future = wgpuQueueOnSubmittedWorkDone(queue, (WGPUQueueWorkDoneCallbackInfo){
                                                                   .callback = WEBGPU_INTERNAL_FenceCallback,
                                                                   .mode = WGPUCallbackMode_WaitAnyOnly,
                                                                   .nextInChain = NULL,
                                                                   .userdata1 = fence,
                                                                   .userdata2 = NULL,
                                                               });

    return fence;
}

static WebGPUFence *WEBGPU_INTERNAL_CreateFenceFromFuture(WGPUFuture future)
{
    WebGPUFence *fence = SDL_calloc(1, sizeof(*fence));
    SDL_SetAtomicInt(&fence->status, 0);

    fence->future.future = future;

    return fence;
}

static void WEBGPU_INTERNAL_ReregisterFence(WGPUQueue queue, WebGPUFence *fence)
{
    if (fence == NULL) {
        return;
    }

    SDL_SetAtomicInt(&fence->status, 0);
    fence->future.future = wgpuQueueOnSubmittedWorkDone(queue, (WGPUQueueWorkDoneCallbackInfo){
                                                                   .callback = WEBGPU_INTERNAL_FenceCallback,
                                                                   .mode = WGPUCallbackMode_WaitAnyOnly,
                                                                   .nextInChain = NULL,
                                                                   .userdata1 = fence,
                                                                   .userdata2 = NULL,
                                                               });
}

static bool WEBGPU_INTERNAL_QueryFence(WebGPURenderer *renderer, WebGPUFence *fence)
{
    if (fence != NULL) {
        if ((bool)SDL_GetAtomicInt(&fence->status)) {
            return true;
        }
        // Despite its name, WaitAny isn't actually blocking unless the TimedWaitAny instance feature is enabled,
        // and you give a value to the timeoutNS argument.
        wgpuInstanceWaitAny(renderer->instance, 1, &fence->future, 0);

        SDL_SetAtomicInt(&fence->status, fence->future.completed);

        return (bool)SDL_GetAtomicInt(&fence->status);
    } else {
        return true;
    }
}

static bool WEBGPU_QueryFence(SDL_GPURenderer *device, SDL_GPUFence *fence)
{
    return WEBGPU_INTERNAL_QueryFence((WebGPURenderer *)device, (WebGPUFence *)fence);
}

static bool WEBGPU_INTERNAL_WaitForFences(WebGPURenderer *renderer, bool waitAll, WebGPUFence *const *fences, Uint32 num_fences)
{
    Uint32 triggeredFenceCount = 0;
    Uint32 triggeredFenceThreshold = waitAll ? num_fences : 1;

    while (triggeredFenceCount < triggeredFenceThreshold) {
        for (int i = 0; i < num_fences; i++) {
            if (WEBGPU_INTERNAL_QueryFence(renderer, fences[i])) {
                triggeredFenceCount++;
            }

            // Spin to appease Emscripten.
            SDL_DelayNS(100);
        }
    }

    // TODO: Timeout functionality?
    return true;
}

static bool WEBGPU_WaitForFences(SDL_GPURenderer *device, bool waitAll, SDL_GPUFence *const *fences, Uint32 num_fences)
{
    return WEBGPU_INTERNAL_WaitForFences((WebGPURenderer *)device, waitAll, (WebGPUFence *const *)fences, num_fences);
}

static bool WEBGPU_Wait(SDL_GPURenderer *driverData)
{
    return WEBGPU_WaitForFences(driverData, true, (SDL_GPUFence **)&((WebGPURenderer *)driverData)->queueDoneFence, 1);
}

static inline size_t WEBGPU_INTERNAL_GetTokenBindGroup(const char *token)
{
    char *search = SDL_strstr(token, "@group(");

    if (search != NULL) {
        int group;

        if (SDL_sscanf(search, "@group(%d)", &group)) {
            return group;
        }
    }

    SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not parse bind group from token.\nToken: '%s'", token);
    return -1;
}

static inline int WEBGPU_INTERNAL_GetTokenBindLocation(const char *token)
{
    char *search = SDL_strstr(token, "@binding(");

    if (search != NULL) {
        int binding;

        if (SDL_sscanf(search, "@binding(%d)", &binding)) {
            return binding;
        }
    }

    SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not parse bind location from token.\nToken: '%s'", token);
    return -1;
}

static inline WGPUTextureViewDimension WEBGPU_INTERNAL_GetTextureViewDimensionFromToken(const char *token)
{
    for (size_t i = 0; i < 17; i++) {
        if (SDL_strstr(token, WGSLTextureViewDimensionIdentifiers[i])) {
            return WGSLTextureViewDimensionIdentifiersIndexIHateNamingVariables[i];
        };
    }

    SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not parse texture view dimension from token.\nToken: '%s'", token);
    return WGPUTextureViewDimension_Undefined;
}

static inline WGPUStorageTextureAccess WEBGPU_INTERNAL_GetStorageTextureAccessFromToken(const char *token)
{
    // TODO: Check for incompatibilities
    for (size_t i = 0; i < 3; i++) {
        if (SDL_strstr(token, WGSLStorageTextureAccessIdentifiers[i])) {
            return WGSLStorageTextureAccessIdentifiersIndexWowISuckAtNamingThings[i];
        }
    }

    SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not parse storage texture access type from token.\nToken: '%s'", token);
    return WGPUStorageTextureAccess_Undefined;
}

static inline WGPUTextureFormat WEBGPU_INTERNAL_GetTextureFormatFromToken(const char *token)
{
    for (size_t i = 0; i < SDL_arraysize(WGSLTextureFormatIdentifiers); i++) {
        if (SDL_strstr(token, WGSLTextureFormatIdentifiers[i])) {
            // TODO: Check if format has invalid usages?
            return WGSLTextureFormatIdentifiersIndexThingamabob[i];
        }
    }

    return WGPUTextureFormat_Undefined;
}
static inline WGPUSamplerBindingType WEBGPU_INTERNAL_GetSamplerBindTypeFromToken(const char *token)
{
    // really jank but who cares
    if (SDL_strstr(token, "sampler_comparison")) {
        return WGPUSamplerBindingType_Comparison;
    } else {
        return WGPUSamplerBindingType_Filtering;
    }
}

static inline bool WEBGPU_INTERNAL_TokenIsDepthTexture(const char *token)
{
    const char *depthTextureIdentifiers[5] = {
        "texture_depth_2d_array",
        "texture_depth_2d",
        "texture_depth_cube_array",
        "texture_depth_cube",
        "texture_depth_multisampled_2d",
    };

    for (int i = 0; i < 5; i++) {
        if (SDL_strstr(token, depthTextureIdentifiers[i])) {
            return true;
        }
    }

    return false;
}

static inline bool WEBGPU_INTERNAL_TokenIsMultisampledTexture(const char *token)
{
    const char *multisampledTextureIdentifiers[2] = {
        "texture_multisampled_2d",
        "texture_depth_multisampled_2d",
    };

    for (int i = 0; i < 2; i++) {
        if (SDL_strstr(token, multisampledTextureIdentifiers[i])) {
            return true;
        }
    }

    return false;
}

static WebGPUBindGroupEntryType WEBGPU_INTERNAL_GetEntryTypeFromToken(const char *token)
{
    if (!token) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Attempting to get entry type of NULL token!");
        return WEBGPU_BIND_GROUP_ENTRY_TYPE_UNKNOWN;
    }

    if (SDL_strstr(token, "sampler")) {
        return WEBGPU_BIND_GROUP_ENTRY_TYPE_SAMPLER;
    }

    // not a sampler, let's check texture
    for (int i = 0; i < SDL_arraysize(WGSLTextureIdentifiers); i++) {
        if (SDL_strstr(token, WGSLTextureIdentifiers[i])) {
            return WEBGPU_BIND_GROUP_ENTRY_TYPE_TEXTURE;
        }
    }

    // not a sampled texture, let's check storage texture
    for (int i = 0; i < SDL_arraysize(WGSLStorageTextureIdentifiers); i++) {
        if (SDL_strstr(token, WGSLStorageTextureIdentifiers[i])) {
            return WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_TEXTURE;
        }
    }

    // not a storage texture, we'll check if it has "storage" in it, and if so, we'll presume it's a storage buffer.
    // kinda garbage way of doing it, but it should work 🤞
    if (SDL_strstr(token, "var<storage")) {
        return WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_BUFFER;
    }

    SDL_LogError(SDL_LOG_CATEGORY_GPU, "Could not figure out entry type for token!\nToken: %s\n", token);
    return WEBGPU_BIND_GROUP_ENTRY_TYPE_UNKNOWN;
}

// FIXME: Commented out lines are still parsed!
static Uint32 WEBGPU_INTERNAL_ParseBindGroupLayoutEntriesFromShader(const char *shaderSource, WebGPUInferredBindGroupLayoutEntry **storePtr)
{
    WebGPUInferredBindGroupLayoutEntry *entries = NULL;
    Uint32 entryCount = 0;
    Uint32 entryCapacity = 0;

    // These are the key words that are used to define a binding.
    // It searches sequentially, where it first finds "@group", then "@binding", then "var"
    // This is pretty much a poor man's regex, since SDL doesn't have any support for regular expressions, and I won't be the one to add it.
    // Regexes are overrated anyways, who needs reliability? - TheStickmahn, The Art Of C
    const char *keywords[3] = { "@group", "@binding", "var" };

    // HACK: Alright, if you want to sample a texture, but that texture doesn't support sampling (e.g, it's a depth texture), you can use
    // SDLGPU_ForceAllowSamplingForTexture(group, binding). That forces that binding to use WGPUTextureSampleType_UnfilterableFloat.
    // Now, importantly: This is a HACK. If you want to use a depth texture, you're much better off using the actual "texture_depth_2d" type in WGSL.
    // However, both Tint and Naga never use that, since WGSL is (seemingly) the only shader language that cares.
    // So, you'd need to rewrite your code to use "sampler_comparison" instead of "sampler", which is a giant pain.
    // Code snippet:
    /*
     * //SDLGPU_ForceAllowSamplingForTexture(2, 0)
     * @group(2u) @binding(0u) var DepthTexture: texture_2d<f32>
     */

    const char *sourceView = shaderSource;
    const char *sourceEnd = shaderSource + SDL_strlen(shaderSource);

    const char *parsedBegin = NULL;
    const char *parsedEnd = NULL;

    Uint32 keywordProgress = 0;

    char **hits = NULL;
    Uint32 hitsCount = 0;
    Uint32 hitsCapacity = 0;

    char *trimmed = SDL_calloc(1, SDL_strlen(shaderSource));
    char *trimmedView = trimmed;
    char *trimmedEnd = trimmed;

    // gross hack
    bool isBindingLocationUnfilterable[4][MAX_TEXTURE_SAMPLERS_PER_STAGE] = { 0 };

    // We'll remove all of the whitespace from shaderSource to make parsing easier
    while (sourceView < sourceEnd) {
        if (!SDL_isspace((unsigned char)*sourceView)) {
            *trimmedView++ = *sourceView;
            trimmedEnd = trimmedView;
        }
        sourceView++;
    }

    trimmedView = trimmed;
    while (trimmedView < trimmedEnd) {
        // First we'll check for SDLGPU_ForceAllowSamplingForTexture.
        char *hackFix = SDL_strstr(trimmedView, "SDLGPU_ForceAllowSamplingForTexture");
        if (hackFix != NULL) {
            int group = 0;
            int binding = 0;
            if (SDL_sscanf(hackFix, "SDLGPU_ForceAllowSamplingForTexture(%d,%d)", &group, &binding) != 0) {
                if (group < 4 && binding < 16) {
                    isBindingLocationUnfilterable[group][binding] = true;
                }
            }

            trimmedView = hackFix;
        }

        char *token = SDL_strstr(trimmedView, keywords[keywordProgress]);

        if (token == NULL) {
            keywordProgress = 0;
            trimmedView = trimmedEnd;
            break;
        }

        if (keywordProgress == 0) {
            parsedBegin = token;
        } else if (keywordProgress == 2) {
            // We'll end at the first semicolon we find
            parsedEnd = SDL_strstr(token, ";");
            char *parsed = SDL_strndup(parsedBegin, parsedEnd - parsedBegin); // We could *probably* just use another view for this but I'm really bad with C strings so I'd rather do this

            WEBGPU_INTERNAL_InsertElementIntoArray(hits, hitsCapacity, hitsCount, char *, parsed);
        }

        keywordProgress = keywordProgress == 2 ? 0 : keywordProgress + 1;
        trimmedView = token;
    }

    // Alright, we've now got an array of binding definitions (i don't really know what to call them lmao),
    // so now we'll look through each one and attempt to figure out what they are
    for (int i = 0; i < hitsCount; i++) {
        WebGPUInferredBindGroupLayoutEntry entry = { 0 };

        char *bind = hits[i];
        bool isUniform = SDL_strstr(bind, "<uniform>") != NULL;

        if (isUniform) {
            continue; // Uniform layouts are constant
        }

        entry.group = WEBGPU_INTERNAL_GetTokenBindGroup(bind);
        entry.binding = WEBGPU_INTERNAL_GetTokenBindLocation(bind);
        entry.type = WEBGPU_INTERNAL_GetEntryTypeFromToken(bind);

        switch (entry.type) {
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_UNKNOWN:
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Unknown entry type for parsed binding!"); // Wow I'm bad at writing error messages
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_SAMPLER:
            // Basic bounds checking, doesn't actually check to see if the binding is in the right group as defined by SDLGPU
            if (entry.group < 4 && entry.binding < 16) {
                entry.sampler.bindType = (isBindingLocationUnfilterable[entry.group][SDL_max(entry.binding - 1, 0)]) ? WGPUSamplerBindingType_NonFiltering : WEBGPU_INTERNAL_GetSamplerBindTypeFromToken(bind);
            }
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_TEXTURE:
            entry.texture.dimension = WEBGPU_INTERNAL_GetTextureViewDimensionFromToken(bind);
            entry.texture.format = WEBGPU_INTERNAL_GetTextureFormatFromToken(bind);
            entry.texture.isDepth = WEBGPU_INTERNAL_TokenIsDepthTexture(bind);
            entry.texture.isMultisampled = WEBGPU_INTERNAL_TokenIsMultisampledTexture(bind);
            if (entry.group < 4 && entry.binding < 16) {
                entry.texture.isForciblyUnfilterable = isBindingLocationUnfilterable[entry.group][entry.binding];
            }
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_BUFFER:
            // TODO: Throw this into an active volcano
            entry.storageBuffer.canRead = SDL_strstr(bind, "read") || SDL_strstr(bind, "read_write");
            entry.storageBuffer.canWrite = SDL_strstr(bind, "write") || SDL_strstr(bind, "read_write");
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_TEXTURE:
            entry.storageTexture.dimension = WEBGPU_INTERNAL_GetTextureViewDimensionFromToken(bind);
            entry.storageTexture.format = WEBGPU_INTERNAL_GetTextureFormatFromToken(bind);
            entry.storageTexture.access = WEBGPU_INTERNAL_GetStorageTextureAccessFromToken(bind);
            break;
        }

        WEBGPU_INTERNAL_InsertElementIntoArray(entries, entryCapacity, entryCount, WebGPUInferredBindGroupLayoutEntry, entry);
    }

    for (int i = 0; i < hitsCount; i++) {
        SDL_free(hits[i]);
    }
    SDL_free(trimmed);
    SDL_free(hits);

    *storePtr = entries;
    return entryCount;
}

// Alright I did some shoddy benchmarking of this.
//
// On a single 7950X thread with DDR5-6000 running on Linux (Release build, ofc)
// this can chug through about 18MiB of shader source code a second.
//
// Really slow, but for our purposes it'll work.
//
// For context: Tint's autogenerated ports of all the SDL_gpu_example shaders comes out to about 26KiB in total, which would take
// roughly 1-2 MS.
//
// The larger issue with this is that we're memory leaking. I'm bad with C strings so I don't know /where/ it's leaking but I do know it is.
static WebGPUShaderBindGroupLayouts *WEBGPU_INTERNAL_GenerateBindGroupLayoutsForShader(const char *shaderSource,
                                                                                       WebGPURenderer *renderer,
                                                                                       WGPUShaderStage stage)
{
    WebGPUShaderBindGroupLayouts *result = SDL_calloc(1, sizeof(WebGPUShaderBindGroupLayouts));
    WebGPUInferredBindGroupLayoutEntry *entries = NULL;
    Uint32 numParsedEntries = WEBGPU_INTERNAL_ParseBindGroupLayoutEntriesFromShader(shaderSource, &entries);

    WGPUBindGroupLayoutEntry *samplerEntries = NULL;
    WGPUBindGroupLayoutEntry *uniformEntries = NULL;

    WGPUBindGroupLayoutDescriptor samplerLayoutDesc = { 0 };
    WGPUBindGroupLayoutDescriptor uniformLayoutDesc = { 0 };

    // We've seperated samplerEntryCount on the off-chance that an invalid entry shows up (somehow)
    Uint32 samplerEntryCount = 0;
    Uint32 samplerEntryCapacity = 0;

    // uniforms are constant bcuz i'm lazy
    uniformEntries = SDL_calloc(4, sizeof(*uniformEntries));

    for (int i = 0; i < numParsedEntries; i++) {
        WebGPUInferredBindGroupLayoutEntry *parsedEntry = &entries[i];
        WGPUBindGroupLayoutEntry entry = { 0 };
        if (!parsedEntry) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "NULL entry in parsed entries!");
            continue;
        }

        entry.binding = parsedEntry->binding;
        entry.visibility = stage;

        switch (parsedEntry->type) {
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_UNKNOWN:
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Parsed bind group entry has unknown type!");
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_SAMPLER:
            entry.sampler.type = parsedEntry->sampler.bindType;
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_TEXTURE:
            entry.texture.viewDimension = parsedEntry->texture.dimension;
            entry.texture.multisampled = parsedEntry->texture.isMultisampled;
            entry.texture.nextInChain = NULL;

            if (parsedEntry->texture.isForciblyUnfilterable) {
                entry.texture.sampleType = WGPUTextureSampleType_UnfilterableFloat;
            } else if (parsedEntry->texture.isDepth) {
                entry.texture.sampleType = WGPUTextureSampleType_Depth;
            } else {
                entry.texture.sampleType = WGPUTextureSampleType_Float;
            }
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_BUFFER:
            entry.buffer.type = parsedEntry->storageBuffer.canWrite ? WGPUBufferBindingType_Storage : WGPUBufferBindingType_ReadOnlyStorage;
            entry.buffer.nextInChain = NULL;
            entry.buffer.minBindingSize = 0;
            entry.buffer.hasDynamicOffset = false;
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_TEXTURE:
            entry.storageTexture.access = parsedEntry->storageTexture.access;
            entry.storageTexture.format = parsedEntry->storageTexture.format;
            entry.storageTexture.viewDimension = parsedEntry->storageTexture.dimension;
            entry.storageTexture.nextInChain = NULL;
            break;
        }

        WEBGPU_INTERNAL_InsertElementIntoArray(samplerEntries, samplerEntryCapacity, samplerEntryCount, WGPUBindGroupLayoutEntry, entry);
    }

    samplerLayoutDesc.entryCount = samplerEntryCount;
    samplerLayoutDesc.entries = samplerEntries;
    samplerLayoutDesc.label.data = "Sampler Storage Bind Group Layout (Render)";
    samplerLayoutDesc.label.length = SDL_strlen(samplerLayoutDesc.label.data);
    samplerLayoutDesc.nextInChain = NULL;

    for (int i = 0; i < 4; i++) {
        uniformEntries[i] = (WGPUBindGroupLayoutEntry){ 0 };
        uniformEntries[i].visibility = stage;
        uniformEntries[i].nextInChain = NULL;
        uniformEntries[i].binding = i;
        uniformEntries[i].buffer = (WGPUBufferBindingLayout){
            .minBindingSize = 0,
            .nextInChain = NULL,
            .hasDynamicOffset = true,
            .type = WGPUBufferBindingType_Uniform,
        };
    }

    uniformLayoutDesc.entryCount = 4;
    uniformLayoutDesc.entries = uniformEntries;
    uniformLayoutDesc.label.data = "Uniform Buffer Bind Group Layout (Render)";
    uniformLayoutDesc.label.length = SDL_strlen(uniformLayoutDesc.label.data);
    uniformLayoutDesc.nextInChain = NULL;

    result->uniformBindGroupLayout = wgpuDeviceCreateBindGroupLayout(renderer->device, &uniformLayoutDesc);
    result->samplerStorageBindGroupLayout = wgpuDeviceCreateBindGroupLayout(renderer->device, &samplerLayoutDesc);

    SDL_free(samplerEntries);
    SDL_free(uniformEntries);

    SDL_free(entries);
    return result;
}

static WebGPUComputeShaderBindGroupLayouts *WEBGPU_INTERNAL_GenerateBindGroupLayoutsForComputeShader(const char *shaderSource,
                                                                                                     WebGPURenderer *renderer)
{
    WebGPUComputeShaderBindGroupLayouts *result = SDL_calloc(1, sizeof(WebGPUComputeShaderBindGroupLayouts));
    WebGPUInferredBindGroupLayoutEntry *entries = NULL;
    Uint32 numParsedEntries = WEBGPU_INTERNAL_ParseBindGroupLayoutEntriesFromShader(shaderSource, &entries);

    WGPUBindGroupLayoutEntry *samplerEntries = NULL;
    WGPUBindGroupLayoutEntry *readWriteEntries = NULL;
    WGPUBindGroupLayoutEntry *uniformEntries = NULL;

    WGPUBindGroupLayoutDescriptor samplerLayoutDesc = { 0 };
    WGPUBindGroupLayoutDescriptor readWriteLayoutDesc = { 0 };
    WGPUBindGroupLayoutDescriptor uniformLayoutDesc = { 0 };

    Uint32 samplerEntryCount = 0;
    Uint32 samplerEntryCapacity = 0;

    Uint32 readWriteEntryCount = 0;
    Uint32 readWriteEntryCapacity = 0;

    // uniforms are constant bcuz i'm lazy
    uniformEntries = SDL_calloc(4, sizeof(*uniformEntries));

    for (int i = 0; i < numParsedEntries; i++) {
        WebGPUInferredBindGroupLayoutEntry *parsedEntry = &entries[i];
        WGPUBindGroupLayoutEntry entry = { 0 };
        if (!parsedEntry) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "NULL entry in parsed entries!");
            continue;
        }

        // TODO: Check that the entry is in the right bind group

        entry.binding = parsedEntry->binding;
        entry.visibility = WGPUShaderStage_Compute;

        switch (parsedEntry->type) {
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_UNKNOWN:
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Parsed bind group entry has unknown type!");
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_SAMPLER:
            entry.sampler.type = parsedEntry->sampler.bindType;
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_TEXTURE:
            entry.texture.sampleType = parsedEntry->texture.isDepth ? WGPUTextureSampleType_Depth : WebGPUTextureFormatToSampleType(parsedEntry->texture.format);
            entry.texture.viewDimension = parsedEntry->texture.dimension;
            entry.texture.multisampled = parsedEntry->texture.isMultisampled;
            entry.texture.nextInChain = NULL;
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_BUFFER:
            entry.buffer.type = parsedEntry->storageBuffer.canWrite ? WGPUBufferBindingType_Storage : WGPUBufferBindingType_ReadOnlyStorage;
            entry.buffer.nextInChain = NULL;
            entry.buffer.minBindingSize = 0;
            entry.buffer.hasDynamicOffset = false;
            break;
        case WEBGPU_BIND_GROUP_ENTRY_TYPE_STORAGE_TEXTURE:
            entry.storageTexture.access = parsedEntry->storageTexture.access;
            entry.storageTexture.format = parsedEntry->storageTexture.format;
            entry.storageTexture.viewDimension = parsedEntry->storageTexture.dimension;
            entry.storageTexture.nextInChain = NULL;
            break;
        }

        if (parsedEntry->group == 0) {
            WEBGPU_INTERNAL_InsertElementIntoArray(samplerEntries, samplerEntryCapacity, samplerEntryCount, WGPUBindGroupLayoutEntry, entry);
        } else if (parsedEntry->group == 1) {
            WEBGPU_INTERNAL_InsertElementIntoArray(readWriteEntries, readWriteEntryCapacity, readWriteEntryCount, WGPUBindGroupLayoutEntry, entry);
        }
    }

    samplerLayoutDesc.entryCount = samplerEntryCount;
    samplerLayoutDesc.entries = samplerEntries;
    samplerLayoutDesc.label.data = "Sampler Storage Bind Group Layout (Compute)";
    samplerLayoutDesc.label.length = SDL_strlen(samplerLayoutDesc.label.data);
    samplerLayoutDesc.nextInChain = NULL;

    readWriteLayoutDesc.entryCount = readWriteEntryCount;
    readWriteLayoutDesc.entries = readWriteEntries;
    readWriteLayoutDesc.label.data = "Read-Write Storage Bind Group (Compute)";
    readWriteLayoutDesc.label.length = SDL_strlen(readWriteLayoutDesc.label.data);
    readWriteLayoutDesc.nextInChain = NULL;

    for (int i = 0; i < 4; i++) {
        uniformEntries[i] = (WGPUBindGroupLayoutEntry){ 0 };
        uniformEntries[i].visibility = WGPUShaderStage_Compute;
        uniformEntries[i].nextInChain = NULL;
        uniformEntries[i].binding = i;
        uniformEntries[i].buffer = (WGPUBufferBindingLayout){
            .minBindingSize = 0,
            .nextInChain = NULL,
            .hasDynamicOffset = true,
            .type = WGPUBufferBindingType_Uniform,
        };
    }

    uniformLayoutDesc.entryCount = 4;
    uniformLayoutDesc.entries = uniformEntries;
    uniformLayoutDesc.label.data = "Uniform Buffer Bind Group Layout (Compute)";
    uniformLayoutDesc.label.length = SDL_strlen(uniformLayoutDesc.label.data);
    uniformLayoutDesc.nextInChain = NULL;

    result->uniformBindGroupLayout = wgpuDeviceCreateBindGroupLayout(renderer->device, &uniformLayoutDesc);
    result->readWriteStorageBindGroupLayout = wgpuDeviceCreateBindGroupLayout(renderer->device, &readWriteLayoutDesc);
    result->samplerStorageBindGroupLayout = wgpuDeviceCreateBindGroupLayout(renderer->device, &samplerLayoutDesc);

    SDL_free(samplerEntries);
    SDL_free(readWriteEntries);
    SDL_free(uniformEntries);

    SDL_free(entries);
    return result;
}

static WebGPUBindGroup *WEBGPU_INTERNAL_GetBindGroupFromIdentifier(WebGPURenderer *renderer, WebGPUBindGroupCacheKey key)
{
    WebGPUBindGroup *result = NULL;
    SDL_FindInHashTable(renderer->bindGroupHashTable, &key, (const void **)&result);

    return result;
}

static bool WEBGPU_INTERNAL_BindGroupCacheIteratorCallback(void *userdata, const SDL_HashTable *table, const void *key, const void *value)
{
    WebGPURenderer *renderer = userdata;
    WebGPUBindGroup *bindGroup = (WebGPUBindGroup *)value;

    if (bindGroup) {
        if (renderer->numSubmissions - bindGroup->lastUsedAtSubmission > renderer->bindGroupsExpireAfter) {
            WEBGPU_INTERNAL_QueueBindGroupForRelease(renderer, bindGroup);
            bindGroup->invalid = true;
        }
    }

    return true;
}

// This function checks all of the bind groups and releases any that haven't been used in a while.
static void WEBGPU_INTERNAL_TrimBindGroupCache(WebGPURenderer *renderer)
{
    SDL_IterateHashTable(renderer->bindGroupHashTable, WEBGPU_INTERNAL_BindGroupCacheIteratorCallback, renderer);
}

static void WEBGPU_INTERNAL_CheckSubmittedCommandBuffers(WebGPURenderer *renderer)
{
    // We create a new buffer to store the non-completed command buffers.
    // Unoptimized? Yes.
    // Do I care? No.
    WebGPUSubmittedCommandBuffer **notCompleted = NULL;
    Uint32 notCompletedCapacity = 0;
    Uint32 notCompletedCount = 0;

    if (renderer->submittedCommandBufferCount == 0) {
        return;
    }

    for (int i = renderer->submittedCommandBufferCount; i-- > 0;) {
        WebGPUSubmittedCommandBuffer *current = renderer->submittedCommandBuffers[i];

        if (current == NULL) {
            SDL_LogError(SDL_LOG_CATEGORY_GPU, "Submitted command buffer is NULL!");
            continue;
        }

        if (!WEBGPU_INTERNAL_QueryFence(renderer, current->fence)) {
            // Still not done, let's shove it into notCompleted.
            WEBGPU_INTERNAL_InsertElementIntoArray(notCompleted, notCompletedCapacity, notCompletedCount, WebGPUSubmittedCommandBuffer *, current);
        } else {
            WEBGPU_INTERNAL_QueueSubmittedCommandBufferForRelease(renderer, current);
        }
    }

    // By now, all of the entries within submittedCommandBuffers should either have been queued for
    // release, or moved to notCompleted. We'll free submittedCommandBuffers and replace it with notCompleted.
    SDL_free(renderer->submittedCommandBuffers);
    renderer->submittedCommandBufferCapacity = notCompletedCapacity;
    renderer->submittedCommandBufferCount = notCompletedCount;
    renderer->submittedCommandBuffers = notCompleted;
}

static void WEBGPU_INTERNAL_ReleaseTextureContainer(WebGPURenderer *renderer, WebGPUTextureContainer *container);
static void WEBGPU_INTERNAL_ReleaseTexture(WebGPURenderer *renderer, WebGPUTexture *texture);
static void WEBGPU_INTERNAL_ReleaseSampler(WebGPURenderer *renderer, WebGPUSampler *sampler);
static void WEBGPU_INTERNAL_ReleaseBuffer(WebGPURenderer *renderer, WebGPUBuffer *buffer);
static void WEBGPU_INTERNAL_ReleaseBufferContainer(WebGPURenderer *renderer, WebGPUBufferContainer *container);

static void WEBGPU_INTERNAL_HandlePendingDestroys(WebGPURenderer *renderer)
{
    WebGPUQueuedDestroy **newQueuedDestroys = NULL;
    Uint32 newQueuedDestroysCapacity = 0;
    Uint32 newQueuedDestroysCount = 0;

    // This needs to be run before the destruction loop since this function
    // is what queues the submitted command buffers for freeing.
    WEBGPU_INTERNAL_CheckSubmittedCommandBuffers(renderer);
    WEBGPU_INTERNAL_TrimBindGroupCache(renderer);

    if (renderer->queuedDestroys != NULL && renderer->queuedDestroyCount > 0) {
        for (int i = 0; i < renderer->queuedDestroyCount; i++) {
            WebGPUQueuedDestroy *current = renderer->queuedDestroys[i];

            int currentRefCount = 0;
            bool forciblyDestroy = false;
            bool wasReleased = false;

            if (current == NULL) {
                continue;
            }

            if (current->destroysAttempted >= 100 && current->destroysAttempted % 100 == 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Queued destroy has been attempted %i times!", current->destroysAttempted);
            }

            if (current->destroysAttempted >= FORCIBLY_DESTROY_QUEUED_DESTROY_AFTER_N_FAILED) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Queued destroy has been attempted %i times! Forcibly destroying.", current->destroysAttempted);
                forciblyDestroy = true;
            }

            switch (current->type) {
            case WEBGPU_QUEUED_DESTROY_INVALID:
                break;
            case WEBGPU_QUEUED_DESTROY_TEXTURE_CONTAINER:
                // Texture containers don't have any direct references,
                // so we'll loop over all of the textures and add up their counts.
                for (int j = 0; j < current->resource.textureContainer->textureCount; j++) {
                    int refCount = SDL_GetAtomicInt(&current->resource.textureContainer->textures[j]->referenceCount);
                    currentRefCount += refCount;
                }

                if (currentRefCount == 0 || forciblyDestroy) {
                    WEBGPU_INTERNAL_ReleaseTextureContainer(renderer, current->resource.textureContainer);
                    wasReleased = true;
                }
                break;
            case WEBGPU_QUEUED_DESTROY_TEXTURE:
                currentRefCount = SDL_GetAtomicInt(&current->resource.texture->referenceCount);
                if (currentRefCount == 0 || forciblyDestroy) {
                    WEBGPU_INTERNAL_ReleaseTexture(renderer, current->resource.texture);
                    wasReleased = true;
                }
                break;
            case WEBGPU_QUEUED_DESTROY_SAMPLER:
                currentRefCount = current->resource.sampler->numDependants;
                if (currentRefCount == 0 || forciblyDestroy) {
                    WEBGPU_INTERNAL_ReleaseSampler(renderer, current->resource.sampler);
                    wasReleased = true;
                }
                break;
            case WEBGPU_QUEUED_DESTROY_BUFFER_CONTAINER:
                // Like with texture containers, the buffer container doesn't have any direct references.
                for (int j = 0; j < current->resource.bufferContainer->bufferCount; j++) {
                    int refCount = SDL_GetAtomicInt(&current->resource.bufferContainer->buffers[j]->referenceCount);
                    currentRefCount += refCount;
                }
                if (currentRefCount == 0 || forciblyDestroy) {
                    WEBGPU_INTERNAL_ReleaseBufferContainer(renderer, current->resource.bufferContainer);
                    wasReleased = true;
                }
                break;
            case WEBGPU_QUEUED_DESTROY_BUFFER:
                currentRefCount = SDL_GetAtomicInt(&current->resource.buffer->referenceCount);
                if (currentRefCount == 0 || forciblyDestroy) {
                    WEBGPU_INTERNAL_ReleaseBuffer(renderer, current->resource.buffer);
                    wasReleased = true;
                }
                break;
            case WEBGPU_QUEUED_DESTROY_FENCE:
                WEBGPU_INTERNAL_WaitForFences(renderer, true, &current->resource.fence, 1);

                SDL_free(current->resource.fence);
                wasReleased = true;

                break;
            case WEBGPU_QUEUED_DESTROY_SUBMITTED_COMMAND_BUFFER:
                // NOTE: I disabled this. I'm sure this won't come back to bite me in the ass.
                // WEBGPU_INTERNAL_WaitForFences(renderer, true, &current->resource.submittedCommandBuffer->fence, 1);

                for (int j = 0; j < current->resource.submittedCommandBuffer->usedBufferCount; j++) {
                    (void)SDL_AtomicDecRef(&current->resource.submittedCommandBuffer->usedBuffers[j]->referenceCount);
                }

                for (int j = 0; j < current->resource.submittedCommandBuffer->usedTextureCount; j++) {
                    (void)SDL_AtomicDecRef(&current->resource.submittedCommandBuffer->usedTextures[j]->referenceCount);
                }

                SDL_free(current->resource.submittedCommandBuffer->usedTextures);
                SDL_free(current->resource.submittedCommandBuffer->usedBuffers);
                SDL_free(current->resource.submittedCommandBuffer->fence);
                SDL_free(current->resource.submittedCommandBuffer);

                wasReleased = true;
                break;
            case WEBGPU_QUEUED_DESTROY_BIND_GROUP:
                // FIXME: Bind groups don't have any reference counts, so there might be some issues with in-flight usages?
                SDL_RemoveFromHashTable(renderer->bindGroupHashTable, &current->resource.bindGroup->key); // The key value destruction function also releases the bind group.
                wasReleased = true;

                break;
            }

            if (wasReleased) {
                SDL_free(current);
            } else {
                WEBGPU_INTERNAL_InsertElementIntoArray(newQueuedDestroys, newQueuedDestroysCapacity,
                                                       newQueuedDestroysCount, WebGPUQueuedDestroy *, current);
                current->destroysAttempted++;
            }
        }

        SDL_free(renderer->queuedDestroys);

        renderer->queuedDestroyCapacity = newQueuedDestroysCapacity;
        renderer->queuedDestroyCount = newQueuedDestroysCount;
        renderer->queuedDestroys = newQueuedDestroys;
    }
}

static WebGPUBindGroupCacheKey WEBGPU_INTERNAL_GenerateKeyForCurrentBinds(WebGPUCommandBuffer *cmdBuf, WebGPUBindGroupType desiredGroup)
{
    WebGPUBindGroupCacheKey result = { 0 };

    switch (desiredGroup) {
    case WEBGPU_BINDGROUP_VERTEXSAMPLERSTORAGE:
    {
        for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
            result.boundTexturesIDs[i] = cmdBuf->vertexStageBinds.boundTextures[i] ? cmdBuf->vertexStageBinds.boundTextures[i]->identifier : 0;
            result.boundSamplersIDs[i] = cmdBuf->vertexStageBinds.boundSamplers[i] ? cmdBuf->vertexStageBinds.boundSamplers[i]->identifier : 0;
        }

        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            result.boundReadOnlyStorageTexturesIDs[i] = cmdBuf->vertexStageBinds.boundStorageTextures[i] ? cmdBuf->vertexStageBinds.boundStorageTextures[i]->identifier : 0;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            result.boundReadOnlyStorageBuffersIDs[i] = cmdBuf->vertexStageBinds.boundStorageBuffers[i] ? cmdBuf->vertexStageBinds.boundStorageBuffers[i]->identifier : 0;
        }
        break;
    }
    case WEBGPU_BINDGROUP_VERTEXUNIFORMS:
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Attempting to get hash of vertex uniform bind group! The uniform bind groups are currently pregenerated!");
        break;
    }
    case WEBGPU_BINDGROUP_FRAGMENTSAMPLERSTORAGE:
    {
        for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
            result.boundTexturesIDs[i] = cmdBuf->fragmentStageBinds.boundTextures[i] ? cmdBuf->fragmentStageBinds.boundTextures[i]->identifier : 0;
            result.boundSamplersIDs[i] = cmdBuf->fragmentStageBinds.boundSamplers[i] ? cmdBuf->fragmentStageBinds.boundSamplers[i]->identifier : 0;
        }

        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            result.boundReadOnlyStorageTexturesIDs[i] = cmdBuf->fragmentStageBinds.boundStorageTextures[i] ? cmdBuf->fragmentStageBinds.boundStorageTextures[i]->identifier : 0;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            result.boundReadOnlyStorageBuffersIDs[i] = cmdBuf->fragmentStageBinds.boundStorageBuffers[i] ? cmdBuf->fragmentStageBinds.boundStorageBuffers[i]->identifier : 0;
        }
        break;
    }
    case WEBGPU_BINDGROUP_FRAGMENTUNIFORMS:
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Attempting to get hash of fragment uniform bind group! The uniform bind groups are currently pregenerated!");
        break;
    }
    case WEBGPU_BINDGROUP_COMPUTESAMPLERSTORAGE:
    {
        for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
            result.boundTexturesIDs[i] = cmdBuf->computeStageBinds.boundTextures[i] ? cmdBuf->computeStageBinds.boundTextures[i]->identifier : 0;
            result.boundSamplersIDs[i] = cmdBuf->computeStageBinds.boundSamplers[i] ? cmdBuf->computeStageBinds.boundSamplers[i]->identifier : 0;
        }

        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            result.boundReadOnlyStorageTexturesIDs[i] = cmdBuf->computeStageBinds.boundReadOnlyStorageTextures[i] ? cmdBuf->computeStageBinds.boundReadOnlyStorageTextures[i]->identifier : 0;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            result.boundReadOnlyStorageBuffersIDs[i] = cmdBuf->computeStageBinds.boundReadOnlyStorageBuffers[i] ? cmdBuf->computeStageBinds.boundReadOnlyStorageBuffers[i]->identifier : 0;
        }
        break;
    }
    case WEBGPU_BINDGROUP_COMPUTEREADWRITESTORAGE:
    {
        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            result.boundReadWriteStorageTexturesIDs[i] = cmdBuf->computeStageBinds.boundReadWriteStorageTextures[i] ? cmdBuf->computeStageBinds.boundReadWriteStorageTextures[i]->identifier : 0;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            result.boundReadWriteStorageBuffersIDs[i] = cmdBuf->computeStageBinds.boundReadWriteStorageBuffers[i] ? cmdBuf->computeStageBinds.boundReadWriteStorageBuffers[i]->identifier : 0;
        }
        break;
    }
    case WEBGPU_BINDGROUP_COMPUTEUNIFORMS:
    {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Attempting to get hash of compute uniform bind group! The uniform bind groups are currently pregenerated!");
        break;
    }
    }

    return result;
}

static WebGPUBindGroup *WEBGPU_INTERNAL_CreateBindGroup(WebGPUCommandBuffer *cmdBuf, WebGPUBindGroupType type, WebGPUBindGroupCacheKey key)
{
    WebGPUBindGroup *result = NULL;

    WGPUBindGroupDescriptor desc = { 0 };
    WGPUBindGroupEntry *entries = NULL;
    WGPUBindGroup bindGroup = NULL;

    Uint32 offset = 0;

    switch (type) {
    case WEBGPU_BINDGROUP_VERTEXSAMPLERSTORAGE:
    {
        entries = SDL_calloc((MAX_TEXTURE_SAMPLERS_PER_STAGE * 2) + MAX_STORAGE_BUFFERS_PER_STAGE + MAX_STORAGE_TEXTURES_PER_STAGE, sizeof(*entries));

        for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
            WebGPUSampler *samplerBind = cmdBuf->vertexStageBinds.boundSamplers[i];
            WebGPUTextureView *textureBind = cmdBuf->vertexStageBinds.boundTextures[i];

            if (samplerBind == NULL && textureBind != NULL) {
                SDL_assert_release(!"(VSS) Texture binding without corresponding sampler binding!");
            } else if (samplerBind != NULL && textureBind == NULL) {
                SDL_assert_release(!"(VSS) Sampler binding without corresponding texture binding!");
            } else if (samplerBind == NULL && textureBind == NULL) {
                continue;
            }

            entries[offset].textureView = textureBind->view;
            entries[offset].binding = offset;
            offset++;

            entries[offset].sampler = samplerBind->sampler;
            entries[offset].binding = offset;
            offset++;
        }

        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            WebGPUTextureView *textureBind = cmdBuf->vertexStageBinds.boundStorageTextures[i];
            if (textureBind == NULL) {
                continue;
            }

            entries[offset].textureView = textureBind->view;
            entries[offset].binding = offset;
            offset++;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            WebGPUBuffer *bufferBind = cmdBuf->vertexStageBinds.boundStorageBuffers[i];
            if (bufferBind == NULL) {
                continue;
            }

            entries[offset].buffer = bufferBind->buffer;
            entries[offset].size = WGPU_WHOLE_SIZE;
            entries[offset].binding = offset;
            offset++;
        }

        desc.entryCount = offset;
        desc.layout = cmdBuf->boundGraphicsPipeline->vertexBindGroupLayouts.samplerStorageBindGroupLayout;
        break;
    }
    case WEBGPU_BINDGROUP_VERTEXUNIFORMS:
    {
        SDL_assert_release(!"Attempting to create vertex uniform bind group! They're currently only pre-generated!");
    }
    case WEBGPU_BINDGROUP_FRAGMENTSAMPLERSTORAGE:
    {
        // We're almost always allocating more slots than needed, but since entries gets freed at the end of this function anyways it doesn't matter much.
        entries = SDL_calloc((MAX_TEXTURE_SAMPLERS_PER_STAGE * 2) + MAX_STORAGE_BUFFERS_PER_STAGE + MAX_STORAGE_TEXTURES_PER_STAGE, sizeof(*entries));

        for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
            WebGPUSampler *samplerBind = cmdBuf->fragmentStageBinds.boundSamplers[i];
            WebGPUTextureView *textureBind = cmdBuf->fragmentStageBinds.boundTextures[i];

            if (samplerBind == NULL && textureBind != NULL) {
                SDL_assert_release(!"(VSS) Texture binding without corresponding sampler binding!");
            } else if (samplerBind != NULL && textureBind == NULL) {
                SDL_assert_release(!"(VSS) Sampler binding without corresponding texture binding!");
            } else if (samplerBind == NULL && textureBind == NULL) {
                continue;
            }

            entries[offset].textureView = textureBind->view;
            entries[offset].binding = offset;
            offset++;

            entries[offset].sampler = samplerBind->sampler;
            entries[offset].binding = offset;
            offset++;
        }

        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            WebGPUTextureView *textureBind = cmdBuf->fragmentStageBinds.boundStorageTextures[i];
            if (textureBind == NULL) {
                continue;
            }

            entries[offset].textureView = textureBind->view;
            entries[offset].binding = offset;
            offset++;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            WebGPUBuffer *bufferBind = cmdBuf->fragmentStageBinds.boundStorageBuffers[i];
            if (bufferBind == NULL) {
                continue;
            }

            entries[offset].buffer = bufferBind->buffer;
            entries[offset].size = WGPU_WHOLE_SIZE;
            entries[offset].binding = offset;
            offset++;
        }

        desc.entryCount = offset;
        desc.layout = cmdBuf->boundGraphicsPipeline->fragmentBindGroupLayouts.samplerStorageBindGroupLayout;

        break;
    }
    case WEBGPU_BINDGROUP_FRAGMENTUNIFORMS:
    {
        SDL_assert_release(!"Attempting to create fragment uniform bind group! They're currently only pre-generated!");
    }
    case WEBGPU_BINDGROUP_COMPUTESAMPLERSTORAGE:
    {
        entries = SDL_calloc((MAX_TEXTURE_SAMPLERS_PER_STAGE * 2) + MAX_STORAGE_BUFFERS_PER_STAGE + MAX_STORAGE_TEXTURES_PER_STAGE, sizeof(*entries));

        for (int i = 0; i < MAX_TEXTURE_SAMPLERS_PER_STAGE; i++) {
            WebGPUSampler *samplerBind = cmdBuf->computeStageBinds.boundSamplers[i];
            WebGPUTextureView *textureBind = cmdBuf->computeStageBinds.boundTextures[i];

            if (samplerBind == NULL && textureBind != NULL) {
                SDL_assert_release(!"(CSS) Texture binding without corresponding sampler binding!");
            } else if (samplerBind != NULL && textureBind == NULL) {
                SDL_assert_release(!"(CSS) Sampler binding without corresponding texture binding!");
            } else if (samplerBind == NULL && textureBind == NULL) {
                continue;
            }

            entries[offset].textureView = textureBind->view;
            entries[offset].binding = offset;
            offset++;

            entries[offset].sampler = samplerBind->sampler;
            entries[offset].binding = offset;
            offset++;
        }

        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            WebGPUTextureView *textureBind = cmdBuf->computeStageBinds.boundReadOnlyStorageTextures[i];
            if (textureBind == NULL) {
                continue;
            }

            entries[offset].textureView = textureBind->view;
            entries[offset].binding = offset;
            offset++;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            WebGPUBuffer *bufferBind = cmdBuf->computeStageBinds.boundReadOnlyStorageBuffers[i];
            if (bufferBind == NULL) {
                continue;
            }

            entries[offset].buffer = bufferBind->buffer;
            entries[offset].size = WGPU_WHOLE_SIZE;
            entries[offset].binding = offset;
            offset++;
        }

        desc.entryCount = offset;
        desc.layout = cmdBuf->boundComputePipeline->bindGroupLayouts->samplerStorageBindGroupLayout;
        break;
    }
    case WEBGPU_BINDGROUP_COMPUTEREADWRITESTORAGE:
    {
        entries = SDL_calloc(MAX_STORAGE_BUFFERS_PER_STAGE + MAX_STORAGE_TEXTURES_PER_STAGE, sizeof(*entries));

        for (int i = 0; i < MAX_STORAGE_TEXTURES_PER_STAGE; i++) {
            WebGPUTextureView *textureBind = cmdBuf->computeStageBinds.boundReadWriteStorageTextures[i];
            if (textureBind == NULL) {
                continue;
            }

            entries[offset].textureView = textureBind->view;
            entries[offset].binding = offset;
            offset++;
        }

        for (int i = 0; i < MAX_STORAGE_BUFFERS_PER_STAGE; i++) {
            WebGPUBuffer *bufferBind = cmdBuf->computeStageBinds.boundReadWriteStorageBuffers[i];
            if (bufferBind == NULL) {
                continue;
            }

            entries[offset].buffer = bufferBind->buffer;
            entries[offset].size = WGPU_WHOLE_SIZE;
            entries[offset].binding = offset;
            offset++;
        }

        desc.entryCount = offset;
        desc.layout = cmdBuf->boundComputePipeline->bindGroupLayouts->readWriteStorageBindGroupLayout;
        break;
    }
    case WEBGPU_BINDGROUP_COMPUTEUNIFORMS:
        SDL_assert_release(!"Attempting to create compute uniform bind group! They're currently only pre-generated!");
    }

    desc.entries = entries;
    desc.label = (WGPUStringView){ NULL, 0 };
    desc.nextInChain = NULL;

    bindGroup = wgpuDeviceCreateBindGroup(cmdBuf->device, &desc);

    if (bindGroup == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create bind group!");
    }

    result = SDL_calloc(1, sizeof(*result));

    result->bindGroup = bindGroup;
    result->key = key;
    result->invalid = false;

    SDL_InsertIntoHashTable(cmdBuf->renderer->bindGroupHashTable, &result->key, result, true);

    SDL_free(entries);
    return result;
}

// Get a bind group for the currently queued bind resources.
// Will create one if no bind group already exists.
static WGPUBindGroup WEBGPU_INTERNAL_GetBindGroup(WebGPUCommandBuffer *cmdBuf, WebGPUBindGroupType type)
{
    if (type == WEBGPU_BINDGROUP_VERTEXUNIFORMS) {
        return cmdBuf->renderer->uniformBufferBindGroups[0];
    } else if (type == WEBGPU_BINDGROUP_FRAGMENTUNIFORMS) {
        return cmdBuf->renderer->uniformBufferBindGroups[1];
    } else if (type == WEBGPU_BINDGROUP_COMPUTEUNIFORMS) {
        return cmdBuf->renderer->uniformBufferBindGroups[2];
    }

    WebGPUBindGroupCacheKey key = WEBGPU_INTERNAL_GenerateKeyForCurrentBinds(cmdBuf, type);
    WebGPUBindGroup *result = WEBGPU_INTERNAL_GetBindGroupFromIdentifier(cmdBuf->renderer, key);

    if (result == NULL) {
        result = WEBGPU_INTERNAL_CreateBindGroup(cmdBuf, type, key);
    }

    result->lastUsedAtSubmission = cmdBuf->renderer->numSubmissions + 1;

    return result->bindGroup;
}

static WebGPUTextureView *WEBGPU_INTERNAL_CreateTextureView(WebGPURenderer *renderer, WebGPUTexture *texture, int depth, int mipLevel)
{
    WebGPUTextureView *result = NULL;

    WGPUTextureFormat format = wgpuTextureGetFormat(texture->texture);
    WGPUTextureUsage usages = wgpuTextureGetUsage(texture->texture);
    WGPUTextureViewDimension dimension = WGPUTextureViewDimension_Undefined;

    Uint32 mipLevelCount = wgpuTextureGetMipLevelCount(texture->texture);
    Uint32 arrayLayerCount = wgpuTextureGetDepthOrArrayLayers(texture->texture);

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
        // if we're only making a texture view slice, we can't use the cube argument
        dimension = depth < 0 ? WGPUTextureViewDimension_Cube : WGPUTextureViewDimension_2D;
        break;
    case SDL_GPU_TEXTURETYPE_CUBE_ARRAY:
        dimension = depth < 0 ? WGPUTextureViewDimension_CubeArray : WGPUTextureViewDimension_2D;
        break;
    }

    WGPUTextureViewDescriptor desc = {
        .aspect = texture->aspect,
        .dimension = dimension,
        .format = format,
        .label = { 0 },
        .usage = usages,
    };

    if (depth < 0) {
        // create whole texture view
        desc.arrayLayerCount = dimension == WGPUTextureViewDimension_2D || dimension == WGPUTextureViewDimension_3D ? 1 : arrayLayerCount;
        desc.baseArrayLayer = 0;
        desc.mipLevelCount = mipLevelCount;
        desc.baseMipLevel = 0;
    } else {
        desc.arrayLayerCount = 1;
        desc.baseArrayLayer = depth;
        desc.mipLevelCount = 1;
        desc.baseMipLevel = mipLevel;
    }

    result = SDL_calloc(1, sizeof(*result));

    result->identifier = renderer->nextBindableResourceID++;
    result->view = wgpuTextureCreateView(texture->texture, &desc);

    return result;
}

// Forward decl
static bool WEBGPU_SupportsTextureFormat(SDL_GPURenderer *driverData, SDL_GPUTextureFormat format, SDL_GPUTextureType type, SDL_GPUTextureUsageFlags usage);
static WebGPUTexture *WEBGPU_INTERNAL_CreateTexture(WebGPURenderer *renderer, const SDL_GPUTextureCreateInfo *createInfo)
{
    WebGPUTexture *texture;

    texture = (WebGPUTexture *)SDL_calloc(1, sizeof(*texture));
    WGPUTextureDescriptor desc = { 0 };

    if (!WEBGPU_SupportsTextureFormat((SDL_GPURenderer *)renderer, createInfo->format, createInfo->type, createInfo->usage)) {
        SDL_assert_release(!"Texture format is not supported!");
    }

    switch (createInfo->type) {
    case SDL_GPU_TEXTURETYPE_2D:
    case SDL_GPU_TEXTURETYPE_2D_ARRAY:
    case SDL_GPU_TEXTURETYPE_CUBE:
    case SDL_GPU_TEXTURETYPE_CUBE_ARRAY:
        desc.dimension = WGPUTextureDimension_2D;
        break;

    case SDL_GPU_TEXTURETYPE_3D:
        desc.dimension = WGPUTextureDimension_3D;
        break;
    }

    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_SAMPLER) {
        desc.usage |= WGPUTextureUsage_TextureBinding;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET) {
        desc.usage |= WGPUTextureUsage_RenderAttachment;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET) {
        desc.usage |= WGPUTextureUsage_RenderAttachment;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ || createInfo->usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ) {
        desc.usage |= WGPUTextureUsage_StorageBinding;
        desc.usage |= WGPUTextureUsage_TextureBinding;
    }
    if (createInfo->usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE || createInfo->usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE) {
        desc.usage |= WGPUTextureUsage_StorageBinding;
        desc.usage |= WGPUTextureUsage_TextureBinding;
    }

    desc.usage |= WGPUTextureUsage_CopyDst;
    desc.usage |= WGPUTextureUsage_CopySrc;

    const char *debugName = SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_TEXTURE_CREATE_NAME_STRING, NULL);

    Uint32 blockWidth = Texture_GetBlockWidth(createInfo->format);
    Uint32 blockHeight = Texture_GetBlockHeight(createInfo->format);

    desc.label = debugName != NULL ? (WGPUStringView){ debugName, SDL_strlen(debugName) } : (WGPUStringView){ NULL, 0 };
    desc.format = SDLToWebGPU_TextureFormat[createInfo->format];
    desc.mipLevelCount = createInfo->num_levels;
    desc.size = (WGPUExtent3D){
        .width = ALIGN_VALUE(createInfo->width, blockWidth),
        .height = ALIGN_VALUE(createInfo->height, blockHeight),
        .depthOrArrayLayers = createInfo->type == SDL_GPU_TEXTURETYPE_2D ? 1 : createInfo->layer_count_or_depth
    };

    switch (createInfo->sample_count) {
    case SDL_GPU_SAMPLECOUNT_1:
        desc.sampleCount = 1;
        break;
    case SDL_GPU_SAMPLECOUNT_4:
        desc.sampleCount = 4;
        break;
    case SDL_GPU_SAMPLECOUNT_2:
    case SDL_GPU_SAMPLECOUNT_8:
        SDL_assert_release(!"Texture sample count must be 1 or 4! Blame WebGPU.");
        break;
    }

    texture->type = createInfo->type;
    texture->format = createInfo->format;
    SDL_SetAtomicInt(&texture->referenceCount, 0);
    texture->texture = wgpuDeviceCreateTexture(renderer->device, &desc);
    // -1 signals that we want a texture view for all of the layers
    texture->fullTextureView = WEBGPU_INTERNAL_CreateTextureView(renderer, texture, -1, 0);

    if (createInfo->type == SDL_GPU_TEXTURETYPE_3D) {
        // manually create view
        WebGPUTextureView *textureView = WEBGPU_INTERNAL_CreateTextureView(renderer, texture, 0, 0);
        WEBGPU_INTERNAL_InsertElementIntoArray(texture->textureViews, texture->textureViewCapacity,
                                               texture->textureViewCount, WebGPUTextureView *, textureView);
    } else {
        for (int i = 0; i < desc.size.depthOrArrayLayers; i++) {
            for (int j = 0; j < desc.mipLevelCount; j++) {
                WebGPUTextureView *textureView = WEBGPU_INTERNAL_CreateTextureView(renderer, texture, i, j);

                WEBGPU_INTERNAL_InsertElementIntoArray(texture->textureViews, texture->textureViewCapacity,
                                                       texture->textureViewCount, WebGPUTextureView *, textureView);
            }
        }
    }

    return texture;
}

static SDL_GPUTexture *WEBGPU_CreateTexture(
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
    container->activeTextureIndex = 0;
    container->textureCapacity = 1;
    container->textureCount = 1;
    container->textures = SDL_malloc(
        container->textureCapacity * sizeof(WebGPUTexture *));
    container->textures[0] = container->activeTexture;
    container->header.info = *createInfo;

    texture->container = container;

    return (SDL_GPUTexture *)container;
}

static SDL_GPUSampler *WEBGPU_CreateSampler(SDL_GPURenderer *device, const SDL_GPUSamplerCreateInfo *createInfo)
{
    WebGPUSampler *sampler;
    sampler = SDL_calloc(1, sizeof(*sampler));

    WGPUSamplerDescriptor desc;
    const char *debugName = SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_SAMPLER_CREATE_NAME_STRING, NULL);
    desc.label = debugName != NULL ? (WGPUStringView){ debugName, SDL_strlen(debugName) } : (WGPUStringView){ NULL, 0 };
    desc.nextInChain = NULL;

    desc.addressModeU = SDLToWebGPU_AddressMode[createInfo->address_mode_u];
    desc.addressModeV = SDLToWebGPU_AddressMode[createInfo->address_mode_v];
    desc.addressModeW = SDLToWebGPU_AddressMode[createInfo->address_mode_w];
    desc.compare = SDLToWebGPU_CompareFunc[createInfo->compare_op];

    desc.lodMaxClamp = createInfo->max_lod == 0 ? 32.0f : createInfo->max_lod;
    desc.lodMinClamp = createInfo->min_lod < 0 ? 0 : createInfo->min_lod;
    desc.magFilter = SDLToWebGPU_FilterMode[createInfo->mag_filter];
    desc.minFilter = SDLToWebGPU_FilterMode[createInfo->min_filter];
    desc.mipmapFilter = SDLToWebGPU_MipmapFilterMode[createInfo->mipmap_mode];
    // Again; gross float -> int cast, blame WebGPU.
    desc.maxAnisotropy = (createInfo->enable_anisotropy && (createInfo->max_anisotropy >= 1)) ? (Uint16)createInfo->max_anisotropy : 1;

    sampler->sampler = wgpuDeviceCreateSampler(((WebGPURenderer *)device)->device, &desc);
    sampler->identifier = ((WebGPURenderer *)device)->nextBindableResourceID++;

    return (SDL_GPUSampler *)sampler;
}

static void WEBGPU_INTERNAL_ReleaseSampler(WebGPURenderer *renderer, WebGPUSampler *sampler)
{
    for (int i = 0; i < sampler->numDependants; i++) {
        // WEBGPU_INTERNAL_InvalidateBindGroup(renderer, sampler->dependants[i]);
    }

    wgpuSamplerRelease(sampler->sampler);
    SDL_free(sampler->dependants);
    SDL_free(sampler);
}

static void WEBGPU_ReleaseSampler(SDL_GPURenderer *device, SDL_GPUSampler *sampler)
{
    WEBGPU_INTERNAL_QueueSamplerForRelease((WebGPURenderer *)device, (WebGPUSampler *)sampler);
}

// forward decl
static bool WEBGPU_SupportsSwapchainComposition(SDL_GPURenderer *driverData, SDL_Window *window, SDL_GPUSwapchainComposition swapchainComposition);
static SDL_GPUTextureFormat WEBGPU_GetSwapchainTextureFormat(SDL_GPURenderer *device, SDL_Window *window);

static bool WEBGPU_ClaimWindow(SDL_GPURenderer *device, SDL_Window *window)
{
    WebGPUWindowData *windowData;
    SDL_PropertiesID props = SDL_GetWindowProperties(window);

    windowData = (WebGPUWindowData *)SDL_calloc(1, sizeof(*windowData));
    SDL_SetPointerProperty(props, WINDOW_PROPERTY_DATA, windowData);

    windowData->window = window;
    windowData->presentMode = SDL_GPU_PRESENTMODE_VSYNC,
    windowData->swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    windowData->renderer = (WebGPURenderer *)device;
    windowData->surface = SDL_WGPU_CreateSurface(window, windowData->renderer->instance);
    windowData->surfaceDirty = false;
    int w = 0;
    int h = 0;

    if (!WEBGPU_SupportsSwapchainComposition(device, window, SDL_GPU_SWAPCHAINCOMPOSITION_SDR)) {
        SDL_assert_release(!"Surface does not support SDR!");
    }

    windowData->swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;

    SDL_GetWindowSizeInPixels(window, &w, &h);

    windowData->surfaceConfig = (WGPUSurfaceConfiguration){
        .device = windowData->renderer->device,
        .usage = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopyDst,
        .format = SDLToWebGPU_TextureFormat[WEBGPU_GetSwapchainTextureFormat(device, window)],
        .presentMode = WGPUPresentMode_Fifo,
        .alphaMode = WGPUCompositeAlphaMode_Auto,
        .width = w,
        .height = h
    };

    wgpuSurfaceConfigure(windowData->surface, &windowData->surfaceConfig);

    if (windowData->surface == NULL) {
        // TODO: I can't be bothered freeing everything
        return false;
    }

    return true;
}

static bool WEBGPU_AcquireSwapchainTexture(SDL_GPUCommandBuffer *commandBuffer, SDL_Window *window, SDL_GPUTexture **swapchainTexture, Uint32 *width, Uint32 *height)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    if (cmdBuf->renderer->submittedCommandBufferCount >= cmdBuf->renderer->maxFramesInFlight) {
        *swapchainTexture = NULL;
        return true;
    }

    WebGPUWindowData *windowData = SDL_GetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, NULL);

    WebGPUTexture *texture;
    WebGPUTexture **textureArray;
    WebGPUTextureContainer *container;

    if (windowData->surfaceDirty) {
        wgpuSurfaceConfigure(windowData->surface, &windowData->surfaceConfig);
        windowData->surfaceDirty = false;
    }

    WGPUSurfaceTexture surfaceTexture = { 0 };
    wgpuSurfaceGetCurrentTexture(windowData->surface, &surfaceTexture);

    if (surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal) {
        // Nothing to do.
    } else if (surfaceTexture.status == WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal) {
        windowData->surfaceDirty = true;
    } else {
        SDL_SetError("Could not acquire surface texture!");
        return false;
    }

    textureArray = SDL_calloc(1, sizeof(WebGPUTexture *));
    texture = SDL_calloc(1, sizeof(WebGPUTexture));
    container = (WebGPUTextureContainer *)SDL_calloc(1, sizeof(*container));

    textureArray[0] = texture;
    texture->container = container;

    texture->texture = surfaceTexture.texture;
    texture->aspect = WGPUTextureAspect_All;
    SDL_SetAtomicInt(&texture->referenceCount, 0);
    texture->format = SwapchainCompositionToSDLFormat(windowData->swapchainComposition, windowData->shouldUseFallbackFormat);
    texture->fullTextureView = WEBGPU_INTERNAL_CreateTextureView(cmdBuf->renderer, texture, -1, 0);

    WebGPUTextureView *textureView = WEBGPU_INTERNAL_CreateTextureView(cmdBuf->renderer, texture, 0, 0);
    WEBGPU_INTERNAL_InsertElementIntoArray(texture->textureViews, texture->textureViewCapacity,
                                           texture->textureViewCount, WebGPUTextureView *, textureView);

    container->textureCapacity = 1;
    container->textureCount = 1;
    container->textures = textureArray;
    container->activeTexture = texture;
    container->activeTextureIndex = 0;

    container->header.info.width = wgpuTextureGetWidth(texture->texture);
    container->header.info.height = wgpuTextureGetHeight(texture->texture);
    container->header.info.format = SwapchainCompositionToSDLFormat(windowData->swapchainComposition, windowData->shouldUseFallbackFormat);
    container->header.info.layer_count_or_depth = 1;
    container->header.info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    container->header.info.num_levels = 1;
    container->header.info.props = 0;

    EXPAND_ARRAY_IF_NEEDED(cmdBuf->surfaces, WGPUSurface, cmdBuf->surfaceCount + 1, cmdBuf->surfaceCapacity, cmdBuf->surfaceCapacity + 1);
    cmdBuf->surfaces[cmdBuf->surfaceCount++] = windowData->surface;

    EXPAND_ARRAY_IF_NEEDED(cmdBuf->acquiredSwapchainTextures, WebGPUTextureContainer *, cmdBuf->swapchainTextureCount + 1, cmdBuf->swapchainTextureCapacity, cmdBuf->swapchainTextureCapacity + 1);
    cmdBuf->acquiredSwapchainTextures[cmdBuf->swapchainTextureCount++] = container;

    *swapchainTexture = (SDL_GPUTexture *)container;
    if (width != NULL) {
        *width = container->header.info.width;
        windowData->surfaceConfig.width = container->header.info.width;
    }
    if (height != NULL) {
        *height = container->header.info.height;
        windowData->surfaceConfig.height = container->header.info.height;
    }

    return true;
}

static SDL_GPUShader *WEBGPU_CreateShader(
    SDL_GPURenderer *driverData,
    const SDL_GPUShaderCreateInfo *createinfo)
{
    WGPUShaderModuleDescriptor desc;

    WebGPUShader *shader;
    shader = SDL_calloc(1, sizeof(*shader));

    const char *debugName = SDL_GetStringProperty(createinfo->props, SDL_PROP_GPU_SHADER_CREATE_NAME_STRING, NULL);
    desc.label = debugName != NULL ? (WGPUStringView){ debugName, SDL_strlen(debugName) } : (WGPUStringView){ NULL, 0 };

    WGPUShaderSourceWGSL source;

    source.code = (WGPUStringView){ .data = (char *)createinfo->code, .length = createinfo->code_size };
    source.chain.next = NULL;
    source.chain.sType = WGPUSType_ShaderSourceWGSL;

    desc.nextInChain = &source.chain;
    shader->shader = wgpuDeviceCreateShaderModule(((WebGPURenderer *)driverData)->device, &desc);

    shader->entrypoint = SDL_strdup(createinfo->entrypoint);
    shader->bindGroupLayouts = WEBGPU_INTERNAL_GenerateBindGroupLayoutsForShader((char *)createinfo->code, ((WebGPURenderer *)driverData),
                                                                                 createinfo->stage == SDL_GPU_SHADERSTAGE_VERTEX ? WGPUShaderStage_Vertex : WGPUShaderStage_Fragment);

    return (SDL_GPUShader *)shader;
}

static void WEBGPU_INTERNAL_ReleaseShader(WebGPUShader *shader)
{
    if (shader->refCount != 0) {
        return;
    }

    wgpuShaderModuleRelease(shader->shader);

    SDL_free(shader->bindGroupLayouts);
    SDL_free(shader->entrypoint);
    SDL_free(shader);
}

static void WEBGPU_ReleaseShader(SDL_GPURenderer *driverData, SDL_GPUShader *shader)
{
    WEBGPU_INTERNAL_ReleaseShader((WebGPUShader *)shader);
}

static SDL_GPUGraphicsPipeline *WEBGPU_CreateGraphicsPipeline(SDL_GPURenderer *driverData, const SDL_GPUGraphicsPipelineCreateInfo *createInfo)
{
    WebGPUGraphicsPipeline *pipeline;
    WGPURenderPipelineDescriptor pipelineDesc;

    WebGPUShader *vertexShader = (WebGPUShader *)createInfo->vertex_shader;
    WebGPUShader *fragmentShader = (WebGPUShader *)createInfo->fragment_shader;

    WGPUDepthStencilState depthStencilState = {
        // yucky wucky
        .depthBias = (Uint32)createInfo->rasterizer_state.depth_bias_constant_factor,
        .depthBiasClamp = createInfo->rasterizer_state.depth_bias_clamp,
        .depthBiasSlopeScale = createInfo->rasterizer_state.depth_bias_slope_factor,
        .depthCompare = SDLToWebGPU_CompareFunc[createInfo->depth_stencil_state.compare_op],
        .depthWriteEnabled = createInfo->depth_stencil_state.enable_depth_write,
        .stencilBack = createInfo->depth_stencil_state.enable_stencil_test ? (WGPUStencilFaceState){
                                                                                 .compare = SDLToWebGPU_CompareFunc[createInfo->depth_stencil_state.back_stencil_state.compare_op],
                                                                                 .depthFailOp = SDLToWebGPU_StencilOp[createInfo->depth_stencil_state.back_stencil_state.depth_fail_op],
                                                                                 .failOp = SDLToWebGPU_StencilOp[createInfo->depth_stencil_state.back_stencil_state.fail_op],
                                                                                 .passOp = SDLToWebGPU_StencilOp[createInfo->depth_stencil_state.back_stencil_state.pass_op],
                                                                             }
                                                                           : (WGPUStencilFaceState){ 0 },
        .stencilFront = createInfo->depth_stencil_state.enable_stencil_test ? (WGPUStencilFaceState){
                                                                                  .compare = SDLToWebGPU_CompareFunc[createInfo->depth_stencil_state.front_stencil_state.compare_op],
                                                                                  .depthFailOp = SDLToWebGPU_StencilOp[createInfo->depth_stencil_state.front_stencil_state.depth_fail_op],
                                                                                  .failOp = SDLToWebGPU_StencilOp[createInfo->depth_stencil_state.front_stencil_state.fail_op],
                                                                                  .passOp = SDLToWebGPU_StencilOp[createInfo->depth_stencil_state.front_stencil_state.pass_op],
                                                                              }
                                                                            : (WGPUStencilFaceState){ 0 },
        .stencilReadMask = createInfo->depth_stencil_state.enable_stencil_test ? createInfo->depth_stencil_state.compare_mask : 0,
        .stencilWriteMask = createInfo->depth_stencil_state.enable_stencil_test ? createInfo->depth_stencil_state.write_mask : 0,
        .format = SDLToWebGPU_TextureFormat[createInfo->target_info.depth_stencil_format],
        .nextInChain = NULL,
    };

    WGPUColorTargetState *colorTargets = SDL_calloc(createInfo->target_info.num_color_targets, sizeof(WGPUColorTargetState));
    WGPUBlendState *colorTargetBlendStates = SDL_calloc(createInfo->target_info.num_color_targets, sizeof(WGPUBlendState));
    for (int i = 0; i < createInfo->target_info.num_color_targets; i++) {
        const SDL_GPUColorTargetDescription *targetDesc = &createInfo->target_info.color_target_descriptions[i];

        bool float32Filterable = wgpuDeviceHasFeature(((WebGPURenderer *)driverData)->device, WGPUFeatureName_Float32Filterable);
        if (WebGPUTextureFormatIsBlendable(SDLToWebGPU_TextureFormat[targetDesc->format], float32Filterable)) {
            colorTargetBlendStates[i].color = (WGPUBlendComponent){
                .dstFactor = SDLToWebGPU_BlendFactor[targetDesc->blend_state.dst_color_blendfactor],
                .srcFactor = SDLToWebGPU_BlendFactor[targetDesc->blend_state.src_color_blendfactor],
                .operation = SDLToWebGPU_BlendOp[targetDesc->blend_state.color_blend_op],
            };

            colorTargetBlendStates[i].alpha = (WGPUBlendComponent){
                .dstFactor = SDLToWebGPU_BlendFactor[targetDesc->blend_state.dst_alpha_blendfactor],
                .srcFactor = SDLToWebGPU_BlendFactor[targetDesc->blend_state.src_alpha_blendfactor],
                .operation = SDLToWebGPU_BlendOp[targetDesc->blend_state.alpha_blend_op],
            };

            colorTargets[i].blend = &colorTargetBlendStates[i];
        } else {
            colorTargets[i].blend = NULL;
        }
        colorTargets[i].format = SDLToWebGPU_TextureFormat[targetDesc->format];
        colorTargets[i].nextInChain = NULL;

        if (targetDesc->blend_state.enable_color_write_mask) {
            if (targetDesc->blend_state.color_write_mask & SDL_GPU_COLORCOMPONENT_A) {
                colorTargets[i].writeMask |= WGPUColorWriteMask_Alpha;
            }
            if (targetDesc->blend_state.color_write_mask & SDL_GPU_COLORCOMPONENT_R) {
                colorTargets[i].writeMask |= WGPUColorWriteMask_Red;
            }
            if (targetDesc->blend_state.color_write_mask & SDL_GPU_COLORCOMPONENT_G) {
                colorTargets[i].writeMask |= WGPUColorWriteMask_Green;
            }
            if (targetDesc->blend_state.color_write_mask & SDL_GPU_COLORCOMPONENT_B) {
                colorTargets[i].writeMask |= WGPUColorWriteMask_Blue;
            }
        } else {
            colorTargets[i].writeMask = WGPUColorWriteMask_All;
        }
    }

    WGPUFragmentState fragmentState = {
        .constantCount = 0,
        .constants = NULL,
        .targets = colorTargets,
        .targetCount = createInfo->target_info.num_color_targets,
        .entryPoint = { fragmentShader->entrypoint, SDL_strlen(fragmentShader->entrypoint) },
        .module = fragmentShader->shader,
    };

    WGPUVertexBufferLayout *vertexBufferLayouts = SDL_calloc(createInfo->vertex_input_state.num_vertex_buffers, sizeof(WGPUVertexBufferLayout));

    for (int i = 0; i < createInfo->vertex_input_state.num_vertex_buffers; i++) {
        vertexBufferLayouts[i].stepMode = SDLToWebGPU_VertexInputRate[createInfo->vertex_input_state.vertex_buffer_descriptions[i].input_rate];
        vertexBufferLayouts[i].arrayStride = createInfo->vertex_input_state.vertex_buffer_descriptions[i].pitch;
        vertexBufferLayouts[i].nextInChain = NULL;

        WGPUVertexAttribute *attributes = SDL_calloc(MAX_VERTEX_ATTRIBUTES, sizeof(WGPUVertexAttribute));
        Uint32 attributeCount = 0;
        Uint32 currentOffset = 0;

        for (int j = 0; j < createInfo->vertex_input_state.num_vertex_attributes; j++) {
            const SDL_GPUVertexAttribute *attr = &createInfo->vertex_input_state.vertex_attributes[j];

            if (createInfo->vertex_input_state.vertex_attributes[j].buffer_slot == i) {
                attributes[currentOffset].offset = attr->offset;
                attributes[currentOffset].format = SDLToWebGPU_VertexFormat[attr->format];
                attributes[currentOffset].nextInChain = NULL;
                attributes[currentOffset].shaderLocation = attr->location;
                attributeCount++;
                currentOffset++;
            }
        }

        vertexBufferLayouts[i].attributeCount = attributeCount;
        vertexBufferLayouts[i].attributes = attributes;
    }

    WGPUVertexState vertexState = {
        .bufferCount = createInfo->vertex_input_state.num_vertex_buffers,
        .buffers = vertexBufferLayouts,
        .constantCount = 0,
        .constants = NULL,
        .entryPoint = { vertexShader->entrypoint, SDL_strlen(vertexShader->entrypoint) },
        .module = vertexShader->shader,
        .nextInChain = NULL,
    };

    WGPUMultisampleState multisampleState = {
        .alphaToCoverageEnabled = createInfo->multisample_state.enable_alpha_to_coverage,
        .mask = 0xFFFFFFFF, // Unimplemented in SDL_GPU
        .nextInChain = NULL,
    };

    switch (createInfo->multisample_state.sample_count) {
    case SDL_GPU_SAMPLECOUNT_1:
        multisampleState.count = 1;
        break;
    case SDL_GPU_SAMPLECOUNT_4:
        multisampleState.count = 4;
        break;
    case SDL_GPU_SAMPLECOUNT_2:
    case SDL_GPU_SAMPLECOUNT_8:
        if (((WebGPURenderer *)driverData)->debugMode) {
            SDL_assert_release(!"WebGPU only supports 1x or 4x multisampling!");
        }
        break;
    }

    WGPUPrimitiveState primitiveState = {
        .cullMode = SDLToWebGPU_CullMode[createInfo->rasterizer_state.cull_mode],
        .frontFace = SDLToWebGPU_FrontFace[createInfo->rasterizer_state.front_face],
        .stripIndexFormat = WGPUIndexFormat_Undefined,
        .topology = SDLToWebGPU_PrimitiveType[createInfo->primitive_type],
        .unclippedDepth = !createInfo->rasterizer_state.enable_depth_clip,
        .nextInChain = NULL,
    };

    WGPUBindGroupLayout *bindGroupLayouts = SDL_calloc(4, sizeof(WGPUBindGroupLayout));
    bindGroupLayouts[0] = vertexShader->bindGroupLayouts->samplerStorageBindGroupLayout;
    bindGroupLayouts[1] = vertexShader->bindGroupLayouts->uniformBindGroupLayout;
    bindGroupLayouts[2] = fragmentShader->bindGroupLayouts->samplerStorageBindGroupLayout;
    bindGroupLayouts[3] = fragmentShader->bindGroupLayouts->uniformBindGroupLayout;

    WGPUPipelineLayoutDescriptor pipelineLayoutDesc = {
        .bindGroupLayoutCount = 4,
        .bindGroupLayouts = bindGroupLayouts,
        .immediateSize = 0,
        .label = WGPU_STRING_VIEW_INIT,
        .nextInChain = NULL,
    };

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(((WebGPURenderer *)driverData)->device, &pipelineLayoutDesc);
    const char *debugName = SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_GRAPHICSPIPELINE_CREATE_NAME_STRING, NULL);

    pipelineDesc.label = debugName != NULL ? (WGPUStringView){ debugName, SDL_strlen(debugName) } : (WGPUStringView){ NULL, 0 };
    pipelineDesc.vertex = vertexState;
    pipelineDesc.fragment = &fragmentState;
    pipelineDesc.multisample = multisampleState;
    pipelineDesc.depthStencil = createInfo->target_info.has_depth_stencil_target ? &depthStencilState : NULL;
    pipelineDesc.primitive = primitiveState;
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.nextInChain = NULL;

    pipeline = SDL_calloc(1, sizeof(WebGPUGraphicsPipeline));
    pipeline->pipeline = wgpuDeviceCreateRenderPipeline(((WebGPURenderer *)driverData)->device, &pipelineDesc);

    pipeline->vertexBindGroupLayouts = *vertexShader->bindGroupLayouts;
    pipeline->fragmentBindGroupLayouts = *fragmentShader->bindGroupLayouts;

    for (int i = 0; i < createInfo->vertex_input_state.num_vertex_buffers; i++) {
        SDL_free((void *)vertexBufferLayouts[i].attributes);
    }

    SDL_free(colorTargetBlendStates);

    SDL_free(vertexBufferLayouts);
    SDL_free(colorTargets);

    SDL_free(bindGroupLayouts);

    return (SDL_GPUGraphicsPipeline *)pipeline;
}

static void WEBGPU_ReleaseGraphicsPipeline(SDL_GPURenderer *device, SDL_GPUGraphicsPipeline *_pipeline)
{
    WebGPUGraphicsPipeline *pipeline = (WebGPUGraphicsPipeline *)_pipeline;

    wgpuRenderPipelineRelease(pipeline->pipeline);
    SDL_free(pipeline);
}

static void WEBGPU_ReleaseFence(SDL_GPURenderer *device, SDL_GPUFence *fence)
{
    WEBGPU_INTERNAL_QueueFenceForRelease((WebGPURenderer *)device, (WebGPUFence *)fence);
}

// Should out to klukaszek for writing the blit code because wow I hate blitting
static void WEBGPU_INTERNAL_InitBlitResources(
    WebGPURenderer *renderer)
{
    SDL_GPUShaderCreateInfo shaderCreateInfo;

    renderer->blitPipelineCapacity = 2;
    renderer->blitPipelineCount = 0;
    renderer->blitPipelines = (BlitPipelineCacheEntry *)SDL_calloc(
        renderer->blitPipelineCapacity, sizeof(BlitPipelineCacheEntry));

    // Fullscreen vertex shader
    SDL_zero(shaderCreateInfo);
    shaderCreateInfo.code = (Uint8 *)blitVert;
    shaderCreateInfo.code_size = SDL_strlen(blitVert);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    shaderCreateInfo.format = SDL_GPU_SHADERFORMAT_WGSL;
    shaderCreateInfo.entrypoint = "main";

    renderer->blitResources.blitVertexShader = WEBGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitResources.blitVertexShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile vertex shader for blit!");
    }

    shaderCreateInfo.code = (Uint8 *)blit2DShader;
    shaderCreateInfo.code_size = SDL_strlen(blit2DShader);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderCreateInfo.num_samplers = 1;
    shaderCreateInfo.num_uniform_buffers = 1;
    renderer->blitResources.blit2DShader = WEBGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitResources.blit2DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile Blit2D pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blit2DArrayShader;
    shaderCreateInfo.code_size = SDL_strlen(blit2DArrayShader);
    shaderCreateInfo.entrypoint = "main";
    renderer->blitResources.blit2DArrayShader = WEBGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitResources.blit2DArrayShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile Blit2DArray pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blit3DShader;
    shaderCreateInfo.code_size = SDL_strlen(blit3DShader);
    renderer->blitResources.blit3DShader = WEBGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitResources.blit3DShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile Blit3D pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blitCubeShader;
    shaderCreateInfo.code_size = SDL_strlen(blitCubeShader);
    renderer->blitResources.blitCubeShader = WEBGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitResources.blitCubeShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitCube pixel shader!");
    }

    shaderCreateInfo.code = (Uint8 *)blitCubeArrayShader;
    shaderCreateInfo.code_size = SDL_strlen(blitCubeArrayShader);
    renderer->blitResources.blitCubeArrayShader = WEBGPU_CreateShader(
        (SDL_GPURenderer *)renderer,
        &shaderCreateInfo);
    if (renderer->blitResources.blitCubeArrayShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to compile BlitCubeArray pixel shader!");
    }

    SDL_GPUSamplerCreateInfo nearestCreateInfo = (SDL_GPUSamplerCreateInfo){
        .min_filter = SDL_GPU_FILTER_NEAREST,
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };

    renderer->blitResources.blitNearestSampler = WEBGPU_CreateSampler(
        (SDL_GPURenderer *)renderer,
        &nearestCreateInfo);

    if (renderer->blitResources.blitNearestSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create blit nearest sampler!");
    }

    SDL_GPUSamplerCreateInfo linearCreateInfo = (SDL_GPUSamplerCreateInfo){
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };

    renderer->blitResources.blitLinearSampler = WEBGPU_CreateSampler(
        (SDL_GPURenderer *)renderer,
        &linearCreateInfo);

    if (renderer->blitResources.blitLinearSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to create blit linear sampler!");
    }
}

static void WEBGPU_INTERNAL_ReleaseBlitResources(WebGPURenderer *renderer)
{
    WEBGPU_INTERNAL_ReleaseShader((WebGPUShader *)renderer->blitResources.blit2DArrayShader);
    WEBGPU_INTERNAL_ReleaseShader((WebGPUShader *)renderer->blitResources.blit2DShader);
    WEBGPU_INTERNAL_ReleaseShader((WebGPUShader *)renderer->blitResources.blit3DShader);
    WEBGPU_INTERNAL_ReleaseShader((WebGPUShader *)renderer->blitResources.blitCubeShader);
    WEBGPU_INTERNAL_ReleaseShader((WebGPUShader *)renderer->blitResources.blitCubeArrayShader);
    WEBGPU_INTERNAL_ReleaseShader((WebGPUShader *)renderer->blitResources.blitVertexShader);
    WEBGPU_INTERNAL_ReleaseSampler(renderer, (WebGPUSampler *)renderer->blitResources.blitLinearSampler);
    WEBGPU_INTERNAL_ReleaseSampler(renderer, (WebGPUSampler *)renderer->blitResources.blitNearestSampler);

    for (int i = 0; i < renderer->blitPipelineCount; i++) {
        WEBGPU_ReleaseGraphicsPipeline((SDL_GPURenderer *)renderer, renderer->blitPipelines[i].pipeline);
    }
}

static WebGPUBuffer *WEBGPU_INTERNAL_CreateBuffer(WebGPURenderer *renderer, Uint64 size, SDL_GPUBufferUsageFlags usageFlags, WebGPUBufferType bufferType, const char *debugName)
{
    WGPUBufferUsage usages = 0;

    if (bufferType == WEBGPU_BUFFER_TYPE_GPU) {
        if (usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX) {
            usages |= WGPUBufferUsage_Vertex;
            usages |= WGPUBufferUsage_CopyDst;
            usages |= WGPUBufferUsage_CopySrc;
        }

        if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX) {
            usages |= WGPUBufferUsage_Index;
            usages |= WGPUBufferUsage_CopyDst;
            usages |= WGPUBufferUsage_CopySrc;
        }

        if (usageFlags & (SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ |
                          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ |
                          SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE)) {
            usages |= WGPUBufferUsage_Storage;
            usages |= WGPUBufferUsage_CopyDst;
            usages |= WGPUBufferUsage_CopySrc;
        }

        if (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT) {
            usages |= WGPUBufferUsage_Indirect;
            usages |= WGPUBufferUsage_CopyDst;
            usages |= WGPUBufferUsage_CopySrc;
        }
    } else if (bufferType == WEBGPU_BUFFER_TYPE_TRANSFER_UPLOAD) {
        usages |= WGPUBufferUsage_MapWrite;
        usages |= WGPUBufferUsage_CopySrc;
    } else if (bufferType == WEBGPU_BUFFER_TYPE_TRANSFER_DOWNLOAD) {
        usages |= WGPUBufferUsage_MapRead;
        usages |= WGPUBufferUsage_CopyDst;
    } else if (bufferType == WEBGPU_BUFFER_TYPE_TRANSFER_GPUONLY) {
        usages |= WGPUBufferUsage_CopyDst;
        usages |= WGPUBufferUsage_CopySrc;
    } else if (bufferType == WEBGPU_BUFFER_TYPE_UNIFORM) {
        usages |= WGPUBufferUsage_Uniform;
        usages |= WGPUBufferUsage_CopyDst;
        usages |= WGPUBufferUsage_CopySrc;
    }

    WebGPUBuffer *buf = SDL_calloc(1, sizeof(*buf));

    buf->size = ALIGN_VALUE(size, 4);
    buf->usage = usages;
    buf->type = bufferType;
    buf->identifier = renderer->nextBindableResourceID++;

    WGPUBufferDescriptor desc = {
        .label = debugName != NULL ? (WGPUStringView){ debugName, SDL_strlen(debugName) } : (WGPUStringView){ NULL, 0 },
        .size = ALIGN_VALUE(size, 4),
        .usage = usages,
        .mappedAtCreation = false,
        .nextInChain = NULL,
    };

    buf->buffer = wgpuDeviceCreateBuffer(renderer->device, &desc);
    SDL_SetAtomicInt(&buf->referenceCount, 0);

    return buf;
}

static WebGPUBufferContainer *WEBGPU_INTERNAL_CreateBufferContainer(
    WebGPURenderer *renderer,
    Uint64 size,
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

    bufferContainer->bufferCapacity = 1;
    bufferContainer->bufferCount = 1;
    bufferContainer->buffers = SDL_calloc(bufferContainer->bufferCapacity, sizeof(WebGPUBuffer *));
    bufferContainer->buffers[0] = bufferContainer->activeBuffer;
    bufferContainer->dedicated = dedicated;
    bufferContainer->debugName = NULL;
    bufferContainer->bufferType = bufferType;
    bufferContainer->usageFlags = usageFlags;
    bufferContainer->size = size;

    if (bufferType == WEBGPU_BUFFER_TYPE_TRANSFER_UPLOAD) {
        bufferContainer->pseudoMappedRange = SDL_malloc(size);
        SDL_memset(bufferContainer->pseudoMappedRange, 0, size);
    } else {
        // This trick only works for upload transfer buffers.
        bufferContainer->pseudoMappedRange = NULL;
    }

    if (debugName != NULL) {
        bufferContainer->debugName = SDL_strdup(debugName);
    }

    return bufferContainer;
}

static void WEBGPU_INTERNAL_ReleaseBuffer(WebGPURenderer *renderer, WebGPUBuffer *buffer)
{
    for (int i = 0; i < buffer->numDependants; i++) {
        // WEBGPU_INTERNAL_InvalidateBindGroup(renderer, buffer->dependants[i]);
    }

    wgpuBufferRelease(buffer->buffer);
    SDL_free(buffer->dependants);
    SDL_free(buffer);
    buffer = NULL;
}

static void WEBGPU_INTERNAL_ReleaseTextureView(WebGPURenderer *renderer, WebGPUTextureView *view)
{
    for (int j = 0; j < view->numDependants; j++) {
        // WEBGPU_INTERNAL_InvalidateBindGroup(renderer, view->dependants[j]);
    }

    wgpuTextureViewRelease(view->view);
    SDL_free(view->dependants);
    SDL_free(view);
    view = NULL;
}

static void WEBGPU_INTERNAL_ReleaseTexture(WebGPURenderer *renderer, WebGPUTexture *texture)
{
    for (int i = 0; i < texture->textureViewCount; i++) {
        WEBGPU_INTERNAL_ReleaseTextureView(renderer, texture->textureViews[i]);
    }

    WEBGPU_INTERNAL_ReleaseTextureView(renderer, texture->fullTextureView);

    wgpuTextureRelease(texture->texture);
    SDL_free(texture->textureViews);
    SDL_free(texture);
    texture = NULL;
}

static void WEBGPU_INTERNAL_ReleaseTextureContainer(WebGPURenderer *renderer, WebGPUTextureContainer *container)
{
    for (int i = 0; i < container->textureCount; i++) {
        WEBGPU_INTERNAL_ReleaseTexture(renderer, container->textures[i]);
    }
    SDL_free(container->textures);
    SDL_free(container);
    container = NULL;
}

static void WEBGPU_INTERNAL_ReleaseBufferContainer(WebGPURenderer *renderer, WebGPUBufferContainer *container)
{
    if (container == NULL) {
        return;
    }

    for (int i = 0; i < container->bufferCount; i++) {
        WEBGPU_INTERNAL_ReleaseBuffer(renderer, container->buffers[i]);
    }

    if (container->debugName != NULL) {
        SDL_free(container->debugName);
        container->debugName = NULL;
    }

    SDL_free(container->pseudoMappedRange);
    SDL_free(container->buffers);
    SDL_free(container);
    container = NULL;
}

static void WEBGPU_INTERNAL_CycleBufferContainer(WebGPURenderer *renderer, WebGPUBufferContainer *container)
{
    if (SDL_GetAtomicInt(&container->activeBuffer->referenceCount) == 0) {
        // The active buffer isn't being used by any command buffers, no need to cycle anything.
        goto finish;
    }

    if (container->bufferCount > 1) {
        for (int i = 0; i < container->bufferCount; i++) {
            WebGPUBuffer *current = container->buffers[i];
            SDL_assert_release(current != NULL);

            if (SDL_GetAtomicInt(&current->referenceCount) != 0) {
                continue;
            } else {
                container->activeBuffer = current;
                goto finish;
            }
        }
    }

    WebGPUBuffer *newBuf =
        WEBGPU_INTERNAL_CreateBuffer(renderer, container->size, container->usageFlags, container->bufferType, NULL);

    WEBGPU_INTERNAL_InsertElementIntoArray(container->buffers, container->bufferCapacity, container->bufferCount, WebGPUBuffer *, newBuf);
    container->activeBuffer = newBuf;

finish:
    return;
}

static void WEBGPU_INTERNAL_CycleTextureContainer(WebGPURenderer *renderer, WebGPUTextureContainer *container)
{
    if (SDL_GetAtomicInt(&container->activeTexture->referenceCount) == 0) {
        goto finish;
    }

    if (container->textureCount > 1) {
        for (int i = 0; i < container->textureCount; i++) {
            WebGPUTexture *current = container->textures[i];
            SDL_assert_release(current != NULL);

            if (SDL_GetAtomicInt(&current->referenceCount) != 0) {
                continue;
            } else {
                container->activeTexture = current;
                goto finish;
            }
        }
    }

    WebGPUTexture *newTex = WEBGPU_INTERNAL_CreateTexture(renderer, &container->header.info);
    WEBGPU_INTERNAL_InsertElementIntoArray(container->textures, container->textureCapacity, container->textureCount, WebGPUTexture *, newTex);
    container->activeTexture = newTex;

finish:
    return;
}
static void WEBGPU_Blit(SDL_GPUCommandBuffer *commandBuffer, const SDL_GPUBlitInfo *info);
static void WEBGPU_CopyTextureToTexture(SDL_GPUCommandBuffer *copyPass, const SDL_GPUTextureLocation *source, const SDL_GPUTextureLocation *destination, Uint32 w, Uint32 h, Uint32 d, bool cycle);
static void WEBGPU_GenerateMipmaps(SDL_GPUCommandBuffer *commandBuffer, SDL_GPUTexture *_texture)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUTexture *texture = ((WebGPUTextureContainer *)_texture)->activeTexture;

    if (texture->type != SDL_GPU_TEXTURETYPE_2D) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "GenerateMipmaps currently only supports non-array 2D textures!");
        return;
    }

    SDL_GPUTexture *renderTexture = WEBGPU_CreateTexture((SDL_GPURenderer *)cmdBuf->renderer, &(SDL_GPUTextureCreateInfo){
                                                                                                  .format = texture->format,
                                                                                                  .height = wgpuTextureGetHeight(texture->texture),
                                                                                                  .width = wgpuTextureGetWidth(texture->texture),
                                                                                                  .layer_count_or_depth = wgpuTextureGetDepthOrArrayLayers(texture->texture),
                                                                                                  .type = texture->type,
                                                                                                  .num_levels = wgpuTextureGetMipLevelCount(texture->texture),
                                                                                                  .usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
                                                                                              });
    Uint32 currentSizeX = wgpuTextureGetWidth(texture->texture);
    Uint32 currentSizeY = wgpuTextureGetHeight(texture->texture);
    for (int i = 0; i < wgpuTextureGetDepthOrArrayLayers(texture->texture); i++) {
        for (int j = 1; j < wgpuTextureGetMipLevelCount(texture->texture); j++) {
            SDL_GPUBlitRegion source = {
                .texture = _texture,
                .layer_or_depth_plane = i,
                .mip_level = j - 1,
                .w = currentSizeX,
                .h = currentSizeY,
            };

            currentSizeX /= 2;
            currentSizeY /= 2;

            SDL_GPUBlitRegion destination = {
                .texture = renderTexture,
                .layer_or_depth_plane = i,
                .mip_level = j,
                .w = currentSizeX,
                .h = currentSizeY,
            };

            WEBGPU_Blit(commandBuffer, &(SDL_GPUBlitInfo){
                                           .filter = SDL_GPU_FILTER_LINEAR,
                                           .load_op = SDL_GPU_LOADOP_DONT_CARE,
                                           .source = source,
                                           .destination = destination,
                                       });

            WEBGPU_BeginCopyPass(commandBuffer);
            WEBGPU_CopyTextureToTexture(
                commandBuffer,
                &(SDL_GPUTextureLocation){
                    .texture = renderTexture,
                    .layer = i,
                    .mip_level = j,
                },
                &(SDL_GPUTextureLocation){
                    .texture = _texture,
                    .layer = i,
                    .mip_level = j,
                },
                currentSizeX, currentSizeY, 1, false);
            WEBGPU_EndCopyPass(commandBuffer);
        }
    }

    WEBGPU_INTERNAL_ReleaseTextureContainer(cmdBuf->renderer, (WebGPUTextureContainer *)renderTexture);
}

static SDL_GPUBuffer *WEBGPU_CreateBuffer(
    SDL_GPURenderer *driverData,
    SDL_GPUBufferUsageFlags usageFlags,
    Uint32 size,
    const char *debugName)
{
    // #15981
    if ((usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX) && (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX) && ((WebGPURenderer *)driverData)->debugMode) {
        SDL_assert_release(!"Buffer cannot be created with both VERTEX and INDEX flags!");
        return NULL;
    }

    return (SDL_GPUBuffer *)WEBGPU_INTERNAL_CreateBufferContainer(
        (WebGPURenderer *)driverData,
        size,
        usageFlags,
        WEBGPU_BUFFER_TYPE_GPU,
        false,
        debugName);
}

static SDL_GPUTransferBuffer *WEBGPU_CreateTransferBuffer(SDL_GPURenderer *device, SDL_GPUTransferBufferUsage usage, Uint32 size, const char *debugName)
{
    return (SDL_GPUTransferBuffer *)WEBGPU_INTERNAL_CreateBufferContainer(
        (WebGPURenderer *)device,
        size,
        usage,
        usage == SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD ? WEBGPU_BUFFER_TYPE_TRANSFER_UPLOAD : WEBGPU_BUFFER_TYPE_TRANSFER_DOWNLOAD,
        false,
        debugName);
}

static void WEBGPU_ReleaseBuffer(SDL_GPURenderer *device, SDL_GPUBuffer *buffer)
{
    if (device != NULL && buffer != NULL) {
        WEBGPU_INTERNAL_QueueBufferContainerForRelease((WebGPURenderer *)device, (WebGPUBufferContainer *)buffer);
    }
}

static void WEBGPU_ReleaseTransferBuffer(SDL_GPURenderer *device, SDL_GPUTransferBuffer *buffer)
{
    if (device != NULL && buffer != NULL) {
        WEBGPU_INTERNAL_QueueBufferContainerForRelease((WebGPURenderer *)device, (WebGPUBufferContainer *)buffer);
    }
}

static void WEBGPU_INTERNAL_MapBufferCallback(WGPUMapAsyncStatus status, WGPUStringView message, void *unused0, void *unused1)
{
    // If the callback's NULL it'll segfault since the PC jumps to 0x0 so we need to have something here.
}

static bool WEBGPU_INTERNAL_MapBuffer(WebGPURenderer *renderer, WebGPUBufferContainer *buffer)
{
    WGPUMapMode mapMode = WGPUMapMode_None;

    if (buffer->activeBuffer->type == WEBGPU_BUFFER_TYPE_TRANSFER_DOWNLOAD) {
        mapMode = WGPUMapMode_Read;
    } else if (buffer->activeBuffer->type == WEBGPU_BUFFER_TYPE_TRANSFER_UPLOAD) {
        mapMode = WGPUMapMode_Write;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Attempting to map non-transfer buffer!");
        return false;
    }

    WebGPUFence *bufferMapFence =
        WEBGPU_INTERNAL_CreateFenceFromFuture(wgpuBufferMapAsync(buffer->activeBuffer->buffer,
                                                                 mapMode, 0,
                                                                 buffer->activeBuffer->size,
                                                                 (WGPUBufferMapCallbackInfo){
                                                                     .callback = WEBGPU_INTERNAL_MapBufferCallback,
                                                                     .mode = WGPUCallbackMode_WaitAnyOnly,
                                                                     .nextInChain = NULL,
                                                                     .userdata1 = NULL,
                                                                     .userdata2 = NULL,
                                                                 }));

    WEBGPU_WaitForFences((SDL_GPURenderer *)renderer, true, (SDL_GPUFence **)&bufferMapFence, 1);

    SDL_free(bufferMapFence);
    return true;
}

static void *WEBGPU_INTERNAL_MapBufferRange(WebGPURenderer *renderer, WebGPUBufferContainer *buffer, size_t offset, size_t size)
{
    // If "size" is -1, then bind the entire buffer.
    size_t bindSize = (size == -1) ? buffer->activeBuffer->size : size;

    if (!WEBGPU_INTERNAL_MapBuffer(renderer, buffer)) {
        SDL_SetError("Failed to map buffer!");
        return NULL;
    }

    if (buffer->activeBuffer->type == WEBGPU_BUFFER_TYPE_TRANSFER_DOWNLOAD) {
        return (void *)wgpuBufferGetConstMappedRange(buffer->activeBuffer->buffer, offset, bindSize);
    } else {
        return wgpuBufferGetMappedRange(buffer->activeBuffer->buffer, offset, bindSize);
    }
}

static void *WEBGPU_MapTransferBuffer(SDL_GPURenderer *device, SDL_GPUTransferBuffer *transferBuffer, bool cycle)
{
    bool isMainThread = SDL_GetCurrentThreadID() == ((WebGPURenderer *)device)->createdByThreadID;

    if (cycle) {
        WEBGPU_INTERNAL_CycleBufferContainer((WebGPURenderer *)device, (WebGPUBufferContainer *)transferBuffer);
    }

    if (((WebGPUBufferContainer *)transferBuffer)->pseudoMappedRange != NULL && isMainThread && !DEV_DISABLE_TRANSFER_BUFFER_PSEUDO_MAPPING) {
        // Time to do our magic tricks.
        if (cycle) {
            SDL_memset(((WebGPUBufferContainer *)transferBuffer)->pseudoMappedRange, 0, ((WebGPUBufferContainer *)transferBuffer)->size);
        }

        // Mapped on CPU.
        ((WebGPUBufferContainer *)transferBuffer)->mapState = MAP_STATE_MAPPED_CPU;

        return ((WebGPUBufferContainer *)transferBuffer)->pseudoMappedRange;
    } else {
        // Mapped on GPU.
        ((WebGPUBufferContainer *)transferBuffer)->mapState = MAP_STATE_MAPPED_GPU;

        return WEBGPU_INTERNAL_MapBufferRange((WebGPURenderer *)device, (WebGPUBufferContainer *)transferBuffer, 0, -1);
    }
}

static void WEBGPU_UnmapTransferBuffer(SDL_GPURenderer *device, SDL_GPUTransferBuffer *transferBuffer)
{
    if (((WebGPUBufferContainer *)transferBuffer)->mapState == MAP_STATE_MAPPED_CPU) {
        // Yeah we don't actually have to do anything.
    } else {
        wgpuBufferUnmap(((WebGPUBufferContainer *)transferBuffer)->activeBuffer->buffer);
    }
}

static void WEBGPU_INTERNAL_CopyBufferToBuffer(WGPUCommandEncoder encoder, WebGPUBufferContainer *sourceBuf,
                                               Uint32 sourceBufOffset, WebGPUBufferContainer *destBuf,
                                               Uint32 destBufOffset, uint32_t size)
{
    wgpuCommandEncoderCopyBufferToBuffer(encoder, sourceBuf->activeBuffer->buffer, sourceBufOffset, destBuf->activeBuffer->buffer, destBufOffset, size);
}

static void WEBGPU_CopyBufferToBuffer(SDL_GPUCommandBuffer *copyPass, const SDL_GPUBufferLocation *source, const SDL_GPUBufferLocation *destination, Uint32 size, bool cycle)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)copyPass;

    if (cycle) {
        WEBGPU_INTERNAL_CycleBufferContainer(((WebGPUCommandBuffer *)copyPass)->renderer, (WebGPUBufferContainer *)destination->buffer);
    }

    SDL_AtomicIncRef(&((WebGPUBufferContainer *)source->buffer)->activeBuffer->referenceCount);
    SDL_AtomicIncRef(&((WebGPUBufferContainer *)destination->buffer)->activeBuffer->referenceCount);

    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount, WebGPUBuffer *, ((WebGPUBufferContainer *)source->buffer)->activeBuffer);
    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount, WebGPUBuffer *, ((WebGPUBufferContainer *)destination->buffer)->activeBuffer);

    WEBGPU_INTERNAL_CopyBufferToBuffer(((WebGPUCommandBuffer *)copyPass)->encoder, (WebGPUBufferContainer *)source->buffer, source->offset, (WebGPUBufferContainer *)destination->buffer, destination->offset, size);
}

static void WEBGPU_CopyTextureToTexture(SDL_GPUCommandBuffer *copyPass, const SDL_GPUTextureLocation *source, const SDL_GPUTextureLocation *destination, Uint32 w, Uint32 h, Uint32 d, bool cycle)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)copyPass;
    WGPUTexelCopyTextureInfo sourceInfo, destInfo;

    if (cycle) {
        WEBGPU_INTERNAL_CycleTextureContainer(cmdBuf->renderer, (WebGPUTextureContainer *)destination->texture);
    }

    // ASTC support. We only care about the destination texture being aligned though.
    Uint32 blockWidthDest = Texture_GetBlockWidth(((WebGPUTextureContainer *)destination->texture)->activeTexture->format);
    Uint32 blockHeightDest = Texture_GetBlockHeight(((WebGPUTextureContainer *)destination->texture)->activeTexture->format);

    sourceInfo.texture = ((WebGPUTextureContainer *)source->texture)->activeTexture->texture;
    sourceInfo.aspect = ((WebGPUTextureContainer *)source->texture)->activeTexture->aspect;
    sourceInfo.origin = (WGPUOrigin3D){ .x = source->x, .y = source->y, .z = source->z + source->layer };
    sourceInfo.mipLevel = source->mip_level;

    destInfo.texture = ((WebGPUTextureContainer *)destination->texture)->activeTexture->texture;
    destInfo.aspect = ((WebGPUTextureContainer *)destination->texture)->activeTexture->aspect;
    destInfo.origin = (WGPUOrigin3D){ .x = destination->x, .y = destination->y, .z = destination->z + destination->layer };
    destInfo.mipLevel = destination->mip_level;

    SDL_AtomicIncRef(&((WebGPUTextureContainer *)source->texture)->activeTexture->referenceCount);
    SDL_AtomicIncRef(&((WebGPUTextureContainer *)destination->texture)->activeTexture->referenceCount);

    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity,
                                           cmdBuf->submitted.usedTextureCount, WebGPUTexture *, ((WebGPUTextureContainer *)source->texture)->activeTexture);
    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity,
                                           cmdBuf->submitted.usedTextureCount, WebGPUTexture *, ((WebGPUTextureContainer *)destination->texture)->activeTexture);

    wgpuCommandEncoderCopyTextureToTexture(cmdBuf->encoder, &sourceInfo, &destInfo, &(WGPUExtent3D){ ALIGN_VALUE(w, blockWidthDest), ALIGN_VALUE(h, blockHeightDest), d });
}

// This was originally a full-blown public function but I decided against it, so that's why it has the opaque types as arguments.
static void WEBGPU_INTERNAL_UploadToBufferFromCPU(SDL_GPUCommandBuffer *copy_pass, const SDL_GPUBufferRegion *region, const void *data, bool cycle)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)copy_pass;

    if (cycle) {
        WEBGPU_INTERNAL_CycleBufferContainer(cmdBuf->renderer, (WebGPUBufferContainer *)region->buffer);
    }

    // FIXME: This function is not thread-safe!
    wgpuQueueWriteBuffer(cmdBuf->queue, ((WebGPUBufferContainer *)region->buffer)->activeBuffer->buffer,
                         region->offset, data, ALIGN_VALUE(region->size, 4));
}

static void WEBGPU_UploadToBuffer(SDL_GPUCommandBuffer *copyPass, const SDL_GPUTransferBufferLocation *source, const SDL_GPUBufferRegion *destination, bool cycle)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)copyPass;

    if (cycle) {
        WEBGPU_INTERNAL_CycleBufferContainer(((WebGPUCommandBuffer *)copyPass)->renderer, (WebGPUBufferContainer *)destination->buffer);
    }

    SDL_AtomicIncRef(&((WebGPUBufferContainer *)source->transfer_buffer)->activeBuffer->referenceCount);
    SDL_AtomicIncRef(&((WebGPUBufferContainer *)destination->buffer)->activeBuffer->referenceCount);

    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount, WebGPUBuffer *, ((WebGPUBufferContainer *)source->transfer_buffer)->activeBuffer);
    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount, WebGPUBuffer *, ((WebGPUBufferContainer *)destination->buffer)->activeBuffer);

    // Spaghetti code here. Opaque pointers are terrifying.
    if (((WebGPUBufferContainer *)source->transfer_buffer)->mapState == MAP_STATE_MAPPED_CPU) {
        WEBGPU_INTERNAL_UploadToBufferFromCPU(copyPass, destination, ((WebGPUBufferContainer *)source->transfer_buffer)->pseudoMappedRange + source->offset, false);
    } else if (((WebGPUBufferContainer *)source->transfer_buffer)->mapState == MAP_STATE_MAPPED_GPU) {
        WEBGPU_INTERNAL_CopyBufferToBuffer(((WebGPUCommandBuffer *)copyPass)->encoder, (WebGPUBufferContainer *)source->transfer_buffer, source->offset,
                                           (WebGPUBufferContainer *)destination->buffer, destination->offset, ALIGN_VALUE(destination->size, 4));
    }
}

static void WEBGPU_UploadToTexture(SDL_GPUCommandBuffer *copyPass, const SDL_GPUTextureTransferInfo *source, const SDL_GPUTextureRegion *destination, bool cycle)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)copyPass;

    if (cycle) {
        WEBGPU_INTERNAL_CycleTextureContainer(cmdBuf->renderer, (WebGPUTextureContainer *)destination->texture);
    }

    // NOTE:
    // WebGPU requires ASTC textures to be sized as a multiple of the block size,
    // i.e; An ASTC 4x5 texture's width must be a multiple of 4 and its height must be a multiple of 5.
    // The backend automatically scales up ASTC textures if needed, which makes it invisible to the end-user that WebGPU has this restriction.
    // However, we also have to pad the destination size in here since, again; it's invisible to the end user.
    // We should probably make a note in the docs stating that ASTC textures in the WebGPU backend are not always the same size as the user expects.
    Uint32 blockWidth = Texture_GetBlockWidth(((WebGPUTextureContainer *)destination->texture)->activeTexture->format);
    Uint32 blockHeight = Texture_GetBlockHeight(((WebGPUTextureContainer *)destination->texture)->activeTexture->format);

    Uint32 paddedWidth = ALIGN_VALUE(destination->w, blockWidth);
    Uint32 paddedHeight = ALIGN_VALUE(destination->h, blockHeight);

    Uint32 pixelsPerRowSource = source->pixels_per_row != 0 ? source->pixels_per_row : destination->w;
    Uint32 pixelsPerRowDest = source->pixels_per_row != 0 ? source->pixels_per_row : paddedWidth;

    size_t bytesPerRowSource = BytesPerRow(pixelsPerRowSource, ((WebGPUTextureContainer *)destination->texture)->activeTexture->format);
    size_t bytesPerRowDest = BytesPerRow(pixelsPerRowDest, ((WebGPUTextureContainer *)destination->texture)->activeTexture->format);

    size_t paddedBytesPerRow = ALIGN_VALUE(bytesPerRowDest, 256);
    size_t blocksPerLayer = (destination->h + blockHeight - 1) / blockHeight;

    WebGPUBuffer *userSourceBuffer = ((WebGPUBufferContainer *)source->transfer_buffer)->activeBuffer;
    WebGPUBuffer *finalSourceBuffer = userSourceBuffer;

    bool hadToPad = false;

    if (((WebGPUBufferContainer *)source->transfer_buffer)->mapState == MAP_STATE_MAPPED_CPU) {
        // No need to pad anything, writeTexture does that for us.
        wgpuQueueWriteTexture(cmdBuf->queue,
                              &(WGPUTexelCopyTextureInfo){
                                  .texture = ((WebGPUTextureContainer *)destination->texture)->activeTexture->texture,
                                  .aspect = WGPUTextureAspect_All,
                                  .mipLevel = destination->mip_level,
                                  .origin = (WGPUOrigin3D){ destination->x, destination->y, destination->z + destination->layer },
                              },
                              ((WebGPUBufferContainer *)source->transfer_buffer)->pseudoMappedRange,
                              ((WebGPUBufferContainer *)source->transfer_buffer)->size,
                              &(WGPUTexelCopyBufferLayout){
                                  .bytesPerRow = bytesPerRowDest,
                                  .offset = source->offset,
                                  .rowsPerImage = blocksPerLayer,
                              },
                              &(WGPUExtent3D){ paddedWidth, paddedHeight, destination->d });
    } else {
        if (paddedBytesPerRow != bytesPerRowDest) {
            // FIXME: Why are we creating a whole new buffer just for a single upload? We need to create a buffer pool.
            WebGPUBuffer *babysittingSourceBuffer = WEBGPU_INTERNAL_CreateBuffer(cmdBuf->renderer, paddedBytesPerRow * blocksPerLayer, 0,
                                                                                 WEBGPU_BUFFER_TYPE_TRANSFER_GPUONLY, "Autopadded Texture Transfer Buffer");

            for (int i = 0; i < blocksPerLayer; i++) {
                wgpuCommandEncoderCopyBufferToBuffer(cmdBuf->encoder, userSourceBuffer->buffer, source->offset + i * bytesPerRowSource,
                                                     babysittingSourceBuffer->buffer, i * paddedBytesPerRow, ALIGN_VALUE(bytesPerRowSource, 4));
            }

            finalSourceBuffer = babysittingSourceBuffer;
            hadToPad = true;
        }

        WGPUTexelCopyBufferInfo sourceInfo = {
            .buffer = finalSourceBuffer->buffer,
            .layout = (WGPUTexelCopyBufferLayout){
                .bytesPerRow = paddedBytesPerRow,
                .rowsPerImage = blocksPerLayer,
                .offset = hadToPad ? 0 : source->offset,
            },
        };

        WGPUTexelCopyTextureInfo destInfo = {
            .aspect = WGPUTextureAspect_All,
            .texture = ((WebGPUTextureContainer *)destination->texture)->activeTexture->texture,
            .mipLevel = destination->mip_level,
            .origin = (WGPUOrigin3D){ destination->x, destination->y, destination->z + destination->layer },
        };

        wgpuCommandEncoderCopyBufferToTexture(cmdBuf->encoder, &sourceInfo, &destInfo, &(WGPUExtent3D){ paddedWidth, paddedHeight, destination->d });

        if (!hadToPad) {
            // If we had to pad the buffer ourselves, there's no actual dependency on the
            // user's source buffer so we don't have to increment the reference count.
            SDL_AtomicIncRef(&finalSourceBuffer->referenceCount);
            WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity,
                                                   cmdBuf->submitted.usedBufferCount, WebGPUBuffer *, finalSourceBuffer);
        }
    }

    SDL_AtomicIncRef(&((WebGPUTextureContainer *)destination->texture)->activeTexture->referenceCount);
    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity,
                                           cmdBuf->submitted.usedTextureCount, WebGPUTexture *,
                                           ((WebGPUTextureContainer *)destination->texture)->activeTexture);

    if (hadToPad) {
        WEBGPU_INTERNAL_QueueBufferForRelease(cmdBuf->renderer, finalSourceBuffer);
    }
}

static void WEBGPU_PushDebugGroup(SDL_GPUCommandBuffer *commandBuffer, const char *name)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    if (cmdBuf->common.render_pass.in_progress) {
        wgpuRenderPassEncoderPushDebugGroup(cmdBuf->renderPassEncoder, (WGPUStringView){ name, SDL_strlen(name) });
    } else if (cmdBuf->common.compute_pass.in_progress) {
        wgpuComputePassEncoderPushDebugGroup(cmdBuf->computePassEncoder, (WGPUStringView){ name, SDL_strlen(name) });
    } else {
        // segfaults are lies made up by the woke left to distract you from the fact
        // that memory access restrictions don't apply to the pure of heart
        wgpuCommandEncoderPushDebugGroup(cmdBuf->encoder, (WGPUStringView){ name, SDL_strlen(name) });
    }
}

static void WEBGPU_PopDebugGroup(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    if (cmdBuf->common.render_pass.in_progress) {
        wgpuRenderPassEncoderPopDebugGroup(cmdBuf->renderPassEncoder);
    } else if (cmdBuf->common.compute_pass.in_progress) {
        wgpuComputePassEncoderPopDebugGroup(cmdBuf->computePassEncoder);
    } else {
        wgpuCommandEncoderPopDebugGroup(cmdBuf->encoder);
    }
}

static void WEBGPU_InsertDebugLabel(SDL_GPUCommandBuffer *commandBuffer, const char *text)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    // Each type of encoder has its own debug group functions for some reason
    if (cmdBuf->common.render_pass.in_progress) {
        wgpuRenderPassEncoderInsertDebugMarker(cmdBuf->renderPassEncoder, (WGPUStringView){ text, SDL_strlen(text) });
    } else if (cmdBuf->common.compute_pass.in_progress) {
        wgpuComputePassEncoderInsertDebugMarker(cmdBuf->computePassEncoder, (WGPUStringView){ text, SDL_strlen(text) });
    } else {
        wgpuCommandEncoderInsertDebugMarker(cmdBuf->encoder, (WGPUStringView){ text, SDL_strlen(text) });
    }
}

static void WEBGPU_BeginRenderPass(SDL_GPUCommandBuffer *commandBuffer, const SDL_GPUColorTargetInfo *colorTargetInfos,
                                   Uint32 numColorTargets, const SDL_GPUDepthStencilTargetInfo *depthStencilTargetInfo)
{
    WebGPUCommandBuffer *wrapper = (WebGPUCommandBuffer *)commandBuffer;

    WGPURenderPassDescriptor desc = { 0 };

    WGPURenderPassColorAttachment *colorAttachments;
    WGPURenderPassDepthStencilAttachment *depthStencilAttachment = NULL;

    colorAttachments = (WGPURenderPassColorAttachment *)SDL_calloc(numColorTargets, sizeof(*colorAttachments));

    for (int i = 0; i < numColorTargets; i++) {
        WebGPUTexture *texture = ((WebGPUTextureContainer *)colorTargetInfos[i].texture)->activeTexture;
        colorAttachments[i].clearValue.a = colorTargetInfos[i].clear_color.a;
        colorAttachments[i].clearValue.r = colorTargetInfos[i].clear_color.r;
        colorAttachments[i].clearValue.g = colorTargetInfos[i].clear_color.g;
        colorAttachments[i].clearValue.b = colorTargetInfos[i].clear_color.b;
        colorAttachments[i].loadOp = SDLToWebGPU_LoadOp[colorTargetInfos[i].load_op];
        colorAttachments[i].storeOp = SDLToWebGPU_StoreOp[colorTargetInfos[i].store_op];
        colorAttachments[i].depthSlice = texture->type == SDL_GPU_TEXTURETYPE_3D ? colorTargetInfos[i].layer_or_depth_plane : WGPU_DEPTH_SLICE_UNDEFINED;

        switch (((WebGPUTextureContainer *)colorTargetInfos[i].texture)->activeTexture->type) {
        case SDL_GPU_TEXTURETYPE_3D:
            colorAttachments[i].view = texture->fullTextureView->view;
            break;
        case SDL_GPU_TEXTURETYPE_2D:
        case SDL_GPU_TEXTURETYPE_2D_ARRAY:
        case SDL_GPU_TEXTURETYPE_CUBE:
        case SDL_GPU_TEXTURETYPE_CUBE_ARRAY:
            colorAttachments[i].view = texture->textureViews[WEBGPU_INTERNAL_GetTextureViewIndex(
                                                                 colorTargetInfos[i].layer_or_depth_plane,
                                                                 colorTargetInfos[i].mip_level,
                                                                 wgpuTextureGetMipLevelCount(texture->texture))]
                                           ->view;
            break;
        }

        if (colorTargetInfos[i].store_op == SDL_GPU_STOREOP_RESOLVE || colorTargetInfos[i].store_op == SDL_GPU_STOREOP_RESOLVE_AND_STORE) {
            WebGPUTexture *resolveTexture = ((WebGPUTextureContainer *)colorTargetInfos[i].resolve_texture)->activeTexture;

            colorAttachments[i].resolveTarget = resolveTexture->textureViews[WEBGPU_INTERNAL_GetTextureViewIndex(
                                                                                 colorTargetInfos[i].resolve_layer,
                                                                                 colorTargetInfos[i].resolve_mip_level,
                                                                                 wgpuTextureGetMipLevelCount(resolveTexture->texture))]
                                                    ->view;
        }
    }

    if (depthStencilTargetInfo != NULL) {
        SDL_GPUTextureFormat textureFormat = ((WebGPUTextureContainer *)depthStencilTargetInfo->texture)->header.info.format;
        depthStencilAttachment = (WGPURenderPassDepthStencilAttachment *)SDL_calloc(1, sizeof(*depthStencilAttachment));

        depthStencilAttachment->depthClearValue = depthStencilTargetInfo->clear_depth;
        depthStencilAttachment->depthLoadOp = SDLToWebGPU_LoadOp[depthStencilTargetInfo->load_op];
        depthStencilAttachment->depthStoreOp = SDLToWebGPU_StoreOp[depthStencilTargetInfo->store_op];
        depthStencilAttachment->stencilClearValue = IsStencilFormat(textureFormat) ? depthStencilTargetInfo->clear_stencil : 0;
        depthStencilAttachment->stencilLoadOp = IsStencilFormat(textureFormat) ? SDLToWebGPU_LoadOp[depthStencilTargetInfo->stencil_load_op] : WGPULoadOp_Undefined;
        depthStencilAttachment->stencilStoreOp = IsStencilFormat(textureFormat) ? SDLToWebGPU_StoreOp[depthStencilTargetInfo->stencil_store_op] : WGPUStoreOp_Undefined;
        depthStencilAttachment->view =
            ((WebGPUTextureContainer *)depthStencilTargetInfo->texture)->activeTexture->textureViews[WEBGPU_INTERNAL_GetTextureViewIndex(depthStencilTargetInfo->layer, depthStencilTargetInfo->mip_level, wgpuTextureGetMipLevelCount(((WebGPUTextureContainer *)depthStencilTargetInfo->texture)->activeTexture->texture))]->view;
    }

    desc.colorAttachments = colorAttachments;
    desc.colorAttachmentCount = numColorTargets;
    desc.depthStencilAttachment = depthStencilTargetInfo != NULL ? depthStencilAttachment : NULL;
    desc.occlusionQuerySet = NULL; // Unimplemented in SDLGPU
    desc.timestampWrites = NULL;   // Unimplemented in SDLGPU as of 2026-06-03, although, there is a proof of concept Query API in development right now.
    desc.label = (WGPUStringView){ NULL, WGPU_STRLEN };

    wrapper->renderPassEncoder = wgpuCommandEncoderBeginRenderPass(wrapper->encoder, &desc);
    wrapper->boundGraphicsPipeline = NULL;
    wrapper->hasBoundGraphicsPipeline = false;

    SDL_free(colorAttachments);
    SDL_free(depthStencilAttachment);
}

static void WEBGPU_SetViewport(SDL_GPUCommandBuffer *renderPass, const SDL_GPUViewport *viewport)
{
    wgpuRenderPassEncoderSetViewport(((WebGPUCommandBuffer *)renderPass)->renderPassEncoder, viewport->x, viewport->y, viewport->w, viewport->h, viewport->min_depth, viewport->max_depth);
}

static void WEBGPU_PushVertexUniformData(SDL_GPUCommandBuffer *commandBuffer, Uint32 slotIndex, const void *data, uint32_t length)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    WebGPUQueuedUniformDataUpload upload = {
        .data = SDL_malloc(ALIGN_VALUE(length, 256)),
        .length = ALIGN_VALUE(length, 256),
        .offset = cmdBuf->vertexStageBinds.currentUniformWriteOffsets[slotIndex],
        .slot = slotIndex,
    };
    SDL_memcpy(upload.data, data, length);

    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->queuedUniformUploads, cmdBuf->queuedUniformUploadCapacity,
                                           cmdBuf->numQueuedUniformUploads, WebGPUQueuedUniformDataUpload, upload);

    // jank and gross but it works
    cmdBuf->vertexStageBinds.currentUniformReadOffsets[slotIndex] = cmdBuf->vertexStageBinds.currentUniformWriteOffsets[slotIndex];
    cmdBuf->vertexStageBinds.currentUniformWriteOffsets[slotIndex] += ALIGN_VALUE(length, 256);
    cmdBuf->vertexStageBinds.uniformBindGroupOutdated = true;
}

static void WEBGPU_PushFragmentUniformData(SDL_GPUCommandBuffer *commandBuffer, Uint32 slotIndex, const void *data, uint32_t length)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    WebGPUQueuedUniformDataUpload upload = {
        .data = SDL_malloc(ALIGN_VALUE(length, 256)),
        .length = ALIGN_VALUE(length, 256),
        .offset = cmdBuf->fragmentStageBinds.currentUniformWriteOffsets[slotIndex],
        .slot = 4 + slotIndex,
    };
    SDL_memcpy(upload.data, data, length);

    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->queuedUniformUploads, cmdBuf->queuedUniformUploadCapacity,
                                           cmdBuf->numQueuedUniformUploads, WebGPUQueuedUniformDataUpload, upload);

    cmdBuf->fragmentStageBinds.currentUniformReadOffsets[slotIndex] = cmdBuf->fragmentStageBinds.currentUniformWriteOffsets[slotIndex];
    cmdBuf->fragmentStageBinds.currentUniformWriteOffsets[slotIndex] += ALIGN_VALUE(length, 256);
    cmdBuf->fragmentStageBinds.uniformBindGroupOutdated = true;
}

static void WEBGPU_PushComputeUniformData(SDL_GPUCommandBuffer *commandBuffer, Uint32 slotIndex, const void *data, uint32_t length)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    WebGPUQueuedUniformDataUpload upload = {
        .data = SDL_malloc(ALIGN_VALUE(length, 256)),
        .length = ALIGN_VALUE(length, 256),
        .offset = cmdBuf->computeStageBinds.currentUniformWriteOffsets[slotIndex],
        .slot = 8 + slotIndex,
    };
    SDL_memcpy(upload.data, data, length);

    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->queuedUniformUploads, cmdBuf->queuedUniformUploadCapacity,
                                           cmdBuf->numQueuedUniformUploads, WebGPUQueuedUniformDataUpload, upload);

    cmdBuf->computeStageBinds.currentUniformReadOffsets[slotIndex] = cmdBuf->computeStageBinds.currentUniformWriteOffsets[slotIndex];
    cmdBuf->computeStageBinds.currentUniformWriteOffsets[slotIndex] += ALIGN_VALUE(length, 256);
    cmdBuf->computeStageBinds.uniformBindGroupOutdated = true;
}

static void WEBGPU_SetBlendConstants(SDL_GPUCommandBuffer *commandBuffer, SDL_FColor blendConstants)
{
    wgpuRenderPassEncoderSetBlendConstant(((WebGPUCommandBuffer *)commandBuffer)->renderPassEncoder, &(WGPUColor){ blendConstants.a, blendConstants.r, blendConstants.g, blendConstants.b });
}

static void WEBGPU_SetScissor(SDL_GPUCommandBuffer *commandBuffer, const SDL_Rect *scissor)
{
    wgpuRenderPassEncoderSetScissorRect(((WebGPUCommandBuffer *)commandBuffer)->renderPassEncoder, scissor->x, scissor->y, scissor->w, scissor->h);
}

static bool WEBGPU_WaitAndAcquireSwapchainTexture(SDL_GPUCommandBuffer *command_buffer, SDL_Window *window, SDL_GPUTexture **swapchain_texture, Uint32 *swapchain_texture_width, Uint32 *swapchain_texture_height)
{
    bool result = WEBGPU_AcquireSwapchainTexture(command_buffer, window, swapchain_texture, swapchain_texture_width, swapchain_texture_height);

    while (*swapchain_texture == NULL) {
        WEBGPU_INTERNAL_HandlePendingDestroys(((WebGPUCommandBuffer *)command_buffer)->renderer);
        SDL_DelayNS(15);

        result = WEBGPU_AcquireSwapchainTexture(command_buffer, window, swapchain_texture, swapchain_texture_width, swapchain_texture_height);
    }

    return result;
}

static void WEBGPU_ReleaseTexture(SDL_GPURenderer *renderer, SDL_GPUTexture *texture)
{
    WEBGPU_INTERNAL_QueueTextureContainerForRelease((WebGPURenderer *)renderer, (WebGPUTextureContainer *)texture);
}

static void WEBGPU_BindGraphicsPipeline(SDL_GPUCommandBuffer *renderPass, SDL_GPUGraphicsPipeline *graphicsPipeline)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;
    wgpuRenderPassEncoderSetPipeline(cmdBuf->renderPassEncoder, ((WebGPUGraphicsPipeline *)graphicsPipeline)->pipeline);

    WEBGPU_INTERNAL_ClearRenderPassBindings(cmdBuf);

    cmdBuf->boundGraphicsPipeline = ((WebGPUGraphicsPipeline *)graphicsPipeline);
    cmdBuf->hasBoundGraphicsPipeline = true;
}

static void WEBGPU_BindVertexBuffers(SDL_GPUCommandBuffer *renderPass, Uint32 firstSlot, const SDL_GPUBufferBinding *binding, uint32_t numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->vertexStageBinds.boundVertexBuffers[i + firstSlot].buffer = ((WebGPUBufferContainer *)binding[i].buffer)->activeBuffer;
        cmdBuf->vertexStageBinds.boundVertexBuffers[i + firstSlot].offset = binding[i].offset;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount,
                                               WebGPUBuffer *, ((WebGPUBufferContainer *)binding[i].buffer)->activeBuffer);
        SDL_AtomicIncRef(&((WebGPUBufferContainer *)binding[i].buffer)->activeBuffer->referenceCount);
    }

    cmdBuf->vertexStageBinds.shouldBindVertexBuffers = true;
}

static void WEBGPU_BindIndexBuffer(SDL_GPUCommandBuffer *renderPass, const SDL_GPUBufferBinding *binding, SDL_GPUIndexElementSize elementSize)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    cmdBuf->vertexStageBinds.boundIndexBuffer.buffer = ((WebGPUBufferContainer *)binding->buffer)->activeBuffer;
    cmdBuf->vertexStageBinds.boundIndexBuffer.offset = binding->offset;

    cmdBuf->vertexStageBinds.indexFormat = SDLToWebGPU_IndexFormat[elementSize];
    cmdBuf->vertexStageBinds.shouldBindIndexBuffer = true;

    WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount,
                                           WebGPUBuffer *, ((WebGPUBufferContainer *)binding->buffer)->activeBuffer);
    SDL_AtomicIncRef(&((WebGPUBufferContainer *)binding->buffer)->activeBuffer->referenceCount);
}

static void WEBGPU_BindVertexSamplers(SDL_GPUCommandBuffer *renderPass, Uint32 firstSlot, const SDL_GPUTextureSamplerBinding *textureSamplerBindings, uint32_t numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    for (int i = 0; i < numBindings; i++) {
        // Hack fix for FNA
        if (cmdBuf->hasBoundGraphicsPipeline) {
            if (cmdBuf->boundGraphicsPipeline->vertexBindGroupLayouts.numSamplerStorageEntries == 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Attempting to bind samplers to vertex shader with no binds!");
                return;
            }
        }
        cmdBuf->vertexStageBinds.boundSamplers[i + firstSlot] = (WebGPUSampler *)textureSamplerBindings[i].sampler;
        cmdBuf->vertexStageBinds.boundTextures[i + firstSlot] = ((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture->fullTextureView;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity, cmdBuf->submitted.usedTextureCount,
                                               WebGPUTexture *, ((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture);

        SDL_AtomicIncRef(&((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture->referenceCount);
    }
    cmdBuf->vertexStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindVertexStorageBuffers(SDL_GPUCommandBuffer *renderPass, Uint32 firstSlot, SDL_GPUBuffer *const *storageBuffers, uint32_t numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->vertexStageBinds.boundStorageBuffers[i + firstSlot] = ((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount,
                                               WebGPUBuffer *, ((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer);
        SDL_AtomicIncRef(&((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer->referenceCount);
    }
    cmdBuf->vertexStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindVertexStorageTextures(SDL_GPUCommandBuffer *renderPass, Uint32 firstSlot, SDL_GPUTexture *const *storageTextures, uint32_t numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->vertexStageBinds.boundStorageTextures[i + firstSlot] = ((WebGPUTextureContainer *)storageTextures[i])->activeTexture->fullTextureView;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity, cmdBuf->submitted.usedTextureCount,
                                               WebGPUTexture *, ((WebGPUTextureContainer *)storageTextures[i])->activeTexture);
        SDL_AtomicIncRef(&((WebGPUTextureContainer *)storageTextures[i])->activeTexture->referenceCount);
    }
    cmdBuf->vertexStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindFragmentSamplers(SDL_GPUCommandBuffer *renderPass, Uint32 firstSlot, const SDL_GPUTextureSamplerBinding *textureSamplerBindings, uint32_t numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    for (int i = 0; i < numBindings; i++) {
        if (cmdBuf->hasBoundGraphicsPipeline) {
            if (cmdBuf->boundGraphicsPipeline->fragmentBindGroupLayouts.numSamplerStorageEntries == 0) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Attempting to bind samplers to fragment shader with no binds!");
                return;
            }
        }
        // Bounds-checking is for cowards too afraid to accept the inherent randomness of the universe, and the innate beauty that it creates.
        cmdBuf->fragmentStageBinds.boundSamplers[i + firstSlot] = (WebGPUSampler *)textureSamplerBindings[i].sampler;
        cmdBuf->fragmentStageBinds.boundTextures[i + firstSlot] = ((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture->fullTextureView;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity, cmdBuf->submitted.usedTextureCount,
                                               WebGPUTexture *, ((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture);

        SDL_AtomicIncRef(&((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture->referenceCount);
    }
    cmdBuf->fragmentStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindFragmentStorageBuffers(SDL_GPUCommandBuffer *renderPass, Uint32 firstSlot, SDL_GPUBuffer *const *storageBuffers, uint32_t numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->fragmentStageBinds.boundStorageBuffers[i + firstSlot] = ((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount,
                                               WebGPUBuffer *, ((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer);
        SDL_AtomicIncRef(&((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer->referenceCount);
    }
    cmdBuf->fragmentStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindFragmentStorageTextures(SDL_GPUCommandBuffer *renderPass, Uint32 firstSlot, SDL_GPUTexture *const *storageTextures, uint32_t numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)renderPass;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->fragmentStageBinds.boundStorageTextures[i + firstSlot] = ((WebGPUTextureContainer *)storageTextures[i])->activeTexture->fullTextureView;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity, cmdBuf->submitted.usedTextureCount,
                                               WebGPUTexture *, ((WebGPUTextureContainer *)storageTextures[i])->activeTexture);
        SDL_AtomicIncRef(&((WebGPUTextureContainer *)storageTextures[i])->activeTexture->referenceCount);
    }
    cmdBuf->fragmentStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindComputeSamplers(SDL_GPUCommandBuffer *commandBuffer, Uint32 firstSlot, const SDL_GPUTextureSamplerBinding *textureSamplerBindings, Uint32 numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->computeStageBinds.boundSamplers[i + firstSlot] = (WebGPUSampler *)textureSamplerBindings[i].sampler;
        cmdBuf->computeStageBinds.boundTextures[i + firstSlot] = ((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture->fullTextureView;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity, cmdBuf->submitted.usedTextureCount,
                                               WebGPUTexture *, ((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture);

        SDL_AtomicIncRef(&((WebGPUTextureContainer *)textureSamplerBindings[i].texture)->activeTexture->referenceCount);
    }
    cmdBuf->computeStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindComputeStorageTextures(SDL_GPUCommandBuffer *commandBuffer, Uint32 firstSlot, SDL_GPUTexture *const *storageTextures, Uint32 numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->computeStageBinds.boundReadOnlyStorageTextures[i + firstSlot] = ((WebGPUTextureContainer *)storageTextures[i])->activeTexture->fullTextureView;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity, cmdBuf->submitted.usedTextureCount,
                                               WebGPUTexture *, ((WebGPUTextureContainer *)storageTextures[i])->activeTexture);
        SDL_AtomicIncRef(&((WebGPUTextureContainer *)storageTextures[i])->activeTexture->referenceCount);
    }
    cmdBuf->computeStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_BindComputeStorageBuffers(SDL_GPUCommandBuffer *commandBuffer, Uint32 firstSlot, SDL_GPUBuffer *const *storageBuffers, Uint32 numBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    for (int i = 0; i < numBindings; i++) {
        cmdBuf->computeStageBinds.boundReadOnlyStorageBuffers[i + firstSlot] = ((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount,
                                               WebGPUBuffer *, ((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer);
        SDL_AtomicIncRef(&((WebGPUBufferContainer *)storageBuffers[i])->activeBuffer->referenceCount);
    }
    cmdBuf->computeStageBinds.samplerStorageBindGroupOutdated = true;
}

static void WEBGPU_INTERNAL_BindQueuedGraphicsResources(WebGPUCommandBuffer *cmdBuf)
{
    WGPUBindGroup bindGroups[4] = {
        WEBGPU_INTERNAL_GetBindGroup(cmdBuf, WEBGPU_BINDGROUP_VERTEXSAMPLERSTORAGE),
        WEBGPU_INTERNAL_GetBindGroup(cmdBuf, WEBGPU_BINDGROUP_VERTEXUNIFORMS),
        WEBGPU_INTERNAL_GetBindGroup(cmdBuf, WEBGPU_BINDGROUP_FRAGMENTSAMPLERSTORAGE),
        WEBGPU_INTERNAL_GetBindGroup(cmdBuf, WEBGPU_BINDGROUP_FRAGMENTUNIFORMS),
    };

    for (int i = 0; i < 4; i++) {
        if (bindGroups[i] != NULL) {
            if (i == 1) {
                // vert uniform
                wgpuRenderPassEncoderSetBindGroup(cmdBuf->renderPassEncoder, i, bindGroups[i], 4,
                                                  cmdBuf->vertexStageBinds.currentUniformReadOffsets);
            } else if (i == 3) {
                // frag uniform
                wgpuRenderPassEncoderSetBindGroup(cmdBuf->renderPassEncoder, i, bindGroups[i], 4,
                                                  cmdBuf->fragmentStageBinds.currentUniformReadOffsets);
            } else {
                wgpuRenderPassEncoderSetBindGroup(cmdBuf->renderPassEncoder, i, bindGroups[i], 0, NULL);
            }
        }
    }

    if (cmdBuf->vertexStageBinds.shouldBindVertexBuffers) {
        for (int i = 0; i < MAX_VERTEX_BUFFERS; i++) {
            if (cmdBuf->vertexStageBinds.boundVertexBuffers[i].buffer == NULL) {
                continue;
            }

            wgpuRenderPassEncoderSetVertexBuffer(cmdBuf->renderPassEncoder,
                                                 i, cmdBuf->vertexStageBinds.boundVertexBuffers[i].buffer->buffer,
                                                 cmdBuf->vertexStageBinds.boundVertexBuffers[i].offset, WGPU_WHOLE_SIZE);
        }
    }

    if (cmdBuf->vertexStageBinds.shouldBindIndexBuffer) {
        if (cmdBuf->vertexStageBinds.boundIndexBuffer.buffer) {
            wgpuRenderPassEncoderSetIndexBuffer(cmdBuf->renderPassEncoder,
                                                cmdBuf->vertexStageBinds.boundIndexBuffer.buffer->buffer,
                                                cmdBuf->vertexStageBinds.indexFormat,
                                                cmdBuf->vertexStageBinds.boundIndexBuffer.offset,
                                                cmdBuf->vertexStageBinds.boundIndexBuffer.buffer->size);
        }
    }

    cmdBuf->vertexStageBinds.shouldBindIndexBuffer = false;
    cmdBuf->vertexStageBinds.shouldBindVertexBuffers = false;
    cmdBuf->vertexStageBinds.uniformBindGroupOutdated = false;
    cmdBuf->fragmentStageBinds.uniformBindGroupOutdated = false;
    cmdBuf->vertexStageBinds.samplerStorageBindGroupOutdated = false;
    cmdBuf->fragmentStageBinds.samplerStorageBindGroupOutdated = false;
}

static void WEBGPU_INTERNAL_BindQueuedComputeResources(WebGPUCommandBuffer *cmdBuf)
{
    WGPUBindGroup bindGroups[3] = {
        WEBGPU_INTERNAL_GetBindGroup(cmdBuf, WEBGPU_BINDGROUP_COMPUTESAMPLERSTORAGE),
        WEBGPU_INTERNAL_GetBindGroup(cmdBuf, WEBGPU_BINDGROUP_COMPUTEREADWRITESTORAGE),
        WEBGPU_INTERNAL_GetBindGroup(cmdBuf, WEBGPU_BINDGROUP_COMPUTEUNIFORMS),
    };

    for (int i = 0; i < 3; i++) {
        if (bindGroups[i] != NULL) {
            if (i == 2) {
                wgpuComputePassEncoderSetBindGroup(cmdBuf->computePassEncoder, i, bindGroups[i],
                                                   4, cmdBuf->computeStageBinds.currentUniformReadOffsets);
            } else {
                wgpuComputePassEncoderSetBindGroup(cmdBuf->computePassEncoder, i, bindGroups[i], 0, NULL);
            }
        }
    }

    cmdBuf->computeStageBinds.uniformBindGroupOutdated = false;
    cmdBuf->computeStageBinds.samplerStorageBindGroupOutdated = false;
    cmdBuf->computeStageBinds.readWriteStorageBindGroupOutdated = false;
}

static void WEBGPU_EndRenderPass(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;
    wgpuRenderPassEncoderEnd(cmdBuf->renderPassEncoder);
    wgpuRenderPassEncoderRelease(cmdBuf->renderPassEncoder);
}

static void WEBGPU_DrawPrimitives(SDL_GPUCommandBuffer *renderPass, Uint32 numVertices, uint32_t numInstances, uint32_t firstVertex, uint32_t firstInstance)
{
    if (!((WebGPUCommandBuffer *)renderPass)->hasBoundGraphicsPipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No bound graphics pipeline!");
    }

    WEBGPU_INTERNAL_BindQueuedGraphicsResources((WebGPUCommandBuffer *)renderPass);
    wgpuRenderPassEncoderDraw(((WebGPUCommandBuffer *)renderPass)->renderPassEncoder, numVertices, numInstances, firstVertex, firstInstance);
}

static void WEBGPU_DrawPrimitivesIndirect(SDL_GPUCommandBuffer *renderPass, SDL_GPUBuffer *buffer, Uint32 offset, uint32_t drawCount)
{
    if (!((WebGPUCommandBuffer *)renderPass)->hasBoundGraphicsPipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No bound graphics pipeline!");
    }
    WEBGPU_INTERNAL_BindQueuedGraphicsResources((WebGPUCommandBuffer *)renderPass);

    for (int i = 0; i < drawCount; i++) {
        // ZEUS!
        // WebGPU, unlike other (cough cough good) API's, doesn't support multi-draw indirect rendering.
        // So, we have to do this.
        // NOTE: Dawn actually supports multi-draw indirect. Maybe we should add conditional support for it?
        wgpuRenderPassEncoderDrawIndirect(((WebGPUCommandBuffer *)renderPass)->renderPassEncoder,
                                          ((WebGPUBufferContainer *)buffer)->activeBuffer->buffer,
                                          (size_t)offset + (i * 16));
    }
}

static void WEBGPU_DrawIndexedPrimitives(SDL_GPUCommandBuffer *renderPass, Uint32 numIndices, uint32_t numInstances, uint32_t firstIndex, Sint32 vertexOffset, uint32_t firstInstance)
{
    if (!((WebGPUCommandBuffer *)renderPass)->hasBoundGraphicsPipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No bound graphics pipeline!");
    }

    WEBGPU_INTERNAL_BindQueuedGraphicsResources((WebGPUCommandBuffer *)renderPass);
    wgpuRenderPassEncoderDrawIndexed(((WebGPUCommandBuffer *)renderPass)->renderPassEncoder, numIndices, numInstances, firstIndex, vertexOffset, firstInstance);
}

static void WEBGPU_DrawIndexedPrimitivesIndirect(SDL_GPUCommandBuffer *renderPass, SDL_GPUBuffer *buffer, Uint32 offset, uint32_t drawCount)
{
    if (!((WebGPUCommandBuffer *)renderPass)->hasBoundGraphicsPipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "No bound graphics pipeline!");
    }
    WEBGPU_INTERNAL_BindQueuedGraphicsResources((WebGPUCommandBuffer *)renderPass);

    for (int i = 0; i < drawCount; i++) {
        wgpuRenderPassEncoderDrawIndexedIndirect(((WebGPUCommandBuffer *)renderPass)->renderPassEncoder,
                                                 ((WebGPUBufferContainer *)buffer)->activeBuffer->buffer,
                                                 (size_t)offset + (i * 20));
    }
}

static void WEBGPU_SetStencilReference(SDL_GPUCommandBuffer *commandBuffer, Uint8 reference)
{
    wgpuRenderPassEncoderSetStencilReference(((WebGPUCommandBuffer *)commandBuffer)->renderPassEncoder, reference);
}

static SDL_GPUComputePipeline *WEBGPU_CreateComputePipeline(SDL_GPURenderer *device, const SDL_GPUComputePipelineCreateInfo *createInfo)
{
    WebGPUComputePipeline *pipeline = NULL;

    WGPUComputePipelineDescriptor pipelineDesc;

    WGPUPipelineLayout pipelineLayout;
    WGPUPipelineLayoutDescriptor pipelineLayoutDesc;
    WGPUBindGroupLayout *pipelineLayoutEntries = NULL;

    WGPUShaderModuleDescriptor shaderDesc;
    WGPUShaderSourceWGSL shaderSource;

    const char *pipelineDebugName = SDL_GetStringProperty(createInfo->props, SDL_PROP_GPU_COMPUTEPIPELINE_CREATE_NAME_STRING, NULL);

    shaderSource.chain.next = NULL;
    shaderSource.chain.sType = WGPUSType_ShaderSourceWGSL;
    shaderSource.code = (WGPUStringView){ (char *)createInfo->code, createInfo->code_size };

    shaderDesc.label = (WGPUStringView){ NULL, 0 };
    shaderDesc.nextInChain = &shaderSource.chain;

    pipeline = SDL_calloc(1, sizeof(*pipeline));

    pipeline->computeShader = wgpuDeviceCreateShaderModule(((WebGPURenderer *)device)->device, &shaderDesc);
    if (pipeline->computeShader == NULL) {
        SDL_SetError("Failed to compile compute shader!");
        SDL_free(pipeline);
        return NULL;
    }

    pipeline->bindGroupLayouts = WEBGPU_INTERNAL_GenerateBindGroupLayoutsForComputeShader((char *)createInfo->code, ((WebGPURenderer *)device));

    if (pipeline->bindGroupLayouts == NULL) {
        SDL_SetError("Failed to generate bind group layouts for compute shader!");

        wgpuShaderModuleRelease(pipeline->computeShader);
        SDL_free(pipeline);

        return NULL;
    }

    pipelineLayoutEntries = SDL_calloc(3, sizeof(*pipelineLayoutEntries));
    pipelineLayoutEntries[0] = pipeline->bindGroupLayouts->samplerStorageBindGroupLayout;
    pipelineLayoutEntries[1] = pipeline->bindGroupLayouts->readWriteStorageBindGroupLayout;
    pipelineLayoutEntries[2] = pipeline->bindGroupLayouts->uniformBindGroupLayout;

    pipelineLayoutDesc.bindGroupLayoutCount = 3;
    pipelineLayoutDesc.immediateSize = 0;
    pipelineLayoutDesc.label = (WGPUStringView){ NULL, 0 };
    pipelineLayoutDesc.nextInChain = NULL;
    pipelineLayoutDesc.bindGroupLayouts = pipelineLayoutEntries;

    pipelineLayout = wgpuDeviceCreatePipelineLayout(((WebGPURenderer *)device)->device, &pipelineLayoutDesc);
    if (pipelineLayout == NULL) {
        SDL_SetError("Failed to create pipeline layout for compute pipeline!");
        wgpuShaderModuleRelease(pipeline->computeShader);
        wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->samplerStorageBindGroupLayout);
        wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->readWriteStorageBindGroupLayout);
        wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->uniformBindGroupLayout);

        SDL_free(pipelineLayoutEntries);
        SDL_free(pipeline);

        return NULL;
    }

    pipelineDesc.compute = (WGPUComputeState){
        .constantCount = 0,
        .constants = NULL,
        .entryPoint = (WGPUStringView){ createInfo->entrypoint, SDL_strlen(createInfo->entrypoint) },
        .module = pipeline->computeShader,
        .nextInChain = NULL,
    };
    pipelineDesc.label = pipelineDebugName != NULL ? (WGPUStringView){ pipelineDebugName, SDL_strlen(pipelineDebugName) } : (WGPUStringView){ NULL, 0 };
    pipelineDesc.layout = pipelineLayout;
    pipelineDesc.nextInChain = NULL;

    pipeline->pipeline = wgpuDeviceCreateComputePipeline(((WebGPURenderer *)device)->device, &pipelineDesc);

    SDL_free(pipelineLayoutEntries);

    if (pipeline->pipeline == NULL) {
        SDL_SetError("Failed to create compute pipeline!");
        wgpuShaderModuleRelease(pipeline->computeShader);
        wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->samplerStorageBindGroupLayout);
        wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->readWriteStorageBindGroupLayout);
        wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->uniformBindGroupLayout);
        SDL_free(pipeline);

        return NULL;
    }

    return (SDL_GPUComputePipeline *)pipeline;
}

static void WEBGPU_ReleaseComputePipeline(SDL_GPURenderer *driverData, SDL_GPUComputePipeline *computePipeline)
{
    // FIXME: We're just trusting that the user's not an idiot and won't try to release a bound compute pipeline.
    WebGPUComputePipeline *pipeline = (WebGPUComputePipeline *)computePipeline;

    wgpuComputePipelineRelease(pipeline->pipeline);
    wgpuShaderModuleRelease(pipeline->computeShader);
    wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->readWriteStorageBindGroupLayout);
    wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->samplerStorageBindGroupLayout);
    wgpuBindGroupLayoutRelease(pipeline->bindGroupLayouts->uniformBindGroupLayout);

    SDL_free(pipeline->bindGroupLayouts);
    SDL_free(computePipeline);
}

static void WEBGPU_INTERNAL_BindComputeReadWriteStorageTextures(WebGPUCommandBuffer *cmdBuf,
                                                                const SDL_GPUStorageTextureReadWriteBinding *bindings, Uint32 numBindings)
{
    for (int i = 0; i < numBindings; i++) {
        if (bindings[i].cycle) {
            WEBGPU_INTERNAL_CycleTextureContainer(cmdBuf->renderer, (WebGPUTextureContainer *)bindings[i].texture);
        }

        // FIXME: Our layer & mip level system is gross and I hate it
        cmdBuf->computeStageBinds.boundReadWriteStorageTextures[i] = ((WebGPUTextureContainer *)bindings->texture)->activeTexture->fullTextureView;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedTextures, cmdBuf->submitted.usedTextureCapacity, cmdBuf->submitted.usedTextureCount,
                                               WebGPUTexture *, ((WebGPUTextureContainer *)bindings[i].texture)->activeTexture);
        SDL_AtomicIncRef(&((WebGPUTextureContainer *)bindings[i].texture)->activeTexture->referenceCount);
    }
    cmdBuf->computeStageBinds.readWriteStorageBindGroupOutdated = true;
}

static void WEBGPU_INTERNAL_BindComputeReadWriteStorageBuffers(WebGPUCommandBuffer *cmdBuf,
                                                               const SDL_GPUStorageBufferReadWriteBinding *bindings, Uint32 numBindings)
{
    for (int i = 0; i < numBindings; i++) {
        if (bindings[i].cycle) {
            WEBGPU_INTERNAL_CycleBufferContainer(cmdBuf->renderer, (WebGPUBufferContainer *)bindings[i].buffer);
        }

        cmdBuf->computeStageBinds.boundReadWriteStorageBuffers[i] = ((WebGPUBufferContainer *)bindings[i].buffer)->activeBuffer;

        WEBGPU_INTERNAL_InsertElementIntoArray(cmdBuf->submitted.usedBuffers, cmdBuf->submitted.usedBufferCapacity, cmdBuf->submitted.usedBufferCount,
                                               WebGPUBuffer *, ((WebGPUBufferContainer *)bindings[i].buffer)->activeBuffer);
        SDL_AtomicIncRef(&((WebGPUBufferContainer *)bindings[i].buffer)->activeBuffer->referenceCount);
    }
    cmdBuf->computeStageBinds.readWriteStorageBindGroupOutdated = true;
}

static void WEBGPU_BeginComputePass(SDL_GPUCommandBuffer *commandBuffer, const SDL_GPUStorageTextureReadWriteBinding *storageTextureBindings, Uint32 numStorageTextureBindings,
                                    const SDL_GPUStorageBufferReadWriteBinding *storageBufferBindings, Uint32 numStorageBufferBindings)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;
    WGPUComputePassDescriptor passDesc;

    passDesc.label = (WGPUStringView){ NULL, 0 };
    passDesc.nextInChain = NULL;
    // Timestamps are unsupported in SDLGPU.
    // 'erm acktually Cosmonaut made a PoC query API hghehgehg' Biden Blast!!!
    passDesc.timestampWrites = NULL;

    cmdBuf->computePassEncoder = wgpuCommandEncoderBeginComputePass(cmdBuf->encoder, &passDesc);
    if (cmdBuf->computePassEncoder == NULL) {
        SDL_SetError("Could not begin compute pass!");
        return;
    }

    WEBGPU_INTERNAL_ClearComputePassBindings(cmdBuf, true);

    WEBGPU_INTERNAL_BindComputeReadWriteStorageTextures(cmdBuf, storageTextureBindings, numStorageTextureBindings);
    WEBGPU_INTERNAL_BindComputeReadWriteStorageBuffers(cmdBuf, storageBufferBindings, numStorageBufferBindings);
}

static void WEBGPU_EndComputePass(SDL_GPUCommandBuffer *computePass)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)computePass;
    wgpuComputePassEncoderEnd(cmdBuf->computePassEncoder);

    wgpuComputePassEncoderRelease(cmdBuf->computePassEncoder);
}

static void WEBGPU_BindComputePipeline(SDL_GPUCommandBuffer *commandBuffer, SDL_GPUComputePipeline *computePipeline)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUComputePipeline *pipeline = (WebGPUComputePipeline *)computePipeline;

    wgpuComputePassEncoderSetPipeline(cmdBuf->computePassEncoder, pipeline->pipeline);
    WEBGPU_INTERNAL_ClearComputePassBindings(cmdBuf, false);

    cmdBuf->boundComputePipeline = (WebGPUComputePipeline *)computePipeline;
    cmdBuf->hasBoundComputePipeline = true;
}

static void WEBGPU_DispatchCompute(SDL_GPUCommandBuffer *commandBuffer, Uint32 groupcountX, Uint32 groupcountY, Uint32 groupcountZ)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    WEBGPU_INTERNAL_BindQueuedComputeResources(cmdBuf);
    wgpuComputePassEncoderDispatchWorkgroups(cmdBuf->computePassEncoder, groupcountX, groupcountY, groupcountZ);
}

static void WEBGPU_DispatchComputeIndirect(SDL_GPUCommandBuffer *commandBuffer, SDL_GPUBuffer *buffer, Uint32 offset)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;

    WEBGPU_INTERNAL_BindQueuedComputeResources(cmdBuf);
    wgpuComputePassEncoderDispatchWorkgroupsIndirect(cmdBuf->computePassEncoder, ((WebGPUBufferContainer *)buffer)->activeBuffer->buffer, offset);
}

static void WEBGPU_INTERNAL_UploadQueuedUniformData(WebGPUCommandBuffer *cmdBuf)
{
    for (int i = 0; i < cmdBuf->numQueuedUniformUploads; i++) {
        WebGPUQueuedUniformDataUpload upload = cmdBuf->queuedUniformUploads[i];

        wgpuQueueWriteBuffer(cmdBuf->queue, cmdBuf->renderer->uniformBuffers[upload.slot]->activeBuffer->buffer, upload.offset, upload.data, upload.length);
        SDL_free(upload.data);
    }
}

static bool WEBGPU_Submit(SDL_GPUCommandBuffer *commandBuffer)
{
    WebGPUCommandBuffer *wrapper = (WebGPUCommandBuffer *)commandBuffer;
    WebGPUSubmittedCommandBuffer *submitted = NULL;
    WebGPURenderer *renderer = wrapper->renderer;

    bool isMainThread = SDL_GetCurrentThreadID() == renderer->createdByThreadID;

    SDL_LockMutex(renderer->submittingCommandBufferLock);

    if (isMainThread) {
        WEBGPU_INTERNAL_UploadQueuedUniformData(wrapper);
    }

#ifdef _MSC_VER
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish((((WebGPUCommandBuffer *)commandBuffer)->encoder), &(WGPUCommandBufferDescriptor)WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT);
#else
    WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish((((WebGPUCommandBuffer *)commandBuffer)->encoder), &WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT);
#endif

    if (!cmdBuf) {
        SDL_free(commandBuffer);
        return false;
    }

    if (wrapper->renderer->queueDoneFence != NULL) {
        // Reregister the fence
        WEBGPU_INTERNAL_ReregisterFence(wrapper->queue, wrapper->renderer->queueDoneFence);
    } else {
        wrapper->renderer->queueDoneFence = WEBGPU_INTERNAL_CreateFence(wrapper->queue);
    }

    wrapper->submitted.fence = WEBGPU_INTERNAL_CreateFence(wrapper->queue);

    submitted = SDL_calloc(1, sizeof(*submitted));
    *submitted = wrapper->submitted;

    WEBGPU_INTERNAL_InsertElementIntoArray(wrapper->renderer->submittedCommandBuffers, wrapper->renderer->submittedCommandBufferCapacity,
                                           wrapper->renderer->submittedCommandBufferCount, WebGPUSubmittedCommandBuffer *, submitted);

    wgpuQueueSubmit(wrapper->queue, 1, &cmdBuf);

    if (isMainThread) {
#ifndef __EMSCRIPTEN__
        for (int i = 0; i < wrapper->surfaceCount; i++) {
            wgpuSurfacePresent(wrapper->surfaces[i]);
        }
#endif
        for (int i = 0; i < wrapper->swapchainTextureCount; i++) {
            WebGPUTextureContainer *container = wrapper->acquiredSwapchainTextures[i];

            for (int j = 0; j < container->textureCount; j++) {
                WEBGPU_INTERNAL_ReleaseTexture(wrapper->renderer, container->textures[j]);
            }

            SDL_free(container->textures);
            SDL_free(container);
        }
    }
    wgpuCommandEncoderRelease(wrapper->encoder);
    wgpuCommandBufferRelease(cmdBuf);

    wrapper->renderer->numSubmissions++;

    // We'll be freeing the "command buffer", so any usage of it will be undefined behaviour.
    // Don't. The docs tell you not to.

    WEBGPU_INTERNAL_FreeCommandBuffer(wrapper);

    SDL_UnlockMutex(renderer->submittingCommandBufferLock);
    return true;
}

static SDL_GPUFence *WEBGPU_SubmitAndAcquireFence(SDL_GPUCommandBuffer *commandBuffer)
{
    WEBGPU_Submit(commandBuffer);

    return (SDL_GPUFence *)WEBGPU_INTERNAL_CreateFence(((WebGPUCommandBuffer *)commandBuffer)->queue);
}

static void WEBGPU_DestroyDevice(SDL_GPUDevice *device)

{
    WebGPURenderer *renderer = (WebGPURenderer *)device->driverData;
    renderer->destroyingSelf = true;

    SDL_LockMutex(renderer->destroyingSelfLock);

    for (int i = 0; i < 12; i++) {
        WEBGPU_INTERNAL_ReleaseBufferContainer(renderer, renderer->uniformBuffers[i]);
    }
    for (int i = 0; i < 3; i++) {
        wgpuBindGroupRelease(renderer->uniformBufferBindGroups[i]);
    }

    WEBGPU_INTERNAL_ReleaseBlitResources(renderer);

    while (renderer->queuedDestroyCount > 0 || renderer->submittedCommandBufferCount > 0) {
        WEBGPU_INTERNAL_HandlePendingDestroys(renderer);
    }

    // Destroying mutexes
    SDL_DestroyMutex(renderer->queryingFenceLock);
    SDL_DestroyMutex(renderer->registeringQueuedDestroyLock);
    SDL_DestroyMutex(renderer->submittingCommandBufferLock);
    SDL_DestroyMutex(renderer->creatingWebGPUResourceLock);

    wgpuQueueRelease(renderer->queue);
    // FIXME: Releasing the device leaks a bunch of memory each time!!! There's 100% some resource I'm not freeing.
    // wgpuDeviceRelease(renderer->device);
    wgpuAdapterRelease(renderer->adapter);
    wgpuInstanceRelease(renderer->instance);

    SDL_DestroyHashTable(renderer->bindGroupHashTable);

    SDL_DestroyProperties(renderer->props);
    SDL_UnlockMutex(renderer->destroyingSelfLock);
    SDL_DestroyMutex(renderer->destroyingSelfLock);
    SDL_free(renderer->blitPipelines);
    SDL_free(renderer->submittedCommandBuffers);
    SDL_free(renderer);
    SDL_free(device);
}

static void WEBGPU_ReleaseWindow(SDL_GPURenderer *driverData, SDL_Window *window)
{
    WebGPUWindowData *windowData = SDL_GetPointerProperty(window->props, WINDOW_PROPERTY_DATA, NULL);
    if (windowData == NULL) {
        return;
    }

    // FIXME: This should be done in the video subsystem!
    wgpuSurfaceUnconfigure(windowData->surface);
    wgpuSurfaceRelease(windowData->surface);
    SDL_ClearProperty(window->props, WINDOW_PROPERTY_DATA);

    SDL_free(windowData);
}

static void WEBGPU_DownloadFromTexture(SDL_GPUCommandBuffer *commandBuffer, const SDL_GPUTextureRegion *source, const SDL_GPUTextureTransferInfo *destination)
{
    WGPUTexelCopyTextureInfo sourceInfo;
    WGPUTexelCopyBufferInfo destinationInfo;

    Uint32 unpaddedBPR = BytesPerRow(source->w, ((WebGPUTextureContainer *)source->texture)->header.info.format);
    Uint32 paddedBPR = ALIGN_VALUE(unpaddedBPR, 256);

    sourceInfo.aspect = WGPUTextureAspect_All;
    sourceInfo.texture = ((WebGPUTextureContainer *)source->texture)->activeTexture->texture;
    sourceInfo.mipLevel = 0;
    sourceInfo.origin = (WGPUOrigin3D){ .x = source->x, .y = source->y, .z = source->z };

    destinationInfo.buffer = ((WebGPUBufferContainer *)destination->transfer_buffer)->activeBuffer->buffer;
    destinationInfo.layout = (WGPUTexelCopyBufferLayout){
        .bytesPerRow = paddedBPR,
        .offset = destination->offset,
        .rowsPerImage = source->h,
    };

    Uint32 blockWidth = Texture_GetBlockWidth(((WebGPUTextureContainer *)source->texture)->activeTexture->format);
    Uint32 blockHeight = Texture_GetBlockHeight(((WebGPUTextureContainer *)source->texture)->activeTexture->format);

    wgpuCommandEncoderCopyTextureToBuffer(((WebGPUCommandBuffer *)commandBuffer)->encoder, &sourceInfo, &destinationInfo, &(WGPUExtent3D){
                                                                                                                              ALIGN_VALUE(source->w, blockWidth),
                                                                                                                              ALIGN_VALUE(source->h, blockHeight),
                                                                                                                              source->layer,
                                                                                                                          });
}

static void WEBGPU_Blit(SDL_GPUCommandBuffer *commandBuffer, const SDL_GPUBlitInfo *info)
{
    WebGPURenderer *renderer = ((WebGPUCommandBuffer *)commandBuffer)->renderer;

    SDL_GPU_BlitCommon(
        commandBuffer,
        info,
        renderer->blitResources.blitLinearSampler,
        renderer->blitResources.blitNearestSampler,
        renderer->blitResources.blitVertexShader,
        renderer->blitResources.blit2DShader,
        renderer->blitResources.blit2DArrayShader,
        renderer->blitResources.blit3DShader,
        renderer->blitResources.blitCubeShader,
        renderer->blitResources.blitCubeArrayShader,
        &renderer->blitPipelines,
        &renderer->blitPipelineCount,
        &renderer->blitPipelineCapacity);
}

static SDL_GPUTextureFormat WEBGPU_GetSwapchainTextureFormat(SDL_GPURenderer *device, SDL_Window *window)
{
    WebGPUWindowData *windowData = SDL_GetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, NULL);

    if (windowData != NULL) {
        return SwapchainCompositionToSDLFormat(windowData->swapchainComposition, windowData->shouldUseFallbackFormat);
    } else {
        SDL_SetError("Window pointer is NULL!");
        return SDL_GPU_TEXTUREFORMAT_INVALID;
    }
}

static bool WEBGPU_SupportsSwapchainComposition(SDL_GPURenderer *driverData, SDL_Window *window, SDL_GPUSwapchainComposition swapchainComposition)
{
    WebGPUWindowData *windowData = SDL_GetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, NULL);
    WGPUSurfaceCapabilities caps = { 0 };
    bool supportsComposition = false;

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "WindowData pointer is NULL!");
        SDL_SetError("WindowData pointer is NULL! Did you call SDL_ClaimWindowForGPUDevice?");
        return false;
    }

    WGPUStatus getCapsStatus = wgpuSurfaceGetCapabilities(windowData->surface, windowData->renderer->adapter, &caps);

    if (getCapsStatus == WGPUStatus_Error) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to get surface capabilities!");
        SDL_SetError("Failed to get surface capabilities!");
        return false;
    }

    for (int i = 0; i < caps.formatCount; i++) {
        if (caps.formats[i] == SwapchainCompositionToFormat[swapchainComposition]) {
            supportsComposition = true;
            windowData->shouldUseFallbackFormat = false;

            break;
        } else if (caps.formats[i] == SwapchainCompositionToFallbackFormat[swapchainComposition]) {
            supportsComposition = true;
            windowData->shouldUseFallbackFormat = true;

            break;
        }
    }

    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return supportsComposition;
}

static bool WEBGPU_SupportsPresentMode(SDL_GPURenderer *driverData, SDL_Window *window, SDL_GPUPresentMode presentMode)
{
    WebGPUWindowData *windowData = SDL_GetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, NULL);
    WGPUSurfaceCapabilities caps = { 0 };
    bool supportsPresentMode = false;

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "WindowData pointer is NULL!");
        SDL_SetError("WindowData pointer is NULL! Did you call SDL_ClaimWindowForGPUDevice?");
        return false;
    }

    WGPUStatus getCapsStatus = wgpuSurfaceGetCapabilities(windowData->surface, windowData->renderer->adapter, &caps);

    if (getCapsStatus == WGPUStatus_Error) {
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "Failed to get surface capabilities!");
        SDL_SetError("Failed to get surface capabilities!");
        return false;
    }

    for (int i = 0; i < caps.presentModeCount; i++) {
        if (caps.presentModes[i] == SDLToWebGPU_PresentMode[presentMode]) {
            supportsPresentMode = true;
            goto finish;
        }
    }

    goto finish;

finish:
    wgpuSurfaceCapabilitiesFreeMembers(caps);
    return supportsPresentMode;
}

static bool WEBGPU_SetSwapchainParameters(SDL_GPURenderer *driverData, SDL_Window *window,
                                          SDL_GPUSwapchainComposition swapchainComposition, SDL_GPUPresentMode presentMode)
{
    WebGPUWindowData *windowData = SDL_GetPointerProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, NULL);

    if (!WEBGPU_SupportsSwapchainComposition(driverData, window, swapchainComposition)) {
        SDL_SetError("Window does not support swapchainComposition!");
        return false;
    }

    if (!WEBGPU_SupportsPresentMode(driverData, window, presentMode)) {
        SDL_SetError("Window does not support presentMode!");
        return false;
    }

    windowData->surfaceConfig.format = windowData->shouldUseFallbackFormat ? SwapchainCompositionToFallbackFormat[swapchainComposition] : SwapchainCompositionToFormat[swapchainComposition];
    windowData->surfaceConfig.presentMode = SDLToWebGPU_PresentMode[presentMode];
    windowData->surfaceConfig.device = windowData->renderer->device;

    windowData->swapchainComposition = swapchainComposition;
    windowData->presentMode = presentMode;

    windowData->surfaceDirty = true;

    return true;
}

static bool WEBGPU_SupportsSampleCount(SDL_GPURenderer *driverData, SDL_GPUTextureFormat format, SDL_GPUSampleCount sampleCount)
{
    if (sampleCount == SDL_GPU_SAMPLECOUNT_2 || sampleCount == SDL_GPU_SAMPLECOUNT_8) {
        SDL_SetError("WebGPU only supports sample counts 1 and 4.");
        return false;
    }

    return true;
}

static void WEBGPU_DownloadFromBuffer(SDL_GPUCommandBuffer *commandBuffer, const SDL_GPUBufferRegion *source, const SDL_GPUTransferBufferLocation *destination)
{
    WebGPUCommandBuffer *cmdBuf = (WebGPUCommandBuffer *)commandBuffer;
    WEBGPU_INTERNAL_CopyBufferToBuffer(cmdBuf->encoder, (WebGPUBufferContainer *)source->buffer, source->offset,
                                       (WebGPUBufferContainer *)destination->transfer_buffer, destination->offset,
                                       ALIGN_VALUE(source->size, 4));
}

static bool WEBGPU_SetAllowedFramesInFlight(SDL_GPURenderer *driverData, Uint32 allowedFramesInFlight)
{
    // NOTE: Not entirely sure this is correct? Should we do something if
    // allowedFramesInFlight < renderer->maxFramesInFlight & swapchain texture has been acquired?

    if (allowedFramesInFlight < 1 || allowedFramesInFlight > 3) {
        SDL_SetError(allowedFramesInFlight < 1 ? "allowedFramesInFlight must be >= 1!" : "allowedFramesInFlight must be <= 3!");
        return false;
    }

    ((WebGPURenderer *)driverData)->maxFramesInFlight = allowedFramesInFlight;
    return true;
};

static void WEBGPU_INTERNAL_InitUniformBuffers(WebGPURenderer *renderer)
{
    for (int i = 0; i < 12; i++) {
        // 1048576 == 1MiB == 2²⁰
        renderer->uniformBuffers[i] = WEBGPU_INTERNAL_CreateBufferContainer(renderer, 1048576, 0, WEBGPU_BUFFER_TYPE_UNIFORM, false, NULL);
    }

    for (int i = 0; i < 3; i++) {
        WGPUBindGroupLayoutEntry *layoutEntries;
        WGPUBindGroupEntry *entries;

        WGPUBindGroupLayoutDescriptor layoutDesc = { 0 };
        WGPUBindGroupDescriptor desc = { 0 };

        WGPUBindGroupLayout layout;

        layoutEntries = SDL_calloc(4, sizeof(*layoutEntries));
        entries = SDL_calloc(4, sizeof(*entries));

        for (int j = 0; j < 4; j++) {
            layoutEntries[j] = (WGPUBindGroupLayoutEntry){ 0 };
            layoutEntries[j].nextInChain = NULL;
            layoutEntries[j].binding = j;
            layoutEntries[j].buffer = (WGPUBufferBindingLayout){
                .minBindingSize = 0,
                .nextInChain = NULL,
                .hasDynamicOffset = true,
                .type = WGPUBufferBindingType_Uniform,
            };

            if (i == 0) {
                layoutEntries[j].visibility = WGPUShaderStage_Vertex;
            } else if (i == 1) {
                layoutEntries[j].visibility = WGPUShaderStage_Fragment;
            } else if (i == 2) {
                layoutEntries[j].visibility = WGPUShaderStage_Compute;
            }

            entries[j] = (WGPUBindGroupEntry){
                .buffer = renderer->uniformBuffers[j + (i * 4)]->activeBuffer->buffer,
                .size = UNIFORM_BUFFER_SIZE,
                .nextInChain = NULL,
                .binding = j,
                .offset = 0,
            };
        }

        layoutDesc.entries = layoutEntries;
        layoutDesc.entryCount = 4;

        layout = wgpuDeviceCreateBindGroupLayout(renderer->device, &layoutDesc);

        desc.layout = layout;
        desc.entries = entries;
        desc.entryCount = 4;

        renderer->uniformBufferBindGroups[i] = wgpuDeviceCreateBindGroup(renderer->device, &desc);
        SDL_free(layoutEntries);
        SDL_free(entries);
    }
}

// -- UNIMPLEMENTED FUNCTIONS --
static bool WEBGPU_WaitForSwapchain(SDL_GPURenderer *driverData, SDL_Window *window)
{
    SDL_assert_release(!"WEBGPU_WaitForSwapchain is unimplemented! Reason: WebGPU abstracts away the swapchain from the user.");
    return true;
}

static bool WEBGPU_Cancel(SDL_GPUCommandBuffer *commandBuffer)
{
    SDL_assert_release(!"WEBGPU_Cancel is unimplemented! Reason: I'm lazy.");
    return true;
}

// NOTE: A lot of this is guesswork at best. WebGPU really has no proper documentation.
static bool WEBGPU_SupportsTextureFormat(SDL_GPURenderer *driverData, SDL_GPUTextureFormat format, SDL_GPUTextureType type, SDL_GPUTextureUsageFlags usage)
{
    // this is horrible garbage code and I hate it so much

    bool hasStorageUsage = usage & (SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ |
                                    SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE);
    bool hasReadWriteStorageUsage = (usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ) || usage & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_SIMULTANEOUS_READ_WRITE;
    bool hasDepthUsage = usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    bool hasColorTargetUsage = usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    bool hasRenderUsage = usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    bool hasSamplerUsage = usage & SDL_GPU_TEXTUREUSAGE_SAMPLER;
    // TODO: Check the type

    WebGPURenderer *renderer = (WebGPURenderer *)driverData;

    if (SDLToWebGPU_TextureFormat[format] == WGPUTextureFormat_Undefined) {
        return false;
    }

    switch (format) {
    case SDL_GPU_TEXTUREFORMAT_INVALID:
    case SDL_GPU_TEXTUREFORMAT_A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B5G6R5_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B5G5R5A1_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B4G4R4A4_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_4x4_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_5x4_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_5x5_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_6x5_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_6x6_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x5_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x6_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x8_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x5_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x6_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x8_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x10_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_12x10_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_ASTC_12x12_FLOAT:
        return false;
    case SDL_GPU_TEXTUREFORMAT_R16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R16G16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R10G10B10A2_UNORM:
        return !hasDepthUsage && !hasReadWriteStorageUsage && !hasSamplerUsage && wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_TextureFormatsTier1);
    case SDL_GPU_TEXTUREFORMAT_R8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_R8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R16_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R16G16_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SNORM:
    case SDL_GPU_TEXTUREFORMAT_R16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_UINT:
    case SDL_GPU_TEXTUREFORMAT_R8_INT:
    case SDL_GPU_TEXTUREFORMAT_R8G8_INT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_INT:
    case SDL_GPU_TEXTUREFORMAT_R16_INT:
    case SDL_GPU_TEXTUREFORMAT_R16G16_INT:
    case SDL_GPU_TEXTUREFORMAT_R16G16B16A16_INT:
    case SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_R32G32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R32G32_UINT:
    case SDL_GPU_TEXTUREFORMAT_R32G32_INT:
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_UINT:
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_INT:
        return !hasDepthUsage && !hasReadWriteStorageUsage;
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB:
        if (hasStorageUsage && !hasReadWriteStorageUsage && wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_BGRA8UnormStorage)) {
            return true;
        } else if (!hasDepthUsage && !hasReadWriteStorageUsage) {
            return true;
        } else {
            return false;
        }
    case SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC4_R_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC5_RG_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM:
    case SDL_GPU_TEXTUREFORMAT_BC6H_RGB_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_BC6H_RGB_UFLOAT:
    case SDL_GPU_TEXTUREFORMAT_BC1_RGBA_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_BC2_RGBA_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_BC3_RGBA_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_BC7_RGBA_UNORM_SRGB:
        return !hasRenderUsage && !hasDepthUsage && !hasReadWriteStorageUsage && wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_TextureCompressionBC);
    case SDL_GPU_TEXTUREFORMAT_R32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_R32_UINT:
    case SDL_GPU_TEXTUREFORMAT_R32_INT:
        return !hasDepthUsage;
    case SDL_GPU_TEXTUREFORMAT_R32G32B32A32_FLOAT:
        if (!wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_Float32Filterable)) {
            if (!wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_Float32Blendable)) {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "While Float32 textures are supported, "
                                                  "they're not filterable nor blendable as neither feature is supported on this device.");
            } else {
                SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "While Float32 textures are supported, "
                                                  "they're not filterable as the required WebGPU feature is not supported on this device.");
            }
        } else if (!wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_Float32Blendable)) {
            SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "While Float32 textures are supported, "
                                              "they're not blendable as the required WebGPU feature is not supported on this device.");
        }

        return !hasDepthUsage && !hasReadWriteStorageUsage;
    case SDL_GPU_TEXTUREFORMAT_R11G11B10_UFLOAT:
        if (hasColorTargetUsage) {
            return wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_RG11B10UfloatRenderable) && !hasDepthUsage && !hasReadWriteStorageUsage;
        } else {
            return false;
        }
    case SDL_GPU_TEXTUREFORMAT_D16_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM:
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT:
    case SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT:
        return hasDepthUsage && !hasReadWriteStorageUsage;
    case SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT:
        return hasDepthUsage && !hasReadWriteStorageUsage && wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_Depth32FloatStencil8);
    case SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM:
    case SDL_GPU_TEXTUREFORMAT_ASTC_4x4_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_5x4_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_5x5_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_6x5_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_6x6_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x5_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x6_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_8x8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x5_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x6_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x8_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_10x10_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_12x10_UNORM_SRGB:
    case SDL_GPU_TEXTUREFORMAT_ASTC_12x12_UNORM_SRGB:
        return !hasRenderUsage && !hasDepthUsage && !hasReadWriteStorageUsage && wgpuDeviceHasFeature(renderer->device, WGPUFeatureName_TextureCompressionASTC);
    default:
        SDL_assert(!"Unsupported SDL_GPUTextureFormat");
        return false;
    }
}

// -- UNSUPPORTED FUNCTIONS --
static void WEBGPU_SetBufferName(SDL_GPURenderer *device, SDL_GPUBuffer *buffer, const char *text)
{
    // No-op.
}

static void WEBGPU_SetTextureName(SDL_GPURenderer *device, SDL_GPUTexture *texture, const char *text)
{
    // No-op.
}

static XrResult WEBGPU_DestroyXRSwapchain(
    SDL_GPURenderer *driverData,
    XrSwapchain swapchain,
    SDL_GPUTexture **swapchainImages)
{
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static SDL_GPUTextureFormat *WEBGPU_GetXRSwapchainFormats(
    SDL_GPURenderer *driverData,
    XrSession session,
    int *num_formats)
{
    return NULL;
}

static XrResult WEBGPU_CreateXRSwapchain(
    SDL_GPURenderer *driverData,
    XrSession session,
    const XrSwapchainCreateInfo *oldCreateInfo,
    SDL_GPUTextureFormat format,
    XrSwapchain *swapchain,
    SDL_GPUTexture ***textures)
{
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static XrResult WEBGPU_CreateXRSession(
    SDL_GPURenderer *driverData,
    const XrSessionCreateInfo *createinfo,
    XrSession *session)
{
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}

static bool WEBGPU_PrepareDriver(SDL_VideoDevice *this, SDL_PropertiesID props)
{
    // TODO: This.
    // There used to be code here, but it hadn't been updated in a while.
    // (Quite frankly, I just got annoyed that it clogged the console with debug device creation information since it uses the same functions)
    return true;
}

static SDL_GPUDevice *WEBGPU_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)
{
    WebGPURenderer *renderer;
    SDL_GPUDevice *result;

    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "NOTE: This backend is EXPERIMENTAL. \e[4mDon't be surprised if it breaks, be surprised if it doesn't.\e[0m");
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "An Emscripten web demo is available at https://thestickmahn.gitlab.io/ihatenamingthings/");
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "Please report any issues to the Github repo.");
    SDL_LogInfo(SDL_LOG_CATEGORY_GPU, "- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -");

    Sint64 bindGroupsExpireAfter = SDL_GetNumberProperty(props, SDL_PROP_GPU_DEVICE_CREATE_WEBGPU_BINDGROUP_EXPIRE_AFTER_N_SUBMITS, DEFAULT_BINDGROUP_EXPIRY);

    bool getAdapterSucceeded = false;
    bool getDeviceSucceeded = false;

    renderer = (WebGPURenderer *)SDL_calloc(1, sizeof(*renderer));

    if (renderer == NULL) {
        return NULL;
    }

    renderer->debugMode = debugMode;
    renderer->preferLowPower = preferLowPower;
    renderer->shouldRecreateLostDevice = true;
    renderer->props = SDL_CreateProperties();
    renderer->bindGroupsExpireAfter = bindGroupsExpireAfter;
    renderer->maxFramesInFlight = 2; // Default

    renderer->bindGroupHashTable = SDL_CreateHashTable(512, true, WEBGPU_INTERNAL_HashBindGroupKey, WEBGPU_INTERNAL_MatchHashedBindGroupKey, WEBGPU_INTERNAL_DestroyCachedBindGroupAndKey, NULL);

    renderer->queryingFenceLock = SDL_CreateMutex();
    renderer->destroyingSelfLock = SDL_CreateMutex();
    renderer->registeringQueuedDestroyLock = SDL_CreateMutex();
    renderer->submittingCommandBufferLock = SDL_CreateMutex();
    renderer->creatingWebGPUResourceLock = SDL_CreateMutex();

    renderer->createdByThreadID = SDL_GetCurrentThreadID();

    if (!SDL_CopyProperties(props, renderer->props)) {
        SDL_Log("Failed to copy properties! Oh no!\n%s", SDL_GetError());
    }

// I do not like MSVC.
#ifdef _MSC_VER
    renderer->instance = wgpuCreateInstance(&(WGPUInstanceDescriptor)WGPU_INSTANCE_DESCRIPTOR_INIT);
#else
    renderer->instance = wgpuCreateInstance(&WGPU_INSTANCE_DESCRIPTOR_INIT);
#endif

    if (!renderer->instance) {
        SDL_free(renderer);
        return NULL;
    }

    WEBGPU_INTERNAL_RequestAdapter(renderer, &getAdapterSucceeded);

    if (!renderer->adapter) {
        wgpuInstanceRelease(renderer->instance);

        SDL_free(renderer);
        return NULL;
    }

    WEBGPU_INTERNAL_RequestDevice(renderer, &getDeviceSucceeded);

    if (!renderer->device) {
        wgpuAdapterRelease(renderer->adapter);
        wgpuInstanceRelease(renderer->instance);

        SDL_free(renderer);
        return NULL;
    }

    renderer->queue = wgpuDeviceGetQueue(renderer->device);

    WEBGPU_INTERNAL_InitUniformBuffers(renderer);
    WEBGPU_INTERNAL_InitBlitResources(renderer);

    result = (SDL_GPUDevice *)SDL_calloc(1, sizeof(SDL_GPUDevice));

    result->driverData = (SDL_GPURenderer *)renderer;
    result->shader_formats = SDL_GPU_SHADERFORMAT_WGSL;

    ASSIGN_DRIVER(WEBGPU)
    return result;
}

SDL_GPUBootstrap WebGPUDriver = {
    "webgpu",
    WEBGPU_PrepareDriver,
    WEBGPU_CreateDevice
};

#endif
