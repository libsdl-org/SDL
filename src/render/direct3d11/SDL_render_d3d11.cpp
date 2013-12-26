/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2012 Sam Lantinga <slouken@libsdl.org>

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

#include "SDL_config.h"

#if SDL_VIDEO_RENDER_D3D11 && !SDL_RENDER_DISABLED

#ifdef __WINRT__
#include <windows.ui.core.h>
#include <windows.foundation.h>

#if WINAPI_FAMILY == WINAPI_FAMILY_APP
#include <windows.ui.xaml.media.dxinterop.h>
#endif

#endif

extern "C" {
#include "../../core/windows/SDL_windows.h"
#include "SDL_hints.h"
//#include "SDL_loadso.h"
#include "SDL_system.h"
#include "SDL_syswm.h"
#include "../SDL_sysrender.h"
#include "SDL_log.h"
#include "../../video/SDL_sysvideo.h"
//#include "stdio.h"
}

#include <fstream>
#include <string>
#include <vector>

#include <D3D11_1.h>
#include <DirectXMath.h>
#include <wrl/client.h>


using namespace DirectX;
using namespace Microsoft::WRL;
using namespace std;

#ifdef __WINRT__
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
#endif

/* Texture sampling types */
static const D3D11_FILTER SDL_D3D11_NEAREST_PIXEL_FILTER = D3D11_FILTER_MIN_MAG_MIP_POINT;
static const D3D11_FILTER SDL_D3D11_LINEAR_FILTER = D3D11_FILTER_MIN_MAG_MIP_LINEAR;

/* Vertex shader, common values */
struct VertexShaderConstants
{
    DirectX::XMFLOAT4X4 model;
    DirectX::XMFLOAT4X4 view;
    DirectX::XMFLOAT4X4 projection;
};

/* Per-vertex data */
struct VertexPositionColor
{
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT2 tex;
    DirectX::XMFLOAT4 color;
};

/* Per-texture data */
typedef struct
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> mainTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> mainTextureResourceView;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mainTextureRenderTargetView;
    SDL_PixelFormat * pixelFormat;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> stagingTexture;
    DirectX::XMINT2 lockedTexturePosition;
    D3D11_FILTER scaleMode;
} D3D11_TextureData;

/* Private renderer data */
typedef struct
{
    Microsoft::WRL::ComPtr<ID3D11Device1> d3dDevice;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> d3dContext;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> mainRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> currentOffscreenRenderTargetView;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> texturePixelShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader> colorPixelShader;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendModeBlend;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendModeAdd;
    Microsoft::WRL::ComPtr<ID3D11BlendState> blendModeMod;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> nearestPixelSampler;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> linearSampler;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> mainRasterizer;
    D3D_FEATURE_LEVEL featureLevel;

    // Vertex buffer constants:
    VertexShaderConstants vertexShaderConstantsData;
    Microsoft::WRL::ComPtr<ID3D11Buffer> vertexShaderConstants;

    // Cached renderer properties.
    DirectX::XMFLOAT2 windowSizeInDIPs;
    DirectX::XMFLOAT2 renderTargetSize;
    Windows::Graphics::Display::DisplayOrientations orientation;

    // Transform used for display orientation.
    DirectX::XMFLOAT4X4 orientationTransform3D;
} D3D11_RenderData;


/* Direct3D 11.x shaders

   SDL's shaders are compiled into SDL itself, to simplify distribution.

   All Direct3D 11.x shaders were compiled with the following:

   fxc /E"main" /T "<TYPE>" /Fo"<OUTPUT FILE>" "<INPUT FILE>"

     Variables:
     - <TYPE>: the type of shader.  A table of utilized shader types is
       listed below.
     - <OUTPUT FILE>: where to store compiled output
     - <INPUT FILE>: where to read shader source code from

     Shader types:
     - ps_4_0_level_9_1: Pixel shader for Windows 8+, including Windows RT
     - vs_4_0_level_9_1: Vertex shader for Windows 8+, including Windows RT
     - ps_4_0_level_9_3: Pixel shader for Windows Phone 8
     - vs_4_0_level_9_3: Vertex shader for Windows Phone 8
  */
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
#define D3D11_USE_SHADER_MODEL_4_0_level_9_3
#else
#define D3D11_USE_SHADER_MODEL_4_0_level_9_1
#endif

/* The texture-rendering pixel shader:

    --- D3D11_PixelShader_Textures.hlsl ---
    Texture2D theTexture : register(t0);
    SamplerState theSampler : register(s0);

    struct PixelShaderInput
    {
        float4 pos : SV_POSITION;
        float2 tex : TEXCOORD0;
        float4 color : COLOR0;
    };

    float4 main(PixelShaderInput input) : SV_TARGET
    {
        return theTexture.Sample(theSampler, input.tex) * input.color;
    }
*/
#if defined(D3D11_USE_SHADER_MODEL_4_0_level_9_1)
static const DWORD D3D11_PixelShader_Textures[] = {
    0x43425844, 0x6299b59f, 0x155258f2, 0x873ab86a, 0xfcbb6dcd, 0x00000001,
    0x00000330, 0x00000006, 0x00000038, 0x000000c0, 0x0000015c, 0x000001d8,
    0x00000288, 0x000002fc, 0x396e6f41, 0x00000080, 0x00000080, 0xffff0200,
    0x00000058, 0x00000028, 0x00280000, 0x00280000, 0x00280000, 0x00240001,
    0x00280000, 0x00000000, 0xffff0200, 0x0200001f, 0x80000000, 0xb0030000,
    0x0200001f, 0x80000000, 0xb00f0001, 0x0200001f, 0x90000000, 0xa00f0800,
    0x03000042, 0x800f0000, 0xb0e40000, 0xa0e40800, 0x03000005, 0x800f0000,
    0x80e40000, 0xb0e40001, 0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff,
    0x52444853, 0x00000094, 0x00000040, 0x00000025, 0x0300005a, 0x00106000,
    0x00000000, 0x04001858, 0x00107000, 0x00000000, 0x00005555, 0x03001062,
    0x00101032, 0x00000001, 0x03001062, 0x001010f2, 0x00000002, 0x03000065,
    0x001020f2, 0x00000000, 0x02000068, 0x00000001, 0x09000045, 0x001000f2,
    0x00000000, 0x00101046, 0x00000001, 0x00107e46, 0x00000000, 0x00106000,
    0x00000000, 0x07000038, 0x001020f2, 0x00000000, 0x00100e46, 0x00000000,
    0x00101e46, 0x00000002, 0x0100003e, 0x54415453, 0x00000074, 0x00000003,
    0x00000001, 0x00000000, 0x00000003, 0x00000001, 0x00000000, 0x00000000,
    0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x46454452, 0x000000a8,
    0x00000000, 0x00000000, 0x00000002, 0x0000001c, 0xffff0400, 0x00000100,
    0x00000072, 0x0000005c, 0x00000003, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000001, 0x00000001, 0x00000067, 0x00000002, 0x00000005,
    0x00000004, 0xffffffff, 0x00000000, 0x00000001, 0x0000000d, 0x53656874,
    0x6c706d61, 0x74007265, 0x65546568, 0x72757478, 0x694d0065, 0x736f7263,
    0x2074666f, 0x20295228, 0x4c534c48, 0x61685320, 0x20726564, 0x706d6f43,
    0x72656c69, 0x332e3920, 0x32392e30, 0x312e3030, 0x34383336, 0xababab00,
    0x4e475349, 0x0000006c, 0x00000003, 0x00000008, 0x00000050, 0x00000000,
    0x00000001, 0x00000003, 0x00000000, 0x0000000f, 0x0000005c, 0x00000000,
    0x00000000, 0x00000003, 0x00000001, 0x00000303, 0x00000065, 0x00000000,
    0x00000000, 0x00000003, 0x00000002, 0x00000f0f, 0x505f5653, 0x5449534f,
    0x004e4f49, 0x43584554, 0x44524f4f, 0x4c4f4300, 0xab00524f, 0x4e47534f,
    0x0000002c, 0x00000001, 0x00000008, 0x00000020, 0x00000000, 0x00000000,
    0x00000003, 0x00000000, 0x0000000f, 0x545f5653, 0x45475241, 0xabab0054
};
#elif defined(D3D11_USE_SHADER_MODEL_4_0_level_9_3)
static const DWORD D3D11_PixelShader_Textures[] = {
    0x43425844, 0x5876569a, 0x01b6c87e, 0x8447454f, 0xc7f3ef10, 0x00000001,
    0x00000330, 0x00000006, 0x00000038, 0x000000c0, 0x0000015c, 0x000001d8,
    0x00000288, 0x000002fc, 0x396e6f41, 0x00000080, 0x00000080, 0xffff0200,
    0x00000058, 0x00000028, 0x00280000, 0x00280000, 0x00280000, 0x00240001,
    0x00280000, 0x00000000, 0xffff0201, 0x0200001f, 0x80000000, 0xb0030000,
    0x0200001f, 0x80000000, 0xb00f0001, 0x0200001f, 0x90000000, 0xa00f0800,
    0x03000042, 0x800f0000, 0xb0e40000, 0xa0e40800, 0x03000005, 0x800f0000,
    0x80e40000, 0xb0e40001, 0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff,
    0x52444853, 0x00000094, 0x00000040, 0x00000025, 0x0300005a, 0x00106000,
    0x00000000, 0x04001858, 0x00107000, 0x00000000, 0x00005555, 0x03001062,
    0x00101032, 0x00000001, 0x03001062, 0x001010f2, 0x00000002, 0x03000065,
    0x001020f2, 0x00000000, 0x02000068, 0x00000001, 0x09000045, 0x001000f2,
    0x00000000, 0x00101046, 0x00000001, 0x00107e46, 0x00000000, 0x00106000,
    0x00000000, 0x07000038, 0x001020f2, 0x00000000, 0x00100e46, 0x00000000,
    0x00101e46, 0x00000002, 0x0100003e, 0x54415453, 0x00000074, 0x00000003,
    0x00000001, 0x00000000, 0x00000003, 0x00000001, 0x00000000, 0x00000000,
    0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x46454452, 0x000000a8,
    0x00000000, 0x00000000, 0x00000002, 0x0000001c, 0xffff0400, 0x00000100,
    0x00000072, 0x0000005c, 0x00000003, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000001, 0x00000001, 0x00000067, 0x00000002, 0x00000005,
    0x00000004, 0xffffffff, 0x00000000, 0x00000001, 0x0000000d, 0x53656874,
    0x6c706d61, 0x74007265, 0x65546568, 0x72757478, 0x694d0065, 0x736f7263,
    0x2074666f, 0x20295228, 0x4c534c48, 0x61685320, 0x20726564, 0x706d6f43,
    0x72656c69, 0x332e3920, 0x32392e30, 0x312e3030, 0x34383336, 0xababab00,
    0x4e475349, 0x0000006c, 0x00000003, 0x00000008, 0x00000050, 0x00000000,
    0x00000001, 0x00000003, 0x00000000, 0x0000000f, 0x0000005c, 0x00000000,
    0x00000000, 0x00000003, 0x00000001, 0x00000303, 0x00000065, 0x00000000,
    0x00000000, 0x00000003, 0x00000002, 0x00000f0f, 0x505f5653, 0x5449534f,
    0x004e4f49, 0x43584554, 0x44524f4f, 0x4c4f4300, 0xab00524f, 0x4e47534f,
    0x0000002c, 0x00000001, 0x00000008, 0x00000020, 0x00000000, 0x00000000,
    0x00000003, 0x00000000, 0x0000000f, 0x545f5653, 0x45475241, 0xabab0054
};
#else
#error "An appropriate 'textures' pixel shader is not defined"
#endif

/* The color-only-rendering pixel shader:

   --- D3D11_PixelShader_Colors.hlsl ---
   struct PixelShaderInput
   {
       float4 pos : SV_POSITION;
       float2 tex : TEXCOORD0;
       float4 color : COLOR0;
   };

   float4 main(PixelShaderInput input) : SV_TARGET
   {
       return input.color;
   }
*/
#if defined(D3D11_USE_SHADER_MODEL_4_0_level_9_1)
static const DWORD D3D11_PixelShader_Colors[] = {
    0x43425844, 0xd74c28fe, 0xa1eb8804, 0x269d512a, 0x7699723d, 0x00000001,
    0x00000240, 0x00000006, 0x00000038, 0x00000084, 0x000000c4, 0x00000140,
    0x00000198, 0x0000020c, 0x396e6f41, 0x00000044, 0x00000044, 0xffff0200,
    0x00000020, 0x00000024, 0x00240000, 0x00240000, 0x00240000, 0x00240000,
    0x00240000, 0xffff0200, 0x0200001f, 0x80000000, 0xb00f0001, 0x02000001,
    0x800f0800, 0xb0e40001, 0x0000ffff, 0x52444853, 0x00000038, 0x00000040,
    0x0000000e, 0x03001062, 0x001010f2, 0x00000002, 0x03000065, 0x001020f2,
    0x00000000, 0x05000036, 0x001020f2, 0x00000000, 0x00101e46, 0x00000002,
    0x0100003e, 0x54415453, 0x00000074, 0x00000002, 0x00000000, 0x00000000,
    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x46454452, 0x00000050, 0x00000000, 0x00000000,
    0x00000000, 0x0000001c, 0xffff0400, 0x00000100, 0x0000001c, 0x7263694d,
    0x666f736f, 0x52282074, 0x4c482029, 0x53204c53, 0x65646168, 0x6f432072,
    0x6c69706d, 0x39207265, 0x2e30332e, 0x30303239, 0x3336312e, 0xab003438,
    0x4e475349, 0x0000006c, 0x00000003, 0x00000008, 0x00000050, 0x00000000,
    0x00000001, 0x00000003, 0x00000000, 0x0000000f, 0x0000005c, 0x00000000,
    0x00000000, 0x00000003, 0x00000001, 0x00000003, 0x00000065, 0x00000000,
    0x00000000, 0x00000003, 0x00000002, 0x00000f0f, 0x505f5653, 0x5449534f,
    0x004e4f49, 0x43584554, 0x44524f4f, 0x4c4f4300, 0xab00524f, 0x4e47534f,
    0x0000002c, 0x00000001, 0x00000008, 0x00000020, 0x00000000, 0x00000000,
    0x00000003, 0x00000000, 0x0000000f, 0x545f5653, 0x45475241, 0xabab0054
};
#elif defined(D3D11_USE_SHADER_MODEL_4_0_level_9_3)
static const DWORD D3D11_PixelShader_Colors[] = {
    0x43425844, 0x93f6ccfc, 0x5f919270, 0x7a11aa4f, 0x9148e931, 0x00000001,
    0x00000240, 0x00000006, 0x00000038, 0x00000084, 0x000000c4, 0x00000140,
    0x00000198, 0x0000020c, 0x396e6f41, 0x00000044, 0x00000044, 0xffff0200,
    0x00000020, 0x00000024, 0x00240000, 0x00240000, 0x00240000, 0x00240000,
    0x00240000, 0xffff0201, 0x0200001f, 0x80000000, 0xb00f0001, 0x02000001,
    0x800f0800, 0xb0e40001, 0x0000ffff, 0x52444853, 0x00000038, 0x00000040,
    0x0000000e, 0x03001062, 0x001010f2, 0x00000002, 0x03000065, 0x001020f2,
    0x00000000, 0x05000036, 0x001020f2, 0x00000000, 0x00101e46, 0x00000002,
    0x0100003e, 0x54415453, 0x00000074, 0x00000002, 0x00000000, 0x00000000,
    0x00000002, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000002, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x46454452, 0x00000050, 0x00000000, 0x00000000,
    0x00000000, 0x0000001c, 0xffff0400, 0x00000100, 0x0000001c, 0x7263694d,
    0x666f736f, 0x52282074, 0x4c482029, 0x53204c53, 0x65646168, 0x6f432072,
    0x6c69706d, 0x39207265, 0x2e30332e, 0x30303239, 0x3336312e, 0xab003438,
    0x4e475349, 0x0000006c, 0x00000003, 0x00000008, 0x00000050, 0x00000000,
    0x00000001, 0x00000003, 0x00000000, 0x0000000f, 0x0000005c, 0x00000000,
    0x00000000, 0x00000003, 0x00000001, 0x00000003, 0x00000065, 0x00000000,
    0x00000000, 0x00000003, 0x00000002, 0x00000f0f, 0x505f5653, 0x5449534f,
    0x004e4f49, 0x43584554, 0x44524f4f, 0x4c4f4300, 0xab00524f, 0x4e47534f,
    0x0000002c, 0x00000001, 0x00000008, 0x00000020, 0x00000000, 0x00000000,
    0x00000003, 0x00000000, 0x0000000f, 0x545f5653, 0x45475241, 0xabab0054,
};
#else
#error "An appropriate 'colors' pixel shader is not defined."
#endif

/* The sole vertex shader:

   --- D3D11_VertexShader.hlsl ---
   #pragma pack_matrix( row_major )

   cbuffer VertexShaderConstants : register(b0)
   {
       matrix model;
       matrix view;
       matrix projection;
   };

   struct VertexShaderInput
   {
       float3 pos : POSITION;
       float2 tex : TEXCOORD0;
       float4 color : COLOR0;
   };

   struct VertexShaderOutput
   {
       float4 pos : SV_POSITION;
       float2 tex : TEXCOORD0;
       float4 color : COLOR0;
   };

   VertexShaderOutput main(VertexShaderInput input)
   {
       VertexShaderOutput output;
       float4 pos = float4(input.pos, 1.0f);

       // Transform the vertex position into projected space.
       pos = mul(pos, model);
       pos = mul(pos, view);
       pos = mul(pos, projection);
       output.pos = pos;

       // Pass through texture coordinates and color values without transformation
       output.tex = input.tex;
       output.color = input.color;

       return output;
   }
*/
#if defined(D3D11_USE_SHADER_MODEL_4_0_level_9_1)
static const DWORD D3D11_VertexShader[] = {
    0x43425844, 0x3f31b022, 0x2ffad8b8, 0xd6c45cbd, 0xa7894c28, 0x00000001,
    0x00000690, 0x00000006, 0x00000038, 0x000001b8, 0x00000418, 0x00000494,
    0x000005ac, 0x0000061c, 0x396e6f41, 0x00000178, 0x00000178, 0xfffe0200,
    0x00000144, 0x00000034, 0x00240001, 0x00300000, 0x00300000, 0x00240000,
    0x00300001, 0x00000000, 0x0001000c, 0x00000000, 0x00000000, 0xfffe0200,
    0x0200001f, 0x80000005, 0x900f0000, 0x0200001f, 0x80010005, 0x900f0001,
    0x0200001f, 0x80020005, 0x900f0002, 0x03000005, 0x800f0000, 0x90550000,
    0xa0e40002, 0x04000004, 0x800f0000, 0x90000000, 0xa0e40001, 0x80e40000,
    0x04000004, 0x800f0000, 0x90aa0000, 0xa0e40003, 0x80e40000, 0x03000002,
    0x800f0000, 0x80e40000, 0xa0e40004, 0x03000005, 0x800f0001, 0x80550000,
    0xa0e40006, 0x04000004, 0x800f0001, 0x80000000, 0xa0e40005, 0x80e40001,
    0x04000004, 0x800f0001, 0x80aa0000, 0xa0e40007, 0x80e40001, 0x04000004,
    0x800f0000, 0x80ff0000, 0xa0e40008, 0x80e40001, 0x03000005, 0x800f0001,
    0x80550000, 0xa0e4000a, 0x04000004, 0x800f0001, 0x80000000, 0xa0e40009,
    0x80e40001, 0x04000004, 0x800f0001, 0x80aa0000, 0xa0e4000b, 0x80e40001,
    0x04000004, 0x800f0000, 0x80ff0000, 0xa0e4000c, 0x80e40001, 0x04000004,
    0xc0030000, 0x80ff0000, 0xa0e40000, 0x80e40000, 0x02000001, 0xc00c0000,
    0x80e40000, 0x02000001, 0xe0030000, 0x90e40001, 0x02000001, 0xe00f0001,
    0x90e40002, 0x0000ffff, 0x52444853, 0x00000258, 0x00010040, 0x00000096,
    0x04000059, 0x00208e46, 0x00000000, 0x0000000c, 0x0300005f, 0x00101072,
    0x00000000, 0x0300005f, 0x00101032, 0x00000001, 0x0300005f, 0x001010f2,
    0x00000002, 0x04000067, 0x001020f2, 0x00000000, 0x00000001, 0x03000065,
    0x00102032, 0x00000001, 0x03000065, 0x001020f2, 0x00000002, 0x02000068,
    0x00000002, 0x08000038, 0x001000f2, 0x00000000, 0x00101556, 0x00000000,
    0x00208e46, 0x00000000, 0x00000001, 0x0a000032, 0x001000f2, 0x00000000,
    0x00101006, 0x00000000, 0x00208e46, 0x00000000, 0x00000000, 0x00100e46,
    0x00000000, 0x0a000032, 0x001000f2, 0x00000000, 0x00101aa6, 0x00000000,
    0x00208e46, 0x00000000, 0x00000002, 0x00100e46, 0x00000000, 0x08000000,
    0x001000f2, 0x00000000, 0x00100e46, 0x00000000, 0x00208e46, 0x00000000,
    0x00000003, 0x08000038, 0x001000f2, 0x00000001, 0x00100556, 0x00000000,
    0x00208e46, 0x00000000, 0x00000005, 0x0a000032, 0x001000f2, 0x00000001,
    0x00100006, 0x00000000, 0x00208e46, 0x00000000, 0x00000004, 0x00100e46,
    0x00000001, 0x0a000032, 0x001000f2, 0x00000001, 0x00100aa6, 0x00000000,
    0x00208e46, 0x00000000, 0x00000006, 0x00100e46, 0x00000001, 0x0a000032,
    0x001000f2, 0x00000000, 0x00100ff6, 0x00000000, 0x00208e46, 0x00000000,
    0x00000007, 0x00100e46, 0x00000001, 0x08000038, 0x001000f2, 0x00000001,
    0x00100556, 0x00000000, 0x00208e46, 0x00000000, 0x00000009, 0x0a000032,
    0x001000f2, 0x00000001, 0x00100006, 0x00000000, 0x00208e46, 0x00000000,
    0x00000008, 0x00100e46, 0x00000001, 0x0a000032, 0x001000f2, 0x00000001,
    0x00100aa6, 0x00000000, 0x00208e46, 0x00000000, 0x0000000a, 0x00100e46,
    0x00000001, 0x0a000032, 0x001020f2, 0x00000000, 0x00100ff6, 0x00000000,
    0x00208e46, 0x00000000, 0x0000000b, 0x00100e46, 0x00000001, 0x05000036,
    0x00102032, 0x00000001, 0x00101046, 0x00000001, 0x05000036, 0x001020f2,
    0x00000002, 0x00101e46, 0x00000002, 0x0100003e, 0x54415453, 0x00000074,
    0x0000000f, 0x00000002, 0x00000000, 0x00000006, 0x00000004, 0x00000000,
    0x00000000, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000003, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x46454452,
    0x00000110, 0x00000001, 0x00000054, 0x00000001, 0x0000001c, 0xfffe0400,
    0x00000100, 0x000000dc, 0x0000003c, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x00000001, 0x74726556, 0x68537865,
    0x72656461, 0x736e6f43, 0x746e6174, 0xabab0073, 0x0000003c, 0x00000003,
    0x0000006c, 0x000000c0, 0x00000000, 0x00000000, 0x000000b4, 0x00000000,
    0x00000040, 0x00000002, 0x000000bc, 0x00000000, 0x000000cc, 0x00000040,
    0x00000040, 0x00000002, 0x000000bc, 0x00000000, 0x000000d1, 0x00000080,
    0x00000040, 0x00000002, 0x000000bc, 0x00000000, 0x65646f6d, 0xabab006c,
    0x00030002, 0x00040004, 0x00000000, 0x00000000, 0x77656976, 0x6f727000,
    0x7463656a, 0x006e6f69, 0x7263694d, 0x666f736f, 0x52282074, 0x4c482029,
    0x53204c53, 0x65646168, 0x6f432072, 0x6c69706d, 0x39207265, 0x2e30332e,
    0x30303239, 0x3336312e, 0xab003438, 0x4e475349, 0x00000068, 0x00000003,
    0x00000008, 0x00000050, 0x00000000, 0x00000000, 0x00000003, 0x00000000,
    0x00000707, 0x00000059, 0x00000000, 0x00000000, 0x00000003, 0x00000001,
    0x00000303, 0x00000062, 0x00000000, 0x00000000, 0x00000003, 0x00000002,
    0x00000f0f, 0x49534f50, 0x4e4f4954, 0x58455400, 0x524f4f43, 0x4f430044,
    0x00524f4c, 0x4e47534f, 0x0000006c, 0x00000003, 0x00000008, 0x00000050,
    0x00000000, 0x00000001, 0x00000003, 0x00000000, 0x0000000f, 0x0000005c,
    0x00000000, 0x00000000, 0x00000003, 0x00000001, 0x00000c03, 0x00000065,
    0x00000000, 0x00000000, 0x00000003, 0x00000002, 0x0000000f, 0x505f5653,
    0x5449534f, 0x004e4f49, 0x43584554, 0x44524f4f, 0x4c4f4300, 0xab00524f,
};
#elif defined(D3D11_USE_SHADER_MODEL_4_0_level_9_3)
static const DWORD D3D11_VertexShader[] = {
    0x43425844, 0xacfd840a, 0x6a6ae1e1, 0xc3649c43, 0x8bfc0816, 0x00000001,
    0x00000690, 0x00000006, 0x00000038, 0x000001b8, 0x00000418, 0x00000494,
    0x000005ac, 0x0000061c, 0x396e6f41, 0x00000178, 0x00000178, 0xfffe0200,
    0x00000144, 0x00000034, 0x00240001, 0x00300000, 0x00300000, 0x00240000,
    0x00300001, 0x00000000, 0x0001000c, 0x00000000, 0x00000000, 0xfffe0201,
    0x0200001f, 0x80000005, 0x900f0000, 0x0200001f, 0x80010005, 0x900f0001,
    0x0200001f, 0x80020005, 0x900f0002, 0x03000005, 0x800f0000, 0x90550000,
    0xa0e40002, 0x04000004, 0x800f0000, 0x90000000, 0xa0e40001, 0x80e40000,
    0x04000004, 0x800f0000, 0x90aa0000, 0xa0e40003, 0x80e40000, 0x03000002,
    0x800f0000, 0x80e40000, 0xa0e40004, 0x03000005, 0x800f0001, 0x80550000,
    0xa0e40006, 0x04000004, 0x800f0001, 0x80000000, 0xa0e40005, 0x80e40001,
    0x04000004, 0x800f0001, 0x80aa0000, 0xa0e40007, 0x80e40001, 0x04000004,
    0x800f0000, 0x80ff0000, 0xa0e40008, 0x80e40001, 0x03000005, 0x800f0001,
    0x80550000, 0xa0e4000a, 0x04000004, 0x800f0001, 0x80000000, 0xa0e40009,
    0x80e40001, 0x04000004, 0x800f0001, 0x80aa0000, 0xa0e4000b, 0x80e40001,
    0x04000004, 0x800f0000, 0x80ff0000, 0xa0e4000c, 0x80e40001, 0x04000004,
    0xc0030000, 0x80ff0000, 0xa0e40000, 0x80e40000, 0x02000001, 0xc00c0000,
    0x80e40000, 0x02000001, 0xe0030000, 0x90e40001, 0x02000001, 0xe00f0001,
    0x90e40002, 0x0000ffff, 0x52444853, 0x00000258, 0x00010040, 0x00000096,
    0x04000059, 0x00208e46, 0x00000000, 0x0000000c, 0x0300005f, 0x00101072,
    0x00000000, 0x0300005f, 0x00101032, 0x00000001, 0x0300005f, 0x001010f2,
    0x00000002, 0x04000067, 0x001020f2, 0x00000000, 0x00000001, 0x03000065,
    0x00102032, 0x00000001, 0x03000065, 0x001020f2, 0x00000002, 0x02000068,
    0x00000002, 0x08000038, 0x001000f2, 0x00000000, 0x00101556, 0x00000000,
    0x00208e46, 0x00000000, 0x00000001, 0x0a000032, 0x001000f2, 0x00000000,
    0x00101006, 0x00000000, 0x00208e46, 0x00000000, 0x00000000, 0x00100e46,
    0x00000000, 0x0a000032, 0x001000f2, 0x00000000, 0x00101aa6, 0x00000000,
    0x00208e46, 0x00000000, 0x00000002, 0x00100e46, 0x00000000, 0x08000000,
    0x001000f2, 0x00000000, 0x00100e46, 0x00000000, 0x00208e46, 0x00000000,
    0x00000003, 0x08000038, 0x001000f2, 0x00000001, 0x00100556, 0x00000000,
    0x00208e46, 0x00000000, 0x00000005, 0x0a000032, 0x001000f2, 0x00000001,
    0x00100006, 0x00000000, 0x00208e46, 0x00000000, 0x00000004, 0x00100e46,
    0x00000001, 0x0a000032, 0x001000f2, 0x00000001, 0x00100aa6, 0x00000000,
    0x00208e46, 0x00000000, 0x00000006, 0x00100e46, 0x00000001, 0x0a000032,
    0x001000f2, 0x00000000, 0x00100ff6, 0x00000000, 0x00208e46, 0x00000000,
    0x00000007, 0x00100e46, 0x00000001, 0x08000038, 0x001000f2, 0x00000001,
    0x00100556, 0x00000000, 0x00208e46, 0x00000000, 0x00000009, 0x0a000032,
    0x001000f2, 0x00000001, 0x00100006, 0x00000000, 0x00208e46, 0x00000000,
    0x00000008, 0x00100e46, 0x00000001, 0x0a000032, 0x001000f2, 0x00000001,
    0x00100aa6, 0x00000000, 0x00208e46, 0x00000000, 0x0000000a, 0x00100e46,
    0x00000001, 0x0a000032, 0x001020f2, 0x00000000, 0x00100ff6, 0x00000000,
    0x00208e46, 0x00000000, 0x0000000b, 0x00100e46, 0x00000001, 0x05000036,
    0x00102032, 0x00000001, 0x00101046, 0x00000001, 0x05000036, 0x001020f2,
    0x00000002, 0x00101e46, 0x00000002, 0x0100003e, 0x54415453, 0x00000074,
    0x0000000f, 0x00000002, 0x00000000, 0x00000006, 0x00000004, 0x00000000,
    0x00000000, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000003, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x46454452,
    0x00000110, 0x00000001, 0x00000054, 0x00000001, 0x0000001c, 0xfffe0400,
    0x00000100, 0x000000dc, 0x0000003c, 0x00000000, 0x00000000, 0x00000000,
    0x00000000, 0x00000000, 0x00000001, 0x00000001, 0x74726556, 0x68537865,
    0x72656461, 0x736e6f43, 0x746e6174, 0xabab0073, 0x0000003c, 0x00000003,
    0x0000006c, 0x000000c0, 0x00000000, 0x00000000, 0x000000b4, 0x00000000,
    0x00000040, 0x00000002, 0x000000bc, 0x00000000, 0x000000cc, 0x00000040,
    0x00000040, 0x00000002, 0x000000bc, 0x00000000, 0x000000d1, 0x00000080,
    0x00000040, 0x00000002, 0x000000bc, 0x00000000, 0x65646f6d, 0xabab006c,
    0x00030002, 0x00040004, 0x00000000, 0x00000000, 0x77656976, 0x6f727000,
    0x7463656a, 0x006e6f69, 0x7263694d, 0x666f736f, 0x52282074, 0x4c482029,
    0x53204c53, 0x65646168, 0x6f432072, 0x6c69706d, 0x39207265, 0x2e30332e,
    0x30303239, 0x3336312e, 0xab003438, 0x4e475349, 0x00000068, 0x00000003,
    0x00000008, 0x00000050, 0x00000000, 0x00000000, 0x00000003, 0x00000000,
    0x00000707, 0x00000059, 0x00000000, 0x00000000, 0x00000003, 0x00000001,
    0x00000303, 0x00000062, 0x00000000, 0x00000000, 0x00000003, 0x00000002,
    0x00000f0f, 0x49534f50, 0x4e4f4954, 0x58455400, 0x524f4f43, 0x4f430044,
    0x00524f4c, 0x4e47534f, 0x0000006c, 0x00000003, 0x00000008, 0x00000050,
    0x00000000, 0x00000001, 0x00000003, 0x00000000, 0x0000000f, 0x0000005c,
    0x00000000, 0x00000000, 0x00000003, 0x00000001, 0x00000c03, 0x00000065,
    0x00000000, 0x00000000, 0x00000003, 0x00000002, 0x0000000f, 0x505f5653,
    0x5449534f, 0x004e4f49, 0x43584554, 0x44524f4f, 0x4c4f4300, 0xab00524f
};
#else
#error "An appropriate vertex shader is not defined."
#endif

/* Direct3D 11.1 renderer implementation */
static SDL_Renderer *D3D11_CreateRenderer(SDL_Window * window, Uint32 flags);
static void D3D11_WindowEvent(SDL_Renderer * renderer,
                            const SDL_WindowEvent *event);
static int D3D11_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int D3D11_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                             const SDL_Rect * rect, const void *srcPixels,
                             int srcPitch);
static int D3D11_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                             const SDL_Rect * rect, void **pixels, int *pitch);
static void D3D11_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture);
static int D3D11_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture);
static int D3D11_UpdateViewport(SDL_Renderer * renderer);
static int D3D11_UpdateClipRect(SDL_Renderer * renderer);
static int D3D11_RenderClear(SDL_Renderer * renderer);
static int D3D11_RenderDrawPoints(SDL_Renderer * renderer,
                                  const SDL_FPoint * points, int count);
static int D3D11_RenderDrawLines(SDL_Renderer * renderer,
                                 const SDL_FPoint * points, int count);
static int D3D11_RenderFillRects(SDL_Renderer * renderer,
                                 const SDL_FRect * rects, int count);
static int D3D11_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                            const SDL_Rect * srcrect, const SDL_FRect * dstrect);
static int D3D11_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                              const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                              const double angle, const SDL_FPoint * center, const SDL_RendererFlip flip);
static int D3D11_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                                  Uint32 format, void * pixels, int pitch);
static void D3D11_RenderPresent(SDL_Renderer * renderer);
static void D3D11_DestroyTexture(SDL_Renderer * renderer,
                                 SDL_Texture * texture);
static void D3D11_DestroyRenderer(SDL_Renderer * renderer);

/* Direct3D 11.1 Internal Functions */
HRESULT D3D11_CreateDeviceResources(SDL_Renderer * renderer);
HRESULT D3D11_CreateWindowSizeDependentResources(SDL_Renderer * renderer);
HRESULT D3D11_UpdateForWindowSizeChange(SDL_Renderer * renderer);
HRESULT D3D11_HandleDeviceLost(SDL_Renderer * renderer);

extern "C" SDL_RenderDriver D3D11_RenderDriver = {
    D3D11_CreateRenderer,
    {
        "direct3d 11.1",
        (
            SDL_RENDERER_ACCELERATED |
            SDL_RENDERER_PRESENTVSYNC |
            SDL_RENDERER_TARGETTEXTURE
        ),                          // flags.  see SDL_RendererFlags
        2,                          // num_texture_formats
        {                           // texture_formats
            SDL_PIXELFORMAT_RGB888,
            SDL_PIXELFORMAT_ARGB8888
        },
        0,                          // max_texture_width: will be filled in later
        0                           // max_texture_height: will be filled in later
    }
};


static Uint32
DXGIFormatToSDLPixelFormat(DXGI_FORMAT dxgiFormat) {
    switch (dxgiFormat) {
        case DXGI_FORMAT_B8G8R8A8_UNORM:
            return SDL_PIXELFORMAT_ARGB8888;
        case DXGI_FORMAT_B8G8R8X8_UNORM:
            return SDL_PIXELFORMAT_RGB888;
        default:
            return SDL_PIXELFORMAT_UNKNOWN;
    }
}

static DXGI_FORMAT
SDLPixelFormatToDXGIFormat(Uint32 sdlFormat)
{
    switch (sdlFormat) {
        case SDL_PIXELFORMAT_ARGB8888:
            return DXGI_FORMAT_B8G8R8A8_UNORM;
        case SDL_PIXELFORMAT_RGB888:
            return DXGI_FORMAT_B8G8R8X8_UNORM;
        default:
            return DXGI_FORMAT_UNKNOWN;
    }
}


//typedef struct
//{
//    float x, y, z;
//    DWORD color;
//    float u, v;
//} Vertex;

SDL_Renderer *
D3D11_CreateRenderer(SDL_Window * window, Uint32 flags)
{
    SDL_Renderer *renderer;
    D3D11_RenderData *data;

    renderer = (SDL_Renderer *) SDL_calloc(1, sizeof(*renderer));
    if (!renderer) {
        SDL_OutOfMemory();
        return NULL;
    }
    SDL_zerop(renderer);

    data = new D3D11_RenderData;    // Use the C++ 'new' operator to make sure the struct's members initialize using C++ rules
    if (!data) {
        SDL_OutOfMemory();
        return NULL;
    }
    data->featureLevel = (D3D_FEATURE_LEVEL) 0;
    data->windowSizeInDIPs = XMFLOAT2(0, 0);
    data->renderTargetSize = XMFLOAT2(0, 0);

    renderer->WindowEvent = D3D11_WindowEvent;
    renderer->CreateTexture = D3D11_CreateTexture;
    renderer->UpdateTexture = D3D11_UpdateTexture;
    renderer->LockTexture = D3D11_LockTexture;
    renderer->UnlockTexture = D3D11_UnlockTexture;
    renderer->SetRenderTarget = D3D11_SetRenderTarget;
    renderer->UpdateViewport = D3D11_UpdateViewport;
    renderer->UpdateClipRect = D3D11_UpdateClipRect;
    renderer->RenderClear = D3D11_RenderClear;
    renderer->RenderDrawPoints = D3D11_RenderDrawPoints;
    renderer->RenderDrawLines = D3D11_RenderDrawLines;
    renderer->RenderFillRects = D3D11_RenderFillRects;
    renderer->RenderCopy = D3D11_RenderCopy;
    renderer->RenderCopyEx = D3D11_RenderCopyEx;
    renderer->RenderReadPixels = D3D11_RenderReadPixels;
    renderer->RenderPresent = D3D11_RenderPresent;
    renderer->DestroyTexture = D3D11_DestroyTexture;
    renderer->DestroyRenderer = D3D11_DestroyRenderer;
    renderer->info = D3D11_RenderDriver.info;
    renderer->driverdata = data;

    // HACK: make sure the SDL_Renderer references the SDL_Window data now, in
    // order to give init functions access to the underlying window handle:
    renderer->window = window;

    /* Initialize Direct3D resources */
    if (FAILED(D3D11_CreateDeviceResources(renderer))) {
        D3D11_DestroyRenderer(renderer);
        return NULL;
    }
    if (FAILED(D3D11_CreateWindowSizeDependentResources(renderer))) {
        D3D11_DestroyRenderer(renderer);
        return NULL;
    }

    // TODO, WinRT: fill in renderer->info.texture_formats where appropriate

    return renderer;
}

static void
D3D11_DestroyRenderer(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;
    if (data) {
        delete data;
        data = NULL;
    }
}

static HRESULT
D3D11_CreateBlendMode(SDL_Renderer * renderer,
                      BOOL enableBlending,
                      D3D11_BLEND srcBlend,
                      D3D11_BLEND destBlend,
                      ID3D11BlendState ** blendStateOutput)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;
    HRESULT result = S_OK;

    D3D11_BLEND_DESC blendDesc;
    memset(&blendDesc, 0, sizeof(blendDesc));
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = enableBlending;
    blendDesc.RenderTarget[0].SrcBlend = srcBlend;
    blendDesc.RenderTarget[0].DestBlend = destBlend;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    result = data->d3dDevice->CreateBlendState(&blendDesc, blendStateOutput);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateBlendState", result);
        return result;
    }

    return S_OK;
}

// Create resources that depend on the device.
HRESULT
D3D11_CreateDeviceResources(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;

    // This flag adds support for surfaces with a different color channel ordering
    // than the API default. It is required for compatibility with Direct2D.
    UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;

    // Make sure Direct3D's debugging feature gets used, if the app requests it.
    //const char *hint = SDL_GetHint(SDL_HINT_RENDER_DIRECT3D11_DEBUG);
    //if (hint) {
    //    if (*hint == '1') {
            creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
    //    }
    //}

    // This array defines the set of DirectX hardware feature levels this app will support.
    // Note the ordering should be preserved.
    // Don't forget to declare your application's minimum required feature level in its
    // description.  All applications are assumed to support 9.1 unless otherwise stated.
    D3D_FEATURE_LEVEL featureLevels[] = 
    {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
        D3D_FEATURE_LEVEL_9_3,
        D3D_FEATURE_LEVEL_9_2,
        D3D_FEATURE_LEVEL_9_1
    };

    // Create the Direct3D 11 API device object and a corresponding context.
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    HRESULT result = S_OK;
    result = D3D11CreateDevice(
        nullptr, // Specify nullptr to use the default adapter.
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        creationFlags, // Set set debug and Direct2D compatibility flags.
        featureLevels, // List of feature levels this app can support.
        ARRAYSIZE(featureLevels),
        D3D11_SDK_VERSION, // Always set this to D3D11_SDK_VERSION for Windows Store apps.
        &device, // Returns the Direct3D device created.
        &data->featureLevel, // Returns feature level of device created.
        &context // Returns the device immediate context.
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", D3D11CreateDevice", result);
        return result;
    }

    // Get the Direct3D 11.1 API device and context interfaces.
    result = device.As(&(data->d3dDevice));
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device to ID3D11Device1", result);
        return result;
    }

    result = context.As(&data->d3dContext);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11DeviceContext to ID3D11DeviceContext1", result);
        return result;
    }

    //
    // Make note of the maximum texture size
    // Max texture sizes are documented on MSDN, at:
    // http://msdn.microsoft.com/en-us/library/windows/apps/ff476876.aspx
    //
    switch (data->d3dDevice->GetFeatureLevel()) {
        case D3D_FEATURE_LEVEL_11_1:
        case D3D_FEATURE_LEVEL_11_0:
            renderer->info.max_texture_width = renderer->info.max_texture_height = 16384;
            break;

        case D3D_FEATURE_LEVEL_10_1:
        case D3D_FEATURE_LEVEL_10_0:
            renderer->info.max_texture_width = renderer->info.max_texture_height = 8192;
            break;

        case D3D_FEATURE_LEVEL_9_3:
            renderer->info.max_texture_width = renderer->info.max_texture_height = 4096;
            break;

        case D3D_FEATURE_LEVEL_9_2:
        case D3D_FEATURE_LEVEL_9_1:
            renderer->info.max_texture_width = renderer->info.max_texture_height = 2048;
            break;
    }

    //
    // Load in SDL's one and only vertex shader:
    //
    result = data->d3dDevice->CreateVertexShader(
        D3D11_VertexShader,
        sizeof(D3D11_VertexShader),
        nullptr,
        &data->vertexShader
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateVertexShader", result);
        return result;
    }

    //
    // Create an input layout for SDL's vertex shader:
    //
    const D3D11_INPUT_ELEMENT_DESC vertexDesc[] = 
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    result = data->d3dDevice->CreateInputLayout(
        vertexDesc,
        ARRAYSIZE(vertexDesc),
        D3D11_VertexShader,
        sizeof(D3D11_VertexShader),
        &data->inputLayout
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateInputLayout", result);
        return result;
    }

    //
    // Load in SDL's pixel shaders
    //

    result = data->d3dDevice->CreatePixelShader(
        D3D11_PixelShader_Textures,
        sizeof(D3D11_PixelShader_Textures),
        nullptr,
        &data->texturePixelShader
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreatePixelShader ['textures' shader]", result);
        return result;
    }

    result = data->d3dDevice->CreatePixelShader(
        D3D11_PixelShader_Colors,
        sizeof(D3D11_PixelShader_Colors),
        nullptr,
        &data->colorPixelShader
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreatePixelShader ['color' shader]", result);
        return result;
    }

    //
    // Setup space to hold vertex shader constants:
    //
    CD3D11_BUFFER_DESC constantBufferDesc(sizeof(VertexShaderConstants), D3D11_BIND_CONSTANT_BUFFER);
    result = data->d3dDevice->CreateBuffer(
		&constantBufferDesc,
		nullptr,
        &data->vertexShaderConstants
		);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateBuffer [vertex shader constants]", result);
        return result;
    }

    //
    // Make sure that the vertex buffer, if already created, gets freed.
    // It will be recreated later.
    //
    data->vertexBuffer = nullptr;

    //
    // Create samplers to use when drawing textures:
    //
    D3D11_SAMPLER_DESC samplerDesc;
    samplerDesc.Filter = SDL_D3D11_NEAREST_PIXEL_FILTER;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = 1;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    samplerDesc.BorderColor[0] = 0.0f;
    samplerDesc.BorderColor[1] = 0.0f;
    samplerDesc.BorderColor[2] = 0.0f;
    samplerDesc.BorderColor[3] = 0.0f;
    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
    result = data->d3dDevice->CreateSamplerState(
        &samplerDesc,
        &data->nearestPixelSampler
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateSamplerState [nearest-pixel filter]", result);
        return result;
    }

    samplerDesc.Filter = SDL_D3D11_LINEAR_FILTER;
    result = data->d3dDevice->CreateSamplerState(
        &samplerDesc,
        &data->linearSampler
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateSamplerState [linear filter]", result);
        return result;
    }

    //
    // Setup the Direct3D rasterizer
    //
    D3D11_RASTERIZER_DESC rasterDesc;
    memset(&rasterDesc, 0, sizeof(rasterDesc));
	rasterDesc.AntialiasedLineEnable = false;
	rasterDesc.CullMode = D3D11_CULL_NONE;
	rasterDesc.DepthBias = 0;
	rasterDesc.DepthBiasClamp = 0.0f;
	rasterDesc.DepthClipEnable = true;
	rasterDesc.FillMode = D3D11_FILL_SOLID;
	rasterDesc.FrontCounterClockwise = false;
	rasterDesc.MultisampleEnable = false;
	rasterDesc.ScissorEnable = false;
	rasterDesc.SlopeScaledDepthBias = 0.0f;
	result = data->d3dDevice->CreateRasterizerState(&rasterDesc, &data->mainRasterizer);
	if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateRasterizerState", result);
        return result;
    }

    //
    // Create blending states:
    //
    result = D3D11_CreateBlendMode(
        renderer,
        TRUE,
        D3D11_BLEND_SRC_ALPHA,
        D3D11_BLEND_INV_SRC_ALPHA,
        &data->blendModeBlend);
    if (FAILED(result)) {
        // D3D11_CreateBlendMode will set the SDL error, if it fails
        return result;
    }

    result = D3D11_CreateBlendMode(
        renderer,
        TRUE,
        D3D11_BLEND_SRC_ALPHA,
        D3D11_BLEND_ONE,
        &data->blendModeAdd);
    if (FAILED(result)) {
        // D3D11_CreateBlendMode will set the SDL error, if it fails
        return result;
    }

    result = D3D11_CreateBlendMode(
        renderer,
        TRUE,
        D3D11_BLEND_ZERO,
        D3D11_BLEND_SRC_COLOR,
        &data->blendModeMod);
    if (FAILED(result)) {
        // D3D11_CreateBlendMode will set the SDL error, if it fails
        return result;
    }

    //
    // All done!
    //
    return S_OK;
}

#ifdef __WINRT__

static ABI::Windows::UI::Core::ICoreWindow *
D3D11_GetCoreWindowFromSDLRenderer(SDL_Renderer * renderer)
{
    SDL_Window * sdlWindow = renderer->window;
    if ( ! renderer->window ) {
        return nullptr;
    }

    SDL_SysWMinfo sdlWindowInfo;
    SDL_VERSION(&sdlWindowInfo.version);
    if ( ! SDL_GetWindowWMInfo(sdlWindow, &sdlWindowInfo) ) {
        return nullptr;
    }

    if (sdlWindowInfo.subsystem != SDL_SYSWM_WINRT) {
        return nullptr;
    }

    if ( ! sdlWindowInfo.info.winrt.window ) {
        return nullptr;
    }

    ABI::Windows::UI::Core::ICoreWindow * coreWindow = nullptr;
    if (FAILED(sdlWindowInfo.info.winrt.window->QueryInterface(&coreWindow))) {
        return nullptr;
    }

    return coreWindow;
}

// Method to convert a length in device-independent pixels (DIPs) to a length in physical pixels.
static float
D3D11_ConvertDipsToPixels(float dips)
{
    static const float dipsPerInch = 96.0f;
    return floor(dips * DisplayProperties::LogicalDpi / dipsPerInch + 0.5f); // Round to nearest integer.
}
#endif

#if WINAPI_FAMILY == WINAPI_FAMILY_APP
// TODO, WinRT, XAML: get the ISwapChainBackgroundPanelNative from something other than a global var
extern ISwapChainBackgroundPanelNative * WINRT_GlobalSwapChainBackgroundPanelNative;
#endif

static DXGI_MODE_ROTATION
D3D11_GetRotationForOrientation(Windows::Graphics::Display::DisplayOrientations orientation)
{
    switch (orientation)
    {
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
        //
        // Windows Phone rotations
        //
        case DisplayOrientations::Landscape:
            return DXGI_MODE_ROTATION_ROTATE90;
        case DisplayOrientations::Portrait:
            return DXGI_MODE_ROTATION_IDENTITY;
        case DisplayOrientations::LandscapeFlipped:
            return DXGI_MODE_ROTATION_ROTATE270;
        case DisplayOrientations::PortraitFlipped:
            return DXGI_MODE_ROTATION_ROTATE180;
#else
        //
        // Non-Windows-Phone rotations (ex: Windows 8, Windows RT)
        //
        case DisplayOrientations::Landscape:
            return DXGI_MODE_ROTATION_IDENTITY;
        case DisplayOrientations::Portrait:
            return DXGI_MODE_ROTATION_ROTATE270;
        case DisplayOrientations::LandscapeFlipped:
            return DXGI_MODE_ROTATION_ROTATE180;
        case DisplayOrientations::PortraitFlipped:
            return DXGI_MODE_ROTATION_ROTATE90;
#endif // WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP

        default:
            return DXGI_MODE_ROTATION_UNSPECIFIED;
    }
}

// Initialize all resources that change when the window's size changes.
// TODO, WinRT: get D3D11_CreateWindowSizeDependentResources working on Win32
HRESULT
D3D11_CreateWindowSizeDependentResources(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;
    HRESULT result = S_OK;
    ABI::Windows::UI::Core::ICoreWindow * coreWindow = D3D11_GetCoreWindowFromSDLRenderer(renderer);

    // Store the window bounds so the next time we get a SizeChanged event we can
    // avoid rebuilding everything if the size is identical.
    ABI::Windows::Foundation::Rect nativeWindowBounds;
    if (coreWindow) {
        result = coreWindow->get_Bounds(&nativeWindowBounds);
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__", ICoreWindow::get_Bounds [get native-window bounds]", result);
            return result;
        }
    } else {
        // TODO, WinRT, XAML: clean up window-bounds code in D3D11_CreateWindowSizeDependentResources
        SDL_DisplayMode displayMode;
        if (SDL_GetDesktopDisplayMode(0, &displayMode) < 0) {
            SDL_SetError(__FUNCTION__", Get Window Bounds (XAML): Unable to retrieve the native window's size");
            return E_FAIL;
        }

        nativeWindowBounds.Width = (FLOAT) displayMode.w;
        nativeWindowBounds.Height = (FLOAT) displayMode.h;
    }

    // TODO, WinRT, XAML: see if window/control sizes are in DIPs, or something else.  If something else, then adjust renderer size tracking accordingly.
    data->windowSizeInDIPs.x = nativeWindowBounds.Width;
    data->windowSizeInDIPs.y = nativeWindowBounds.Height;

    // Calculate the necessary swap chain and render target size in pixels.
    float windowWidth = D3D11_ConvertDipsToPixels(data->windowSizeInDIPs.x);
    float windowHeight = D3D11_ConvertDipsToPixels(data->windowSizeInDIPs.y);

    // The width and height of the swap chain must be based on the window's
    // landscape-oriented width and height. If the window is in a portrait
    // orientation, the dimensions must be reversed.
    data->orientation = DisplayProperties::CurrentOrientation;

#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
    const bool swapDimensions = false;
#else
    const bool swapDimensions =
        data->orientation == DisplayOrientations::Portrait ||
        data->orientation == DisplayOrientations::PortraitFlipped;
#endif
    data->renderTargetSize.x = swapDimensions ? windowHeight : windowWidth;
    data->renderTargetSize.y = swapDimensions ? windowWidth : windowHeight;

    if(data->swapChain != nullptr)
    {
        // If the swap chain already exists, resize it.
        result = data->swapChain->ResizeBuffers(
            2, // Double-buffered swap chain.
            static_cast<UINT>(data->renderTargetSize.x),
            static_cast<UINT>(data->renderTargetSize.y),
            DXGI_FORMAT_B8G8R8A8_UNORM,
            0
            );
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGISwapChain1::ResizeBuffers", result);
            return result;
        }
    }
    else
    {
        const bool usingXAML = (coreWindow == nullptr);

        // Otherwise, create a new one using the same adapter as the existing Direct3D device.
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {0};
        swapChainDesc.Width = static_cast<UINT>(data->renderTargetSize.x); // Match the size of the window.
        swapChainDesc.Height = static_cast<UINT>(data->renderTargetSize.y);
        swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // This is the most common swap chain format.
        swapChainDesc.Stereo = false;
        swapChainDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
        swapChainDesc.SampleDesc.Quality = 0;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = 2; // Use double-buffering to minimize latency.
#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
        swapChainDesc.Scaling = DXGI_SCALING_STRETCH; // On phone, only stretch and aspect-ratio stretch scaling are allowed.
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD; // On phone, no swap effects are supported.
#else
        if (usingXAML) {
            swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        } else {
            swapChainDesc.Scaling = DXGI_SCALING_NONE;
        }
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL; // All Windows Store apps must use this SwapEffect.
#endif
        swapChainDesc.Flags = 0;

        ComPtr<IDXGIDevice1> dxgiDevice;
        result = data->d3dDevice.As(&dxgiDevice);
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1 to IDXGIDevice1", result);
            return result;
        }

        ComPtr<IDXGIAdapter> dxgiAdapter;
        result = dxgiDevice->GetAdapter(&dxgiAdapter);
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIDevice1::GetAdapter", result);
            return result;
        }

        ComPtr<IDXGIFactory2> dxgiFactory;
        result = dxgiAdapter->GetParent(
            __uuidof(IDXGIFactory2), 
            &dxgiFactory
            );
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIAdapter::GetParent", result);
            return result;
        }

        if (usingXAML) {
            result = dxgiFactory->CreateSwapChainForComposition(
                data->d3dDevice.Get(),
                &swapChainDesc,
                nullptr,
                &data->swapChain);
            if (FAILED(result)) {
                WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIFactory2::CreateSwapChainForComposition", result);
                return result;
            }

#if WINAPI_FAMILY == WINAPI_FAMILY_APP
            result = WINRT_GlobalSwapChainBackgroundPanelNative->SetSwapChain(data->swapChain.Get());
            if (FAILED(result)) {
                WIN_SetErrorFromHRESULT(__FUNCTION__ ", ISwapChainBackgroundPanelNative::SetSwapChain", result);
                return result;
            }
#else
            SDL_SetError(__FUNCTION__ ", XAML support is not yet available for Windows Phone");
            return E_FAIL;
#endif
        } else {
            IUnknown * coreWindowAsIUnknown = nullptr;
            result = coreWindow->QueryInterface(&coreWindowAsIUnknown);
            if (FAILED(result)) {
                WIN_SetErrorFromHRESULT(__FUNCTION__ ", ICoreWindow to IUnknown", result);
                return result;
            }

            result = dxgiFactory->CreateSwapChainForCoreWindow(
                data->d3dDevice.Get(),
                coreWindowAsIUnknown,
                &swapChainDesc,
                nullptr, // Allow on all displays.
                &data->swapChain
                );
            if (FAILED(result)) {
                WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIFactory2::CreateSwapChainForCoreWindow", result);
                return result;
            }
        }
        
        // Ensure that DXGI does not queue more than one frame at a time. This both reduces latency and
        // ensures that the application will only render after each VSync, minimizing power consumption.
        result = dxgiDevice->SetMaximumFrameLatency(1);
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGIDevice1::SetMaximumFrameLatency", result);
            return result;
        }
    }
    
#if WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP
    // Set the proper orientation for the swap chain, and generate the
    // 3D matrix transformation for rendering to the rotated swap chain.
    //
    // To note, the call for this, IDXGISwapChain1::SetRotation, is not necessary
    // on Windows Phone, nor is it supported there.  It's only needed in Windows 8/RT.
    DXGI_MODE_ROTATION rotation = D3D11_GetRotationForOrientation(data->orientation);
    result = data->swapChain->SetRotation(rotation);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGISwapChain1::SetRotation" , result);
        return result;
    }
#endif

    // Create a render target view of the swap chain back buffer.
    ComPtr<ID3D11Texture2D> backBuffer;
    result = data->swapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        &backBuffer
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGISwapChain1::GetBuffer [back-buffer]", result);
        return result;
    }

    result = data->d3dDevice->CreateRenderTargetView(
        backBuffer.Get(),
        nullptr,
        &data->mainRenderTargetView
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateRenderTargetView", result);
        return result;
    }

    if (D3D11_UpdateViewport(renderer) != 0) {
        // D3D11_UpdateViewport will set the SDL error if it fails.
        return E_FAIL;
    }

    return S_OK;
}

// This method is called when the window's size changes.
HRESULT
D3D11_UpdateForWindowSizeChange(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;
    HRESULT result = S_OK;
    ABI::Windows::UI::Core::ICoreWindow * coreWindow = D3D11_GetCoreWindowFromSDLRenderer(renderer);
    ABI::Windows::Foundation::Rect coreWindowBounds;

    result = coreWindow->get_Bounds(&coreWindowBounds);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ICoreWindow::get_Bounds [get window bounds]", result);
        return result;
    }

    if (coreWindowBounds.Width  != data->windowSizeInDIPs.x ||
        coreWindowBounds.Height != data->windowSizeInDIPs.y ||
        data->orientation != DisplayProperties::CurrentOrientation)
    {
        ID3D11RenderTargetView* nullViews[] = {nullptr};
        data->d3dContext->OMSetRenderTargets(ARRAYSIZE(nullViews), nullViews, nullptr);
        data->mainRenderTargetView = nullptr;
        data->d3dContext->Flush();
        result = D3D11_CreateWindowSizeDependentResources(renderer);
        if (FAILED(result)) {
            /* D3D11_CreateWindowSizeDependentResources will set the SDL error */
            return result;
        }
    }

    return S_OK;
}

HRESULT
D3D11_HandleDeviceLost(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;
    HRESULT result = S_OK;

    // Reset these member variables to ensure that D3D11_UpdateForWindowSizeChange recreates all resources.
    data->windowSizeInDIPs.x = 0;
    data->windowSizeInDIPs.y = 0;
    data->swapChain = nullptr;

    result = D3D11_CreateDeviceResources(renderer);
    if (FAILED(result)) {
        /* D3D11_CreateDeviceResources will set the SDL error */
        return result;
    }

    result = D3D11_UpdateForWindowSizeChange(renderer);
    if (FAILED(result)) {
        /* D3D11_UpdateForWindowSizeChange will set the SDL error */
        return result;
    }

    return S_OK;
}

static void
D3D11_WindowEvent(SDL_Renderer * renderer, const SDL_WindowEvent *event)
{
    //D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;

    if (event->event == SDL_WINDOWEVENT_SIZE_CHANGED) {
        D3D11_UpdateForWindowSizeChange(renderer);
    }
}

static D3D11_FILTER
GetScaleQuality(void)
{
    const char *hint = SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY);
    if (!hint || *hint == '0' || SDL_strcasecmp(hint, "nearest") == 0) {
        return SDL_D3D11_NEAREST_PIXEL_FILTER;
    } else /* if (*hint == '1' || SDL_strcasecmp(hint, "linear") == 0) */ {
        return SDL_D3D11_LINEAR_FILTER;
    }
}

static int
D3D11_CreateTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    D3D11_TextureData *textureData;
    HRESULT result;
    DXGI_FORMAT textureFormat = SDLPixelFormatToDXGIFormat(texture->format);
    if (textureFormat == SDL_PIXELFORMAT_UNKNOWN) {
        return SDL_SetError("%s, An unsupported SDL pixel format (0x%x) was specified",
            __FUNCTION__, texture->format);
    }

    textureData = new D3D11_TextureData;
    if (!textureData) {
        SDL_OutOfMemory();
        return -1;
    }
    textureData->pixelFormat = SDL_AllocFormat(texture->format);
    textureData->lockedTexturePosition = XMINT2(0, 0);
    textureData->scaleMode = GetScaleQuality();

    texture->driverdata = textureData;

    D3D11_TEXTURE2D_DESC textureDesc = {0};
    textureDesc.Width = texture->w;
    textureDesc.Height = texture->h;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = textureFormat;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.MiscFlags = 0;

    if (texture->access == SDL_TEXTUREACCESS_STREAMING) {
        textureDesc.Usage = D3D11_USAGE_DYNAMIC;
        textureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else {
        textureDesc.Usage = D3D11_USAGE_DEFAULT;
        textureDesc.CPUAccessFlags = 0;
    }

    if (texture->access == SDL_TEXTUREACCESS_TARGET) {
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    } else {
        textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    }

#if 0
    // Fill the texture with a non-black color, for debugging purposes:
    const int numPixels = textureDesc.Width * textureDesc.Height;
    const int pixelSizeInBytes = textureData->pixelFormat->BytesPerPixel;
    std::vector<uint8> initialTexturePixels(numPixels * pixelSizeInBytes, 0x00);
    for (int i = 0; i < (numPixels * pixelSizeInBytes); i += pixelSizeInBytes) {
        initialTexturePixels[i+0] = 0xff;
        initialTexturePixels[i+1] = 0xff;
        initialTexturePixels[i+2] = 0x00;
        initialTexturePixels[i+3] = 0xff;
    }
    D3D11_SUBRESOURCE_DATA initialTextureData = {0};
    initialTextureData.pSysMem = (void *)&(initialTexturePixels[0]);
    initialTextureData.SysMemPitch = textureDesc.Width * pixelSizeInBytes;
    initialTextureData.SysMemSlicePitch = numPixels * pixelSizeInBytes;
#endif

    result = rendererData->d3dDevice->CreateTexture2D(
        &textureDesc,
        NULL,   // &initialTextureData,
        &textureData->mainTexture
        );
    if (FAILED(result)) {
        D3D11_DestroyTexture(renderer, texture);
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateTexture2D", result);
        return -1;
    }

    if (texture->access & SDL_TEXTUREACCESS_TARGET) {
        D3D11_RENDER_TARGET_VIEW_DESC renderTargetViewDesc;
        renderTargetViewDesc.Format = textureDesc.Format;
        renderTargetViewDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        renderTargetViewDesc.Texture2D.MipSlice = 0;

        result = rendererData->d3dDevice->CreateRenderTargetView(
            textureData->mainTexture.Get(),
            &renderTargetViewDesc,
            &textureData->mainTextureRenderTargetView);
        if (FAILED(result)) {
            D3D11_DestroyTexture(renderer, texture);
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateRenderTargetView", result);
            return -1;
        }
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC resourceViewDesc;
    resourceViewDesc.Format = textureDesc.Format;
    resourceViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceViewDesc.Texture2D.MostDetailedMip = 0;
    resourceViewDesc.Texture2D.MipLevels = textureDesc.MipLevels;
    result = rendererData->d3dDevice->CreateShaderResourceView(
        textureData->mainTexture.Get(),
        &resourceViewDesc,
        &textureData->mainTextureResourceView
        );
    if (FAILED(result)) {
        D3D11_DestroyTexture(renderer, texture);
        WIN_SetErrorFromHRESULT(__FUNCTION__ "ID3D11Device1::CreateShaderResourceView", result);
        return -1;
    }

    return 0;
}

static void
D3D11_DestroyTexture(SDL_Renderer * renderer,
                     SDL_Texture * texture)
{
    D3D11_TextureData *textureData = (D3D11_TextureData *) texture->driverdata;

    if (textureData) {
        if (textureData->pixelFormat) {
            SDL_FreeFormat(textureData->pixelFormat);
            textureData->pixelFormat = NULL;
        }

        delete textureData;
        texture->driverdata = NULL;
    }
}

static int
D3D11_UpdateTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                    const SDL_Rect * rect, const void * srcPixels,
                    int srcPitch)
{
    // Lock the texture, retrieving a buffer to write pixel data to:
    void * destPixels = NULL;
    int destPitch = 0;
    if (D3D11_LockTexture(renderer, texture, rect, &destPixels, &destPitch) != 0) {
        // An error is already set.  Attach some info to it, then return to
        // the caller.
        std::string errorMessage = string(__FUNCTION__ ", Lock Texture Failed: ") + SDL_GetError();
        return SDL_SetError(errorMessage.c_str());
    }

    // Copy pixel data to the locked texture's memory:
    for (int y = 0; y < rect->h; ++y) {
        memcpy(
            ((Uint8 *)destPixels) + (destPitch * y),
            ((Uint8 *)srcPixels) + (srcPitch * y),
            srcPitch
            );
    }

    // Commit the texture's memory back to Direct3D:
    D3D11_UnlockTexture(renderer, texture);

    // Return to the caller:
    return 0;
}

static int
D3D11_LockTexture(SDL_Renderer * renderer, SDL_Texture * texture,
                  const SDL_Rect * rect, void **pixels, int *pitch)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    D3D11_TextureData *textureData = (D3D11_TextureData *) texture->driverdata;
    HRESULT result = S_OK;

    if (textureData->stagingTexture) {
        return SDL_SetError("texture is already locked");
    }
    
    // Create a 'staging' texture, which will be used to write to a portion
    // of the main texture.  This is necessary, as Direct3D 11.1 does not
    // have the ability to write a CPU-bound pixel buffer to a rectangular
    // subrect of a texture.  Direct3D 11.1 can, however, write a pixel
    // buffer to an entire texture, hence the use of a staging texture.
    //
    // TODO, WinRT: consider avoiding the use of a staging texture in D3D11_LockTexture if/when the entire texture is being updated
    D3D11_TEXTURE2D_DESC stagingTextureDesc;
    textureData->mainTexture->GetDesc(&stagingTextureDesc);
    stagingTextureDesc.Width = rect->w;
    stagingTextureDesc.Height = rect->h;
    stagingTextureDesc.BindFlags = 0;
    stagingTextureDesc.MiscFlags = 0;
    stagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    stagingTextureDesc.Usage = D3D11_USAGE_STAGING;
    result = rendererData->d3dDevice->CreateTexture2D(
        &stagingTextureDesc,
        NULL,
        &textureData->stagingTexture);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateTexture2D [create staging texture]", result);
        return -1;
    }

    // Get a write-only pointer to data in the staging texture:
    D3D11_MAPPED_SUBRESOURCE textureMemory = {0};
    result = rendererData->d3dContext->Map(
        textureData->stagingTexture.Get(),
        D3D11CalcSubresource(0, 0, 0),
        D3D11_MAP_WRITE,
        0,
        &textureMemory
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11DeviceContext1::Map [map staging texture]", result);
        textureData->stagingTexture = nullptr;
        return -1;
    }

    // Make note of where the staging texture will be written to (on a
    // call to SDL_UnlockTexture):
    textureData->lockedTexturePosition = XMINT2(rect->x, rect->y);

    // Make sure the caller has information on the texture's pixel buffer,
    // then return:
    *pixels = textureMemory.pData;
    *pitch = textureMemory.RowPitch;
    return 0;
}

static void
D3D11_UnlockTexture(SDL_Renderer * renderer, SDL_Texture * texture)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    D3D11_TextureData *textureData = (D3D11_TextureData *) texture->driverdata;

    // Commit the pixel buffer's changes back to the staging texture:
    rendererData->d3dContext->Unmap(
        textureData->stagingTexture.Get(),
        0);

    // Copy the staging texture's contents back to the main texture:
    rendererData->d3dContext->CopySubresourceRegion(
        textureData->mainTexture.Get(),
        D3D11CalcSubresource(0, 0, 0),
        textureData->lockedTexturePosition.x,
        textureData->lockedTexturePosition.y,
        0,
        textureData->stagingTexture.Get(),
        D3D11CalcSubresource(0, 0, 0),
        NULL);

    // Clean up and return:
    textureData->stagingTexture = nullptr;
    textureData->lockedTexturePosition = XMINT2(0, 0);
}

static int
D3D11_SetRenderTarget(SDL_Renderer * renderer, SDL_Texture * texture)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;

    if (texture == NULL) {
        rendererData->currentOffscreenRenderTargetView = nullptr;
        return 0;
    }

    D3D11_TextureData *textureData = (D3D11_TextureData *) texture->driverdata;

    if (!textureData->mainTextureRenderTargetView) {
        return SDL_SetError("specified texture is not a render target");
    }

    rendererData->currentOffscreenRenderTargetView = textureData->mainTextureRenderTargetView;

    return 0;
}

static int
D3D11_UpdateViewport(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;

    if (renderer->viewport.w == 0 || renderer->viewport.h == 0) {
        // If the viewport is empty, assume that it is because
        // SDL_CreateRenderer is calling it, and will call it again later
        // with a non-empty viewport.
        return 0;
    }

    // Make sure the SDL viewport gets rotated to that of the physical display's orientation.
    // Keep in mind here that the Y-axis will be been inverted (from Direct3D's
    // default coordinate system) so rotations will be done in the opposite
    // direction of the DXGI_MODE_ROTATION enumeration.
    switch (D3D11_GetRotationForOrientation(data->orientation))
    {
        case DXGI_MODE_ROTATION_IDENTITY:
            XMStoreFloat4x4(&data->vertexShaderConstantsData.projection, XMMatrixIdentity());
            break;
        case DXGI_MODE_ROTATION_ROTATE270:
            XMStoreFloat4x4(&data->vertexShaderConstantsData.projection, XMMatrixRotationZ(XM_PIDIV2));
            break;
        case DXGI_MODE_ROTATION_ROTATE180:
            XMStoreFloat4x4(&data->vertexShaderConstantsData.projection, XMMatrixRotationZ(XM_PI));
            break;
        case DXGI_MODE_ROTATION_ROTATE90:
            XMStoreFloat4x4(&data->vertexShaderConstantsData.projection, XMMatrixRotationZ(-XM_PIDIV2));
            break;
        default:
            return SDL_SetError("An unknown DisplayOrientation is being used");
    }

    //
    // Update the view matrix
    //
    float viewportWidth = (float) renderer->viewport.w;
    float viewportHeight = (float) renderer->viewport.h;
    XMStoreFloat4x4(&data->vertexShaderConstantsData.view,
        XMMatrixMultiply(
            XMMatrixScaling(2.0f / viewportWidth, 2.0f / viewportHeight, 1.0f),
            XMMatrixMultiply(
                XMMatrixTranslation(-1, -1, 0),
                XMMatrixRotationX(XM_PI)
                )));
#if 0
    data->vertexShaderConstantsData.view = XMMatrixIdentity();
#endif

    //
    // Reset the model matrix
    //
    XMStoreFloat4x4(&data->vertexShaderConstantsData.model, XMMatrixIdentity());

    //
    // Update the Direct3D viewport, which seems to be aligned to the
    // swap buffer's coordinate space, which is always in either
    // a landscape mode, for all Windows 8/RT devices, or a portrait mode,
    // for Windows Phone devices.
    //
    SDL_FRect orientationAlignedViewport;

#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
    const bool swapDimensions =
        data->orientation == DisplayOrientations::Landscape ||
        data->orientation == DisplayOrientations::LandscapeFlipped;
#else
    const bool swapDimensions =
        data->orientation == DisplayOrientations::Portrait ||
        data->orientation == DisplayOrientations::PortraitFlipped;
#endif
    if (swapDimensions) {
        orientationAlignedViewport.x = (float) renderer->viewport.y;
        orientationAlignedViewport.y = (float) renderer->viewport.x;
        orientationAlignedViewport.w = (float) renderer->viewport.h;
        orientationAlignedViewport.h = (float) renderer->viewport.w;
    } else {
        orientationAlignedViewport.x = (float) renderer->viewport.x;
        orientationAlignedViewport.y = (float) renderer->viewport.y;
        orientationAlignedViewport.w = (float) renderer->viewport.w;
        orientationAlignedViewport.h = (float) renderer->viewport.h;
    }
    // TODO, WinRT: get custom viewports working with non-Landscape modes (Portrait, PortraitFlipped, and LandscapeFlipped)

    D3D11_VIEWPORT viewport;
    memset(&viewport, 0, sizeof(viewport));
    viewport.TopLeftX = orientationAlignedViewport.x;
    viewport.TopLeftY = orientationAlignedViewport.y;
    viewport.Width = orientationAlignedViewport.w;
    viewport.Height = orientationAlignedViewport.h;
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;
    data->d3dContext->RSSetViewports(1, &viewport);

#if 0
    SDL_Log("%s, oav={%.0f,%.0f,%.0f,%.0f}, rend={%.0f,%.0f}\n",
        __FUNCTION__,
        orientationAlignedViewport.x,
        orientationAlignedViewport.y,
        orientationAlignedViewport.w,
        orientationAlignedViewport.h,
        data->renderTargetSize.x,
        data->renderTargetSize.y);
#endif

    return 0;
}

static int
D3D11_UpdateClipRect(SDL_Renderer * renderer)
{
    // TODO, WinRT: implement D3D11_UpdateClipRect
    return 0;
}

static ComPtr<ID3D11RenderTargetView> &
D3D11_GetCurrentRenderTargetView(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;
    if (data->currentOffscreenRenderTargetView) {
        return data->currentOffscreenRenderTargetView;
    } else {
        return data->mainRenderTargetView;
    }
}

static int
D3D11_RenderClear(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;
    const float colorRGBA[] = {
        (renderer->r / 255.0f),
        (renderer->g / 255.0f),
        (renderer->b / 255.0f),
        (renderer->a / 255.0f)
    };
    data->d3dContext->ClearRenderTargetView(
        D3D11_GetCurrentRenderTargetView(renderer).Get(),
        colorRGBA
        );
    return 0;
}

static int
D3D11_UpdateVertexBuffer(SDL_Renderer *renderer,
                         const void * vertexData, size_t dataSizeInBytes)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    HRESULT result = S_OK;
    D3D11_BUFFER_DESC vertexBufferDesc;

    if (rendererData->vertexBuffer) {
        rendererData->vertexBuffer->GetDesc(&vertexBufferDesc);
    } else {
        memset(&vertexBufferDesc, 0, sizeof(vertexBufferDesc));
    }

    if (vertexBufferDesc.ByteWidth >= dataSizeInBytes) {
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        ZeroMemory(&mappedResource, sizeof(D3D11_MAPPED_SUBRESOURCE));
        result = rendererData->d3dContext->Map(rendererData->vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11DeviceContext1::Map [vertex buffer]", result);
            return -1;
        }
        memcpy(mappedResource.pData, vertexData, dataSizeInBytes);
        rendererData->d3dContext->Unmap(rendererData->vertexBuffer.Get(), 0);
    } else {
        vertexBufferDesc.ByteWidth = dataSizeInBytes;
        vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
        vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA vertexBufferData = {0};
        vertexBufferData.pSysMem = vertexData;
        vertexBufferData.SysMemPitch = 0;
        vertexBufferData.SysMemSlicePitch = 0;

        result = rendererData->d3dDevice->CreateBuffer(
            &vertexBufferDesc,
            &vertexBufferData,
            &rendererData->vertexBuffer
            );
        if (FAILED(result)) {
            WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateBuffer [vertex buffer]", result);
            return -1;
        }
    }

    UINT stride = sizeof(VertexPositionColor);
    UINT offset = 0;
    rendererData->d3dContext->IASetVertexBuffers(
        0,
        1,
        rendererData->vertexBuffer.GetAddressOf(),
        &stride,
        &offset
        );

    return 0;
}

static void
D3D11_RenderStartDrawOp(SDL_Renderer * renderer)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;

    rendererData->d3dContext->OMSetRenderTargets(
        1,
        D3D11_GetCurrentRenderTargetView(renderer).GetAddressOf(),
        nullptr
        );
}

static void
D3D11_RenderSetBlendMode(SDL_Renderer * renderer, SDL_BlendMode blendMode)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    switch (blendMode) {
        case SDL_BLENDMODE_BLEND:
            rendererData->d3dContext->OMSetBlendState(rendererData->blendModeBlend.Get(), 0, 0xFFFFFFFF);
            break;
        case SDL_BLENDMODE_ADD:
            rendererData->d3dContext->OMSetBlendState(rendererData->blendModeAdd.Get(), 0, 0xFFFFFFFF);
            break;
        case SDL_BLENDMODE_MOD:
            rendererData->d3dContext->OMSetBlendState(rendererData->blendModeMod.Get(), 0, 0xFFFFFFFF);
            break;
        case SDL_BLENDMODE_NONE:
            rendererData->d3dContext->OMSetBlendState(NULL, 0, 0xFFFFFFFF);
            break;
    }
}

static void
D3D11_SetPixelShader(SDL_Renderer * renderer,
                     ID3D11PixelShader * shader,
                     ID3D11ShaderResourceView * shaderResource,
                     ID3D11SamplerState * sampler)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    rendererData->d3dContext->PSSetShader(shader, nullptr, 0);
    rendererData->d3dContext->PSSetShaderResources(0, 1, &shaderResource);
    rendererData->d3dContext->PSSetSamplers(0, 1, &sampler);
}

static void
D3D11_RenderFinishDrawOp(SDL_Renderer * renderer,
                         D3D11_PRIMITIVE_TOPOLOGY primitiveTopology,
                         UINT vertexCount)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;

    rendererData->d3dContext->UpdateSubresource(
        rendererData->vertexShaderConstants.Get(),
        0,
        NULL,
        &rendererData->vertexShaderConstantsData,
        0,
        0
        );

    rendererData->d3dContext->IASetPrimitiveTopology(primitiveTopology);
    rendererData->d3dContext->IASetInputLayout(rendererData->inputLayout.Get());
    rendererData->d3dContext->VSSetShader(rendererData->vertexShader.Get(), nullptr, 0);
    rendererData->d3dContext->VSSetConstantBuffers(0, 1, rendererData->vertexShaderConstants.GetAddressOf());
    rendererData->d3dContext->RSSetState(rendererData->mainRasterizer.Get());
    rendererData->d3dContext->Draw(vertexCount, 0);
}

static int
D3D11_RenderDrawPoints(SDL_Renderer * renderer,
                       const SDL_FPoint * points, int count)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    float r, g, b, a;

    r = (float)(renderer->r / 255.0f);
    g = (float)(renderer->g / 255.0f);
    b = (float)(renderer->b / 255.0f);
    a = (float)(renderer->a / 255.0f);

    VertexPositionColor * vertices = SDL_stack_alloc(VertexPositionColor, count);
    for (int i = 0; i < min(count, 128); ++i) {
        const VertexPositionColor v = {XMFLOAT3(points[i].x, points[i].y, 0.0f),  XMFLOAT2(0.0f, 0.0f), XMFLOAT4(r, g, b, a)};
        vertices[i] = v;
    }

    D3D11_RenderStartDrawOp(renderer);
    D3D11_RenderSetBlendMode(renderer, renderer->blendMode);
    if (D3D11_UpdateVertexBuffer(renderer, vertices, (unsigned int)count * sizeof(VertexPositionColor)) != 0) {
        SDL_stack_free(vertices);
        return -1;
    }

    D3D11_SetPixelShader(
        renderer,
        rendererData->colorPixelShader.Get(),
        nullptr,
        nullptr);

    D3D11_RenderFinishDrawOp(renderer, D3D11_PRIMITIVE_TOPOLOGY_POINTLIST, count);
    SDL_stack_free(vertices);
    return 0;
}

static int
D3D11_RenderDrawLines(SDL_Renderer * renderer,
                      const SDL_FPoint * points, int count)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    float r, g, b, a;

    r = (float)(renderer->r / 255.0f);
    g = (float)(renderer->g / 255.0f);
    b = (float)(renderer->b / 255.0f);
    a = (float)(renderer->a / 255.0f);

    VertexPositionColor * vertices = SDL_stack_alloc(VertexPositionColor, count);
    for (int i = 0; i < count; ++i) {
        const VertexPositionColor v = {XMFLOAT3(points[i].x, points[i].y, 0.0f),  XMFLOAT2(0.0f, 0.0f), XMFLOAT4(r, g, b, a)};
        vertices[i] = v;
    }

    D3D11_RenderStartDrawOp(renderer);
    D3D11_RenderSetBlendMode(renderer, renderer->blendMode);
    if (D3D11_UpdateVertexBuffer(renderer, vertices, (unsigned int)count * sizeof(VertexPositionColor)) != 0) {
        SDL_stack_free(vertices);
        return -1;
    }

    D3D11_SetPixelShader(
        renderer,
        rendererData->colorPixelShader.Get(),
        nullptr,
        nullptr);

    D3D11_RenderFinishDrawOp(renderer, D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP, count);
    SDL_stack_free(vertices);
    return 0;
}

static int
D3D11_RenderFillRects(SDL_Renderer * renderer,
                      const SDL_FRect * rects, int count)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    float r, g, b, a;

    r = (float)(renderer->r / 255.0f);
    g = (float)(renderer->g / 255.0f);
    b = (float)(renderer->b / 255.0f);
    a = (float)(renderer->a / 255.0f);

#if 0
    // Set up a test pattern:
    SDL_FRect _rects[] = {
        {-1.1f, 1.1f, 1.1f, -1.1f},
        {-1.0f, 1.0f, 1.0f, -1.0f},     // red
        {0.0f, 1.0f, 1.0f, -1.0f},      // green
        {-1.0f, 0.0f, 1.0f, -1.0f},     // blue
        {0.0f, 0.0f, 1.0f, -1.0f}       // white
    };
    count = sizeof(_rects) / sizeof(SDL_FRect);
#define rects _rects
#endif

    for (int i = 0; i < count; ++i) {
        D3D11_RenderStartDrawOp(renderer);
        D3D11_RenderSetBlendMode(renderer, renderer->blendMode);

#if 0
        // Set colors for the test pattern:
        a = 1.0f;
        switch (i) {
            case 0: r = 1.0f; g = 1.0f; b = 0.0f; break;
            case 1: r = 1.0f; g = 0.0f; b = 0.0f; break;
            case 2: r = 0.0f; g = 1.0f; b = 0.0f; break;
            case 3: r = 0.0f; g = 0.0f; b = 1.0f; break;
            case 4: r = 1.0f; g = 1.0f; b = 1.0f; break;
        }
#endif

        VertexPositionColor vertices[] = {
            {XMFLOAT3(rects[i].x, rects[i].y, 0.0f),                           XMFLOAT2(0.0f, 0.0f), XMFLOAT4(r, g, b, a)},
            {XMFLOAT3(rects[i].x, rects[i].y + rects[i].h, 0.0f),              XMFLOAT2(0.0f, 0.0f), XMFLOAT4(r, g, b, a)},
            {XMFLOAT3(rects[i].x + rects[i].w, rects[i].y, 0.0f),              XMFLOAT2(0.0f, 0.0f), XMFLOAT4(r, g, b, a)},
            {XMFLOAT3(rects[i].x + rects[i].w, rects[i].y + rects[i].h, 0.0f), XMFLOAT2(0.0f, 0.0f), XMFLOAT4(r, g, b, a)},
        };
        if (D3D11_UpdateVertexBuffer(renderer, vertices, sizeof(vertices)) != 0) {
            return -1;
        }

        D3D11_SetPixelShader(
            renderer,
            rendererData->colorPixelShader.Get(),
            nullptr,
            nullptr);

        D3D11_RenderFinishDrawOp(renderer, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertices) / sizeof(VertexPositionColor));
    }

    return 0;
}

static ID3D11SamplerState *
D3D11_RenderGetSampler(SDL_Renderer * renderer, SDL_Texture * texture)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    D3D11_TextureData *textureData = (D3D11_TextureData *) texture->driverdata;

    switch (textureData->scaleMode) {
        case SDL_D3D11_NEAREST_PIXEL_FILTER:
            return rendererData->nearestPixelSampler.Get();
        case SDL_D3D11_LINEAR_FILTER:
            return rendererData->linearSampler.Get();
        default:
            return NULL;
    }
}

static int
D3D11_RenderCopy(SDL_Renderer * renderer, SDL_Texture * texture,
                 const SDL_Rect * srcrect, const SDL_FRect * dstrect)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    D3D11_TextureData *textureData = (D3D11_TextureData *) texture->driverdata;

    D3D11_RenderStartDrawOp(renderer);
    D3D11_RenderSetBlendMode(renderer, texture->blendMode);

    float minu = (float) srcrect->x / texture->w;
    float maxu = (float) (srcrect->x + srcrect->w) / texture->w;
    float minv = (float) srcrect->y / texture->h;
    float maxv = (float) (srcrect->y + srcrect->h) / texture->h;

    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    if (texture->modMode & SDL_TEXTUREMODULATE_COLOR) {
        r = (float)(texture->r / 255.0f);
        g = (float)(texture->g / 255.0f);
        b = (float)(texture->b / 255.0f);
    }
    if (texture->modMode & SDL_TEXTUREMODULATE_ALPHA) {
        a = (float)(texture->a / 255.0f);
    }

    VertexPositionColor vertices[] = {
        {XMFLOAT3(dstrect->x, dstrect->y, 0.0f),                           XMFLOAT2(minu, minv), XMFLOAT4(r, g, b, a)},
        {XMFLOAT3(dstrect->x, dstrect->y + dstrect->h, 0.0f),              XMFLOAT2(minu, maxv), XMFLOAT4(r, g, b, a)},
        {XMFLOAT3(dstrect->x + dstrect->w, dstrect->y, 0.0f),              XMFLOAT2(maxu, minv), XMFLOAT4(r, g, b, a)},
        {XMFLOAT3(dstrect->x + dstrect->w, dstrect->y + dstrect->h, 0.0f), XMFLOAT2(maxu, maxv), XMFLOAT4(r, g, b, a)},
    };
    if (D3D11_UpdateVertexBuffer(renderer, vertices, sizeof(vertices)) != 0) {
        return -1;
    }

    ID3D11SamplerState *textureSampler = D3D11_RenderGetSampler(renderer, texture);
    D3D11_SetPixelShader(
        renderer,
        rendererData->texturePixelShader.Get(),
        textureData->mainTextureResourceView.Get(),
        textureSampler);

    D3D11_RenderFinishDrawOp(renderer, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertices) / sizeof(VertexPositionColor));

    return 0;
}

static int
D3D11_RenderCopyEx(SDL_Renderer * renderer, SDL_Texture * texture,
                   const SDL_Rect * srcrect, const SDL_FRect * dstrect,
                   const double angle, const SDL_FPoint * center, const SDL_RendererFlip flip)
{
    D3D11_RenderData *rendererData = (D3D11_RenderData *) renderer->driverdata;
    D3D11_TextureData *textureData = (D3D11_TextureData *) texture->driverdata;

    D3D11_RenderStartDrawOp(renderer);
    D3D11_RenderSetBlendMode(renderer, texture->blendMode);

    float minu = (float) srcrect->x / texture->w;
    float maxu = (float) (srcrect->x + srcrect->w) / texture->w;
    float minv = (float) srcrect->y / texture->h;
    float maxv = (float) (srcrect->y + srcrect->h) / texture->h;

    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;
    if (texture->modMode & SDL_TEXTUREMODULATE_COLOR) {
        r = (float)(texture->r / 255.0f);
        g = (float)(texture->g / 255.0f);
        b = (float)(texture->b / 255.0f);
    }
    if (texture->modMode & SDL_TEXTUREMODULATE_ALPHA) {
        a = (float)(texture->a / 255.0f);
    }

    if (flip & SDL_FLIP_HORIZONTAL) {
        float tmp = maxu;
        maxu = minu;
        minu = tmp;
    }
    if (flip & SDL_FLIP_VERTICAL) {
        float tmp = maxv;
        maxv = minv;
        minv = tmp;
    }

    XMFLOAT4X4 oldModelMatrix = rendererData->vertexShaderConstantsData.model;
    XMStoreFloat4x4(
        &rendererData->vertexShaderConstantsData.model,
        XMMatrixMultiply(
            XMMatrixRotationZ((float)(XM_PI * (float) angle / 180.0f)),
            XMMatrixTranslation(dstrect->x + center->x, dstrect->y + center->y, 0)
            ));

    const float minx = -center->x;
    const float maxx = dstrect->w - center->x;
    const float miny = -center->y;
    const float maxy = dstrect->h - center->y;

    VertexPositionColor vertices[] = {
        {XMFLOAT3(minx, miny, 0.0f), XMFLOAT2(minu, minv), XMFLOAT4(r, g, b, a)},
        {XMFLOAT3(minx, maxy, 0.0f), XMFLOAT2(minu, maxv), XMFLOAT4(r, g, b, a)},
        {XMFLOAT3(maxx, miny, 0.0f), XMFLOAT2(maxu, minv), XMFLOAT4(r, g, b, a)},
        {XMFLOAT3(maxx, maxy, 0.0f), XMFLOAT2(maxu, maxv), XMFLOAT4(r, g, b, a)},
    };
    if (D3D11_UpdateVertexBuffer(renderer, vertices, sizeof(vertices)) != 0) {
        return -1;
    }

    ID3D11SamplerState *textureSampler = D3D11_RenderGetSampler(renderer, texture);
    D3D11_SetPixelShader(
        renderer,
        rendererData->texturePixelShader.Get(),
        textureData->mainTextureResourceView.Get(),
        textureSampler);

    D3D11_RenderFinishDrawOp(renderer, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP, sizeof(vertices) / sizeof(VertexPositionColor));

    rendererData->vertexShaderConstantsData.model = oldModelMatrix;

    return 0;
}

static int
D3D11_RenderReadPixels(SDL_Renderer * renderer, const SDL_Rect * rect,
                       Uint32 format, void * pixels, int pitch)
{
    D3D11_RenderData * data = (D3D11_RenderData *) renderer->driverdata;
    HRESULT result = S_OK;

    // Retrieve a pointer to the back buffer:
    ComPtr<ID3D11Texture2D> backBuffer;
    result = data->swapChain->GetBuffer(
        0,
        __uuidof(ID3D11Texture2D),
        &backBuffer
        );
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", IDXGISwapChain1::GetBuffer [get back buffer]", result);
        return -1;
    }

    // Create a staging texture to copy the screen's data to:
    ComPtr<ID3D11Texture2D> stagingTexture;
    D3D11_TEXTURE2D_DESC stagingTextureDesc;
    backBuffer->GetDesc(&stagingTextureDesc);
    stagingTextureDesc.Width = rect->w;
    stagingTextureDesc.Height = rect->h;
    stagingTextureDesc.BindFlags = 0;
    stagingTextureDesc.MiscFlags = 0;
    stagingTextureDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingTextureDesc.Usage = D3D11_USAGE_STAGING;
    result = data->d3dDevice->CreateTexture2D(
        &stagingTextureDesc,
        NULL,
        &stagingTexture);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11Device1::CreateTexture2D [create staging texture]", result);
        return -1;
    }

    // Copy the desired portion of the back buffer to the staging texture:
    D3D11_BOX srcBox;
    switch (D3D11_GetRotationForOrientation(data->orientation)) {
        case DXGI_MODE_ROTATION_IDENTITY:
            srcBox.left = rect->x;
            srcBox.right = rect->x + rect->w;
            srcBox.top = rect->y;
            srcBox.bottom = rect->y + rect->h;
            break;
        case DXGI_MODE_ROTATION_ROTATE270:
            srcBox.left = rect->y;
            srcBox.right = rect->y + rect->h;
            srcBox.top = renderer->viewport.w - rect->x - rect->w;
            srcBox.bottom = renderer->viewport.w - rect->x;
            break;
        case DXGI_MODE_ROTATION_ROTATE180:
            srcBox.left = renderer->viewport.w - rect->x - rect->w;
            srcBox.right = renderer->viewport.w - rect->x;
            srcBox.top = renderer->viewport.h - rect->y - rect->h;
            srcBox.bottom = renderer->viewport.h - rect->y;
            break;
        case DXGI_MODE_ROTATION_ROTATE90:
            srcBox.left = renderer->viewport.h - rect->y - rect->h;
            srcBox.right = renderer->viewport.h - rect->y;
            srcBox.top = rect->x;
            srcBox.bottom = rect->x + rect->h;
            break;
        default:
            return SDL_SetError("The physical display is in an unknown or unsupported orientation");
    }
    srcBox.front = 0;
    srcBox.back = 1;
    data->d3dContext->CopySubresourceRegion(
        stagingTexture.Get(),
        D3D11CalcSubresource(0, 0, 0),
        0, 0, 0,
        backBuffer.Get(),
        D3D11CalcSubresource(0, 0, 0),
        &srcBox);

    // Map the staging texture's data to CPU-accessible memory:
    D3D11_MAPPED_SUBRESOURCE textureMemory = {0};
    result = data->d3dContext->Map(
        stagingTexture.Get(),
        D3D11CalcSubresource(0, 0, 0),
        D3D11_MAP_READ,
        0,
        &textureMemory);
    if (FAILED(result)) {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11DeviceContext1::Map [map staging texture]", result);
        return -1;
    }

    // Copy the data into the desired buffer, converting pixels to the
    // desired format at the same time:
    if (SDL_ConvertPixels(
        rect->w, rect->h,
        DXGIFormatToSDLPixelFormat(stagingTextureDesc.Format),
        textureMemory.pData,
        textureMemory.RowPitch,
        format,
        pixels,
        pitch) != 0)
    {
        // When SDL_ConvertPixels fails, it'll have already set the format.
        // Get the error message, and attach some extra data to it.
        std::string errorMessage = string(__FUNCTION__ ", Convert Pixels failed: ") + SDL_GetError();
        return SDL_SetError(errorMessage.c_str());
    }

    // Unmap the texture:
    data->d3dContext->Unmap(
        stagingTexture.Get(),
        D3D11CalcSubresource(0, 0, 0));

    // All done.  The staging texture will be cleaned up in it's container
    // ComPtr<>'s destructor.
    return 0;
}

static void
D3D11_RenderPresent(SDL_Renderer * renderer)
{
    D3D11_RenderData *data = (D3D11_RenderData *) renderer->driverdata;

#if WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP
    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    HRESULT hr = data->swapChain->Present(1, 0);
#else
    // The application may optionally specify "dirty" or "scroll"
    // rects to improve efficiency in certain scenarios.
    // This option is not available on Windows Phone 8, to note.
    DXGI_PRESENT_PARAMETERS parameters = {0};
    parameters.DirtyRectsCount = 0;
    parameters.pDirtyRects = nullptr;
    parameters.pScrollRect = nullptr;
    parameters.pScrollOffset = nullptr;
    
    // The first argument instructs DXGI to block until VSync, putting the application
    // to sleep until the next VSync. This ensures we don't waste any cycles rendering
    // frames that will never be displayed to the screen.
    HRESULT hr = data->swapChain->Present1(1, 0, &parameters);
#endif

    // Discard the contents of the render target.
    // This is a valid operation only when the existing contents will be entirely
    // overwritten. If dirty or scroll rects are used, this call should be removed.
    data->d3dContext->DiscardView(data->mainRenderTargetView.Get());

    // If the device was removed either by a disconnect or a driver upgrade, we 
    // must recreate all device resources.
    //
    // TODO, WinRT: consider throwing an exception if D3D11_RenderPresent fails, especially if there is a way to salvedge debug info from users' machines
    if (hr == DXGI_ERROR_DEVICE_REMOVED)
    {
        hr = D3D11_HandleDeviceLost(renderer);
        if (FAILED(hr)) {
            /* D3D11_HandleDeviceLost will set the SDL error */
        }
    }
    else
    {
        WIN_SetErrorFromHRESULT(__FUNCTION__ ", ID3D11DeviceContext1::DiscardView", hr);
    }
}

#endif /* SDL_VIDEO_RENDER_D3D && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
