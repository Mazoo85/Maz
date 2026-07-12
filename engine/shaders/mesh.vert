#version 450
// Mesh vertex shader: transform by the MVP and pass a world-space normal to the fragment stage.

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(push_constant) uniform Push {
    mat4 mvp;
    mat4 model;
} pc;

layout(location = 0) out vec3 vNormal;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    // model is a rigid rotation here, so mat3(model) is fine for normals.
    vNormal = mat3(pc.model) * inNormal;
}
