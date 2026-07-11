#version 450
// 3D mesh vertex shader: MVP transform, world-space normal, UV, and the position in the light's
// clip space for shadow lookup.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(push_constant) uniform Push {
    mat4 mvp;
    mat4 model;
    mat4 lightVP;
} pc;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vColor;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec4 vLightPos;

void main() {
    vec4 world = pc.model * vec4(inPos, 1.0);
    gl_Position = pc.mvp * vec4(inPos, 1.0);
    vNormal = mat3(pc.model) * inNormal;
    vColor = inColor;
    vUV = inUV;
    vLightPos = pc.lightVP * world;
}
