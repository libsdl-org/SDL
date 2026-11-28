struct Constants {
    colorScale: f32,
};

struct PSInput {
    @location(0u) vColor: vec4<f32>,
};

struct PSOutput {
    @location(0u) oColor: vec4<f32>,
};

@group(3u) @binding(0u) var<uniform> constants: Constants;

fn getOutputColor(rgba: vec4<f32>, colorScale: f32) -> vec4<f32> {
    return vec4<f32>(rgba.rgb * colorScale, rgba.a);
}

@fragment
fn main(input: PSInput) -> PSOutput {
    var output: PSOutput;
    output.oColor = getOutputColor(vec4<f32>(1.0f), constants.colorScale) * input.vColor;

    return output;
}
