
#include "VULKAN_PixelShader_Common.hlsli"

float4 main(PixelShaderInput input) : SV_TARGET
{
    return texture0.Sample(sampler0, input.tex);
}
