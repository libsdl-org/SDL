
struct VSInput
{
    float3 Position : TEXCOORD0;
    float3 Color : TEXCOORD1;
};

struct VSOutput
{
    float4 Color : TEXCOORD0;
    float4 Position : SV_Position;
};

float4 main(VSOutput input) : SV_Target0
{
    return input.Color;
}
