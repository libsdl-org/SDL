struct PSInput {
    float4 v_color : COLOR0;
};

struct PSOutput {
    float4 o_color : SV_Target;
};

PSOutput main(PSInput input) {
    PSOutput output;
    output.o_color = input.v_color;
    return output;
}
