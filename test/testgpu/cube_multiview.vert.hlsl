
#include "cube.hlsli"

cbuffer UBO : register(b0, space1)
{
    float4x4 ModelViewProj[2];
};

VSOutput main(VSInput input, uint viewID : SV_ViewID)
{
    VSOutput output;
    output.Color = float4(input.Color, 1.0f);
    output.Position = mul(ModelViewProj[viewID], float4(input.Position, 1.0f));
    return output;
}
