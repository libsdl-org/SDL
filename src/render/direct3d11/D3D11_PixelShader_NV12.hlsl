Texture2D theTextureY : register(t0);
Texture2D theTextureUV : register(t1);
SamplerState theSampler : register(s0);

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

cbuffer Constants : register(b0)
{
    float4 Yoffset;
    float4 Rcoeff;
    float4 Gcoeff;
    float4 Bcoeff;
};


float4 main(PixelShaderInput input) : SV_TARGET
{
    float4 Output;

    float3 yuv;
    yuv.x = theTextureY.Sample(theSampler, input.tex).r;
    yuv.yz = theTextureUV.Sample(theSampler, input.tex).rg;

    yuv += Yoffset.xyz;
    Output.r = dot(yuv, Rcoeff.xyz);
    Output.g = dot(yuv, Gcoeff.xyz);
    Output.b = dot(yuv, Bcoeff.xyz);
    Output.a = 1.0f;

    return Output * input.color;
}
