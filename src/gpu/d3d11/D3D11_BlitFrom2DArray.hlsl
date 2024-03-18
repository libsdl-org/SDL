struct VertextoPixel
{
    float4 pos              : SV_POSITION;
    float2 tex              : TEXCOORD0;
};

Texture2DArray Source : register(t0);
sampler SourceSampler : register(s0);

cbuffer ConstantBuffer : register(b0)
{
    int Layer;
}

float4 main(VertextoPixel input) : SV_Target0
{
    return Source.Sample(SourceSampler, float3(input.tex, Layer));
}