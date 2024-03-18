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
#include "../SDL_gpu_driver.h"

#if SDL_GPU_D3D11

#define CINTERFACE
#define COBJMACROS
#include <d3dcompiler.h>

/* __stdcall declaration, largely taken from vkd3d_windows.h */
#ifdef _WIN32
#define D3DCOMPILER_API __stdcall
#else
# ifdef __stdcall
#  undef __stdcall
# endif
# ifdef __x86_64__
#  define __stdcall __attribute__((ms_abi))
# else
#  if (__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)) || defined(__APPLE__)
#   define __stdcall __attribute__((__stdcall__)) __attribute__((__force_align_arg_pointer__))
#  else
#   define __stdcall __attribute__((__stdcall__))
#  endif
# endif
# define D3DCOMPILER_API __stdcall
#endif

/* vkd3d uses stdcall for its ID3D10Blob implementation */
#ifndef _WIN32
typedef struct VKD3DBlob VKD3DBlob;
typedef struct VKD3DBlobVtbl
{
	HRESULT(__stdcall* QueryInterface)(
		VKD3DBlob* This,
		REFIID riid,
		void** ppvObject);
	ULONG(__stdcall* AddRef)(VKD3DBlob* This);
	ULONG(__stdcall* Release)(VKD3DBlob* This);
	LPVOID(__stdcall* GetBufferPointer)(VKD3DBlob* This);
	SIZE_T(__stdcall* GetBufferSize)(VKD3DBlob* This);
} VKD3DBlobVtbl;
struct VKD3DBlob
{
	const VKD3DBlobVtbl* lpVtbl;
};
#define ID3D10Blob VKD3DBlob
#define ID3DBlob VKD3DBlob
#endif

/* rename the DLL for different platforms */
#if defined(WIN32)
#undef D3DCOMPILER_DLL
#define D3DCOMPILER_DLL D3DCOMPILER_DLL_A
#elif defined(__APPLE__)
#undef D3DCOMPILER_DLL
#define D3DCOMPILER_DLL "libvkd3d-utils.1.dylib"
#else
#undef D3DCOMPILER_DLL
#define D3DCOMPILER_DLL "libvkd3d-utils.so.1"
#endif

/* D3DCompile signature */
typedef HRESULT(D3DCOMPILER_API* PFN_D3DCOMPILE)(
	LPCVOID pSrcData,
	SIZE_T SrcDataSize,
	LPCSTR pSourceName,
	const D3D_SHADER_MACRO* pDefines,
	ID3DInclude* pInclude,
	LPCSTR pEntrypoint,
	LPCSTR pTarget,
	UINT Flags1,
	UINT Flags2,
	ID3DBlob** ppCode,
	ID3DBlob** ppErrorMsgs
);

static void* d3dcompiler_dll;
static PFN_D3DCOMPILE D3DCompile_func;

#if 0 /* TODO */
static void D3D11_Quit(void)
{
	if (d3dcompiler_dll) {
		SDL_UnloadObject(d3dcompiler_dll);
	}
	d3dcompiler_dll = NULL;
	D3DCompile_func = NULL;
}
#endif

extern SDL_GpuShader* D3D11_CreateShader(
	SDL_GpuRenderer *driverData,
	SDL_GpuShaderCreateInfo *shaderCreateInfo
);

SDL_GpuShader* D3D11_CompileFromSPIRVCross(
	SDL_GpuRenderer *driverData,
	SDL_GpuShaderStageFlagBits shader_stage,
	const char *entryPointName,
	const char *source
) {
	HRESULT result;
	ID3DBlob *blob;
	ID3DBlob *error_blob;
	SDL_GpuShaderCreateInfo createInfo;
	SDL_GpuShader *shader;
	const char *profile;

	/* FIXME: d3dcompiler could probably be loaded in a better spot */

	/* Load the DLL if we haven't already */
	if (!d3dcompiler_dll) {
		d3dcompiler_dll = SDL_LoadObject(D3DCOMPILER_DLL);
		if (!d3dcompiler_dll) {
			SDL_SetError("Failed to load " D3DCOMPILER_DLL);
			return NULL;
		}
	}

	/* Load the D3DCompile function if we haven't already */
	if (!D3DCompile_func) {
		D3DCompile_func = (PFN_D3DCOMPILE) SDL_LoadFunction(d3dcompiler_dll, "D3DCompile");
		if (!D3DCompile_func) {
			SDL_SetError("Failed to load D3DCompile function");
			return NULL;
		}
	}

	if (shader_stage == SDL_GPU_SHADERSTAGE_VERTEX)
	{
		profile = "vs_5_0";
	}
	else if (shader_stage == SDL_GPU_SHADERSTAGE_FRAGMENT)
	{
		profile = "ps_5_0";
	}
	else if (shader_stage == SDL_GPU_SHADERSTAGE_COMPUTE)
	{
		profile = "cs_5_0";
	}
	else
	{
		SDL_SetError("%s", "Unrecognized shader stage!");
		return NULL;
	}

	/* Compile! */
	result = D3DCompile_func(
		source,
		SDL_strlen(source),
		NULL,
		NULL,
		NULL,
		"main", /* entry point name ignored */
		profile,
		0,
		0,
		&blob,
		&error_blob
	);
	if (result < 0) {
		SDL_SetError("%s", (const char*) ID3D10Blob_GetBufferPointer(error_blob));
		ID3D10Blob_Release(error_blob);
		return NULL;
	}

	/* Create the shader */
	createInfo.code = ID3D10Blob_GetBufferPointer(blob);
	createInfo.codeSize = ID3D10Blob_GetBufferSize(blob);
	createInfo.format = SDL_GPU_SHADERFORMAT_DXBC;
	createInfo.stage = shader_stage;
	createInfo.entryPointName = entryPointName;
	shader = D3D11_CreateShader(driverData, &createInfo);

	/* Clean up */
	ID3D10Blob_Release(blob);

	return shader;
}

#endif /* SDL_GPU_D3D11 */
