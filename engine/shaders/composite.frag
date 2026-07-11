#version 450
// Composite / post-process: sample the resolved HDR scene color, add the precomputed (downsampled,
// separably-blurred) bloom, then optionally exposure-scale and ACES-tonemap, and write the swapchain
// image. With bloom strength 0 and tonemap off this is a faithful passthrough of the scene color.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uScene;
layout(set = 0, binding = 1) uniform sampler2D uBloom;

layout(push_constant) uniform Push {
    vec4 params; // x = bloom strength, y = threshold (unused here), z = exposure, w = tonemap (>0.5)
    vec4 grade;  // x = vignette strength, y = saturation, z = contrast, w = grade enable (>0.5)
    vec4 extra;  // x = chromatic aberration strength (radial RGB split)
} pc;

layout(location = 0) out vec4 outColor;

// Narkowicz ACES filmic tonemap: maps unbounded HDR into [0,1] with a pleasing shoulder.
vec3 acesTonemap(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    // Chromatic aberration: split the R/B channels radially from the center (0 = off = single tap).
    vec3 scene;
    float ca = pc.extra.x;
    if (ca > 0.0) {
        vec2 dir = vUv - 0.5;
        scene.r = texture(uScene, vUv + dir * ca).r;
        scene.g = texture(uScene, vUv).g;
        scene.b = texture(uScene, vUv - dir * ca).b;
    } else {
        scene = texture(uScene, vUv).rgb;
    }
    vec3 bloom = texture(uBloom, vUv).rgb;
    vec3 color = scene + bloom * pc.params.x;

    // Optional HDR tonemap: exposure-scale then ACES. Off (w <= 0.5) leaves values untouched.
    if (pc.params.w > 0.5) {
        color = acesTonemap(color * pc.params.z);
    }

    // Optional color grade: saturation, contrast, and a soft vignette. Off (w <= 0.5) = untouched.
    if (pc.grade.w > 0.5) {
        float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
        color = mix(vec3(luma), color, pc.grade.y);            // saturation
        color = clamp((color - 0.5) * pc.grade.z + 0.5, 0.0, 1.0); // contrast about mid-grey
        vec2 d = vUv - 0.5;
        float vig = 1.0 - pc.grade.x * smoothstep(0.15, 0.75, dot(d, d) * 2.0);
        color *= vig;                                          // darken toward the corners
    }

    outColor = vec4(color, 1.0);
}
