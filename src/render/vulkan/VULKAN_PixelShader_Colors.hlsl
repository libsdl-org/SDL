
#include "VULKAN_PixelShader_Common.incl"

float4 main(PixelShaderInput input) : SV_TARGET0
{
    return GetOutputColor(1.0) * input.color;
}
