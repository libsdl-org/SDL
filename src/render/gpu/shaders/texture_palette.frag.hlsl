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

PSOutput main(PSInput input) {
    PSOutput output;
    float index = u_texture.Sample(u_sampler1, input.v_uv).r * 255;
    float4 color = u_palette.Sample(u_sampler2, float2((index + 0.5) / 256, 0.5));
    output.o_color = color * input.v_color;
    return output;
}
