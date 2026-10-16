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

float3 PQFromNits(float3 v)
{
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;

    float3 y = saturate(v / 10000.0);
    float3 num = c1 + c2 * pow(abs(y), m1);
    float3 den = 1.0 + c3 * pow(abs(y), m1);
    return pow(abs(num / den), m2);
}

PSOutput main(PSInput input) {
    PSOutput output;
    // Apply color scaling, which includes the SDR white point, then convert from scRGB to nits and encode as PQ
    float3 rgb = input.v_color.rgb * color_scale;
    output.o_color.rgb = PQFromNits(rgb * 80.0);
    output.o_color.a = input.v_color.a;
    return output;
}
