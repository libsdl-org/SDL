void main(
    float3 position : POSITION,
    float4 color    : COLOR0,

    uniform float4x4 modelViewProj,

    out float4 oPosition : POSITION,
    out float4 oColor    : COLOR0
)
{
    oPosition = mul(float4(position, 1.0), modelViewProj);
    oColor = color;
}