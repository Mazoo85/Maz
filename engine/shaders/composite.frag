#version 450
// Composite / post-process: sample the resolved scene color and write the swapchain image. With
// bloom strength 0 this is a faithful passthrough (so 2D/3D apps are pixel-identical); above 0 it
// adds a threshold bloom — bright areas are blurred and added back for a soft glow.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uScene;

layout(push_constant) uniform Push {
    vec4 params; // x = bloom strength, y = brightness threshold
} pc;

layout(location = 0) out vec4 outColor;

// Keep only the part of a color brighter than the threshold.
vec3 brightPass(vec3 c, float t) {
    float b = max(max(c.r, c.g), c.b);
    return c * smoothstep(t, t + 0.25, b);
}

void main() {
    vec3 scene = texture(uScene, vUv).rgb;
    vec3 color = scene;

    float strength = pc.params.x;
    if (strength > 0.0) {
        // Gaussian-weighted bright blur (7x7 taps at 3-texel spacing) added back for a soft glow.
        vec2 texel = 1.0 / vec2(textureSize(uScene, 0));
        vec3 bloom = vec3(0.0);
        float wsum = 0.0;
        const int R = 3;
        for (int j = -R; j <= R; ++j) {
            for (int i = -R; i <= R; ++i) {
                vec2 off = vec2(float(i), float(j)) * texel * 3.0;
                float w = exp(-float(i * i + j * j) / 6.0);
                bloom += brightPass(texture(uScene, vUv + off).rgb, pc.params.y) * w;
                wsum += w;
            }
        }
        color = scene + (bloom / wsum) * strength;
    }

    outColor = vec4(color, 1.0);
}
