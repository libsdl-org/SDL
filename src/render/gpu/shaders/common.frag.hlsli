
float4 GetOutputColor(float4 rgba)
{
    float4 output;

    output.rgb = rgba.rgb * color_scale;
    output.a = rgba.a;

    return output;
}
