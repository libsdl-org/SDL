// NOTE:
// This was developed using the Vulkan backend as a guide.
// Also, to slouken, cosmonaut, or anybody else who's gonna have to read through this code: I'm sorry.
// I've seen ancient Egyptian hieroglyphics that were easier to read than this code.

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

typedef struct WebGPURenderer
{
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUQueue queue;

    bool debugMode;
    bool preferLowPower;
    bool shouldRecreateLostDevice;

} WebGPURenderer;

static void WEBGPU_UncapturedErrorCallback(WGPUDevice const *device, WGPUErrorType type, WGPUStringView message, void *debugMode, void *unused)
{
    if (debugMode) {
        // FIXME: Again, not sure if this is how I should report errors.
        SDL_LogError(SDL_LOG_CATEGORY_GPU, "WebGPU uncaptured error!\n'%s'\n(Type %X)", message.data, type);
    }
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

    // Alright, if we've reached this logic everything went well!
    // Let's free all resources and return a success!
    result = true;
    goto finished;

finished:
    // NOTE: Not entirely sure if this is safe or if it'll crash the program.
    // This is the first time I've ever used C's memory allocation stuff. -- TheStickmahn
    renderer->shouldRecreateLostDevice = false;
    renderer->debugMode = false; // shut up

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

static SDL_GPUDevice *WEBGPU_CreateDevice(bool debugMode, bool preferLowPower, SDL_PropertiesID props)
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

    result = (SDL_GPUDevice *)SDL_calloc(1, sizeof(SDL_GPUDevice));
    // FIXME: Uncomment this when everything been implemented
    //
    // ASSIGN_DRIVER(WEBGPU)
}

SDL_GPUBootstrap WebGPUDriver = {
    "webgpu",
    WEBGPU_PrepareDriver,
    WEBGPU_CreateDevice
};

#endif
