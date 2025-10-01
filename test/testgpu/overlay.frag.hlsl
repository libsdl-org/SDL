
#include "overlay.hlsli"

Texture2D<float4> t0 : register(t0, space2);
SamplerState s0 : register(s0, space2);

float4 main(VSOutput input) : SV_Target0
{
    return t0.Sample(s0, input.uv);
}
