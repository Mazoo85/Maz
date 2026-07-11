#version 450
// Composite / post-process: sample the resolved scene color and write the swapchain image. With
// bloom strength 0 this is a faithful passthrough (so 2D/3D apps are pixel-identical); above 0 it
// adds a threshold bloom — bright areas are blurred and added back for a soft glow.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uScene;

layout(push_constant) uniform Push {
    vec4 params; // x = bloom strength, y = threshold, z = exposure, w = tonemap enable (>0.5)
} pc;

layout(location = 0) out vec4 outColor;

// Keep only the part of a color brighter than the threshold.
vec3 brightPass(vec3 c, float t) {
    float b = max(max(c.r, c.g), c.b);
    return c * smoothstep(t, t + 0.25, b);
}

// Narkowicz ACES filmic tonemap: maps unbounded HDR into [0,1] with a pleasing shoulder.
vec3 acesTonemap(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
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

    // Optional HDR tonemap: exposure-scale then ACES. Off (w <= 0.5) leaves values untouched, so
    // apps that don't opt in are a faithful passthrough of the (already [0,1]) scene color.
    if (pc.params.w > 0.5) {
        color = acesTonemap(color * pc.params.z);
    }

    outColor = vec4(color, 1.0);
}
