Texture2D theTexture : register(t0);
SamplerState theSampler : register(s0);

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

#define TextureRS \
    "RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "            DENY_HULL_SHADER_ROOT_ACCESS )," \
    "RootConstants(num32BitConstants=32, b0),"\
    "DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( Sampler(s0), visibility = SHADER_VISIBILITY_PIXEL )"

[RootSignature(TextureRS)]
float4 main(PixelShaderInput input) : SV_TARGET
{
    return theTexture.Sample(theSampler, input.tex) * input.color;
}