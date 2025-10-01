
#include "cube.hlsli"

cbuffer UBO : register(b0, space1)
{
    float4x4 ModelViewProj;
};

VSOutput main(VSInput input)
{
    VSOutput output;
    output.Color = float4(input.Color, 1.0f);
    output.Position = mul(ModelViewProj, float4(input.Position, 1.0f));
    return output;
}
