void main(
    float3 position : POSITION,
    float2 texcoord : TEXCOORD0,

    uniform float4x4 modelViewProj,

    out float4 oPosition : POSITION,
    out float2 oTexcoord : TEXCOORD0
)
{
    // oPosition = mul(modelViewProj, float4(position, 1.0));
    oPosition = mul(float4(position, 1.0), modelViewProj);   // <-- v * M instead of M * v
    oTexcoord = texcoord;
}
