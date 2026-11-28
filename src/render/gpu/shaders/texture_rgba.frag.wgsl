struct Constants {
    colorScale: f32,
};

struct PSInput {
    @location(0u) vColor: vec4<f32>,
    @location(1u) vUv: vec2<f32>,
};

struct PSOutput {
    @location(0u) oColor: vec4<f32>,
};

@group(2u) @binding(0u) var uTexture: texture_2d<f32>;
@group(2u) @binding(1u) var uSampler: sampler;

@group(3u) @binding(0u) var<uniform> constants: Constants;

fn getOutputColor(rgba: vec4<f32>, colorScale: f32) -> vec4<f32> {
    return vec4<f32>(rgba.rgb * colorScale, rgba.a);
}

@fragment
fn main(input: PSInput) -> PSOutput {
    var output: PSOutput;
    output.oColor = getOutputColor(textureSample(uTexture, uSampler, input.vUv), constants.colorScale) * input.vColor;

    return output;
}
