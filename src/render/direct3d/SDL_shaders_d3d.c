/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2021 Sam Lantinga <slouken@libsdl.org>

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
#include "../../SDL_internal.h"

#include "SDL_render.h"
#include "SDL_system.h"

#if SDL_VIDEO_RENDER_D3D && !SDL_RENDER_DISABLED

#include "../../core/windows/SDL_windows.h"

#include <d3d9.h>

#include "SDL_shaders_d3d.h"

/* The shaders here were compiled with:

       fxc /T ps_2_0 /Fo"<OUTPUT FILE>" "<INPUT FILE>"

   Shader object code was converted to a list of DWORDs via the following
   *nix style command (available separately from Windows + MSVC):

     hexdump -v -e '6/4 "0x%08.8x, " "\n"' <FILE>
*/


/* --- D3D9_PixelShader_ARGB.hlsl ---
* This shader implements Bicubic filtering with CatmullRom and it is based on:
* https://gist.github.com/TheRealMJP/c83b8c0f46b63f3a88a5986f4fa982b1
* 
    Texture2D theTexture : register(t0);
    SamplerState theSampler = sampler_state
    {
	    addressU = Clamp;
	    addressV = Clamp;
	    mipfilter = NONE;
	    minfilter = LINEAR;
	    magfilter = LINEAR;
    };
    float4 texSize : register(c1);

    float4 SampleTextureCatmullRom(in Texture2D<float4> tex, in SamplerState linearSampler, in float2 uv, in float2 texSize)
    {
        // We're going to sample a a 4x4 grid of texels surrounding the target UV coordinate. We'll do this by rounding
        // down the sample location to get the exact center of our "starting" texel. The starting texel will be at
        // location [1, 1] in the grid, where [0, 0] is the top left corner.
        float2 samplePos = uv * texSize;
        float2 texPos1 = floor(samplePos - 0.5f) + 0.5f;

        // Compute the fractional offset from our starting texel to our original sample location, which we'll
        // feed into the Catmull-Rom spline function to get our filter weights.
        float2 f = samplePos - texPos1;

        // Compute the Catmull-Rom weights using the fractional offset that we calculated earlier.
        // These equations are pre-expanded based on our knowledge of where the texels will be located,
        // which lets us avoid having to evaluate a piece-wise function.
        float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
        float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
        float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
        float2 w3 = f * f * (-0.5f + 0.5f * f);

        // Work out weighting factors and sampling offsets that will let us use bilinear filtering to
        // simultaneously evaluate the middle 2 samples from the 4x4 grid.
        float2 w12 = w1 + w2;
        float2 offset12 = w2 / (w1 + w2);

        // Compute the final UV coordinates we'll use for sampling the texture
        float2 texPos0 = texPos1 - 1;
        float2 texPos3 = texPos1 + 2;
        float2 texPos12 = texPos1 + offset12;

        texPos0 /= texSize;
        texPos3 /= texSize;
        texPos12 /= texSize;

        float4 result = 0.0f;
        result += tex.Sample(linearSampler, float2(texPos0.x, texPos0.y)) * w0.x * w0.y;
        result += tex.Sample(linearSampler, float2(texPos12.x, texPos0.y)) * w12.x * w0.y;
        result += tex.Sample(linearSampler, float2(texPos3.x, texPos0.y)) * w3.x * w0.y;

        result += tex.Sample(linearSampler, float2(texPos0.x, texPos12.y)) * w0.x * w12.y;
        result += tex.Sample(linearSampler, float2(texPos12.x, texPos12.y)) * w12.x * w12.y;
        result += tex.Sample(linearSampler, float2(texPos3.x, texPos12.y)) * w3.x * w12.y;

        result += tex.Sample(linearSampler, float2(texPos0.x, texPos3.y)) * w0.x * w3.y;
        result += tex.Sample(linearSampler, float2(texPos12.x, texPos3.y)) * w12.x * w3.y;
        result += tex.Sample(linearSampler, float2(texPos3.x, texPos3.y)) * w3.x * w3.y;

        return result;
    }

    struct PixelShaderInput
    {
	    float4 pos : SV_POSITION;
	    float2 tex : TEXCOORD0;
	    float4 color : COLOR0;
    };

    float4 main(PixelShaderInput input) : SV_TARGET
    {
	    float width = texSize[0];
	    float height = texSize[1];
	    //theTexture.GetDimensions(width,height);
	    float2 size = float2(width,height);
	    float4 res = SampleTextureCatmullRom(theTexture, theSampler, input.tex, size);
	    return res * input.color;
	    //return theTexture.Sample(theSampler, input.tex) * input.color;
    }

*/
static const DWORD D3D9_PixelShader_ARGB_Bicubic_CatmullRom[] = {
    0xffff0200, 0x002efffe, 0x42415443, 0x0000001c, 0x0000008b, 0xffff0200,
    0x00000002, 0x0000001c, 0x00000100, 0x00000084, 0x00000044, 0x00010002,
    0x00060001, 0x0000004c, 0x00000000, 0x0000005c, 0x00000003, 0x00000001,
    0x00000074, 0x00000000, 0x53786574, 0x00657a69, 0x00030001, 0x00040001,
    0x00000001, 0x00000000, 0x53656874, 0x6c706d61, 0x742b7265, 0x65546568,
    0x72757478, 0xabab0065, 0x00070004, 0x00040001, 0x00000001, 0x00000000,
    0x325f7370, 0x4d00305f, 0x6f726369, 0x74666f73, 0x29522820, 0x534c4820,
    0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970, 0x30312072, 0xab00312e,
    0x05000051, 0xa00f0000, 0xbf000000, 0x3f000000, 0x3f800000, 0x40200000,
    0x05000051, 0xa00f0002, 0x3fc00000, 0xc0200000, 0x40000000, 0x00000000,
    0x0200001f, 0x80000000, 0xb0030000, 0x0200001f, 0x80000000, 0x900f0000,
    0x0200001f, 0x90000000, 0xa00f0800, 0x02000001, 0x80080000, 0xa0000000,
    0x04000004, 0x80030000, 0xb0e40000, 0xa0e40001, 0x80ff0000, 0x02000013,
    0x800c0000, 0x801b0000, 0x03000002, 0x80030000, 0x811b0000, 0x80e40000,
    0x03000002, 0x800c0000, 0x801b0000, 0xa0550000, 0x04000004, 0x80030001,
    0xb0e40000, 0xa0e40001, 0x811b0000, 0x04000004, 0x800c0001, 0x801b0001,
    0xa1000002, 0xa0aa0002, 0x04000004, 0x800c0001, 0x801b0001, 0x80e40001,
    0xa0550000, 0x03000005, 0x80030002, 0x801b0001, 0x80e40001, 0x04000004,
    0x800c0002, 0x801b0001, 0xa0000002, 0xa0550002, 0x03000005, 0x80030003,
    0x80e40001, 0x80e40001, 0x04000004, 0x800c0002, 0x801b0003, 0x80e40002,
    0xa0aa0000, 0x04000004, 0x800c0001, 0x801b0001, 0x80e40001, 0x80e40002,
    0x02000006, 0x80010004, 0x80ff0001, 0x02000006, 0x80020004, 0x80aa0001,
    0x04000004, 0x800c0000, 0x801b0002, 0x801b0004, 0x80e40000, 0x02000006,
    0x80010002, 0xa0000001, 0x02000006, 0x80020002, 0xa0550001, 0x03000005,
    0x80030004, 0x801b0000, 0x80e40002, 0x02000001, 0x80010005, 0x80000004,
    0x03000002, 0x800c0000, 0x801b0000, 0xa0000000, 0x03000002, 0x80030000,
    0x80e40000, 0xa0ff0000, 0x03000005, 0x80030000, 0x80e40002, 0x80e40000,
    0x03000005, 0x80030002, 0x80e40002, 0x801b0000, 0x02000001, 0x80020005,
    0x80550002, 0x02000001, 0x80010006, 0x80000005, 0x02000001, 0x80020007,
    0x80550005, 0x02000001, 0x80010008, 0x80000002, 0x02000001, 0x80010007,
    0x80000000, 0x02000001, 0x80010009, 0x80000007, 0x02000001, 0x80020008,
    0x80550004, 0x02000001, 0x8001000a, 0x80000008, 0x02000001, 0x80020009,
    0x80550008, 0x02000001, 0x8002000a, 0x80550000, 0x02000001, 0x80020006,
    0x8055000a, 0x03000042, 0x800f0005, 0x80e40005, 0xa0e40800, 0x03000042,
    0x800f0002, 0x80e40002, 0xa0e40800, 0x03000042, 0x800f0007, 0x80e40007,
    0xa0e40800, 0x03000042, 0x800f0004, 0x80e40004, 0xa0e40800, 0x03000042,
    0x800f0008, 0x80e40008, 0xa0e40800, 0x03000042, 0x800f0009, 0x80e40009,
    0xa0e40800, 0x03000042, 0x800f0000, 0x80e40000, 0xa0e40800, 0x03000042,
    0x800f0006, 0x80e40006, 0xa0e40800, 0x03000005, 0x800f0005, 0x80ff0001,
    0x80e40005, 0x04000004, 0x800c0003, 0x801b0001, 0xa1550000, 0xa0aa0000,
    0x04000004, 0x800c0003, 0x801b0001, 0x80e40003, 0xa0000000, 0x03000005,
    0x800c0003, 0x801b0001, 0x80e40003, 0x04000004, 0x80030001, 0x80e40001,
    0xa0550000, 0xa0000000, 0x03000005, 0x80030001, 0x80e40001, 0x80e40003,
    0x03000005, 0x800f0005, 0x80aa0003, 0x80e40005, 0x03000005, 0x800f0002,
    0x80e40002, 0x80ff0003, 0x04000004, 0x800f0002, 0x80e40002, 0x80aa0003,
    0x80e40005, 0x03000005, 0x800f0005, 0x80000001, 0x80e40007, 0x04000004,
    0x800f0002, 0x80e40005, 0x80aa0003, 0x80e40002, 0x03000005, 0x800f0004,
    0x80ff0001, 0x80e40004, 0x03000005, 0x800f0005, 0x80000001, 0x80e40009,
    0x03000005, 0x800f0007, 0x80ff0003, 0x80e40008, 0x04000004, 0x800f0002,
    0x80e40007, 0x80aa0001, 0x80e40002, 0x04000004, 0x800f0002, 0x80e40004,
    0x80aa0001, 0x80e40002, 0x04000004, 0x800f0002, 0x80e40005, 0x80aa0001,
    0x80e40002, 0x03000005, 0x800f0000, 0x80e40000, 0x80000001, 0x03000042,
    0x800f0004, 0x80e4000a, 0xa0e40800, 0x03000005, 0x800f0005, 0x80ff0001,
    0x80e40006, 0x03000005, 0x800f0003, 0x80ff0003, 0x80e40004, 0x04000004,
    0x800f0002, 0x80e40003, 0x80550001, 0x80e40002, 0x04000004, 0x800f0002,
    0x80e40005, 0x80550001, 0x80e40002, 0x04000004, 0x800f0000, 0x80e40000,
    0x80550001, 0x80e40002, 0x03000005, 0x800f0000, 0x80e40000, 0x90e40000,
    0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff,
};

/* --- D3D9_PixelShader_ARGB_Bicubic_Bspline.hlsl ---
* This shader implements Bicubic filtering with Bspline and it is based on:
https://groups.google.com/g/comp.graphics.api.opengl/c/kqrujgJfTxo*
    Texture2D theTexture : register(t0);
    SamplerState theSampler = sampler_state
    {
	    addressU = Clamp;
	    addressV = Clamp;
	    mipfilter = NONE;
	    minfilter = LINEAR;
	    magfilter = LINEAR;
    };
    float4 texSize : register(c1);

    float4 cubic(float v)
    {
        float4 n = float4(1.0, 2.0, 3.0, 4.0) - v;
        float4 s = n * n * n;
        float x = s.x;
        float y = s.y - 4.0 * s.x;
        float z = s.z - 4.0 * s.y + 6.0 * s.x;
        float w = 6.0 - x - y - z;
        return float4(x, y, z, w);
    }

    float4 BicubicBSpline(in Texture2D<float4> tex, in SamplerState linearSampler, in float2 uv, in float2 texscale )
    {
	    float2 texcoord = uv * texscale;
	    float2 fxy = frac(texcoord);
	    texcoord -= fxy;
	
	    float4 xcubic = cubic(fxy.x);
	    float4 ycubic = cubic(fxy.y);

	    float4 c = float4(texcoord.x - 0.5, texcoord.x + 1.5, texcoord.y - 0.5, texcoord.y + 1.5);
	    float4 s = float4(xcubic.x + xcubic.y, xcubic.z + xcubic.w, ycubic.x + ycubic.y, ycubic.z + ycubic.w);
	    float4 offset = c + float4(xcubic.y, xcubic.w, ycubic.y, ycubic.w) / s;

	    float4 sample0 = tex.Sample(linearSampler,float2(offset.x, offset.z) * (1/texscale)); 
	    float4 sample1 = tex.Sample(linearSampler,float2(offset.y, offset.z) * (1/texscale)); 
	    float4 sample2 = tex.Sample(linearSampler,float2(offset.x, offset.w) * (1/texscale)); 
	    float4 sample3 = tex.Sample(linearSampler,float2(offset.y, offset.w) * (1/texscale)); 

	    float sx = s.x / (s.x + s.y);
	    float sy = s.z / (s.z + s.w);

	    return lerp( lerp(sample3, sample2, sx), lerp(sample1, sample0, sx), sy);
    }

    struct PixelShaderInput
    {
	    float4 pos : SV_POSITION;
	    float2 tex : TEXCOORD0;
	    float4 color : COLOR0;
    };

    float4 main(PixelShaderInput input) : SV_TARGET
    {
	    float width = texSize[0];
	    float height = texSize[1];
	    //theTexture.GetDimensions(width,height);
	    float2 size = float2(width,height);
	    float4 res = BicubicBSpline(theTexture, theSampler, input.tex, size);
	    return res * input.color;
	    //return theTexture.Sample(theSampler, input.tex) * input.color;
    }


*/
static const DWORD D3D9_PixelShader_ARGB_Bicubic_Bspline[] = {
    0xffff0200, 0x002efffe, 0x42415443, 0x0000001c, 0x0000008b, 0xffff0200,
    0x00000002, 0x0000001c, 0x00000100, 0x00000084, 0x00000044, 0x00010002,
    0x00060001, 0x0000004c, 0x00000000, 0x0000005c, 0x00000003, 0x00000001,
    0x00000074, 0x00000000, 0x53786574, 0x00657a69, 0x00030001, 0x00040001,
    0x00000001, 0x00000000, 0x53656874, 0x6c706d61, 0x742b7265, 0x65546568,
    0x72757478, 0xabab0065, 0x00070004, 0x00040001, 0x00000001, 0x00000000,
    0x325f7370, 0x4d00305f, 0x6f726369, 0x74666f73, 0x29522820, 0x534c4820,
    0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970, 0x30312072, 0xab00312e,
    0x05000051, 0xa00f0000, 0x3f800000, 0x40000000, 0x40400000, 0x40800000,
    0x05000051, 0xa00f0002, 0xbf000000, 0x3fc00000, 0xbf000000, 0x3fc00000,
    0x05000051, 0xa00f0003, 0x40c00000, 0x00000000, 0x00000000, 0x00000000,
    0x0200001f, 0x80000000, 0xb0030000, 0x0200001f, 0x80000000, 0x900f0000,
    0x0200001f, 0x90000000, 0xa00f0800, 0x03000005, 0x80020000, 0xb0000000,
    0xa0000001, 0x03000005, 0x80080000, 0xb0550000, 0xa0550001, 0x02000013,
    0x800c0001, 0x80ff0000, 0x02000013, 0x80030001, 0x80550000, 0x04000004,
    0x800c0000, 0xb0550000, 0xa0550001, 0x81e40001, 0x04000004, 0x80030000,
    0xb0000000, 0xa0000001, 0x81e40001, 0x03000002, 0x800f0000, 0x80e40000,
    0xa0e40002, 0x03000002, 0x80070001, 0x81550001, 0xa0e40000, 0x03000002,
    0x80070002, 0x81ff0001, 0xa0e40000, 0x03000005, 0x80070003, 0x80e40001,
    0x80e40001, 0x03000005, 0x800e0001, 0x801b0001, 0x801b0003, 0x04000004,
    0x80080002, 0x80aa0001, 0xa1ff0000, 0x80550001, 0x04000004, 0x80080002,
    0x80ff0001, 0xa0000003, 0x80ff0002, 0x04000004, 0x80010004, 0x80ff0001,
    0xa1ff0000, 0x80aa0001, 0x04000004, 0x80020001, 0x80000003, 0x81000001,
    0xa0000003, 0x04000004, 0x80010001, 0x80000003, 0x80000001, 0x80000004,
    0x03000002, 0x80020001, 0x81000004, 0x80550001, 0x03000002, 0x80020004,
    0x81ff0002, 0x80550001, 0x03000005, 0x80070003, 0x80e40002, 0x80e40002,
    0x03000005, 0x800e0002, 0x801b0002, 0x801b0003, 0x04000004, 0x80040001,
    0x80aa0002, 0xa1ff0000, 0x80550002, 0x04000004, 0x80040001, 0x80ff0002,
    0xa0000003, 0x80aa0001, 0x04000004, 0x80040004, 0x80ff0002, 0xa1ff0000,
    0x80aa0002, 0x04000004, 0x80080001, 0x80000003, 0x81000002, 0xa0000003,
    0x04000004, 0x80010002, 0x80000003, 0x80000002, 0x80aa0004, 0x03000002,
    0x80080001, 0x81aa0004, 0x80ff0001, 0x03000002, 0x80080004, 0x81aa0001,
    0x80ff0001, 0x02000006, 0x80020003, 0x80550001, 0x03000002, 0x80020001,
    0x80550001, 0x80000001, 0x02000006, 0x80020001, 0x80550001, 0x03000005,
    0x80020001, 0x80550001, 0x80000001, 0x02000006, 0x80010003, 0x80000001,
    0x02000006, 0x80080003, 0x80ff0001, 0x03000002, 0x80010001, 0x80ff0001,
    0x80000002, 0x02000006, 0x80010001, 0x80000001, 0x03000005, 0x80010001,
    0x80000001, 0x80000002, 0x02000006, 0x80040003, 0x80000002, 0x04000004,
    0x800f0000, 0x80e40004, 0x80e40003, 0x80e40000, 0x02000006, 0x80010002,
    0xa0000001, 0x02000006, 0x80020002, 0xa0550001, 0x03000005, 0x80010003,
    0x80000000, 0x80000002, 0x03000005, 0x80020003, 0x80ff0000, 0x80550002,
    0x03000005, 0x80010004, 0x80550000, 0x80000002, 0x03000005, 0x80020004,
    0x80ff0000, 0x80550002, 0x03000005, 0x80010005, 0x80000000, 0x80000002,
    0x03000005, 0x80020005, 0x80aa0000, 0x80550002, 0x03000005, 0x80030000,
    0x80c90000, 0x80e40002, 0x03000042, 0x800f0002, 0x80e40003, 0xa0e40800,
    0x03000042, 0x800f0003, 0x80e40004, 0xa0e40800, 0x03000042, 0x800f0000,
    0x80e40000, 0xa0e40800, 0x03000042, 0x800f0004, 0x80e40005, 0xa0e40800,
    0x04000012, 0x800f0005, 0x80550001, 0x80e40002, 0x80e40003, 0x04000012,
    0x800f0002, 0x80550001, 0x80e40004, 0x80e40000, 0x04000012, 0x800f0000,
    0x80000001, 0x80e40002, 0x80e40005, 0x03000005, 0x800f0000, 0x80e40000,
    0x90e40000, 0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff,
};

/* --- D3D9_PixelShader_YUV_JPEG.hlsl ---
    Texture2D theTextureY : register(t0);
    Texture2D theTextureU : register(t1);
    Texture2D theTextureV : register(t2);
    SamplerState theSampler = sampler_state
    {
        addressU = Clamp;
        addressV = Clamp;
        mipfilter = NONE;
        minfilter = LINEAR;
        magfilter = LINEAR;
    };

    struct PixelShaderInput
    {
        float4 pos : SV_POSITION;
        float2 tex : TEXCOORD0;
        float4 color : COLOR0;
    };

    float4 main(PixelShaderInput input) : SV_TARGET
    {
        const float3 offset = {0.0, -0.501960814, -0.501960814};
        const float3 Rcoeff = {1.0000,  0.0000,  1.4020};
        const float3 Gcoeff = {1.0000, -0.3441, -0.7141};
        const float3 Bcoeff = {1.0000,  1.7720,  0.0000};

        float4 Output;

        float3 yuv;
        yuv.x = theTextureY.Sample(theSampler, input.tex).r;
        yuv.y = theTextureU.Sample(theSampler, input.tex).r;
        yuv.z = theTextureV.Sample(theSampler, input.tex).r;

        yuv += offset;
        Output.r = dot(yuv, Rcoeff);
        Output.g = dot(yuv, Gcoeff);
        Output.b = dot(yuv, Bcoeff);
        Output.a = 1.0f;

        return Output * input.color;
    }
*/
static const DWORD D3D9_PixelShader_YUV_JPEG[] = {
    0xffff0200, 0x0044fffe, 0x42415443, 0x0000001c, 0x000000d7, 0xffff0200,
    0x00000003, 0x0000001c, 0x00000100, 0x000000d0, 0x00000058, 0x00010003,
    0x00000001, 0x00000070, 0x00000000, 0x00000080, 0x00020003, 0x00000001,
    0x00000098, 0x00000000, 0x000000a8, 0x00000003, 0x00000001, 0x000000c0,
    0x00000000, 0x53656874, 0x6c706d61, 0x742b7265, 0x65546568, 0x72757478,
    0xab005565, 0x00070004, 0x00040001, 0x00000001, 0x00000000, 0x53656874,
    0x6c706d61, 0x742b7265, 0x65546568, 0x72757478, 0xab005665, 0x00070004,
    0x00040001, 0x00000001, 0x00000000, 0x53656874, 0x6c706d61, 0x742b7265,
    0x65546568, 0x72757478, 0xab005965, 0x00070004, 0x00040001, 0x00000001,
    0x00000000, 0x325f7370, 0x4d00305f, 0x6f726369, 0x74666f73, 0x29522820,
    0x534c4820, 0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970, 0x2e362072,
    0x36392e33, 0x312e3030, 0x34383336, 0xababab00, 0x05000051, 0xa00f0000,
    0x00000000, 0xbf008081, 0xbf008081, 0x3f800000, 0x05000051, 0xa00f0001,
    0x3f800000, 0x00000000, 0x3fb374bc, 0x00000000, 0x05000051, 0xa00f0002,
    0x3f800000, 0xbeb02de0, 0xbf36cf42, 0x00000000, 0x05000051, 0xa00f0003,
    0x3f800000, 0x3fe2d0e5, 0x00000000, 0x00000000, 0x0200001f, 0x80000000,
    0xb0030000, 0x0200001f, 0x80000000, 0x900f0000, 0x0200001f, 0x90000000,
    0xa00f0800, 0x0200001f, 0x90000000, 0xa00f0801, 0x0200001f, 0x90000000,
    0xa00f0802, 0x03000042, 0x800f0000, 0xb0e40000, 0xa0e40800, 0x03000042,
    0x800f0001, 0xb0e40000, 0xa0e40801, 0x03000042, 0x800f0002, 0xb0e40000,
    0xa0e40802, 0x02000001, 0x80020000, 0x80000001, 0x02000001, 0x80040000,
    0x80000002, 0x03000002, 0x80070000, 0x80e40000, 0xa0e40000, 0x03000008,
    0x80010001, 0x80e40000, 0xa0e40001, 0x03000008, 0x80020001, 0x80e40000,
    0xa0e40002, 0x0400005a, 0x80040001, 0x80e40000, 0xa0e40003, 0xa0aa0003,
    0x02000001, 0x80080001, 0xa0ff0000, 0x03000005, 0x800f0000, 0x80e40001,
    0x90e40000, 0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff
};

/* --- D3D9_PixelShader_YUV_BT601.hlsl ---
    Texture2D theTextureY : register(t0);
    Texture2D theTextureU : register(t1);
    Texture2D theTextureV : register(t2);
    SamplerState theSampler = sampler_state
    {
        addressU = Clamp;
        addressV = Clamp;
        mipfilter = NONE;
        minfilter = LINEAR;
        magfilter = LINEAR;
    };

    struct PixelShaderInput
    {
        float4 pos : SV_POSITION;
        float2 tex : TEXCOORD0;
        float4 color : COLOR0;
    };

    float4 main(PixelShaderInput input) : SV_TARGET
    {
        const float3 offset = {-0.0627451017, -0.501960814, -0.501960814};
        const float3 Rcoeff = {1.1644,  0.0000,  1.5960};
        const float3 Gcoeff = {1.1644, -0.3918, -0.8130};
        const float3 Bcoeff = {1.1644,  2.0172,  0.0000};

        float4 Output;

        float3 yuv;
        yuv.x = theTextureY.Sample(theSampler, input.tex).r;
        yuv.y = theTextureU.Sample(theSampler, input.tex).r;
        yuv.z = theTextureV.Sample(theSampler, input.tex).r;

        yuv += offset;
        Output.r = dot(yuv, Rcoeff);
        Output.g = dot(yuv, Gcoeff);
        Output.b = dot(yuv, Bcoeff);
        Output.a = 1.0f;

        return Output * input.color;
    }
*/
static const DWORD D3D9_PixelShader_YUV_BT601[] = {
    0xffff0200, 0x0044fffe, 0x42415443, 0x0000001c, 0x000000d7, 0xffff0200,
    0x00000003, 0x0000001c, 0x00000100, 0x000000d0, 0x00000058, 0x00010003,
    0x00000001, 0x00000070, 0x00000000, 0x00000080, 0x00020003, 0x00000001,
    0x00000098, 0x00000000, 0x000000a8, 0x00000003, 0x00000001, 0x000000c0,
    0x00000000, 0x53656874, 0x6c706d61, 0x742b7265, 0x65546568, 0x72757478,
    0xab005565, 0x00070004, 0x00040001, 0x00000001, 0x00000000, 0x53656874,
    0x6c706d61, 0x742b7265, 0x65546568, 0x72757478, 0xab005665, 0x00070004,
    0x00040001, 0x00000001, 0x00000000, 0x53656874, 0x6c706d61, 0x742b7265,
    0x65546568, 0x72757478, 0xab005965, 0x00070004, 0x00040001, 0x00000001,
    0x00000000, 0x325f7370, 0x4d00305f, 0x6f726369, 0x74666f73, 0x29522820,
    0x534c4820, 0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970, 0x2e362072,
    0x36392e33, 0x312e3030, 0x34383336, 0xababab00, 0x05000051, 0xa00f0000,
    0xbd808081, 0xbf008081, 0xbf008081, 0x3f800000, 0x05000051, 0xa00f0001,
    0x3f950b0f, 0x00000000, 0x3fcc49ba, 0x00000000, 0x05000051, 0xa00f0002,
    0x3f950b0f, 0xbec89a02, 0xbf5020c5, 0x00000000, 0x05000051, 0xa00f0003,
    0x3f950b0f, 0x400119ce, 0x00000000, 0x00000000, 0x0200001f, 0x80000000,
    0xb0030000, 0x0200001f, 0x80000000, 0x900f0000, 0x0200001f, 0x90000000,
    0xa00f0800, 0x0200001f, 0x90000000, 0xa00f0801, 0x0200001f, 0x90000000,
    0xa00f0802, 0x03000042, 0x800f0000, 0xb0e40000, 0xa0e40800, 0x03000042,
    0x800f0001, 0xb0e40000, 0xa0e40801, 0x03000042, 0x800f0002, 0xb0e40000,
    0xa0e40802, 0x02000001, 0x80020000, 0x80000001, 0x02000001, 0x80040000,
    0x80000002, 0x03000002, 0x80070000, 0x80e40000, 0xa0e40000, 0x03000008,
    0x80010001, 0x80e40000, 0xa0e40001, 0x03000008, 0x80020001, 0x80e40000,
    0xa0e40002, 0x0400005a, 0x80040001, 0x80e40000, 0xa0e40003, 0xa0aa0003,
    0x02000001, 0x80080001, 0xa0ff0000, 0x03000005, 0x800f0000, 0x80e40001,
    0x90e40000, 0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff
};

/* --- D3D9_PixelShader_YUV_BT709.hlsl ---
    Texture2D theTextureY : register(t0);
    Texture2D theTextureU : register(t1);
    Texture2D theTextureV : register(t2);
    SamplerState theSampler = sampler_state
    {
        addressU = Clamp;
        addressV = Clamp;
        mipfilter = NONE;
        minfilter = LINEAR;
        magfilter = LINEAR;
    };

    struct PixelShaderInput
    {
        float4 pos : SV_POSITION;
        float2 tex : TEXCOORD0;
        float4 color : COLOR0;
    };

    float4 main(PixelShaderInput input) : SV_TARGET
    {
        const float3 offset = {-0.0627451017, -0.501960814, -0.501960814};
        const float3 Rcoeff = {1.1644,  0.0000,  1.7927};
        const float3 Gcoeff = {1.1644, -0.2132, -0.5329};
        const float3 Bcoeff = {1.1644,  2.1124,  0.0000};

        float4 Output;

        float3 yuv;
        yuv.x = theTextureY.Sample(theSampler, input.tex).r;
        yuv.y = theTextureU.Sample(theSampler, input.tex).r;
        yuv.z = theTextureV.Sample(theSampler, input.tex).r;

        yuv += offset;
        Output.r = dot(yuv, Rcoeff);
        Output.g = dot(yuv, Gcoeff);
        Output.b = dot(yuv, Bcoeff);
        Output.a = 1.0f;

        return Output * input.color;
    }
*/
static const DWORD D3D9_PixelShader_YUV_BT709[] = {
    0xffff0200, 0x0044fffe, 0x42415443, 0x0000001c, 0x000000d7, 0xffff0200,
    0x00000003, 0x0000001c, 0x00000100, 0x000000d0, 0x00000058, 0x00010003,
    0x00000001, 0x00000070, 0x00000000, 0x00000080, 0x00020003, 0x00000001,
    0x00000098, 0x00000000, 0x000000a8, 0x00000003, 0x00000001, 0x000000c0,
    0x00000000, 0x53656874, 0x6c706d61, 0x742b7265, 0x65546568, 0x72757478,
    0xab005565, 0x00070004, 0x00040001, 0x00000001, 0x00000000, 0x53656874,
    0x6c706d61, 0x742b7265, 0x65546568, 0x72757478, 0xab005665, 0x00070004,
    0x00040001, 0x00000001, 0x00000000, 0x53656874, 0x6c706d61, 0x742b7265,
    0x65546568, 0x72757478, 0xab005965, 0x00070004, 0x00040001, 0x00000001,
    0x00000000, 0x325f7370, 0x4d00305f, 0x6f726369, 0x74666f73, 0x29522820,
    0x534c4820, 0x6853204c, 0x72656461, 0x6d6f4320, 0x656c6970, 0x2e362072,
    0x36392e33, 0x312e3030, 0x34383336, 0xababab00, 0x05000051, 0xa00f0000,
    0xbd808081, 0xbf008081, 0xbf008081, 0x3f800000, 0x05000051, 0xa00f0001,
    0x3f950b0f, 0x00000000, 0x3fe57732, 0x00000000, 0x05000051, 0xa00f0002,
    0x3f950b0f, 0xbe5a511a, 0xbf086c22, 0x00000000, 0x05000051, 0xa00f0003,
    0x3f950b0f, 0x40073190, 0x00000000, 0x00000000, 0x0200001f, 0x80000000,
    0xb0030000, 0x0200001f, 0x80000000, 0x900f0000, 0x0200001f, 0x90000000,
    0xa00f0800, 0x0200001f, 0x90000000, 0xa00f0801, 0x0200001f, 0x90000000,
    0xa00f0802, 0x03000042, 0x800f0000, 0xb0e40000, 0xa0e40800, 0x03000042,
    0x800f0001, 0xb0e40000, 0xa0e40801, 0x03000042, 0x800f0002, 0xb0e40000,
    0xa0e40802, 0x02000001, 0x80020000, 0x80000001, 0x02000001, 0x80040000,
    0x80000002, 0x03000002, 0x80070000, 0x80e40000, 0xa0e40000, 0x03000008,
    0x80010001, 0x80e40000, 0xa0e40001, 0x03000008, 0x80020001, 0x80e40000,
    0xa0e40002, 0x0400005a, 0x80040001, 0x80e40000, 0xa0e40003, 0xa0aa0003,
    0x02000001, 0x80080001, 0xa0ff0000, 0x03000005, 0x800f0000, 0x80e40001,
    0x90e40000, 0x02000001, 0x800f0800, 0x80e40000, 0x0000ffff
};


static const DWORD *D3D9_shaders[] = {
    D3D9_PixelShader_YUV_JPEG,
    D3D9_PixelShader_YUV_BT601,
    D3D9_PixelShader_YUV_BT709,
    D3D9_PixelShader_ARGB_Bicubic_CatmullRom,
    D3D9_PixelShader_ARGB_Bicubic_Bspline,
};

HRESULT D3D9_CreatePixelShader(IDirect3DDevice9 *d3dDevice, D3D9_Shader shader, IDirect3DPixelShader9 **pixelShader)
{
    return IDirect3DDevice9_CreatePixelShader(d3dDevice, D3D9_shaders[shader], pixelShader);
}

#endif /* SDL_VIDEO_RENDER_D3D && !SDL_RENDER_DISABLED */

/* vi: set ts=4 sw=4 expandtab: */
