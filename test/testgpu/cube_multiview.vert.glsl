// FIXME: This is here because DXC doesn't work on my machine, so I can't compile HLSL.
#version 460 core
#extension GL_EXT_multiview : require

layout(location = 0) in vec3 iPosition;
layout(location = 1) in vec3 iColor;

layout(set = 1, binding = 0) uniform UBO {
    mat4 ModelViewProj[2];
};

layout(location = 0) out vec4 oColor;

void main() {
    oColor = vec4(iColor, 1.0);
    gl_Position = ModelViewProj[gl_ViewIndex] * vec4(iPosition, 1.0);
}
