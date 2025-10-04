
#include "D3D9_PixelShader_Palette.hlsli"

float4 main(PixelShaderInput input) : SV_TARGET
{
    return SamplePaletteLinear(input.tex) * input.color;
}
