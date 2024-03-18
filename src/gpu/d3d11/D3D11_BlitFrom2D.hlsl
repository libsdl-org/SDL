struct VertextoPixel
{
    float4 pos              : SV_POSITION;
    float2 tex              : TEXCOORD0;
};

Texture2D Source : register(t0);
sampler SourceSampler : register(s0);

float4 main(VertextoPixel input) : SV_Target0
{
    return Source.Sample(SourceSampler, input.tex);
}