#include <metal_texture>

using namespace metal;

vertex float4 SDL_Simple_vertex(constant float2 *position [[buffer(0)]], uint vid [[vertex_id]])
{
    return float4(position[vid].x, position[vid].y, 0.0f, 1.0f);
}
 
fragment float4 SDL_Simple_fragment(constant float4 &col [[buffer(0)]])
{
    return col;
}

struct CopyVertex
{
    float4 position [[position]];
    float2 texcoord;
};

vertex CopyVertex SDL_Copy_vertex(constant float2 *position [[buffer(0)]], constant float2 *texcoords [[buffer(1)]], uint vid [[vertex_id]])
{
    CopyVertex retval;
    retval.position = float4(position[vid].x, position[vid].y, 0.0f, 1.0f);
    retval.texcoord = texcoords[vid];
    return retval;
}

fragment float4 SDL_Copy_fragment(CopyVertex vert [[stage_in]], constant float4 &col [[buffer(0)]], texture2d<float> tex [[texture(0)]])
{
    constexpr sampler samp; // !!! FIXME: linear sampling, etc?
    return tex.sample(samp, vert.texcoord) * col;
}

