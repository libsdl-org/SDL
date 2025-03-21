cbuffer Context : register(b0, space3) {
    float distance_field_range;
    float2 texture_size;
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

float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}

float screenPxRange(float2 texCoord) {
    float2 unitRange = float2(distance_field_range, distance_field_range)/texture_size;
    float2 screenTexSize = float2(1.0, 1.0)/fwidth(texCoord);
    return max(0.5*dot(unitRange, screenTexSize), 1.0);
}

PSOutput main(PSInput input) {
    PSOutput output;
    float3 msd = u_texture.Sample(u_sampler, input.v_uv).rgb;
    float sd = median(msd.r, msd.g, msd.b);
    float screenPxDistance = screenPxRange(input.v_uv)*(sd - 0.5);
    float opacity = clamp(screenPxDistance + 0.5, 0.0, 1.0);
    output.o_color.rgb = input.v_color.rgb;
    output.o_color.a = (input.v_color.a * opacity);
    return output;
}
