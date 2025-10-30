
layout (set = 0, binding = 1) Texture2D texture0 : register(t0);
layout (set = 0, binding = 2) Texture2D texture1 : register(t1);
SamplerState sampler0 : register(s0);
SamplerState sampler1 : register(s1);

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

// These should mirror the definitions in SDL_render_vulkan.c
static const float TONEMAP_NONE = 0;
static const float TONEMAP_LINEAR = 1;
static const float TONEMAP_CHROME = 2;

static const float TEXTURETYPE_NONE = 0;
static const float TEXTURETYPE_RGB = 1;
static const float TEXTURETYPE_RGB_PIXELART = 2;
static const float TEXTURETYPE_PALETTE_NEAREST = 3;
static const float TEXTURETYPE_PALETTE_LINEAR = 4;
static const float TEXTURETYPE_PALETTE_PIXELART = 5;

static const float INPUTTYPE_UNSPECIFIED = 0;
static const float INPUTTYPE_SRGB = 1;
static const float INPUTTYPE_SCRGB = 2;
static const float INPUTTYPE_HDR10 = 3;

layout (set = 0, binding = 0) cbuffer Constants : register(b1)
{
    float scRGB_output;
    float texture_type;
    float input_type;
    float color_scale;
    float4 texel_size;

    float tonemap_method;
    float tonemap_factor1;
    float tonemap_factor2;
    float sdr_white_point;
};

static const float3x3 mat709to2020 = {
    { 0.627404, 0.329283, 0.043313 },
    { 0.069097, 0.919541, 0.011362 },
    { 0.016391, 0.088013, 0.895595 }
};

static const float3x3 mat2020to709 = {
    { 1.660496, -0.587656, -0.072840 },
    { -0.124547, 1.132895, -0.008348 },
    { -0.018154, -0.100597, 1.118751 }
};

float sRGBtoLinear(float v)
{
    if (v <= 0.04045) {
        v = (v / 12.92);
    } else {
        v = pow(abs(v + 0.055) / 1.055, 2.4);
    }
    return v;
}

float sRGBfromLinear(float v)
{
    if (v <= 0.0031308) {
        v = (v * 12.92);
    } else {
        v = (pow(abs(v), 1.0 / 2.4) * 1.055 - 0.055);
    }
    return v;
}

float3 PQtoLinear(float3 v)
{
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float oo_m1 = 1.0 / 0.1593017578125;
    const float oo_m2 = 1.0 / 78.84375;

    float3 num = max(pow(abs(v), oo_m2) - c1, 0.0);
    float3 den = c2 - c3 * pow(abs(v), oo_m2);
    return (10000.0 * pow(abs(num / den), oo_m1) / sdr_white_point);
}

float3 ApplyTonemap(float3 v)
{
    if (tonemap_method == TONEMAP_LINEAR) {
        v *= tonemap_factor1;
    } else if (tonemap_method == TONEMAP_CHROME) {
        if (input_type == INPUTTYPE_SCRGB) {
            // Convert to BT.2020 colorspace for tone mapping
            v = mul(mat709to2020, v);
        }

        float vmax = max(v.r, max(v.g, v.b));
        if (vmax > 0.0) {
            float scale = (1.0 + tonemap_factor1 * vmax) / (1.0 + tonemap_factor2 * vmax);
            v *= scale;
        }

        if (input_type == INPUTTYPE_SCRGB) {
            // Convert to BT.709 colorspace after tone mapping
            v = mul(mat2020to709, v);
        }
    }
    return v;
}

float4 SamplePaletteNearest(float2 uv)
{
    float index = texture0.Sample(sampler0, uv).r * 255;
    return texture1.Sample(sampler1, float2((index + 0.5) / 256, 0.5));
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

float2 GetPixelArtUV(float2 uv)
{
    // box filter size in texel units
    float2 boxSize = clamp(fwidth(uv) * texel_size.zw, 1e-5, 1);

    // scale uv by texture size to get texel coordinate
    float2 tx = uv * texel_size.zw - 0.5 * boxSize;

    // compute offset for pixel-sized box filter
    float2 txOffset = smoothstep(1 - boxSize, 1, frac(tx));

    // compute bilinear sample uv coordinates
    return (floor(tx) + 0.5 + txOffset) * texel_size.xy;
}

float4 GetInputColor(PixelShaderInput input)
{
    float4 rgba;

    if (texture_type == TEXTURETYPE_RGB) {
        rgba = texture0.Sample(sampler0, input.tex);
    } else if (texture_type == TEXTURETYPE_RGB_PIXELART) {
        float2 uv = GetPixelArtUV(input.tex);
        rgba = texture0.SampleGrad(sampler0, uv, ddx(input.tex), ddy(input.tex));
    } else if (texture_type == TEXTURETYPE_PALETTE_NEAREST) {
        rgba = SamplePaletteNearest(input.tex);
    } else if (texture_type == TEXTURETYPE_PALETTE_LINEAR) {
        rgba = SamplePaletteLinear(input.tex);
    } else if (texture_type == TEXTURETYPE_PALETTE_PIXELART) {
        float2 uv = GetPixelArtUV(input.tex);
        rgba = SamplePaletteLinear(uv);
    } else {
        // Error!
        rgba.r = 1.0;
        rgba.g = 0.0;
        rgba.b = 1.0;
        rgba.a = 1.0;
    }
    return rgba;
}

float4 GetOutputColor(float4 rgba)
{
    float4 output;

    output.rgb = rgba.rgb * color_scale;
    output.a = rgba.a;

    return output;
}

float3 GetOutputColorFromSRGB(float3 rgb)
{
    float3 output;

    if (scRGB_output) {
        rgb.r = sRGBtoLinear(rgb.r);
        rgb.g = sRGBtoLinear(rgb.g);
        rgb.b = sRGBtoLinear(rgb.b);
    }

    output.rgb = rgb * color_scale;

    return output;
}

float3 GetOutputColorFromLinear(float3 rgb)
{
    float3 output;

    output.rgb = rgb * color_scale;

    if (!scRGB_output) {
        output.r = sRGBfromLinear(output.r);
        output.g = sRGBfromLinear(output.g);
        output.b = sRGBfromLinear(output.b);
        output.rgb = saturate(output.rgb);
    }

    return output;
}

float4 AdvancedPixelShader(PixelShaderInput input)
{
    float4 rgba = GetInputColor(input);
    float4 output;

    if (input_type == INPUTTYPE_HDR10) {
        rgba.rgb = PQtoLinear(rgba.rgb);
    }

    if (tonemap_method != TONEMAP_NONE) {
        rgba.rgb = ApplyTonemap(rgba.rgb);
    }

    if (input_type == INPUTTYPE_SRGB) {
        output.rgb = GetOutputColorFromSRGB(rgba.rgb);
        output.a = rgba.a;
    } else if (input_type == INPUTTYPE_SCRGB) {
        output.rgb = GetOutputColorFromLinear(rgba.rgb);
        output.a = rgba.a;
    } else if (input_type == INPUTTYPE_HDR10) {
        rgba.rgb = mul(mat2020to709, rgba.rgb);
        output.rgb = GetOutputColorFromLinear(rgba.rgb);
        output.a = rgba.a;
    } else {
        output = GetOutputColor(rgba);
    }

    return output * input.color;
}
