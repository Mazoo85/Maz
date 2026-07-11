#version 450
// Separable 9-tap Gaussian blur. Run once horizontally then once vertically; the direction is the
// per-pixel UV step passed in the push constant.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uSrc;

layout(push_constant) uniform Push {
    vec4 p; // xy = UV step along the blur axis
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    const float w[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    vec2 d = pc.p.xy;
    vec3 c = texture(uSrc, vUv).rgb * w[0];
    for (int i = 1; i < 5; ++i) {
        c += texture(uSrc, vUv + d * float(i)).rgb * w[i];
        c += texture(uSrc, vUv - d * float(i)).rgb * w[i];
    }
    outColor = vec4(c, 1.0);
}
