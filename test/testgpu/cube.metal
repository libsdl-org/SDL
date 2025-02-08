#include <metal_stdlib>
using namespace metal;

struct VSOutput
{
    float4 color [[user(locn0)]];
    float4 position [[position]];
};

#ifdef VERTEX

struct UBO
{
    float4x4 modelViewProj;
};

struct VSInput
{
    float3 position [[attribute(0)]];
    float3 color [[attribute(1)]];
};

vertex VSOutput vs_main(VSInput input [[stage_in]], constant UBO& ubo [[buffer(0)]])
{
    VSOutput output;
    output.color = float4(input.color, 1.0);
    output.position = ubo.modelViewProj * float4(input.position, 1.0);
    return output;
}

#else

fragment float4 fs_main(VSOutput input [[stage_in]])
{
    return input.color;
}

#endif