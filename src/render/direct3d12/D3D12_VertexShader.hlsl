#pragma pack_matrix( row_major )

cbuffer VertexShaderConstants : register(b0)
{
    matrix model;
    matrix projectionAndView;
};

struct VertexShaderInput
{
    float3 pos : POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

struct VertexShaderOutput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

#define ColorRS \
    "RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "DENY_HULL_SHADER_ROOT_ACCESS )," \
    "RootConstants(num32BitConstants=32, b0)," \
    "RootConstants(num32BitConstants=20, b1)"\

#define TextureRS \
    "RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "            DENY_HULL_SHADER_ROOT_ACCESS )," \
    "RootConstants(num32BitConstants=32, b0),"\
    "RootConstants(num32BitConstants=20, b1),"\
    "DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( Sampler(s0), visibility = SHADER_VISIBILITY_PIXEL )"

#define YUVRS \
    "RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "            DENY_HULL_SHADER_ROOT_ACCESS )," \
    "RootConstants(num32BitConstants=32, b0),"\
    "RootConstants(num32BitConstants=20, b1),"\
    "DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( SRV(t1), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( SRV(t2), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( Sampler(s0), visibility = SHADER_VISIBILITY_PIXEL )"

#define NVRS \
    "RootFlags ( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
    "            DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
    "            DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
    "            DENY_HULL_SHADER_ROOT_ACCESS )," \
    "RootConstants(num32BitConstants=32, b0),"\
    "RootConstants(num32BitConstants=20, b1),"\
    "DescriptorTable ( SRV(t0), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( SRV(t1), visibility = SHADER_VISIBILITY_PIXEL ),"\
    "DescriptorTable ( Sampler(s0), visibility = SHADER_VISIBILITY_PIXEL )"

[RootSignature(ColorRS)]
VertexShaderOutput mainColor(VertexShaderInput input)
{
    VertexShaderOutput output;
    float4 pos = float4(input.pos, 1.0f);

    // Transform the vertex position into projected space.
    pos = mul(pos, model);
    pos = mul(pos, projectionAndView);
    output.pos = pos;

    // Pass through texture coordinates and color values without transformation
    output.tex = input.tex;
    output.color = input.color;

    return output;
}

[RootSignature(TextureRS)]
VertexShaderOutput mainTexture(VertexShaderInput input)
{
    return mainColor(input);
}

[RootSignature(YUVRS)]
VertexShaderOutput mainYUV(VertexShaderInput input)
{
    return mainColor(input);
}

[RootSignature(NVRS)]
VertexShaderOutput mainNV(VertexShaderInput input)
{
    return mainColor(input);
}
