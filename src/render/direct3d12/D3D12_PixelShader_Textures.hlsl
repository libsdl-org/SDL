Texture2D theTexture : register(t0);
SamplerState theSampler : register(s0);

#include "D3D12_PixelShader_Common.incl"

#define TextureRS \
    "RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "            DENY_HULL_SHADER_ROOT_ACCESS )," \
    "RootConstants(num32BitConstants=20, b1),"\
    "DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( Sampler(s0), visibility = SHADER_VISIBILITY_PIXEL )"

[RootSignature(TextureRS)]
float4 main(PixelShaderInput input) : SV_TARGET
{
    return GetOutputColor(theTexture.Sample(theSampler, input.tex)) * input.color;
}
