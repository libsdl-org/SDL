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
#include <dxgi.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>

#include "SDL_gpu_driver.h"
#include "SDL_gpu_d3d11_cdefines.h"

 /* Defines */

#define D3D11_DLL "d3d11.dll"
#define DXGI_DLL "dxgi.dll"
#define DXGIDEBUG_DLL "dxgidebug.dll"
#define D3D11_CREATE_DEVICE_FUNC "D3D11CreateDevice"
#define D3DCOMPILE_FUNC "D3DCompile"
#define CREATE_DXGI_FACTORY1_FUNC "CreateDXGIFactory1"
#define DXGI_GET_DEBUG_INTERFACE_FUNC "DXGIGetDebugInterface"
#define WINDOW_PROPERTY_DATA "SDL_GpuD3D11WindowPropertyData"
#define UBO_BUFFER_SIZE 1048576 /* 1 MiB */

#define NOT_IMPLEMENTED SDL_assert(0 && "Not implemented!");

/* Macros */

#define ERROR_CHECK(msg) \
	if (FAILED(res)) \
	{ \
		D3D11_INTERNAL_LogError(renderer->device, msg, res); \
	}

#define ERROR_CHECK_RETURN(msg, ret) \
	if (FAILED(res)) \
	{ \
		D3D11_INTERNAL_LogError(renderer->device, msg, res); \
		return ret; \
	}

#define EXPAND_ARRAY_IF_NEEDED(arr, elementType, newCount, capacity, newCapacity)	\
	if (newCount >= capacity)							\
	{										\
		capacity = newCapacity;							\
		arr = (elementType*) SDL_realloc(					\
			arr,								\
			sizeof(elementType) * capacity					\
		);									\
	}

#define TRACK_RESOURCE(resource, type, array, count, capacity) \
	Uint32 i; \
	\
	for (i = 0; i < commandBuffer->count; i += 1) \
	{ \
		if (commandBuffer->array[i] == resource) \
		{ \
			return; \
		} \
	} \
	\
	if (commandBuffer->count == commandBuffer->capacity) \
	{ \
		commandBuffer->capacity += 1; \
		commandBuffer->array = SDL_realloc( \
			commandBuffer->array, \
			commandBuffer->capacity * sizeof(type) \
		); \
	} \
	commandBuffer->array[commandBuffer->count] = resource; \
	commandBuffer->count += 1; \
	SDL_AtomicIncRef(&resource->referenceCount);

/* D3DCompile signature */

typedef HRESULT(WINAPI *PFN_D3DCOMPILE)(
	LPCVOID pSrcData,
	SIZE_T SrcDataSize,
	LPCSTR pSourceName,
	const D3D_SHADER_MACRO* pDefines,
	ID3DInclude* pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags1,
	UINT Flags2,
	ID3DBlob **ppCode,
	ID3DBlob **ppErrorMsgs
);

/* Forward Declarations */

static void D3D11_Wait(SDL_GpuRenderer *driverData);
static void D3D11_UnclaimWindow(
	SDL_GpuRenderer * driverData,
	void *windowHandle
);

 /* Conversions */

static DXGI_FORMAT RefreshToD3D11_TextureFormat[] =
{
	DXGI_FORMAT_R8G8B8A8_UNORM,	/* R8G8B8A8 */
	DXGI_FORMAT_B8G8R8A8_UNORM,	/* B8G8R8A8 */
	DXGI_FORMAT_B5G6R5_UNORM,	/* R5G6B5 */ /* FIXME: Swizzle? */
	DXGI_FORMAT_B5G5R5A1_UNORM,	/* A1R5G5B5 */ /* FIXME: Swizzle? */
	DXGI_FORMAT_B4G4R4A4_UNORM,	/* B4G4R4A4 */
	DXGI_FORMAT_R10G10B10A2_UNORM,	/* A2R10G10B10 */
	DXGI_FORMAT_R16G16_UNORM,	/* R16G16 */
	DXGI_FORMAT_R16G16B16A16_UNORM,	/* R16G16B16A16 */
	DXGI_FORMAT_R8_UNORM,		/* R8 */
	DXGI_FORMAT_BC1_UNORM,		/* BC1 */
	DXGI_FORMAT_BC2_UNORM,		/* BC2 */
	DXGI_FORMAT_BC3_UNORM,		/* BC3 */
	DXGI_FORMAT_BC7_UNORM,		/* BC7 */
	DXGI_FORMAT_R8G8_SNORM,		/* R8G8_SNORM */
	DXGI_FORMAT_R8G8B8A8_SNORM,	/* R8G8B8A8_SNORM */
	DXGI_FORMAT_R16_FLOAT,		/* R16_SFLOAT */
	DXGI_FORMAT_R16G16_FLOAT,	/* R16G16_SFLOAT */
	DXGI_FORMAT_R16G16B16A16_FLOAT,	/* R16G16B16A16_SFLOAT */
	DXGI_FORMAT_R32_FLOAT,		/* R32_SFLOAT */
	DXGI_FORMAT_R32G32_FLOAT,	/* R32G32_SFLOAT */
	DXGI_FORMAT_R32G32B32A32_FLOAT,	/* R32G32B32A32_SFLOAT */
	DXGI_FORMAT_R8_UINT,		/* R8_UINT */
	DXGI_FORMAT_R8G8_UINT,		/* R8G8_UINT */
	DXGI_FORMAT_R8G8B8A8_UINT,	/* R8G8B8A8_UINT */
	DXGI_FORMAT_R16_UINT,		/* R16_UINT */
	DXGI_FORMAT_R16G16_UINT,	/* R16G16_UINT */
	DXGI_FORMAT_R16G16B16A16_UINT,	/* R16G16B16A16_UINT */
	DXGI_FORMAT_D16_UNORM,		/* D16_UNORM */
	DXGI_FORMAT_D32_FLOAT,		/* D32_SFLOAT */
	DXGI_FORMAT_D24_UNORM_S8_UINT,	/* D16_UNORM_S8_UINT */
	DXGI_FORMAT_D32_FLOAT_S8X24_UINT/* D32_SFLOAT_S8_UINT */
};

static DXGI_FORMAT RefreshToD3D11_VertexFormat[] =
{
	DXGI_FORMAT_R32_UINT,		/* UINT */
	DXGI_FORMAT_R32_FLOAT,		/* FLOAT */
	DXGI_FORMAT_R32G32_FLOAT,	/* VECTOR2 */
	DXGI_FORMAT_R32G32B32_FLOAT,	/* VECTOR3 */
	DXGI_FORMAT_R32G32B32A32_FLOAT,	/* VECTOR4 */
	DXGI_FORMAT_R8G8B8A8_UNORM,	/* COLOR */
	DXGI_FORMAT_R8G8B8A8_UINT,	/* BYTE4 */
	DXGI_FORMAT_R16G16_SINT,	/* SHORT2 */
	DXGI_FORMAT_R16G16B16A16_SINT,	/* SHORT4 */
	DXGI_FORMAT_R16G16_SNORM,	/* NORMALIZEDSHORT2 */
	DXGI_FORMAT_R16G16B16A16_SNORM,	/* NORMALIZEDSHORT4 */
	DXGI_FORMAT_R16G16_FLOAT,	/* HALFVECTOR2 */
	DXGI_FORMAT_R16G16B16A16_FLOAT	/* HALFVECTOR4 */
};

static Uint32 RefreshToD3D11_SampleCount[] =
{
	1,	/* SDL_GPU_SAMPLECOUNT_1 */
	2,	/* SDL_GPU_SAMPLECOUNT_2 */
	4,	/* SDL_GPU_SAMPLECOUNT_4 */
	8	/* SDL_GPU_SAMPLECOUNT_8 */
};

static DXGI_FORMAT RefreshToD3D11_IndexType[] =
{
	DXGI_FORMAT_R16_UINT,	/* 16BIT */
	DXGI_FORMAT_R32_UINT	/* 32BIT */
};

static D3D11_PRIMITIVE_TOPOLOGY RefreshToD3D11_PrimitiveType[] =
{
	D3D_PRIMITIVE_TOPOLOGY_POINTLIST,	/* POINTLIST */
	D3D_PRIMITIVE_TOPOLOGY_LINELIST,	/* LINELIST */
	D3D_PRIMITIVE_TOPOLOGY_LINESTRIP,	/* LINESTRIP */
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST,	/* TRIANGLELIST */
	D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP	/* TRIANGLESTRIP */
};

static D3D11_CULL_MODE RefreshToD3D11_CullMode[] =
{
	D3D11_CULL_NONE,	/* NONE */
	D3D11_CULL_FRONT,	/* FRONT */
	D3D11_CULL_BACK		/* BACK */
};

static D3D11_BLEND RefreshToD3D11_BlendFactor[] =
{
	D3D11_BLEND_ZERO,		/* ZERO */
	D3D11_BLEND_ONE,		/* ONE */
	D3D11_BLEND_SRC_COLOR,		/* SRC_COLOR */
	D3D11_BLEND_INV_SRC_COLOR,	/* ONE_MINUS_SRC_COLOR */
	D3D11_BLEND_DEST_COLOR,		/* DST_COLOR */
	D3D11_BLEND_INV_DEST_COLOR,	/* ONE_MINUS_DST_COLOR */
	D3D11_BLEND_SRC_ALPHA,		/* SRC_ALPHA */
	D3D11_BLEND_INV_SRC_ALPHA,	/* ONE_MINUS_SRC_ALPHA */
	D3D11_BLEND_DEST_ALPHA,		/* DST_ALPHA */
	D3D11_BLEND_INV_DEST_ALPHA,	/* ONE_MINUS_DST_ALPHA */
	D3D11_BLEND_BLEND_FACTOR,	/* CONSTANT_COLOR */
	D3D11_BLEND_INV_BLEND_FACTOR,	/* ONE_MINUS_CONSTANT_COLOR */
	D3D11_BLEND_SRC_ALPHA_SAT,	/* SRC_ALPHA_SATURATE */
};

static D3D11_BLEND_OP RefreshToD3D11_BlendOp[] =
{
	D3D11_BLEND_OP_ADD,		/* ADD */
	D3D11_BLEND_OP_SUBTRACT,	/* SUBTRACT */
	D3D11_BLEND_OP_REV_SUBTRACT,	/* REVERSE_SUBTRACT */
	D3D11_BLEND_OP_MIN,		/* MIN */
	D3D11_BLEND_OP_MAX		/* MAX */
};

static D3D11_COMPARISON_FUNC RefreshToD3D11_CompareOp[] =
{
	D3D11_COMPARISON_NEVER,		/* NEVER */
	D3D11_COMPARISON_LESS,		/* LESS */
	D3D11_COMPARISON_EQUAL,		/* EQUAL */
	D3D11_COMPARISON_LESS_EQUAL,	/* LESS_OR_EQUAL */
	D3D11_COMPARISON_GREATER,	/* GREATER */
	D3D11_COMPARISON_NOT_EQUAL,	/* NOT_EQUAL */
	D3D11_COMPARISON_GREATER_EQUAL,	/* GREATER_OR_EQUAL */
	D3D11_COMPARISON_ALWAYS		/* ALWAYS */
};

static D3D11_STENCIL_OP RefreshToD3D11_StencilOp[] =
{
	D3D11_STENCIL_OP_KEEP,		/* KEEP */
	D3D11_STENCIL_OP_ZERO,		/* ZERO */
	D3D11_STENCIL_OP_REPLACE,	/* REPLACE */
	D3D11_STENCIL_OP_INCR_SAT,	/* INCREMENT_AND_CLAMP */
	D3D11_STENCIL_OP_DECR_SAT,	/* DECREMENT_AND_CLAMP */
	D3D11_STENCIL_OP_INVERT,	/* INVERT */
	D3D11_STENCIL_OP_INCR,		/* INCREMENT_AND_WRAP */
	D3D11_STENCIL_OP_DECR		/* DECREMENT_AND_WRAP */
};

static D3D11_INPUT_CLASSIFICATION RefreshToD3D11_VertexInputRate[] =
{
	D3D11_INPUT_PER_VERTEX_DATA,	/* VERTEX */
	D3D11_INPUT_PER_INSTANCE_DATA	/* INSTANCE */
};

static D3D11_TEXTURE_ADDRESS_MODE RefreshToD3D11_SamplerAddressMode[] =
{
	D3D11_TEXTURE_ADDRESS_WRAP,     /* REPEAT */
	D3D11_TEXTURE_ADDRESS_MIRROR,   /* MIRRORED_REPEAT */
	D3D11_TEXTURE_ADDRESS_CLAMP,    /* CLAMP_TO_EDGE */
	D3D11_TEXTURE_ADDRESS_BORDER    /* CLAMP_TO_BORDER */
};

static void RefreshToD3D11_BorderColor(
	SDL_GpuSamplerStateCreateInfo *createInfo,
	D3D11_SAMPLER_DESC *desc
) {
	switch (createInfo->borderColor)
	{
	case SDL_GPU_BORDERCOLOR_FLOAT_OPAQUE_BLACK:
	case SDL_GPU_BORDERCOLOR_INT_OPAQUE_BLACK:
		desc->BorderColor[0] = 0.0f;
		desc->BorderColor[1] = 0.0f;
		desc->BorderColor[2] = 0.0f;
		desc->BorderColor[3] = 1.0f;
		break;

	case SDL_GPU_BORDERCOLOR_FLOAT_OPAQUE_WHITE:
	case SDL_GPU_BORDERCOLOR_INT_OPAQUE_WHITE:
		desc->BorderColor[0] = 1.0f;
		desc->BorderColor[1] = 1.0f;
		desc->BorderColor[2] = 1.0f;
		desc->BorderColor[3] = 1.0f;
		break;

	case SDL_GPU_BORDERCOLOR_FLOAT_TRANSPARENT_BLACK:
	case SDL_GPU_BORDERCOLOR_INT_TRANSPARENT_BLACK:
		desc->BorderColor[0] = 0.0f;
		desc->BorderColor[1] = 0.0f;
		desc->BorderColor[2] = 0.0f;
		desc->BorderColor[3] = 0.0f;
		break;
	}
}

static D3D11_FILTER RefreshToD3D11_Filter(SDL_GpuSamplerStateCreateInfo *createInfo)
{
	if (createInfo->minFilter == SDL_GPU_FILTER_LINEAR)
	{
		if (createInfo->magFilter == SDL_GPU_FILTER_LINEAR)
		{
			if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR)
			{
				return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			}
			else
			{
				return D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			}
		}
		else
		{
			if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR)
			{
				return D3D11_FILTER_MIN_LINEAR_MAG_POINT_MIP_LINEAR;
			}
			else
			{
				return D3D11_FILTER_MIN_LINEAR_MAG_MIP_POINT;
			}
		}
	}
	else
	{
		if (createInfo->magFilter == SDL_GPU_FILTER_LINEAR)
		{
			if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR)
			{
				return D3D11_FILTER_MIN_POINT_MAG_MIP_LINEAR;
			}
			else
			{
				return D3D11_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
			}
		}
		else
		{
			if (createInfo->mipmapMode == SDL_GPU_SAMPLERMIPMAPMODE_LINEAR)
			{
				return D3D11_FILTER_MIN_MAG_POINT_MIP_LINEAR;
			}
			else
			{
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

	ID3D11RenderTargetView *colorTargetView; /* NULL if not a color target */
	ID3D11DepthStencilView *depthStencilTargetView; /* NULL if not a depth stencil target */
	ID3D11UnorderedAccessView *uav; /* NULL if not used in compute */
	ID3D11Resource *msaaHandle; /* NULL if not using MSAA */
	ID3D11RenderTargetView *msaaTargetView; /* NULL if not an MSAA color target */

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

typedef struct D3D11WindowData
{
	void* windowHandle;
	IDXGISwapChain *swapchain;
    D3D11Texture texture;
    D3D11TextureContainer textureContainer;
	SDL_GpuPresentMode presentMode;
} D3D11WindowData;

typedef struct D3D11ShaderModule
{
	ID3D11DeviceChild *shader; /* ID3D11VertexShader, ID3D11PixelShader, ID3D11ComputeShader */
	ID3D10Blob *blob;
} D3D11ShaderModule;

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
	ID3D11InputLayout *inputLayout;
	Uint32 *vertexStrides;
	Uint32 numVertexSamplers;
	Uint32 vertexUniformBlockSize;

	ID3D11PixelShader *fragmentShader;
	Uint32 numFragmentSamplers;
	Uint32 fragmentUniformBlockSize;
} D3D11GraphicsPipeline;

typedef struct D3D11ComputePipeline
{
	ID3D11ComputeShader *computeShader;
	Uint32 computeUniformBlockSize;
	Uint32 numTextures;
	Uint32 numBuffers;
} D3D11ComputePipeline;

typedef struct D3D11Buffer
{
	ID3D11Buffer *handle;
	ID3D11UnorderedAccessView* uav;
	Uint32 size;
	SDL_AtomicInt referenceCount;
} D3D11Buffer;

typedef struct D3D11BufferContainer
{
	SDL_GpuBufferUsageFlags usage;
	D3D11Buffer *activeBuffer;

	Uint32 bufferCapacity;
	Uint32 bufferCount;
	D3D11Buffer **buffers;

	char *debugName;
} D3D11BufferContainer;

typedef struct D3D11BufferTransfer
{
	ID3D11Buffer *stagingBuffer;
} D3D11BufferTransfer;

typedef struct D3D11TextureTransfer
{
	Uint8 *data;
} D3D11TextureTransfer;

typedef struct D3D11TransferBuffer
{
	Uint32 size;
	SDL_AtomicInt referenceCount;

	union
	{
		D3D11BufferTransfer bufferTransfer;
		D3D11TextureTransfer textureTransfer;
	};
} D3D11TransferBuffer;

typedef struct D3D11TransferBufferContainer
{
	SDL_GpuTransferUsage usage;
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
	D3D11Buffer d3d11Buffer;
	Uint32 offset; /* number of bytes written */
	Uint32 drawOffset; /* parameter for SetConstantBuffers */
} D3D11UniformBuffer;

typedef struct D3D11Fence
{
	ID3D11Query *handle;
} D3D11Fence;

typedef struct D3D11CommandBuffer
{
	/* Deferred Context */
	ID3D11DeviceContext1 *context;

	/* Window */
	D3D11WindowData *windowData;

	/* Render Pass */
	D3D11GraphicsPipeline *graphicsPipeline;

	/* Render Pass MSAA resolve */
	D3D11Texture *colorTargetResolveTexture[MAX_COLOR_TARGET_BINDINGS];
	Uint32 colorTargetResolveSubresourceIndex[MAX_COLOR_TARGET_BINDINGS];
	ID3D11Resource *colorTargetMsaaHandle[MAX_COLOR_TARGET_BINDINGS];

	/* Compute Pass */
	D3D11ComputePipeline *computePipeline;

	/* Fences */
	D3D11Fence *fence;
	Uint8 autoReleaseFence;

	/* Uniforms */
	D3D11UniformBuffer *vertexUniformBuffer;
	D3D11UniformBuffer *fragmentUniformBuffer;
	D3D11UniformBuffer *computeUniformBuffer;

	D3D11UniformBuffer **boundUniformBuffers;
	Uint32 boundUniformBufferCount;
	Uint32 boundUniformBufferCapacity;

	/* Reference Counting */
	D3D11Buffer **usedGpuBuffers;
	Uint32 usedGpuBufferCount;
	Uint32 usedGpuBufferCapacity;

	D3D11TransferBuffer **usedTransferBuffers;
	Uint32 usedTransferBufferCount;
	Uint32 usedTransferBufferCapacity;

	D3D11TextureSubresource **usedTextureSubresources;
	Uint32 usedTextureSubresourceCount;
	Uint32 usedTextureSubresourceCapacity;
} D3D11CommandBuffer;

typedef struct D3D11Sampler
{
	ID3D11SamplerState *handle;
} D3D11Sampler;

typedef struct D3D11Renderer
{
	ID3D11Device1 *device;
	ID3D11DeviceContext *immediateContext;
	IDXGIFactory1 *factory;
	IDXGIAdapter1 *adapter;
	IDXGIDebug *dxgiDebug;
	IDXGIInfoQueue *dxgiInfoQueue;
	void *d3d11_dll;
	void *dxgi_dll;
	void *dxgidebug_dll;
	void *d3dcompiler_dll;

	Uint8 debugMode;
	BOOL supportsTearing;
	Uint8 supportsFlipDiscard;

	PFN_D3DCOMPILE D3DCompileFunc;

	D3D11WindowData **claimedWindows;
	Uint32 claimedWindowCount;
	Uint32 claimedWindowCapacity;

	D3D11CommandBuffer **availableCommandBuffers;
	Uint32 availableCommandBufferCount;
	Uint32 availableCommandBufferCapacity;

	D3D11CommandBuffer **submittedCommandBuffers;
	Uint32 submittedCommandBufferCount;
	Uint32 submittedCommandBufferCapacity;

	D3D11UniformBuffer **availableUniformBuffers;
	Uint32 availableUniformBufferCount;
	Uint32 availableUniformBufferCapacity;

	D3D11Fence **availableFences;
	Uint32 availableFenceCount;
	Uint32 availableFenceCapacity;

	D3D11TransferBufferContainer **transferBufferContainersToDestroy;
	Uint32 transferBufferContainersToDestroyCount;
	Uint32 transferBufferContainersToDestroyCapacity;

	SDL_Mutex *contextLock;
	SDL_Mutex *acquireCommandBufferLock;
	SDL_Mutex *uniformBufferLock;
	SDL_Mutex *fenceLock;
	SDL_Mutex *windowLock;
} D3D11Renderer;

/* Logging */

static void D3D11_INTERNAL_LogError(
	ID3D11Device1 *device,
	const char *msg,
	HRESULT res
) {
	#define MAX_ERROR_LEN 1024 /* FIXME: Arbitrary! */

	/* Buffer for text, ensure space for \0 terminator after buffer */
	char wszMsgBuff[MAX_ERROR_LEN + 1];
	DWORD dwChars; /* Number of chars returned. */

	if (res == DXGI_ERROR_DEVICE_REMOVED)
	{
		res = ID3D11Device_GetDeviceRemovedReason(device);
	}

	/* Try to get the message from the system errors. */
	dwChars = FormatMessage(
		FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL,
		res,
		0,
		wszMsgBuff,
		MAX_ERROR_LEN,
		NULL
	);

	/* No message? Screw it, just post the code. */
	if (dwChars == 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s! Error Code: 0x%08lX", msg, res);
		return;
	}

	/* Ensure valid range */
	dwChars = SDL_min(dwChars, MAX_ERROR_LEN);

	/* Trim whitespace from tail of message */
	while (dwChars > 0)
	{
		if (wszMsgBuff[dwChars - 1] <= ' ')
		{
			dwChars--;
		}
		else
		{
			break;
		}
	}

	/* Ensure null-terminated string */
	wszMsgBuff[dwChars] = '\0';

	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s! Error Code: %s (0x%08lX)", msg, wszMsgBuff, res);
}

/* Helper Functions */

static inline Uint32 D3D11_INTERNAL_CalcSubresource(
	Uint32 mipLevel,
	Uint32 arraySlice,
	Uint32 numLevels
) {
	return mipLevel + (arraySlice * numLevels);
}

static inline Uint32 D3D11_INTERNAL_NextHighestAlignment(
	Uint32 n,
	Uint32 align
) {
	return align * ((n + align - 1) / align);
}

static DXGI_FORMAT D3D11_INTERNAL_GetTypelessFormat(
	DXGI_FORMAT typedFormat
) {
	switch (typedFormat)
	{
	case DXGI_FORMAT_D16_UNORM:
		return DXGI_FORMAT_R16_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT:
		return DXGI_FORMAT_R32_TYPELESS;
	case DXGI_FORMAT_D24_UNORM_S8_UINT:
		return DXGI_FORMAT_R24G8_TYPELESS;
	case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		return DXGI_FORMAT_R32G8X24_TYPELESS;
	default:
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cannot get typeless DXGI format of format %d", typedFormat);
		return 0;
	}
}

static DXGI_FORMAT D3D11_INTERNAL_GetSampleableFormat(
	DXGI_FORMAT format
) {
	switch (format)
	{
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

static void D3D11_DestroyDevice(
	SDL_GpuDevice *device
) {
	D3D11Renderer *renderer = (D3D11Renderer*) device->driverData;

	/* Flush any remaining GPU work... */
	D3D11_Wait(device->driverData);

	/* Release the window data */
	for (Sint32 i = renderer->claimedWindowCount - 1; i >= 0; i -= 1)
	{
		D3D11_UnclaimWindow(device->driverData, renderer->claimedWindows[i]->windowHandle);
	}
	SDL_free(renderer->claimedWindows);

	/* Release command buffer infrastructure */
	for (Uint32 i = 0; i < renderer->availableCommandBufferCount; i += 1)
	{
		D3D11CommandBuffer *commandBuffer = renderer->availableCommandBuffers[i];
		ID3D11DeviceContext_Release(commandBuffer->context);
		SDL_free(commandBuffer->boundUniformBuffers);
		SDL_free(commandBuffer->usedGpuBuffers);
		SDL_free(commandBuffer->usedTransferBuffers);
		SDL_free(commandBuffer);
	}
	SDL_free(renderer->availableCommandBuffers);
	SDL_free(renderer->submittedCommandBuffers);

	/* Release uniform buffer infrastructure */
	for (Uint32 i = 0; i < renderer->availableUniformBufferCount; i += 1)
	{
		D3D11UniformBuffer *uniformBuffer = renderer->availableUniformBuffers[i];
		ID3D11Buffer_Release(uniformBuffer->d3d11Buffer.handle);
		SDL_free(uniformBuffer);
	}
	SDL_free(renderer->availableUniformBuffers);

	/* Release fence infrastructure */
	for (Uint32 i = 0; i < renderer->availableFenceCount; i += 1)
	{
		D3D11Fence *fence = renderer->availableFences[i];
		ID3D11Query_Release(fence->handle);
		SDL_free(fence);
	}
	SDL_free(renderer->availableFences);

	/* Release the mutexes */
	SDL_DestroyMutex(renderer->acquireCommandBufferLock);
	SDL_DestroyMutex(renderer->contextLock);
	SDL_DestroyMutex(renderer->uniformBufferLock);
	SDL_DestroyMutex(renderer->fenceLock);
	SDL_DestroyMutex(renderer->windowLock);

	/* Release the device and associated objects */
	ID3D11DeviceContext_Release(renderer->immediateContext);
	ID3D11Device_Release(renderer->device);
	IDXGIAdapter_Release(renderer->adapter);
	IDXGIFactory_Release(renderer->factory);

	/* Report leaks and clean up debug objects */
	if (renderer->dxgiDebug)
	{
		IDXGIDebug_ReportLiveObjects(
			renderer->dxgiDebug,
			D3D_IID_DXGI_DEBUG_ALL,
			DXGI_DEBUG_RLO_SUMMARY | DXGI_DEBUG_RLO_DETAIL
		);
		IDXGIDebug_Release(renderer->dxgiDebug);
	}

	if (renderer->dxgiInfoQueue)
	{
		IDXGIInfoQueue_Release(renderer->dxgiInfoQueue);
	}

	/* Release the DLLs */
	SDL_UnloadObject(renderer->d3d11_dll);
	SDL_UnloadObject(renderer->dxgi_dll);
	if (renderer->dxgidebug_dll)
	{
		SDL_UnloadObject(renderer->dxgidebug_dll);
	}
	SDL_UnloadObject(renderer->d3dcompiler_dll);

	/* Free the primary structures */
	SDL_free(renderer);
	SDL_free(device);
}

/* Resource tracking */

static void D3D11_INTERNAL_TrackGpuBuffer(
	D3D11CommandBuffer *commandBuffer,
	D3D11Buffer *buffer
) {
	TRACK_RESOURCE(
		buffer,
		D3D11Buffer*,
		usedGpuBuffers,
		usedGpuBufferCount,
		usedGpuBufferCapacity
	);
}

static void D3D11_INTERNAL_TrackTransferBuffer(
	D3D11CommandBuffer *commandBuffer,
	D3D11TransferBuffer *buffer
) {
	TRACK_RESOURCE(
		buffer,
		D3D11TransferBuffer*,
		usedTransferBuffers,
		usedTransferBufferCount,
		usedTransferBufferCapacity
	);
}

static void D3D11_INTERNAL_TrackTextureSubresource(
	D3D11CommandBuffer *commandBuffer,
	D3D11TextureSubresource *textureSubresource
) {
	TRACK_RESOURCE(
		textureSubresource,
		D3D11TextureSubresource*,
		usedTextureSubresources,
		usedTextureSubresourceCount,
		usedTextureSubresourceCapacity
	);
}

/* Drawing */

static void D3D11_SetGraphicsConstantBuffers(
	D3D11CommandBuffer *commandBuffer
) {
	Uint32 vertexOffsetInConstants = commandBuffer->vertexUniformBuffer != NULL ? commandBuffer->vertexUniformBuffer->drawOffset / 16 : 0;
	Uint32 fragmentOffsetInConstants = commandBuffer->fragmentUniformBuffer != NULL ? commandBuffer->fragmentUniformBuffer->drawOffset / 16 : 0;
	Uint32 vertexBlockSizeInConstants = commandBuffer->graphicsPipeline->vertexUniformBlockSize / 16;
	Uint32 fragmentBlockSizeInConstants = commandBuffer->graphicsPipeline->fragmentUniformBlockSize / 16;
	ID3D11Buffer *nullBuf = NULL;

	if (commandBuffer->vertexUniformBuffer != NULL)
	{
		/* stupid workaround for god awful D3D11 drivers
		 * see: https://learn.microsoft.com/en-us/windows/win32/api/d3d11_1/nf-d3d11_1-id3d11devicecontext1-vssetconstantbuffers1#calling-vssetconstantbuffers1-with-command-list-emulation
		 */
		ID3D11DeviceContext1_VSSetConstantBuffers(
			commandBuffer->context,
			0,
			1,
			&nullBuf
		);

		ID3D11DeviceContext1_VSSetConstantBuffers1(
			commandBuffer->context,
			0,
			1,
			&commandBuffer->vertexUniformBuffer->d3d11Buffer.handle,
			&vertexOffsetInConstants,
			&vertexBlockSizeInConstants
		);
	}

	if (commandBuffer->fragmentUniformBuffer != NULL)
	{
		/* another stupid workaround for god awful D3D11 drivers */
		ID3D11DeviceContext1_PSSetConstantBuffers(
			commandBuffer->context,
			0,
			1,
			&nullBuf
		);

		ID3D11DeviceContext1_PSSetConstantBuffers1(
			commandBuffer->context,
			0,
			1,
			&commandBuffer->fragmentUniformBuffer->d3d11Buffer.handle,
			&fragmentOffsetInConstants,
			&fragmentBlockSizeInConstants
		);
	}
}

static void D3D11_DrawInstancedPrimitives(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 baseVertex,
	Uint32 startIndex,
	Uint32 primitiveCount,
	Uint32 instanceCount
) {
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;

	D3D11_SetGraphicsConstantBuffers(d3d11CommandBuffer);

	ID3D11DeviceContext_DrawIndexedInstanced(
		d3d11CommandBuffer->context,
		PrimitiveVerts(d3d11CommandBuffer->graphicsPipeline->primitiveType, primitiveCount),
		instanceCount,
		startIndex,
		baseVertex,
		0
	);
}

static void D3D11_DrawPrimitives(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 vertexStart,
	Uint32 primitiveCount
) {
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;

	D3D11_SetGraphicsConstantBuffers(d3d11CommandBuffer);

	ID3D11DeviceContext_Draw(
		d3d11CommandBuffer->context,
		PrimitiveVerts(d3d11CommandBuffer->graphicsPipeline->primitiveType, primitiveCount),
		vertexStart
	);
}

static void D3D11_DrawPrimitivesIndirect(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *gpuBuffer,
	Uint32 offsetInBytes,
	Uint32 drawCount,
	Uint32 stride
) {
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11Buffer *d3d11Buffer = ((D3D11BufferContainer*) gpuBuffer)->activeBuffer;

	D3D11_SetGraphicsConstantBuffers(d3d11CommandBuffer);

	/* D3D11: "We have multi-draw at home!"
	 * Multi-draw at home:
	 */
	for (Uint32 i = 0; i < drawCount; i += 1)
	{
		ID3D11DeviceContext_DrawInstancedIndirect(
			d3d11CommandBuffer->context,
			d3d11Buffer->handle,
			offsetInBytes + (stride * i)
		);
	}

	D3D11_INTERNAL_TrackGpuBuffer(d3d11CommandBuffer, d3d11Buffer);
}

static void D3D11_DispatchCompute(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 groupCountX,
	Uint32 groupCountY,
	Uint32 groupCountZ
) {
	D3D11CommandBuffer* d3d11CommandBuffer = (D3D11CommandBuffer*)commandBuffer;
	Uint32 computeOffsetInConstants = d3d11CommandBuffer->computeUniformBuffer != NULL ? d3d11CommandBuffer->computeUniformBuffer->drawOffset / 16 : 0;
	Uint32 computeBlockSizeInConstants = (Uint32) (d3d11CommandBuffer->computePipeline->computeUniformBlockSize / 16);

	ID3D11Buffer *nullBuf = NULL;

	if (d3d11CommandBuffer->computeUniformBuffer != NULL)
	{
		/* another stupid workaround for god awful D3D11 drivers */
		ID3D11DeviceContext1_CSSetConstantBuffers(
			d3d11CommandBuffer->context,
			0,
			1,
			&nullBuf
		);

		ID3D11DeviceContext1_CSSetConstantBuffers1(
			d3d11CommandBuffer->context,
			0,
			1,
			&d3d11CommandBuffer->computeUniformBuffer->d3d11Buffer.handle,
			&computeOffsetInConstants,
			&computeBlockSizeInConstants
		);
	}

	ID3D11DeviceContext_Dispatch(
		d3d11CommandBuffer->context,
		groupCountX,
		groupCountY,
		groupCountZ
	);
}

/* State Creation */

static ID3D11BlendState* D3D11_INTERNAL_FetchBlendState(
	D3D11Renderer *renderer,
	Uint32 numColorAttachments,
	SDL_GpuColorAttachmentDescription *colorAttachments
) {
	ID3D11BlendState *result;
	D3D11_BLEND_DESC blendDesc;
	HRESULT res;

	/* Create a new blend state.
	 * The spec says the driver will not create duplicate states, so there's no need to cache.
	 */
	SDL_zero(blendDesc); /* needed for any unused RT entries */

	blendDesc.AlphaToCoverageEnable = FALSE;
	blendDesc.IndependentBlendEnable = TRUE;

	for (Uint32 i = 0; i < numColorAttachments; i += 1)
	{
		blendDesc.RenderTarget[i].BlendEnable = colorAttachments[i].blendState.blendEnable;
		blendDesc.RenderTarget[i].BlendOp = RefreshToD3D11_BlendOp[
			colorAttachments[i].blendState.colorBlendOp
		];
		blendDesc.RenderTarget[i].BlendOpAlpha = RefreshToD3D11_BlendOp[
			colorAttachments[i].blendState.alphaBlendOp
		];
		blendDesc.RenderTarget[i].DestBlend = RefreshToD3D11_BlendFactor[
			colorAttachments[i].blendState.dstColorBlendFactor
		];
		blendDesc.RenderTarget[i].DestBlendAlpha = RefreshToD3D11_BlendFactor[
			colorAttachments[i].blendState.dstAlphaBlendFactor
		];
		blendDesc.RenderTarget[i].RenderTargetWriteMask = colorAttachments[i].blendState.colorWriteMask;
		blendDesc.RenderTarget[i].SrcBlend = RefreshToD3D11_BlendFactor[
			colorAttachments[i].blendState.srcColorBlendFactor
		];
		blendDesc.RenderTarget[i].SrcBlendAlpha = RefreshToD3D11_BlendFactor[
			colorAttachments[i].blendState.srcAlphaBlendFactor
		];
	}

	res = ID3D11Device_CreateBlendState(
		renderer->device,
		&blendDesc,
		&result
	);
	ERROR_CHECK_RETURN("Could not create blend state", NULL);

	return result;
}

static ID3D11DepthStencilState* D3D11_INTERNAL_FetchDepthStencilState(
	D3D11Renderer *renderer,
	SDL_GpuDepthStencilState depthStencilState
) {
	ID3D11DepthStencilState *result;
	D3D11_DEPTH_STENCIL_DESC dsDesc;
	HRESULT res;

	/* Create a new depth-stencil state.
	 * The spec says the driver will not create duplicate states, so there's no need to cache.
	 */
	dsDesc.DepthEnable = depthStencilState.depthTestEnable;
	dsDesc.StencilEnable = depthStencilState.stencilTestEnable;
	dsDesc.DepthFunc = RefreshToD3D11_CompareOp[depthStencilState.compareOp];
	dsDesc.DepthWriteMask = (
		depthStencilState.depthWriteEnable ?
			D3D11_DEPTH_WRITE_MASK_ALL :
			D3D11_DEPTH_WRITE_MASK_ZERO
	);

	dsDesc.BackFace.StencilFunc = RefreshToD3D11_CompareOp[depthStencilState.backStencilState.compareOp];
	dsDesc.BackFace.StencilDepthFailOp = RefreshToD3D11_StencilOp[depthStencilState.backStencilState.depthFailOp];
	dsDesc.BackFace.StencilFailOp = RefreshToD3D11_StencilOp[depthStencilState.backStencilState.failOp];
	dsDesc.BackFace.StencilPassOp = RefreshToD3D11_StencilOp[depthStencilState.backStencilState.passOp];

	dsDesc.FrontFace.StencilFunc = RefreshToD3D11_CompareOp[depthStencilState.frontStencilState.compareOp];
	dsDesc.FrontFace.StencilDepthFailOp = RefreshToD3D11_StencilOp[depthStencilState.frontStencilState.depthFailOp];
	dsDesc.FrontFace.StencilFailOp = RefreshToD3D11_StencilOp[depthStencilState.frontStencilState.failOp];
	dsDesc.FrontFace.StencilPassOp = RefreshToD3D11_StencilOp[depthStencilState.frontStencilState.passOp];

	dsDesc.StencilReadMask = depthStencilState.compareMask;
	dsDesc.StencilWriteMask = depthStencilState.writeMask;

	if (depthStencilState.depthBoundsTestEnable)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11 does not support Depth Bounds tests!");
	}

	res = ID3D11Device_CreateDepthStencilState(
		renderer->device,
		&dsDesc,
		&result
	);
	ERROR_CHECK_RETURN("Could not create depth-stencil state", NULL);

	return result;
}

static ID3D11RasterizerState* D3D11_INTERNAL_FetchRasterizerState(
	D3D11Renderer *renderer,
	SDL_GpuRasterizerState rasterizerState
) {
	ID3D11RasterizerState *result;
	D3D11_RASTERIZER_DESC rasterizerDesc;
	HRESULT res;

	/* Create a new rasterizer state.
	 * The spec says the driver will not create duplicate states, so there's no need to cache.
	 */
	rasterizerDesc.AntialiasedLineEnable = FALSE;
	rasterizerDesc.CullMode = RefreshToD3D11_CullMode[rasterizerState.cullMode];
	rasterizerDesc.DepthBias = (INT) rasterizerState.depthBiasConstantFactor;
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
		&result
	);
	ERROR_CHECK_RETURN("Could not create rasterizer state", NULL);

	return result;
}

static Uint32 D3D11_INTERNAL_FindIndexOfVertexBinding(
	Uint32 targetBinding,
	const SDL_GpuVertexBinding *bindings,
	Uint32 numBindings
) {
	for (Uint32 i = 0; i < numBindings; i += 1)
	{
		if (bindings[i].binding == targetBinding)
		{
			return i;
		}
	}

	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find vertex binding %u!", targetBinding);
	return 0;
}

static ID3D11InputLayout* D3D11_INTERNAL_FetchInputLayout(
	D3D11Renderer *renderer,
	SDL_GpuVertexInputState inputState,
	void *shaderBytes,
	size_t shaderByteLength
) {
	ID3D11InputLayout *result = NULL;
	D3D11_INPUT_ELEMENT_DESC *elementDescs;
	Uint32 bindingIndex;
	HRESULT res;

	/* Don't bother creating/fetching an input layout if there are no attributes. */
	if (inputState.vertexAttributeCount == 0)
	{
		return NULL;
	}

	/* Allocate an array of vertex elements */
	elementDescs = SDL_stack_alloc(
		D3D11_INPUT_ELEMENT_DESC,
		inputState.vertexAttributeCount
	);

	/* Create the array of input elements */
	for (Uint32 i = 0; i < inputState.vertexAttributeCount; i += 1)
	{
		elementDescs[i].AlignedByteOffset = inputState.vertexAttributes[i].offset;
		elementDescs[i].Format = RefreshToD3D11_VertexFormat[
			inputState.vertexAttributes[i].format
		];
		elementDescs[i].InputSlot = inputState.vertexAttributes[i].binding;

		bindingIndex = D3D11_INTERNAL_FindIndexOfVertexBinding(
			elementDescs[i].InputSlot,
			inputState.vertexBindings,
			inputState.vertexBindingCount
		);
		elementDescs[i].InputSlotClass = RefreshToD3D11_VertexInputRate[
			inputState.vertexBindings[bindingIndex].inputRate
		];
		/* The spec requires this to be 0 for per-vertex data */
		elementDescs[i].InstanceDataStepRate = (
			elementDescs[i].InputSlotClass == D3D11_INPUT_PER_INSTANCE_DATA ? 1 : 0
		);

		elementDescs[i].SemanticIndex = inputState.vertexAttributes[i].location;
		elementDescs[i].SemanticName = "TEXCOORD";
	}

	res = ID3D11Device_CreateInputLayout(
		renderer->device,
		elementDescs,
		inputState.vertexAttributeCount,
		shaderBytes,
		shaderByteLength,
		&result
	);
	if (FAILED(res))
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create input layout! Error: %lX", res);
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

static SDL_GpuComputePipeline* D3D11_CreateComputePipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuComputeShaderInfo *computeShaderInfo
) {
	(void) driverData; /* used by other backends */
	D3D11ShaderModule *shaderModule = (D3D11ShaderModule*) computeShaderInfo->shaderModule;

	D3D11ComputePipeline *pipeline = SDL_malloc(sizeof(D3D11ComputePipeline));
	pipeline->numTextures = computeShaderInfo->imageBindingCount;
	pipeline->numBuffers = computeShaderInfo->bufferBindingCount;
	pipeline->computeShader = (ID3D11ComputeShader*) shaderModule->shader;
	pipeline->computeUniformBlockSize = D3D11_INTERNAL_NextHighestAlignment(
		computeShaderInfo->uniformBufferSize,
		256
	);

	return (SDL_GpuComputePipeline*) pipeline;
}

static SDL_GpuGraphicsPipeline* D3D11_CreateGraphicsPipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuGraphicsPipelineCreateInfo *pipelineCreateInfo
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11ShaderModule *vertShaderModule = (D3D11ShaderModule*) pipelineCreateInfo->vertexShaderInfo.shaderModule;
	D3D11ShaderModule *fragShaderModule = (D3D11ShaderModule*) pipelineCreateInfo->fragmentShaderInfo.shaderModule;

	D3D11GraphicsPipeline *pipeline = SDL_malloc(sizeof(D3D11GraphicsPipeline));

	/* Blend */

	pipeline->colorAttachmentBlendState = D3D11_INTERNAL_FetchBlendState(
		renderer,
		pipelineCreateInfo->attachmentInfo.colorAttachmentCount,
		pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions
	);

	pipeline->numColorAttachments = pipelineCreateInfo->attachmentInfo.colorAttachmentCount;
	for (Sint32 i = 0; i < pipeline->numColorAttachments; i += 1)
	{
		pipeline->colorAttachmentFormats[i] = RefreshToD3D11_TextureFormat[
			pipelineCreateInfo->attachmentInfo.colorAttachmentDescriptions[i].format
		];
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
		pipelineCreateInfo->depthStencilState
	);

	pipeline->hasDepthStencilAttachment = pipelineCreateInfo->attachmentInfo.hasDepthStencilAttachment;
	pipeline->depthStencilAttachmentFormat = RefreshToD3D11_TextureFormat[
		pipelineCreateInfo->attachmentInfo.depthStencilFormat
	];
	pipeline->stencilRef = pipelineCreateInfo->depthStencilState.reference;

	/* Rasterizer */

	pipeline->primitiveType = pipelineCreateInfo->primitiveType;
	pipeline->rasterizerState = D3D11_INTERNAL_FetchRasterizerState(
		renderer,
		pipelineCreateInfo->rasterizerState
	);

	/* Vertex Shader */

	pipeline->vertexShader = (ID3D11VertexShader*) vertShaderModule->shader;
	pipeline->numVertexSamplers = pipelineCreateInfo->vertexShaderInfo.samplerBindingCount;
	pipeline->vertexUniformBlockSize = D3D11_INTERNAL_NextHighestAlignment(
		(Uint32) pipelineCreateInfo->vertexShaderInfo.uniformBufferSize,
		256
	);

	/* Input Layout */

	pipeline->inputLayout = D3D11_INTERNAL_FetchInputLayout(
		renderer,
		pipelineCreateInfo->vertexInputState,
		ID3D10Blob_GetBufferPointer(vertShaderModule->blob),
		ID3D10Blob_GetBufferSize(vertShaderModule->blob)
	);

	if (pipelineCreateInfo->vertexInputState.vertexBindingCount > 0)
	{
		pipeline->vertexStrides = SDL_malloc(
			sizeof(Uint32) *
			pipelineCreateInfo->vertexInputState.vertexBindingCount
		);

		for (Uint32 i = 0; i < pipelineCreateInfo->vertexInputState.vertexBindingCount; i += 1)
		{
			pipeline->vertexStrides[i] = pipelineCreateInfo->vertexInputState.vertexBindings[i].stride;
		}
	}
	else
	{
		pipeline->vertexStrides = NULL;
	}

	/* Fragment Shader */

	pipeline->fragmentShader = (ID3D11PixelShader*) fragShaderModule->shader;
	pipeline->numFragmentSamplers = pipelineCreateInfo->fragmentShaderInfo.samplerBindingCount;
	pipeline->fragmentUniformBlockSize = D3D11_INTERNAL_NextHighestAlignment(
		(Uint32) pipelineCreateInfo->fragmentShaderInfo.uniformBufferSize,
		256
	);

	return (SDL_GpuGraphicsPipeline*) pipeline;
}

/* Debug Naming */

static const GUID GUID_D3DDebugObjectName = { 0x429b8c22, 0x9188, 0x4b0c, { 0x87, 0x42, 0xac, 0xb0, 0xbf, 0x85, 0xc2, 0x00 } };

static void D3D11_INTERNAL_SetGpuBufferName(
	D3D11Renderer *renderer,
	D3D11Buffer *buffer,
	const char *text
) {
	if (renderer->debugMode)
	{
		ID3D11DeviceChild_SetPrivateData(
			buffer->handle,
			&GUID_D3DDebugObjectName,
			(UINT) SDL_strlen(text),
			text
		);
	}
}

static void D3D11_SetGpuBufferName(
	SDL_GpuRenderer *driverData,
	SDL_GpuBuffer *buffer,
	const char *text
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11BufferContainer *container = (D3D11BufferContainer*) buffer;

	if (renderer->debugMode)
	{
		container->debugName = SDL_realloc(
			container->debugName,
			SDL_strlen(text) + 1
		);

		SDL_utf8strlcpy(
			container->debugName,
			text,
			SDL_strlen(text) + 1
		);

		for (Uint32 i = 0; i < container->bufferCount; i += 1)
		{
			D3D11_INTERNAL_SetGpuBufferName(
				renderer,
				container->buffers[i],
				text
			);
		}
	}
}

static void D3D11_INTERNAL_SetTextureName(
	D3D11Renderer *renderer,
	D3D11Texture *texture,
	const char *text
) {
	if (renderer->debugMode)
	{
		ID3D11DeviceChild_SetPrivateData(
			texture->handle,
			&GUID_D3DDebugObjectName,
			(UINT) SDL_strlen(text),
			text
		);
	}
}

static void D3D11_SetTextureName(
	SDL_GpuRenderer *driverData,
	SDL_GpuTexture *texture,
	const char *text
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11TextureContainer *container = (D3D11TextureContainer*) texture;

	if (renderer->debugMode)
	{
		container->debugName = SDL_realloc(
			container->debugName,
			SDL_strlen(text) + 1
		);

		SDL_utf8strlcpy(
			container->debugName,
			text,
			SDL_strlen(text) + 1
		);

		for (Uint32 i = 0; i < container->textureCount; i += 1)
		{
			D3D11_INTERNAL_SetTextureName(
				renderer,
				container->textures[i],
				text
			);
		}
	}
}

/* Resource Creation */

static SDL_GpuSampler* D3D11_CreateSampler(
	SDL_GpuRenderer *driverData,
	SDL_GpuSamplerStateCreateInfo *samplerStateCreateInfo
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11_SAMPLER_DESC samplerDesc;
	ID3D11SamplerState *samplerStateHandle;
	D3D11Sampler *d3d11Sampler;
	HRESULT res;

	samplerDesc.AddressU = RefreshToD3D11_SamplerAddressMode[samplerStateCreateInfo->addressModeU];
	samplerDesc.AddressV = RefreshToD3D11_SamplerAddressMode[samplerStateCreateInfo->addressModeV];
	samplerDesc.AddressW = RefreshToD3D11_SamplerAddressMode[samplerStateCreateInfo->addressModeW];

	RefreshToD3D11_BorderColor(
		samplerStateCreateInfo,
		&samplerDesc
	);

	samplerDesc.ComparisonFunc = (
		samplerStateCreateInfo->compareEnable ?
			RefreshToD3D11_CompareOp[samplerStateCreateInfo->compareOp] :
			RefreshToD3D11_CompareOp[SDL_GPU_COMPAREOP_ALWAYS]
	);
	samplerDesc.MaxAnisotropy = (
		samplerStateCreateInfo->anisotropyEnable ?
			(UINT) samplerStateCreateInfo->maxAnisotropy :
			0
	);
	samplerDesc.Filter = RefreshToD3D11_Filter(samplerStateCreateInfo);
	samplerDesc.MaxLOD = samplerStateCreateInfo->maxLod;
	samplerDesc.MinLOD = samplerStateCreateInfo->minLod;
	samplerDesc.MipLODBias = samplerStateCreateInfo->mipLodBias;

	res = ID3D11Device_CreateSamplerState(
		renderer->device,
		&samplerDesc,
		&samplerStateHandle
	);
	ERROR_CHECK_RETURN("Could not create sampler state", NULL);

	d3d11Sampler = (D3D11Sampler*) SDL_malloc(sizeof(D3D11Sampler));
	d3d11Sampler->handle = samplerStateHandle;

	return (SDL_GpuSampler*) d3d11Sampler;
}

static SDL_GpuShaderModule* D3D11_CreateShaderModule(
	SDL_GpuRenderer *driverData,
	SDL_GpuShaderModuleCreateInfo *shaderModuleCreateInfo
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11ShaderModule* shaderModule;
	SDL_GpuShaderType shaderType = shaderModuleCreateInfo->type;
	const char *profileNames[] = { "vs_5_0", "ps_5_0", "cs_5_0" };
	ID3D10Blob *blob, *errorBlob;
	ID3D11DeviceChild *shader = NULL;
	HRESULT res;

	/* Compile HLSL to DXBC */
	res = renderer->D3DCompileFunc(
		shaderModuleCreateInfo->code,
		shaderModuleCreateInfo->codeSize,
		NULL,
		NULL,
		NULL,
		"main", /* API FIXME: Intentionally ignoring entryPointName because it MUST be "main" anyway */
		profileNames[shaderType],
		0,
		0,
		&blob,
		&errorBlob
	);
	if (FAILED(res))
	{
		SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
			"D3DCompile Error (%s): %p",
			profileNames[shaderType],
			ID3D10Blob_GetBufferPointer(errorBlob)
		);
		ID3D10Blob_Release(errorBlob);
		return NULL;
	}

	/* Actually create the shader */
	if (shaderType == SDL_GPU_SHADERTYPE_VERTEX)
	{
		res = ID3D11Device_CreateVertexShader(
			renderer->device,
			ID3D10Blob_GetBufferPointer(blob),
			ID3D10Blob_GetBufferSize(blob),
			NULL,
			(ID3D11VertexShader**) &shader
		);
		if (FAILED(res))
		{
			D3D11_INTERNAL_LogError(renderer->device, "Could not compile vertex shader", res);
			ID3D10Blob_Release(blob);
			return NULL;
		}
	}
	else if (shaderType == SDL_GPU_SHADERTYPE_FRAGMENT)
	{
		res = ID3D11Device_CreatePixelShader(
			renderer->device,
			ID3D10Blob_GetBufferPointer(blob),
			ID3D10Blob_GetBufferSize(blob),
			NULL,
			(ID3D11PixelShader**) &shader
		);
		if (FAILED(res))
		{
			D3D11_INTERNAL_LogError(renderer->device, "Could not compile pixel shader", res);
			ID3D10Blob_Release(blob);
			return NULL;
		}
	}
	else if (shaderType == SDL_GPU_SHADERTYPE_COMPUTE)
	{
		res = ID3D11Device_CreateComputeShader(
			renderer->device,
			ID3D10Blob_GetBufferPointer(blob),
			ID3D10Blob_GetBufferSize(blob),
			NULL,
			(ID3D11ComputeShader**) &shader
		);
		if (FAILED(res))
		{
			D3D11_INTERNAL_LogError(renderer->device, "Could not compile compute shader", res);
			ID3D10Blob_Release(blob);
			return NULL;
		}
	}

	/* Allocate and set up the shader module */
	shaderModule = (D3D11ShaderModule*) SDL_malloc(sizeof(D3D11ShaderModule));
	shaderModule->shader = shader;
	shaderModule->blob = blob;

	return (SDL_GpuShaderModule*) shaderModule;
}

static D3D11Texture* D3D11_Internal_CreateTexture(
	D3D11Renderer *renderer,
	SDL_GpuTextureCreateInfo *textureCreateInfo
) {
Uint8 isColorTarget, isDepthStencil, isSampler, isCompute, isMultisample;
	DXGI_FORMAT format;
	ID3D11Resource *textureHandle;
	ID3D11ShaderResourceView *srv = NULL;
	D3D11Texture *d3d11Texture;
	HRESULT res;

	isColorTarget = textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_COLOR_TARGET_BIT;
	isDepthStencil = textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET_BIT;
	isSampler = textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_SAMPLER_BIT;
	isCompute = textureCreateInfo->usageFlags & SDL_GPU_TEXTUREUSAGE_COMPUTE_BIT;
	isMultisample = textureCreateInfo->sampleCount > 1;

	format = RefreshToD3D11_TextureFormat[textureCreateInfo->format];
	if (isDepthStencil)
	{
		format = D3D11_INTERNAL_GetTypelessFormat(format);
	}

	if (textureCreateInfo->depth <= 1)
	{
		D3D11_TEXTURE2D_DESC desc2D;

		desc2D.BindFlags = 0;
		if (isSampler)
		{
			desc2D.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}
		if (isCompute)
		{
			desc2D.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		if (isColorTarget)
		{
			desc2D.BindFlags |= D3D11_BIND_RENDER_TARGET;
		}
		if (isDepthStencil)
		{
			desc2D.BindFlags |= D3D11_BIND_DEPTH_STENCIL;
		}

		desc2D.Width = textureCreateInfo->width;
		desc2D.Height = textureCreateInfo->height;
		desc2D.ArraySize = textureCreateInfo->isCube ? 6 : textureCreateInfo->layerCount;
		desc2D.CPUAccessFlags = 0;
		desc2D.Format = format;
		desc2D.MipLevels = textureCreateInfo->levelCount;
		desc2D.MiscFlags = textureCreateInfo->isCube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
		desc2D.SampleDesc.Count = 1;
		desc2D.SampleDesc.Quality = 0;
		desc2D.Usage = D3D11_USAGE_DEFAULT;

		res = ID3D11Device_CreateTexture2D(
			renderer->device,
			&desc2D,
			NULL,
			(ID3D11Texture2D**) &textureHandle
		);
		ERROR_CHECK_RETURN("Could not create Texture2D", NULL);

		/* Create the SRV, if applicable */
		if (isSampler)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = D3D11_INTERNAL_GetSampleableFormat(format);

			if (textureCreateInfo->isCube)
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
				srvDesc.TextureCube.MipLevels = desc2D.MipLevels;
				srvDesc.TextureCube.MostDetailedMip = 0;
			}
			else if (textureCreateInfo->layerCount > 1)
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Texture2DArray.MipLevels = desc2D.MipLevels;
				srvDesc.Texture2DArray.MostDetailedMip = 0;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.ArraySize = textureCreateInfo->layerCount;
			}
			else
			{
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = desc2D.MipLevels;
				srvDesc.Texture2D.MostDetailedMip = 0;
			}

			res = ID3D11Device_CreateShaderResourceView(
				renderer->device,
				textureHandle,
				&srvDesc,
				&srv
			);
			if (FAILED(res))
			{
				ID3D11Resource_Release(textureHandle);
				D3D11_INTERNAL_LogError(renderer->device, "Could not create SRV for 2D texture", res);
				return NULL;
			}
		}
	}
	else
	{
		D3D11_TEXTURE3D_DESC desc3D;

		desc3D.BindFlags = 0;
		if (isSampler)
		{
			desc3D.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}
		if (isCompute)
		{
			desc3D.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		if (isColorTarget)
		{
			desc3D.BindFlags |= D3D11_BIND_RENDER_TARGET;
		}

		desc3D.Width = textureCreateInfo->width;
		desc3D.Height = textureCreateInfo->height;
		desc3D.Depth = textureCreateInfo->depth;
		desc3D.CPUAccessFlags = 0;
		desc3D.Format = format;
		desc3D.MipLevels = textureCreateInfo->levelCount;
		desc3D.MiscFlags = 0;
		desc3D.Usage = D3D11_USAGE_DEFAULT;

		res = ID3D11Device_CreateTexture3D(
			renderer->device,
			&desc3D,
			NULL,
			(ID3D11Texture3D**) &textureHandle
		);
		ERROR_CHECK_RETURN("Could not create Texture3D", NULL);

		/* Create the SRV, if applicable */
		if (isSampler)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = format;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
			srvDesc.Texture3D.MipLevels = desc3D.MipLevels;
			srvDesc.Texture3D.MostDetailedMip = 0;

			res = ID3D11Device_CreateShaderResourceView(
				renderer->device,
				textureHandle,
				&srvDesc,
				&srv
			);
			if (FAILED(res))
			{
				ID3D11Resource_Release(textureHandle);
				D3D11_INTERNAL_LogError(renderer->device, "Could not create SRV for 3D texture", res);
				return NULL;
			}
		}
	}

	d3d11Texture = (D3D11Texture*) SDL_malloc(sizeof(D3D11Texture));
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
		d3d11Texture->subresourceCount * sizeof(D3D11TextureSubresource)
	);

	for (Uint32 layerIndex = 0; layerIndex < d3d11Texture->layerCount; layerIndex += 1)
	{
		for (Uint32 levelIndex = 0; levelIndex < d3d11Texture->levelCount; levelIndex += 1)
		{
			Uint32 subresourceIndex = D3D11_INTERNAL_CalcSubresource(
				levelIndex,
				layerIndex,
				d3d11Texture->levelCount
			);

			d3d11Texture->subresources[subresourceIndex].parent = d3d11Texture;
			d3d11Texture->subresources[subresourceIndex].layer = layerIndex;
			d3d11Texture->subresources[subresourceIndex].level = levelIndex;
			d3d11Texture->subresources[subresourceIndex].index = subresourceIndex;

			d3d11Texture->subresources[subresourceIndex].colorTargetView = NULL;
			d3d11Texture->subresources[subresourceIndex].depthStencilTargetView = NULL;
			d3d11Texture->subresources[subresourceIndex].uav = NULL;
			d3d11Texture->subresources[subresourceIndex].msaaHandle = NULL;
			d3d11Texture->subresources[subresourceIndex].msaaTargetView = NULL;

			if (isMultisample)
			{
				D3D11_TEXTURE2D_DESC desc2D;

				if (isColorTarget)
				{
					desc2D.BindFlags = D3D11_BIND_RENDER_TARGET;
				}
				else if (isDepthStencil)
				{
					desc2D.BindFlags = D3D11_BIND_DEPTH_STENCIL;
				}

				desc2D.Width = textureCreateInfo->width;
				desc2D.Height = textureCreateInfo->height;
				desc2D.ArraySize = 1;
				desc2D.CPUAccessFlags = 0;
				desc2D.Format = format;
				desc2D.MipLevels = 1;
				desc2D.MiscFlags = 0;
				desc2D.SampleDesc.Count = RefreshToD3D11_SampleCount[textureCreateInfo->sampleCount];
				desc2D.SampleDesc.Quality = D3D11_STANDARD_MULTISAMPLE_PATTERN;
				desc2D.Usage = D3D11_USAGE_DEFAULT;

				res = ID3D11Device_CreateTexture2D(
					renderer->device,
					&desc2D,
					NULL,
					(ID3D11Texture2D**) &d3d11Texture->subresources[subresourceIndex].msaaHandle
				);
				ERROR_CHECK_RETURN("Could not create MSAA texture!", NULL);

				if (!isDepthStencil)
				{
					D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;

					rtvDesc.Format = format;
					rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;

					res = ID3D11Device_CreateRenderTargetView(
						renderer->device,
						d3d11Texture->subresources[subresourceIndex].msaaHandle,
						&rtvDesc,
						&d3d11Texture->subresources[subresourceIndex].msaaTargetView
					);
					ERROR_CHECK_RETURN("Could not create MSAA RTV!", NULL);
				}
			}

			if (d3d11Texture->isRenderTarget)
			{
				if (isDepthStencil)
				{
					D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;

					dsvDesc.Format = RefreshToD3D11_TextureFormat[d3d11Texture->format];
					dsvDesc.Flags = 0;

					if (isMultisample)
					{
						dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
					}
					else
					{
						dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
						dsvDesc.Texture2D.MipSlice = levelIndex;
					}

					res = ID3D11Device_CreateDepthStencilView(
						renderer->device,
						isMultisample ? d3d11Texture->subresources[subresourceIndex].msaaHandle : d3d11Texture->handle,
						&dsvDesc,
						&d3d11Texture->subresources[subresourceIndex].depthStencilTargetView
					);
					ERROR_CHECK_RETURN("Could not create DSV!", NULL);
				}
				else
				{
					D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;

					rtvDesc.Format = RefreshToD3D11_TextureFormat[d3d11Texture->format];

					if (d3d11Texture->layerCount > 1)
					{
						rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
						rtvDesc.Texture2DArray.MipSlice = levelIndex;
						rtvDesc.Texture2DArray.FirstArraySlice = layerIndex;
						rtvDesc.Texture2DArray.ArraySize = 1;
					}
					else if (d3d11Texture->depth > 1)
					{
						rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
						rtvDesc.Texture3D.MipSlice = levelIndex;
						rtvDesc.Texture3D.FirstWSlice = 0;
						rtvDesc.Texture3D.WSize = d3d11Texture->depth;
					}
					else
					{
						rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
						rtvDesc.Texture2D.MipSlice = levelIndex;
					}

					res = ID3D11Device_CreateRenderTargetView(
						renderer->device,
						d3d11Texture->handle,
						&rtvDesc,
						&d3d11Texture->subresources[subresourceIndex].colorTargetView
					);
					ERROR_CHECK_RETURN("Could not create RTV!", NULL);
				}
			}

			if (isCompute)
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
				uavDesc.Format = format;

				if (d3d11Texture->layerCount > 1)
				{
					uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
					uavDesc.Texture2DArray.MipSlice = levelIndex;
					uavDesc.Texture2DArray.FirstArraySlice = layerIndex;
					uavDesc.Texture2DArray.ArraySize = 1;
				}
				else if (d3d11Texture->depth > 1)
				{
					uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
					uavDesc.Texture3D.MipSlice = levelIndex;
					uavDesc.Texture3D.FirstWSlice = 0;
					uavDesc.Texture3D.WSize = d3d11Texture->layerCount;
				}
				else
				{
					uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
					uavDesc.Texture2D.MipSlice = levelIndex;
				}

				res = ID3D11Device_CreateUnorderedAccessView(
					renderer->device,
					d3d11Texture->handle,
					&uavDesc,
					&d3d11Texture->subresources[subresourceIndex].uav
				);
				ERROR_CHECK_RETURN("Could not create UAV!", NULL);
			}
		}
	}

	return d3d11Texture;
}

static SDL_GpuTexture* D3D11_CreateTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuTextureCreateInfo *textureCreateInfo
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11TextureContainer *container;
	D3D11Texture *texture;

	texture = D3D11_Internal_CreateTexture(
		renderer,
		textureCreateInfo
	);

	if (texture == NULL)
	{
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
		container->textureCapacity * sizeof(D3D11Texture*)
	);
	container->textures[0] = texture;
	container->debugName = NULL;

	return (SDL_GpuTexture*) container;
}

static void D3D11_INTERNAL_CycleActiveTexture(
	D3D11Renderer *renderer,
	D3D11TextureContainer *container
) {
	for (Uint32 i = 0; i < container->textureCount; i += 1)
	{
		Uint32 refCountTotal = 0;
		for (Uint32 j = 0; j < container->textures[i]->layerCount * container->textures[i]->levelCount; j += 1)
		{
			refCountTotal += SDL_AtomicGet(&container->textures[i]->subresources[j].referenceCount);
		}

		if (refCountTotal == 0)
		{
			container->activeTexture = container->textures[i];
			return;
		}
	}

	EXPAND_ARRAY_IF_NEEDED(
		container->textures,
		D3D11Texture*,
		container->textureCount + 1,
		container->textureCapacity,
		container->textureCapacity + 1
	);

	container->textures[container->textureCount] = D3D11_Internal_CreateTexture(
		renderer,
		&container->createInfo
	);
	container->textureCount += 1;

	container->activeTexture = container->textures[container->textureCount - 1];

	if (renderer->debugMode && container->debugName != NULL)
	{
		D3D11_INTERNAL_SetTextureName(
			renderer,
			container->activeTexture,
			container->debugName
		);
	}
}

static D3D11TextureSubresource* D3D11_INTERNAL_FetchTextureSubresource(
	D3D11Texture *texture,
	Uint32 layer,
	Uint32 level
) {
	Uint32 index = D3D11_INTERNAL_CalcSubresource(level, layer, texture->layerCount);
	return &texture->subresources[index];
}

static D3D11TextureSubresource* D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
	D3D11Renderer *renderer,
	D3D11TextureContainer *container,
	Uint32 layer,
	Uint32 level,
	Uint8 cycle
) {
	D3D11TextureSubresource *subresource = D3D11_INTERNAL_FetchTextureSubresource(
		container->activeTexture,
		layer,
		level
	);

	if (cycle && container->canBeCycled)
	{
		D3D11_INTERNAL_CycleActiveTexture(
			renderer,
			container
		);

		subresource = D3D11_INTERNAL_FetchTextureSubresource(
			container->activeTexture,
			layer,
			level
		);
	}

	return subresource;
}

static D3D11Buffer* D3D11_INTERNAL_CreateGpuBuffer(
	D3D11Renderer *renderer,
	SDL_GpuBufferUsageFlags usageFlags,
	Uint32 sizeInBytes
) {
	D3D11_BUFFER_DESC bufferDesc;
	ID3D11Buffer *bufferHandle;
	ID3D11UnorderedAccessView *uav = NULL;
	D3D11Buffer *d3d11Buffer;
	HRESULT res;

	bufferDesc.BindFlags = 0;
	if (usageFlags & SDL_GPU_BUFFERUSAGE_VERTEX_BIT)
	{
		bufferDesc.BindFlags |= D3D11_BIND_VERTEX_BUFFER;
	}
	if (usageFlags & SDL_GPU_BUFFERUSAGE_INDEX_BIT)
	{
		bufferDesc.BindFlags |= D3D11_BIND_INDEX_BUFFER;
	}
	if ((usageFlags & SDL_GPU_BUFFERUSAGE_COMPUTE_BIT) || (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT_BIT))
	{
		bufferDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
	}

	bufferDesc.ByteWidth = sizeInBytes;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.StructureByteStride = 0;

	bufferDesc.MiscFlags = 0;
	if (usageFlags & SDL_GPU_BUFFERUSAGE_INDIRECT_BIT)
	{
		bufferDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
	}
	if (usageFlags & SDL_GPU_BUFFERUSAGE_COMPUTE_BIT)
	{
		bufferDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
	}

	res = ID3D11Device_CreateBuffer(
		renderer->device,
		&bufferDesc,
		NULL,
		&bufferHandle
	);
	ERROR_CHECK_RETURN("Could not create buffer", NULL);

	/* Create a UAV for the buffer */
	if (usageFlags & SDL_GPU_BUFFERUSAGE_COMPUTE_BIT)
	{
		D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
		uavDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;
		uavDesc.Buffer.NumElements = sizeInBytes / sizeof(Uint32);

		res = ID3D11Device_CreateUnorderedAccessView(
			renderer->device,
			(ID3D11Resource*) bufferHandle,
			&uavDesc,
			&uav
		);
		if (FAILED(res))
		{
			ID3D11Buffer_Release(bufferHandle);
			ERROR_CHECK_RETURN("Could not create UAV for buffer!", NULL);
		}
	}

	d3d11Buffer = SDL_malloc(sizeof(D3D11Buffer));
	d3d11Buffer->handle = bufferHandle;
	d3d11Buffer->size = sizeInBytes;
	d3d11Buffer->uav = uav;
	SDL_AtomicSet(&d3d11Buffer->referenceCount, 0);

	return d3d11Buffer;
}

static SDL_GpuBuffer* D3D11_CreateGpuBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuBufferUsageFlags usageFlags,
	Uint32 sizeInBytes
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11BufferContainer *container;
	D3D11Buffer *buffer;

	buffer = D3D11_INTERNAL_CreateGpuBuffer(
		renderer,
		usageFlags,
		sizeInBytes
	);

	if (buffer == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GpuBuffer!");
		return NULL;
	}

	container = SDL_malloc(sizeof(D3D11BufferContainer));
	container->usage = usageFlags;
	container->activeBuffer = buffer;
	container->bufferCapacity = 1;
	container->bufferCount = 1;
	container->buffers = SDL_malloc(
		container->bufferCapacity * sizeof(D3D11Buffer*)
	);
	container->buffers[0] = container->activeBuffer;
	container->debugName = NULL;

	return (SDL_GpuBuffer*) container;
}

static void D3D11_INTERNAL_CycleActiveGpuBuffer(
	D3D11Renderer *renderer,
	D3D11BufferContainer *container
) {
	Uint32 size = container->activeBuffer->size;

	for (Uint32 i = 0; i < container->bufferCount; i += 1)
	{
		if (SDL_AtomicGet(&container->buffers[i]->referenceCount) == 0)
		{
			container->activeBuffer = container->buffers[i];
			return;
		}
	}

	EXPAND_ARRAY_IF_NEEDED(
		container->buffers,
		D3D11Buffer*,
		container->bufferCount + 1,
		container->bufferCapacity,
		container->bufferCapacity + 1
	);

	container->buffers[container->bufferCount] = D3D11_INTERNAL_CreateGpuBuffer(
		renderer,
		container->usage,
		size
	);
	container->bufferCount += 1;

	container->activeBuffer = container->buffers[container->bufferCount - 1];

	if (renderer->debugMode && container->debugName != NULL)
	{
		D3D11_INTERNAL_SetGpuBufferName(
			renderer,
			container->activeBuffer,
			container->debugName
		);
	}
}

static D3D11Buffer* D3D11_INTERNAL_PrepareGpuBufferForWrite(
	D3D11Renderer *renderer,
	D3D11BufferContainer *container,
	Uint8 cycle
) {
	if (
		cycle &&
		SDL_AtomicGet(&container->activeBuffer->referenceCount) > 0
	) {
		D3D11_INTERNAL_CycleActiveGpuBuffer(
			renderer,
			container
		);
	}

	return container->activeBuffer;
}

static D3D11TransferBuffer* D3D11_INTERNAL_CreateTransferBuffer(
	D3D11Renderer *renderer,
	SDL_GpuTransferUsage usage,
	Uint32 sizeInBytes
) {
	D3D11TransferBuffer *transferBuffer = SDL_malloc(sizeof(D3D11TransferBuffer));

	transferBuffer->size = sizeInBytes;
	SDL_AtomicSet(&transferBuffer->referenceCount, 0);

	if (usage == SDL_GPU_TRANSFERUSAGE_BUFFER)
	{
		D3D11_BUFFER_DESC stagingBufferDesc;
		HRESULT res;

		stagingBufferDesc.ByteWidth = sizeInBytes;
		stagingBufferDesc.Usage = D3D11_USAGE_STAGING;
		stagingBufferDesc.BindFlags = 0;
		stagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ|D3D11_CPU_ACCESS_WRITE;
		stagingBufferDesc.MiscFlags = 0;
		stagingBufferDesc.StructureByteStride = 0;

		res = ID3D11Device_CreateBuffer(
			renderer->device,
			&stagingBufferDesc,
			NULL,
			&transferBuffer->bufferTransfer.stagingBuffer
		);
		ERROR_CHECK_RETURN("Could not create staging buffer", NULL);
	}
	else /* TRANSFERUSAGE_TEXTURE */
	{
		transferBuffer->textureTransfer.data = (Uint8*) SDL_malloc(sizeInBytes);
	}

	return transferBuffer;
}

/* This actually returns a container handle so we can rotate buffers on Cycle. */
static SDL_GpuTransferBuffer* D3D11_CreateTransferBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuTransferUsage usage,
	Uint32 sizeInBytes
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer*) SDL_malloc(sizeof(D3D11TransferBufferContainer));

	container->usage = usage;
	container->bufferCapacity = 1;
	container->bufferCount = 1;
	container->buffers = SDL_malloc(
		container->bufferCapacity * sizeof(D3D11TransferBuffer*)
	);

	container->buffers[0] = D3D11_INTERNAL_CreateTransferBuffer(
		renderer,
		usage,
		sizeInBytes
	);

	container->activeBuffer = container->buffers[0];

	return (SDL_GpuTransferBuffer*) container;
}

/* TransferBuffer Data */

static void D3D11_INTERNAL_CycleActiveTransferBuffer(
	D3D11Renderer *renderer,
	D3D11TransferBufferContainer *container
) {
	Uint32 size = container->activeBuffer->size;

	for (Uint32 i = 0; i < container->bufferCount; i += 1)
	{
		if (SDL_AtomicGet(&container->buffers[i]->referenceCount) == 0)
		{
			container->activeBuffer = container->buffers[i];
			return;
		}
	}

	EXPAND_ARRAY_IF_NEEDED(
		container->buffers,
		D3D11TransferBuffer*,
		container->bufferCount + 1,
		container->bufferCapacity,
		container->bufferCapacity + 1
	);

	container->buffers[container->bufferCount] = D3D11_INTERNAL_CreateTransferBuffer(
		renderer,
		container->usage,
		size
	);
	container->bufferCount += 1;

	container->activeBuffer = container->buffers[container->bufferCount - 1];
}

static void D3D11_SetTransferData(
	SDL_GpuRenderer *driverData,
	void* data,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuTransferOptions transferOption
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer*) transferBuffer;
	D3D11TransferBuffer *buffer = container->activeBuffer;
	HRESULT res;

	/* Rotate the transfer buffer if necessary */
	if (
		transferOption == SDL_GPU_TRANSFEROPTIONS_CYCLE &&
		SDL_AtomicGet(&container->activeBuffer->referenceCount) > 0
	) {
		D3D11_INTERNAL_CycleActiveTransferBuffer(
			renderer,
			container
		);
		buffer = container->activeBuffer;
	}

	if (container->usage == SDL_GPU_TRANSFERUSAGE_BUFFER)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubresource;

		SDL_LockMutex(renderer->contextLock);
		res = ID3D11DeviceContext_Map(
			renderer->immediateContext,
			(ID3D11Resource*) buffer->bufferTransfer.stagingBuffer,
			0,
			D3D11_MAP_WRITE,
			0,
			&mappedSubresource
		);
		ERROR_CHECK_RETURN("Failed to map staging buffer", );

		SDL_memcpy(
			((Uint8*) mappedSubresource.pData) + copyParams->dstOffset,
			((Uint8*) data) + copyParams->srcOffset,
			copyParams->size
		);

		ID3D11DeviceContext_Unmap(
			renderer->immediateContext,
			(ID3D11Resource*) buffer->bufferTransfer.stagingBuffer,
			0
		);
		SDL_UnlockMutex(renderer->contextLock);
	}
	else /* TEXTURE */
	{
		SDL_memcpy(
			buffer->textureTransfer.data + copyParams->dstOffset,
			((Uint8*) data) + copyParams->srcOffset,
			copyParams->size
		);
	}
}

static void D3D11_GetTransferData(
	SDL_GpuRenderer *driverData,
	SDL_GpuTransferBuffer *transferBuffer,
	void* data,
	SDL_GpuBufferCopy *copyParams
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer*) transferBuffer;
	D3D11TransferBuffer *buffer = container->activeBuffer;
	HRESULT res;

	if (container->usage == SDL_GPU_TRANSFERUSAGE_BUFFER)
	{
		D3D11_MAPPED_SUBRESOURCE mappedSubresource;

		SDL_LockMutex(renderer->contextLock);
		res = ID3D11DeviceContext_Map(
			renderer->immediateContext,
			(ID3D11Resource*) buffer->bufferTransfer.stagingBuffer,
			0,
			D3D11_MAP_READ,
			0,
			&mappedSubresource
		);
		ERROR_CHECK_RETURN("Failed to map staging buffer", );

		SDL_memcpy(
			((Uint8*) data) + copyParams->dstOffset,
			((Uint8*) mappedSubresource.pData) + copyParams->srcOffset,
			copyParams->size
		);

		ID3D11DeviceContext_Unmap(
			renderer->immediateContext,
			(ID3D11Resource*) buffer->bufferTransfer.stagingBuffer,
			0
		);
		SDL_UnlockMutex(renderer->contextLock);
	}
	else /* TEXTURE */
	{
		SDL_memcpy(
			((Uint8*) data) + copyParams->dstOffset,
			(Uint8*) buffer->textureTransfer.data + copyParams->srcOffset,
			copyParams->size
		);
	}
}

/* Copy Pass */

static void D3D11_BeginCopyPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	/* no-op */
}

static void D3D11_UploadToTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuTextureRegion *textureRegion,
	SDL_GpuBufferImageCopy *copyParams,
	SDL_GpuTextureWriteOptions writeOption
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11TransferBufferContainer *transferContainer = (D3D11TransferBufferContainer*) transferBuffer;
	D3D11TransferBuffer *d3d11TransferBuffer = transferContainer->activeBuffer;
	D3D11TextureContainer *d3d11TextureContainer = (D3D11TextureContainer*) textureRegion->textureSlice.texture;
	Uint32 bufferStride = copyParams->bufferStride;
	Uint32 bufferImageHeight = copyParams->bufferImageHeight;
	Sint32 w = textureRegion->w;
	Sint32 h = textureRegion->h;

	D3D11TextureSubresource *textureSubresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
		renderer,
		d3d11TextureContainer,
		textureRegion->textureSlice.layer,
		textureRegion->textureSlice.mipLevel,
		writeOption == SDL_GPU_TEXTUREWRITEOPTIONS_CYCLE
	);

	Sint32 blockSize = Texture_GetBlockSize(textureSubresource->parent->format);
	if (blockSize > 1)
	{
		w = (w + blockSize - 1) & ~(blockSize - 1);
		h = (h + blockSize - 1) & ~(blockSize - 1);
	}

	if (bufferStride == 0 || bufferImageHeight == 0)
	{
		bufferStride = BytesPerRow(w, textureSubresource->parent->format);
		bufferImageHeight = h * Texture_GetFormatSize(textureSubresource->parent->format);
	}

	D3D11_BOX dstBox;
	dstBox.left = textureRegion->x;
	dstBox.top = textureRegion->y;
	dstBox.front = textureRegion->z;
	dstBox.right = textureRegion->x + w;
	dstBox.bottom = textureRegion->y + h;
	dstBox.back = textureRegion->z + 1;

	ID3D11DeviceContext1_UpdateSubresource1(
		d3d11CommandBuffer->context,
		textureSubresource->parent->handle,
		textureSubresource->index,
		&dstBox,
		(Uint8*) d3d11TransferBuffer->textureTransfer.data + copyParams->bufferOffset,
		bufferStride,
		bufferStride * bufferImageHeight,
		writeOption == SDL_GPU_TEXTUREWRITEOPTIONS_CYCLE ? D3D11_COPY_NO_OVERWRITE : 0
	);

	D3D11_INTERNAL_TrackTextureSubresource(d3d11CommandBuffer, textureSubresource);
	D3D11_INTERNAL_TrackTransferBuffer(d3d11CommandBuffer, d3d11TransferBuffer);
}

static void D3D11_UploadToBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBuffer *gpuBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions writeOption
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11TransferBufferContainer *transferContainer = (D3D11TransferBufferContainer*) transferBuffer;
	D3D11TransferBuffer *d3d11TransferBuffer = transferContainer->activeBuffer;
	D3D11BufferContainer *bufferContainer = (D3D11BufferContainer*) gpuBuffer;
	D3D11_BOX srcBox = { copyParams->srcOffset, 0, 0, copyParams->srcOffset + copyParams->size, 1, 1 };

	D3D11Buffer *d3d11Buffer = D3D11_INTERNAL_PrepareGpuBufferForWrite(
		renderer,
		bufferContainer,
		writeOption == SDL_GPU_BUFFERWRITEOPTIONS_CYCLE
	);

	ID3D11DeviceContext1_CopySubresourceRegion1(
		d3d11CommandBuffer->context,
		(ID3D11Resource*) d3d11Buffer->handle,
		0,
		copyParams->dstOffset,
		0,
		0,
		(ID3D11Resource*) d3d11TransferBuffer->bufferTransfer.stagingBuffer,
		0,
		&srcBox,
		D3D11_COPY_NO_OVERWRITE /* always no overwrite because we manually discard */
	);

	D3D11_INTERNAL_TrackGpuBuffer(d3d11CommandBuffer, d3d11Buffer);
	D3D11_INTERNAL_TrackTransferBuffer(d3d11CommandBuffer, d3d11TransferBuffer);
}

static void D3D11_DownloadFromTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuTextureRegion *textureRegion,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferImageCopy *copyParams,
	SDL_GpuTransferOptions transferOption
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer*) transferBuffer;
	D3D11TransferBuffer *d3d11TransferBuffer = container->activeBuffer;
	D3D11TextureContainer *d3d11TextureContainer = (D3D11TextureContainer*) textureRegion->textureSlice.texture;
	D3D11_TEXTURE2D_DESC stagingDesc;
	ID3D11Resource *stagingTexture;
	D3D11TextureSubresource *textureSubresource = D3D11_INTERNAL_FetchTextureSubresource(
		d3d11TextureContainer->activeTexture,
		textureRegion->textureSlice.layer,
		textureRegion->textureSlice.mipLevel
	);
	Sint32 formatSize = Texture_GetFormatSize(textureSubresource->parent->format);
	Uint32 bufferStride = copyParams->bufferStride;
	Uint32 bufferImageHeight = copyParams->bufferImageHeight;
	D3D11_BOX srcBox = {textureRegion->x, textureRegion->y, textureRegion->z, textureRegion->x + textureRegion->w, textureRegion->y + textureRegion->h, 1};
	D3D11_MAPPED_SUBRESOURCE subresource;
	HRESULT res;

	/* Rotate the transfer buffer if necessary */
	if (
		transferOption == SDL_GPU_TRANSFEROPTIONS_CYCLE &&
		SDL_AtomicGet(&container->activeBuffer->referenceCount) > 0
	) {
		D3D11_INTERNAL_CycleActiveTransferBuffer(
			renderer,
			container
		);
		d3d11TransferBuffer = container->activeBuffer;
	}

	if (bufferStride == 0 || bufferImageHeight == 0)
	{
		bufferStride = BytesPerRow(textureRegion->w, textureSubresource->parent->format);
		bufferImageHeight = textureRegion->h * Texture_GetFormatSize(textureSubresource->parent->format);
	}

	stagingDesc.Width = textureRegion->w;
	stagingDesc.Height = textureRegion->h;
	stagingDesc.MipLevels = 1;
	stagingDesc.ArraySize = 1;
	stagingDesc.Format = RefreshToD3D11_TextureFormat[textureSubresource->parent->format];
	stagingDesc.SampleDesc.Count = 1;
	stagingDesc.SampleDesc.Quality = 0;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;

	res = ID3D11Device_CreateTexture2D(
		renderer->device,
		&stagingDesc,
		NULL,
		(ID3D11Texture2D**) &stagingTexture
	);
	ERROR_CHECK_RETURN("Staging texture creation failed",)

	/* Readback is only possible on CPU timeline in D3D11 */
	SDL_LockMutex(renderer->contextLock);
	ID3D11DeviceContext_CopySubresourceRegion(
		renderer->immediateContext,
		stagingTexture,
		0,
		0,
		0,
		0,
		textureSubresource->parent->handle,
		textureSubresource->index,
		&srcBox
	);

	/* Read from the staging texture */
	res = ID3D11DeviceContext_Map(
		renderer->immediateContext,
		stagingTexture,
		0,
		D3D11_MAP_READ,
		0,
		&subresource
	);
	ERROR_CHECK_RETURN("Could not map texture for reading",)

	Uint8* dataPtr = (Uint8*) d3d11TransferBuffer->textureTransfer.data + copyParams->bufferOffset;

	/* TODO: figure out 3D copy */
	for (Uint32 row = textureRegion->y; row < textureRegion->h; row += 1)
	{
		SDL_memcpy(
			dataPtr,
			(Uint8*) subresource.pData + (row * subresource.RowPitch) + (textureRegion->x * formatSize),
			bufferStride
		);
		dataPtr += bufferStride;
	}

	ID3D11DeviceContext1_Unmap(
		renderer->immediateContext,
		stagingTexture,
		0
	);
	SDL_UnlockMutex(renderer->contextLock);

	/* Clean up the staging texture */
	ID3D11Texture2D_Release(stagingTexture);
}

static void D3D11_DownloadFromBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuBuffer *gpuBuffer,
	SDL_GpuTransferBuffer *transferBuffer,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuTransferOptions transferOption
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11TransferBufferContainer *container = (D3D11TransferBufferContainer*) transferBuffer;
	D3D11TransferBuffer *d3d11TransferBuffer = container->activeBuffer;
	D3D11BufferContainer *d3d11BufferContainer = (D3D11BufferContainer*) gpuBuffer;
	D3D11_BOX srcBox = { copyParams->srcOffset, 0, 0, copyParams->size, 1, 1 };

	/* Rotate the transfer buffer if necessary */
	if (
		transferOption == SDL_GPU_TRANSFEROPTIONS_CYCLE &&
		SDL_AtomicGet(&container->activeBuffer->referenceCount) > 0
	) {
		D3D11_INTERNAL_CycleActiveTransferBuffer(
			renderer,
			container
		);
		d3d11TransferBuffer = container->activeBuffer;
	}

	/* Readback is only possible on CPU timeline in D3D11 */
	SDL_LockMutex(renderer->contextLock);
	ID3D11DeviceContext_CopySubresourceRegion(
		renderer->immediateContext,
		(ID3D11Resource*) d3d11TransferBuffer->bufferTransfer.stagingBuffer,
		0,
		copyParams->dstOffset,
		0,
		0,
		(ID3D11Resource*) d3d11BufferContainer->activeBuffer->handle,
		0,
		&srcBox
	);
	SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_CopyTextureToTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureRegion *source,
	SDL_GpuTextureRegion *destination,
	SDL_GpuTextureWriteOptions writeOption
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11TextureContainer *srcContainer = (D3D11TextureContainer*) source->textureSlice.texture;
	D3D11TextureContainer *dstContainer = (D3D11TextureContainer*) destination->textureSlice.texture;

	D3D11_BOX srcBox = { source->x, source->y, source->z, source->x + source->w, source->y + source->h, 1 };

	D3D11TextureSubresource *srcSubresource = D3D11_INTERNAL_FetchTextureSubresource(
		srcContainer->activeTexture,
		source->textureSlice.layer,
		source->textureSlice.mipLevel
	);

	D3D11TextureSubresource *dstSubresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
		renderer,
		dstContainer,
		destination->textureSlice.layer,
		destination->textureSlice.mipLevel,
		writeOption == SDL_GPU_TEXTUREWRITEOPTIONS_CYCLE
	);

	ID3D11DeviceContext1_CopySubresourceRegion1(
		d3d11CommandBuffer->context,
		dstSubresource->parent->handle,
		dstSubresource->index,
		destination->x,
		destination->y,
		destination->z,
		srcSubresource->parent->handle,
		srcSubresource->index,
		&srcBox,
		writeOption == SDL_GPU_TEXTUREWRITEOPTIONS_CYCLE ? D3D11_COPY_NO_OVERWRITE : 0
	);

	D3D11_INTERNAL_TrackTextureSubresource(d3d11CommandBuffer, srcSubresource);
	D3D11_INTERNAL_TrackTextureSubresource(d3d11CommandBuffer, dstSubresource);
}

static void D3D11_CopyBufferToBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBuffer *source,
	SDL_GpuBuffer *destination,
	SDL_GpuBufferCopy *copyParams,
	SDL_GpuBufferWriteOptions writeOption
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11BufferContainer *srcBufferContainer = (D3D11BufferContainer*) source;
	D3D11BufferContainer *dstBufferContainer = (D3D11BufferContainer*) destination;
	D3D11_BOX srcBox = { copyParams->srcOffset, 0, 0, copyParams->srcOffset + copyParams->size, 1, 1 };

	D3D11Buffer *srcBuffer = srcBufferContainer->activeBuffer;
	D3D11Buffer *dstBuffer = D3D11_INTERNAL_PrepareGpuBufferForWrite(
		renderer,
		dstBufferContainer,
		writeOption == SDL_GPU_BUFFERWRITEOPTIONS_CYCLE
	);

	ID3D11DeviceContext1_CopySubresourceRegion1(
		d3d11CommandBuffer->context,
		(ID3D11Resource*) dstBuffer->handle,
		0,
		copyParams->dstOffset,
		0,
		0,
		(ID3D11Resource*) srcBuffer->handle,
		0,
		&srcBox,
		D3D11_COPY_NO_OVERWRITE /* always no overwrite because we either manually discard or the write is unsafe */
	);

	D3D11_INTERNAL_TrackGpuBuffer(d3d11CommandBuffer, srcBuffer);
	D3D11_INTERNAL_TrackGpuBuffer(d3d11CommandBuffer, dstBuffer);
}

static void D3D11_GenerateMipmaps(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTexture *texture
) {
	(void) driverData; /* used by other backends */
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11TextureContainer *d3d11TextureContainer = (D3D11TextureContainer*) texture;

	ID3D11DeviceContext1_GenerateMips(
		d3d11CommandBuffer->context,
		d3d11TextureContainer->activeTexture->shaderView
	);

	for (Uint32 i = 0; i < d3d11TextureContainer->activeTexture->subresourceCount; i += 1)
	{
		D3D11_INTERNAL_TrackTextureSubresource(
			d3d11CommandBuffer,
			&d3d11TextureContainer->activeTexture->subresources[i]
		);
	}
}

static void D3D11_EndCopyPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	/* no-op */
}

/* Uniforms */

static Uint8 D3D11_INTERNAL_CreateUniformBuffer(
	D3D11Renderer *renderer
) {
	D3D11_BUFFER_DESC bufferDesc;
	ID3D11Buffer *bufferHandle;
	D3D11UniformBuffer *uniformBuffer;
	HRESULT res;

	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.ByteWidth = UBO_BUFFER_SIZE;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;

	res = ID3D11Device_CreateBuffer(
		renderer->device,
		&bufferDesc,
		NULL,
		&bufferHandle
	);
	ERROR_CHECK_RETURN("Failed to create uniform buffer", 0);

	uniformBuffer = SDL_malloc(sizeof(D3D11UniformBuffer));
	uniformBuffer->offset = 0;
	uniformBuffer->drawOffset = 0;
	uniformBuffer->d3d11Buffer.handle = bufferHandle;
	uniformBuffer->d3d11Buffer.size = UBO_BUFFER_SIZE;
	uniformBuffer->d3d11Buffer.uav = NULL;

	/* Add it to the available pool */
	if (renderer->availableUniformBufferCount >= renderer->availableUniformBufferCapacity)
	{
		renderer->availableUniformBufferCapacity *= 2;

		renderer->availableUniformBuffers = SDL_realloc(
			renderer->availableUniformBuffers,
			sizeof(D3D11UniformBuffer*) * renderer->availableUniformBufferCapacity
		);
	}

	renderer->availableUniformBuffers[renderer->availableUniformBufferCount] = uniformBuffer;
	renderer->availableUniformBufferCount += 1;

	return 1;
}

static Uint8 D3D11_INTERNAL_AcquireUniformBuffer(
	D3D11Renderer *renderer,
	D3D11CommandBuffer *commandBuffer,
	D3D11UniformBuffer **uniformBufferToBind
) {
	D3D11UniformBuffer *uniformBuffer;

	/* Acquire a uniform buffer from the pool */
	SDL_LockMutex(renderer->uniformBufferLock);

	if (renderer->availableUniformBufferCount == 0)
	{
		if (!D3D11_INTERNAL_CreateUniformBuffer(renderer))
		{
			SDL_UnlockMutex(renderer->uniformBufferLock);
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create uniform buffer!");
			return 0;
		}
	}

	uniformBuffer = renderer->availableUniformBuffers[renderer->availableUniformBufferCount - 1];
	renderer->availableUniformBufferCount -= 1;

	SDL_UnlockMutex(renderer->uniformBufferLock);

	/* Reset the uniform buffer */
	uniformBuffer->offset = 0;
	uniformBuffer->drawOffset = 0;

	/* Bind the uniform buffer to the command buffer */
	if (commandBuffer->boundUniformBufferCount >= commandBuffer->boundUniformBufferCapacity)
	{
		commandBuffer->boundUniformBufferCapacity *= 2;
		commandBuffer->boundUniformBuffers = SDL_realloc(
			commandBuffer->boundUniformBuffers,
			sizeof(D3D11UniformBuffer*) * commandBuffer->boundUniformBufferCapacity
		);
	}
	commandBuffer->boundUniformBuffers[commandBuffer->boundUniformBufferCount] = uniformBuffer;
	commandBuffer->boundUniformBufferCount += 1;

	*uniformBufferToBind = uniformBuffer;

	return 1;
}

static void D3D11_INTERNAL_SetUniformBufferData(
	D3D11Renderer *renderer,
	D3D11CommandBuffer *commandBuffer,
	D3D11UniformBuffer *uniformBuffer,
	void* data,
	Uint32 dataLength
) {
	D3D11_MAPPED_SUBRESOURCE subres;

	HRESULT res = ID3D11DeviceContext_Map(
		commandBuffer->context,
		(ID3D11Resource*) uniformBuffer->d3d11Buffer.handle,
		0,
		uniformBuffer->offset == 0 ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE,
		0,
		&subres
	);
	ERROR_CHECK_RETURN("Could not map buffer for writing!", );

	SDL_memcpy(
		(Uint8*) subres.pData + uniformBuffer->offset,
		data,
		dataLength
	);

	ID3D11DeviceContext_Unmap(
		commandBuffer->context,
		(ID3D11Resource*) uniformBuffer->d3d11Buffer.handle,
		0
	);
}

static void D3D11_PushVertexShaderUniforms(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11GraphicsPipeline *graphicsPipeline = d3d11CommandBuffer->graphicsPipeline;

	if (d3d11CommandBuffer->vertexUniformBuffer->offset + graphicsPipeline->vertexUniformBlockSize >= UBO_BUFFER_SIZE)
	{
		/* Out of space! Get a new uniform buffer. */
		D3D11_INTERNAL_AcquireUniformBuffer(
			renderer,
			d3d11CommandBuffer,
			&d3d11CommandBuffer->vertexUniformBuffer
		);
	}

	d3d11CommandBuffer->vertexUniformBuffer->drawOffset = d3d11CommandBuffer->vertexUniformBuffer->offset;

	D3D11_INTERNAL_SetUniformBufferData(
		renderer,
		d3d11CommandBuffer,
		d3d11CommandBuffer->vertexUniformBuffer,
		data,
		dataLengthInBytes
	);

	d3d11CommandBuffer->vertexUniformBuffer->offset += graphicsPipeline->vertexUniformBlockSize;
}

static void D3D11_PushFragmentShaderUniforms(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11GraphicsPipeline *graphicsPipeline = d3d11CommandBuffer->graphicsPipeline;

	if (d3d11CommandBuffer->fragmentUniformBuffer->offset + graphicsPipeline->fragmentUniformBlockSize >= UBO_BUFFER_SIZE)
	{
		/* Out of space! Get a new uniform buffer. */
		D3D11_INTERNAL_AcquireUniformBuffer(
			renderer,
			d3d11CommandBuffer,
			&d3d11CommandBuffer->fragmentUniformBuffer
		);
	}

	d3d11CommandBuffer->fragmentUniformBuffer->drawOffset = d3d11CommandBuffer->fragmentUniformBuffer->offset;

	D3D11_INTERNAL_SetUniformBufferData(
		renderer,
		d3d11CommandBuffer,
		d3d11CommandBuffer->fragmentUniformBuffer,
		data,
		dataLengthInBytes
	);

	d3d11CommandBuffer->fragmentUniformBuffer->offset += graphicsPipeline->fragmentUniformBlockSize;
}

static void D3D11_PushComputeShaderUniforms(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *data,
	Uint32 dataLengthInBytes
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11ComputePipeline *computePipeline = d3d11CommandBuffer->computePipeline;

	if (d3d11CommandBuffer->computeUniformBuffer->offset + computePipeline->computeUniformBlockSize >= UBO_BUFFER_SIZE)
	{
		/* Out of space! Get a new uniform buffer. */
		D3D11_INTERNAL_AcquireUniformBuffer(
			renderer,
			d3d11CommandBuffer,
			&d3d11CommandBuffer->computeUniformBuffer
		);
	}

	d3d11CommandBuffer->computeUniformBuffer->drawOffset = d3d11CommandBuffer->computeUniformBuffer->offset;

	D3D11_INTERNAL_SetUniformBufferData(
		renderer,
		d3d11CommandBuffer,
		d3d11CommandBuffer->computeUniformBuffer,
		data,
		dataLengthInBytes
	);

	d3d11CommandBuffer->computeUniformBuffer->offset +=
		(Uint32) computePipeline->computeUniformBlockSize;
}

/* Samplers */

static void D3D11_BindVertexSamplers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
) {
	(void) driverData; /* used by other backends */
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	ID3D11ShaderResourceView* srvs[MAX_VERTEXTEXTURE_SAMPLERS];
	ID3D11SamplerState* d3d11Samplers[MAX_VERTEXTEXTURE_SAMPLERS];

	Sint32 numVertexSamplers = d3d11CommandBuffer->graphicsPipeline->numVertexSamplers;

	for (Sint32 i = 0; i < numVertexSamplers; i += 1)
	{
		srvs[i] = ((D3D11TextureContainer*) pBindings[i].texture)->activeTexture->shaderView;
		d3d11Samplers[i] = ((D3D11Sampler*) pBindings[i].sampler)->handle;
	}

	ID3D11DeviceContext_VSSetShaderResources(
		d3d11CommandBuffer->context,
		0,
		numVertexSamplers,
		srvs
	);

	ID3D11DeviceContext_VSSetSamplers(
		d3d11CommandBuffer->context,
		0,
		numVertexSamplers,
		d3d11Samplers
	);
}

static void D3D11_BindFragmentSamplers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuTextureSamplerBinding *pBindings
) {
	(void) driverData; /* used by other backends*/
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	ID3D11ShaderResourceView* srvs[MAX_TEXTURE_SAMPLERS];
	ID3D11SamplerState* d3d11Samplers[MAX_TEXTURE_SAMPLERS];

	Sint32 numFragmentSamplers = d3d11CommandBuffer->graphicsPipeline->numFragmentSamplers;

	for (Sint32 i = 0; i < numFragmentSamplers; i += 1)
	{
		srvs[i] = ((D3D11TextureContainer*) pBindings[i].texture)->activeTexture->shaderView;
		d3d11Samplers[i] = ((D3D11Sampler*) pBindings[i].sampler)->handle;
	}

	ID3D11DeviceContext_PSSetShaderResources(
		d3d11CommandBuffer->context,
		0,
		numFragmentSamplers,
		srvs
	);

	ID3D11DeviceContext_PSSetSamplers(
		d3d11CommandBuffer->context,
		0,
		numFragmentSamplers,
		d3d11Samplers
	);
}

/* Disposal */

static void D3D11_QueueDestroyTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuTexture *texture
) {
    (void) driverData; /* used by other backends */
	D3D11TextureContainer *container = (D3D11TextureContainer*) texture;

	for (Uint32 i = 0; i < container->textureCount; i += 1)
	{
		D3D11Texture *d3d11Texture = container->textures[i];

		if (d3d11Texture->shaderView)
		{
			ID3D11ShaderResourceView_Release(d3d11Texture->shaderView);
		}

		for (Uint32 subresourceIndex = 0; subresourceIndex < d3d11Texture->subresourceCount; subresourceIndex += 1)
		{
			if (d3d11Texture->subresources[subresourceIndex].msaaHandle != NULL)
			{
				ID3D11Resource_Release(d3d11Texture->subresources[subresourceIndex].msaaHandle);
			}

			if (d3d11Texture->subresources[subresourceIndex].msaaTargetView != NULL)
			{
				ID3D11RenderTargetView_Release(d3d11Texture->subresources[subresourceIndex].msaaTargetView);
			}

			if (d3d11Texture->subresources[subresourceIndex].colorTargetView != NULL)
			{
				ID3D11RenderTargetView_Release(d3d11Texture->subresources[subresourceIndex].colorTargetView);
			}

			if (d3d11Texture->subresources[subresourceIndex].depthStencilTargetView != NULL)
			{
				ID3D11DepthStencilView_Release(d3d11Texture->subresources[subresourceIndex].depthStencilTargetView);
			}

			if (d3d11Texture->subresources[subresourceIndex].uav != NULL)
			{
				ID3D11UnorderedAccessView_Release(d3d11Texture->subresources[subresourceIndex].uav);
			}
		}
		SDL_free(d3d11Texture->subresources);

		ID3D11Resource_Release(d3d11Texture->handle);
	}

	SDL_free(container->textures);
	SDL_free(container);
}

static void D3D11_QueueDestroySampler(
	SDL_GpuRenderer *driverData,
	SDL_GpuSampler *sampler
) {
    (void) driverData; /* used by other backends */
	D3D11Sampler *d3d11Sampler = (D3D11Sampler*) sampler;
	ID3D11SamplerState_Release(d3d11Sampler->handle);
	SDL_free(d3d11Sampler);
}

static void D3D11_QueueDestroyGpuBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuBuffer *gpuBuffer
) {
    (void) driverData; /* used by other backends */
	D3D11BufferContainer *container = (D3D11BufferContainer*) gpuBuffer;

	for (Uint32 i = 0; i < container->bufferCount; i += 1)
	{
		D3D11Buffer *d3d11Buffer = container->buffers[i];

		if (d3d11Buffer->uav)
		{
			ID3D11UnorderedAccessView_Release(d3d11Buffer->uav);
		}

		ID3D11Buffer_Release(d3d11Buffer->handle);

		SDL_free(d3d11Buffer);
	}

	SDL_free(container->buffers);
	SDL_free(container);
}

static void D3D11_QueueDestroyTransferBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuTransferBuffer *transferBuffer
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;

	SDL_LockMutex(renderer->contextLock);

	EXPAND_ARRAY_IF_NEEDED(
		renderer->transferBufferContainersToDestroy,
		D3D11TransferBufferContainer*,
		renderer->transferBufferContainersToDestroyCount + 1,
		renderer->transferBufferContainersToDestroyCapacity,
		renderer->transferBufferContainersToDestroyCapacity + 1
	);

	renderer->transferBufferContainersToDestroy[
		renderer->transferBufferContainersToDestroyCount
	] = (D3D11TransferBufferContainer*) transferBuffer;
	renderer->transferBufferContainersToDestroyCount += 1;

	SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_INTERNAL_DestroyTransferBufferContainer(
	D3D11TransferBufferContainer *transferBufferContainer
) {
	for (Uint32 i = 0; i < transferBufferContainer->bufferCount; i += 1)
	{
		if (transferBufferContainer->usage == SDL_GPU_TRANSFERUSAGE_BUFFER)
		{
			ID3D11Buffer_Release(transferBufferContainer->buffers[i]->bufferTransfer.stagingBuffer);
		}
		else /* TEXTURE */
		{
			SDL_free(transferBufferContainer->buffers[i]->textureTransfer.data);
		}
		SDL_free(transferBufferContainer->buffers[i]);
	}
	SDL_free(transferBufferContainer->buffers);
}

static void D3D11_QueueDestroyShaderModule(
	SDL_GpuRenderer *driverData,
	SDL_GpuShaderModule *shaderModule
) {
    (void) driverData; /* used by other backends */
	D3D11ShaderModule *d3dShaderModule = (D3D11ShaderModule*) shaderModule;

	if (d3dShaderModule->shader)
	{
		ID3D11DeviceChild_Release(d3dShaderModule->shader);
	}
	if (d3dShaderModule->blob)
	{
		ID3D10Blob_Release(d3dShaderModule->blob);
	}

	SDL_free(d3dShaderModule);
}

static void D3D11_QueueDestroyComputePipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuComputePipeline *computePipeline
) {
	D3D11ComputePipeline *d3d11ComputePipeline = (D3D11ComputePipeline*) computePipeline;
	SDL_free(d3d11ComputePipeline);
}

static void D3D11_QueueDestroyGraphicsPipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuGraphicsPipeline *graphicsPipeline
) {
    (void) driverData; /* used by other backends */
	D3D11GraphicsPipeline *d3d11GraphicsPipeline = (D3D11GraphicsPipeline*) graphicsPipeline;

	ID3D11BlendState_Release(d3d11GraphicsPipeline->colorAttachmentBlendState);
	ID3D11DepthStencilState_Release(d3d11GraphicsPipeline->depthStencilState);
	ID3D11RasterizerState_Release(d3d11GraphicsPipeline->rasterizerState);

	if (d3d11GraphicsPipeline->inputLayout)
	{
		ID3D11InputLayout_Release(d3d11GraphicsPipeline->inputLayout);
	}
	if (d3d11GraphicsPipeline->vertexStrides)
	{
		SDL_free(d3d11GraphicsPipeline->vertexStrides);
	}

	SDL_free(d3d11GraphicsPipeline);
}

/* Graphics State */

static void D3D11_INTERNAL_AllocateCommandBuffers(
	D3D11Renderer *renderer,
	Uint32 allocateCount
) {
	D3D11CommandBuffer *commandBuffer;
	HRESULT res;

	renderer->availableCommandBufferCapacity += allocateCount;

	renderer->availableCommandBuffers = SDL_realloc(
		renderer->availableCommandBuffers,
		sizeof(D3D11CommandBuffer*) * renderer->availableCommandBufferCapacity
	);

	for (Uint32 i = 0; i < allocateCount; i += 1)
	{
		commandBuffer = SDL_malloc(sizeof(D3D11CommandBuffer));

		/* Deferred Device Context */
		res = ID3D11Device1_CreateDeferredContext1(
			renderer->device,
			0,
			&commandBuffer->context
		);
		ERROR_CHECK("Could not create deferred context");

		/* Bound Uniform Buffers */
		commandBuffer->boundUniformBufferCapacity = 16;
		commandBuffer->boundUniformBufferCount = 0;
		commandBuffer->boundUniformBuffers = SDL_malloc(
			commandBuffer->boundUniformBufferCapacity * sizeof(D3D11UniformBuffer*)
		);

		/* Reference Counting */
		commandBuffer->usedGpuBufferCapacity = 4;
		commandBuffer->usedGpuBufferCount = 0;
		commandBuffer->usedGpuBuffers = SDL_malloc(
			commandBuffer->usedGpuBufferCapacity * sizeof(D3D11Buffer*)
		);

		commandBuffer->usedTransferBufferCapacity = 4;
		commandBuffer->usedTransferBufferCount = 0;
		commandBuffer->usedTransferBuffers = SDL_malloc(
			commandBuffer->usedTransferBufferCapacity * sizeof(D3D11TransferBuffer*)
		);

		commandBuffer->usedTextureSubresourceCapacity = 4;
		commandBuffer->usedTextureSubresourceCount = 0;
		commandBuffer->usedTextureSubresources = SDL_malloc(
			commandBuffer->usedTextureSubresourceCapacity * sizeof(D3D11TextureSubresource*)
		);

		renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = commandBuffer;
		renderer->availableCommandBufferCount += 1;
	}
}

static D3D11CommandBuffer* D3D11_INTERNAL_GetInactiveCommandBufferFromPool(
	D3D11Renderer *renderer
) {
	D3D11CommandBuffer *commandBuffer;

	if (renderer->availableCommandBufferCount == 0)
	{
		D3D11_INTERNAL_AllocateCommandBuffers(
			renderer,
			renderer->availableCommandBufferCapacity
		);
	}

	commandBuffer = renderer->availableCommandBuffers[renderer->availableCommandBufferCount - 1];
	renderer->availableCommandBufferCount -= 1;

	return commandBuffer;
}

static Uint8 D3D11_INTERNAL_CreateFence(
	D3D11Renderer *renderer
) {
	D3D11_QUERY_DESC queryDesc;
	ID3D11Query *queryHandle;
	D3D11Fence* fence;
	HRESULT res;

	queryDesc.Query = D3D11_QUERY_EVENT;
	queryDesc.MiscFlags = 0;
	res = ID3D11Device_CreateQuery(
		renderer->device,
		&queryDesc,
		&queryHandle
	);
	ERROR_CHECK_RETURN("Could not create query", 0);

	fence = SDL_malloc(sizeof(D3D11Fence));
	fence->handle = queryHandle;

	/* Add it to the available pool */
	if (renderer->availableFenceCount >= renderer->availableFenceCapacity)
	{
		renderer->availableFenceCapacity *= 2;

		renderer->availableFences = SDL_realloc(
			renderer->availableFences,
			sizeof(D3D11Fence*) * renderer->availableFenceCapacity
		);
	}

	renderer->availableFences[renderer->availableFenceCount] = fence;
	renderer->availableFenceCount += 1;

	return 1;
}

static Uint8 D3D11_INTERNAL_AcquireFence(
	D3D11Renderer *renderer,
	D3D11CommandBuffer *commandBuffer
) {
	D3D11Fence *fence;

	/* Acquire a fence from the pool */
	SDL_LockMutex(renderer->fenceLock);

	if (renderer->availableFenceCount == 0)
	{
		if (!D3D11_INTERNAL_CreateFence(renderer))
		{
			SDL_UnlockMutex(renderer->fenceLock);
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create fence!");
			return 0;
		}
	}

	fence = renderer->availableFences[renderer->availableFenceCount - 1];
	renderer->availableFenceCount -= 1;

	SDL_UnlockMutex(renderer->fenceLock);

	/* Associate the fence with the command buffer */
	commandBuffer->fence = fence;

	return 1;
}

static SDL_GpuCommandBuffer* D3D11_AcquireCommandBuffer(
	SDL_GpuRenderer *driverData
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *commandBuffer;

	SDL_LockMutex(renderer->acquireCommandBufferLock);

	commandBuffer = D3D11_INTERNAL_GetInactiveCommandBufferFromPool(renderer);
	commandBuffer->windowData = NULL;
	commandBuffer->graphicsPipeline = NULL;
	commandBuffer->computePipeline = NULL;
	commandBuffer->vertexUniformBuffer = NULL;
	commandBuffer->fragmentUniformBuffer = NULL;
	commandBuffer->computeUniformBuffer = NULL;
	for (Uint32 i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1)
	{
		commandBuffer->colorTargetResolveTexture[i] = NULL;
		commandBuffer->colorTargetResolveSubresourceIndex[i] = 0;
		commandBuffer->colorTargetMsaaHandle[i] = NULL;
	}

	D3D11_INTERNAL_AcquireFence(renderer, commandBuffer);
	commandBuffer->autoReleaseFence = 1;

	SDL_UnlockMutex(renderer->acquireCommandBufferLock);

	return (SDL_GpuCommandBuffer*) commandBuffer;
}

static void D3D11_BeginRenderPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuColorAttachmentInfo *colorAttachmentInfos,
	Uint32 colorAttachmentCount,
	SDL_GpuDepthStencilAttachmentInfo *depthStencilAttachmentInfo
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	ID3D11RenderTargetView* rtvs[MAX_COLOR_TARGET_BINDINGS];
	ID3D11DepthStencilView *dsv = NULL;
	Uint32 vpWidth = UINT_MAX;
	Uint32 vpHeight = UINT_MAX;
	D3D11_VIEWPORT viewport;
	D3D11_RECT scissorRect;

	/* Clear the bound targets for the current command buffer */
	for (Uint32 i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1)
	{
		d3d11CommandBuffer->colorTargetResolveTexture[i] = NULL;
		d3d11CommandBuffer->colorTargetResolveSubresourceIndex[i] = 0;
		d3d11CommandBuffer->colorTargetMsaaHandle[i] = NULL;
	}

	/* Set up the new color target bindings */
	for (Uint32 i = 0; i < colorAttachmentCount; i += 1)
	{
		D3D11TextureContainer *container = (D3D11TextureContainer*) colorAttachmentInfos[i].textureSlice.texture;
		D3D11TextureSubresource *subresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
			renderer,
			container,
			colorAttachmentInfos[i].textureSlice.layer,
			colorAttachmentInfos[i].textureSlice.mipLevel,
			colorAttachmentInfos[i].writeOption == SDL_GPU_TEXTUREWRITEOPTIONS_CYCLE
		);

		if (subresource->msaaHandle != NULL)
		{
			d3d11CommandBuffer->colorTargetResolveTexture[i] = subresource->parent;
			d3d11CommandBuffer->colorTargetResolveSubresourceIndex[i] = subresource->index;
			d3d11CommandBuffer->colorTargetMsaaHandle[i] = subresource->msaaHandle;

			rtvs[i] = subresource->msaaTargetView;
		}
		else
		{
			rtvs[i] = subresource->colorTargetView;
		}
	}

	/* Get the DSV for the depth stencil attachment, if applicable */
	if (depthStencilAttachmentInfo != NULL)
	{
		D3D11TextureContainer *container = (D3D11TextureContainer*) depthStencilAttachmentInfo->textureSlice.texture;
		D3D11TextureSubresource *subresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
			renderer,
			container,
			depthStencilAttachmentInfo->textureSlice.layer,
			depthStencilAttachmentInfo->textureSlice.mipLevel,
			depthStencilAttachmentInfo->writeOption == SDL_GPU_TEXTUREWRITEOPTIONS_CYCLE
		);

		dsv = subresource->depthStencilTargetView;
	}

	/* Actually set the RTs */
	ID3D11DeviceContext_OMSetRenderTargets(
		d3d11CommandBuffer->context,
		colorAttachmentCount,
		colorAttachmentCount > 0 ? rtvs : NULL,
		dsv
	);

	/* Perform load ops on the RTs */
	for (Uint32 i = 0; i < colorAttachmentCount; i += 1)
	{
		if (colorAttachmentInfos[i].loadOp == SDL_GPU_LOADOP_CLEAR)
		{
			float clearColors[] =
			{
				colorAttachmentInfos[i].clearColor.x,
				colorAttachmentInfos[i].clearColor.y,
				colorAttachmentInfos[i].clearColor.z,
				colorAttachmentInfos[i].clearColor.w
			};
			ID3D11DeviceContext_ClearRenderTargetView(
				d3d11CommandBuffer->context,
				rtvs[i],
				clearColors
			);
		}
	}

	if (depthStencilAttachmentInfo != NULL)
	{
		D3D11_CLEAR_FLAG dsClearFlags = 0;
		if (depthStencilAttachmentInfo->loadOp == SDL_GPU_LOADOP_CLEAR)
		{
			dsClearFlags |= D3D11_CLEAR_DEPTH;
		}
		if (depthStencilAttachmentInfo->stencilLoadOp == SDL_GPU_LOADOP_CLEAR)
		{
			dsClearFlags |= D3D11_CLEAR_STENCIL;
		}

		if (dsClearFlags != 0)
		{
			ID3D11DeviceContext_ClearDepthStencilView(
				d3d11CommandBuffer->context,
				dsv,
				dsClearFlags,
				depthStencilAttachmentInfo->depthStencilClearValue.depth,
				(Uint8) depthStencilAttachmentInfo->depthStencilClearValue.stencil
			);
		}
	}

	/* The viewport cannot be larger than the smallest attachment. */
	for (Uint32 i = 0; i < colorAttachmentCount; i += 1)
	{
		D3D11Texture *texture = ((D3D11TextureContainer*) colorAttachmentInfos[i].textureSlice.texture)->activeTexture;
		Uint32 w = texture->width >> colorAttachmentInfos[i].textureSlice.mipLevel;
		Uint32 h = texture->height >> colorAttachmentInfos[i].textureSlice.mipLevel;

		if (w < vpWidth)
		{
			vpWidth = w;
		}

		if (h < vpHeight)
		{
			vpHeight = h;
		}
	}

	if (depthStencilAttachmentInfo != NULL)
	{
		D3D11Texture *texture = ((D3D11TextureContainer*) depthStencilAttachmentInfo->textureSlice.texture)->activeTexture;
		Uint32 w = texture->width >> depthStencilAttachmentInfo->textureSlice.mipLevel;
		Uint32 h = texture->height >> depthStencilAttachmentInfo->textureSlice.mipLevel;

		if (w < vpWidth)
		{
			vpWidth = w;
		}

		if (h < vpHeight)
		{
			vpHeight = h;
		}
	}

	/* Set default viewport and scissor state */
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = (FLOAT) vpWidth;
	viewport.Height = (FLOAT) vpHeight;
	viewport.MinDepth = 0;
	viewport.MaxDepth = 1;

	ID3D11DeviceContext_RSSetViewports(
		d3d11CommandBuffer->context,
		1,
		&viewport
	);

	scissorRect.left = 0;
	scissorRect.right = (LONG) viewport.Width;
	scissorRect.top = 0;
	scissorRect.bottom = (LONG) viewport.Height;

	ID3D11DeviceContext_RSSetScissorRects(
		d3d11CommandBuffer->context,
		1,
		&scissorRect
	);
}

static void D3D11_EndRenderPass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;

	d3d11CommandBuffer->vertexUniformBuffer = NULL;
	d3d11CommandBuffer->fragmentUniformBuffer = NULL;
	d3d11CommandBuffer->computeUniformBuffer = NULL;

	/* Resolve MSAA color render targets */
	for (Uint32 i = 0; i < MAX_COLOR_TARGET_BINDINGS; i += 1)
	{
		if (d3d11CommandBuffer->colorTargetMsaaHandle[i] != NULL)
		{
			ID3D11DeviceContext_ResolveSubresource(
				d3d11CommandBuffer->context,
				d3d11CommandBuffer->colorTargetResolveTexture[i]->handle,
				d3d11CommandBuffer->colorTargetResolveSubresourceIndex[i],
				d3d11CommandBuffer->colorTargetMsaaHandle[i],
				0,
				RefreshToD3D11_TextureFormat[d3d11CommandBuffer->colorTargetResolveTexture[i]->format]
			);
		}
	}
}

static void D3D11_BindGraphicsPipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuGraphicsPipeline *graphicsPipeline
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11GraphicsPipeline *pipeline = (D3D11GraphicsPipeline*) graphicsPipeline;

	d3d11CommandBuffer->graphicsPipeline = pipeline;

	/* Get a vertex uniform buffer if we need one */
	if (d3d11CommandBuffer->vertexUniformBuffer == NULL && pipeline->vertexUniformBlockSize > 0)
	{
		D3D11_INTERNAL_AcquireUniformBuffer(
			renderer,
			d3d11CommandBuffer,
			&d3d11CommandBuffer->vertexUniformBuffer
		);
	}

	/* Get a fragment uniform buffer if we need one */
	if (d3d11CommandBuffer->fragmentUniformBuffer == NULL && pipeline->fragmentUniformBlockSize > 0)
	{
		D3D11_INTERNAL_AcquireUniformBuffer(
			renderer,
			d3d11CommandBuffer,
			&d3d11CommandBuffer->fragmentUniformBuffer
		);
	}

	ID3D11DeviceContext_OMSetBlendState(
		d3d11CommandBuffer->context,
		pipeline->colorAttachmentBlendState,
		pipeline->blendConstants,
		pipeline->multisampleState.sampleMask
	);

	ID3D11DeviceContext_OMSetDepthStencilState(
		d3d11CommandBuffer->context,
		pipeline->depthStencilState,
		pipeline->stencilRef
	);

	ID3D11DeviceContext_IASetPrimitiveTopology(
		d3d11CommandBuffer->context,
		RefreshToD3D11_PrimitiveType[pipeline->primitiveType]
	);

	ID3D11DeviceContext_IASetInputLayout(
		d3d11CommandBuffer->context,
		pipeline->inputLayout
	);

	ID3D11DeviceContext_RSSetState(
		d3d11CommandBuffer->context,
		pipeline->rasterizerState
	);

	ID3D11DeviceContext_VSSetShader(
		d3d11CommandBuffer->context,
		pipeline->vertexShader,
		NULL,
		0
	);

	ID3D11DeviceContext_PSSetShader(
		d3d11CommandBuffer->context,
		pipeline->fragmentShader,
		NULL,
		0
	);
}

static void D3D11_SetViewport(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuViewport *viewport
) {
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11_VIEWPORT vp =
	{
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
		&vp
	);
}

static void D3D11_SetScissor(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuRect *scissor
) {
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11_RECT rect =
	{
		scissor->x,
		scissor->y,
		scissor->x + scissor->w,
		scissor->y + scissor->h
	};

	ID3D11DeviceContext_RSSetScissorRects(
		d3d11CommandBuffer->context,
		1,
		&rect
	);
}

static void D3D11_BindVertexBuffers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	Uint32 firstBinding,
	Uint32 bindingCount,
	SDL_GpuBufferBinding *pBindings
) {
    (void) driverData; /* used by other backends */
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	ID3D11Buffer *bufferHandles[MAX_BUFFER_BINDINGS];
	UINT bufferOffsets[MAX_BUFFER_BINDINGS];

	for (Uint32 i = 0; i < bindingCount; i += 1)
	{
		D3D11Buffer *currentBuffer = ((D3D11BufferContainer*) pBindings[i].gpuBuffer)->activeBuffer;
		bufferHandles[i] = currentBuffer->handle;
		bufferOffsets[i] = pBindings[i].offset;
		D3D11_INTERNAL_TrackGpuBuffer(d3d11CommandBuffer, currentBuffer);
	}

	ID3D11DeviceContext_IASetVertexBuffers(
		d3d11CommandBuffer->context,
		firstBinding,
		bindingCount,
		bufferHandles,
		&d3d11CommandBuffer->graphicsPipeline->vertexStrides[firstBinding],
		bufferOffsets
	);
}

static void D3D11_BindIndexBuffer(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuBufferBinding *pBinding,
	SDL_GpuIndexElementSize indexElementSize
) {
    (void) driverData; /* used by other backends */
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11Buffer *d3d11Buffer = ((D3D11BufferContainer*) pBinding->gpuBuffer)->activeBuffer;

	D3D11_INTERNAL_TrackGpuBuffer(d3d11CommandBuffer, d3d11Buffer);

	ID3D11DeviceContext_IASetIndexBuffer(
		d3d11CommandBuffer->context,
		d3d11Buffer->handle,
		RefreshToD3D11_IndexType[indexElementSize],
		(UINT) pBinding->offset
	);
}

/* Compute State */

static void D3D11_BeginComputePass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	/* no-op */
}

static void D3D11_BindComputePipeline(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputePipeline *computePipeline
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11ComputePipeline *pipeline = (D3D11ComputePipeline*) computePipeline;

	d3d11CommandBuffer->computePipeline = pipeline;

	/* Get a compute uniform buffer if we need one */
	if (d3d11CommandBuffer->computeUniformBuffer == NULL && pipeline->computeUniformBlockSize > 0)
	{
		D3D11_INTERNAL_AcquireUniformBuffer(
			renderer,
			d3d11CommandBuffer,
			&d3d11CommandBuffer->computeUniformBuffer
		);
	}

	ID3D11DeviceContext_CSSetShader(
		d3d11CommandBuffer->context,
		pipeline->computeShader,
		NULL,
		0
	);
}

static void D3D11_BindComputeBuffers(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeBufferBinding *pBindings
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	ID3D11UnorderedAccessView* uavs[MAX_BUFFER_BINDINGS];

	Sint32 numBuffers = d3d11CommandBuffer->computePipeline->numBuffers;

	for (Sint32 i = 0; i < numBuffers; i += 1)
	{
		D3D11BufferContainer *currentBufferContainer = (D3D11BufferContainer*) pBindings[i].gpuBuffer;
		D3D11Buffer* currentBuffer = D3D11_INTERNAL_PrepareGpuBufferForWrite(
			renderer,
			currentBufferContainer,
			pBindings[i].writeOption == SDL_GPU_COMPUTEBINDOPTIONS_CYCLE
		);
		uavs[i] = currentBuffer->uav;
		D3D11_INTERNAL_TrackGpuBuffer(d3d11CommandBuffer, currentBuffer);
	}

	ID3D11DeviceContext_CSSetUnorderedAccessViews(
		d3d11CommandBuffer->context,
		0,
		numBuffers,
		uavs,
		NULL
	);
}

static void D3D11_BindComputeTextures(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	SDL_GpuComputeTextureBinding *pBindings
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	ID3D11UnorderedAccessView *uavs[MAX_TEXTURE_SAMPLERS];

	Sint32 numTextures = d3d11CommandBuffer->computePipeline->numTextures;

	for (Sint32 i = 0; i < numTextures; i += 1)
	{
		D3D11TextureContainer *textureContainer = ((D3D11TextureContainer*) pBindings[i].textureSlice.texture);
		D3D11TextureSubresource *subresource = D3D11_INTERNAL_PrepareTextureSubresourceForWrite(
			renderer,
			textureContainer,
			pBindings[i].textureSlice.layer,
			pBindings[i].textureSlice.mipLevel,
			pBindings[i].writeOption == SDL_GPU_COMPUTEBINDOPTIONS_CYCLE
		);

		uavs[i] = subresource->uav;
	}

	ID3D11DeviceContext_CSSetUnorderedAccessViews(
		d3d11CommandBuffer->context,
		0,
		numTextures,
		uavs,
		NULL
	);
}

static void D3D11_EndComputePass(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	/* no-op */
}

/* Window and Swapchain Management */

static D3D11WindowData* D3D11_INTERNAL_FetchWindowData(
	void *windowHandle
) {
	SDL_PropertiesID properties = SDL_GetWindowProperties(windowHandle);
	return (D3D11WindowData*) SDL_GetProperty(properties, WINDOW_PROPERTY_DATA, NULL);
}

static Uint8 D3D11_INTERNAL_InitializeSwapchainTexture(
	D3D11Renderer *renderer,
	IDXGISwapChain *swapchain,
	D3D11Texture *pTexture
) {
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
		(void**) &swapchainTexture
	);
	ERROR_CHECK_RETURN("Could not get buffer from swapchain!", 0);

	/* Create the SRV for the swapchain */
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	res = ID3D11Device_CreateShaderResourceView(
		renderer->device,
		(ID3D11Resource *)swapchainTexture,
		&srvDesc,
		&srv
	);
	if (FAILED(res))
	{
		ID3D11Texture2D_Release(swapchainTexture);
		D3D11_INTERNAL_LogError(renderer->device, "Swapchain SRV creation failed", res);
		return 0;
	}

	/* Create the RTV for the swapchain */
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	res = ID3D11Device_CreateRenderTargetView(
		renderer->device,
		(ID3D11Resource*) swapchainTexture,
		&rtvDesc,
		&rtv
	);
	if (FAILED(res))
	{
		ID3D11ShaderResourceView_Release(srv);
		ID3D11Texture2D_Release(swapchainTexture);
		D3D11_INTERNAL_LogError(renderer->device, "Swapchain RTV creation failed", res);
		return 0;
	}

	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	res = ID3D11Device_CreateUnorderedAccessView(
		renderer->device,
		(ID3D11Resource *)swapchainTexture,
		&uavDesc,
		&uav
	);
	if (FAILED(res))
	{
		ID3D11ShaderResourceView_Release(srv);
		ID3D11RenderTargetView_Release(rtv);
		ID3D11Texture2D_Release(swapchainTexture);
		D3D11_INTERNAL_LogError(renderer->device, "Swapchain UAV creation failed", res);
		return 0;
	}

	/* Fill out the texture struct */
	pTexture->handle = NULL; /* This will be set in AcquireSwapchainTexture. */
	pTexture->shaderView = srv;
	pTexture->subresourceCount = 1;
	pTexture->subresources = SDL_malloc(sizeof(D3D11TextureSubresource));
	pTexture->subresources[0].colorTargetView = rtv;
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

	return 1;
}

static Uint8 D3D11_INTERNAL_CreateSwapchain(
	D3D11Renderer *renderer,
	D3D11WindowData *windowData,
	SDL_GpuPresentMode presentMode
) {
	HWND dxgiHandle;
	int width, height;
	DXGI_SWAP_CHAIN_DESC swapchainDesc;
	IDXGIFactory1 *pParent;
	IDXGISwapChain *swapchain;
	HRESULT res;

	/* Get the DXGI handle */
	dxgiHandle = (HWND) SDL_GetProperty(SDL_GetWindowProperties(windowData->windowHandle), SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

	/* Get the window size */
	SDL_GetWindowSize((SDL_Window*) windowData->windowHandle, &width, &height);

	/* Initialize the swapchain buffer descriptor */
	swapchainDesc.BufferDesc.Width = 0;
	swapchainDesc.BufferDesc.Height = 0;
	swapchainDesc.BufferDesc.RefreshRate.Numerator = 0;
	swapchainDesc.BufferDesc.RefreshRate.Denominator = 0;
	/* TODO: support different swapchain formats? */
	swapchainDesc.BufferDesc.Format = RefreshToD3D11_TextureFormat[SDL_GPU_TEXTUREFORMAT_R8G8B8A8];
	swapchainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapchainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	/* Initialize the rest of the swapchain descriptor */
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.OutputWindow = dxgiHandle;
	swapchainDesc.Windowed = 1;

	if (renderer->supportsTearing)
	{
		swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
		/* We know this is supported because tearing support implies DXGI 1.5+ */
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	}
	else
	{
		swapchainDesc.Flags = 0;
		swapchainDesc.SwapEffect = (
			renderer->supportsFlipDiscard ?
				DXGI_SWAP_EFFECT_FLIP_DISCARD :
				DXGI_SWAP_EFFECT_DISCARD
		);
	}

	/* Create the swapchain! */
	res = IDXGIFactory1_CreateSwapChain(
		(IDXGIFactory1*) renderer->factory,
		(IUnknown*) renderer->device,
		&swapchainDesc,
		&swapchain
	);
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
		(void**) &pParent
	);
	if (FAILED(res))
	{
		SDL_LogWarn(
            SDL_LOG_CATEGORY_APPLICATION,
			"Could not get swapchain parent! Error Code: %08lX",
			res
		);
	}
	else
	{
		/* Disable DXGI window crap */
		res = IDXGIFactory1_MakeWindowAssociation(
			pParent,
			dxgiHandle,
			DXGI_MWA_NO_WINDOW_CHANGES
		);
		if (FAILED(res))
		{
			SDL_LogWarn(
                SDL_LOG_CATEGORY_APPLICATION,
				"MakeWindowAssociation failed! Error Code: %08lX",
				res
			);
		}

		/* We're done with the parent now */
		IDXGIFactory1_Release(pParent);
	}

	/* Initialize the swapchain data */
	windowData->swapchain = swapchain;
	windowData->presentMode = presentMode;

	if (!D3D11_INTERNAL_InitializeSwapchainTexture(
		renderer,
		swapchain,
		&windowData->texture
	)) {
		IDXGISwapChain_Release(swapchain);
		return 0;
	}

	return 1;
}

static Uint8 D3D11_INTERNAL_ResizeSwapchain(
	D3D11Renderer *renderer,
	D3D11WindowData *windowData,
	Sint32 width,
	Sint32 height
) {
	/* Release the old views */
	ID3D11ShaderResourceView_Release(windowData->texture.shaderView);
	ID3D11RenderTargetView_Release(windowData->texture.subresources[0].colorTargetView);
	ID3D11UnorderedAccessView_Release(windowData->texture.subresources[0].uav);
	SDL_free(windowData->texture.subresources);

	/* Resize the swapchain */
	HRESULT res = IDXGISwapChain_ResizeBuffers(
		windowData->swapchain,
		0, /* Keep buffer count the same */
		width,
		height,
		DXGI_FORMAT_UNKNOWN, /* Keep the old format */
		renderer->supportsTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0
	);
	ERROR_CHECK_RETURN("Could not resize swapchain buffers", 0);

	/* Create the texture object for the swapchain */
	return D3D11_INTERNAL_InitializeSwapchainTexture(
		renderer,
		windowData->swapchain,
		&windowData->texture
	);
}

static Uint8 D3D11_ClaimWindow(
	SDL_GpuRenderer *driverData,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11WindowData *windowData = D3D11_INTERNAL_FetchWindowData(windowHandle);

	if (windowData == NULL)
	{
		windowData = (D3D11WindowData*) SDL_malloc(sizeof(D3D11WindowData));
		windowData->windowHandle = windowHandle;

		if (D3D11_INTERNAL_CreateSwapchain(renderer, windowData, presentMode))
		{
            SDL_SetProperty(SDL_GetWindowProperties(windowHandle), WINDOW_PROPERTY_DATA, windowData);

			SDL_LockMutex(renderer->windowLock);

			if (renderer->claimedWindowCount >= renderer->claimedWindowCapacity)
			{
				renderer->claimedWindowCapacity *= 2;
				renderer->claimedWindows = SDL_realloc(
					renderer->claimedWindows,
					renderer->claimedWindowCapacity * sizeof(D3D11WindowData*)
				);
			}
			renderer->claimedWindows[renderer->claimedWindowCount] = windowData;
			renderer->claimedWindowCount += 1;

			SDL_UnlockMutex(renderer->windowLock);

			return 1;
		}
		else
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not create swapchain, failed to claim window!");
			SDL_free(windowData);
			return 0;
		}
	}
	else
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Window already claimed!");
		return 0;
	}
}

static void D3D11_UnclaimWindow(
	SDL_GpuRenderer *driverData,
	void *windowHandle
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11WindowData *windowData = D3D11_INTERNAL_FetchWindowData(windowHandle);

	if (windowData == NULL)
	{
		return;
	}

	D3D11_Wait(driverData);

	ID3D11ShaderResourceView_Release(windowData->texture.shaderView);
	ID3D11RenderTargetView_Release(windowData->texture.subresources[0].colorTargetView);
	ID3D11UnorderedAccessView_Release(windowData->texture.subresources[0].uav);
	SDL_free(windowData->texture.subresources);
	IDXGISwapChain_Release(windowData->swapchain);

	SDL_LockMutex(renderer->windowLock);
	for (Uint32 i = 0; i < renderer->claimedWindowCount; i += 1)
	{
		if (renderer->claimedWindows[i]->windowHandle == windowHandle)
		{
			renderer->claimedWindows[i] = renderer->claimedWindows[renderer->claimedWindowCount - 1];
			renderer->claimedWindowCount -= 1;
			break;
		}
	}
	SDL_UnlockMutex(renderer->windowLock);

	SDL_free(windowData);

    SDL_ClearProperty(SDL_GetWindowProperties(windowHandle), WINDOW_PROPERTY_DATA);
}

static SDL_GpuTexture* D3D11_AcquireSwapchainTexture(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer,
	void *windowHandle,
	Uint32 *pWidth,
	Uint32 *pHeight
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11WindowData *windowData;
	DXGI_SWAP_CHAIN_DESC swapchainDesc;
	int w, h;
	HRESULT res;

	windowData = D3D11_INTERNAL_FetchWindowData(windowHandle);
	if (windowData == NULL)
	{
		return NULL;
	}

	/* Check for window size changes and resize the swapchain if needed. */
	IDXGISwapChain_GetDesc(windowData->swapchain, &swapchainDesc);
	SDL_GetWindowSize((SDL_Window*) windowHandle, &w, &h);

	if (w != swapchainDesc.BufferDesc.Width || h != swapchainDesc.BufferDesc.Height)
	{
		res = D3D11_INTERNAL_ResizeSwapchain(
			renderer,
			windowData,
			w,
			h
		);
		ERROR_CHECK_RETURN("Could not resize swapchain", NULL);
	}

	/* Set the handle on the windowData texture data. */
	res = IDXGISwapChain_GetBuffer(
		windowData->swapchain,
		0,
		&D3D_IID_ID3D11Texture2D,
		(void**) &windowData->texture.handle
	);
	ERROR_CHECK_RETURN("Could not acquire swapchain!", NULL);

	/* Let the command buffer know it's associated with this swapchain. */
	d3d11CommandBuffer->windowData = windowData;

	/* Send the dimensions to the out parameters. */
	*pWidth = windowData->texture.width;
	*pHeight = windowData->texture.height;

    /* Set up the texture container */
    SDL_zero(windowData->textureContainer);
    windowData->textureContainer.canBeCycled = 0;
    windowData->textureContainer.activeTexture = &windowData->texture;
    windowData->textureContainer.textureCapacity = 1;
    windowData->textureContainer.textureCount = 1;

	/* Return the swapchain texture */
	return (SDL_GpuTexture*) &windowData->textureContainer;
}

static SDL_GpuTextureFormat D3D11_GetSwapchainFormat(
	SDL_GpuRenderer *driverData,
	void *windowHandle
) {
	return SDL_GPU_TEXTUREFORMAT_R8G8B8A8;
}

static void D3D11_SetSwapchainPresentMode(
	SDL_GpuRenderer *driverData,
	void *windowHandle,
	SDL_GpuPresentMode presentMode
) {
	D3D11WindowData *windowData = D3D11_INTERNAL_FetchWindowData(windowHandle);
	windowData->presentMode = presentMode;
}

/* Submission and Fences */

static void D3D11_INTERNAL_ReleaseFenceToPool(
	D3D11Renderer *renderer,
	D3D11Fence *fence
) {
	SDL_LockMutex(renderer->fenceLock);

	if (renderer->availableFenceCount == renderer->availableFenceCapacity)
	{
		renderer->availableFenceCapacity *= 2;
		renderer->availableFences = SDL_realloc(
			renderer->availableFences,
			renderer->availableFenceCapacity * sizeof(D3D11Fence*)
		);
	}
	renderer->availableFences[renderer->availableFenceCount] = fence;
	renderer->availableFenceCount += 1;

	SDL_UnlockMutex(renderer->fenceLock);
}

static void D3D11_INTERNAL_CleanCommandBuffer(
	D3D11Renderer *renderer,
	D3D11CommandBuffer *commandBuffer
) {
	/* Bound uniform buffers are now available */
	SDL_LockMutex(renderer->uniformBufferLock);
	for (Uint32 i = 0; i < commandBuffer->boundUniformBufferCount; i += 1)
	{
		if (renderer->availableUniformBufferCount == renderer->availableUniformBufferCapacity)
		{
			renderer->availableUniformBufferCapacity *= 2;
			renderer->availableUniformBuffers = SDL_realloc(
				renderer->availableUniformBuffers,
				renderer->availableUniformBufferCapacity * sizeof(D3D11UniformBuffer*)
			);
		}

		renderer->availableUniformBuffers[renderer->availableUniformBufferCount] = commandBuffer->boundUniformBuffers[i];
		renderer->availableUniformBufferCount += 1;
	}
	SDL_UnlockMutex(renderer->uniformBufferLock);

	commandBuffer->boundUniformBufferCount = 0;

	/* Reference Counting */

	for (Uint32 i = 0; i < commandBuffer->usedGpuBufferCount; i += 1)
	{
		(void)SDL_AtomicDecRef(&commandBuffer->usedGpuBuffers[i]->referenceCount);
	}
	commandBuffer->usedGpuBufferCount = 0;

	for (Uint32 i = 0; i < commandBuffer->usedTransferBufferCount; i += 1)
	{
		(void)SDL_AtomicDecRef(&commandBuffer->usedTransferBuffers[i]->referenceCount);
	}
	commandBuffer->usedTransferBufferCount = 0;

	for (Uint32 i = 0; i < commandBuffer->usedTextureSubresourceCount; i += 1)
	{
		(void)SDL_AtomicDecRef(&commandBuffer->usedTextureSubresources[i]->referenceCount);
	}
	commandBuffer->usedTextureSubresourceCount = 0;

	/* The fence is now available (unless SubmitAndAcquireFence was called) */
	if (commandBuffer->autoReleaseFence)
	{
		D3D11_INTERNAL_ReleaseFenceToPool(renderer, commandBuffer->fence);
	}

	/* Return command buffer to pool */
	SDL_LockMutex(renderer->acquireCommandBufferLock);
	if (renderer->availableCommandBufferCount == renderer->availableCommandBufferCapacity)
	{
		renderer->availableCommandBufferCapacity += 1;
		renderer->availableCommandBuffers = SDL_realloc(
			renderer->availableCommandBuffers,
			renderer->availableCommandBufferCapacity * sizeof(D3D11CommandBuffer*)
		);
	}
	renderer->availableCommandBuffers[renderer->availableCommandBufferCount] = commandBuffer;
	renderer->availableCommandBufferCount += 1;
	SDL_UnlockMutex(renderer->acquireCommandBufferLock);

	/* Remove this command buffer from the submitted list */
	for (Uint32 i = 0; i < renderer->submittedCommandBufferCount; i += 1)
	{
		if (renderer->submittedCommandBuffers[i] == commandBuffer)
		{
			renderer->submittedCommandBuffers[i] = renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount - 1];
			renderer->submittedCommandBufferCount -= 1;
		}
	}
}

static void D3D11_INTERNAL_WaitForFence(
	D3D11Renderer *renderer,
	D3D11Fence *fence
) {
	BOOL queryData;
	HRESULT res;

	SDL_LockMutex(renderer->contextLock);

	do
	{
		res = ID3D11DeviceContext_GetData(
			renderer->immediateContext,
			(ID3D11Asynchronous*)fence->handle,
			&queryData,
			sizeof(queryData),
			0
		);
	}
	while (res != S_OK); /* Spin until we get a result back... */

	SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_INTERNAL_PerformPendingDestroys(
	D3D11Renderer *renderer
) {
	for (Sint32 i = renderer->transferBufferContainersToDestroyCount - 1; i >= 0; i -= 1)
	{
		Sint32 referenceCount = 0;
		for (Uint32 j = 0; j < renderer->transferBufferContainersToDestroy[i]->bufferCount; j += 1)
		{
			referenceCount += SDL_AtomicGet(&renderer->transferBufferContainersToDestroy[i]->buffers[j]->referenceCount);
		}

		if (referenceCount == 0)
		{
			D3D11_INTERNAL_DestroyTransferBufferContainer(
				renderer->transferBufferContainersToDestroy[i]
			);

			renderer->transferBufferContainersToDestroy[i] = renderer->transferBufferContainersToDestroy[renderer->transferBufferContainersToDestroyCount - 1];
			renderer->transferBufferContainersToDestroyCount -= 1;
		}
	}
}

static void D3D11_Submit(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	ID3D11CommandList *commandList;
	HRESULT res;

	SDL_LockMutex(renderer->contextLock);

	/* Notify the command buffer completion query that we have completed recording */
	ID3D11DeviceContext_End(
		renderer->immediateContext,
		(ID3D11Asynchronous*) d3d11CommandBuffer->fence->handle
	);

	/* Serialize the commands into the command list */
	res = ID3D11DeviceContext_FinishCommandList(
		d3d11CommandBuffer->context,
		0,
		&commandList
	);
	ERROR_CHECK("Could not finish command list recording!");

	/* Submit the command list to the immediate context */
	ID3D11DeviceContext_ExecuteCommandList(
		renderer->immediateContext,
		commandList,
		0
	);
	ID3D11CommandList_Release(commandList);

	/* Mark the command buffer as submitted */
	if (renderer->submittedCommandBufferCount >= renderer->submittedCommandBufferCapacity)
	{
		renderer->submittedCommandBufferCapacity = renderer->submittedCommandBufferCount + 1;

		renderer->submittedCommandBuffers = SDL_realloc(
			renderer->submittedCommandBuffers,
			sizeof(D3D11CommandBuffer*) * renderer->submittedCommandBufferCapacity
		);
	}

	renderer->submittedCommandBuffers[renderer->submittedCommandBufferCount] = d3d11CommandBuffer;
	renderer->submittedCommandBufferCount += 1;

	/* Present, if applicable */
	if (d3d11CommandBuffer->windowData)
	{
		/* FIXME: Is there some way to emulate FIFO_RELAXED? */

		Uint32 syncInterval = 1;
		if (	d3d11CommandBuffer->windowData->presentMode == SDL_GPU_PRESENTMODE_IMMEDIATE ||
			(renderer->supportsFlipDiscard && d3d11CommandBuffer->windowData->presentMode == SDL_GPU_PRESENTMODE_MAILBOX)
		) {
			syncInterval = 0;
		}

		Uint32 presentFlags = 0;
		if (	renderer->supportsTearing &&
			d3d11CommandBuffer->windowData->presentMode == SDL_GPU_PRESENTMODE_IMMEDIATE	)
		{
			presentFlags = DXGI_PRESENT_ALLOW_TEARING;
		}

		IDXGISwapChain_Present(
			d3d11CommandBuffer->windowData->swapchain,
			syncInterval,
			presentFlags
		);

		ID3D11Texture2D_Release(d3d11CommandBuffer->windowData->texture.handle);
	}

	/* Check if we can perform any cleanups */
	for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1)
	{
		BOOL queryData;
		res = ID3D11DeviceContext_GetData(
			renderer->immediateContext,
			(ID3D11Asynchronous*) renderer->submittedCommandBuffers[i]->fence->handle,
			&queryData,
			sizeof(queryData),
			0
		);
		if (res == S_OK)
		{
			D3D11_INTERNAL_CleanCommandBuffer(
				renderer,
				renderer->submittedCommandBuffers[i]
			);
		}
	}

	D3D11_INTERNAL_PerformPendingDestroys(renderer);

	SDL_UnlockMutex(renderer->contextLock);
}

static SDL_GpuFence* D3D11_SubmitAndAcquireFence(
	SDL_GpuRenderer *driverData,
	SDL_GpuCommandBuffer *commandBuffer
) {
	D3D11CommandBuffer *d3d11CommandBuffer = (D3D11CommandBuffer*) commandBuffer;
	D3D11Fence *fence = d3d11CommandBuffer->fence;

	d3d11CommandBuffer->autoReleaseFence = 0;
	D3D11_Submit(driverData, commandBuffer);

	return (SDL_GpuFence*) fence;
}

static void D3D11_Wait(
	SDL_GpuRenderer *driverData
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11CommandBuffer *commandBuffer;

	/*
	 * Wait for all submitted command buffers to complete.
	 * Sort of equivalent to vkDeviceWaitIdle.
	 */
	for (Uint32 i = 0; i < renderer->submittedCommandBufferCount; i += 1)
	{
		D3D11_INTERNAL_WaitForFence(
			renderer,
			renderer->submittedCommandBuffers[i]->fence
		);
	}

	SDL_LockMutex(renderer->contextLock); /* This effectively acts as a lock around submittedCommandBuffers */

	for (Sint32 i = renderer->submittedCommandBufferCount - 1; i >= 0; i -= 1)
	{
		commandBuffer = renderer->submittedCommandBuffers[i];
		D3D11_INTERNAL_CleanCommandBuffer(renderer, commandBuffer);
	}

	D3D11_INTERNAL_PerformPendingDestroys(renderer);

	SDL_UnlockMutex(renderer->contextLock);
}

static void D3D11_WaitForFences(
	SDL_GpuRenderer *driverData,
	Uint8 waitAll,
	Uint32 fenceCount,
	SDL_GpuFence **pFences
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11Fence *fence;
	BOOL queryData;
	HRESULT res = S_FALSE;

	if (waitAll)
	{
		for (Uint32 i = 0; i < fenceCount; i += 1)
		{
			fence = (D3D11Fence*) pFences[i];
			D3D11_INTERNAL_WaitForFence(renderer, fence);
		}
	}
	else
	{
		SDL_LockMutex(renderer->contextLock);

		while (res != S_OK)
		{
			for (Uint32 i = 0; i < fenceCount; i += 1)
			{
				fence = (D3D11Fence*) pFences[i];
				res = ID3D11DeviceContext_GetData(
					renderer->immediateContext,
					(ID3D11Asynchronous*) fence->handle,
					&queryData,
					sizeof(queryData),
					0
				);
				if (res == S_OK)
				{
					break;
				}
			}
		}

		SDL_UnlockMutex(renderer->contextLock);
	}
}

static int D3D11_QueryFence(
	SDL_GpuRenderer *driverData,
	SDL_GpuFence *fence
) {
	D3D11Renderer *renderer = (D3D11Renderer*) driverData;
	D3D11Fence *d3d11Fence = (D3D11Fence*) fence;
	BOOL queryData;
	HRESULT res;

	SDL_LockMutex(renderer->contextLock);

	res = ID3D11DeviceContext_GetData(
		renderer->immediateContext,
		(ID3D11Asynchronous*) d3d11Fence->handle,
		&queryData,
		sizeof(queryData),
		0
	);

	SDL_UnlockMutex(renderer->contextLock);

	return res == S_OK;
}

static void D3D11_ReleaseFence(
	SDL_GpuRenderer *driverData,
	SDL_GpuFence *fence
) {
	D3D11_INTERNAL_ReleaseFenceToPool(
		(D3D11Renderer*) driverData,
		(D3D11Fence*) fence
	);
}

/* Device Creation */

static Uint8 D3D11_PrepareDriver(
	Uint32 *flags
) {
	void *d3d11_dll, *d3dcompiler_dll, *dxgi_dll;
	PFN_D3D11_CREATE_DEVICE D3D11CreateDeviceFunc;
	D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_1 };
	PFN_D3DCOMPILE D3DCompileFunc;
	PFN_CREATE_DXGI_FACTORY1 CreateDXGIFactoryFunc;
	HRESULT res;

	/* Can we load D3D11? */

	d3d11_dll = SDL_LoadObject(D3D11_DLL);
	if (d3d11_dll == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find " D3D11_DLL);
		return 0;
	}

	D3D11CreateDeviceFunc = (PFN_D3D11_CREATE_DEVICE) SDL_LoadFunction(
		d3d11_dll,
		D3D11_CREATE_DEVICE_FUNC
	);
	if (D3D11CreateDeviceFunc == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find function " D3D11_CREATE_DEVICE_FUNC " in " D3D11_DLL);
		SDL_UnloadObject(d3d11_dll);
		return 0;
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
		NULL
	);

	SDL_UnloadObject(d3d11_dll);

	if (FAILED(res))
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not create D3D11Device with feature level 11_0");
		return 0;
	}

	/* Can we load D3DCompiler? */

	d3dcompiler_dll = SDL_LoadObject(D3DCOMPILER_DLL);
	if (d3dcompiler_dll == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find " D3DCOMPILER_DLL);
		return 0;
	}

	D3DCompileFunc = (PFN_D3DCOMPILE) SDL_LoadFunction(
		d3dcompiler_dll,
		D3DCOMPILE_FUNC
	);
	SDL_UnloadObject(d3dcompiler_dll); /* We're not going to call this function, so we can just unload now. */
	if (D3DCompileFunc == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find function " D3DCOMPILE_FUNC " in " D3DCOMPILER_DLL);
		return 0;
	}

	/* Can we load DXGI? */

	dxgi_dll = SDL_LoadObject(DXGI_DLL);
	if (dxgi_dll == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find " DXGI_DLL);
		return 0;
	}

	CreateDXGIFactoryFunc = (PFN_CREATE_DXGI_FACTORY1) SDL_LoadFunction(
		dxgi_dll,
		CREATE_DXGI_FACTORY1_FUNC
	);
	SDL_UnloadObject(dxgi_dll); /* We're not going to call this function, so we can just unload now. */
	if (CreateDXGIFactoryFunc == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "D3D11: Could not find function " CREATE_DXGI_FACTORY1_FUNC " in " DXGI_DLL);
		return 0;
	}

	/* No window flags required */

	return 1;
}

static void D3D11_INTERNAL_TryInitializeDXGIDebug(D3D11Renderer *renderer)
{
	PFN_DXGI_GET_DEBUG_INTERFACE DXGIGetDebugInterfaceFunc;
	HRESULT res;

	renderer->dxgidebug_dll = SDL_LoadObject(DXGIDEBUG_DLL);
	if (renderer->dxgidebug_dll == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not find " DXGIDEBUG_DLL);
		return;
	}

	DXGIGetDebugInterfaceFunc = (PFN_DXGI_GET_DEBUG_INTERFACE) SDL_LoadFunction(
		renderer->dxgidebug_dll,
		DXGI_GET_DEBUG_INTERFACE_FUNC
	);
	if (DXGIGetDebugInterfaceFunc == NULL)
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " DXGI_GET_DEBUG_INTERFACE_FUNC);
		return;
	}

	res = DXGIGetDebugInterfaceFunc(&D3D_IID_IDXGIDebug, (void**) &renderer->dxgiDebug);
	if (FAILED(res))
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not get IDXGIDebug interface");
	}

	res = DXGIGetDebugInterfaceFunc(&D3D_IID_IDXGIInfoQueue, (void**) &renderer->dxgiInfoQueue);
	if (FAILED(res))
	{
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Could not get IDXGIInfoQueue interface");
	}
}

static SDL_GpuDevice* D3D11_CreateDevice(
	Uint8 debugMode
) {
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
	SDL_GpuDevice* result;

	/* Allocate and zero out the renderer */
	renderer = (D3D11Renderer*) SDL_calloc(1, sizeof(D3D11Renderer));

	/* Load the D3DCompiler library */
	renderer->d3dcompiler_dll = SDL_LoadObject(D3DCOMPILER_DLL);
	if (renderer->d3dcompiler_dll == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find " D3DCOMPILER_DLL);
		return NULL;
	}

	/* Load the D3DCompile function pointer */
	renderer->D3DCompileFunc = (PFN_D3DCOMPILE) SDL_LoadFunction(
		renderer->d3dcompiler_dll,
		D3DCOMPILE_FUNC
	);
	if (renderer->D3DCompileFunc == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " D3DCOMPILE_FUNC);
		return NULL;
	}

	/* Load the DXGI library */
	renderer->dxgi_dll = SDL_LoadObject(DXGI_DLL);
	if (renderer->dxgi_dll == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find " DXGI_DLL);
		return NULL;
	}

	/* Load the CreateDXGIFactory1 function */
	CreateDXGIFactoryFunc = (PFN_CREATE_DXGI_FACTORY1) SDL_LoadFunction(
		renderer->dxgi_dll,
		CREATE_DXGI_FACTORY1_FUNC
	);
	if (CreateDXGIFactoryFunc == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " CREATE_DXGI_FACTORY1_FUNC);
		return NULL;
	}

	/* Create the DXGI factory */
	res = CreateDXGIFactoryFunc(
		&D3D_IID_IDXGIFactory1,
		(void**) &renderer->factory
	);
	ERROR_CHECK_RETURN("Could not create DXGIFactory", NULL);

	/* Check for flip-model discard support (supported on Windows 10+) */
	res = IDXGIFactory1_QueryInterface(
		renderer->factory,
		&D3D_IID_IDXGIFactory4,
		(void**) &factory4
	);
	if (SUCCEEDED(res))
	{
		renderer->supportsFlipDiscard = 1;
		IDXGIFactory4_Release(factory4);
	}

	/* Check for explicit tearing support */
	res = IDXGIFactory1_QueryInterface(
		renderer->factory,
		&D3D_IID_IDXGIFactory5,
		(void**) &factory5
	);
	if (SUCCEEDED(res))
	{
		res = IDXGIFactory5_CheckFeatureSupport(
			factory5,
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&renderer->supportsTearing,
			sizeof(renderer->supportsTearing)
		);
		if (FAILED(res))
		{
			renderer->supportsTearing = FALSE;
		}
		IDXGIFactory5_Release(factory5);
	}

	/* Select the appropriate device for rendering */
	res = IDXGIAdapter1_QueryInterface(
		renderer->factory,
		&D3D_IID_IDXGIFactory6,
		(void**) &factory6
	);
	if (SUCCEEDED(res))
	{
		IDXGIFactory6_EnumAdapterByGpuPreference(
			factory6,
			0,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			&D3D_IID_IDXGIAdapter1,
			(void**) &renderer->adapter
		);
		IDXGIFactory6_Release(factory6);
	}
	else
	{
		IDXGIFactory1_EnumAdapters1(
			renderer->factory,
			0,
			&renderer->adapter
		);
	}

	/* Get information about the selected adapter. Used for logging info. */
	IDXGIAdapter1_GetDesc1(renderer->adapter, &adapterDesc);

	/* Initialize the DXGI debug layer, if applicable */
	if (debugMode)
	{
		D3D11_INTERNAL_TryInitializeDXGIDebug(renderer);
	}

	/* Load the D3D library */
	renderer->d3d11_dll = SDL_LoadObject(D3D11_DLL);
	if (renderer->d3d11_dll == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not find " D3D11_DLL);
		return NULL;
	}

	/* Load the CreateDevice function */
	D3D11CreateDeviceFunc = (PFN_D3D11_CREATE_DEVICE) SDL_LoadFunction(
		renderer->d3d11_dll,
		D3D11_CREATE_DEVICE_FUNC
	);
	if (D3D11CreateDeviceFunc == NULL)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Could not load function: " D3D11_CREATE_DEVICE_FUNC);
		return NULL;
	}

	/* Set up device flags */
	flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
	if (debugMode)
	{
		flags |= D3D11_CREATE_DEVICE_DEBUG;
	}

	/* Create the device */
	ID3D11Device *d3d11Device;
tryCreateDevice:
	res = D3D11CreateDeviceFunc(
		(IDXGIAdapter*) renderer->adapter,
		D3D_DRIVER_TYPE_UNKNOWN, /* Must be UNKNOWN if adapter is non-null according to spec */
		NULL,
		flags,
		levels,
		SDL_arraysize(levels),
		D3D11_SDK_VERSION,
		&d3d11Device,
		NULL,
		&renderer->immediateContext
	);
	if (FAILED(res) && debugMode)
	{
		/* If device creation failed, and we're in debug mode, remove the debug flag and try again. */
		SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Creating device in debug mode failed with error %08lX. Trying non-debug.", res);
		flags &= ~D3D11_CREATE_DEVICE_DEBUG;
		debugMode = 0;
		goto tryCreateDevice;
	}

	ERROR_CHECK_RETURN("Could not create D3D11 device", NULL);

	/* The actual device we want is the ID3D11Device1 interface... */
	res = ID3D11Device_QueryInterface(
		d3d11Device,
		&D3D_IID_ID3D11Device1,
		(void**) &renderer->device
	);
	ERROR_CHECK_RETURN("Could not get ID3D11Device1 interface", NULL);

	/* Release the old device interface, we don't need it anymore */
	ID3D11Device_Release(d3d11Device);

	/* Set up the info queue */
	if (renderer->dxgiInfoQueue)
	{
		DXGI_INFO_QUEUE_MESSAGE_SEVERITY sevList[] =
		{
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
			&filter
		);
	}

	/* Print driver info */
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "SDL GPU Driver: D3D11");
	SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "D3D11 Adapter: %S", adapterDesc.Description);

	/* Create mutexes */
	renderer->contextLock = SDL_CreateMutex();
	renderer->acquireCommandBufferLock = SDL_CreateMutex();
	renderer->uniformBufferLock = SDL_CreateMutex();
	renderer->fenceLock = SDL_CreateMutex();
	renderer->windowLock = SDL_CreateMutex();

	/* Initialize miscellaneous renderer members */
	renderer->debugMode = (flags & D3D11_CREATE_DEVICE_DEBUG);

	/* Create command buffer pool */
	D3D11_INTERNAL_AllocateCommandBuffers(renderer, 2);

	/* Create uniform buffer pool */
	renderer->availableUniformBufferCapacity = 16;
	renderer->availableUniformBuffers = SDL_malloc(
		sizeof(D3D11UniformBuffer*) * renderer->availableUniformBufferCapacity
	);

	/* Create fence pool */
	renderer->availableFenceCapacity = 2;
	renderer->availableFences = SDL_malloc(
		sizeof(D3D11Fence*) * renderer->availableFenceCapacity
	);

	/* Create deferred transfer array */
	renderer->transferBufferContainersToDestroyCapacity = 2;
	renderer->transferBufferContainersToDestroyCount = 0;
	renderer->transferBufferContainersToDestroy = SDL_malloc(
		renderer->transferBufferContainersToDestroyCapacity * sizeof(D3D11TransferBufferContainer*)
	);

	/* Create claimed window list */
	renderer->claimedWindowCapacity = 1;
	renderer->claimedWindows = SDL_malloc(
		sizeof(D3D11WindowData*) * renderer->claimedWindowCapacity
	);

	/* Create the SDL_Gpu Device */
	result = (SDL_GpuDevice*) SDL_malloc(sizeof(SDL_GpuDevice));
	ASSIGN_DRIVER(D3D11)
	result->driverData = (SDL_GpuRenderer*) renderer;

	return result;
}

SDL_GpuDriver D3D11Driver = {
	"D3D11",
	D3D11_PrepareDriver,
	D3D11_CreateDevice
};

#endif /*SDL_GPU_D3D11*/
