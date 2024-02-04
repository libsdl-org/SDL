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
