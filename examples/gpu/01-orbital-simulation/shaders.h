// These shaders are all in textual WGSL.
// This is for two reasons:
// - 1: This SDL3 example is only ever meant to run on the WebGPU backend, on the SDL website.
// - 2: I didn't want to rewrite these shaders.
// They're also formatted a little grossly since Clang & MSVC don't support R"" strings.

/**
 * This is the vertex shader for each asteroid.
 * It reads the asteroid storage buffer (previously used in the compute shader), and displaces each asteroid by the buffer entry's "pos" field.
 */
const char *vtxShader = "\n\
struct Asteroid {\n\
  pos: vec3<f32>,\n\
  vel: vec3<f32>,\n\
};\n\
@group(0u) @binding(0u) var<storage, read> posBuf: array<Asteroid>;\n\
@group(1u) @binding(0u) var<uniform> camMatrix: mat4x4<f32>;\n\
\n\
@vertex\n\
fn main(@location(0u) pos: vec3<f32>, @builtin(instance_index) index: u32) -> @builtin(position) vec4<f32> {\n\
  return vec4<f32>(pos + posBuf[index].pos, 1.0f) * camMatrix;\n\
}";

/**
 * This is the asteroid fragment shader.
 * Very simple, this just returns an almost-pure white.
 */
const char *frgShader = "\n\
@fragment\n\
fn main() -> @location(0) vec4<f32> {\n\
  return vec4<f32>(0.9f, 0.9f, 0.9f, 1.0f);\n\
}";

/**
 * This is the compute shader for the asteroids.
 * It uses a simplified version of Newton's law of universal gravitation
 * to calculate the force applied to each asteroid.
 * F = GM / r²
 */
const char *cmpShader = "\n\
// This is the struct which stores each asteroid's data.\n\
struct Asteroid {\n\
    pos: vec3<f32>,\n\
    vel: vec3<f32>,\n\
};\n\
\n\
// This is the uniform data that's pushed to the compute shader.\n\
// You don't *have* to organize your uniform data into a struct, but that's how I like to do it.\n\
struct UniformData {\n\
    timescale: f32,\n\
    numAsteroids: f32,\n\
    deltaTime: f32,\n\
};\n\
\n\
// G & M are merged into one since floating point numbers are a little weird at large sizes.\n\
const GM = 3.986004418e+14;\n\
\n\
@group(1u) @binding(0u) var<storage, read_write> buf: array<Asteroid>;\n\
@group(2u) @binding(0u) var<uniform> uniformData: UniformData;\n\
\n\
fn accel(pos: vec3<f32>) -> vec3<f32>{\n\
    // We don't bother calculating the force exerted upon the Earth by each asteroid since it's miniscule.\n\
    // This isn't meant to be a 100% accurate physics simulation, it's meant to run in your browser :P\n\
\n\
    // r is the distance between the asteroid and the Earth.\n\
    // Since the earth's center of mass is at (0, 0, 0), we don't have to add anything to the asteroid's position.\n\
    let r2: f32 = pow(pos.x, 2) + pow(pos.y, 2) + pow(pos.z, 2);\n\
    let r: f32 = sqrt(r2);\n\
\n\
    // Finally we'll calculate the force exerted upon the asteroid.\n\
    return pos * -(GM / (r2 * r));\n\
}\n\
\n\
@compute @workgroup_size(64, 1, 1)\n\
fn main(@builtin(global_invocation_id) id: vec3<u32>){\n\
    var ast: Asteroid = buf[id.x];\n\
    // We'll first calculate the updated velocity, and then the position.\n\
    ast.vel += (accel(ast.pos) * (uniformData.deltaTime)) * uniformData.timescale;\n\
    ast.pos += (ast.vel * uniformData.deltaTime) * uniformData.timescale;\n\
\n\
    buf[id.x] = ast;\n\
}";
