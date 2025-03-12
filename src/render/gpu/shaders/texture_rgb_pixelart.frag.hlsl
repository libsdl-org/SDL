#include "texture_pixelart.frag.hlsli"

PSOutput main(PSInput input) {
    PSOutput output;
    float2 uv = GetPixelArtUV(input);
    output.o_color = float4(u_texture.SampleGrad(u_sampler, uv, ddx(input.v_uv), ddy(input.v_uv)).rgb, 1.0f) * input.v_color;
    return output;
}
