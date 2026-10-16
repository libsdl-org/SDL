#include <metal_common>
#include <metal_texture>
#include <metal_matrix>
#include <metal_stdlib>

using namespace metal;

// These should mirror the definitions in SDL_render_metal.m
#define TONEMAP_NONE            0
#define TONEMAP_LINEAR          1
#define TONEMAP_CHROME          2

#define TEXTURETYPE_NONE                0
#define TEXTURETYPE_RGB                 1
#define TEXTURETYPE_RGB_PIXELART        2
#define TEXTURETYPE_PALETTE_NEAREST     3
#define TEXTURETYPE_PALETTE_LINEAR      4
#define TEXTURETYPE_PALETTE_PIXELART    5
#define TEXTURETYPE_NV12                6
#define TEXTURETYPE_NV21                7
#define TEXTURETYPE_YUV                 8

#define INPUTTYPE_UNSPECIFIED   0
#define INPUTTYPE_SRGB          1
#define INPUTTYPE_SCRGB         2
#define INPUTTYPE_HDR10         3

#define OUTPUTTYPE_UNSPECIFIED  0
#define OUTPUTTYPE_SRGB         1
#define OUTPUTTYPE_SCRGB        2
#define OUTPUTTYPE_HDR10        3

struct ShaderConstants
{
    float output_type;
    float texture_type;
    float input_type;
    float color_scale;
    float4 texel_size;

    float tonemap_method;
    float tonemap_factor1;
    float tonemap_factor2;
    float sdr_white_point;
};

struct YUVDecode
{
    float3 offset;
    float3x3 matrix;
};

constant float SCRGB_NITS = 80.0;

constant float3x3 mat709to2020 = {
    { 0.627404, 0.329283, 0.043313 },
    { 0.069097, 0.919541, 0.011362 },
    { 0.016391, 0.088013, 0.895595 }
};

constant float3x3 mat2020to709 = {
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

float3 PQtoLinear(float3 v, float sdr_white_point)
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

float3 PQfromNits(float3 v)
{
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float m1 = 0.1593017578125;
    const float m2 = 78.84375;

    float3 y = clamp(v / 10000.0, 0.0, 1.0);
    float3 num = c1 + c2 * pow(y, m1);
    float3 den = 1.0 + c3 * pow(y, m1);
    return pow(num / den, m2);
}

float3 ApplyTonemap(float3 v, float tonemap_method, float tonemap_factor1, float tonemap_factor2)
{
    if (tonemap_method == TONEMAP_LINEAR) {
        v *= tonemap_factor1;
    } else if (tonemap_method == TONEMAP_CHROME) {
        float vmax = max(v.r, max(v.g, v.b));
        if (vmax > 0.0) {
            float scale = (1.0 + tonemap_factor1 * vmax) / (1.0 + tonemap_factor2 * vmax);
            v *= scale;
        }
    }
    return v;
}

float4 SamplePaletteNearest(texture2d<float> tex0, texture2d<float> tex1, sampler s0, sampler s1, float2 uv)
{
    float index = tex0.sample(s0, uv).r * 255;
    return tex1.sample(s1, float2((index + 0.5) / 256, 0.5));
}

// Implementation with thanks from bgolus:
// https://discussions.unity.com/t/how-to-make-data-shader-support-bilinear-trilinear/598639/8
float4 SamplePaletteLinear(texture2d<float> tex0, texture2d<float> tex1, sampler s0, sampler s1, float2 uv, float4 texel_size)
{
    // scale & offset uvs to integer values at texel centers
    float2 uv_texels = uv * texel_size.zw + 0.5;

    // get uvs for the center of the 4 surrounding texels by flooring
    float4 uv_min_max = float4((floor(uv_texels) - 0.5) * texel_size.xy, (floor(uv_texels) + 0.5) * texel_size.xy);

    // blend factor
    float2 uv_frac = fract(uv_texels);

    // sample all 4 texels
    float4 texelA = SamplePaletteNearest(tex0, tex1, s0, s1, uv_min_max.xy);
    float4 texelB = SamplePaletteNearest(tex0, tex1, s0, s1, uv_min_max.xw);
    float4 texelC = SamplePaletteNearest(tex0, tex1, s0, s1, uv_min_max.zy);
    float4 texelD = SamplePaletteNearest(tex0, tex1, s0, s1, uv_min_max.zw);

    // bilinear interpolation
    return mix(mix(texelA, texelB, uv_frac.y), mix(texelC, texelD, uv_frac.y), uv_frac.x);
}

float2 GetPixelArtUV(float2 uv, float4 texel_size)
{
    // box filter size in texel units
    float2 boxSize = clamp(fwidth(uv) * texel_size.zw, 1e-5, 1);

    // scale uv by texture size to get texel coordinate
    float2 tx = uv * texel_size.zw - 0.5 * boxSize;

    // compute offset for pixel-sized box filter
    float2 txOffset = smoothstep(1 - boxSize, 1, fract(tx));

    // compute bilinear sample uv coordinates
    return (floor(tx) + 0.5 + txOffset) * texel_size.xy;
}

float4 GetOutputColorSimple(constant ShaderConstants &c, float4 vertex_color)
{
    float4 output = float4(c.color_scale, c.color_scale, c.color_scale, 1.0) * vertex_color;
    if (c.output_type == OUTPUTTYPE_HDR10) {
        output.rgb = PQfromNits(output.rgb * SCRGB_NITS);
    }
    return output;
}

float4 GetOutputColor(float4 rgba, constant ShaderConstants &c, float4 vertex_color)
{
    float3 rgb = rgba.rgb;
    bool rgb2020 = false;

    // Convert the input to linear values, unless we're staying in sRGB space
    if (c.input_type == INPUTTYPE_HDR10) {
        rgb = PQtoLinear(rgb, c.sdr_white_point);
        rgb2020 = true;
    } else if (c.input_type == INPUTTYPE_SRGB) {
        if (c.output_type != OUTPUTTYPE_SRGB) {
            rgb.r = sRGBtoLinear(rgb.r);
            rgb.g = sRGBtoLinear(rgb.g);
            rgb.b = sRGBtoLinear(rgb.b);
        }
    } else if (c.input_type == INPUTTYPE_UNSPECIFIED) {
        // The sampler has already converted to linear if necessary
    } else if (c.input_type != INPUTTYPE_SCRGB) {
        // Unexpected input type, use magenta error color
        return float4(1.0, 0.0, 1.0, 1.0);
    }

    // Tone mapping is done in the BT.2020 colorspace
    if (c.tonemap_method != TONEMAP_NONE) {
        if (!rgb2020) {
            rgb = rgb * mat709to2020;
            rgb2020 = true;
        }
        rgb = ApplyTonemap(rgb, c.tonemap_method, c.tonemap_factor1, c.tonemap_factor2);
    }

    // Convert to the output color primaries
    if (c.output_type == OUTPUTTYPE_HDR10) {
        if (!rgb2020) {
            rgb = rgb * mat709to2020;
        }
    } else {
        if (rgb2020) {
            rgb = rgb * mat2020to709;
        }
    }

    rgb *= c.color_scale;

    if (c.output_type == OUTPUTTYPE_SRGB &&
        (c.input_type == INPUTTYPE_SCRGB || c.input_type == INPUTTYPE_HDR10)) {
        rgb.r = sRGBfromLinear(rgb.r);
        rgb.g = sRGBfromLinear(rgb.g);
        rgb.b = sRGBfromLinear(rgb.b);
        rgb = clamp(rgb, 0.0, 1.0);
    }

    float4 output = float4(rgb, rgba.a) * vertex_color;
    if (c.output_type == OUTPUTTYPE_HDR10) {
        output.rgb = PQfromNits(output.rgb * SCRGB_NITS);
    }
    return output;
}

struct SolidVertexInput
{
    float2 position [[attribute(0)]];
    float4 color    [[attribute(1)]];
};

struct SolidVertexOutput
{
    float4 position [[position]];
    float4 color;
    float pointSize [[point_size]];
};

vertex SolidVertexOutput SDL_Solid_vertex(SolidVertexInput in [[stage_in]],
                                          constant float4x4 &projection [[buffer(2)]],
                                          constant float4x4 &transform [[buffer(3)]])
{
    SolidVertexOutput v;
    v.position = (projection * transform) * float4(in.position, 0.0f, 1.0f);
    v.color = in.color;
    v.pointSize = 1.0f;
    return v;
}

fragment float4 SDL_Solid_fragment(SolidVertexInput in [[stage_in]],
                                   constant ShaderConstants &c [[buffer(0)]])
{
    return GetOutputColorSimple(c, in.color);
}

struct CopyVertexInput
{
    float2 position [[attribute(0)]];
    float4 color    [[attribute(1)]];
    float2 texcoord [[attribute(2)]];
};

struct CopyVertexOutput
{
    float4 position [[position]];
    float4 color;
    float2 texcoord;
};

vertex CopyVertexOutput SDL_Copy_vertex(CopyVertexInput in [[stage_in]],
                                        constant float4x4 &projection [[buffer(2)]],
                                        constant float4x4 &transform [[buffer(3)]])
{
    CopyVertexOutput v;
    v.position = (projection * transform) * float4(in.position, 0.0f, 1.0f);
    v.color = in.color;
    v.texcoord = in.texcoord;
    return v;
}

fragment float4 SDL_Palette_fragment(CopyVertexOutput vert [[stage_in]],
                                     constant ShaderConstants &c [[buffer(0)]],
                                     texture2d<float> tex0 [[texture(0)]],
                                     texture2d<float> tex1 [[texture(1)]],
                                     sampler s0 [[sampler(0)]],
                                     sampler s1 [[sampler(1)]])
{
    float4 rgba;

    if (c.texture_type == TEXTURETYPE_PALETTE_NEAREST) {
        rgba = SamplePaletteNearest(tex0, tex1, s0, s1, vert.texcoord);
    } else if (c.texture_type == TEXTURETYPE_PALETTE_LINEAR) {
        rgba = SamplePaletteLinear(tex0, tex1, s0, s1, vert.texcoord, c.texel_size);
    } else if (c.texture_type == TEXTURETYPE_PALETTE_PIXELART) {
        float2 uv = GetPixelArtUV(vert.texcoord, c.texel_size);
        rgba = SamplePaletteLinear(tex0, tex1, s0, s1, uv, c.texel_size);
    } else {
        // Unexpected texture type, use magenta error color
        rgba = float4(1.0, 0.0, 1.0, 1.0);
    }
    return GetOutputColor(rgba, c, vert.color);
}

fragment float4 SDL_Copy_fragment(CopyVertexOutput vert [[stage_in]],
                                  constant ShaderConstants &c [[buffer(0)]],
                                  texture2d<float> tex [[texture(0)]],
                                  sampler s [[sampler(0)]])
{
    float4 rgba;

    if (c.texture_type == TEXTURETYPE_RGB) {
        rgba = tex.sample(s, vert.texcoord);
    } else if (c.texture_type == TEXTURETYPE_RGB_PIXELART) {
        float2 uv = GetPixelArtUV(vert.texcoord, c.texel_size);
        rgba = tex.sample(s, uv, gradient2d(dfdx(vert.texcoord), dfdy(vert.texcoord)));
    } else {
        // Unexpected texture type, use magenta error color
        rgba = float4(1.0, 0.0, 1.0, 1.0);
    }
    return GetOutputColor(rgba, c, vert.color);
}

fragment float4 SDL_YUV_fragment(CopyVertexOutput vert [[stage_in]],
                                 constant ShaderConstants &c [[buffer(0)]],
                                 constant YUVDecode &decode [[buffer(1)]],
                                 texture2d<float> texY [[texture(0)]],
                                 texture2d<float> texU [[texture(1)]],
                                 texture2d<float> texV [[texture(2)]],
                                 sampler s [[sampler(0)]])
{
    float3 yuv;
    yuv.x = texY.sample(s, vert.texcoord).r;
    yuv.y = texU.sample(s, vert.texcoord).r;
    yuv.z = texV.sample(s, vert.texcoord).r;

    float4 rgba;
    rgba.rgb = (yuv + decode.offset) * decode.matrix;
    rgba.a = 1.0;

    return GetOutputColor(rgba, c, vert.color);
}

fragment float4 SDL_NV12_fragment(CopyVertexOutput vert [[stage_in]],
                                  constant ShaderConstants &c [[buffer(0)]],
                                  constant YUVDecode &decode [[buffer(1)]],
                                  texture2d<float> texY [[texture(0)]],
                                  texture2d<float> texUV [[texture(1)]],
                                  sampler s [[sampler(0)]])
{
    float4 rgba;
    if (c.texture_type == TEXTURETYPE_NV12) {
        float3 yuv;
        yuv.x = texY.sample(s, vert.texcoord).r;
        yuv.yz = texUV.sample(s, vert.texcoord).rg;

        rgba.rgb = (yuv + decode.offset) * decode.matrix;
    } else if (c.texture_type == TEXTURETYPE_NV21) {
        float3 yuv;
        yuv.x = texY.sample(s, vert.texcoord).r;
        yuv.yz = texUV.sample(s, vert.texcoord).gr;

        rgba.rgb = (yuv + decode.offset) * decode.matrix;
    } else {
        // Unexpected texture type, use magenta error color
        rgba.rgb = float3(1.0, 0.0, 1.0);
    }
    rgba.a = 1.0;

    return GetOutputColor(rgba, c, vert.color);
}
