#include "VULKAN_PixelShader_Common.incl"

float4 main(PixelShaderInput input) : SV_TARGET
{
    return GetOutputColor(texture0.Sample(sampler0, input.tex)) * input.color;
}
