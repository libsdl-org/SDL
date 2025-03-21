Texture2D u_texture : register(t0, space2);
SamplerState u_sampler : register(s0, space2);

struct PSInput {
    float4 v_color : COLOR0;
    float2 v_uv : TEXCOORD0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;
    float4 color = u_texture.Sample(u_sampler, input.v_uv) * input.v_color;
    float gray = color.r * 0.299 + color.g * 0.587 + color.r * 0.114;
    output.o_color = float4(gray, gray, gray, color.a);
    return output;
}
