#version 450

#ifdef VERTEX

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_color;

layout (location = 0) out vec4 out_color;

layout (set = 1, binding = 0) uniform UBO
{
    mat4x4 modelViewProj;
};

void main()
{
    out_color = vec4(in_color, 1.0);
    gl_Position = modelViewProj * vec4(in_position, 1.0);
}

#else

layout (location = 0) in vec4 in_color;
layout (location = 0) out vec4 out_color;

void main()
{
    out_color = in_color;
}

#endif