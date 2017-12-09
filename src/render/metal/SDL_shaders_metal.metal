#include <metal_texture>

using namespace metal;

struct SolidVertex
{
	float4 position [[position]];
	float pointSize [[point_size]];
};

vertex SolidVertex SDL_Solid_vertex(constant float2 *position [[buffer(0)]], uint vid [[vertex_id]])
{
    SolidVertex v;
	v.position = float4(position[vid].x, position[vid].y, 0.0f, 1.0f);
	v.pointSize = 0.5f;
	return v;
}
 
fragment float4 SDL_Solid_fragment(constant float4 &col [[buffer(0)]])
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
    CopyVertex v;
    v.position = float4(position[vid].x, position[vid].y, 0.0f, 1.0f);
    v.texcoord = texcoords[vid];
    return v;
}

fragment float4 SDL_Copy_fragment(CopyVertex vert [[stage_in]], constant float4 &col [[buffer(0)]], texture2d<float> tex [[texture(0)]])
{
    constexpr sampler samp; // !!! FIXME: linear sampling, etc?
    return tex.sample(samp, vert.texcoord) * col;
}

