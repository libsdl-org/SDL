
#include "overlay.hlsli"

static const uint verts[6] = {0, 1, 2, 0, 2, 3};
static const float2 uvs[4] = {
    {0.0f, 0.0f},
    {1.0f, 0.0f},
    {1.0f, 1.0f},
    {0.0f, 1.0f}
};
static const float2 pos[4] = {
    {-1.0f, 1.0f},
    {1.0f, 1.0f},
    {1.0f, -1.0f},
    {-1.0f, -1.0f}
};

VSOutput main(uint id : SV_VertexID)
{
    VSOutput output;
    uint vert = verts[id];
    output.uv = uvs[vert];
    output.pos = float4(pos[vert], 0.0f, 1.0f);
    return output;
}
