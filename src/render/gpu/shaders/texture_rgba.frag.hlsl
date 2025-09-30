
cbuffer Constants : register(b0, space3)
{
    float color_scale;
};

Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

#include "common.frag.hlsli"

PSOutput main(PSInput input) {
    PSOutput output;
    output.o_color = GetOutputColor(u_texture.Sample(u_sampler, input.v_uv)) * input.v_color;
    return output;
}
