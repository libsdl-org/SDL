#include "VULKAN_PixelShader_Common.incl"

float4 main(PixelShaderInput input) : SV_TARGET
{
	return AdvancedPixelShader(input);
}
