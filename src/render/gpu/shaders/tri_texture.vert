#version 450

layout(location = 0) in vec2 a_position;
layout(location = 1) in vec4 a_color;
layout(location = 2) in vec2 a_uv;

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;

layout(set = 1, binding = 0) uniform Context {
    mat4 mvp;
    vec4 color;  /* XXX unused */
    vec2 texture_size;
} u_context;

void main() {
    gl_Position = u_context.mvp * vec4(a_position, 0, 1);
    v_color = a_color;
    v_uv = a_uv / u_context.texture_size;
}
