#version 450
// Sprite vertex shader: map a pixel-space quad through the 2D orthographic projection.
// Positions arrive already in screen pixels (origin top-left); the push-constant matrix
// (maz::math::ortho2D) turns them into Vulkan clip space. UV + tint pass straight through.

layout(location = 0) in vec2 inPos;    // screen pixels, top-left origin
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec4 inColor;   // per-vertex tint (multiplied with the texture)

layout(push_constant) uniform Push {
    mat4 proj;
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    gl_Position = pc.proj * vec4(inPos, 0.0, 1.0);
    vUV = inUV;
    vColor = inColor;
}
