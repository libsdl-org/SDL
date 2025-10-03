
cbuffer Constants : register(b0, space3)
{
    float color_scale;
};

struct PSInput {
    float4 v_color : COLOR0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

#include "common.frag.hlsli"

PSOutput main(PSInput input) {
    PSOutput output;
    output.o_color = GetOutputColor(1.0) * input.v_color;
    return output;
}
