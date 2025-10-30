
cbuffer Constants
{
    float4 texel_size;
};

uniform sampler2D image;
uniform sampler1D palette;

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

static const float TEXTURETYPE_NONE = 0;
static const float TEXTURETYPE_PALETTE_NEAREST = 1;
static const float TEXTURETYPE_PALETTE_LINEAR = 2;

float4 SamplePaletteNearest(float2 uv)
{
    float index = tex2D(image, uv).r * 255;
    return tex1D(palette, (index + 0.5) / 256);
}

// Implementation with thanks from bgolus:
// https://discussions.unity.com/t/how-to-make-data-shader-support-bilinear-trilinear/598639/8
float4 SamplePaletteLinear(float2 uv)
{
    // scale & offset uvs to integer values at texel centers
    float2 uv_texels = uv * texel_size.zw + 0.5;

    // get uvs for the center of the 4 surrounding texels by flooring
    float4 uv_min_max = float4((floor(uv_texels) - 0.5) * texel_size.xy, (floor(uv_texels) + 0.5) * texel_size.xy);

    // blend factor
    float2 uv_frac = frac(uv_texels);

    // sample all 4 texels
    float4 texelA = SamplePaletteNearest(uv_min_max.xy);
    float4 texelB = SamplePaletteNearest(uv_min_max.xw);
    float4 texelC = SamplePaletteNearest(uv_min_max.zy);
    float4 texelD = SamplePaletteNearest(uv_min_max.zw);

    // bilinear interpolation
    return lerp(lerp(texelA, texelB, uv_frac.y), lerp(texelC, texelD, uv_frac.y), uv_frac.x);
}

