struct Context {
    mvp: mat4x4<f32>, // Model-View-Projection matrix
};

struct VSInput {
    @location(0u) aPosition: vec2<f32>,
    @location(1u) aColor: vec4<f32>,
    @location(2u) aUv: vec2<f32>,
};

struct VSOutput {
    @location(0u) vColor: vec4<f32>,
    @location(1u) vUv: vec2<f32>,
    @builtin(position) glPosition: vec4<f32>,
}

@group(1u) @binding(0u) var<uniform> context: Context;

@vertex
fn main(input: VSInput) -> VSOutput {
    var output: VSOutput;
    output.glPosition = context.mvp * vec4<f32>(input.aPosition, 0.0f, 1.0f);
    output.vColor = input.aColor;
    output.vUv = input.aUv;

    return output;
}
