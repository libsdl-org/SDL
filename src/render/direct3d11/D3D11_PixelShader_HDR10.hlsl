Texture2D theTextureY : register(t0);
Texture2D theTextureUV : register(t1);
SamplerState theSampler : register(s0);

#include "D3D11_PixelShader_Common.incl"

float3 PQtoNits(float3 v)
{
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float oo_m1 = 1.0 / 0.1593017578125;
    const float oo_m2 = 1.0 / 78.84375;

    float3 num = max(pow(abs(v), oo_m2) - c1, 0.0);
    float3 den = c2 - c3 * pow(abs(v), oo_m2);
    return 10000.0 * pow(abs(num / den), oo_m1);
}

float4 main(PixelShaderInput input) : SV_TARGET
{
    const float3x3 mat2020to709 = {
        1.660496, -0.587656, -0.072840,
        -0.124547, 1.132895, -0.008348,
        -0.018154, -0.100597, 1.118751
    };

    float3 yuv;
    yuv.x = theTextureY.Sample(theSampler, input.tex).r;
    yuv.yz = theTextureUV.Sample(theSampler, input.tex).rg;

    float3 rgb;
    yuv += Yoffset.xyz;
    rgb.r = dot(yuv, Rcoeff.xyz);
    rgb.g = dot(yuv, Gcoeff.xyz);
    rgb.b = dot(yuv, Bcoeff.xyz);

    rgb = PQtoNits(rgb);

    rgb = mul(mat2020to709, rgb);

    rgb = scRGBfromNits(rgb);

    return GetOutputColorFromSCRGB(rgb) * input.color;
}
