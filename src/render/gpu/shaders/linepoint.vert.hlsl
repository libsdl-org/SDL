cbuffer Context : register(b0, space1) {
    float4x4 mvp;  // Model-View-Projection matrix
    float4 color;  // Color
};

struct VSInput {
    float2 a_position : POSITION;
};

struct VSOutput {
    float4 v_color : COLOR;
    float4 gl_Position : SV_POSITION;
    [[vk::builtin("PointSize")]]
    float gl_PointSize : PSIZE;
};

VSOutput main(VSInput input) {
    VSOutput output;
    output.v_color = color;
    output.gl_Position = mul(mvp, float4(input.a_position, 0.0, 1.0));
    output.gl_PointSize = 1.0f;
    return output;
}
