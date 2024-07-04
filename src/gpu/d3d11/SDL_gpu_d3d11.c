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

#if SDL_GPU_D3D11

#define D3D11_NO_HELPERS
#define CINTERFACE
#define COBJMACROS

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <dxgi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#include "../SDL_gpu_driver.h"

/* __stdcall declaration, largely taken from vkd3d_windows.h */
#ifdef _WIN32
#define D3DCOMPILER_API STDMETHODCALLTYPE
#else
#ifdef __stdcall
#undef __stdcall
#endif
#ifdef __x86_64__
#define __stdcall __attribute__((ms_abi))
#else
#if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)) || defined(__APPLE__)
#define __stdcall __attribute__((__stdcall__)) __attribute__((__force_align_arg_pointer__))
#else
#define __stdcall __attribute__((__stdcall__))
#endif
#endif
#define D3DCOMPILER_API __stdcall
#endif

/* vkd3d uses stdcall for its ID3D10Blob implementation */
#ifndef _WIN32
typedef struct VKD3DBlob VKD3DBlob;
typedef struct VKD3DBlobVtbl
{
    HRESULT(__stdcall *QueryInterface)
    (
        VKD3DBlob *This,
        REFIID riid,
        void **ppvObject);
    ULONG(__stdcall *AddRef)
    (VKD3DBlob *This);
    ULONG(__stdcall *Release)
    (VKD3DBlob *This);
    LPVOID(__stdcall *GetBufferPointer)
    (VKD3DBlob *This);
    SIZE_T(__stdcall *GetBufferSize)
    (VKD3DBlob *This);
} VKD3DBlobVtbl;
struct VKD3DBlob
{
    const VKD3DBlobVtbl *lpVtbl;
};
#define ID3D10Blob VKD3DBlob
#define ID3DBlob   VKD3DBlob
#endif

/* MinGW doesn't implement this yet */
#ifdef _WIN32
#define HAVE_IDXGIINFOQUEUE
#endif

/* Function Pointer Signatures */
typedef HRESULT(WINAPI *PFN_CREATE_DXGI_FACTORY1)(const GUID *riid, void **ppFactory);
typedef HRESULT(WINAPI *PFN_DXGI_GET_DEBUG_INTERFACE)(const GUID *riid, void **ppDebug);
typedef HRESULT(D3DCOMPILER_API *PFN_D3DCOMPILE)(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, const D3D_SHADER_MACRO *pDefines, ID3DInclude *pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob **ppCode, ID3DBlob **ppErrorMsgs);

/* IIDs (from https://magnumdb.com) */
static const IID D3D_IID_IDXGIFactory1 = { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };
static const IID D3D_IID_IDXGIFactory4 = { 0x1bc6ea02, 0xef36, 0x464f, { 0xbf, 0x0c, 0x21, 0xca, 0x39, 0xe5, 0x16, 0x8a } };
static const IID D3D_IID_IDXGIFactory5 = { 0x7632e1f5, 0xee65, 0x4dca, { 0x87, 0xfd, 0x84, 0xcd, 0x75, 0xf8, 0x83, 0x8d } };
static const IID D3D_IID_IDXGIFactory6 = { 0xc1b6694f, 0xff09, 0x44a9, { 0xb0, 0x3c, 0x77, 0x90, 0x0a, 0x0a, 0x1d, 0x17 } };
static const IID D3D_IID_IDXGIAdapter1 = { 0x29038f61, 0x3839, 0x4626, { 0x91, 0xfd, 0x08, 0x68, 0x79, 0x01, 0x1a, 0x05 } };
static const IID D3D_IID_IDXGISwapChain3 = { 0x94d99bdb, 0xf1f8, 0x4ab0, { 0xb2, 0x36, 0x7d, 0xa0, 0x17, 0x0e, 0xda, 0xb1 } };
static const IID D3D_IID_ID3D11Texture2D = { 0x6f15aaf2, 0xd208, 0x4e89, { 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c } };
static const IID D3D_IID_ID3DUserDefinedAnnotation = { 0xb2daad8b, 0x03d4, 0x4dbf, { 0x95, 0xeb, 0x32, 0xab, 0x4b, 0x63, 0xd0, 0xab } };
static const IID D3D_IID_ID3D11Device1 = { 0xa04bfb29, 0x08ef, 0x43d6, { 0xa4, 0x9c, 0xa9, 0xbd, 0xbd, 0xcb, 0xe6, 0x86 } };
static const IID D3D_IID_IDXGIDebug = { 0x119e7452, 0xde9e, 0x40fe, { 0x88, 0x06, 0x88, 0xf9, 0x0c, 0x12, 0xb4, 0x41 } };
#ifdef HAVE_IDXGIINFOQUEUE
static const IID D3D_IID_IDXGIInfoQueue = { 0xd67441c7, 0x672a, 0x476f, { 0x9e, 0x82, 0xcd, 0x55, 0xb4, 0x49, 0x49, 0xce } };
#endif

static const GUID D3D_IID_D3DDebugObjectName = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 } };
static const GUID D3D_IID_DXGI_DEBUG_ALL = { 0xe48ae283, 0xda80, 0x490b, { 0x87, 0xe6, 0x43, 0xe9, 0xa9, 0xcf, 0xda, 0x08 } };

/* Defines */

#if defined(_WIN32)
#define D3D11_DLL     "d3d11.dll"
#define DXGI_DLL      "dxgi.dll"
#define DXGIDEBUG_DLL "dxgidebug.dll"
#elif defined(__APPLE__)
#define D3D11_DLL       "libdxvk_d3d11.dylib"
#define DXGI_DLL        "libdxvk_dxgi.dylib"
#define DXGIDEBUG_DLL   "libdxvk_dxgidebug.dylib"
#define D3DCOMPILER_DLL "libvkd3d-utils.1.dylib"
#else
#define D3D11_DLL       "libdxvk_d3d11.so"
#define DXGI_DLL        "libdxvk_dxgi.so"
#define DXGIDEBUG_DLL   "libdxvk_dxgidebug.so"
#define D3DCOMPILER_DLL "libvkd3d-utils.so.1"
#endif

#define D3D11_CREATE_DEVICE_FUNC      "D3D11CreateDevice"
#define CREATE_DXGI_FACTORY1_FUNC     "CreateDXGIFactory1"
#define D3DCOMPILE_FUNC               "D3DCompile"
#define DXGI_GET_DEBUG_INTERFACE_FUNC "DXGIGetDebugInterface"
#define WINDOW_PROPERTY_DATA          "SDL_GpuD3D11WindowPropertyData"

#define SDL_GPU_SHADERSTAGE_COMPUTE 2

#ifdef _WIN32
#define HRESULT_FMT "(0x%08lX)"
#else
#define HRESULT_FMT "(0x%08X)"
#endif

/* Built-in shaders, compiled with compile_shaders.bat */

#define g_main D3D11_BlitFrom2D
#include "D3D11_BlitFrom2D.h"
#undef g_main

#define g_main D3D11_BlitFrom2DArray
#include "D3D11_BlitFrom2DArray.h"
#undef g_main

#define g_main D3D11_FullscreenVert
#include "D3D11_FullscreenVert.h"
#undef g_main

/* Macros */

#define ERROR_CHECK(msg)                                     \
    if (FAILED(res)) {                                       \
        D3D11_INTERNAL_LogError(renderer->device, msg, res); \
    }

#define ERROR_CHECK_RETURN(msg, ret)                         \
    if (FAILED(res)) {                                       \
        D3D11_INTERNAL_LogError(renderer->device, msg, res); \
        return ret;                                          \
    }

#define EXPAND_ARRAY_IF_NEEDED(arr, elementType, newCount, capacity, newCapacity) \
    if (newCount >= capacity) {                                                   \
        capacity = newCapacity;                                                   \
        arr = (elementType *)SDL_realloc(                                         \
            arr,                                                                  \
            sizeof(elementType) * capacity);                                      \
    }

#define TRACK_RESOURCE(resource, type, array, count, capacity) \
    Uint32 i;                                                  \
                                                               \
    for (i = 0; i < commandBuffer->count; i += 1) {            \
        if (commandBuffer->array[i] == resource) {             \
            return;                                            \
        }                                                      \
    }                                                          \
                                                               \
    if (commandBuffer->count == commandBuffer->capacity) {     \
        commandBuffer->capacity += 1;                          \
        commandBuffer->array = SDL_realloc(                    \
            commandBuffer->array,                              \
            commandBuffer->capacity * sizeof(type));           \
    }                                                          \
    commandBuffer->array[commandBuffer->count] = resource;     \
    commandBuffer->count += 1;                                 \
    SDL_AtomicIncRef(&resource->referenceCount);

/* Forward Declarations */

static void D3D11_Wait(SDL_GpuRenderer *driverData);
static void D3D11_UnclaimWindow(
    SDL_GpuRenderer *driverData,
    SDL_Window *window);
static void D3D11_INTERNAL_DestroyBlitPipelines(SDL_GpuRenderer *driverData);

/* Conversions */

static DXGI_FORMAT SwapchainCompositionToTextureFormat[] = {
    DXGI_FORMAT_B8G8R8A8_UNORM,                /* SDR */
    DXGI_FORMAT_B8G8R8A8_UNORM, /* SDR_SRGB */ /* NOTE: The RTV uses the sRGB format */
    DXGI_FORMAT_R16G16B16A16_FLOAT,            /* HDR */
    DXGI_FORMAT_R10G10B10A2_UNORM,             /* HDR_ADVANCED*/
};

static DXGI_COLOR_SPACE_TYPE SwapchainCompositionToColorSpace[] = {
    DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,   /* SDR */
    DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,   /* SDR_SRGB */
    DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709,   /* HDR */
    DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 /* HDR_ADVANCED */
};

static DXGI_FORMAT SDLToD3D11_TextureFormat[] = {
    DXGI_FORMAT_R8G8B8A8_UNORM,                /* R8G8B8A8 */
    DXGI_FORMAT_B8G8R8A8_UNORM,                /* B8G8R8A8 */
    DXGI_FORMAT_B5G6R5_UNORM,                  /* B5G6R5 */
    DXGI_FORMAT_B5G5R5A1_UNORM,                /* B5G5R5A1 */
    DXGI_FORMAT_B4G4R4A4_UNORM,                /* B4G4R4A4 */
    DXGI_FORMAT_R10G10B10A2_UNORM,             /* R10G10B10A2 */
    DXGI_FORMAT_R16G16_UNORM,                  /* R16G16 */
    DXGI_FORMAT_R16G16B16A16_UNORM,            /* R16G16B16A16 */
    DXGI_FORMAT_R8_UNORM,                      /* R8 */
    DXGI_FORMAT_A8_UNORM,                      /* A8 */
    DXGI_FORMAT_BC1_UNORM,                     /* BC1 */
    DXGI_FORMAT_BC2_UNORM,                     /* BC2 */
    DXGI_FORMAT_BC3_UNORM,                     /* BC3 */
    DXGI_FORMAT_BC7_UNORM,                     /* BC7 */
    DXGI_FORMAT_R8G8_SNORM,                    /* R8G8_SNORM */
    DXGI_FORMAT_R8G8B8A8_SNORM,                /* R8G8B8A8_SNORM */
    DXGI_FORMAT_R16_FLOAT,                     /* R16_SFLOAT */
    DXGI_FORMAT_R16G16_FLOAT,                  /* R16G16_SFLOAT */
    DXGI_FORMAT_R16G16B16A16_FLOAT,            /* R16G16B16A16_SFLOAT */
    DXGI_FORMAT_R32_FLOAT,                     /* R32_SFLOAT */
    DXGI_FORMAT_R32G32_FLOAT,                  /* R32G32_SFLOAT */
    DXGI_FORMAT_R32G32B32A32_FLOAT,            /* R32G32B32A32_SFLOAT */
    DXGI_FORMAT_R8_UINT,                       /* R8_UINT */
    DXGI_FORMAT_R8G8_UINT,                     /* R8G8_UINT */
    DXGI_FORMAT_R8G8B8A8_UINT,                 /* R8G8B8A8_UINT */
    DXGI_FORMAT_R16_UINT,                      /* R16_UINT */
    DXGI_FORMAT_R16G16_UINT,                   /* R16G16_UINT */
    DXGI_FORMAT_R16G16B16A16_UINT,             /* R16G16B16A16_UINT */
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,           /* R8G8B8A8_SRGB */
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,           /* B8G8R8A8_SRGB */
    DXGI_FORMAT_BC3_UNORM_SRGB,                /* BC3_SRGB */
    DXGI_FORMAT_BC7_UNORM_SRGB,                /* BC7_SRGB */
    DXGI_FORMAT_D16_UNORM,                     /* D16_UNORM */
    DXGI_FORMAT_D24_UNORM_S8_UINT,             /* D24_UNORM */
    DXGI_FORMAT_D32_FLOAT,                     /* D32_SFLOAT */
    DXGI_FORMAT_D24_UNORM_S8_UINT,             /* D24_UNORM_S8_UINT */
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT,          /* D32_SFLOAT_S8_UINT */
};

static DXGI_FORMAT SDLToD3D11_VertexFormat[] = {
    DXGI_FORMAT_R32_UINT,           /* UINT */
    DXGI_FORMAT_R32_FLOAT,          /* FLOAT */
    DXGI_FORMAT_R32G32_FLOAT,       /* VECTOR2 */
    DXGI_FORMAT_R32G32B32_FLOAT,    /* VECTOR3 */
    DXGI_FORMAT_R32G32B32A32_FLOAT, /* VECTOR4 */
    DXGI_FORMAT_R8G8B8A8_UNORM,     /* COLOR */
    DXGI_FORMAT_R8G8B8A8_UINT,      /* BYTE4 */
    DXGI_FORMAT_R16G16_SINT,        /* SHORT2 */
    DXGI_FORMAT_R16G16B16A16_SINT,  /* SHORT4 */
    DXGI_FORMAT_R16G16_SNORM,       /* NORMALIZEDSHORT2 */
    DXGI_FORMAT_R16G16B16A16_SNORM, /* NORMALIZEDSHORT4 */
    DXGI_FORMAT_R16G16_FLOAT,       /* HALFVECTOR2 */
    DXGI_FORMAT_R16G16B16A16_FLOAT  /* HALFVECTOR4 */
};

static Uint32 SDLToD3D11_SampleCount[] = {
    1, /* SDL_GPU_SAMPLECOUNT_1 */
    2, /* SDL_GPU_SAMPLECOUNT_2 */
    4, /* SDL_GPU_SAMPLECOUNT_4 */
    8  /* SDL_GPU_SAMPLECOUNT_8 */
};

static DXGI_FORMAT SDLToD3D11_IndexType[] = {
    DXGI_FORMAT_R16_UINT, /* 16BIT */
    DXGI_FORMAT_R32_UINT  /* 32BIT */
};

static D3D11_PRIMITIVE_TOPOLOGY SDLToD3D11_PrimitiveType[] = {
    D3D_PRIMITIVE_TOPOLOGY_POINTLIST,    /* POINTLIST */
    D3D_PRIMITIVE_TOPOLOGY_LINELIST,     /* LINELIST */
    D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,    /* LINESTRIP */
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST, /* TRIANGLELIST */
    D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP /* TRIANGLESTRIP */
};

static D3D11_CULL_MODE SDLToD3D11_CullMode[] = {
    D3D11_CULL_NONE,  /* NONE */
    D3D11_CULL_FRONT, /* FRONT */
    D3D11_CULL_BACK   /* BACK */
};

static D3D11_BLEND SDLToD3D11_BlendFactor[] = {
    D3D11_BLEND_ZERO,             /* ZERO */
    D3D11_BLEND_ONE,              /* ONE */
    D3D11_BLEND_SRC_COLOR,        /* SRC_COLOR */
    D3D11_BLEND_INV_SRC_COLOR,    /* ONE_MINUS_SRC_COLOR */
    D3D11_BLEND_DEST_COLOR,       /* DST_COLOR */
    D3D11_BLEND_INV_DEST_COLOR,   /* ONE_MINUS_DST_COLOR */
    D3D11_BLEND_SRC_ALPHA,        /* SRC_ALPHA */
    D3D11_BLEND_INV_SRC_ALPHA,    /* ONE_MINUS_SRC_ALPHA */
    D3D11_BLEND_DEST_ALPHA,       /* DST_ALPHA */
    D3D11_BLEND_INV_DEST_ALPHA,   /* ONE_MINUS_DST_ALPHA */
    D3D11_BLEND_BLEND_FACTOR,     /* CONSTANT_COLOR */
    D3D11_BLEND_INV_BLEND_FACTOR, /* ONE_MINUS_CONSTANT_COLOR */
    D3D11_BLEND_SRC_ALPHA_SAT,    /* SRC_ALPHA_SATURATE */
};

static D3D11_BLEND SDLToD3D11_BlendFactorAlpha[] = {
    D3D11_BLEND_ZERO,             /* ZERO */
    D3D11_BLEND_ONE,              /* ONE */
    D3D11_BLEND_SRC_ALPHA,        /* SRC_COLOR */
    D3D11_BLEND_INV_SRC_ALPHA,    /* ONE_MINUS_SRC_COLOR */
    D3D11_BLEND_DEST_ALPHA,       /* DST_COLOR */
    D3D11_BLEND_INV_DEST_ALPHA,   /* ONE_MINUS_DST_COLOR */
    D3D11_BLEND_SRC_ALPHA,        /* SRC_ALPHA */
    D3D11_BLEND_INV_SRC_ALPHA,    /* ONE_MINUS_SRC_ALPHA */
    D3D11_BLEND_DEST_ALPHA,       /* DST_ALPHA */
    D3D11_BLEND_INV_DEST_ALPHA,   /* ONE_MINUS_DST_ALPHA */
    D3D11_BLEND_BLEND_FACTOR,     /* CONSTANT_COLOR */
    D3D11_BLEND_INV_BLEND_FACTOR, /* ONE_MINUS_CONSTANT_COLOR */
    D3D11_BLEND_SRC_ALPHA_SAT,    /* SRC_ALPHA_SATURATE */
};

static D3D11_BLEND_OP SDLToD3D11_BlendOp[] = {
    D3D11_BLEND_OP_ADD,          /* ADD */
    D3D11_BLEND_OP_SUBTRACT,     /* SUBTRACT */
    D3D11_BLEND_OP_REV_SUBTRACT, /* REVERSE_SUBTRACT */
    D3D11_BLEND_OP_MIN,          /* MIN */
    D3D11_BLEND_OP_MAX           /* MAX */
};

static D3D11_COMPARISON_FUNC SDLToD3D11_CompareOp[] = {
    D3D11_COMPARISON_NEVER,         /* NEVER */
    D3D11_COMPARISON_LESS,          /* LESS */
    D3D11_COMPARISON_EQUAL,         /* EQUAL */
    D3D11_COMPARISON_LESS_EQUAL,    /* LESS_OR_EQUAL */
    D3D11_COMPARISON_GREATER,       /* GREATER */
    D3D11_COMPARISON_NOT_EQUAL,     /* NOT_EQUAL */
    D3D11_COMPARISON_GREATER_EQUAL, /* GREATER_OR_EQUAL */
    D3D11_COMPARISON_ALWAYS         /* ALWAYS */
};

static D3D11_STENCIL_OP SDLToD3D11_StencilOp[] = {
    D3D11_STENCIL_OP_KEEP,     /* KEEP */
    D3D11_STENCIL_OP_ZERO,     /* ZERO */
    D3D11_STENCIL_OP_REPLACE,  /* REPLACE */
    D3D11_STENCIL_OP_INCR_SAT, /* INCREMENT_AND_CLAMP */
    D3D11_STENCIL_OP_DECR_SAT, /* DECREMENT_AND_CLAMP */
    D3D11_STENCIL_OP_INVERT,   /* INVERT */
    D3D11_STENCIL_OP_INCR,     /* INCREMENT_AND_WRAP */
    D3D11_STENCIL_OP_DECR      /* DECREMENT_AND_WRAP */
};

static D3D11_INPUT_CLASSIFICATION SDLToD3D11_VertexInputRate[] = {
    D3D11_INPUT_PER_VERTEX_DATA,  /* VERTEX */
    D3D11_INPUT_PER_INSTANCE_DATA /* INSTANCE */
};

static D3D11_TEXTURE_ADDRESS_MODE SDLToD3D11_SamplerAddressMode[] = {
    D3D11_TEXTURE_ADDRESS_WRAP,   /* REPEAT */
    D3D11_TEXTURE_ADDRESS_MIRROR, /* MIRRORED_REPEAT */
    D3D11_TEXTURE_ADDRESS_CLAMP   /* CLAMP_TO_EDGE */
};

static D3D11_FILTER SDLToD3D11_Filter(SDL_GpuSamplerCreateInfo *createInfo)
{
    if (createInfo->minFilter == SDL_GPU_FILTER_LINEAR) {
        if (createInfo->magFilter == SDL_GPU_FILTER_LINEAR) {
            if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR) {
                return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            } else {
                return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
            }
        } else {
            if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR) {
                return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
            } else {
                return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
            }
        }
    } else {
        if (createInfo->magFilter == SDL_GPU_FILTER_LINEAR) {
            if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR) {
                return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
            } else {
                return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            }
        } else {
            if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR) {
                return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            } else {
                return D3D11_FILTER_MIN_MAG_MIP_POINT;
            }
        }
    }
}

/* Structs */

typedef struct D3D11Texture D3D11Texture;

typedef struct D3D11TextureSubresource
{
    D3D11Texture *parent;
    Uint32 layer;
    Uint32 level;
    Uint32 index;

    ID3D11RenderTargetView *colorTargetView;        /* NULL if not a color target */
    ID3D11DepthStencilView *depthStencilTargetView; /* NULL if not a depth stencil target */
    ID3D11ShaderResourceView *srv;                  /* NULL if not a storage texture */
    ID3D11UnorderedAccessView *uav;                 /* NULL if not a storage texture */
    ID3D11Resource *msaaHandle;                     /* NULL if not using MSAA */
    ID3D11RenderTargetView *msaaTargetView;         /* NULL if not an MSAA color target */

    SDL_AtomicInt referenceCount;
} D3D11TextureSubresource;

struct D3D11Texture
{
    /* D3D Handles */
    ID3D11Resource *handle; /* ID3D11Texture2D* or ID3D11Texture3D* */
    ID3D11ShaderResourceView *shaderView;

    D3D11TextureSubresource *subresources;
    Uint32 subresourceCount; /* layerCount * levelCount */

    /* Basic Info */
    SDL_GpuTextureFormat format;
    Uint32 width;
    Uint32 height;
    Uint32 depth;
    Uint32 levelCount;
    Uint32 layerCount;
    Uint8 isCube;
    Uint8 isRenderTarget;
};

typedef struct D3D11TextureContainer
{
    SDL_GpuTextureCreateInfo createInfo;
    D3D11Texture *activeTexture;
    Uint8 canBeCycled;

    Uint32 textureCapacity;
    Uint32 textureCount;
    D3D11Texture **textures;

    char *debugName;
} D3D11TextureContainer;

typedef struct D3D11Fence
{
    ID3D11Query *handle;
    SDL_AtomicInt referenceCount;
} D3D11Fence;

typedef struct D3D11WindowData
{
    SDL_Window *window;
    IDXGISwapChain *swapchain;
    D3D11Texture texture;
    D3D11TextureContainer textureContainer;
    SDL_GpuPresentMode presentMode;
    SDL_GpuSwapchainComposition swapchainComposition;
    DXGI_FORMAT swapchainFormat;
    DXGI_COLOR_SPACE_TYPE swapchainColorSpace;
    D3D11Fence *inFlightFences[MAX_FRAMES_IN_FLIGHT];
    Uint32 frameCounter;
} D3D11WindowData;

typedef struct D3D11Shader
{
    ID3D11DeviceChild *handle; /* ID3D11VertexShader, ID3D11PixelShader, ID3D11ComputeShader */
    void *bytecode;
    size_t bytecodeSize;

    Uint32 samplerCount;
    Uint32 uniformBufferCount;
    Uint32 storageBufferCount;
    Uint32 storageTextureCount;
} D3D11Shader;

typedef struct D3D11GraphicsPipeline
{
    float blendConstants[4];
    Sint32 numColorAttachments;
    DXGI_FORMAT colorAttachmentFormats[MAX_COLOR_TARGET_BINDINGS];
    ID3D11BlendState *colorAttachmentBlendState;

    SDL_GpuMultisampleState multisampleState;

    Uint8 hasDepthStencilAttachment;
    DXGI_FORMAT depthStencilAttachmentFormat;
    ID3D11DepthStencilState *depthStencilState;
    Uint32 stencilRef;

    SDL_GpuPrimitiveType primitiveType;
    ID3D11RasterizerState *rasterizerState;

    ID3D11VertexShader *vertexShader;
    ID3D11PixelShader *fragmentShader;

    ID3D11InputLayout *inputLayout;
    Uint32 *vertexStrides;

    Uint32 vertexSamplerCount;
    Uint32 vertexUniformBufferCount;
    Uint32 vertexStorageBufferCount;
    Uint32 vertexStorageTextureCount;

    Uint32 fragmentSamplerCount;
    Uint32 fragmentUniformBufferCount;
    Uint32 fragmentStorageBufferCount;
    Uint32 fragmentStorageTextureCount;
} D3D11GraphicsPipeline;

typedef struct D3D11ComputePipeline
{
    ID3D11ComputeShader *computeShader;

    Uint32 readOnlyStorageTextureCount;
    Uint32 readWriteStorageTextureCount;
    Uint32 readOnlyStorageBufferCount;
    Uint32 readWriteStorageBufferCount;
    Uint32 uniformBufferCount;
} D3D11ComputePipeline;

typedef struct D3D11Buffer
{
    ID3D11Buffer *handle;
    ID3D11UnorderedAccessView *uav;
    ID3D11ShaderResourceView *srv;
    Uint32 size;
    SDL_AtomicInt referenceCount;
} D3D11Buffer;

typedef struct D3D11BufferContainer
{
    D3D11Buffer *activeBuffer;

    Uint32 bufferCapacity;
    Uint32 bufferCount;
    D3D11Buffer **buffers;

    D3D11_BUFFER_DESC bufferDesc;

    char *debugName;
} D3D11BufferContainer;

typedef struct D3D11BufferDownload
{
    ID3D11Buffer *stagingBuffer;
    Uint32 dstOffset;
    Uint32 size;
} D3D11BufferDownload;

typedef struct D3D11TextureDownload
{
    ID3D11Resource *stagingTexture;
    Uint32 width;
    Uint32 height;
    Uint32 depth;
    Uint32 bufferOffset;
    Uint32 bytesPerRow;
    Uint32 bytesPerDepthSlice;
} D3D11TextureDownload;

typedef struct D3D11TransferBuffer
{
    Uint8 *data;
    Uint32 size;
    SDL_AtomicInt referenceCount;

    D3D11BufferDownload *bufferDownloads;
    Uint32 bufferDownloadCount;
    Uint32 bufferDownloadCapacity;

    D3D11TextureDownload *textureDownloads;
    Uint32 textureDownloadCount;
    Uint32 textureDownloadCapacity;
} D3D11TransferBuffer;

typedef struct D3D11TransferBufferContainer
{
    D3D11TransferBuffer *activeBuffer;

    /* These are all the buffers that have been used by this container.
     * If the resource is bound and then updated with DISCARD, a new resource
     * will be added to this list.
     * These can be reused after they are submitted and command processing is complete.
     */
    Uint32 bufferCapacity;
    Uint32 bufferCount;
    D3D11TransferBuffer **buffers;
} D3D11TransferBufferContainer;

typedef struct D3D11UniformBuffer
{
    ID3D11Buffer *buffer;
    void *mappedData;

    Uint32 drawOffset;
    Uint32 writeOffset;
    Uint32 currentBlockSize;
} D3D11UniformBuffer;

typedef struct D3D11Renderer D3D11Renderer;

typedef struct D3D11CommandBuffer
{
    CommandBufferCommonHeader common;
    D3D11Renderer *renderer;

    /* Deferred Context */
    ID3D11DeviceContext1 *context;

    /* Presentation */
    D3D11WindowData **windowDatas;
    Uint32 windowDataCount;
    Uint32 windowDataCapacity;

    /* Render Pass */
    D3D11GraphicsPipeline *graphicsPipeline;

    /* Render Pass MSAA resolve */
    D3D11Texture *colorTargetResolveTexture[MAX_COLOR_TARGET_BINDINGS];
    Uint32 colorTargetResolveSubresourceIndex[MAX_COLOR_TARGET_BINDINGS];
    ID3D11Resource *colorTargetMsaaHandle[MAX_COLOR_TARGET_BINDINGS];

    /* Compute Pass */
    D3D11ComputePipeline *computePipeline;

    /* Debug Annotation */
    ID3DUserDefinedAnnotation *annotation;

    /* Resource slot state */

    SDL_bool needVertexSamplerBind;
    SDL_bool needVertexResourceBind;
    SDL_bool needVertexUniformBufferBind;

    SDL_bool needFragmentSamplerBind;
    SDL_bool needFragmentResourceBind;
    SDL_bool needFragmentUniformBufferBind;

    SDL_bool needComputeUAVBind;
    SDL_bool needComputeSRVBind;
    SDL_bool needComputeUniformBufferBind;

    ID3D11SamplerState *vertexSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    ID3D11ShaderResourceView *vertexShaderResourceViews[MAX_TEXTURE_SAMPLERS_PER_STAGE +
                                                        MAX_STORAGE_BUFFERS_PER_STAGE +
                                                        MAX_STORAGE_TEXTURES_PER_STAGE];

    ID3D11SamplerState *fragmentSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];
    ID3D11ShaderResourceView *fragmentShaderResourceViews[MAX_TEXTURE_SAMPLERS_PER_STAGE +
                                                          MAX_STORAGE_BUFFERS_PER_STAGE +
                                                          MAX_STORAGE_TEXTURES_PER_STAGE];

    ID3D11ShaderResourceView *computeShaderResourceViews[MAX_STORAGE_TEXTURES_PER_STAGE +
                                                         MAX_STORAGE_BUFFERS_PER_STAGE];
    ID3D11UnorderedAccessView *computeUnorderedAccessViews[MAX_STORAGE_TEXTURES_PER_STAGE +
                                                           MAX_STORAGE_BUFFERS_PER_STAGE];

    /* Uniform buffers */
    D3D11UniformBuffer *vertexUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    D3D11UniformBuffer *fragmentUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];
    D3D11UniformBuffer *computeUniformBuffers[MAX_UNIFORM_BUFFERS_PER_STAGE];

    /* Fences */
    D3D11Fence *fence;
    Uint8 autoReleaseFence;

    /* Reference Counting */
    D3D11Buffer **usedBuffers;
    Uint32 usedBufferCount;
    Uint32 usedBufferCapacity;

    D3D11TransferBuffer **usedTransferBuffers;
    Uint32 usedTransferBufferCount;
    Uint32 usedTransferBufferCapacity;

    D3D11TextureSubresource **usedTextureSubresources;
    Uint32 usedTextureSubresourceCount;
    Uint32 usedTextureSubresourceCapacity;

    D3D11UniformBuffer **usedUniformBuffers;
    Uint32 usedUniformBufferCount;
    Uint32 usedUniformBufferCapacity;
} D3D11CommandBuffer;

typedef struct D3D11Sampler
{
    ID3D11SamplerState *handle;
} D3D11Sampler;

struct D3D11Renderer
{
    ID3D11Device1 *device;
    ID3D11DeviceContext *immediateContext;
    IDXGIFactory1 *factory;
    IDXGIAdapter1 *adapter;
    IDXGIDebug *dxgiDebug;
#ifdef HAVE_IDXGIINFOQUEUE
    IDXGIInfoQueue *dxgiInfoQueue;
#endif

    void *d3d11_dll;
    void *dxgi_dll;
    void *dxgidebug_dll;

    void *d3dcompiler_dll;
    PFN_D3DCOMPILE D3DCompile_func;

    Uint8 debugMode;
    BOOL supportsTearing;
    Uint8 supportsFlipDiscard;

    SDL_iconv_t iconv;

    /* Blit */
    SDL_GpuGraphicsPipeline *blitFrom2DPipeline;
    SDL_GpuGraphicsPipeline *blitFrom2DArrayPipeline; /* also cube */
    SDL_GpuSampler *blitNearestSampler;
    SDL_GpuSampler *blitLinearSampler;

    /* Resource Tracking */
    D3D11WindowData **claimedWindows;
    Uint32 claimedWindowCount;
    Uint32 claimedWindowCapacity;

    D3D11CommandBuffer **availableCommandBuffers;
    Uint32 availableCommandBufferCount;
    Uint32 availableCommandBufferCapacity;

    D3D11CommandBuffer **submittedCommandBuffers;
    Uint32 submittedCommandBufferCount;
    Uint32 submittedCommandBufferCapacity;

    D3D11Fence **availableFences;
    Uint32 availableFenceCount;
    Uint32 availableFenceCapacity;

    D3D11UniformBuffer **uniformBufferPool;
    Uint32 uniformBufferPoolCount;
    Uint32 uniformBufferPoolCapacity;

    D3D11TransferBufferContainer **transferBufferContainersToDestroy;
    Uint32 transferBufferContainersToDestroyCount;
    Uint32 transferBufferContainersToDestroyCapacity;

    D3D11BufferContainer **bufferContainersToDestroy;
    Uint32 bufferContainersToDestroyCount;
    Uint32 bufferContainersToDestroyCapacity;

    D3D11TextureContainer **textureContainersToDestroy;
    Uint32 textureContainersToDestroyCount;
    Uint32 textureContainersToDestroyCapacity;

    SDL_Mutex *contextLock;
    SDL_Mutex *acquireCommandBufferLock;
    SDL_Mutex *acquireUniformBufferLock;
    SDL_Mutex *fenceLock;
    SDL_Mutex *windowLock;
};

/* Null arrays for resetting shader resource slots */

ID3D11RenderTargetView *nullRTVs[MAX_COLOR_TARGET_BINDINGS];

ID3D11ShaderResourceView *nullSRVs[MAX_TEXTURE_SAMPLERS_PER_STAGE +
                                   MAX_STORAGE_TEXTURES_PER_STAGE +
                                   MAX_STORAGE_BUFFERS_PER_STAGE];

ID3D11SamplerState *nullSamplers[MAX_TEXTURE_SAMPLERS_PER_STAGE];

ID3D11UnorderedAccessView *nullUAVs[MAX_STORAGE_TEXTURES_PER_STAGE +
                                    MAX_STORAGE_BUFFERS_PER_STAGE];

/* Logging */

static void D3D11_INTERNAL_LogError(
    ID3D11Device1 *device,
    const char *msg,
    HRESULT res)
{
#define MAX_ERROR_LEN 1024 /* FIXME: Arbitrary! */

    /* Buffer for text, ensure space for \0 terminator after buffer */
    char wszMsgBuff[MAX_ERROR_LEN + 1];
    DWORD dwChars; /* Number of chars returned. */

    if (res == DXGI_ERROR_DEVICE_REMOVED) {
        res = ID3D11Device_GetDeviceRemovedReason(device);
    }

    /* Try to get the message from the system errors. */
#ifdef _WIN32
    dwChars = FormatMessage(
        FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        res,
        0,
        wszMsgBuff,
        MAX_ERROR_LEN,
        NULL);
#else
    /* FIXME: Do we have error strings in dxvk-native? -flibit */
    dwChars = 0;
#endif

    /* No message? Screw it, just post the code. */
    if (dwChars == 0) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s! Error Code: " HRESULT_FMT, msg, res);
        return;
    }

    /* Ensure valid range */
    dwChars = SDL_min(dwChars, MAX_ERROR_LEN);

    /* Trim whitespace from tail of message */
    while (dwChars > 0) {
        if (wszMsgBuff[dwChars - 1] <= ' ') {
            dwChars--;
        } else {
            break;
        }
    }

    /* Ensure null-terminated string */
    wszMsgBuff[dwChars] = '\0';

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s! Error Code: %s " HRESULT_FMT, msg, wszMsgBuff, res);
}

/* Helper Functions */

static inline Uint32 D3D11_INTERNAL_CalcSubresource(
    Uint32 mipLevel,
    Uint32 arraySlice,
    Uint32 numLevels)
{
    return mipLevel + (arraySlice * numLevels);
}

static inline Uint32 D3D11_INTERNAL_NextHighestAlignment(
    Uint32 n,
    Uint32 align)
{
    return align * ((n + align - 1) / align);
}

static DXGI_FORMAT D3D11_INTERNAL_GetTypelessFormat(
    DXGI_FORMAT typedFormat)
{
    switch (typedFormat) {
    case DXGI_FORMAT_D16_UNORM:
        return DXGI_FORMAT_R16_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT:
        return DXGI_FORMAT_R32_TYPELESS;
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        return DXGI_FORMAT_R24G8_TYPELESS;
    case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        return DXGI_FORMAT_R32G8X24_TYPELESS;
    default:
        return DXGI_FORMAT_UNKNOWN;
    }
}

static DXGI_FORMAT D3D11_INTERNAL_GetSampleableFormat(
    DXGI_FORMAT format)
{
    switch (format) {
    case DXGI_FORMAT_R16_TYPELESS:
        return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R32_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R24G8_TYPELESS:
        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case DXGI_FORMAT_R32G8X24_TYPELESS:
        return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    default:
        return format;
    }
}

/* Quit */

static void D3D11_INTERNAL_DestroyBufferContainer(
    D3D11BufferContainer *container)
{
    for (Uint32 i = 0; i < container->bufferCount; i += 1) {
        D3D11Buffer *d3d11Buffer = container->buffers[i];

        if (d3d11Buffer->uav != NULL) {
            ID3D11UnorderedAccessView_Release(d3d11Buffer->uav);
        }

        if (d3d11Buffer->srv != NULL) {
            ID3D11ShaderResourceView_Release(d3d11Buffer->srv);
        }

        ID3D11Buffer_Release(d3d11Buffer->handle);

        SDL_free(d3d11Buffer);
    }

    SDL_free(container->buffers);
    SDL_free(container);
}

static void D3D11_DestroyDevice(
    SDL_GpuDevice *device)
{
    D3D11Renderer *renderer = (D3D11Renderer *)device->driverData;

    /* Flush any remaining GPU work... */
    D3D11_Wait(device->driverData);

    /* Release the window data */
    for (Sint32 i = renderer->claimedWindowCount - 1; i >= 0; i -= 1) {
        D3D11_UnclaimWindow(device->driverData, renderer->claimedWindows[i]->window);
    }
    SDL_free(renderer->claimedWindows);

    /* Release the blit resources */
    D3D11_INTERNAL_DestroyBlitPipelines(device->driverData);

    /* Release UBOs */
    for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        ID3D11Buffer_Release(renderer->uniformBufferPool[i]->buffer);
        SDL_free(renderer->uniformBufferPool[i]);
    }
    SDL_free(renderer->uniformBufferPool);

    /* Release command buffer infrastructure */
    for (Uint32 i = 0; i < renderer->availableCommandBufferCount; i += 1) {
        D3D11CommandBuffer *commandBuffer = renderer->availableCommandBuffers[i];
        if (commandBuffer->annotation) {
            ID3DUserDefinedAnnotation_Release(commandBuffer->annotation);
        }
        ID3D11DeviceContext_Release(commandBuffer->context);
        SDL_free(commandBuffer->usedBuffers);
        SDL_free(commandBuffer->usedTransferBuffers);
        SDL_free(commandBuffer);
    }
    SDL_free(renderer->availableCommandBuffers);
    SDL_free(renderer->submittedCommandBuffers);

    /* Release fence infrastructure */
    for (Uint32 i = 0; i < renderer->availableFenceCount; i += 1) {
        D3D11Fence *fence = renderer->availableFences[i];
        ID3D11Query_Release(fence->handle);
        SDL_free(fence);
    }
    SDL_free(renderer->availableFences);

    /* Release the iconv, if applicable */
    if (renderer->iconv != NULL) {
        SDL_iconv_close(renderer->iconv);
    }

    /* Release the mutexes */
    SDL_DestroyMutex(renderer->acquireCommandBufferLock);
    SDL_DestroyMutex(renderer->acquireUniformBufferLock);
    SDL_DestroyMutex(renderer->contextLock);
    SDL_DestroyMutex(renderer->fenceLock);
    SDL_DestroyMutex(renderer->windowLock);

    /* Release the device and associated objects */
    ID3D11DeviceContext_Release(renderer->immediateContext);
    ID3D11Device_Release(renderer->device);
    IDXGIAdapter_Release(renderer->adapter);
    IDXGIFactory_Release(renderer->factory);

    /* Report leaks and clean up debug objects */
    if (renderer->dxgiDebug) {
        IDXGIDebug_ReportLiveObjects(
            renderer->dxgiDebug,
            D3D_IID_DXGI_DEBUG_ALL,
            DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL);
        IDXGIDebug_Release(renderer->dxgiDebug);
    }

#ifdef HAVE_IDXGIINFOQUEUE
    if (renderer->dxgiInfoQueue) {
        IDXGIInfoQueue_Release(renderer->dxgiInfoQueue);
    }
#endif

    /* Release the DLLs */
    SDL_UnloadObject(renderer->d3d11_dll);
    SDL_UnloadObject(renderer->dxgi_dll);
    SDL_UnloadObject(renderer->d3dcompiler_dll);
    if (renderer->dxgidebug_dll) {
        SDL_UnloadObject(renderer->dxgidebug_dll);
    }

    /* Free the primary structures */
    SDL_free(renderer);
    SDL_free(device);
}

/* Resource tracking */

static void D3D11_INTERNAL_TrackBuffer(
    D3D11CommandBuffer *commandBuffer,
    D3D11Buffer *buffer)
{
    TRACK_RESOURCE(
        buffer,
        D3D11Buffer *,
        usedBuffers,
        usedBufferCount,
        usedBufferCapacity);
}

static void D3D11_INTERNAL_TrackTransferBuffer(
    D3D11CommandBuffer *commandBuffer,
    D3D11TransferBuffer *buffer)
{
    TRACK_RESOURCE(
        buffer,
        D3D11TransferBuffer *,
        usedTransferBuffers,
        usedTransferBufferCount,
        usedTransferBufferCapacity);
}

static void D3D11_INTERNAL_TrackTextureSubresource(
    D3D11CommandBuffer *commandBuffer,
    D3D11TextureSubresource *textureSubresource)
{
    TRACK_RESOURCE(
        textureSubresource,
        D3D11TextureSubresource *,
        usedTextureSubresources,
        usedTextureSubresourceCount,
        usedTextureSubresourceCapacity);
}

static void D3D11_INTERNAL_TrackUniformBuffer(
    D3D11CommandBuffer *commandBuffer,
    D3D11UniformBuffer *uniformBuffer)
{
    Uint32 i;
    for (i = 0; i < commandBuffer->usedUniformBufferCount; i += 1) {
        if (commandBuffer->usedUniformBuffers[i] == uniformBuffer) {
            return;
        }
    }

    if (commandBuffer->usedUniformBufferCount == commandBuffer->usedUniformBufferCapacity) {
        commandBuffer->usedUniformBufferCapacity += 1;
        commandBuffer->usedUniformBuffers = SDL_realloc(
            commandBuffer->usedUniformBuffers,
            commandBuffer->usedUniformBufferCapacity * sizeof(D3D11UniformBuffer *));
    }

    commandBuffer->usedUniformBuffers[commandBuffer->usedUniformBufferCount] = uniformBuffer;
    commandBuffer->usedUniformBufferCount += 1;
}

/* Disposal */

static void D3D11_INTERNAL_DestroyTexture(D3D11Texture *d3d11Texture)
{
    if (d3d11Texture->shaderView) {
        ID3D11ShaderResourceView_Release(d3d11Texture->shaderView);
    }

    for (Uint32 subresourceIndex = 0; subresourceIndex < d3d11Texture->subresourceCount; subresourceIndex += 1) {
        if (d3d11Texture->subresources[subresourceIndex].msaaHandle != NULL) {
            ID3D11Resource_Release(d3d11Texture->subresources[subresourceIndex].msaaHandle);
        }

        if (d3d11Texture->subresources[subresourceIndex].msaaTargetView != NULL) {
            ID3D11RenderTargetView_Release(d3d11Texture->subresources[subresourceIndex].msaaTargetView);
        }

        if (d3d11Texture->subresources[subresourceIndex].colorTargetView != NULL) {
            ID3D11RenderTargetView_Release(d3d11Texture->subresources[subresourceIndex].colorTargetView);
        }

        if (d3d11Texture->subresources[subresourceIndex].depthStencilTargetView != NULL) {
            ID3D11DepthStencilView_Release(d3d11Texture->subresources[subresourceIndex].depthStencilTargetView);
        }

        if (d3d11Texture->subresources[subresourceIndex].srv != NULL) {
            ID3D11ShaderResourceView_Release(d3d11Texture->subresources[subresourceIndex].srv);
        }

        if (d3d11Texture->subresources[subresourceIndex].uav != NULL) {
            ID3D11UnorderedAccessView_Release(d3d11Texture->subresources[subresourceIndex].uav);
        }
    }
    SDL_free(d3d11Texture->subresources);

    ID3D11Resource_Release(d3d11Texture->handle);
}

static void D3D11_INTERNAL_DestroyTextureContainer(
    D3D11TextureContainer *container)
{
    for (Uint32 i = 0; i < container->textureCount; i += 1) {
        D3D11_INTERNAL_DestroyTexture(container->textures[i]);
    }

    SDL_free(container->textures);
    SDL_free(container);
}

static void D3D11_ReleaseTexture(
    SDL_GpuRenderer *driverData,
    SDL_GpuTexture *texture)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11TextureContainer *container = (D3D11TextureContainer *)texture;

    SDL_LockMutex(renderer->contextLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->textureContainersToDestroy,
        D3D11TextureContainer *,
        renderer->textureContainersToDestroyCount + 1,
        renderer->textureContainersToDestroyCapacity,
        renderer->textureContainersToDestroyCapacity + 1);

    renderer->textureContainersToDestroy[renderer->textureContainersToDestroyCount] = container;
    renderer->textureContainersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_ReleaseSampler(
    SDL_GpuRenderer *driverData,
    SDL_GpuSampler *sampler)
{
    (void)driverData; /* used by other backends */
    D3D11Sampler *d3d11Sampler = (D3D11Sampler *)sampler;
    ID3D11SamplerState_Release(d3d11Sampler->handle);
    SDL_free(d3d11Sampler);
}

static void D3D11_ReleaseBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuBuffer *buffer)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11BufferContainer *container = (D3D11BufferContainer *)buffer;

    SDL_LockMutex(renderer->contextLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->bufferContainersToDestroy,
        D3D11BufferContainer *,
        renderer->bufferContainersToDestroyCount + 1,
        renderer->bufferContainersToDestroyCapacity,
        renderer->bufferContainersToDestroyCapacity + 1);

    renderer->bufferContainersToDestroy[renderer->bufferContainersToDestroyCount] = container;
    renderer->bufferContainersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_ReleaseTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBuffer *transferBuffer)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;

    SDL_LockMutex(renderer->contextLock);

    EXPAND_ARRAY_IF_NEEDED(
        renderer->transferBufferContainersToDestroy,
        D3D11TransferBufferContainer *,
        renderer->transferBufferContainersToDestroyCount + 1,
        renderer->transferBufferContainersToDestroyCapacity,
        renderer->transferBufferContainersToDestroyCapacity + 1);

    renderer->transferBufferContainersToDestroy[renderer->transferBufferContainersToDestroyCount] = (D3D11TransferBufferContainer *)transferBuffer;
    renderer->transferBufferContainersToDestroyCount += 1;

    SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_INTERNAL_DestroyTransferBufferContainer(
    D3D11TransferBufferContainer *transferBufferContainer)
{
    for (Uint32 i = 0; i < transferBufferContainer->bufferCount; i += 1) {
        if (transferBufferContainer->buffers[i]->bufferDownloadCount > 0) {
            SDL_free(transferBufferContainer->buffers[i]->bufferDownloads);
        }
        if (transferBufferContainer->buffers[i]->textureDownloadCount > 0) {
            SDL_free(transferBufferContainer->buffers[i]->textureDownloads);
        }
        SDL_free(transferBufferContainer->buffers[i]->data);
        SDL_free(transferBufferContainer->buffers[i]);
    }
    SDL_free(transferBufferContainer->buffers);
}

static void D3D11_ReleaseShader(
    SDL_GpuRenderer *driverData,
    SDL_GpuShader *shader)
{
    (void)driverData; /* used by other backends */
    D3D11Shader *d3dShader = (D3D11Shader *)shader;
    ID3D11DeviceChild_Release(d3dShader->handle);
    if (d3dShader->bytecode) {
        SDL_free(d3dShader->bytecode);
    }
    SDL_free(d3dShader);
}

static void D3D11_ReleaseComputePipeline(
    SDL_GpuRenderer *driverData,
    SDL_GpuComputePipeline *computePipeline)
{
    D3D11ComputePipeline *d3d11ComputePipeline = (D3D11ComputePipeline *)computePipeline;

    ID3D11ComputeShader_Release(d3d11ComputePipeline->computeShader);

    SDL_free(d3d11ComputePipeline);
}

static void D3D11_ReleaseGraphicsPipeline(
    SDL_GpuRenderer *driverData,
    SDL_GpuGraphicsPipeline *graphicsPipeline)
{
    (void)driverData; /* used by other backends */
    D3D11GraphicsPipeline *d3d11GraphicsPipeline = (D3D11GraphicsPipeline *)graphicsPipeline;

    ID3D11BlendState_Release(d3d11GraphicsPipeline->colorAttachmentBlendState);
    ID3D11DepthStencilState_Release(d3d11GraphicsPipeline->depthStencilState);
    ID3D11RasterizerState_Release(d3d11GraphicsPipeline->rasterizerState);

    if (d3d11GraphicsPipeline->inputLayout) {
        ID3D11InputLayout_Release(d3d11GraphicsPipeline->inputLayout);
    }
    if (d3d11GraphicsPipeline->vertexStrides) {
        SDL_free(d3d11GraphicsPipeline->vertexStrides);
    }

    ID3D11VertexShader_Release(d3d11GraphicsPipeline->vertexShader);
    ID3D11PixelShader_Release(d3d11GraphicsPipeline->fragmentShader);

    SDL_free(d3d11GraphicsPipeline);
}

/* State Creation */

static ID3D11BlendState *D3D11_INTERNAL_FetchBlendState(
    D3D11Renderer *renderer,
    Uint32 numColorAttachments,
    SDL_GpuColorAttachmentDescription *colorAttachments)
{
    ID3D11BlendState *result;
    D3D11_BLEND_DESC blendDesc;
    HRESULT res;

    /* Create a new blend state.
     * The spec says the driver will not create duplicate states, so there's no need to cache.
     */
    SDL_zero(blendDesc); /* needed for any unused RT entries */

    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = TRUE;

    for (Uint32 i = 0; i < numColorAttachments; i += 1) {
        blendDesc.RenderTarget[i].BlendEnable = colorAttachments[i].blendState.blendEnable;
        blendDesc.RenderTarget[i].BlendOp = SDLToD3D11_BlendOp[colorAttachments[i].blendState.colorBlendOp];
        blendDesc.RenderTarget[i].BlendOpAlpha = SDLToD3D11_BlendOp[colorAttachments[i].blendState.alphaBlendOp];
        blendDesc.RenderTarget[i].DestBlend = SDLToD3D11_BlendFactor[colorAttachments[i].blendState.dstColorBlendFactor];
        blendDesc.RenderTarget[i].DestBlendAlpha = SDLToD3D11_BlendFactorAlpha[colorAttachments[i].blendState.dstAlphaBlendFactor];
        blendDesc.RenderTarget[i].RenderTargetWriteMask = colorAttachments[i].blendState.colorWriteMask;
        blendDesc.RenderTarget[i].SrcBlend = SDLToD3D11_BlendFactor[colorAttachments[i].blendState.srcColorBlendFactor];
        blendDesc.RenderTarget[i].SrcBlendAlpha = SDLToD3D11_BlendFactorAlpha[colorAttachments[i].blendState.srcAlphaBlendFactor];
    }

    res = ID3D11Device_CreateBlendState(
        renderer->device,
        &blendDesc,
        &result);
    ERROR_CHECK_RETURN("Could not create blend state", NULL);

    return result;
}

static ID3D11DepthStencilState *D3D11_INTERNAL_FetchDepthStencilState(
    D3D11Renderer *renderer,
    SDL_GpuDepthStencilState depthStencilState)
{
    ID3D11DepthStencilState *result;
    D3D11_DEPTH_STENCIL_DESC dsDesc;
    HRESULT res;

    /* Create a new depth-stencil state.
     * The spec says the driver will not create duplicate states, so there's no need to cache.
     */
    dsDesc.DepthEnable = depthStencilState.depthTestEnable;
    dsDesc.StencilEnable = depthStencilState.stencilTestEnable;
    dsDesc.DepthFunc = SDLToD3D11_CompareOp[depthStencilState.compareOp];
    dsDesc.DepthWriteMask = (depthStencilState.depthWriteEnable ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO);

    dsDesc.BackFace.StencilFunc = SDLToD3D11_CompareOp[depthStencilState.backStencilState.compareOp];
    dsDesc.BackFace.StencilDepthFailOp = SDLToD3D11_StencilOp[depthStencilState.backStencilState.depthFailOp];
    dsDesc.BackFace.StencilFailOp = SDLToD3D11_StencilOp[depthStencilState.backStencilState.failOp];
    dsDesc.BackFace.StencilPassOp = SDLToD3D11_StencilOp[depthStencilState.backStencilState.passOp];

    dsDesc.FrontFace.StencilFunc = SDLToD3D11_CompareOp[depthStencilState.frontStencilState.compareOp];
    dsDesc.FrontFace.StencilDepthFailOp = SDLToD3D11_StencilOp[depthStencilState.frontStencilState.depthFailOp];
    dsDesc.FrontFace.StencilFailOp = SDLToD3D11_StencilOp[depthStencilState.frontStencilState.failOp];
    dsDesc.FrontFace.StencilPassOp = SDLToD3D11_StencilOp[depthStencilState.frontStencilState.passOp];

    dsDesc.StencilReadMask = depthStencilState.compareMask;
    dsDesc.StencilWriteMask = depthStencilState.writeMask;

    res = ID3D11Device_CreateDepthStencilState(
        renderer->device,
        &dsDesc,
        &result);
    ERROR_CHECK_RETURN("Could not create depth-stencil state", NULL);

    return result;
}

static ID3D11RasterizerState *D3D11_INTERNAL_FetchRasterizerState(
    D3D11Renderer *renderer,
    SDL_GpuRasterizerState rasterizerState)
{
    ID3D11RasterizerState *result;
    D3D11_RASTERIZER_DESC rasterizerDesc;
    HRESULT res;

    /* Create a new rasterizer state.
     * The spec says the driver will not create duplicate states, so there's no need to cache.
     */
    rasterizerDesc.AntialiasedLineEnable = FALSE;
    rasterizerDesc.CullMode = SDLToD3D11_CullMode[rasterizerState.cullMode];
    rasterizerDesc.DepthBias = SDL_lroundf(rasterizerState.depthBiasConstantFactor);
    rasterizerDesc.DepthBiasClamp = rasterizerState.depthBiasClamp;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.FillMode = (rasterizerState.fillMode == SDL_GPU_FILLMODE_FILL) ? D3D11_FILL_SOLID : D3D11_FILL_WIREFRAME;
    rasterizerDesc.FrontCounterClockwise = (rasterizerState.frontFace == SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE);
    rasterizerDesc.MultisampleEnable = TRUE; /* only applies to MSAA render targets */
    rasterizerDesc.ScissorEnable = TRUE;
    rasterizerDesc.SlopeScaledDepthBias = rasterizerState.depthBiasSlopeFactor;

    res = ID3D11Device_CreateRasterizerState(
        renderer->device,
        &rasterizerDesc,
        &result);
    ERROR_CHECK_RETURN("Could not create rasterizer state", NULL);

    return result;
}

static Uint32 D3D11_INTERNAL_FindIndexOfVertexBinding(
    Uint32 targetBinding,
    const SDL_GpuVertexBinding *bindings,
    Uint32 numBindings)
{
    for (Uint32 i = 0; i < numBindings; i += 1) {
        if (bindings[i].binding == targetBinding) {
            return i;
        }
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find vertex binding %u!", targetBinding);
    return 0;
}

static ID3D11InputLayout *D3D11_INTERNAL_FetchInputLayout(
    D3D11Renderer *renderer,
    SDL_GpuVertexInputState inputState,
    void *shaderBytes,
    size_t shaderByteLength)
{
    ID3D11InputLayout *result = NULL;
    D3D11_INPUT_ELEMENT_DESC *elementDescs;
    Uint32 bindingIndex;
    HRESULT res;

    /* Don't bother creating/fetching an input layout if there are no attributes. */
    if (inputState.vertexAttributeCount == 0) {
        return NULL;
    }

    /* Allocate an array of vertex elements */
    elementDescs = SDL_stack_alloc(
        D3D11_INPUT_ELEMENT_DESC,
        inputState.vertexAttributeCount);

    /* Create the array of input elements */
    for (Uint32 i = 0; i < inputState.vertexAttributeCount; i += 1) {
        elementDescs[i].AlignedByteOffset = inputState.vertexAttributes[i].offset;
        elementDescs[i].Format = SDLToD3D11_VertexFormat[inputState.vertexAttributes[i].format];
        elementDescs[i].InputSlot = inputState.vertexAttributes[i].binding;

        bindingIndex = D3D11_INTERNAL_FindIndexOfVertexBinding(
            elementDescs[i].InputSlot,
            inputState.vertexBindings,
            inputState.vertexBindingCount);
        elementDescs[i].InputSlotClass = SDLToD3D11_VertexInputRate[inputState.vertexBindings[bindingIndex].inputRate];
        /* The spec requires this to be 0 for per-vertex data */
        elementDescs[i].InstanceDataStepRate = (inputState.vertexBindings[bindingIndex].inputRate == SDL_GPU_VERTEXINPUTRATE_INSTANCE ? inputState.vertexBindings[bindingIndex].stepRate : 0);

        elementDescs[i].SemanticIndex = inputState.vertexAttributes[i].location;
        elementDescs[i].SemanticName = "TEXCOORD";
    }

    res = ID3D11Device_CreateInputLayout(
        renderer->device,
        elementDescs,
        inputState.vertexAttributeCount,
        shaderBytes,
        shaderByteLength,
        &result);
    if (FAILED(res)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create input layout! Error: " HRESULT_FMT, res);
        SDL_stack_free(elementDescs);
        return NULL;
    }

    /* FIXME:
     * These are not cached by the driver! Should we cache them, or allow duplicates?
     * If we have one input layout per graphics pipeline maybe that wouldn't be so bad...?
     */

    SDL_stack_free(elementDescs);
    return result;
}

/* Pipeline Creation */

static ID3D11DeviceChild *D3D11_INTERNAL_CreateID3D11Shader(
    D3D11Renderer *renderer,
    Uint32 stage,
    SDL_GpuShaderFormat format,
    const Uint8 *code,
    size_t codeSize,
    const char *entryPointName,
    void **pBytecode,
    size_t *pBytecodeSize)
{
    const char *profiles[3] = { "vs_5_0", "ps_5_0", "cs_5_0" };
    ID3DBlob *blob = NULL;
    ID3DBlob *errorBlob;
    const Uint8 *bytecode;
    size_t bytecodeSize;
    ID3D11DeviceChild *handle = NULL;
    HRESULT res;

    if (format == SDL_GPU_SHADERFORMAT_HLSL) {
        res = renderer->D3DCompile_func(
            code,
            codeSize,
            NULL,
            NULL,
            NULL,
            "main", /* entry point name ignored */
            profiles[stage],
            0,
            0,
            &blob,
            &errorBlob);
        if (res < 0) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", (const char *)ID3D10Blob_GetBufferPointer(errorBlob));
            ID3D10Blob_Release(errorBlob);
            return NULL;
        }
        bytecode = ID3D10Blob_GetBufferPointer(blob);
        bytecodeSize = ID3D10Blob_GetBufferSize(blob);
    } else if (format == SDL_GPU_SHADERFORMAT_DXBC) {
        bytecode = code;
        bytecodeSize = codeSize;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Incompatible shader format for D3D11");
        return NULL;
    }

    /* Create the shader from the byte blob */
    if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
        res = ID3D11Device_CreateVertexShader(
            renderer->device,
            bytecode,
            bytecodeSize,
            NULL,
            (ID3D11VertexShader **)&handle);
        if (FAILED(res)) {
            D3D11_INTERNAL_LogError(renderer->device, "Could not create vertex shader", res);
            return NULL;
        }
    } else if (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        res = ID3D11Device_CreatePixelShader(
            renderer->device,
            bytecode,
            bytecodeSize,
            NULL,
            (ID3D11PixelShader **)&handle);
        if (FAILED(res)) {
            D3D11_INTERNAL_LogError(renderer->device, "Could not create pixel shader", res);
            return NULL;
        }
    } else if (stage == SDL_GPU_SHADERSTAGE_COMPUTE) {
        res = ID3D11Device_CreateComputeShader(
            renderer->device,
            bytecode,
            bytecodeSize,
            NULL,
            (ID3D11ComputeShader **)&handle);
        if (FAILED(res)) {
            D3D11_INTERNAL_LogError(renderer->device, "Could not create compute shader", res);
            return NULL;
        }
    }

    if (pBytecode != NULL) {
        *pBytecode = SDL_malloc(bytecodeSize);
        SDL_memcpy(*pBytecode, bytecode, bytecodeSize);
        *pBytecodeSize = bytecodeSize;
    }

    /* Clean up */
    if (blob) {
        ID3D10Blob_Release(blob);
    }

    return handle;
}

static SDL_GpuComputePipeline *D3D11_CreateComputePipeline(
    SDL_GpuRenderer *driverData,
    SDL_GpuComputePipelineCreateInfo *pipelineCreateInfo)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    ID3D11ComputeShader *shader;
    D3D11ComputePipeline *pipeline;

    shader = (ID3D11ComputeShader *)D3D11_INTERNAL_CreateID3D11Shader(
        renderer,
        SDL_GPU_SHADERSTAGE_COMPUTE,
        pipelineCreateInfo->format,
        pipelineCreateInfo->code,
        pipelineCreateInfo->codeSize,
        pipelineCreateInfo->entryPointName,
        NULL,
        NULL);
    if (shader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create compute pipeline!");
        return NULL;
    }

    pipeline = SDL_malloc(sizeof(D3D11ComputePipeline));
    pipeline->computeShader = shader;
    pipeline->readOnlyStorageTextureCount = pipelineCreateInfo->readOnlyStorageTextureCount;
    pipeline->readWriteStorageTextureCount = pipelineCreateInfo->readWriteStorageTextureCount;
    pipeline->readOnlyStorageBufferCount = pipelineCreateInfo->readOnlyStorageBufferCount;
    pipeline->readWriteStorageBufferCount = pipelineCreateInfo->readWriteStorageBufferCount;
    pipeline->uniformBufferCount = pipelineCreateInfo->uniformBufferCount;
    /* thread counts are ignored in d3d11 */

    return (SDL_GpuComputePipeline *)pipeline;
}

static SDL_GpuGraphicsPipeline *D3D11_CreateGraphicsPipeline(
    SDL_GpuRenderer *driverData,
    SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11Shader *vertShader = (D3D11Shader *)pipelineCreateInfo->vertexShader;
    D3D11Shader *fragShader = (D3D11Shader *)pipelineCreateInfo->fragmentShader;
    D3D11GraphicsPipeline *pipeline = SDL_malloc(sizeof(D3D11GraphicsPipeline));
    Uint32 i;

    /* Blend */

    pipeline->colorAttachmentBlendState = D3D11_INTERNAL_FetchBlendState(
        renderer,
        pipelineCreateInfo->attachmentInfo.colorAttachmentCount,
        pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions);

    pipeline->numColorAttachments = pipelineCreateInfo->attachmentInfo.colorAttachmentCount;
    for (i = 0; i < pipeline->numColorAttachments; i += 1) {
        pipeline->colorAttachmentFormats[i] = SDLToD3D11_TextureFormat[pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].format];
    }

    pipeline->blendConstants[0] = pipelineCreateInfo->blendConstants[0];
    pipeline->blendConstants[1] = pipelineCreateInfo->blendConstants[1];
    pipeline->blendConstants[2] = pipelineCreateInfo->blendConstants[2];
    pipeline->blendConstants[3] = pipelineCreateInfo->blendConstants[3];

    /* Multisample */

    pipeline->multisampleState = pipelineCreateInfo->multisampleState;

    /* Depth-Stencil */

    pipeline->depthStencilState = D3D11_INTERNAL_FetchDepthStencilState(
        renderer,
        pipelineCreateInfo->depthStencilState);

    pipeline->hasDepthStencilAttachment = pipelineCreateInfo->attachmentInfo.hasDepthStencilAttachment;
    pipeline->depthStencilAttachmentFormat = SDLToD3D11_TextureFormat[pipelineCreateInfo->attachmentInfo.depthStencilFormat];
    pipeline->stencilRef = pipelineCreateInfo->depthStencilState.reference;

    /* Rasterizer */

    pipeline->primitiveType = pipelineCreateInfo->primitiveType;
    pipeline->rasterizerState = D3D11_INTERNAL_FetchRasterizerState(
        renderer,
        pipelineCreateInfo->rasterizerState);

    /* Shaders */

    pipeline->vertexShader = (ID3D11VertexShader *)vertShader->handle;
    ID3D11VertexShader_AddRef(pipeline->vertexShader);

    pipeline->fragmentShader = (ID3D11PixelShader *)fragShader->handle;
    ID3D11PixelShader_AddRef(pipeline->fragmentShader);

    /* Input Layout */

    pipeline->inputLayout = D3D11_INTERNAL_FetchInputLayout(
        renderer,
        pipelineCreateInfo->vertexInputState,
        vertShader->bytecode,
        vertShader->bytecodeSize);

    if (pipelineCreateInfo->vertexInputState.vertexBindingCount > 0) {
        pipeline->vertexStrides = SDL_malloc(
            sizeof(Uint32) *
            pipelineCreateInfo->vertexInputState.vertexBindingCount);

        for (i = 0; i < pipelineCreateInfo->vertexInputState.vertexBindingCount; i += 1) {
            pipeline->vertexStrides[i] = pipelineCreateInfo->vertexInputState.vertexBindings[i].stride;
        }
    } else {
        pipeline->vertexStrides = NULL;
    }

    /* Resource layout */

    pipeline->vertexSamplerCount = vertShader->samplerCount;
    pipeline->vertexStorageTextureCount = vertShader->storageTextureCount;
    pipeline->vertexStorageBufferCount = vertShader->storageBufferCount;
    pipeline->vertexUniformBufferCount = vertShader->uniformBufferCount;

    pipeline->fragmentSamplerCount = fragShader->samplerCount;
    pipeline->fragmentStorageTextureCount = fragShader->storageTextureCount;
    pipeline->fragmentStorageBufferCount = fragShader->storageBufferCount;
    pipeline->fragmentUniformBufferCount = fragShader->uniformBufferCount;

    return (SDL_GpuGraphicsPipeline *)pipeline;
}

/* Debug Naming */

static void D3D11_INTERNAL_SetBufferName(
    D3D11Renderer *renderer,
    D3D11Buffer *buffer,
    const char *text)
{
    if (renderer->debugMode) {
        ID3D11DeviceChild_SetPrivateData(
            buffer->handle,
            &D3D_IID_D3DDebugObjectName,
            (UINT)SDL_strlen(text),
            text);
    }
}

static void D3D11_SetBufferName(
    SDL_GpuRenderer *driverData,
    SDL_GpuBuffer *buffer,
    const char *text)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11BufferContainer *container = (D3D11BufferContainer *)buffer;
    size_t textLength = SDL_strlen(text) + 1;

    if (renderer->debugMode) {
        container->debugName = SDL_realloc(
            container->debugName,
            textLength);

        SDL_utf8strlcpy(
            container->debugName,
            text,
            textLength);

        for (Uint32 i = 0; i < container->bufferCount; i += 1) {
            D3D11_INTERNAL_SetBufferName(
                renderer,
                container->buffers[i],
                text);
        }
    }
}

static void D3D11_INTERNAL_SetTextureName(
    D3D11Renderer *renderer,
    D3D11Texture *texture,
    const char *text)
{
    if (renderer->debugMode) {
        ID3D11DeviceChild_SetPrivateData(
            texture->handle,
            &D3D_IID_D3DDebugObjectName,
            (UINT)SDL_strlen(text),
            text);
    }
}

static void D3D11_SetTextureName(
    SDL_GpuRenderer *driverData,
    SDL_GpuTexture *texture,
    const char *text)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11TextureContainer *container = (D3D11TextureContainer *)texture;
    size_t textLength = SDL_strlen(text) + 1;

    if (renderer->debugMode) {
        container->debugName = SDL_realloc(
            container->debugName,
            textLength);

        SDL_utf8strlcpy(
            container->debugName,
            text,
            textLength);

        for (Uint32 i = 0; i < container->textureCount; i += 1) {
            D3D11_INTERNAL_SetTextureName(
                renderer,
                container->textures[i],
                text);
        }
    }
}

static SDL_bool D3D11_INTERNAL_StrToWStr(
    D3D11Renderer *renderer,
    const char *str,
    wchar_t *wstr,
    size_t wstr_size)
{
    size_t inlen, result;
    size_t outlen = wstr_size;

    if (renderer->iconv == NULL) {
        renderer->iconv = SDL_iconv_open("WCHAR_T", "UTF-8");
        SDL_assert(renderer->iconv);
    }

    /* Convert... */
    inlen = SDL_strlen(str) + 1;
    result = SDL_iconv(
        renderer->iconv,
        &str,
        &inlen,
        (char **)&wstr,
        &outlen);

    /* Check... */
    switch (result) {
    case SDL_ICONV_ERROR:
    case SDL_ICONV_E2BIG:
    case SDL_ICONV_EILSEQ:
    case SDL_ICONV_EINVAL:
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Failed to convert string to wchar_t!");
        return SDL_FALSE;
    default:
        break;
    }

    return SDL_TRUE;
}

static void D3D11_InsertDebugLabel(
    SDL_GpuCommandBuffer *commandBuffer,
    const char *text)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;

    if (d3d11CommandBuffer->annotation == NULL) {
        return;
    }

    wchar_t wstr[256];
    if (!D3D11_INTERNAL_StrToWStr(renderer, text, wstr, sizeof(wstr))) {
        return;
    }

    ID3DUserDefinedAnnotation_SetMarker(d3d11CommandBuffer->annotation, wstr);
}

static void D3D11_PushDebugGroup(
    SDL_GpuCommandBuffer *commandBuffer,
    const char *name)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;

    if (d3d11CommandBuffer->annotation == NULL) {
        return;
    }

    wchar_t wstr[256];
    if (!D3D11_INTERNAL_StrToWStr(renderer, name, wstr, sizeof(wstr))) {
        return;
    }

    ID3DUserDefinedAnnotation_BeginEvent(d3d11CommandBuffer->annotation, wstr);
}

static void D3D11_PopDebugGroup(
    SDL_GpuCommandBuffer *commandBuffer)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    if (d3d11CommandBuffer->annotation == NULL) {
        return;
    }
    ID3DUserDefinedAnnotation_EndEvent(d3d11CommandBuffer->annotation);
}

/* Resource Creation */

static SDL_GpuSampler *D3D11_CreateSampler(
    SDL_GpuRenderer *driverData,
    SDL_GpuSamplerCreateInfo *samplerCreateInfo)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11_SAMPLER_DESC samplerDesc;
    ID3D11SamplerState *samplerStateHandle;
    D3D11Sampler *d3d11Sampler;
    HRESULT res;

    samplerDesc.AddressU = SDLToD3D11_SamplerAddressMode[samplerCreateInfo->addressModeU];
    samplerDesc.AddressV = SDLToD3D11_SamplerAddressMode[samplerCreateInfo->addressModeV];
    samplerDesc.AddressW = SDLToD3D11_SamplerAddressMode[samplerCreateInfo->addressModeW];
    samplerDesc.ComparisonFunc = (samplerCreateInfo->compareEnable ? SDLToD3D11_CompareOp[samplerCreateInfo->compareOp] : SDLToD3D11_CompareOp[SDL_GPU_COMPAREOP_ALWAYS]);
    samplerDesc.MaxAnisotropy = (samplerCreateInfo->anisotropyEnable ? (UINT)samplerCreateInfo->maxAnisotropy : 0);
    samplerDesc.Filter = SDLToD3D11_Filter(samplerCreateInfo);
    samplerDesc.MaxLOD = samplerCreateInfo->maxLod;
    samplerDesc.MinLOD = samplerCreateInfo->minLod;
    samplerDesc.MipLODBias = samplerCreateInfo->mipLodBias;
    SDL_zeroa(samplerDesc.BorderColor); /* arbitrary, unused */

    res = ID3D11Device_CreateSamplerState(
        renderer->device,
        &samplerDesc,
        &samplerStateHandle);
    ERROR_CHECK_RETURN("Could not create sampler state", NULL);

    d3d11Sampler = (D3D11Sampler *)SDL_malloc(sizeof(D3D11Sampler));
    d3d11Sampler->handle = samplerStateHandle;

    return (SDL_GpuSampler *)d3d11Sampler;
}

SDL_GpuShader *D3D11_CreateShader(
    SDL_GpuRenderer *driverData,
    SDL_GpuShaderCreateInfo *shaderCreateInfo)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    ID3D11DeviceChild *handle;
    void *bytecode;
    size_t bytecodeSize;
    D3D11Shader *shader;

    handle = D3D11_INTERNAL_CreateID3D11Shader(
        renderer,
        shaderCreateInfo->stage,
        shaderCreateInfo->format,
        shaderCreateInfo->code,
        shaderCreateInfo->codeSize,
        shaderCreateInfo->entryPointName,
        shaderCreateInfo->stage == SDL_GPU_SHADERSTAGE_VERTEX ? &bytecode : NULL,
        shaderCreateInfo->stage == SDL_GPU_SHADERSTAGE_VERTEX ? &bytecodeSize : NULL);
    if (!handle) {
        return NULL;
    }

    shader = (D3D11Shader *)SDL_calloc(1, sizeof(D3D11Shader));
    shader->handle = handle;
    shader->samplerCount = shaderCreateInfo->samplerCount;
    shader->storageBufferCount = shaderCreateInfo->storageBufferCount;
    shader->storageTextureCount = shaderCreateInfo->storageTextureCount;
    shader->uniformBufferCount = shaderCreateInfo->uniformBufferCount;
    if (shaderCreateInfo->stage == SDL_GPU_SHADERSTAGE_VERTEX) {
        /* Store the raw bytecode and its length for creating InputLayouts */
        shader->bytecode = bytecode;
        shader->bytecodeSize = bytecodeSize;
    }

    return (SDL_GpuShader *)shader;
}

static D3D11Texture *D3D11_INTERNAL_CreateTexture(
    D3D11Renderer *renderer,
    SDL_GpuTextureCreateInfo *textureCreateInfo,
    D3D11_SUBRESOURCE_DATA *initialData)
{
    Uint8 isSampler, isColorTarget, isDepthStencil, isMultisample, isStaging, needSubresourceSRV, needSubresourceUAV;
    DXGI_FORMAT format;
    ID3D11Resource *textureHandle;
    ID3D11ShaderResourceView *srv = NULL;
    D3D11Texture *d3d11Texture;
    HRESULT res;

    isColorTarget = textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT;
    isDepthStencil = textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT;
    isSampler = textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT;
    needSubresourceSRV =
        (textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT) ||
        (textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT);
    needSubresourceUAV =
        (textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT);
    isMultisample = textureCreateInfo->sampleCount > SDL_GPU_SAMPLECOUNT_1;
    isStaging = textureCreateInfo->usageFlags == 0;

    format = SDLToD3D11_TextureFormat[textureCreateInfo->format];
    if (isDepthStencil) {
        format = D3D11_INTERNAL_GetTypelessFormat(format);
    }

    if (textureCreateInfo->depth <= 1) {
        D3D11_TEXTURE2D_DESC desc2D;

        desc2D.BindFlags = 0;
        if (isSampler || needSubresourceSRV) {
            desc2D.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        }
        if (needSubresourceUAV) {
            desc2D.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }
        if (isColorTarget) {
            desc2D.BindFlags |= D3D11_BIND_RENDER_TARGET;
        }
        if (isDepthStencil) {
            desc2D.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
        }

        desc2D.Width = textureCreateInfo->width;
        desc2D.Height = textureCreateInfo->height;
        desc2D.ArraySize = textureCreateInfo->isCube ? 6 : textureCreateInfo->layerCount;
        desc2D.CPUAccessFlags = isStaging ? D3D11_CPU_ACCESS_WRITE : 0;
        desc2D.Format = format;
        desc2D.MipLevels = textureCreateInfo->levelCount;
        desc2D.MiscFlags = textureCreateInfo->isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
        desc2D.SampleDesc.Count = 1;
        desc2D.SampleDesc.Quality = 0;
        desc2D.Usage = isStaging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;

        res = ID3D11Device_CreateTexture2D(
            renderer->device,
            &desc2D,
            initialData,
            (ID3D11Texture2D **)&textureHandle);
        ERROR_CHECK_RETURN("Could not create Texture2D", NULL);

        /* Create the SRV, if applicable */
        if (isSampler) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format = D3D11_INTERNAL_GetSampleableFormat(format);

            if (textureCreateInfo->isCube) {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                srvDesc.TextureCube.MipLevels = desc2D.MipLevels;
                srvDesc.TextureCube.MostDetailedMip = 0;
            } else if (textureCreateInfo->layerCount > 1) {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                srvDesc.Texture2DArray.MipLevels = desc2D.MipLevels;
                srvDesc.Texture2DArray.MostDetailedMip = 0;
                srvDesc.Texture2DArray.FirstArraySlice = 0;
                srvDesc.Texture2DArray.ArraySize = textureCreateInfo->layerCount;
            } else {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MipLevels = desc2D.MipLevels;
                srvDesc.Texture2D.MostDetailedMip = 0;
            }

            res = ID3D11Device_CreateShaderResourceView(
                renderer->device,
                textureHandle,
                &srvDesc,
                &srv);
            if (FAILED(res)) {
                ID3D11Resource_Release(textureHandle);
                D3D11_INTERNAL_LogError(renderer->device, "Could not create SRV for 2D texture", res);
                return NULL;
            }
        }
    } else {
        D3D11_TEXTURE3D_DESC desc3D;

        desc3D.BindFlags = 0;
        if (isSampler || needSubresourceSRV) {
            desc3D.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
        }
        if (needSubresourceUAV) {
            desc3D.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }
        if (isColorTarget) {
            desc3D.BindFlags |= D3D11_BIND_RENDER_TARGET;
        }

        desc3D.Width = textureCreateInfo->width;
        desc3D.Height = textureCreateInfo->height;
        desc3D.Depth = textureCreateInfo->depth;
        desc3D.CPUAccessFlags = isStaging ? D3D11_CPU_ACCESS_WRITE : 0;
        desc3D.Format = format;
        desc3D.MipLevels = textureCreateInfo->levelCount;
        desc3D.MiscFlags = 0;
        desc3D.Usage = isStaging ? D3D11_USAGE_STAGING : D3D11_USAGE_DEFAULT;

        res = ID3D11Device_CreateTexture3D(
            renderer->device,
            &desc3D,
            initialData,
            (ID3D11Texture3D **)&textureHandle);
        ERROR_CHECK_RETURN("Could not create Texture3D", NULL);

        /* Create the SRV, if applicable */
        if (isSampler) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
            srvDesc.Format = format;
            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
            srvDesc.Texture3D.MipLevels = desc3D.MipLevels;
            srvDesc.Texture3D.MostDetailedMip = 0;

            res = ID3D11Device_CreateShaderResourceView(
                renderer->device,
                textureHandle,
                &srvDesc,
                &srv);
            if (FAILED(res)) {
                ID3D11Resource_Release(textureHandle);
                D3D11_INTERNAL_LogError(renderer->device, "Could not create SRV for 3D texture", res);
                return NULL;
            }
        }
    }

    d3d11Texture = (D3D11Texture *)SDL_malloc(sizeof(D3D11Texture));
    d3d11Texture->handle = textureHandle;
    d3d11Texture->format = textureCreateInfo->format;
    d3d11Texture->width = textureCreateInfo->width;
    d3d11Texture->height = textureCreateInfo->height;
    d3d11Texture->depth = textureCreateInfo->depth;
    d3d11Texture->levelCount = textureCreateInfo->levelCount;
    d3d11Texture->layerCount = textureCreateInfo->layerCount;
    d3d11Texture->isCube = textureCreateInfo->isCube;
    d3d11Texture->isRenderTarget = isColorTarget || isDepthStencil;
    d3d11Texture->shaderView = srv;

    d3d11Texture->subresourceCount = d3d11Texture->levelCount * d3d11Texture->layerCount;
    d3d11Texture->subresources = SDL_malloc(
        d3d11Texture->subresourceCount * sizeof(D3D11TextureSubresource));

    for (Uint32 layerIndex = 0; layerIndex < d3d11Texture->layerCount; layerIndex += 1) {
        for (Uint32 levelIndex = 0; levelIndex < d3d11Texture->levelCount; levelIndex += 1) {
            Uint32 subresourceIndex = D3D11_INTERNAL_CalcSubresource(
                levelIndex,
                layerIndex,
                d3d11Texture->levelCount);

            d3d11Texture->subresources[subresourceIndex].parent = d3d11Texture;
            d3d11Texture->subresources[subresourceIndex].layer = layerIndex;
            d3d11Texture->subresources[subresourceIndex].level = levelIndex;
            d3d11Texture->subresources[subresourceIndex].index = subresourceIndex;

            d3d11Texture->subresources[subresourceIndex].colorTargetView = NULL;
            d3d11Texture->subresources[subresourceIndex].depthStencilTargetView = NULL;
            d3d11Texture->subresources[subresourceIndex].srv = NULL;
            d3d11Texture->subresources[subresourceIndex].uav = NULL;
            d3d11Texture->subresources[subresourceIndex].msaaHandle = NULL;
            d3d11Texture->subresources[subresourceIndex].msaaTargetView = NULL;
            SDL_AtomicSet(&d3d11Texture->subresources[subresourceIndex].referenceCount, 0);

            if (isMultisample) {
                D3D11_TEXTURE2D_DESC desc2D;

                if (isColorTarget) {
                    desc2D.BindFlags = D3D11_BIND_RENDER_TARGET;
                } else if (isDepthStencil) {
                    desc2D.BindFlags = D3D11_BIND_DEPTH_STENCIL;
                }

                desc2D.Width = textureCreateInfo->width;
                desc2D.Height = textureCreateInfo->height;
                desc2D.ArraySize = 1;
                desc2D.CPUAccessFlags = 0;
                desc2D.Format = format;
                desc2D.MipLevels = 1;
                desc2D.MiscFlags = 0;
                desc2D.SampleDesc.Count = SDLToD3D11_SampleCount[textureCreateInfo->sampleCount];
                desc2D.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
                desc2D.Usage = D3D11_USAGE_DEFAULT;

                res = ID3D11Device_CreateTexture2D(
                    renderer->device,
                    &desc2D,
                    NULL,
                    (ID3D11Texture2D **)&d3d11Texture->subresources[subresourceIndex].msaaHandle);
                ERROR_CHECK_RETURN("Could not create MSAA texture!", NULL);

                if (!isDepthStencil) {
                    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;

                    rtvDesc.Format = format;
                    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

                    res = ID3D11Device_CreateRenderTargetView(
                        renderer->device,
                        d3d11Texture->subresources[subresourceIndex].msaaHandle,
                        &rtvDesc,
                        &d3d11Texture->subresources[subresourceIndex].msaaTargetView);
                    ERROR_CHECK_RETURN("Could not create MSAA RTV!", NULL);
                }
            }

            if (d3d11Texture->isRenderTarget) {
                if (isDepthStencil) {
                    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;

                    dsvDesc.Format = SDLToD3D11_TextureFormat[d3d11Texture->format];
                    dsvDesc.Flags = 0;

                    if (isMultisample) {
                        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
                    } else {
                        dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
                        dsvDesc.Texture2D.MipSlice = levelIndex;
                    }

                    res = ID3D11Device_CreateDepthStencilView(
                        renderer->device,
                        isMultisample ? d3d11Texture->subresources[subresourceIndex].msaaHandle : d3d11Texture->handle,
                        &dsvDesc,
                        &d3d11Texture->subresources[subresourceIndex].depthStencilTargetView);
                    ERROR_CHECK_RETURN("Could not create DSV!", NULL);
                } else {
                    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;

                    rtvDesc.Format = SDLToD3D11_TextureFormat[d3d11Texture->format];

                    if (d3d11Texture->layerCount > 1) {
                        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
                        rtvDesc.Texture2DArray.MipSlice = levelIndex;
                        rtvDesc.Texture2DArray.FirstArraySlice = layerIndex;
                        rtvDesc.Texture2DArray.ArraySize = 1;
                    } else if (d3d11Texture->depth > 1) {
                        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
                        rtvDesc.Texture3D.MipSlice = levelIndex;
                        rtvDesc.Texture3D.FirstWSlice = 0;
                        rtvDesc.Texture3D.WSize = d3d11Texture->depth;
                    } else {
                        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
                        rtvDesc.Texture2D.MipSlice = levelIndex;
                    }

                    res = ID3D11Device_CreateRenderTargetView(
                        renderer->device,
                        d3d11Texture->handle,
                        &rtvDesc,
                        &d3d11Texture->subresources[subresourceIndex].colorTargetView);
                    ERROR_CHECK_RETURN("Could not create RTV!", NULL);
                }
            }

            if (needSubresourceSRV) {
                D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;

                srvDesc.Format = format;

                if (d3d11Texture->layerCount > 1) {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
                    srvDesc.Texture2DArray.ArraySize = 1;
                    srvDesc.Texture2DArray.FirstArraySlice = layerIndex;
                    srvDesc.Texture2DArray.MipLevels = 1;
                    srvDesc.Texture2DArray.MostDetailedMip = levelIndex;
                } else if (d3d11Texture->depth > 1) {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
                    srvDesc.Texture3D.MipLevels = 1;
                    srvDesc.Texture3D.MostDetailedMip = levelIndex;
                } else {
                    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                    srvDesc.Texture2D.MipLevels = 1;
                    srvDesc.Texture2D.MostDetailedMip = levelIndex;
                }

                res = ID3D11Device_CreateShaderResourceView(
                    renderer->device,
                    d3d11Texture->handle,
                    &srvDesc,
                    &d3d11Texture->subresources[subresourceIndex].srv);
                ERROR_CHECK_RETURN("Could not create SRV!", NULL);
            }

            if (needSubresourceUAV) {
                D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
                uavDesc.Format = format;

                if (d3d11Texture->layerCount > 1) {
                    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
                    uavDesc.Texture2DArray.MipSlice = levelIndex;
                    uavDesc.Texture2DArray.FirstArraySlice = layerIndex;
                    uavDesc.Texture2DArray.ArraySize = 1;
                } else if (d3d11Texture->depth > 1) {
                    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
                    uavDesc.Texture3D.MipSlice = levelIndex;
                    uavDesc.Texture3D.FirstWSlice = 0;
                    uavDesc.Texture3D.WSize = d3d11Texture->layerCount;
                } else {
                    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                    uavDesc.Texture2D.MipSlice = levelIndex;
                }

                res = ID3D11Device_CreateUnorderedAccessView(
                    renderer->device,
                    d3d11Texture->handle,
                    &uavDesc,
                    &d3d11Texture->subresources[subresourceIndex].uav);
                ERROR_CHECK_RETURN("Could not create UAV!", NULL);
            }
        }
    }

    return d3d11Texture;
}

static SDL_GpuSampleCount D3D11_GetBestSampleCount(
    SDL_GpuRenderer *driverData,
    SDL_GpuTextureFormat format,
    SDL_GpuSampleCount desiredSampleCount)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    SDL_GpuSampleCount maxSupported = SDL_GPU_SAMPLECOUNT_8;
    Uint32 levels;
    HRESULT res;

    while (maxSupported > SDL_GPU_SAMPLECOUNT_1) {
        res = ID3D11Device_CheckMultisampleQualityLevels(
            renderer->device,
            SDLToD3D11_TextureFormat[format],
            SDLToD3D11_SampleCount[desiredSampleCount],
            &levels);
        if (SUCCEEDED(res) && levels > 0) {
            break;
        }
        maxSupported -= 1;
    }

    return (SDL_GpuSampleCount)SDL_min(maxSupported, desiredSampleCount);
}

static SDL_GpuTexture *D3D11_CreateTexture(
    SDL_GpuRenderer *driverData,
    SDL_GpuTextureCreateInfo *textureCreateInfo)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11TextureContainer *container;
    D3D11Texture *texture;
    SDL_GpuTextureCreateInfo newTextureCreateInfo = *textureCreateInfo;

    newTextureCreateInfo.sampleCount = D3D11_GetBestSampleCount(
        driverData,
        textureCreateInfo->format,
        textureCreateInfo->sampleCount);

    texture = D3D11_INTERNAL_CreateTexture(
        renderer,
        &newTextureCreateInfo,
        NULL);

    if (texture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create texture!");
        return NULL;
    }

    container = SDL_malloc(sizeof(D3D11TextureContainer));
    container->canBeCycled = 1;
    container->createInfo = *textureCreateInfo;
    container->activeTexture = texture;
    container->textureCapacity = 1;
    container->textureCount = 1;
    container->textures = SDL_malloc(
        container->textureCapacity * sizeof(D3D11Texture *));
    container->textures[0] = texture;
    container->debugName = NULL;

    return (SDL_GpuTexture *)container;
}

static void D3D11_INTERNAL_CycleActiveTexture(
    D3D11Renderer *renderer,
    D3D11TextureContainer *container)
{
    for (Uint32 i = 0; i < container->textureCount; i += 1) {
        Uint32 refCountTotal = 0;
        for (Uint32 j = 0; j < container->textures[i]->subresourceCount; j += 1) {
            refCountTotal += SDL_AtomicGet(&container->textures[i]->subresources[j].referenceCount);
        }

        if (refCountTotal == 0) {
            container->activeTexture = container->textures[i];
            return;
        }
    }

    EXPAND_ARRAY_IF_NEEDED(
        container->textures,
        D3D11Texture *,
        container->textureCount + 1,
        container->textureCapacity,
        container->textureCapacity + 1);

    container->textures[container->textureCount] = D3D11_INTERNAL_CreateTexture(
        renderer,
        &container->createInfo,
        NULL);
    container->textureCount += 1;

    container->activeTexture = container->textures[container->textureCount - 1];

    if (renderer->debugMode && container->debugName != NULL) {
        D3D11_INTERNAL_SetTextureName(
            renderer,
            container->activeTexture,
            container->debugName);
    }
}

static D3D11TextureSubresource *D3D11_INTERNAL_FetchTextureSubresource(
    D3D11Texture *texture,
    Uint32 layer,
    Uint32 level)
{
    Uint32 index = D3D11_INTERNAL_CalcSubresource(level, layer, texture->levelCount);
    return &texture->subresources[index];
}

static D3D11TextureSubresource *D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
    D3D11Renderer *renderer,
    D3D11TextureContainer *container,
    Uint32 layer,
    Uint32 level,
    SDL_bool cycle)
{
    D3D11TextureSubresource *subresource = D3D11_INTERNAL_FetchTextureSubresource(
        container->activeTexture,
        layer,
        level);

    if (
        container->canBeCycled &&
        cycle &&
        SDL_AtomicGet(&subresource->referenceCount) > 0) {
        D3D11_INTERNAL_CycleActiveTexture(
            renderer,
            container);

        subresource = D3D11_INTERNAL_FetchTextureSubresource(
            container->activeTexture,
            layer,
            level);
    }

    return subresource;
}

static D3D11Buffer *D3D11_INTERNAL_CreateBuffer(
    D3D11Renderer *renderer,
    D3D11_BUFFER_DESC *bufferDesc,
    Uint32 sizeInBytes)
{
    ID3D11Buffer *bufferHandle;
    ID3D11UnorderedAccessView *uav = NULL;
    ID3D11ShaderResourceView *srv = NULL;
    D3D11Buffer *d3d11Buffer;
    HRESULT res;

    /* Storage buffers have to be 4-aligned, so might as well align them all */
    sizeInBytes = D3D11_INTERNAL_NextHighestAlignment(sizeInBytes, 4);

    res = ID3D11Device_CreateBuffer(
        renderer->device,
        bufferDesc,
        NULL,
        &bufferHandle);
    ERROR_CHECK_RETURN("Could not create buffer", NULL);

    /* Storage buffer */
    if (bufferDesc->MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS) {
        /* Create a UAV for the buffer */

        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
        uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = sizeInBytes / sizeof(Uint32);
        uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

        res = ID3D11Device_CreateUnorderedAccessView(
            renderer->device,
            (ID3D11Resource *)bufferHandle,
            &uavDesc,
            &uav);
        if (FAILED(res)) {
            ID3D11Buffer_Release(bufferHandle);
            ERROR_CHECK_RETURN("Could not create UAV for buffer!", NULL);
        }

        /* Create a SRV for the buffer */

        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
        srvDesc.Format = DXGI_FORMAT_R32_TYPELESS;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
        srvDesc.BufferEx.FirstElement = 0;
        srvDesc.BufferEx.NumElements = sizeInBytes / sizeof(Uint32);
        srvDesc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;

        res = ID3D11Device_CreateShaderResourceView(
            renderer->device,
            (ID3D11Resource *)bufferHandle,
            &srvDesc,
            &srv);
        if (FAILED(res)) {
            ID3D11Buffer_Release(bufferHandle);
            ERROR_CHECK_RETURN("Could not create SRV for buffer!", NULL);
        }
    }

    d3d11Buffer = SDL_malloc(sizeof(D3D11Buffer));
    d3d11Buffer->handle = bufferHandle;
    d3d11Buffer->size = sizeInBytes;
    d3d11Buffer->uav = uav;
    d3d11Buffer->srv = srv;
    SDL_AtomicSet(&d3d11Buffer->referenceCount, 0);

    return d3d11Buffer;
}

static SDL_GpuBuffer *D3D11_CreateBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuBufferUsageFlags usageFlags,
    Uint32 sizeInBytes)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11BufferContainer *container;
    D3D11Buffer *buffer;
    D3D11_BUFFER_DESC bufferDesc;

    bufferDesc.BindFlags = 0;
    if (usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX_BIT) {
        bufferDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
    }
    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX_BIT) {
        bufferDesc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
    }
    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT_BIT) {
        bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
    }

    if (usageFlags & (SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT |
                      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT |
                      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE_BIT)) {
        bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
    }

    bufferDesc.ByteWidth = sizeInBytes;
    bufferDesc.Usage = D3D11_USAGE_DEFAULT;
    bufferDesc.CPUAccessFlags = 0;
    bufferDesc.StructureByteStride = 0;
    bufferDesc.MiscFlags = 0;

    if (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT_BIT) {
        bufferDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    }
    if (usageFlags & (SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ_BIT |
                      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_READ_BIT |
                      SDL_GPU_BUFFERUSAGE_COMPUTE_STORAGE_WRITE_BIT)) {
        bufferDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    }

    buffer = D3D11_INTERNAL_CreateBuffer(
        renderer,
        &bufferDesc,
        sizeInBytes);

    if (buffer == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create buffer!");
        return NULL;
    }

    container = SDL_malloc(sizeof(D3D11BufferContainer));
    container->activeBuffer = buffer;
    container->bufferCapacity = 1;
    container->bufferCount = 1;
    container->buffers = SDL_malloc(
        container->bufferCapacity * sizeof(D3D11Buffer *));
    container->buffers[0] = container->activeBuffer;
    container->bufferDesc = bufferDesc;
    container->debugName = NULL;

    return (SDL_GpuBuffer *)container;
}

static D3D11UniformBuffer *D3D11_INTERNAL_CreateUniformBuffer(
    D3D11Renderer *renderer,
    Uint32 sizeInBytes)
{
    D3D11UniformBuffer *uniformBuffer;
    ID3D11Buffer *buffer;
    D3D11_BUFFER_DESC bufferDesc;
    HRESULT res;

    bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bufferDesc.ByteWidth = sizeInBytes;
    bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bufferDesc.MiscFlags = 0;
    bufferDesc.StructureByteStride = 0;
    bufferDesc.Usage = D3D11_USAGE_DYNAMIC;

    res = ID3D11Device_CreateBuffer(
        renderer->device,
        &bufferDesc,
        NULL,
        &buffer);
    ERROR_CHECK_RETURN("Could not create uniform buffer", NULL)

    uniformBuffer = SDL_malloc(sizeof(D3D11UniformBuffer));
    uniformBuffer->buffer = buffer;
    uniformBuffer->writeOffset = 0;
    uniformBuffer->drawOffset = 0;
    uniformBuffer->currentBlockSize = 0;

    return uniformBuffer;
}

static void D3D11_INTERNAL_CycleActiveBuffer(
    D3D11Renderer *renderer,
    D3D11BufferContainer *container)
{
    Uint32 size = container->activeBuffer->size;

    for (Uint32 i = 0; i < container->bufferCount; i += 1) {
        if (SDL_AtomicGet(&container->buffers[i]->referenceCount) == 0) {
            container->activeBuffer = container->buffers[i];
            return;
        }
    }

    EXPAND_ARRAY_IF_NEEDED(
        container->buffers,
        D3D11Buffer *,
        container->bufferCount + 1,
        container->bufferCapacity,
        container->bufferCapacity + 1);

    container->buffers[container->bufferCount] = D3D11_INTERNAL_CreateBuffer(
        renderer,
        &container->bufferDesc,
        size);
    container->bufferCount += 1;

    container->activeBuffer = container->buffers[container->bufferCount - 1];

    if (renderer->debugMode && container->debugName != NULL) {
        D3D11_INTERNAL_SetBufferName(
            renderer,
            container->activeBuffer,
            container->debugName);
    }
}

static D3D11Buffer *D3D11_INTERNAL_PrepareBufferForWrite(
    D3D11Renderer *renderer,
    D3D11BufferContainer *container,
    SDL_bool cycle)
{
    if (
        cycle &&
        SDL_AtomicGet(&container->activeBuffer->referenceCount) > 0) {
        D3D11_INTERNAL_CycleActiveBuffer(
            renderer,
            container);
    }

    return container->activeBuffer;
}

static D3D11TransferBuffer *D3D11_INTERNAL_CreateTransferBuffer(
    D3D11Renderer *renderer,
    Uint32 sizeInBytes)
{
    D3D11TransferBuffer *transferBuffer = SDL_malloc(sizeof(D3D11TransferBuffer));

    transferBuffer->data = (Uint8 *)SDL_malloc(sizeInBytes);
    transferBuffer->size = sizeInBytes;
    SDL_AtomicSet(&transferBuffer->referenceCount, 0);

    transferBuffer->bufferDownloads = NULL;
    transferBuffer->bufferDownloadCount = 0;
    transferBuffer->bufferDownloadCapacity = 0;

    transferBuffer->textureDownloads = NULL;
    transferBuffer->textureDownloadCount = 0;
    transferBuffer->textureDownloadCapacity = 0;

    return transferBuffer;
}

/* This actually returns a container handle so we can rotate buffers on Cycle. */
static SDL_GpuTransferBuffer *D3D11_CreateTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBufferUsage usage, /* ignored on D3D11 */
    Uint32 sizeInBytes)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer *)SDL_malloc(sizeof(D3D11TransferBufferContainer));

    container->bufferCapacity = 1;
    container->bufferCount = 1;
    container->buffers = SDL_malloc(
        container->bufferCapacity * sizeof(D3D11TransferBuffer *));

    container->buffers[0] = D3D11_INTERNAL_CreateTransferBuffer(
        renderer,
        sizeInBytes);

    container->activeBuffer = container->buffers[0];

    return (SDL_GpuTransferBuffer *)container;
}

/* TransferBuffer Data */

static void D3D11_INTERNAL_CycleActiveTransferBuffer(
    D3D11Renderer *renderer,
    D3D11TransferBufferContainer *container)
{
    Uint32 size = container->activeBuffer->size;

    for (Uint32 i = 0; i < container->bufferCount; i += 1) {
        if (SDL_AtomicGet(&container->buffers[i]->referenceCount) == 0) {
            container->activeBuffer = container->buffers[i];
            return;
        }
    }

    EXPAND_ARRAY_IF_NEEDED(
        container->buffers,
        D3D11TransferBuffer *,
        container->bufferCount + 1,
        container->bufferCapacity,
        container->bufferCapacity + 1);

    container->buffers[container->bufferCount] = D3D11_INTERNAL_CreateTransferBuffer(
        renderer,
        size);
    container->bufferCount += 1;

    container->activeBuffer = container->buffers[container->bufferCount - 1];
}

static void D3D11_MapTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBuffer *transferBuffer,
    SDL_bool cycle,
    void **ppData)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer *)transferBuffer;
    D3D11TransferBuffer *buffer = container->activeBuffer;

    /* Rotate the transfer buffer if necessary */
    if (
        cycle &&
        SDL_AtomicGet(&container->activeBuffer->referenceCount) > 0) {
        D3D11_INTERNAL_CycleActiveTransferBuffer(
            renderer,
            container);
        buffer = container->activeBuffer;
    }

    *ppData = buffer->data;
}

static void D3D11_UnmapTransferBuffer(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBuffer *transferBuffer)
{
    /* no-op */
    (void)driverData;
    (void)transferBuffer;
}

static void D3D11_SetTransferData(
    SDL_GpuRenderer *driverData,
    const void *source,
    SDL_GpuTransferBufferRegion *destination,
    SDL_bool cycle)
{
    D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer *)destination->transferBuffer;
    void *dataPtr;

    D3D11_MapTransferBuffer(driverData, destination->transferBuffer, cycle, &dataPtr);

    SDL_memcpy(
        ((Uint8 *)container->activeBuffer->data) + destination->offset,
        source,
        destination->size);

    D3D11_UnmapTransferBuffer(driverData, destination->transferBuffer);
}

static void D3D11_GetTransferData(
    SDL_GpuRenderer *driverData,
    SDL_GpuTransferBufferRegion *source,
    void *destination)
{
    (void)driverData;
    D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer *)source->transferBuffer;

    SDL_memcpy(
        destination,
        ((Uint8 *)container->activeBuffer->data) + source->offset,
        source->size);
}

/* Copy Pass */

static void D3D11_BeginCopyPass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    /* no-op */
}

static void D3D11_UploadToTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureTransferInfo *source,
    SDL_GpuTextureRegion *destination,
    SDL_bool cycle)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    D3D11TransferBufferContainer *transferContainer = (D3D11TransferBufferContainer *)source->transferBuffer;
    D3D11TransferBuffer *d3d11TransferBuffer = transferContainer->activeBuffer;
    D3D11TextureContainer *d3d11TextureContainer = (D3D11TextureContainer *)destination->textureSlice.texture;
    Uint32 bufferStride = source->imagePitch;
    Uint32 bufferImageHeight = source->imageHeight;
    Sint32 w = destination->w;
    Sint32 h = destination->h;
    D3D11Texture *stagingTexture;
    SDL_GpuTextureCreateInfo stagingTextureCreateInfo;
    D3D11_SUBRESOURCE_DATA initialData;

    D3D11TextureSubresource *textureSubresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
        renderer,
        d3d11TextureContainer,
        destination->textureSlice.layer,
        destination->textureSlice.mipLevel,
        cycle);

    Sint32 blockSize = Texture_GetBlockSize(textureSubresource->parent->format);
    if (blockSize > 1) {
        w = (w + blockSize - 1) & ~(blockSize - 1);
        h = (h + blockSize - 1) & ~(blockSize - 1);
    }

    if (bufferStride == 0 || bufferImageHeight == 0) {
        bufferStride = BytesPerRow(w, textureSubresource->parent->format);
        bufferImageHeight = h * SDL_GpuTextureFormatTexelBlockSize(textureSubresource->parent->format);
    }

    /* UpdateSubresource1 is completely busted on AMD, it truncates after X bytes.
     * So we get to do this Fun (Tm) workaround where we create a staging texture
     * with initial data before issuing a copy command.
     */

    stagingTextureCreateInfo.width = w;
    stagingTextureCreateInfo.height = h;
    stagingTextureCreateInfo.depth = destination->d;
    stagingTextureCreateInfo.layerCount = 1;
    stagingTextureCreateInfo.levelCount = 1;
    stagingTextureCreateInfo.isCube = 0;
    stagingTextureCreateInfo.usageFlags = 0;
    stagingTextureCreateInfo.sampleCount = SDL_GPU_SAMPLECOUNT_1;
    stagingTextureCreateInfo.format = ((D3D11TextureContainer *)destination->textureSlice.texture)->createInfo.format;

    initialData.pSysMem = d3d11TransferBuffer->data + source->offset;
    initialData.SysMemPitch = bufferStride;
    initialData.SysMemSlicePitch = bufferStride * bufferImageHeight;

    stagingTexture = D3D11_INTERNAL_CreateTexture(
        renderer,
        &stagingTextureCreateInfo,
        &initialData);

    if (stagingTexture == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Staging texture creation failed");
        return;
    }

    ID3D11DeviceContext_CopySubresourceRegion(
        d3d11CommandBuffer->context,
        textureSubresource->parent->handle,
        textureSubresource->index,
        destination->x,
        destination->y,
        destination->z,
        stagingTexture->handle,
        0,
        NULL);

    /* Clean up the staging texture */
    D3D11_INTERNAL_DestroyTexture(stagingTexture);

    D3D11_INTERNAL_TrackTextureSubresource(d3d11CommandBuffer, textureSubresource);
    D3D11_INTERNAL_TrackTransferBuffer(d3d11CommandBuffer, d3d11TransferBuffer);
}

static void D3D11_UploadToBuffer(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTransferBufferLocation *source,
    SDL_GpuBufferRegion *destination,
    SDL_bool cycle)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    D3D11TransferBufferContainer *transferContainer = (D3D11TransferBufferContainer *)source->transferBuffer;
    D3D11TransferBuffer *d3d11TransferBuffer = transferContainer->activeBuffer;
    D3D11BufferContainer *bufferContainer = (D3D11BufferContainer *)destination->buffer;
    D3D11Buffer *d3d11Buffer = D3D11_INTERNAL_PrepareBufferForWrite(
        renderer,
        bufferContainer,
        cycle);
    ID3D11Buffer *stagingBuffer;
    D3D11_BUFFER_DESC stagingBufferDesc;
    D3D11_SUBRESOURCE_DATA stagingBufferData;
    HRESULT res;

    /* Upload to staging buffer immediately */
    stagingBufferDesc.ByteWidth = destination->size;
    stagingBufferDesc.Usage = D3D11_USAGE_STAGING;
    stagingBufferDesc.BindFlags = 0;
    stagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    stagingBufferDesc.MiscFlags = 0;
    stagingBufferDesc.StructureByteStride = 0;

    stagingBufferData.pSysMem = d3d11TransferBuffer->data + source->offset;
    stagingBufferData.SysMemPitch = 0;
    stagingBufferData.SysMemSlicePitch = 0;

    res = ID3D11Device_CreateBuffer(
        renderer->device,
        &stagingBufferDesc,
        &stagingBufferData,
        &stagingBuffer);
    ERROR_CHECK_RETURN("Could not create staging buffer", )

    /* Copy from staging buffer to buffer */
    ID3D11DeviceContext1_CopySubresourceRegion(
        d3d11CommandBuffer->context,
        (ID3D11Resource *)d3d11Buffer->handle,
        0,
        destination->offset,
        0,
        0,
        (ID3D11Resource *)stagingBuffer,
        0,
        NULL);

    ID3D11Buffer_Release(stagingBuffer);

    D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, d3d11Buffer);
    D3D11_INTERNAL_TrackTransferBuffer(d3d11CommandBuffer, d3d11TransferBuffer);
}

static void D3D11_DownloadFromTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureRegion *source,
    SDL_GpuTextureTransferInfo *destination)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = d3d11CommandBuffer->renderer;
    D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer *)destination->transferBuffer;
    D3D11TransferBuffer *d3d11TransferBuffer = container->activeBuffer;
    D3D11TextureContainer *d3d11TextureContainer = (D3D11TextureContainer *)source->textureSlice.texture;
    D3D11_TEXTURE2D_DESC stagingDesc2D;
    D3D11_TEXTURE3D_DESC stagingDesc3D;
    D3D11TextureSubresource *textureSubresource = D3D11_INTERNAL_FetchTextureSubresource(
        d3d11TextureContainer->activeTexture,
        source->textureSlice.layer,
        source->textureSlice.mipLevel);
    D3D11TextureDownload *textureDownload;
    Uint32 bufferStride = destination->imagePitch;
    Uint32 bufferImageHeight = destination->imageHeight;
    Uint32 bytesPerRow, bytesPerDepthSlice;
    D3D11_BOX srcBox = { source->x, source->y, source->z, source->x + source->w, source->y + source->h, 1 };
    HRESULT res;

    if (d3d11TransferBuffer->textureDownloadCount >= d3d11TransferBuffer->textureDownloadCapacity) {
        d3d11TransferBuffer->textureDownloadCapacity += 1;
        d3d11TransferBuffer->textureDownloads = SDL_realloc(
            d3d11TransferBuffer->textureDownloads,
            d3d11TransferBuffer->textureDownloadCapacity * sizeof(D3D11TextureDownload));
    }

    textureDownload = &d3d11TransferBuffer->textureDownloads[d3d11TransferBuffer->textureDownloadCount];
    d3d11TransferBuffer->textureDownloadCount += 1;

    if (bufferStride == 0 || bufferImageHeight == 0) {
        bufferStride = source->w;
        bufferImageHeight = source->h;
    }

    bytesPerRow = BytesPerRow(bufferStride, textureSubresource->parent->format);
    bytesPerDepthSlice = bytesPerRow * bufferImageHeight;

    if (source->d == 1) {
        stagingDesc2D.Width = source->w;
        stagingDesc2D.Height = source->h;
        stagingDesc2D.MipLevels = 1;
        stagingDesc2D.ArraySize = 1;
        stagingDesc2D.Format = SDLToD3D11_TextureFormat[textureSubresource->parent->format];
        stagingDesc2D.SampleDesc.Count = 1;
        stagingDesc2D.SampleDesc.Quality = 0;
        stagingDesc2D.Usage = D3D11_USAGE_STAGING;
        stagingDesc2D.BindFlags = 0;
        stagingDesc2D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc2D.MiscFlags = 0;

        res = ID3D11Device_CreateTexture2D(
            renderer->device,
            &stagingDesc2D,
            NULL,
            (ID3D11Texture2D **)&textureDownload->stagingTexture);
        ERROR_CHECK_RETURN("Staging texture creation failed", )
    } else {
        stagingDesc3D.Width = source->w;
        stagingDesc3D.Height = source->h;
        stagingDesc3D.Depth = source->d;
        stagingDesc3D.MipLevels = 1;
        stagingDesc3D.Format = SDLToD3D11_TextureFormat[textureSubresource->parent->format];
        stagingDesc3D.Usage = D3D11_USAGE_STAGING;
        stagingDesc3D.BindFlags = 0;
        stagingDesc3D.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        stagingDesc3D.MiscFlags = 0;

        res = ID3D11Device_CreateTexture3D(
            renderer->device,
            &stagingDesc3D,
            NULL,
            (ID3D11Texture3D **)&textureDownload->stagingTexture);
    }

    textureDownload->width = source->w;
    textureDownload->height = source->h;
    textureDownload->depth = source->d;
    textureDownload->bufferOffset = destination->offset;
    textureDownload->bytesPerRow = bytesPerRow;
    textureDownload->bytesPerDepthSlice = bytesPerDepthSlice;

    ID3D11DeviceContext1_CopySubresourceRegion1(
        d3d11CommandBuffer->context,
        textureDownload->stagingTexture,
        0,
        0,
        0,
        0,
        textureSubresource->parent->handle,
        textureSubresource->index,
        &srcBox,
        D3D11_COPY_NO_OVERWRITE);

    D3D11_INTERNAL_TrackTextureSubresource(d3d11CommandBuffer, textureSubresource);
    D3D11_INTERNAL_TrackTransferBuffer(d3d11CommandBuffer, d3d11TransferBuffer);
}

static void D3D11_DownloadFromBuffer(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBufferRegion *source,
    SDL_GpuTransferBufferLocation *destination)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = d3d11CommandBuffer->renderer;
    D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer *)destination->transferBuffer;
    D3D11TransferBuffer *d3d11TransferBuffer = container->activeBuffer;
    D3D11BufferContainer *d3d11BufferContainer = (D3D11BufferContainer *)source->buffer;
    D3D11BufferDownload *bufferDownload;
    D3D11_BOX srcBox = { source->offset, 0, 0, source->size, 1, 1 };
    D3D11_BUFFER_DESC stagingBufferDesc;
    HRESULT res;

    if (d3d11TransferBuffer->bufferDownloadCount >= d3d11TransferBuffer->bufferDownloadCapacity) {
        d3d11TransferBuffer->bufferDownloadCapacity += 1;
        d3d11TransferBuffer->bufferDownloads = SDL_realloc(
            d3d11TransferBuffer->bufferDownloads,
            d3d11TransferBuffer->bufferDownloadCapacity * sizeof(D3D11BufferDownload));
    }

    bufferDownload = &d3d11TransferBuffer->bufferDownloads[d3d11TransferBuffer->bufferDownloadCount];
    d3d11TransferBuffer->bufferDownloadCount += 1;

    stagingBufferDesc.ByteWidth = source->size;
    stagingBufferDesc.Usage = D3D11_USAGE_STAGING;
    stagingBufferDesc.BindFlags = 0;
    stagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingBufferDesc.MiscFlags = 0;
    stagingBufferDesc.StructureByteStride = 0;

    res = ID3D11Device_CreateBuffer(
        renderer->device,
        &stagingBufferDesc,
        NULL,
        &bufferDownload->stagingBuffer);
    ERROR_CHECK_RETURN("Could not create staging buffer", )

    ID3D11DeviceContext1_CopySubresourceRegion1(
        d3d11CommandBuffer->context,
        (ID3D11Resource *)bufferDownload->stagingBuffer,
        0,
        0,
        0,
        0,
        (ID3D11Resource *)d3d11BufferContainer->activeBuffer->handle,
        0,
        &srcBox,
        D3D11_COPY_NO_OVERWRITE);

    bufferDownload->dstOffset = destination->offset;
    bufferDownload->size = source->size;

    D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, d3d11BufferContainer->activeBuffer);
    D3D11_INTERNAL_TrackTransferBuffer(d3d11CommandBuffer, d3d11TransferBuffer);
}

static void D3D11_CopyTextureToTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureLocation *source,
    SDL_GpuTextureLocation *destination,
    Uint32 w,
    Uint32 h,
    Uint32 d,
    SDL_bool cycle)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    D3D11TextureContainer *srcContainer = (D3D11TextureContainer *)source->textureSlice.texture;
    D3D11TextureContainer *dstContainer = (D3D11TextureContainer *)destination->textureSlice.texture;

    D3D11_BOX srcBox = { source->x, source->y, source->z, source->x + w, source->y + h, 1 };

    D3D11TextureSubresource *srcSubresource = D3D11_INTERNAL_FetchTextureSubresource(
        srcContainer->activeTexture,
        source->textureSlice.layer,
        source->textureSlice.mipLevel);

    D3D11TextureSubresource *dstSubresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
        renderer,
        dstContainer,
        destination->textureSlice.layer,
        destination->textureSlice.mipLevel,
        cycle);

    ID3D11DeviceContext1_CopySubresourceRegion(
        d3d11CommandBuffer->context,
        dstSubresource->parent->handle,
        dstSubresource->index,
        destination->x,
        destination->y,
        destination->z,
        srcSubresource->parent->handle,
        srcSubresource->index,
        &srcBox);

    D3D11_INTERNAL_TrackTextureSubresource(d3d11CommandBuffer, srcSubresource);
    D3D11_INTERNAL_TrackTextureSubresource(d3d11CommandBuffer, dstSubresource);
}

static void D3D11_CopyBufferToBuffer(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBufferLocation *source,
    SDL_GpuBufferLocation *destination,
    Uint32 size,
    SDL_bool cycle)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    D3D11BufferContainer *srcBufferContainer = (D3D11BufferContainer *)source->buffer;
    D3D11BufferContainer *dstBufferContainer = (D3D11BufferContainer *)destination->buffer;
    D3D11_BOX srcBox = { source->offset, 0, 0, source->offset + size, 1, 1 };

    D3D11Buffer *srcBuffer = srcBufferContainer->activeBuffer;
    D3D11Buffer *dstBuffer = D3D11_INTERNAL_PrepareBufferForWrite(
        renderer,
        dstBufferContainer,
        cycle);

    ID3D11DeviceContext1_CopySubresourceRegion(
        d3d11CommandBuffer->context,
        (ID3D11Resource *)dstBuffer->handle,
        0,
        destination->offset,
        0,
        0,
        (ID3D11Resource *)srcBuffer->handle,
        0,
        &srcBox);

    D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, srcBuffer);
    D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, dstBuffer);
}

static void D3D11_GenerateMipmaps(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTexture *texture)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11TextureContainer *d3d11TextureContainer = (D3D11TextureContainer *)texture;

    ID3D11DeviceContext1_GenerateMips(
        d3d11CommandBuffer->context,
        d3d11TextureContainer->activeTexture->shaderView);

    for (Uint32 i = 0; i < d3d11TextureContainer->activeTexture->subresourceCount; i += 1) {
        D3D11_INTERNAL_TrackTextureSubresource(
            d3d11CommandBuffer,
            &d3d11TextureContainer->activeTexture->subresources[i]);
    }
}

static void D3D11_EndCopyPass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    /* no-op */
}

/* Graphics State */

static void D3D11_INTERNAL_AllocateCommandBuffers(
    D3D11Renderer *renderer,
    Uint32 allocateCount)
{
    D3D11CommandBuffer *commandBuffer;
    HRESULT res;

    renderer->availableCommandBufferCapacity += allocateCount;

    renderer->availableCommandBuffers = SDL_realloc(
        renderer->availableCommandBuffers,
        sizeof(D3D11CommandBuffer *) * renderer->availableCommandBufferCapacity);

    for (Uint32 i = 0; i < allocateCount; i += 1) {
        commandBuffer = SDL_calloc(1, sizeof(D3D11CommandBuffer));
        commandBuffer->renderer = renderer;

        /* Deferred Device Context */
        res = ID3D11Device1_CreateDeferredContext1(
            renderer->device,
            0,
            &commandBuffer->context);
        ERROR_CHECK("Could not create deferred context");

        /* Initialize debug annotation support, if available */
        ID3D11DeviceContext_QueryInterface(
            commandBuffer->context,
            &D3D_IID_ID3DUserDefinedAnnotation,
            (void **)&commandBuffer->annotation);

        /* Window handling */
        commandBuffer->windowDataCapacity = 1;
        commandBuffer->windowDataCount = 0;
        commandBuffer->windowDatas = SDL_malloc(
            commandBuffer->windowDataCapacity * sizeof(D3D11WindowData *));

        /* Reference Counting */
        commandBuffer->usedBufferCapacity = 4;
        commandBuffer->usedBufferCount = 0;
        commandBuffer->usedBuffers = SDL_malloc(
            commandBuffer->usedBufferCapacity * sizeof(D3D11Buffer *));

        commandBuffer->usedTransferBufferCapacity = 4;
        commandBuffer->usedTransferBufferCount = 0;
        commandBuffer->usedTransferBuffers = SDL_malloc(
            commandBuffer->usedTransferBufferCapacity * sizeof(D3D11TransferBuffer *));

        commandBuffer->usedTextureSubresourceCapacity = 4;
        commandBuffer->usedTextureSubresourceCount = 0;
        commandBuffer->usedTextureSubresources = SDL_malloc(
            commandBuffer->usedTextureSubresourceCapacity * sizeof(D3D11TextureSubresource *));

        commandBuffer->usedUniformBufferCapacity = 4;
        commandBuffer->usedUniformBufferCount = 0;
        commandBuffer->usedUniformBuffers = SDL_malloc(
            commandBuffer->usedUniformBufferCapacity * sizeof(D3D11UniformBuffer *));

        renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = commandBuffer;
        renderer->availableCommandBufferCount += 1;
    }
}

static D3D11CommandBuffer *D3D11_INTERNAL_GetInactiveCommandBufferFromPool(
    D3D11Renderer *renderer)
{
    D3D11CommandBuffer *commandBuffer;

    if (renderer->availableCommandBufferCount == 0) {
        D3D11_INTERNAL_AllocateCommandBuffers(
            renderer,
            renderer->availableCommandBufferCapacity);
    }

    commandBuffer = renderer->availableCommandBuffers[renderer->availableCommandBufferCount - 1];
    renderer->availableCommandBufferCount -= 1;

    return commandBuffer;
}

static SDL_bool D3D11_INTERNAL_CreateFence(
    D3D11Renderer *renderer)
{
    D3D11_QUERY_DESC queryDesc;
    ID3D11Query *queryHandle;
    D3D11Fence *fence;
    HRESULT res;

    queryDesc.Query = D3D11_QUERY_EVENT;
    queryDesc.MiscFlags = 0;
    res = ID3D11Device_CreateQuery(
        renderer->device,
        &queryDesc,
        &queryHandle);
    ERROR_CHECK_RETURN("Could not create query", 0);

    fence = SDL_malloc(sizeof(D3D11Fence));
    fence->handle = queryHandle;
    SDL_AtomicSet(&fence->referenceCount, 0);

    /* Add it to the available pool */
    if (renderer->availableFenceCount >= renderer->availableFenceCapacity) {
        renderer->availableFenceCapacity *= 2;
        renderer->availableFences = SDL_realloc(
            renderer->availableFences,
            sizeof(D3D11Fence *) * renderer->availableFenceCapacity);
    }

    renderer->availableFences[renderer->availableFenceCount] = fence;
    renderer->availableFenceCount += 1;

    return SDL_TRUE;
}

static SDL_bool D3D11_INTERNAL_AcquireFence(
    D3D11CommandBuffer *commandBuffer)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    D3D11Fence *fence;

    /* Acquire a fence from the pool */
    SDL_LockMutex(renderer->fenceLock);

    if (renderer->availableFenceCount == 0) {
        if (!D3D11_INTERNAL_CreateFence(renderer)) {
            SDL_UnlockMutex(renderer->fenceLock);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create fence!");
            return SDL_FALSE;
        }
    }

    fence = renderer->availableFences[renderer->availableFenceCount - 1];
    renderer->availableFenceCount -= 1;

    SDL_UnlockMutex(renderer->fenceLock);

    /* Associate the fence with the command buffer */
    commandBuffer->fence = fence;
    (void)SDL_AtomicIncRef(&commandBuffer->fence->referenceCount);

    return SDL_TRUE;
}

static SDL_GpuCommandBuffer *D3D11_AcquireCommandBuffer(
    SDL_GpuRenderer *driverData)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11CommandBuffer *commandBuffer;
    Uint32 i;

    SDL_LockMutex(renderer->acquireCommandBufferLock);

    commandBuffer = D3D11_INTERNAL_GetInactiveCommandBufferFromPool(renderer);
    commandBuffer->graphicsPipeline = NULL;
    commandBuffer->computePipeline = NULL;
    for (i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1) {
        commandBuffer->colorTargetResolveTexture[i] = NULL;
        commandBuffer->colorTargetResolveSubresourceIndex[i] = 0;
        commandBuffer->colorTargetMsaaHandle[i] = NULL;
    }

    for (i = 0; i < MAX_UNIFORM_BUFFERS_PER_STAGE; i += 1) {
        commandBuffer->vertexUniformBuffers[i] = NULL;
        commandBuffer->fragmentUniformBuffers[i] = NULL;
        commandBuffer->computeUniformBuffers[i] = NULL;
    }

    commandBuffer->needVertexSamplerBind = SDL_TRUE;
    commandBuffer->needVertexResourceBind = SDL_TRUE;
    commandBuffer->needVertexUniformBufferBind = SDL_TRUE;
    commandBuffer->needFragmentSamplerBind = SDL_TRUE;
    commandBuffer->needFragmentResourceBind = SDL_TRUE;
    commandBuffer->needFragmentUniformBufferBind = SDL_TRUE;
    commandBuffer->needComputeUAVBind = SDL_TRUE;
    commandBuffer->needComputeSRVBind = SDL_TRUE;
    commandBuffer->needComputeUniformBufferBind = SDL_TRUE;

    SDL_zeroa(commandBuffer->vertexSamplers);
    SDL_zeroa(commandBuffer->vertexShaderResourceViews);
    SDL_zeroa(commandBuffer->fragmentSamplers);
    SDL_zeroa(commandBuffer->fragmentShaderResourceViews);
    SDL_zeroa(commandBuffer->computeShaderResourceViews);
    SDL_zeroa(commandBuffer->computeUnorderedAccessViews);

    D3D11_INTERNAL_AcquireFence(commandBuffer);
    commandBuffer->autoReleaseFence = 1;

    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    return (SDL_GpuCommandBuffer *)commandBuffer;
}

static D3D11UniformBuffer *D3D11_INTERNAL_AcquireUniformBufferFromPool(D3D11Renderer *renderer)
{
    D3D11UniformBuffer *uniformBuffer;

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    if (renderer->uniformBufferPoolCount > 0) {
        uniformBuffer = renderer->uniformBufferPool[renderer->uniformBufferPoolCount - 1];
        renderer->uniformBufferPoolCount -= 1;
    } else {
        uniformBuffer = D3D11_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    return uniformBuffer;
}

static void D3D11_INTERNAL_ReturnUniformBufferToPool(
    D3D11Renderer *renderer,
    D3D11UniformBuffer *uniformBuffer)
{
    if (renderer->uniformBufferPoolCount >= renderer->uniformBufferPoolCapacity) {
        renderer->uniformBufferPoolCapacity *= 2;
        renderer->uniformBufferPool = SDL_realloc(
            renderer->uniformBufferPool,
            renderer->uniformBufferPoolCapacity * sizeof(D3D11UniformBuffer *));
    }

    renderer->uniformBufferPool[renderer->uniformBufferPoolCount] = uniformBuffer;
    renderer->uniformBufferPoolCount += 1;

    uniformBuffer->writeOffset = 0;
    uniformBuffer->drawOffset = 0;
    uniformBuffer->mappedData = NULL;
}

static void D3D11_INTERNAL_PushUniformData(
    D3D11CommandBuffer *d3d11CommandBuffer,
    SDL_GpuShaderStage shaderStage,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    D3D11Renderer *renderer = d3d11CommandBuffer->renderer;
    D3D11UniformBuffer *d3d11UniformBuffer;
    D3D11_MAPPED_SUBRESOURCE subres;
    HRESULT res;

    if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
        if (d3d11CommandBuffer->vertexUniformBuffers[slotIndex] == NULL) {
            d3d11CommandBuffer->vertexUniformBuffers[slotIndex] = D3D11_INTERNAL_AcquireUniformBufferFromPool(
                d3d11CommandBuffer->renderer);
        }
        d3d11UniformBuffer = d3d11CommandBuffer->vertexUniformBuffers[slotIndex];
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        if (d3d11CommandBuffer->fragmentUniformBuffers[slotIndex] == NULL) {
            d3d11CommandBuffer->fragmentUniformBuffers[slotIndex] = D3D11_INTERNAL_AcquireUniformBufferFromPool(
                d3d11CommandBuffer->renderer);
        }
        d3d11UniformBuffer = d3d11CommandBuffer->fragmentUniformBuffers[slotIndex];
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
        if (d3d11CommandBuffer->computeUniformBuffers[slotIndex] == NULL) {
            d3d11CommandBuffer->computeUniformBuffers[slotIndex] = D3D11_INTERNAL_AcquireUniformBufferFromPool(
                d3d11CommandBuffer->renderer);
        }
        d3d11UniformBuffer = d3d11CommandBuffer->computeUniformBuffers[slotIndex];
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized shader stage!");
        return;
    }

    d3d11UniformBuffer->currentBlockSize =
        D3D11_INTERNAL_NextHighestAlignment(
            dataLengthInBytes,
            256);

    /* If there is no more room, acquire a new uniform buffer */
    if (d3d11UniformBuffer->writeOffset + d3d11UniformBuffer->currentBlockSize >= UNIFORM_BUFFER_SIZE) {
        ID3D11DeviceContext_Unmap(
            d3d11CommandBuffer->context,
            (ID3D11Resource *)d3d11UniformBuffer->buffer,
            0);
        d3d11UniformBuffer->mappedData = NULL;

        D3D11_INTERNAL_TrackUniformBuffer(
            d3d11CommandBuffer,
            d3d11UniformBuffer);

        d3d11UniformBuffer = D3D11_INTERNAL_AcquireUniformBufferFromPool(d3d11CommandBuffer->renderer);

        d3d11UniformBuffer->drawOffset = 0;
        d3d11UniformBuffer->writeOffset = 0;

        if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
            d3d11CommandBuffer->vertexUniformBuffers[slotIndex] = d3d11UniformBuffer;
        } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
            d3d11CommandBuffer->fragmentUniformBuffers[slotIndex] = d3d11UniformBuffer;
        } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
            d3d11CommandBuffer->computeUniformBuffers[slotIndex] = d3d11UniformBuffer;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized shader stage!");
        }
    }

    /* Map the uniform data on first push */
    if (d3d11UniformBuffer->writeOffset == 0) {
        res = ID3D11DeviceContext_Map(
            d3d11CommandBuffer->context,
            (ID3D11Resource *)d3d11UniformBuffer->buffer,
            0,
            D3D11_MAP_WRITE_DISCARD,
            0,
            &subres);
        ERROR_CHECK_RETURN("Failed to map uniform buffer", )

        d3d11UniformBuffer->mappedData = subres.pData;
    }

    d3d11UniformBuffer->drawOffset = d3d11UniformBuffer->writeOffset;

    SDL_memcpy(
        (Uint8 *)d3d11UniformBuffer->mappedData + d3d11UniformBuffer->writeOffset,
        data,
        dataLengthInBytes);

    d3d11UniformBuffer->writeOffset += d3d11UniformBuffer->currentBlockSize;

    if (shaderStage == SDL_GPU_SHADERSTAGE_VERTEX) {
        d3d11CommandBuffer->needVertexUniformBufferBind = SDL_TRUE;
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_FRAGMENT) {
        d3d11CommandBuffer->needFragmentUniformBufferBind = SDL_TRUE;
    } else if (shaderStage == SDL_GPU_SHADERSTAGE_COMPUTE) {
        d3d11CommandBuffer->needComputeUniformBufferBind = SDL_TRUE;
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized shader stage!");
    }
}

static void D3D11_BeginRenderPass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
    Uint32 colorAttachmentCount,
    SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    ID3D11RenderTargetView *rtvs[MAX_COLOR_TARGET_BINDINGS];
    ID3D11DepthStencilView *dsv = NULL;
    Uint32 vpWidth = SDL_MAX_UINT32;
    Uint32 vpHeight = SDL_MAX_UINT32;
    D3D11_VIEWPORT viewport;
    D3D11_RECT scissorRect;

    d3d11CommandBuffer->needVertexSamplerBind = SDL_TRUE;
    d3d11CommandBuffer->needVertexResourceBind = SDL_TRUE;
    d3d11CommandBuffer->needFragmentSamplerBind = SDL_TRUE;
    d3d11CommandBuffer->needFragmentResourceBind = SDL_TRUE;

    /* Clear the bound targets for the current command buffer */
    for (Uint32 i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1) {
        d3d11CommandBuffer->colorTargetResolveTexture[i] = NULL;
        d3d11CommandBuffer->colorTargetResolveSubresourceIndex[i] = 0;
        d3d11CommandBuffer->colorTargetMsaaHandle[i] = NULL;
    }

    /* Set up the new color target bindings */
    for (Uint32 i = 0; i < colorAttachmentCount; i += 1) {
        D3D11TextureContainer *container = (D3D11TextureContainer *)colorAttachmentInfos[i].textureSlice.texture;
        D3D11TextureSubresource *subresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
            renderer,
            container,
            colorAttachmentInfos[i].textureSlice.layer,
            colorAttachmentInfos[i].textureSlice.mipLevel,
            colorAttachmentInfos[i].cycle);

        if (subresource->msaaHandle != NULL) {
            d3d11CommandBuffer->colorTargetResolveTexture[i] = subresource->parent;
            d3d11CommandBuffer->colorTargetResolveSubresourceIndex[i] = subresource->index;
            d3d11CommandBuffer->colorTargetMsaaHandle[i] = subresource->msaaHandle;

            rtvs[i] = subresource->msaaTargetView;
        } else {
            rtvs[i] = subresource->colorTargetView;
        }

        D3D11_INTERNAL_TrackTextureSubresource(
            d3d11CommandBuffer,
            subresource);
    }

    /* Get the DSV for the depth stencil attachment, if applicable */
    if (depthStencilAttachmentInfo != NULL) {
        D3D11TextureContainer *container = (D3D11TextureContainer *)depthStencilAttachmentInfo->textureSlice.texture;
        D3D11TextureSubresource *subresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
            renderer,
            container,
            depthStencilAttachmentInfo->textureSlice.layer,
            depthStencilAttachmentInfo->textureSlice.mipLevel,
            depthStencilAttachmentInfo->cycle);

        dsv = subresource->depthStencilTargetView;

        D3D11_INTERNAL_TrackTextureSubresource(
            d3d11CommandBuffer,
            subresource);
    }

    /* Actually set the RTs */
    ID3D11DeviceContext_OMSetRenderTargets(
        d3d11CommandBuffer->context,
        colorAttachmentCount,
        colorAttachmentCount > 0 ? rtvs : NULL,
        dsv);

    /* Perform load ops on the RTs */
    for (Uint32 i = 0; i < colorAttachmentCount; i += 1) {
        if (colorAttachmentInfos[i].loadOp == SDL_GPU_LOADOP_CLEAR) {
            float clearColors[] = {
                colorAttachmentInfos[i].clearColor.r,
                colorAttachmentInfos[i].clearColor.g,
                colorAttachmentInfos[i].clearColor.b,
                colorAttachmentInfos[i].clearColor.a
            };
            ID3D11DeviceContext_ClearRenderTargetView(
                d3d11CommandBuffer->context,
                rtvs[i],
                clearColors);
        }
    }

    if (depthStencilAttachmentInfo != NULL) {
        D3D11_CLEAR_FLAG dsClearFlags = 0;
        if (depthStencilAttachmentInfo->loadOp == SDL_GPU_LOADOP_CLEAR) {
            dsClearFlags |= D3D11_CLEAR_DEPTH;
        }
        if (depthStencilAttachmentInfo->stencilLoadOp == SDL_GPU_LOADOP_CLEAR) {
            dsClearFlags |= D3D11_CLEAR_STENCIL;
        }

        if (dsClearFlags != 0) {
            ID3D11DeviceContext_ClearDepthStencilView(
                d3d11CommandBuffer->context,
                dsv,
                dsClearFlags,
                depthStencilAttachmentInfo->depthStencilClearValue.depth,
                (Uint8)depthStencilAttachmentInfo->depthStencilClearValue.stencil);
        }
    }

    /* The viewport cannot be larger than the smallest attachment. */
    for (Uint32 i = 0; i < colorAttachmentCount; i += 1) {
        D3D11Texture *texture = ((D3D11TextureContainer *)colorAttachmentInfos[i].textureSlice.texture)->activeTexture;
        Uint32 w = texture->width >> colorAttachmentInfos[i].textureSlice.mipLevel;
        Uint32 h = texture->height >> colorAttachmentInfos[i].textureSlice.mipLevel;

        if (w < vpWidth) {
            vpWidth = w;
        }

        if (h < vpHeight) {
            vpHeight = h;
        }
    }

    if (depthStencilAttachmentInfo != NULL) {
        D3D11Texture *texture = ((D3D11TextureContainer *)depthStencilAttachmentInfo->textureSlice.texture)->activeTexture;
        Uint32 w = texture->width >> depthStencilAttachmentInfo->textureSlice.mipLevel;
        Uint32 h = texture->height >> depthStencilAttachmentInfo->textureSlice.mipLevel;

        if (w < vpWidth) {
            vpWidth = w;
        }

        if (h < vpHeight) {
            vpHeight = h;
        }
    }

    /* Set default viewport and scissor state */
    viewport.TopLeftX = 0;
    viewport.TopLeftY = 0;
    viewport.Width = (FLOAT)vpWidth;
    viewport.Height = (FLOAT)vpHeight;
    viewport.MinDepth = 0;
    viewport.MaxDepth = 1;

    ID3D11DeviceContext_RSSetViewports(
        d3d11CommandBuffer->context,
        1,
        &viewport);

    scissorRect.left = 0;
    scissorRect.right = (LONG)viewport.Width;
    scissorRect.top = 0;
    scissorRect.bottom = (LONG)viewport.Height;

    ID3D11DeviceContext_RSSetScissorRects(
        d3d11CommandBuffer->context,
        1,
        &scissorRect);
}

static void D3D11_BindGraphicsPipeline(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuGraphicsPipeline *graphicsPipeline)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11GraphicsPipeline *pipeline = (D3D11GraphicsPipeline *)graphicsPipeline;

    d3d11CommandBuffer->graphicsPipeline = pipeline;

    ID3D11DeviceContext_OMSetBlendState(
        d3d11CommandBuffer->context,
        pipeline->colorAttachmentBlendState,
        pipeline->blendConstants,
        pipeline->multisampleState.sampleMask);

    ID3D11DeviceContext_OMSetDepthStencilState(
        d3d11CommandBuffer->context,
        pipeline->depthStencilState,
        pipeline->stencilRef);

    ID3D11DeviceContext_IASetPrimitiveTopology(
        d3d11CommandBuffer->context,
        SDLToD3D11_PrimitiveType[pipeline->primitiveType]);

    ID3D11DeviceContext_IASetInputLayout(
        d3d11CommandBuffer->context,
        pipeline->inputLayout);

    ID3D11DeviceContext_RSSetState(
        d3d11CommandBuffer->context,
        pipeline->rasterizerState);

    ID3D11DeviceContext_VSSetShader(
        d3d11CommandBuffer->context,
        pipeline->vertexShader,
        NULL,
        0);

    ID3D11DeviceContext_PSSetShader(
        d3d11CommandBuffer->context,
        pipeline->fragmentShader,
        NULL,
        0);

    /* Acquire uniform buffers if necessary */
    for (Uint32 i = 0; i < pipeline->vertexUniformBufferCount; i += 1) {
        if (d3d11CommandBuffer->vertexUniformBuffers[i] == NULL) {
            d3d11CommandBuffer->vertexUniformBuffers[i] = D3D11_INTERNAL_AcquireUniformBufferFromPool(
                d3d11CommandBuffer->renderer);
        }
    }

    for (Uint32 i = 0; i < pipeline->fragmentUniformBufferCount; i += 1) {
        if (d3d11CommandBuffer->fragmentUniformBuffers[i] == NULL) {
            d3d11CommandBuffer->fragmentUniformBuffers[i] = D3D11_INTERNAL_AcquireUniformBufferFromPool(
                d3d11CommandBuffer->renderer);
        }
    }

    /* Mark that uniform bindings are needed */
    d3d11CommandBuffer->needVertexUniformBufferBind = SDL_TRUE;
    d3d11CommandBuffer->needFragmentUniformBufferBind = SDL_TRUE;
}

static void D3D11_SetViewport(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuViewport *viewport)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11_VIEWPORT vp = {
        viewport->x,
        viewport->y,
        viewport->w,
        viewport->h,
        viewport->minDepth,
        viewport->maxDepth
    };

    ID3D11DeviceContext_RSSetViewports(
        d3d11CommandBuffer->context,
        1,
        &vp);
}

static void D3D11_SetScissor(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuRect *scissor)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11_RECT rect = {
        scissor->x,
        scissor->y,
        scissor->x + scissor->w,
        scissor->y + scissor->h
    };

    ID3D11DeviceContext_RSSetScissorRects(
        d3d11CommandBuffer->context,
        1,
        &rect);
}

static void D3D11_BindVertexBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstBinding,
    SDL_GpuBufferBinding *pBindings,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    ID3D11Buffer *bufferHandles[MAX_BUFFER_BINDINGS];
    UINT bufferOffsets[MAX_BUFFER_BINDINGS];

    for (Uint32 i = 0; i < bindingCount; i += 1) {
        D3D11Buffer *currentBuffer = ((D3D11BufferContainer *)pBindings[i].buffer)->activeBuffer;
        bufferHandles[i] = currentBuffer->handle;
        bufferOffsets[i] = pBindings[i].offset;
        D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, currentBuffer);
    }

    ID3D11DeviceContext_IASetVertexBuffers(
        d3d11CommandBuffer->context,
        firstBinding,
        bindingCount,
        bufferHandles,
        &d3d11CommandBuffer->graphicsPipeline->vertexStrides[firstBinding],
        bufferOffsets);
}

static void D3D11_BindIndexBuffer(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBufferBinding *pBinding,
    SDL_GpuIndexElementSize indexElementSize)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Buffer *d3d11Buffer = ((D3D11BufferContainer *)pBinding->buffer)->activeBuffer;

    D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, d3d11Buffer);

    ID3D11DeviceContext_IASetIndexBuffer(
        d3d11CommandBuffer->context,
        d3d11Buffer->handle,
        SDLToD3D11_IndexType[indexElementSize],
        (UINT)pBinding->offset);
}

static void D3D11_BindVertexSamplers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11TextureContainer *textureContainer;
    Uint32 i, j;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (D3D11TextureContainer *)textureSamplerBindings[i].texture;

        for (j = 0; j < textureContainer->activeTexture->subresourceCount; j += 1) {
            D3D11_INTERNAL_TrackTextureSubresource(
                d3d11CommandBuffer,
                &textureContainer->activeTexture->subresources[j]);
        }

        d3d11CommandBuffer->vertexSamplers[firstSlot + i] =
            ((D3D11Sampler *)textureSamplerBindings[i].sampler)->handle;

        d3d11CommandBuffer->vertexShaderResourceViews[firstSlot + i] =
            textureContainer->activeTexture->shaderView;
    }

    d3d11CommandBuffer->needVertexSamplerBind = SDL_TRUE;
    d3d11CommandBuffer->needVertexResourceBind = SDL_TRUE;
}

static void D3D11_BindVertexStorageTextures(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11TextureContainer *textureContainer;
    D3D11TextureSubresource *textureSubresource;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (D3D11TextureContainer *)storageTextureSlices[i].texture;
        textureSubresource = D3D11_INTERNAL_FetchTextureSubresource(
            textureContainer->activeTexture,
            storageTextureSlices[i].layer,
            storageTextureSlices[i].mipLevel);

        D3D11_INTERNAL_TrackTextureSubresource(
            d3d11CommandBuffer,
            textureSubresource);

        d3d11CommandBuffer->vertexShaderResourceViews[firstSlot + i +
                                                      d3d11CommandBuffer->graphicsPipeline->vertexSamplerCount] = textureSubresource->srv;
    }

    d3d11CommandBuffer->needVertexResourceBind = SDL_TRUE;
}

static void D3D11_BindVertexStorageBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11BufferContainer *bufferContainer;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        bufferContainer = (D3D11BufferContainer *)storageBuffers[i];

        D3D11_INTERNAL_TrackBuffer(
            d3d11CommandBuffer,
            bufferContainer->activeBuffer);

        d3d11CommandBuffer->vertexShaderResourceViews[firstSlot + i +
                                                      d3d11CommandBuffer->graphicsPipeline->vertexSamplerCount +
                                                      d3d11CommandBuffer->graphicsPipeline->vertexStorageTextureCount] = bufferContainer->activeBuffer->srv;
    }

    d3d11CommandBuffer->needVertexResourceBind = SDL_TRUE;
}

static void D3D11_BindFragmentSamplers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSamplerBinding *textureSamplerBindings,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11TextureContainer *textureContainer;
    Uint32 i, j;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (D3D11TextureContainer *)textureSamplerBindings[i].texture;

        for (j = 0; j < textureContainer->activeTexture->subresourceCount; j += 1) {
            D3D11_INTERNAL_TrackTextureSubresource(
                d3d11CommandBuffer,
                &textureContainer->activeTexture->subresources[j]);
        }

        d3d11CommandBuffer->fragmentSamplers[firstSlot + i] =
            ((D3D11Sampler *)textureSamplerBindings[i].sampler)->handle;

        d3d11CommandBuffer->fragmentShaderResourceViews[firstSlot + i] =
            textureContainer->activeTexture->shaderView;
    }

    d3d11CommandBuffer->needFragmentSamplerBind = SDL_TRUE;
    d3d11CommandBuffer->needFragmentResourceBind = SDL_TRUE;
}

static void D3D11_BindFragmentStorageTextures(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11TextureContainer *textureContainer;
    D3D11TextureSubresource *textureSubresource;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (D3D11TextureContainer *)storageTextureSlices[i].texture;
        textureSubresource = D3D11_INTERNAL_FetchTextureSubresource(
            textureContainer->activeTexture,
            storageTextureSlices[i].layer,
            storageTextureSlices[i].mipLevel);

        D3D11_INTERNAL_TrackTextureSubresource(
            d3d11CommandBuffer,
            textureSubresource);

        d3d11CommandBuffer->fragmentShaderResourceViews[firstSlot + i +
                                                        d3d11CommandBuffer->graphicsPipeline->fragmentSamplerCount] = textureSubresource->srv;
    }

    d3d11CommandBuffer->needFragmentResourceBind = SDL_TRUE;
}

static void D3D11_BindFragmentStorageBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11BufferContainer *bufferContainer;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        bufferContainer = (D3D11BufferContainer *)storageBuffers[i];

        D3D11_INTERNAL_TrackBuffer(
            d3d11CommandBuffer,
            bufferContainer->activeBuffer);

        d3d11CommandBuffer->fragmentShaderResourceViews[firstSlot + i +
                                                        d3d11CommandBuffer->graphicsPipeline->fragmentSamplerCount +
                                                        d3d11CommandBuffer->graphicsPipeline->fragmentStorageTextureCount] = bufferContainer->activeBuffer->srv;
    }

    d3d11CommandBuffer->needFragmentResourceBind = SDL_TRUE;
}

static void D3D11_INTERNAL_BindGraphicsResources(
    D3D11CommandBuffer *commandBuffer)
{
    D3D11GraphicsPipeline *graphicsPipeline = commandBuffer->graphicsPipeline;

    Uint32 vertexResourceCount =
        graphicsPipeline->vertexSamplerCount +
        graphicsPipeline->vertexStorageTextureCount +
        graphicsPipeline->vertexStorageBufferCount;

    Uint32 fragmentResourceCount =
        graphicsPipeline->fragmentSamplerCount +
        graphicsPipeline->fragmentStorageTextureCount +
        graphicsPipeline->fragmentStorageBufferCount;

    ID3D11Buffer *nullBuf = NULL;
    Uint32 offsetInConstants, blockSizeInConstants, i;

    if (commandBuffer->needVertexSamplerBind) {
        if (graphicsPipeline->vertexSamplerCount > 0) {
            ID3D11DeviceContext_VSSetSamplers(
                commandBuffer->context,
                0,
                graphicsPipeline->vertexSamplerCount,
                commandBuffer->vertexSamplers);
        }

        commandBuffer->needVertexSamplerBind = SDL_FALSE;
    }

    if (commandBuffer->needVertexResourceBind) {
        if (vertexResourceCount > 0) {
            ID3D11DeviceContext_VSSetShaderResources(
                commandBuffer->context,
                0,
                vertexResourceCount,
                commandBuffer->vertexShaderResourceViews);
        }

        commandBuffer->needVertexResourceBind = SDL_FALSE;
    }

    if (commandBuffer->needVertexUniformBufferBind) {
        for (i = 0; i < graphicsPipeline->vertexUniformBufferCount; i += 1) {
            /* stupid workaround for god awful D3D11 drivers
             * see: https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1#calling-vssetconstantbuffers1-with-command-list-emulation
             */
            ID3D11DeviceContext1_VSSetConstantBuffers(
                commandBuffer->context,
                i,
                1,
                &nullBuf);

            offsetInConstants = commandBuffer->vertexUniformBuffers[i]->drawOffset / 16;
            blockSizeInConstants = commandBuffer->vertexUniformBuffers[i]->currentBlockSize / 16;

            ID3D11DeviceContext1_VSSetConstantBuffers1(
                commandBuffer->context,
                i,
                1,
                &commandBuffer->vertexUniformBuffers[i]->buffer,
                &offsetInConstants,
                &blockSizeInConstants);

            D3D11_INTERNAL_TrackUniformBuffer(
                commandBuffer,
                commandBuffer->vertexUniformBuffers[i]);
        }

        commandBuffer->needVertexUniformBufferBind = SDL_FALSE;
    }

    if (commandBuffer->needFragmentSamplerBind) {
        if (graphicsPipeline->fragmentSamplerCount > 0) {
            ID3D11DeviceContext_PSSetSamplers(
                commandBuffer->context,
                0,
                graphicsPipeline->fragmentSamplerCount,
                commandBuffer->fragmentSamplers);
        }

        commandBuffer->needFragmentSamplerBind = SDL_FALSE;
    }

    if (commandBuffer->needFragmentResourceBind) {
        if (fragmentResourceCount > 0) {
            ID3D11DeviceContext_PSSetShaderResources(
                commandBuffer->context,
                0,
                fragmentResourceCount,
                commandBuffer->fragmentShaderResourceViews);
        }

        commandBuffer->needFragmentResourceBind = SDL_FALSE;
    }

    if (commandBuffer->needFragmentUniformBufferBind) {
        for (i = 0; i < graphicsPipeline->fragmentUniformBufferCount; i += 1) {
            /* stupid workaround for god awful D3D11 drivers
             * see: https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-pssetconstantbuffers1#calling-pssetconstantbuffers1-with-command-list-emulation
             */
            ID3D11DeviceContext1_PSSetConstantBuffers(
                commandBuffer->context,
                i,
                1,
                &nullBuf);

            offsetInConstants = commandBuffer->fragmentUniformBuffers[i]->drawOffset / 16;
            blockSizeInConstants = commandBuffer->fragmentUniformBuffers[i]->currentBlockSize / 16;

            ID3D11DeviceContext1_PSSetConstantBuffers1(
                commandBuffer->context,
                i,
                1,
                &commandBuffer->fragmentUniformBuffers[i]->buffer,
                &offsetInConstants,
                &blockSizeInConstants);

            D3D11_INTERNAL_TrackUniformBuffer(
                commandBuffer,
                commandBuffer->fragmentUniformBuffers[i]);
        }

        commandBuffer->needFragmentUniformBufferBind = SDL_FALSE;
    }
}

static void D3D11_DrawIndexedPrimitives(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 baseVertex,
    Uint32 startIndex,
    Uint32 primitiveCount,
    Uint32 instanceCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11_INTERNAL_BindGraphicsResources(d3d11CommandBuffer);

    ID3D11DeviceContext_DrawIndexedInstanced(
        d3d11CommandBuffer->context,
        PrimitiveVerts(d3d11CommandBuffer->graphicsPipeline->primitiveType, primitiveCount),
        instanceCount,
        startIndex,
        baseVertex,
        0);
}

static void D3D11_DrawPrimitives(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 vertexStart,
    Uint32 primitiveCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11_INTERNAL_BindGraphicsResources(d3d11CommandBuffer);

    ID3D11DeviceContext_Draw(
        d3d11CommandBuffer->context,
        PrimitiveVerts(d3d11CommandBuffer->graphicsPipeline->primitiveType, primitiveCount),
        vertexStart);
}

static void D3D11_DrawPrimitivesIndirect(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11_INTERNAL_BindGraphicsResources(d3d11CommandBuffer);

    D3D11Buffer *d3d11Buffer = ((D3D11BufferContainer *)buffer)->activeBuffer;

    /* D3D11: "We have multi-draw at home!"
     * Multi-draw at home:
     */
    for (Uint32 i = 0; i < drawCount; i += 1) {
        ID3D11DeviceContext_DrawInstancedIndirect(
            d3d11CommandBuffer->context,
            d3d11Buffer->handle,
            offsetInBytes + (stride * i));
    }

    D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, d3d11Buffer);
}

static void D3D11_DrawIndexedPrimitivesIndirect(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuBuffer *buffer,
    Uint32 offsetInBytes,
    Uint32 drawCount,
    Uint32 stride)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11_INTERNAL_BindGraphicsResources(d3d11CommandBuffer);

    D3D11Buffer *d3d11Buffer = ((D3D11BufferContainer *)buffer)->activeBuffer;

    /* D3D11: "We have multi-draw at home!"
     * Multi-draw at home:
     */
    for (Uint32 i = 0; i < drawCount; i += 1) {
        ID3D11DeviceContext_DrawIndexedInstancedIndirect(
            d3d11CommandBuffer->context,
            d3d11Buffer->handle,
            offsetInBytes + (stride * i));
    }

    D3D11_INTERNAL_TrackBuffer(d3d11CommandBuffer, d3d11Buffer);
}

static void D3D11_EndRenderPass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    Uint32 i;

    /* Set render target slots to NULL to avoid NULL set behavior */
    /* https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-pssetshaderresources */
    ID3D11DeviceContext_OMSetRenderTargets(
        d3d11CommandBuffer->context,
        MAX_COLOR_TARGET_BINDINGS,
        nullRTVs,
        NULL);

    /* Resolve MSAA color render targets */
    for (i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1) {
        if (d3d11CommandBuffer->colorTargetMsaaHandle[i] != NULL) {
            ID3D11DeviceContext_ResolveSubresource(
                d3d11CommandBuffer->context,
                d3d11CommandBuffer->colorTargetResolveTexture[i]->handle,
                d3d11CommandBuffer->colorTargetResolveSubresourceIndex[i],
                d3d11CommandBuffer->colorTargetMsaaHandle[i],
                0,
                SDLToD3D11_TextureFormat[d3d11CommandBuffer->colorTargetResolveTexture[i]->format]);
        }
    }
}

static void D3D11_PushVertexUniformData(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    D3D11_INTERNAL_PushUniformData(
        (D3D11CommandBuffer *)commandBuffer,
        SDL_GPU_SHADERSTAGE_VERTEX,
        slotIndex,
        data,
        dataLengthInBytes);
}

static void D3D11_PushFragmentUniformData(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    D3D11_INTERNAL_PushUniformData(
        (D3D11CommandBuffer *)commandBuffer,
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        slotIndex,
        data,
        dataLengthInBytes);
}

/* Blit */

static void D3D11_Blit(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuTextureRegion *source,
    SDL_GpuTextureRegion *destination,
    SDL_GpuFilter filterMode,
    SDL_bool cycle)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    D3D11TextureContainer *sourceTextureContainer = (D3D11TextureContainer *)source->textureSlice.texture;
    D3D11TextureContainer *destinationTextureContainer = (D3D11TextureContainer *)destination->textureSlice.texture;
    SDL_GpuColorAttachmentInfo colorAttachmentInfo;
    SDL_GpuViewport viewport;
    SDL_GpuTextureSamplerBinding textureSamplerBinding;

    if (destinationTextureContainer->activeTexture->depth > 1) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "3D blit destination not implemented!");
        return;
    }

    /* Unused */
    colorAttachmentInfo.clearColor.r = 0;
    colorAttachmentInfo.clearColor.g = 0;
    colorAttachmentInfo.clearColor.b = 0;
    colorAttachmentInfo.clearColor.a = 0;

    /* If the entire destination is blitted, we don't have to load */
    if (
        destinationTextureContainer->activeTexture->layerCount == 1 &&
        destinationTextureContainer->activeTexture->levelCount == 1 &&
        destination->w == destinationTextureContainer->activeTexture->width &&
        destination->h == destinationTextureContainer->activeTexture->height &&
        destination->d == destinationTextureContainer->activeTexture->depth) {
        colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_DONT_CARE;
    } else {
        colorAttachmentInfo.loadOp = SDL_GPU_LOADOP_LOAD;
    }

    colorAttachmentInfo.storeOp = SDL_GPU_STOREOP_STORE;

    colorAttachmentInfo.textureSlice = destination->textureSlice;
    colorAttachmentInfo.cycle = cycle;

    D3D11_BeginRenderPass(
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

    D3D11_SetViewport(
        commandBuffer,
        &viewport);

    if (
        sourceTextureContainer->activeTexture->layerCount == 1 &&
        sourceTextureContainer->activeTexture->depth == 1) {
        /* 2D source */
        D3D11_BindGraphicsPipeline(
            commandBuffer,
            renderer->blitFrom2DPipeline);
    } else if (
        sourceTextureContainer->activeTexture->layerCount > 1) {
        /* 2D array source */
        D3D11_BindGraphicsPipeline(
            commandBuffer,
            renderer->blitFrom2DArrayPipeline);

        D3D11_PushFragmentUniformData(
            commandBuffer,
            0,
            &source->textureSlice.layer,
            sizeof(Uint32));
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "3D blit source not implemented!");
        return;
    }

    textureSamplerBinding.texture = source->textureSlice.texture;
    textureSamplerBinding.sampler =
        filterMode == SDL_GPU_FILTER_NEAREST ? renderer->blitNearestSampler : renderer->blitLinearSampler;

    if (((D3D11TextureContainer *)textureSamplerBinding.texture)->activeTexture->shaderView == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Blit source texture must be created with SAMPLER bit!");
    }

    D3D11_BindFragmentSamplers(
        commandBuffer,
        0,
        &textureSamplerBinding,
        1);

    D3D11_DrawPrimitives(
        commandBuffer,
        0,
        1);

    D3D11_EndRenderPass(commandBuffer);
}

/* Compute State */

static void D3D11_BeginComputePass(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuStorageTextureReadWriteBinding *storageTextureBindings,
    Uint32 storageTextureBindingCount,
    SDL_GpuStorageBufferReadWriteBinding *storageBufferBindings,
    Uint32 storageBufferBindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11TextureContainer *textureContainer;
    D3D11TextureSubresource *textureSubresource;
    D3D11BufferContainer *bufferContainer;
    D3D11Buffer *buffer;
    Uint32 i;

    for (i = 0; i < storageTextureBindingCount; i += 1) {
        textureContainer = (D3D11TextureContainer *)storageTextureBindings[i].textureSlice.texture;

        textureSubresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
            d3d11CommandBuffer->renderer,
            textureContainer,
            storageTextureBindings[i].textureSlice.layer,
            storageTextureBindings[i].textureSlice.mipLevel,
            storageTextureBindings[i].cycle);

        D3D11_INTERNAL_TrackTextureSubresource(
            d3d11CommandBuffer,
            textureSubresource);

        d3d11CommandBuffer->computeUnorderedAccessViews[i] =
            textureSubresource->uav;
    }

    for (i = 0; i < storageBufferBindingCount; i += 1) {
        bufferContainer = (D3D11BufferContainer *)storageBufferBindings[i].buffer;

        buffer = D3D11_INTERNAL_PrepareBufferForWrite(
            d3d11CommandBuffer->renderer,
            bufferContainer,
            storageBufferBindings[i].cycle);

        D3D11_INTERNAL_TrackBuffer(
            d3d11CommandBuffer,
            buffer);

        d3d11CommandBuffer->computeUnorderedAccessViews[i + storageTextureBindingCount] = buffer->uav;
    }

    d3d11CommandBuffer->needComputeUAVBind = SDL_TRUE;
}

static void D3D11_BindComputePipeline(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_GpuComputePipeline *computePipeline)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11ComputePipeline *pipeline = (D3D11ComputePipeline *)computePipeline;

    d3d11CommandBuffer->computePipeline = pipeline;

    ID3D11DeviceContext_CSSetShader(
        d3d11CommandBuffer->context,
        pipeline->computeShader,
        NULL,
        0);

    /* Acquire uniform buffers if necessary */
    for (Uint32 i = 0; i < pipeline->uniformBufferCount; i += 1) {
        if (d3d11CommandBuffer->computeUniformBuffers[i] == NULL) {
            d3d11CommandBuffer->computeUniformBuffers[i] = D3D11_INTERNAL_AcquireUniformBufferFromPool(
                d3d11CommandBuffer->renderer);
        }
    }

    d3d11CommandBuffer->needComputeUniformBufferBind = SDL_TRUE;
}

static void D3D11_BindComputeStorageTextures(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuTextureSlice *storageTextureSlices,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11TextureContainer *textureContainer;
    D3D11TextureSubresource *textureSubresource;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        textureContainer = (D3D11TextureContainer *)storageTextureSlices[i].texture;
        textureSubresource = D3D11_INTERNAL_FetchTextureSubresource(
            textureContainer->activeTexture,
            storageTextureSlices[i].layer,
            storageTextureSlices[i].mipLevel);

        D3D11_INTERNAL_TrackTextureSubresource(
            d3d11CommandBuffer,
            textureSubresource);

        d3d11CommandBuffer->computeShaderResourceViews[firstSlot + i] =
            textureSubresource->srv;
    }

    d3d11CommandBuffer->needComputeSRVBind = SDL_TRUE;
}

static void D3D11_BindComputeStorageBuffers(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 firstSlot,
    SDL_GpuBuffer **storageBuffers,
    Uint32 bindingCount)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11BufferContainer *bufferContainer;
    Uint32 i;

    for (i = 0; i < bindingCount; i += 1) {
        bufferContainer = (D3D11BufferContainer *)storageBuffers[i];

        D3D11_INTERNAL_TrackBuffer(
            d3d11CommandBuffer,
            bufferContainer->activeBuffer);

        d3d11CommandBuffer->computeShaderResourceViews[firstSlot + i +
                                                       d3d11CommandBuffer->computePipeline->readOnlyStorageTextureCount] = bufferContainer->activeBuffer->srv;
    }

    d3d11CommandBuffer->needComputeSRVBind = SDL_TRUE;
}

static void D3D11_PushComputeUniformData(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 slotIndex,
    const void *data,
    Uint32 dataLengthInBytes)
{
    D3D11_INTERNAL_PushUniformData(
        (D3D11CommandBuffer *)commandBuffer,
        SDL_GPU_SHADERSTAGE_COMPUTE,
        slotIndex,
        data,
        dataLengthInBytes);
}

static void D3D11_INTERNAL_BindComputeResources(
    D3D11CommandBuffer *commandBuffer)
{
    D3D11ComputePipeline *computePipeline = commandBuffer->computePipeline;

    Uint32 readOnlyResourceCount =
        computePipeline->readOnlyStorageTextureCount +
        computePipeline->readOnlyStorageBufferCount;

    Uint32 readWriteResourceCount =
        computePipeline->readWriteStorageTextureCount +
        computePipeline->readWriteStorageBufferCount;

    ID3D11Buffer *nullBuf = NULL;
    Uint32 offsetInConstants, blockSizeInConstants, i;

    if (commandBuffer->needComputeUAVBind) {
        ID3D11DeviceContext_CSSetUnorderedAccessViews(
            commandBuffer->context,
            0,
            readWriteResourceCount,
            commandBuffer->computeUnorderedAccessViews,
            NULL);

        commandBuffer->needComputeUAVBind = SDL_FALSE;
    }

    if (commandBuffer->needComputeSRVBind) {
        ID3D11DeviceContext_CSSetShaderResources(
            commandBuffer->context,
            0,
            readOnlyResourceCount,
            commandBuffer->computeShaderResourceViews);

        commandBuffer->needComputeSRVBind = SDL_FALSE;
    }

    if (commandBuffer->needComputeUniformBufferBind) {
        for (i = 0; i < computePipeline->uniformBufferCount; i += 1) {
            /* stupid workaround for god awful D3D11 drivers
             * see: https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1#calling-vssetconstantbuffers1-with-command-list-emulation
             */
            ID3D11DeviceContext1_CSSetConstantBuffers(
                commandBuffer->context,
                i,
                1,
                &nullBuf);

            offsetInConstants = commandBuffer->computeUniformBuffers[i]->drawOffset / 16;
            blockSizeInConstants = commandBuffer->computeUniformBuffers[i]->currentBlockSize / 16;

            ID3D11DeviceContext1_CSSetConstantBuffers1(
                commandBuffer->context,
                i,
                1,
                &commandBuffer->computeUniformBuffers[i]->buffer,
                &offsetInConstants,
                &blockSizeInConstants);

            D3D11_INTERNAL_TrackUniformBuffer(
                commandBuffer,
                commandBuffer->computeUniformBuffers[i]);
        }
        commandBuffer->needComputeUniformBufferBind = SDL_FALSE;
    }
}

static void D3D11_DispatchCompute(
    SDL_GpuCommandBuffer *commandBuffer,
    Uint32 groupCountX,
    Uint32 groupCountY,
    Uint32 groupCountZ)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11_INTERNAL_BindComputeResources(d3d11CommandBuffer);

    ID3D11DeviceContext_Dispatch(
        d3d11CommandBuffer->context,
        groupCountX,
        groupCountY,
        groupCountZ);
}

static void D3D11_EndComputePass(
    SDL_GpuCommandBuffer *commandBuffer)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;

    /* reset UAV slots to avoid NULL set behavior */
    /* https://learn.microsoft.com/en-us/windows/win32/api/d3d11/nf-d3d11-id3d11devicecontext-cssetshaderresources */
    ID3D11DeviceContext_CSSetUnorderedAccessViews(
        d3d11CommandBuffer->context,
        0,
        MAX_STORAGE_TEXTURES_PER_STAGE + MAX_STORAGE_BUFFERS_PER_STAGE,
        nullUAVs,
        NULL);

    d3d11CommandBuffer->computePipeline = NULL;
}

/* Fence Cleanup */

static void D3D11_INTERNAL_ReleaseFenceToPool(
    D3D11Renderer *renderer,
    D3D11Fence *fence)
{
    SDL_LockMutex(renderer->fenceLock);

    if (renderer->availableFenceCount == renderer->availableFenceCapacity) {
        renderer->availableFenceCapacity *= 2;
        renderer->availableFences = SDL_realloc(
            renderer->availableFences,
            renderer->availableFenceCapacity * sizeof(D3D11Fence *));
    }
    renderer->availableFences[renderer->availableFenceCount] = fence;
    renderer->availableFenceCount += 1;

    SDL_UnlockMutex(renderer->fenceLock);
}

static void D3D11_ReleaseFence(
    SDL_GpuRenderer *driverData,
    SDL_GpuFence *fence)
{
    D3D11Fence *d3d11Fence = (D3D11Fence *)fence;

    if (SDL_AtomicDecRef(&d3d11Fence->referenceCount)) {
        D3D11_INTERNAL_ReleaseFenceToPool(
            (D3D11Renderer *)driverData,
            d3d11Fence);
    }
}

/* Cleanup */

/* D3D11 does not provide a deferred texture-to-buffer copy operation,
 * so instead of the transfer buffer containing an actual D3D11 buffer,
 * the transfer buffer data is just a malloc'd pointer.
 * In the download operation we copy data to a staging resource, and
 * wait until the command buffer has finished executing to map the staging resource.
 */

static void D3D11_INTERNAL_MapAndCopyBufferDownload(
    D3D11Renderer *renderer,
    D3D11TransferBuffer *transferBuffer,
    D3D11BufferDownload *bufferDownload)
{
    D3D11_MAPPED_SUBRESOURCE subres;
    HRESULT res;

    SDL_LockMutex(renderer->contextLock);
    res = ID3D11DeviceContext_Map(
        renderer->immediateContext,
        (ID3D11Resource *)bufferDownload->stagingBuffer,
        0,
        D3D11_MAP_READ,
        0,
        &subres);
    ERROR_CHECK_RETURN("Failed to map staging buffer", )

    SDL_memcpy(
        ((Uint8 *)transferBuffer->data) + bufferDownload->dstOffset,
        ((Uint8 *)subres.pData),
        bufferDownload->size);

    ID3D11DeviceContext_Unmap(
        renderer->immediateContext,
        (ID3D11Resource *)bufferDownload->stagingBuffer,
        0);
    SDL_UnlockMutex(renderer->contextLock);

    ID3D11Buffer_Release(bufferDownload->stagingBuffer);
}

static void D3D11_INTERNAL_MapAndCopyTextureDownload(
    D3D11Renderer *renderer,
    D3D11TransferBuffer *transferBuffer,
    D3D11TextureDownload *textureDownload)
{
    D3D11_MAPPED_SUBRESOURCE subres;
    HRESULT res;
    Uint32 dataPtrOffset;
    Uint32 depth, row;

    SDL_LockMutex(renderer->contextLock);
    res = ID3D11DeviceContext_Map(
        renderer->immediateContext,
        (ID3D11Resource *)textureDownload->stagingTexture,
        0,
        D3D11_MAP_READ,
        0,
        &subres);
    ERROR_CHECK_RETURN("Could not map staging textre", )

    for (depth = 0; depth < textureDownload->depth; depth += 1) {
        dataPtrOffset = textureDownload->bufferOffset + (depth * textureDownload->bytesPerDepthSlice);

        for (row = 0; row < textureDownload->height; row += 1) {
            SDL_memcpy(
                transferBuffer->data + dataPtrOffset,
                (Uint8 *)subres.pData + (depth * subres.DepthPitch) + (row * subres.RowPitch),
                textureDownload->bytesPerRow);
            dataPtrOffset += textureDownload->bytesPerRow;
        }
    }

    ID3D11DeviceContext_Unmap(
        renderer->immediateContext,
        textureDownload->stagingTexture,
        0);

    SDL_UnlockMutex(renderer->contextLock);

    ID3D11Resource_Release(textureDownload->stagingTexture);
}

static void D3D11_INTERNAL_CleanCommandBuffer(
    D3D11Renderer *renderer,
    D3D11CommandBuffer *commandBuffer)
{
    Uint32 i, j;

    /* Perform deferred download map and copy */

    for (i = 0; i < commandBuffer->usedTransferBufferCount; i += 1) {
        D3D11TransferBuffer *transferBuffer = commandBuffer->usedTransferBuffers[i];

        for (j = 0; j < transferBuffer->bufferDownloadCount; j += 1) {
            D3D11_INTERNAL_MapAndCopyBufferDownload(
                renderer,
                transferBuffer,
                &transferBuffer->bufferDownloads[j]);
        }

        for (j = 0; j < transferBuffer->textureDownloadCount; j += 1) {
            D3D11_INTERNAL_MapAndCopyTextureDownload(
                renderer,
                transferBuffer,
                &transferBuffer->textureDownloads[j]);
        }

        transferBuffer->bufferDownloadCount = 0;
        transferBuffer->textureDownloadCount = 0;
    }

    /* Uniform buffers are now available */

    SDL_LockMutex(renderer->acquireUniformBufferLock);

    for (i = 0; i < commandBuffer->usedUniformBufferCount; i += 1) {
        D3D11_INTERNAL_ReturnUniformBufferToPool(
            renderer,
            commandBuffer->usedUniformBuffers[i]);
    }
    commandBuffer->usedUniformBufferCount = 0;

    SDL_UnlockMutex(renderer->acquireUniformBufferLock);

    /* Reference Counting */

    for (i = 0; i < commandBuffer->usedBufferCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedBuffers[i]->referenceCount);
    }
    commandBuffer->usedBufferCount = 0;

    for (i = 0; i < commandBuffer->usedTransferBufferCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedTransferBuffers[i]->referenceCount);
    }
    commandBuffer->usedTransferBufferCount = 0;

    for (i = 0; i < commandBuffer->usedTextureSubresourceCount; i += 1) {
        (void)SDL_AtomicDecRef(&commandBuffer->usedTextureSubresources[i]->referenceCount);
    }
    commandBuffer->usedTextureSubresourceCount = 0;

    /* Reset presentation */
    commandBuffer->windowDataCount = 0;

    /* The fence is now available (unless SubmitAndAcquireFence was called) */
    if (commandBuffer->autoReleaseFence) {
        D3D11_ReleaseFence(
            (SDL_GpuRenderer *)renderer,
            (SDL_GpuFence *)commandBuffer->fence);
    }

    /* Return command buffer to pool */
    SDL_LockMutex(renderer->acquireCommandBufferLock);
    if (renderer->availableCommandBufferCount == renderer->availableCommandBufferCapacity) {
        renderer->availableCommandBufferCapacity += 1;
        renderer->availableCommandBuffers = SDL_realloc(
            renderer->availableCommandBuffers,
            renderer->availableCommandBufferCapacity * sizeof(D3D11CommandBuffer *));
    }
    renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = commandBuffer;
    renderer->availableCommandBufferCount += 1;
    SDL_UnlockMutex(renderer->acquireCommandBufferLock);

    /* Remove this command buffer from the submitted list */
    for (i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
        if (renderer->submittedCommandBuffers[i] == commandBuffer) {
            renderer->submittedCommandBuffers[i] = renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount - 1];
            renderer->submittedCommandBufferCount -= 1;
        }
    }
}

static void D3D11_INTERNAL_PerformPendingDestroys(
    D3D11Renderer *renderer)
{
    Sint32 referenceCount = 0;
    Sint32 i;
    Uint32 j, k;

    for (i = renderer->transferBufferContainersToDestroyCount - 1; i >= 0; i -= 1) {
        referenceCount = 0;
        for (j = 0; j < renderer->transferBufferContainersToDestroy[i]->bufferCount; j += 1) {
            referenceCount += SDL_AtomicGet(&renderer->transferBufferContainersToDestroy[i]->buffers[j]->referenceCount);
        }

        if (referenceCount == 0) {
            D3D11_INTERNAL_DestroyTransferBufferContainer(
                renderer->transferBufferContainersToDestroy[i]);

            renderer->transferBufferContainersToDestroy[i] = renderer->transferBufferContainersToDestroy[renderer->transferBufferContainersToDestroyCount - 1];
            renderer->transferBufferContainersToDestroyCount -= 1;
        }
    }

    for (i = renderer->bufferContainersToDestroyCount - 1; i >= 0; i -= 1) {
        referenceCount = 0;
        for (j = 0; j < renderer->bufferContainersToDestroy[i]->bufferCount; j += 1) {
            referenceCount += SDL_AtomicGet(&renderer->bufferContainersToDestroy[i]->buffers[j]->referenceCount);
        }

        if (referenceCount == 0) {
            D3D11_INTERNAL_DestroyBufferContainer(
                renderer->bufferContainersToDestroy[i]);

            renderer->bufferContainersToDestroy[i] = renderer->bufferContainersToDestroy[renderer->bufferContainersToDestroyCount - 1];
            renderer->bufferContainersToDestroyCount -= 1;
        }
    }

    for (i = renderer->textureContainersToDestroyCount - 1; i >= 0; i -= 1) {
        referenceCount = 0;
        for (j = 0; j < renderer->textureContainersToDestroy[i]->textureCount; j += 1) {
            for (k = 0; k < renderer->textureContainersToDestroy[i]->textures[j]->subresourceCount; k += 1) {
                referenceCount += SDL_AtomicGet(&renderer->textureContainersToDestroy[i]->textures[j]->subresources[k].referenceCount);
            }
        }

        if (referenceCount == 0) {
            D3D11_INTERNAL_DestroyTextureContainer(
                renderer->textureContainersToDestroy[i]);

            renderer->textureContainersToDestroy[i] = renderer->textureContainersToDestroy[renderer->textureContainersToDestroyCount - 1];
            renderer->textureContainersToDestroyCount -= 1;
        }
    }
}

/* Fences */

static void D3D11_INTERNAL_WaitForFence(
    D3D11Renderer *renderer,
    D3D11Fence *fence)
{
    BOOL queryData;
    HRESULT res;

    SDL_LockMutex(renderer->contextLock);

    do {
        res = ID3D11DeviceContext_GetData(
            renderer->immediateContext,
            (ID3D11Asynchronous *)fence->handle,
            &queryData,
            sizeof(queryData),
            0);
    } while (res != S_OK); /* Spin until we get a result back... */

    SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_WaitForFences(
    SDL_GpuRenderer *driverData,
    SDL_bool waitAll,
    SDL_GpuFence **pFences,
    Uint32 fenceCount)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11Fence *fence;
    BOOL queryData;
    HRESULT res = S_FALSE;

    if (waitAll) {
        for (Uint32 i = 0; i < fenceCount; i += 1) {
            fence = (D3D11Fence *)pFences[i];
            D3D11_INTERNAL_WaitForFence(renderer, fence);
        }
    } else {
        SDL_LockMutex(renderer->contextLock);

        while (res != S_OK) {
            for (Uint32 i = 0; i < fenceCount; i += 1) {
                fence = (D3D11Fence *)pFences[i];
                res = ID3D11DeviceContext_GetData(
                    renderer->immediateContext,
                    (ID3D11Asynchronous *)fence->handle,
                    &queryData,
                    sizeof(queryData),
                    0);
                if (res == S_OK) {
                    break;
                }
            }
        }

        SDL_UnlockMutex(renderer->contextLock);
    }

    SDL_LockMutex(renderer->contextLock);

    /* Check if we can perform any cleanups */
    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        res = ID3D11DeviceContext_GetData(
            renderer->immediateContext,
            (ID3D11Asynchronous *)renderer->submittedCommandBuffers[i]->fence->handle,
            &queryData,
            sizeof(queryData),
            0);
        if (res == S_OK) {
            D3D11_INTERNAL_CleanCommandBuffer(
                renderer,
                renderer->submittedCommandBuffers[i]);
        }
    }

    D3D11_INTERNAL_PerformPendingDestroys(renderer);

    SDL_UnlockMutex(renderer->contextLock);
}

static SDL_bool D3D11_QueryFence(
    SDL_GpuRenderer *driverData,
    SDL_GpuFence *fence)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11Fence *d3d11Fence = (D3D11Fence *)fence;
    BOOL queryData;
    HRESULT res;

    SDL_LockMutex(renderer->contextLock);

    res = ID3D11DeviceContext_GetData(
        renderer->immediateContext,
        (ID3D11Asynchronous *)d3d11Fence->handle,
        &queryData,
        sizeof(queryData),
        0);

    SDL_UnlockMutex(renderer->contextLock);

    return res == S_OK;
}

/* Window and Swapchain Management */

static D3D11WindowData *D3D11_INTERNAL_FetchWindowData(
    SDL_Window *window)
{
    SDL_PropertiesID properties = SDL_GetWindowProperties(window);
    return (D3D11WindowData *)SDL_GetProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

static SDL_bool D3D11_INTERNAL_InitializeSwapchainTexture(
    D3D11Renderer *renderer,
    IDXGISwapChain *swapchain,
    DXGI_FORMAT swapchainFormat,
    DXGI_FORMAT rtvFormat,
    D3D11Texture *pTexture)
{
    ID3D11Texture2D *swapchainTexture;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
    D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
    D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
    D3D11_TEXTURE2D_DESC textureDesc;
    ID3D11ShaderResourceView *srv;
    ID3D11RenderTargetView *rtv;
    ID3D11UnorderedAccessView *uav;
    HRESULT res;

    /* Clear all the texture data */
    SDL_zerop(pTexture);

    /* Grab the buffer from the swapchain */
    res = IDXGISwapChain_GetBuffer(
        swapchain,
        0,
        &D3D_IID_ID3D11Texture2D,
        (void **)&swapchainTexture);
    ERROR_CHECK_RETURN("Could not get buffer from swapchain!", 0);

    /* Create the SRV for the swapchain */
    srvDesc.Format = swapchainFormat;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    srvDesc.Texture2D.MostDetailedMip = 0;

    res = ID3D11Device_CreateShaderResourceView(
        renderer->device,
        (ID3D11Resource *)swapchainTexture,
        &srvDesc,
        &srv);
    if (FAILED(res)) {
        ID3D11Texture2D_Release(swapchainTexture);
        D3D11_INTERNAL_LogError(renderer->device, "Swapchain SRV creation failed", res);
        return SDL_FALSE;
    }

    /* Create the RTV for the swapchain */
    rtvDesc.Format = rtvFormat;
    rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Texture2D.MipSlice = 0;

    res = ID3D11Device_CreateRenderTargetView(
        renderer->device,
        (ID3D11Resource *)swapchainTexture,
        &rtvDesc,
        &rtv);
    if (FAILED(res)) {
        ID3D11ShaderResourceView_Release(srv);
        ID3D11Texture2D_Release(swapchainTexture);
        D3D11_INTERNAL_LogError(renderer->device, "Swapchain RTV creation failed", res);
        return SDL_FALSE;
    }

    uavDesc.Format = swapchainFormat;
    uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    uavDesc.Texture2D.MipSlice = 0;

    res = ID3D11Device_CreateUnorderedAccessView(
        renderer->device,
        (ID3D11Resource *)swapchainTexture,
        &uavDesc,
        &uav);
    if (FAILED(res)) {
        ID3D11ShaderResourceView_Release(srv);
        ID3D11RenderTargetView_Release(rtv);
        ID3D11Texture2D_Release(swapchainTexture);
        D3D11_INTERNAL_LogError(renderer->device, "Swapchain UAV creation failed", res);
        return SDL_FALSE;
    }

    /* Fill out the texture struct */
    pTexture->handle = NULL; /* This will be set in AcquireSwapchainTexture. */
    pTexture->shaderView = srv;
    pTexture->subresourceCount = 1;
    pTexture->subresources = SDL_malloc(sizeof(D3D11TextureSubresource));
    pTexture->subresources[0].colorTargetView = rtv;
    pTexture->subresources[0].srv = srv;
    pTexture->subresources[0].uav = uav;
    pTexture->subresources[0].depthStencilTargetView = NULL;
    pTexture->subresources[0].msaaHandle = NULL;
    pTexture->subresources[0].msaaTargetView = NULL;
    pTexture->subresources[0].layer = 0;
    pTexture->subresources[0].level = 0;
    pTexture->subresources[0].index = 0;
    pTexture->subresources[0].parent = pTexture;
    SDL_AtomicSet(&pTexture->subresources[0].referenceCount, 0);

    ID3D11Texture2D_GetDesc(swapchainTexture, &textureDesc);
    pTexture->levelCount = textureDesc.MipLevels;
    pTexture->width = textureDesc.Width;
    pTexture->height = textureDesc.Height;
    pTexture->depth = 1;
    pTexture->isCube = 0;
    pTexture->isRenderTarget = 1;

    /* Cleanup */
    ID3D11Texture2D_Release(swapchainTexture);

    return SDL_TRUE;
}

static SDL_bool D3D11_INTERNAL_CreateSwapchain(
    D3D11Renderer *renderer,
    D3D11WindowData *windowData,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode)
{
    HWND dxgiHandle;
    int width, height;
    Uint32 i;
    DXGI_SWAP_CHAIN_DESC swapchainDesc;
    DXGI_FORMAT swapchainFormat;
    IDXGIFactory1 *pParent;
    IDXGISwapChain *swapchain;
    IDXGISwapChain3 *swapchain3;
    Uint32 colorSpaceSupport;
    HRESULT res;

    /* Get the DXGI handle */
#ifdef _WIN32
    dxgiHandle = (HWND)SDL_GetProperty(SDL_GetWindowProperties(windowData->window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);
#else
    dxgiHandle = (HWND)windowData->window;
#endif

    /* Get the window size */
    SDL_GetWindowSize(windowData->window, &width, &height);

    swapchainFormat = SwapchainCompositionToTextureFormat[swapchainComposition];

    /* Initialize the swapchain buffer descriptor */
    swapchainDesc.BufferDesc.Width = 0;
    swapchainDesc.BufferDesc.Height = 0;
    swapchainDesc.BufferDesc.RefreshRate.Numerator = 0;
    swapchainDesc.BufferDesc.RefreshRate.Denominator = 0;
    swapchainDesc.BufferDesc.Format = swapchainFormat;
    swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

    /* Initialize the rest of the swapchain descriptor */
    swapchainDesc.SampleDesc.Count = 1;
    swapchainDesc.SampleDesc.Quality = 0;
    swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT;
    swapchainDesc.BufferCount = 2;
    swapchainDesc.OutputWindow = dxgiHandle;
    swapchainDesc.Windowed = 1;

    if (renderer->supportsTearing) {
        swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        /* We know this is supported because tearing support implies DXGI 1.5+ */
        swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    } else {
        swapchainDesc.Flags = 0;
        swapchainDesc.SwapEffect = (renderer->supportsFlipDiscard ? DXGI_SWAP_EFFECT_FLIP_DISCARD : DXGI_SWAP_EFFECT_DISCARD);
    }

    /* Create the swapchain! */
    res = IDXGIFactory1_CreateSwapChain(
        (IDXGIFactory1 *)renderer->factory,
        (IUnknown *)renderer->device,
        &swapchainDesc,
        &swapchain);
    ERROR_CHECK_RETURN("Could not create swapchain", 0);

    /*
     * The swapchain's parent is a separate factory from the factory that
     * we used to create the swapchain, and only that parent can be used to
     * set the window association. Trying to set an association on our factory
     * will silently fail and doesn't even verify arguments or return errors.
     * See https://gamedev.net/forums/topic/634235-dxgidisabling-altenter/4999955/
     */
    res = IDXGISwapChain_GetParent(
        swapchain,
        &D3D_IID_IDXGIFactory1,
        (void **)&pParent);
    if (FAILED(res)) {
        SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
            "Could not get swapchain parent! Error Code: " HRESULT_FMT,
            res);
    } else {
        /* Disable DXGI window crap */
        res = IDXGIFactory1_MakeWindowAssociation(
            pParent,
            dxgiHandle,
            DXGI_MWA_NO_WINDOW_CHANGES);
        if (FAILED(res)) {
            SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
                "MakeWindowAssociation failed! Error Code: " HRESULT_FMT,
                res);
        }

        /* We're done with the parent now */
        IDXGIFactory1_Release(pParent);
    }
    /* Initialize the swapchain data */
    windowData->swapchain = swapchain;
    windowData->presentMode = presentMode;
    windowData->swapchainComposition = swapchainComposition;
    windowData->swapchainFormat = swapchainFormat;
    windowData->swapchainColorSpace = SwapchainCompositionToColorSpace[swapchainComposition];
    windowData->frameCounter = 0;

    for (i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
        windowData->inFlightFences[i] = NULL;
    }

    if (SUCCEEDED(IDXGISwapChain3_QueryInterface(
            swapchain,
            &D3D_IID_IDXGISwapChain3,
            (void **)&swapchain3))) {
        IDXGISwapChain3_CheckColorSpaceSupport(
            swapchain3,
            windowData->swapchainColorSpace,
            &colorSpaceSupport);

        if (!(colorSpaceSupport & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT)) {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Requested colorspace is unsupported!");
            return SDL_FALSE;
        }

        IDXGISwapChain3_SetColorSpace1(
            swapchain3,
            windowData->swapchainColorSpace);

        IDXGISwapChain3_Release(swapchain3);
    } else {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "DXGI 1.4 not supported, cannot use colorspace other than SDL_GPU_COLORSPACE_NONLINEAR_SRGB!");
        return SDL_FALSE;
    }

    /* If a you are using a FLIP model format you can't create the swapchain as DXGI_FORMAT_B8G8R8A8_UNORM_SRGB.
     * You have to create the swapchain as DXGI_FORMAT_B8G8R8A8_UNORM and then set the render target view's format to DXGI_FORMAT_B8G8R8A8_UNORM_SRGB
     */
    if (!D3D11_INTERNAL_InitializeSwapchainTexture(
            renderer,
            swapchain,
            swapchainFormat,
            (swapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : windowData->swapchainFormat,
            &windowData->texture)) {
        IDXGISwapChain_Release(swapchain);
        return SDL_FALSE;
    }

    /* Initialize dummy container */
    SDL_zerop(&windowData->textureContainer);
    windowData->textureContainer.textures = SDL_calloc(1, sizeof(D3D11Texture *));

    return SDL_TRUE;
}

static SDL_bool D3D11_INTERNAL_ResizeSwapchain(
    D3D11Renderer *renderer,
    D3D11WindowData *windowData,
    Sint32 width,
    Sint32 height)
{
    /* Release the old views */
    ID3D11ShaderResourceView_Release(windowData->texture.shaderView);
    ID3D11RenderTargetView_Release(windowData->texture.subresources[0].colorTargetView);
    /* NOTE: subresource srv is same as shaderView, not released */
    ID3D11UnorderedAccessView_Release(windowData->texture.subresources[0].uav);
    SDL_free(windowData->texture.subresources);

    /* Resize the swapchain */
    HRESULT res = IDXGISwapChain_ResizeBuffers(
        windowData->swapchain,
        0, /* Keep buffer count the same */
        width,
        height,
        DXGI_FORMAT_UNKNOWN, /* Keep the old format */
        renderer->supportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
    ERROR_CHECK_RETURN("Could not resize swapchain buffers", 0);

    /* Create the texture object for the swapchain */
    return D3D11_INTERNAL_InitializeSwapchainTexture(
        renderer,
        windowData->swapchain,
        windowData->swapchainFormat,
        (windowData->swapchainComposition == SDL_GPU_SWAPCHAINCOMPOSITION_SDR_LINEAR) ? DXGI_FORMAT_B8G8R8A8_UNORM_SRGB : windowData->swapchainFormat,
        &windowData->texture);
}

static SDL_bool D3D11_SupportsSwapchainComposition(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    DXGI_FORMAT format;
    Uint32 formatSupport = 0;
    HRESULT res;

    format = SwapchainCompositionToTextureFormat[swapchainComposition];

    res = ID3D11Device_CheckFormatSupport(
        renderer->device,
        format,
        &formatSupport);
    if (FAILED(res)) {
        /* Format is apparently unknown */
        return SDL_FALSE;
    }

    return (formatSupport & D3D11_FORMAT_SUPPORT_DISPLAY);
}

static SDL_bool D3D11_SupportsPresentMode(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuPresentMode presentMode)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    (void)window; /* used by other backends */
    switch (presentMode) {
    case SDL_GPU_PRESENTMODE_IMMEDIATE:
    case SDL_GPU_PRESENTMODE_VSYNC:
        return SDL_TRUE;
    case SDL_GPU_PRESENTMODE_MAILBOX:
        return renderer->supportsFlipDiscard;
    }
    SDL_assert(!"Unrecognized present mode");
    return SDL_FALSE;
}

static SDL_bool D3D11_ClaimWindow(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11WindowData *windowData = D3D11_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        windowData = (D3D11WindowData *)SDL_malloc(sizeof(D3D11WindowData));
        windowData->window = window;

        if (D3D11_INTERNAL_CreateSwapchain(renderer, windowData, swapchainComposition, presentMode)) {
            SDL_SetProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA, windowData);

            SDL_LockMutex(renderer->windowLock);

            if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity) {
                renderer->claimedWindowCapacity *= 2;
                renderer->claimedWindows = SDL_realloc(
                    renderer->claimedWindows,
                    renderer->claimedWindowCapacity * sizeof(D3D11WindowData *));
            }
            renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
            renderer->claimedWindowCount += 1;

            SDL_UnlockMutex(renderer->windowLock);

            return SDL_TRUE;
        } else {
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create swapchain, failed to claim window!");
            SDL_free(windowData);
            return SDL_FALSE;
        }
    } else {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Window already claimed!");
        return SDL_FALSE;
    }
}

static void D3D11_INTERNAL_DestroySwapchain(
    D3D11Renderer *renderer,
    D3D11WindowData *windowData)
{
    Uint32 i;

    D3D11_Wait((SDL_GpuRenderer *)renderer);

    ID3D11ShaderResourceView_Release(windowData->texture.shaderView);
    ID3D11RenderTargetView_Release(windowData->texture.subresources[0].colorTargetView);
    /* NOTE: subresource srv is same as shaderView, not released */
    ID3D11UnorderedAccessView_Release(windowData->texture.subresources[0].uav);
    SDL_free(windowData->texture.subresources);
    SDL_free(windowData->textureContainer.textures);
    IDXGISwapChain_Release(windowData->swapchain);

    /* DXGI will crash if we don't flush deferred swapchain destruction */
    SDL_LockMutex(renderer->contextLock);
    ID3D11DeviceContext_ClearState(renderer->immediateContext);
    ID3D11DeviceContext_Flush(renderer->immediateContext);
    SDL_UnlockMutex(renderer->contextLock);

    windowData->swapchain = NULL;

    for (i = 0; i < MAX_FRAMES_IN_FLIGHT; i += 1) {
        if (windowData->inFlightFences[i] != NULL) {
            D3D11_ReleaseFence(
                (SDL_GpuRenderer *)renderer,
                (SDL_GpuFence *)windowData->inFlightFences[i]);
        }
    }
}

static void D3D11_UnclaimWindow(
    SDL_GpuRenderer *driverData,
    SDL_Window *window)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11WindowData *windowData = D3D11_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        return;
    }

    D3D11_INTERNAL_DestroySwapchain(
        renderer,
        windowData);

    SDL_LockMutex(renderer->windowLock);
    for (Uint32 i = 0; i < renderer->claimedWindowCount; i += 1) {
        if (renderer->claimedWindows[i]->window == window) {
            renderer->claimedWindows[i] = renderer->claimedWindows[renderer->claimedWindowCount - 1];
            renderer->claimedWindowCount -= 1;
            break;
        }
    }
    SDL_UnlockMutex(renderer->windowLock);

    SDL_free(windowData);

    SDL_ClearProperty(SDL_GetWindowProperties(window), WINDOW_PROPERTY_DATA);
}

static SDL_GpuTexture *D3D11_AcquireSwapchainTexture(
    SDL_GpuCommandBuffer *commandBuffer,
    SDL_Window *window,
    Uint32 *pWidth,
    Uint32 *pHeight)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    D3D11WindowData *windowData;
    DXGI_SWAP_CHAIN_DESC swapchainDesc;
    int w, h;
    HRESULT res;

    windowData = D3D11_INTERNAL_FetchWindowData(window);
    if (windowData == NULL) {
        return NULL;
    }

    /* Check for window size changes and resize the swapchain if needed. */
    IDXGISwapChain_GetDesc(windowData->swapchain, &swapchainDesc);
    SDL_GetWindowSize(window, &w, &h);

    if (w != swapchainDesc.BufferDesc.Width || h != swapchainDesc.BufferDesc.Height) {
        res = D3D11_INTERNAL_ResizeSwapchain(
            renderer,
            windowData,
            w,
            h);
        ERROR_CHECK_RETURN("Could not resize swapchain", NULL);
    }

    if (windowData->inFlightFences[windowData->frameCounter] != NULL) {
        if (windowData->presentMode == SDL_GPU_PRESENTMODE_VSYNC) {
            /* In VSYNC mode, block until the least recent presented frame is done */
            D3D11_WaitForFences(
                (SDL_GpuRenderer *)renderer,
                SDL_TRUE,
                (SDL_GpuFence **)&windowData->inFlightFences[windowData->frameCounter],
                1);
        } else {
            if (!D3D11_QueryFence(
                    (SDL_GpuRenderer *)d3d11CommandBuffer->renderer,
                    (SDL_GpuFence *)windowData->inFlightFences[windowData->frameCounter])) {
                /*
                 * In MAILBOX or IMMEDIATE mode, if the least recent fence is not signaled,
                 * return NULL to indicate that rendering should be skipped
                 */
                return NULL;
            }
        }

        D3D11_ReleaseFence(
            (SDL_GpuRenderer *)d3d11CommandBuffer->renderer,
            (SDL_GpuFence *)windowData->inFlightFences[windowData->frameCounter]);

        windowData->inFlightFences[windowData->frameCounter] = NULL;
    }

    /* Set the handle on the windowData texture data. */
    res = IDXGISwapChain_GetBuffer(
        windowData->swapchain,
        0,
        &D3D_IID_ID3D11Texture2D,
        (void **)&windowData->texture.handle);
    ERROR_CHECK_RETURN("Could not acquire swapchain!", NULL);

    /* Send the dimensions to the out parameters. */
    *pWidth = windowData->texture.width;
    *pHeight = windowData->texture.height;

    /* Set up the texture container */
    windowData->textureContainer.canBeCycled = 0;
    windowData->textureContainer.activeTexture = &windowData->texture;
    windowData->textureContainer.textures[0] = &windowData->texture;
    windowData->textureContainer.textureCapacity = 1;
    windowData->textureContainer.textureCount = 1;

    /* Set up presentation */
    if (d3d11CommandBuffer->windowDataCount == d3d11CommandBuffer->windowDataCapacity) {
        d3d11CommandBuffer->windowDataCapacity += 1;
        d3d11CommandBuffer->windowDatas = SDL_realloc(
            d3d11CommandBuffer->windowDatas,
            d3d11CommandBuffer->windowDataCapacity * sizeof(D3D11WindowData *));
    }
    d3d11CommandBuffer->windowDatas[d3d11CommandBuffer->windowDataCount] = windowData;
    d3d11CommandBuffer->windowDataCount += 1;

    /* Return the swapchain texture */
    return (SDL_GpuTexture *)&windowData->textureContainer;
}

static SDL_GpuTextureFormat D3D11_GetSwapchainTextureFormat(
    SDL_GpuRenderer *driverData,
    SDL_Window *window)
{
    D3D11WindowData *windowData = D3D11_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot get swapchain format, window has not been claimed!");
        return 0;
    }

    switch (windowData->swapchainFormat) {
    case DXGI_FORMAT_B8G8R8A8_UNORM:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8;

    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        return SDL_GPU_TEXTUREFORMAT_B8G8R8A8_SRGB;

    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        return SDL_GPU_TEXTUREFORMAT_R16G16B16A16_SFLOAT;

    case DXGI_FORMAT_R10G10B10A2_UNORM:
        return SDL_GPU_TEXTUREFORMAT_R10G10B10A2;

    default:
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unrecognized swapchain format!");
        return 0;
    }
}

static SDL_bool D3D11_SetSwapchainParameters(
    SDL_GpuRenderer *driverData,
    SDL_Window *window,
    SDL_GpuSwapchainComposition swapchainComposition,
    SDL_GpuPresentMode presentMode)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11WindowData *windowData = D3D11_INTERNAL_FetchWindowData(window);

    if (windowData == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot set swapchain parameters on unclaimed window!");
        return SDL_FALSE;
    }

    if (!D3D11_SupportsSwapchainComposition(driverData, window, swapchainComposition)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Swapchain composition not supported!");
        return SDL_FALSE;
    }

    if (!D3D11_SupportsPresentMode(driverData, window, presentMode)) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Present mode not supported!");
        return SDL_FALSE;
    }

    if (
        swapchainComposition != windowData->swapchainComposition ||
        presentMode != windowData->presentMode) {
        D3D11_Wait(driverData);

        /* Recreate the swapchain */
        D3D11_INTERNAL_DestroySwapchain(
            renderer,
            windowData);

        return D3D11_INTERNAL_CreateSwapchain(
            renderer,
            windowData,
            swapchainComposition,
            presentMode);
    }

    return SDL_TRUE;
}

/* Submission */

static void D3D11_Submit(
    SDL_GpuCommandBuffer *commandBuffer)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Renderer *renderer = (D3D11Renderer *)d3d11CommandBuffer->renderer;
    ID3D11CommandList *commandList;
    HRESULT res;

    /* Unmap uniform buffers */

    for (Uint32 i = 0; i < MAX_UNIFORM_BUFFERS_PER_STAGE; i += 1) {
        if (d3d11CommandBuffer->vertexUniformBuffers[i] != NULL) {
            ID3D11DeviceContext_Unmap(
                d3d11CommandBuffer->context,
                (ID3D11Resource *)d3d11CommandBuffer->vertexUniformBuffers[i]->buffer,
                0);
        }

        if (d3d11CommandBuffer->fragmentUniformBuffers[i] != NULL) {
            ID3D11DeviceContext_Unmap(
                d3d11CommandBuffer->context,
                (ID3D11Resource *)d3d11CommandBuffer->fragmentUniformBuffers[i]->buffer,
                0);
        }

        if (d3d11CommandBuffer->computeUniformBuffers[i] != NULL) {
            ID3D11DeviceContext_Unmap(
                d3d11CommandBuffer->context,
                (ID3D11Resource *)d3d11CommandBuffer->computeUniformBuffers[i]->buffer,
                0);
        }
    }

    SDL_LockMutex(renderer->contextLock);

    /* Notify the command buffer completion query that we have completed recording */
    ID3D11DeviceContext_End(
        renderer->immediateContext,
        (ID3D11Asynchronous *)d3d11CommandBuffer->fence->handle);

    /* Serialize the commands into the command list */
    res = ID3D11DeviceContext_FinishCommandList(
        d3d11CommandBuffer->context,
        0,
        &commandList);
    ERROR_CHECK("Could not finish command list recording!");

    /* Submit the command list to the immediate context */
    ID3D11DeviceContext_ExecuteCommandList(
        renderer->immediateContext,
        commandList,
        0);
    ID3D11CommandList_Release(commandList);

    /* Mark the command buffer as submitted */
    if (renderer->submittedCommandBufferCount >= renderer->submittedCommandBufferCapacity) {
        renderer->submittedCommandBufferCapacity = renderer->submittedCommandBufferCount + 1;

        renderer->submittedCommandBuffers = SDL_realloc(
            renderer->submittedCommandBuffers,
            sizeof(D3D11CommandBuffer *) * renderer->submittedCommandBufferCapacity);
    }

    renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount] = d3d11CommandBuffer;
    renderer->submittedCommandBufferCount += 1;

    /* Present, if applicable */
    for (Uint32 i = 0; i < d3d11CommandBuffer->windowDataCount; i += 1) {
        D3D11WindowData *windowData = d3d11CommandBuffer->windowDatas[i];

        Uint32 syncInterval = 1;
        if (windowData->presentMode == SDL_GPU_PRESENTMODE_IMMEDIATE ||
            (renderer->supportsFlipDiscard && windowData->presentMode == SDL_GPU_PRESENTMODE_MAILBOX)) {
            syncInterval = 0;
        }

        Uint32 presentFlags = 0;
        if (renderer->supportsTearing &&
            windowData->presentMode == SDL_GPU_PRESENTMODE_IMMEDIATE) {
            presentFlags = DXGI_PRESENT_ALLOW_TEARING;
        }

        IDXGISwapChain_Present(
            windowData->swapchain,
            syncInterval,
            presentFlags);

        ID3D11Texture2D_Release(windowData->texture.handle);

        windowData->inFlightFences[windowData->frameCounter] = d3d11CommandBuffer->fence;

        (void)SDL_AtomicIncRef(&d3d11CommandBuffer->fence->referenceCount);

        windowData->frameCounter = (windowData->frameCounter + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    /* Check if we can perform any cleanups */
    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        BOOL queryData;
        res = ID3D11DeviceContext_GetData(
            renderer->immediateContext,
            (ID3D11Asynchronous *)renderer->submittedCommandBuffers[i]->fence->handle,
            &queryData,
            sizeof(queryData),
            0);
        if (res == S_OK) {
            D3D11_INTERNAL_CleanCommandBuffer(
                renderer,
                renderer->submittedCommandBuffers[i]);
        }
    }

    D3D11_INTERNAL_PerformPendingDestroys(renderer);

    SDL_UnlockMutex(renderer->contextLock);
}

static SDL_GpuFence *D3D11_SubmitAndAcquireFence(
    SDL_GpuCommandBuffer *commandBuffer)
{
    D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer *)commandBuffer;
    D3D11Fence *fence = d3d11CommandBuffer->fence;

    d3d11CommandBuffer->autoReleaseFence = 0;
    D3D11_Submit(commandBuffer);

    return (SDL_GpuFence *)fence;
}

static void D3D11_Wait(
    SDL_GpuRenderer *driverData)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    D3D11CommandBuffer *commandBuffer;

    /*
     * Wait for all submitted command buffers to complete.
     * Sort of equivalent to vkDeviceWaitIdle.
     */
    for (Uint32 i = 0; i < renderer->submittedCommandBufferCount; i += 1) {
        D3D11_INTERNAL_WaitForFence(
            renderer,
            renderer->submittedCommandBuffers[i]->fence);
    }

    SDL_LockMutex(renderer->contextLock); /* This effectively acts as a lock around submittedCommandBuffers */

    for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1) {
        commandBuffer = renderer->submittedCommandBuffers[i];
        D3D11_INTERNAL_CleanCommandBuffer(renderer, commandBuffer);
    }

    D3D11_INTERNAL_PerformPendingDestroys(renderer);

    SDL_UnlockMutex(renderer->contextLock);
}

/* Format Info */

static SDL_bool D3D11_IsTextureFormatSupported(
    SDL_GpuRenderer *driverData,
    SDL_GpuTextureFormat format,
    SDL_GpuTextureType type,
    SDL_GpuTextureUsageFlags usage)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;
    DXGI_FORMAT dxgiFormat = SDLToD3D11_TextureFormat[format];
    DXGI_FORMAT typelessFormat = D3D11_INTERNAL_GetTypelessFormat(dxgiFormat);
    UINT formatSupport, sampleableFormatSupport;
    HRESULT res;

    res = ID3D11Device_CheckFormatSupport(
        renderer->device,
        dxgiFormat,
        &formatSupport);
    if (FAILED(res)) {
        /* Format is apparently unknown */
        return SDL_FALSE;
    }

    /* Depth textures are stored as typeless textures, but interpreted as color textures for sampling.
     * In order to get supported usages for both interpretations, we have to do this.
     */
    if (typelessFormat != DXGI_FORMAT_UNKNOWN) {
        res = ID3D11Device_CheckFormatSupport(
            renderer->device,
            D3D11_INTERNAL_GetSampleableFormat(typelessFormat),
            &sampleableFormatSupport);
        if (SUCCEEDED(res)) {
            formatSupport |= sampleableFormatSupport;
        }
    }

    /* Is the texture type supported? */
    if (type == SDL_GPU_TEXTURETYPE_2D && !(formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE2D)) {
        return SDL_FALSE;
    }
    if (type == SDL_GPU_TEXTURETYPE_3D && !(formatSupport & D3D11_FORMAT_SUPPORT_TEXTURE3D)) {
        return SDL_FALSE;
    }
    if (type == SDL_GPU_TEXTURETYPE_CUBE && !(formatSupport & D3D11_FORMAT_SUPPORT_TEXTURECUBE)) {
        return SDL_FALSE;
    }

    /* Are the usage flags supported? */
    if ((usage & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT) && !(formatSupport & D3D11_FORMAT_SUPPORT_SHADER_SAMPLE)) {
        return SDL_FALSE;
    }
    if ((usage & (SDL_GPU_TEXTUREUSAGE_GRAPHICS_STORAGE_READ_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_READ_BIT | SDL_GPU_TEXTUREUSAGE_COMPUTE_STORAGE_WRITE_BIT)) && !(formatSupport & D3D11_FORMAT_SUPPORT_SHADER_LOAD)) {
        return SDL_FALSE;
    }
    if ((usage & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT) && !(formatSupport & D3D11_FORMAT_SUPPORT_RENDER_TARGET)) {
        return SDL_FALSE;
    }
    if ((usage & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT) && !(formatSupport & D3D11_FORMAT_SUPPORT_DEPTH_STENCIL)) {
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

/* Device Creation */

static SDL_bool D3D11_PrepareDriver(SDL_VideoDevice *_this)
{
    void *d3d11_dll, *dxgi_dll, *d3dcompiler_dll;
    PFN_D3D11_CREATE_DEVICE D3D11CreateDeviceFunc;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1 };
    PFN_CREATE_DXGI_FACTORY1 CreateDXGIFactoryFunc;
    PFN_D3DCOMPILE D3DCompileFunc;
    HRESULT res;

    /* Can we load D3D11? */

    d3d11_dll = SDL_LoadObject(D3D11_DLL);
    if (d3d11_dll == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find " D3D11_DLL);
        return SDL_FALSE;
    }

    D3D11CreateDeviceFunc = (PFN_D3D11_CREATE_DEVICE)SDL_LoadFunction(
        d3d11_dll,
        D3D11_CREATE_DEVICE_FUNC);
    if (D3D11CreateDeviceFunc == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find function " D3D11_CREATE_DEVICE_FUNC " in " D3D11_DLL);
        SDL_UnloadObject(d3d11_dll);
        return SDL_FALSE;
    }

    /* Can we create a device? */

    res = D3D11CreateDeviceFunc(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        levels,
        SDL_arraysize(levels),
        D3D11_SDK_VERSION,
        NULL,
        NULL,
        NULL);

    SDL_UnloadObject(d3d11_dll);

    if (FAILED(res)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not create D3D11Device with feature level 11_0");
        return SDL_FALSE;
    }

    /* Can we load DXGI? */

    dxgi_dll = SDL_LoadObject(DXGI_DLL);
    if (dxgi_dll == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find " DXGI_DLL);
        return SDL_FALSE;
    }

    CreateDXGIFactoryFunc = (PFN_CREATE_DXGI_FACTORY1)SDL_LoadFunction(
        dxgi_dll,
        CREATE_DXGI_FACTORY1_FUNC);
    SDL_UnloadObject(dxgi_dll); /* We're not going to call this function, so we can just unload now. */
    if (CreateDXGIFactoryFunc == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find function " CREATE_DXGI_FACTORY1_FUNC " in " DXGI_DLL);
        return SDL_FALSE;
    }

    /* Can we load D3DCompiler? */

    d3dcompiler_dll = SDL_LoadObject(D3DCOMPILER_DLL);
    if (d3dcompiler_dll == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find " D3DCOMPILER_DLL);
        return SDL_FALSE;
    }

    D3DCompileFunc = (PFN_D3DCOMPILE)SDL_LoadFunction(
        d3dcompiler_dll,
        D3DCOMPILE_FUNC);
    SDL_UnloadObject(d3dcompiler_dll); /* We're not going to call this function, so we can just unload now. */
    if (D3DCompileFunc == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find function D3DCompile in " D3DCOMPILER_DLL);
        return SDL_FALSE;
    }

    return SDL_TRUE;
}

static void D3D11_INTERNAL_TryInitializeDXGIDebug(D3D11Renderer *renderer)
{
    PFN_DXGI_GET_DEBUG_INTERFACE DXGIGetDebugInterfaceFunc;
    HRESULT res;

    renderer->dxgidebug_dll = SDL_LoadObject(DXGIDEBUG_DLL);
    if (renderer->dxgidebug_dll == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not find " DXGIDEBUG_DLL);
        return;
    }

    DXGIGetDebugInterfaceFunc = (PFN_DXGI_GET_DEBUG_INTERFACE)SDL_LoadFunction(
        renderer->dxgidebug_dll,
        DXGI_GET_DEBUG_INTERFACE_FUNC);
    if (DXGIGetDebugInterfaceFunc == NULL) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " DXGI_GET_DEBUG_INTERFACE_FUNC);
        return;
    }

    res = DXGIGetDebugInterfaceFunc(&D3D_IID_IDXGIDebug, (void **)&renderer->dxgiDebug);
    if (FAILED(res)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not get IDXGIDebug interface");
    }

#ifdef HAVE_IDXGIINFOQUEUE
    res = DXGIGetDebugInterfaceFunc(&D3D_IID_IDXGIInfoQueue, (void **)&renderer->dxgiInfoQueue);
    if (FAILED(res)) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not get IDXGIInfoQueue interface");
    }
#endif
}

static void D3D11_INTERNAL_InitBlitPipelines(
    D3D11Renderer *renderer)
{
    SDL_GpuShaderCreateInfo shaderCreateInfo;
    SDL_GpuShader *fullscreenVertexShader;
    SDL_GpuShader *blitFrom2DPixelShader;
    SDL_GpuShader *blitFrom2DArrayPixelShader;
    SDL_GpuGraphicsPipelineCreateInfo blitPipelineCreateInfo;
    SDL_GpuSamplerCreateInfo samplerCreateInfo;
    SDL_GpuVertexBinding binding;
    SDL_GpuVertexAttribute attribute;
    SDL_GpuColorAttachmentDescription colorAttachmentDesc;

    /* Fullscreen vertex shader */
    SDL_zero(shaderCreateInfo);
    shaderCreateInfo.code = (Uint8 *)D3D11_FullscreenVert;
    shaderCreateInfo.codeSize = sizeof(D3D11_FullscreenVert);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    shaderCreateInfo.format = SDL_GPU_SHADERFORMAT_DXBC;
    shaderCreateInfo.entryPointName = "main";

    fullscreenVertexShader = D3D11_CreateShader(
        (SDL_GpuRenderer *)renderer,
        &shaderCreateInfo);

    if (fullscreenVertexShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to compile fullscreen vertex shader!");
    }

    /* Blit from 2D pixel shader */
    shaderCreateInfo.code = (Uint8 *)D3D11_BlitFrom2D;
    shaderCreateInfo.codeSize = sizeof(D3D11_BlitFrom2D);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderCreateInfo.samplerCount = 1;

    blitFrom2DPixelShader = D3D11_CreateShader(
        (SDL_GpuRenderer *)renderer,
        &shaderCreateInfo);

    if (blitFrom2DPixelShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to compile blit from 2D pixel shader!");
    }

    /* Blit from 2D array pixel shader */
    shaderCreateInfo.code = (Uint8 *)D3D11_BlitFrom2DArray;
    shaderCreateInfo.codeSize = sizeof(D3D11_BlitFrom2DArray);
    shaderCreateInfo.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    shaderCreateInfo.uniformBufferCount = 1;

    blitFrom2DArrayPixelShader = D3D11_CreateShader(
        (SDL_GpuRenderer *)renderer,
        &shaderCreateInfo);

    if (blitFrom2DArrayPixelShader == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to compile blit from 2D array pixel shader!");
    }

    /* Blit from 2D pipeline */
    SDL_zero(blitPipelineCreateInfo);

    SDL_zero(colorAttachmentDesc);
    colorAttachmentDesc.blendState.colorWriteMask = 0xF;
    colorAttachmentDesc.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8; /* format doesn't matter in d3d11 */

    blitPipelineCreateInfo.attachmentInfo.colorAttachmentDescriptions = &colorAttachmentDesc;
    blitPipelineCreateInfo.attachmentInfo.colorAttachmentCount = 1;
    blitPipelineCreateInfo.attachmentInfo.depthStencilFormat = SDL_GPU_TEXTUREFORMAT_D16_UNORM; /* arbitrary */
    blitPipelineCreateInfo.attachmentInfo.hasDepthStencilAttachment = 0;

    binding.binding = 0;
    binding.inputRate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    binding.stepRate = 0;
    binding.stride = 64;

    attribute.binding = 0;
    attribute.format = SDL_GPU_VERTEXELEMENTFORMAT_VECTOR2;
    attribute.location = 0;
    attribute.offset = 0;

    blitPipelineCreateInfo.vertexInputState.vertexAttributeCount = 1;
    blitPipelineCreateInfo.vertexInputState.vertexAttributes = &attribute;
    blitPipelineCreateInfo.vertexInputState.vertexBindingCount = 1;
    blitPipelineCreateInfo.vertexInputState.vertexBindings = &binding;

    blitPipelineCreateInfo.vertexShader = fullscreenVertexShader;
    blitPipelineCreateInfo.fragmentShader = blitFrom2DPixelShader;

    blitPipelineCreateInfo.multisampleState.multisampleCount = SDL_GPU_SAMPLECOUNT_1;
    blitPipelineCreateInfo.multisampleState.sampleMask = 0xFFFFFFFF;

    blitPipelineCreateInfo.primitiveType = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    blitPipelineCreateInfo.blendConstants[0] = 1.0f;
    blitPipelineCreateInfo.blendConstants[1] = 1.0f;
    blitPipelineCreateInfo.blendConstants[2] = 1.0f;
    blitPipelineCreateInfo.blendConstants[3] = 1.0f;

    renderer->blitFrom2DPipeline = D3D11_CreateGraphicsPipeline(
        (SDL_GpuRenderer *)renderer,
        &blitPipelineCreateInfo);

    if (renderer->blitFrom2DPipeline == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create blit pipeline!");
    }

    /* Blit from 2D array pipeline */
    blitPipelineCreateInfo.fragmentShader = blitFrom2DArrayPixelShader;
    renderer->blitFrom2DArrayPipeline = D3D11_CreateGraphicsPipeline(
        (SDL_GpuRenderer *)renderer,
        &blitPipelineCreateInfo);

    /* Create samplers */
    samplerCreateInfo.addressModeU = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerCreateInfo.anisotropyEnable = 0;
    samplerCreateInfo.compareEnable = 0;
    samplerCreateInfo.magFilter = SDL_GPU_FILTER_NEAREST;
    samplerCreateInfo.minFilter = SDL_GPU_FILTER_NEAREST;
    samplerCreateInfo.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerCreateInfo.mipLodBias = 0.0f;
    samplerCreateInfo.minLod = 0;
    samplerCreateInfo.maxLod = 1000;

    renderer->blitNearestSampler = D3D11_CreateSampler(
        (SDL_GpuRenderer *)renderer,
        &samplerCreateInfo);

    if (renderer->blitNearestSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create blit nearest sampler!");
    }

    samplerCreateInfo.magFilter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.minFilter = SDL_GPU_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;

    renderer->blitLinearSampler = D3D11_CreateSampler(
        (SDL_GpuRenderer *)renderer,
        &samplerCreateInfo);

    if (renderer->blitLinearSampler == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create blit linear sampler!");
    }

    /* Clean up */
    D3D11_ReleaseShader((SDL_GpuRenderer *)renderer, fullscreenVertexShader);
    D3D11_ReleaseShader((SDL_GpuRenderer *)renderer, blitFrom2DPixelShader);
    D3D11_ReleaseShader((SDL_GpuRenderer *)renderer, blitFrom2DArrayPixelShader);
}

static void D3D11_INTERNAL_DestroyBlitPipelines(
    SDL_GpuRenderer *driverData)
{
    D3D11Renderer *renderer = (D3D11Renderer *)driverData;

    D3D11_ReleaseSampler(driverData, renderer->blitLinearSampler);
    D3D11_ReleaseSampler(driverData, renderer->blitNearestSampler);

    D3D11_ReleaseGraphicsPipeline(driverData, renderer->blitFrom2DPipeline);
    D3D11_ReleaseGraphicsPipeline(driverData, renderer->blitFrom2DArrayPipeline);
}

static SDL_GpuDevice *D3D11_CreateDevice(SDL_bool debugMode, SDL_bool preferLowPower)
{
    D3D11Renderer *renderer;
    PFN_CREATE_DXGI_FACTORY1 CreateDXGIFactoryFunc;
    PFN_D3D11_CREATE_DEVICE D3D11CreateDeviceFunc;
    D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1 };
    IDXGIFactory4 *factory4;
    IDXGIFactory5 *factory5;
    IDXGIFactory6 *factory6;
    Uint32 flags;
    DXGI_ADAPTER_DESC1 adapterDesc;
    HRESULT res;
    SDL_GpuDevice *result;

    /* Allocate and zero out the renderer */
    renderer = (D3D11Renderer *)SDL_calloc(1, sizeof(D3D11Renderer));

    /* Load the D3DCompiler library */
    renderer->d3dcompiler_dll = SDL_LoadObject(D3DCOMPILER_DLL);
    if (renderer->d3dcompiler_dll == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find " D3DCOMPILER_DLL);
        return NULL;
    }

    renderer->D3DCompile_func = (PFN_D3DCOMPILE)SDL_LoadFunction(renderer->d3dcompiler_dll, D3DCOMPILE_FUNC);
    if (renderer->D3DCompile_func == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " D3DCOMPILE_FUNC);
        return NULL;
    }

    /* Load the DXGI library */
    renderer->dxgi_dll = SDL_LoadObject(DXGI_DLL);
    if (renderer->dxgi_dll == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find " DXGI_DLL);
        return NULL;
    }

    /* Load the CreateDXGIFactory1 function */
    CreateDXGIFactoryFunc = (PFN_CREATE_DXGI_FACTORY1)SDL_LoadFunction(
        renderer->dxgi_dll,
        CREATE_DXGI_FACTORY1_FUNC);
    if (CreateDXGIFactoryFunc == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " CREATE_DXGI_FACTORY1_FUNC);
        return NULL;
    }

    /* Create the DXGI factory */
    res = CreateDXGIFactoryFunc(
        &D3D_IID_IDXGIFactory1,
        (void **)&renderer->factory);
    ERROR_CHECK_RETURN("Could not create DXGIFactory", NULL);

    /* Check for flip-model discard support (supported on Windows 10+) */
    res = IDXGIFactory1_QueryInterface(
        renderer->factory,
        &D3D_IID_IDXGIFactory4,
        (void **)&factory4);
    if (SUCCEEDED(res)) {
        renderer->supportsFlipDiscard = 1;
        IDXGIFactory4_Release(factory4);
    }

    /* Check for explicit tearing support */
    res = IDXGIFactory1_QueryInterface(
        renderer->factory,
        &D3D_IID_IDXGIFactory5,
        (void **)&factory5);
    if (SUCCEEDED(res)) {
        res = IDXGIFactory5_CheckFeatureSupport(
            factory5,
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &renderer->supportsTearing,
            sizeof(renderer->supportsTearing));
        if (FAILED(res)) {
            renderer->supportsTearing = FALSE;
        }
        IDXGIFactory5_Release(factory5);
    }

    /* Select the appropriate device for rendering */
    res = IDXGIAdapter1_QueryInterface(
        renderer->factory,
        &D3D_IID_IDXGIFactory6,
        (void **)&factory6);
    if (SUCCEEDED(res)) {
        IDXGIFactory6_EnumAdapterByGpuPreference(
            factory6,
            0,
            preferLowPower ?
                DXGI_GPU_PREFERENCE_MINIMUM_POWER :
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
            &D3D_IID_IDXGIAdapter1,
            (void **)&renderer->adapter);
        IDXGIFactory6_Release(factory6);
    } else {
        IDXGIFactory1_EnumAdapters1(
            renderer->factory,
            0,
            &renderer->adapter);
    }

    /* Get information about the selected adapter. Used for logging info. */
    IDXGIAdapter1_GetDesc1(renderer->adapter, &adapterDesc);

    /* Initialize the DXGI debug layer, if applicable */
    if (debugMode) {
        D3D11_INTERNAL_TryInitializeDXGIDebug(renderer);
    }

    /* Load the D3D library */
    renderer->d3d11_dll = SDL_LoadObject(D3D11_DLL);
    if (renderer->d3d11_dll == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find " D3D11_DLL);
        return NULL;
    }

    /* Load the CreateDevice function */
    D3D11CreateDeviceFunc = (PFN_D3D11_CREATE_DEVICE)SDL_LoadFunction(
        renderer->d3d11_dll,
        D3D11_CREATE_DEVICE_FUNC);
    if (D3D11CreateDeviceFunc == NULL) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " D3D11_CREATE_DEVICE_FUNC);
        return NULL;
    }

    /* Set up device flags */
    flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (debugMode) {
        flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    /* Create the device */
    ID3D11Device *d3d11Device;
tryCreateDevice:
    res = D3D11CreateDeviceFunc(
        (IDXGIAdapter *)renderer->adapter,
        D3D_DRIVER_TYPE_UNKNOWN, /* Must be UNKNOWN if adapter is non-null according to spec */
        NULL,
        flags,
        levels,
        SDL_arraysize(levels),
        D3D11_SDK_VERSION,
        &d3d11Device,
        NULL,
        &renderer->immediateContext);
    if (FAILED(res) && debugMode) {
        /* If device creation failed, and we're in debug mode, remove the debug flag and try again. */
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Creating device in debug mode failed with error " HRESULT_FMT ". Trying non-debug.", res);
        flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        debugMode = 0;
        goto tryCreateDevice;
    }

    ERROR_CHECK_RETURN("Could not create D3D11 device", NULL);

    /* The actual device we want is the ID3D11Device1 interface... */
    res = ID3D11Device_QueryInterface(
        d3d11Device,
        &D3D_IID_ID3D11Device1,
        (void **)&renderer->device);
    ERROR_CHECK_RETURN("Could not get ID3D11Device1 interface", NULL);

    /* Release the old device interface, we don't need it anymore */
    ID3D11Device_Release(d3d11Device);

#ifdef HAVE_IDXGIINFOQUEUE
    /* Set up the info queue */
    if (renderer->dxgiInfoQueue) {
        DXGI_INFO_QUEUE_MESSAGE_SEVERITY sevList[] = {
            DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION,
            DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR,
            DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING,
            // DXGI_INFO_QUEUE_MESSAGE_SEVERITY_INFO, /* This can be a bit much, so toggle as needed for debugging. */
            DXGI_INFO_QUEUE_MESSAGE_SEVERITY_MESSAGE
        };
        DXGI_INFO_QUEUE_FILTER filter = { 0 };
        filter.AllowList.NumSeverities = SDL_arraysize(sevList);
        filter.AllowList.pSeverityList = sevList;

        IDXGIInfoQueue_PushStorageFilter(
            renderer->dxgiInfoQueue,
            D3D_IID_DXGI_DEBUG_ALL,
            &filter);
    }
#endif

    /* Print driver info */
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL GPU Driver: D3D11");
    SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "D3D11 Adapter: %S", adapterDesc.Description);

    /* Create mutexes */
    renderer->contextLock = SDL_CreateMutex();
    renderer->acquireCommandBufferLock = SDL_CreateMutex();
    renderer->acquireUniformBufferLock = SDL_CreateMutex();
    renderer->fenceLock = SDL_CreateMutex();
    renderer->windowLock = SDL_CreateMutex();

    /* Initialize miscellaneous renderer members */
    renderer->debugMode = (flags & D3D11_CREATE_DEVICE_DEBUG);

    /* Create command buffer pool */
    D3D11_INTERNAL_AllocateCommandBuffers(renderer, 2);

    /* Create fence pool */
    renderer->availableFenceCapacity = 2;
    renderer->availableFences = SDL_malloc(
        sizeof(D3D11Fence *) * renderer->availableFenceCapacity);

    /* Create uniform buffer pool */

    renderer->uniformBufferPoolCapacity = 32;
    renderer->uniformBufferPoolCount = 32;
    renderer->uniformBufferPool = SDL_malloc(
        renderer->uniformBufferPoolCapacity * sizeof(D3D11UniformBuffer *));

    for (Uint32 i = 0; i < renderer->uniformBufferPoolCount; i += 1) {
        renderer->uniformBufferPool[i] = D3D11_INTERNAL_CreateUniformBuffer(
            renderer,
            UNIFORM_BUFFER_SIZE);
    }

    /* Create deferred destroy arrays */
    renderer->transferBufferContainersToDestroyCapacity = 2;
    renderer->transferBufferContainersToDestroyCount = 0;
    renderer->transferBufferContainersToDestroy = SDL_malloc(
        renderer->transferBufferContainersToDestroyCapacity * sizeof(D3D11TransferBufferContainer *));

    renderer->bufferContainersToDestroyCapacity = 2;
    renderer->bufferContainersToDestroyCount = 0;
    renderer->bufferContainersToDestroy = SDL_malloc(
        renderer->bufferContainersToDestroyCapacity * sizeof(D3D11BufferContainer *));

    renderer->textureContainersToDestroyCapacity = 2;
    renderer->textureContainersToDestroyCount = 0;
    renderer->textureContainersToDestroy = SDL_malloc(
        renderer->textureContainersToDestroyCapacity * sizeof(D3D11TextureContainer *));

    /* Create claimed window list */
    renderer->claimedWindowCapacity = 1;
    renderer->claimedWindows = SDL_malloc(
        sizeof(D3D11WindowData *) * renderer->claimedWindowCapacity);

    /* Initialize null states */

    SDL_zeroa(nullRTVs);
    SDL_zeroa(nullSRVs);
    SDL_zeroa(nullSamplers);
    SDL_zeroa(nullUAVs);

    /* Initialize built-in pipelines */
    D3D11_INTERNAL_InitBlitPipelines(renderer);

    /* Create the SDL_Gpu Device */
    result = (SDL_GpuDevice *)SDL_malloc(sizeof(SDL_GpuDevice));
    ASSIGN_DRIVER(D3D11)
    result->driverData = (SDL_GpuRenderer *)renderer;

    return result;
}

SDL_GpuDriver D3D11Driver = {
    "D3D11",
    SDL_GPU_BACKEND_D3D11,
    D3D11_PrepareDriver,
    D3D11_CreateDevice
};

#endif /*SDL_GPU_D3D11*/
