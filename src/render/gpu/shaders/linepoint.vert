#version 450

layout(location = 0) in vec2 a_position;

layout(location = 0) out vec4 v_color;

layout(set = 1, binding = 0) uniform Context {
    mat4 mvp;
    vec4 color;
    vec2 texture_size;  /* XXX unused */
} u_context;

void main() {
    gl_PointSize = 1.0;  /* FIXME: D3D11 pls */
    gl_Position = u_context.mvp * vec4(a_position, 0, 1);
    v_color = u_context.color;
}
