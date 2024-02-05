Texture2D theTextureY : register(t0);
Texture2D theTextureUV : register(t1);
SamplerState theSampler : register(s0);

#include "D3D11_PixelShader_Common.incl"

float4 main(PixelShaderInput input) : SV_TARGET
{
    float3 yuv;
    yuv.x = theTextureY.Sample(theSampler, input.tex).r;
    yuv.yz = theTextureUV.Sample(theSampler, input.tex).gr;

    float3 rgb;
    yuv += Yoffset.xyz;
    rgb.r = dot(yuv, Rcoeff.xyz);
    rgb.g = dot(yuv, Gcoeff.xyz);
    rgb.b = dot(yuv, Bcoeff.xyz);

    return GetOutputColorFromSRGB(rgb) * input.color;
}
