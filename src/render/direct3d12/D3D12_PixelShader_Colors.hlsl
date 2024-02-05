
#include "D3D12_PixelShader_Common.incl"

#define ColorRS \
    "RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS )," \
    "RootConstants(num32BitConstants=20, b1)"

[RootSignature(ColorRS)]
float4 main(PixelShaderInput input) : SV_TARGET0
{
    return GetOutputColor(1.0) * input.color;
}
