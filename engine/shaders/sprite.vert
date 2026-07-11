#version 450
// Batched 2D sprite vertex shader. Quads are pre-transformed on the CPU into world/pixel space;
// this only applies the camera view-projection passed via push constant.

layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    gl_Position = pc.viewProj * vec4(inPos, 0.0, 1.0);
    vUV = inUV;
    vColor = inColor;
}
