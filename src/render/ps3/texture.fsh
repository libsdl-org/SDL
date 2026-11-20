void main(
    float2 texcoord : TEXCOORD0,

    uniform sampler2D tex : TEXUNIT0,

    out float4 color : COLOR
)
{
    color = tex2D(tex, texcoord);
   // color = float4(1.0, 0.0, 0.0, 1.0);  // hardcoded red, ignore texture sampling entirely

}
