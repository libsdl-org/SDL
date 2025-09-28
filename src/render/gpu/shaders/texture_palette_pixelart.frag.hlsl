cbuffer Context : register(b0, space3) {
    float4 texel_size;
};

Texture2D u_texture : register(t0, space2);
Texture2D u_palette : register(t1, space2);
SamplerState u_sampler1 : register(s0, space2);
SamplerState u_sampler2 : register(s1, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

float2 GetPixelArtUV(PSInput input)
{
    // box filter size in texel units
    float2 boxSize = clamp(fwidth(input.v_uv) * texel_size.zw, 1e-5, 1);

    // scale uv by texture size to get texel coordinate
    float2 tx = input.v_uv * texel_size.zw - 0.5 * boxSize;

    // compute offset for pixel-sized box filter
    float2 txOffset = smoothstep(1 - boxSize, 1, frac(tx));

    // compute bilinear sample uv coordinates
    float2 uv = (floor(tx) + 0.5 + txOffset) * texel_size.xy;

    return uv;
}


PSOutput main(PSInput input) {
    PSOutput output;
    float2 uv = GetPixelArtUV(input);
    float index = u_texture.SampleGrad(u_sampler1, uv, ddx(input.v_uv), ddy(input.v_uv)).r * 255;
    float4 color = u_palette.Sample(u_sampler2, float2((index + 0.5) / 256, 0.5));
    output.o_color = color * input.v_color;
    return output;
}
