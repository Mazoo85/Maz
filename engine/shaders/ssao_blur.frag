#version 450
// 4x4 box blur of the raw AO to smear the per-pixel hash noise into a smooth occlusion field.

layout(location = 0) in vec2 vUv;
layout(set = 0, binding = 0) uniform sampler2D uAO;
layout(location = 0) out float outAO;

void main() {
    vec2 texel = 1.0 / vec2(textureSize(uAO, 0));
    float sum = 0.0;
    for (int y = -2; y < 2; ++y) {
        for (int x = -2; x < 2; ++x) {
            sum += texture(uAO, vUv + vec2(x, y) * texel).r;
        }
    }
    outAO = sum / 16.0;
}
