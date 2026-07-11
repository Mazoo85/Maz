#version 450
// Sample the sprite texture and modulate by the per-vertex tint color.

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(uTexture, vUV) * vColor;
}
