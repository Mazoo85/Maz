#version 450
// 3D mesh vertex shader. Transforms by the model-view-projection; passes a world-space normal,
// the vertex color, and the texture coordinate to the fragment stage.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(push_constant) uniform Push {
    mat4 mvp;
    mat4 model;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vColor;
layout(location = 2) out vec2 vUV;

void main() {
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    vNormal = mat3(pc.model) * inNormal;
    vColor = inColor;
    vUV = inUV;
}
