struct VertextoPixel
{
    float4 pos              : SV_POSITION;
    float2 tex              : TEXCOORD0;
};

Texture2D Source : register(t0, space2);
sampler SourceSampler : register(s0, space2);
cbuffer SourceRegionBuffer : register(b0, space3)
{
    float2 UVLeftTop;
    float2 UVDimensions;
};

float4 main(VertextoPixel input) : SV_Target0
{
    float2 texCoord = UVLeftTop + UVDimensions * input.tex;
    return Source.Sample(SourceSampler, texCoord);
}
