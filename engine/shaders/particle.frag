#version 450
// Soft round particle: radial falloff from the quad center, tinted by the particle color. Additive
// blending builds up a glow where particles overlap.

layout(location = 0) in vec2 vUV;
layout(location = 1) in vec4 vColor;

layout(location = 0) out vec4 outColor;

void main() {
    float d = length(vUV - 0.5) * 2.0;      // 0 at center, 1 at edge
    float a = smoothstep(1.0, 0.0, d);       // soft round mask
    outColor = vec4(vColor.rgb, vColor.a * a);
}
