struct Context {
    mvp: mat4x4<f32>, // Model-View-Projection matrix
};

struct VSInput {
    @location(0u) aPosition: vec2<f32>,
    @location(1u) aColor: vec4<f32>,
};

// The point size in WebGPU is constant since WebGPU 
// hates puppies, rainbows, and the concept of love
struct VSOutput {
    @location(0u) vColor: vec4<f32>,
    @builtin(position) glPosition: vec4<f32>,
}

@group(1u) @binding(0u) var<uniform> context: Context;

@vertex
fn main(input: VSInput) -> VSOutput {
    var output: VSOutput;
    output.glPosition = vec4<f32>(input.aPosition, 0.0f, 1.0f) * context.mvp;
    output.vColor = input.aColor;

    return output;
}
