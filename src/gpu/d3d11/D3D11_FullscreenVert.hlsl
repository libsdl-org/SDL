struct VertextoPixel
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
};

VertextoPixel main(uint vI : SV_VERTEXID)
{
    float2 inTex = float2((vI << 1) & 2, vI & 2);
    VertextoPixel Out = (VertextoPixel)0;
    Out.pos = float4(inTex * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    Out.tex = inTex;
    return Out;
}