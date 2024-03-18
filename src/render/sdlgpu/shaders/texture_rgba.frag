#version 450

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;

layout(set = 2, binding = 0) uniform sampler2D u_texture;

layout(location = 0) out vec4 o_color;

void main() {
    o_color = texture(u_texture, v_uv) * v_color;
}
