#version 450
// Sprite fragment shader: sample the bound texture and modulate by the per-vertex tint.
// Straight-alpha; the pipeline does the src-alpha / one-minus-src-alpha blend.

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(uTexture, vUV) * vColor;
}
