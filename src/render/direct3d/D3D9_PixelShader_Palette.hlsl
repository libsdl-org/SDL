
uniform sampler2D image;
uniform sampler1D palette;

struct PixelShaderInput
{
    float4 pos : SV_POSITION;
    float2 tex : TEXCOORD0;
    float4 color : COLOR0;
};

float4 main(PixelShaderInput input) : SV_TARGET
{
    float4 Output;
    float index;
    index = tex2D(image, input.tex).r;
    Output = tex1D(palette, index * (255. / 256) + (0.5 / 256));
    return Output * input.color;
}
