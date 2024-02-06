#include <metal_common>
#include <metal_texture>
#include <metal_matrix>

using namespace metal;

float3 scRGBtoNits(float3 v)
{
    return v * 80.0;
}

float3 scRGBfromNits(float3 v)
{
    return v / 80.0;
}

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

float3 PQtoNits(float3 v)
{
    const float c1 = 0.8359375;
    const float c2 = 18.8515625;
    const float c3 = 18.6875;
    const float oo_m1 = 1.0 / 0.1593017578125;
    const float oo_m2 = 1.0 / 78.84375;

    float3 num = max(pow(abs(v), oo_m2) - c1, 0.0);
    float3 den = c2 - c3 * pow(abs(v), oo_m2);
    return 10000.0 * pow(abs(num / den), oo_m1);
}

float4 GetOutputColor(float4 rgba, float color_scale)
{
    float4 output;

    output.rgb = rgba.rgb * color_scale;
    output.a = rgba.a;

    return output;
}

float4 GetOutputColorFromSRGB(float3 rgb, float scRGB_output, float color_scale)
{
    float4 output;

    if (scRGB_output) {
        rgb.r = sRGBtoLinear(rgb.r);
        rgb.g = sRGBtoLinear(rgb.g);
        rgb.b = sRGBtoLinear(rgb.b);
    }

    output.rgb = rgb * color_scale;
    output.a = 1.0;

    return output;
}

float4 GetOutputColorFromSCRGB(float3 rgb, float scRGB_output, float color_scale)
{
    float4 output;

    output.rgb = rgb * color_scale;
    output.a = 1.0;

    if (!scRGB_output) {
        output.r = sRGBfromLinear(output.r);
        output.g = sRGBfromLinear(output.g);
        output.b = sRGBfromLinear(output.b);
        output.rgb = clamp(output.rgb, 0.0, 1.0);
    }

    return output;
}

struct ShaderConstants
{
    float scRGB_output;
    float color_scale;
};

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

struct YUVDecode
{
    float3 offset;
    float3x3 matrix;
};

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

    return GetOutputColorFromSRGB(rgb, c.scRGB_output, c.color_scale) * vert.color;
}

fragment float4 SDL_NV12_fragment(CopyVertexOutput vert [[stage_in]],
                                  constant ShaderConstants &c [[buffer(0)]],
                                  constant YUVDecode &decode [[buffer(1)]],
                                  texture2d<float> texY [[texture(0)]],
                                  texture2d<float> texUV [[texture(1)]],
                                  sampler s [[sampler(0)]])
{
    float3 yuv;
    yuv.x = texY.sample(s, vert.texcoord).r;
    yuv.yz = texUV.sample(s, vert.texcoord).rg;

    float3 rgb;
    rgb = (yuv + decode.offset) * decode.matrix;

    return GetOutputColorFromSRGB(rgb, c.scRGB_output, c.color_scale) * vert.color;
}

fragment float4 SDL_NV21_fragment(CopyVertexOutput vert [[stage_in]],
                                  constant ShaderConstants &c [[buffer(0)]],
                                  constant YUVDecode &decode [[buffer(1)]],
                                  texture2d<float> texY [[texture(0)]],
                                  texture2d<float> texUV [[texture(1)]],
                                  sampler s [[sampler(0)]])
{
    float3 yuv;
    yuv.x = texY.sample(s, vert.texcoord).r;
    yuv.yz = texUV.sample(s, vert.texcoord).gr;

    float3 rgb;
    rgb = (yuv + decode.offset) * decode.matrix;

    return GetOutputColorFromSRGB(rgb, c.scRGB_output, c.color_scale) * vert.color;
}

fragment float4 SDL_HDR10_fragment(CopyVertexOutput vert [[stage_in]],
                                   constant ShaderConstants &c [[buffer(0)]],
                                   constant YUVDecode &decode [[buffer(1)]],
                                   texture2d<float> texY [[texture(0)]],
                                   texture2d<float> texUV [[texture(1)]],
                                   sampler s [[sampler(0)]])
{
    const float3x3 mat2020to709 = {
        { 1.660496, -0.587656, -0.072840 },
        { -0.124547, 1.132895, -0.008348 },
        { -0.018154, -0.100597, 1.118751 }
    };

    float3 yuv;
    yuv.x = texY.sample(s, vert.texcoord).r;
    yuv.yz = texUV.sample(s, vert.texcoord).rg;

    float3 rgb;
    rgb = (yuv + decode.offset) * decode.matrix;

    rgb = PQtoNits(rgb);

    rgb = rgb * mat2020to709;

    rgb = scRGBfromNits(rgb);

    return GetOutputColorFromSCRGB(rgb, c.scRGB_output, c.color_scale) * vert.color;
}

