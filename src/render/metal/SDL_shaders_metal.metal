#include <metal_common>
#include <metal_texture>
#include <metal_matrix>

using namespace metal;

// These should mirror the definitions in SDL_render_metal.m
#define TONEMAP_NONE            0
#define TONEMAP_LINEAR          1
#define TONEMAP_CHROME          2

#define TEXTURETYPE_NONE        0
#define TEXTURETYPE_RGB         1
#define TEXTURETYPE_NV12        2
#define TEXTURETYPE_NV21        3
#define TEXTURETYPE_YUV         4

#define INPUTTYPE_UNSPECIFIED   0
#define INPUTTYPE_SRGB          1
#define INPUTTYPE_SCRGB         2
#define INPUTTYPE_HDR10         3

struct ShaderConstants
{
    float scRGB_output;
    float texture_type;
    float input_type;
    float color_scale;

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

float3 ApplyTonemap(float3 v, float input_type, float tonemap_method, float tonemap_factor1, float tonemap_factor2)
{
    const float3x3 mat709to2020 = {
        { 0.627404, 0.329283, 0.043313 },
        { 0.069097, 0.919541, 0.011362 },
        { 0.016391, 0.088013, 0.895595 }
    };
    const float3x3 mat2020to709 = {
        { 1.660496, -0.587656, -0.072840 },
        { -0.124547, 1.132895, -0.008348 },
        { -0.018154, -0.100597, 1.118751 }
    };

    if (tonemap_method == TONEMAP_LINEAR) {
        v *= tonemap_factor1;
    } else if (tonemap_method == TONEMAP_CHROME) {
        if (input_type == INPUTTYPE_SCRGB) {
            // Convert to BT.2020 colorspace for tone mapping
            v = v * mat709to2020;
        }

        float vmax = max(v.r, max(v.g, v.b));
        if (vmax > 0.0) {
            float scale = (1.0 + tonemap_factor1 * vmax) / (1.0 + tonemap_factor2 * vmax);
            v *= scale;
        }

        if (input_type == INPUTTYPE_SCRGB) {
            // Convert to BT.709 colorspace after tone mapping
            v = v * mat2020to709;
        }
    }
    return v;
}

float4 GetInputColor(float2 texcoord, float texture_type, constant YUVDecode &decode, texture2d<float> tex0, texture2d<float> tex1, sampler s)
{
    float4 rgba;

    if (texture_type == TEXTURETYPE_NONE) {
        rgba = 1.0;
    } else if (texture_type == TEXTURETYPE_RGB) {
        rgba = tex0.sample(s, texcoord);
    } else if (texture_type == TEXTURETYPE_NV12) {
        float3 yuv;
        yuv.x = tex0.sample(s, texcoord).r;
        yuv.yz = tex1.sample(s, texcoord).rg;

        rgba.rgb = (yuv + decode.offset) * decode.matrix;
        rgba.a = 1.0;
    } else if (texture_type == TEXTURETYPE_NV21) {
        float3 yuv;
        yuv.x = tex0.sample(s, texcoord).r;
        yuv.yz = tex1.sample(s, texcoord).gr;

        rgba.rgb = (yuv + decode.offset) * decode.matrix;
        rgba.a = 1.0;
    } else {
        // Error!
        rgba.r = 1.0;
        rgba.g = 0.0;
        rgba.b = 0.0;
        rgba.a = 1.0;
    }
    return rgba;
}

float4 GetOutputColor(float4 rgba, float color_scale)
{
    float4 output;

    output.rgb = rgba.rgb * color_scale;
    output.a = rgba.a;

    return output;
}

float3 GetOutputColorFromSRGB(float3 rgb, float scRGB_output, float color_scale)
{
    float3 output;

    if (scRGB_output) {
        rgb.r = sRGBtoLinear(rgb.r);
        rgb.g = sRGBtoLinear(rgb.g);
        rgb.b = sRGBtoLinear(rgb.b);
    }

    output = rgb * color_scale;

    return output;
}

float3 GetOutputColorFromLinear(float3 rgb, float scRGB_output, float color_scale)
{
    float3 output;

    output = rgb * color_scale;

    if (!scRGB_output) {
        output.r = sRGBfromLinear(output.r);
        output.g = sRGBfromLinear(output.g);
        output.b = sRGBfromLinear(output.b);
        output = clamp(output.rgb, 0.0, 1.0);
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
    return GetOutputColor(1.0, c.color_scale) * in.color;
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

fragment float4 SDL_Copy_fragment(CopyVertexOutput vert [[stage_in]],
                                  constant ShaderConstants &c [[buffer(0)]],
                                  texture2d<float> tex [[texture(0)]],
                                  sampler s [[sampler(0)]])
{
    return GetOutputColor(tex.sample(s, vert.texcoord), c.color_scale) * vert.color;
}

fragment float4 SDL_Advanced_fragment(CopyVertexOutput vert [[stage_in]],
                                      constant ShaderConstants &c [[buffer(0)]],
                                      constant YUVDecode &decode [[buffer(1)]],
                                      texture2d<float> tex0 [[texture(0)]],
                                      texture2d<float> tex1 [[texture(1)]],
                                      sampler s [[sampler(0)]])
{
    const float3x3 mat2020to709 = {
        { 1.660496, -0.587656, -0.072840 },
        { -0.124547, 1.132895, -0.008348 },
        { -0.018154, -0.100597, 1.118751 }
    };
    float4 rgba = GetInputColor(vert.texcoord, c.texture_type, decode, tex0, tex1, s);
    float4 output;

    if (c.input_type == INPUTTYPE_HDR10) {
        rgba.rgb = PQtoLinear(rgba.rgb, c.sdr_white_point);
    }

    if (c.tonemap_method != TONEMAP_NONE) {
        rgba.rgb = ApplyTonemap(rgba.rgb, c.input_type, c.tonemap_method, c.tonemap_factor1, c.tonemap_factor2);
    }

    if (c.input_type == INPUTTYPE_SRGB) {
        output.rgb = GetOutputColorFromSRGB(rgba.rgb, c.scRGB_output, c.color_scale);
        output.a = rgba.a;
    } else if (c.input_type == INPUTTYPE_SCRGB) {
        output.rgb = GetOutputColorFromLinear(rgba.rgb, c.scRGB_output, c.color_scale);
        output.a = rgba.a;
    } else if (c.input_type == INPUTTYPE_HDR10) {
        rgba.rgb = rgba.rgb * mat2020to709;
        output.rgb = GetOutputColorFromLinear(rgba.rgb, c.scRGB_output, c.color_scale);
        output.a = rgba.a;
    } else {
        output = GetOutputColor(rgba, c.color_scale);
    }

    return output * vert.color;
}

fragment float4 SDL_YUV_fragment(CopyVertexOutput vert [[stage_in]],
                                 constant ShaderConstants &c [[buffer(0)]],
                                 constant YUVDecode &decode [[buffer(1)]],
                                 texture2d<float> texY [[texture(0)]],
                                 texture2d_array<float> texUV [[texture(1)]],
                                 sampler s [[sampler(0)]])
{
    float3 yuv;
    yuv.x = texY.sample(s, vert.texcoord).r;
    yuv.y = texUV.sample(s, vert.texcoord, 0).r;
    yuv.z = texUV.sample(s, vert.texcoord, 1).r;

    float3 rgb;
    rgb = (yuv + decode.offset) * decode.matrix;

    float4 output;
    output.rgb = GetOutputColorFromSRGB(rgb, c.scRGB_output, c.color_scale);
    output.a = 1.0;

    return output * vert.color;
}
