#version 450
// Composite / post-process: sample the resolved HDR scene color, add the precomputed (downsampled,
// separably-blurred) bloom, then optionally exposure-scale and ACES-tonemap, and write the swapchain
// image. With bloom strength 0 and tonemap off this is a faithful passthrough of the scene color.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uScene;
layout(set = 0, binding = 1) uniform sampler2D uBloom;
layout(set = 0, binding = 2) uniform sampler2D uAO;

layout(push_constant) uniform Push {
    vec4 params; // x = bloom strength, y = threshold (unused here), z = exposure, w = tonemap (>0.5)
    vec4 grade;  // x = vignette strength, y = saturation, z = contrast, w = grade enable (>0.5)
    vec4 extra;  // x = chromatic aberration, y = film-grain strength, z = grain time seed,
                 // w = tonemap operator (0 = ACES Narkowicz, 1 = ACES fitted, 2 = AgX)
    vec4 ssao;   // x = SSAO strength (0 = off); the AO texture darkens the scene by this amount
} pc;

layout(location = 0) out vec4 outColor;

// --- Tonemap operators (all return display-linear; the sRGB swapchain applies the OETF) ---

// Narkowicz ACES filmic: a cheap curve fit, maps unbounded HDR into [0,1] with a pleasing shoulder.
vec3 acesNarkowicz(vec3 x) {
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

// Stephen Hill's fitted ACES (RRT+ODT) with the proper input/output color matrices — more accurate
// than Narkowicz, with less of the highlight hue shift.
vec3 acesFitted(vec3 color) {
    const mat3 inMat = mat3(0.59719, 0.07600, 0.02840,
                            0.35458, 0.90834, 0.13383,
                            0.04823, 0.01566, 0.83777);
    const mat3 outMat = mat3( 1.60475, -0.10208, -0.00327,
                             -0.53108,  1.10813, -0.07276,
                             -0.07367, -0.00605,  1.07602);
    color = inMat * color;
    vec3 a = color * (color + 0.0245786) - 0.000090537;
    vec3 b = color * (0.983729 * color + 0.4329510) + 0.238081;
    color = a / b;
    return clamp(outMat * color, 0.0, 1.0);
}

// AgX (Godot 4.2+'s default): log-encode through an inset matrix, a sigmoid contrast, then the
// outset matrix back to linear. Desaturates highlights gracefully and avoids ACES' notorious hue
// twists (reds toward orange, blues toward purple). Minimal fit after Benjamin Wrensch / Troy Sobotka.
vec3 agxContrast(vec3 x) {
    vec3 x2 = x * x;
    vec3 x4 = x2 * x2;
    return 15.5 * x4 * x2 - 40.14 * x4 * x + 31.96 * x4 - 6.868 * x2 * x
           + 0.4298 * x2 + 0.1191 * x - 0.00232;
}
vec3 agx(vec3 val) {
    const mat3 agxIn = mat3(0.842479062253094, 0.0423282422610123, 0.0423756549057051,
                            0.0784335999999992, 0.878468636469772,  0.0784336,
                            0.0792237451477643, 0.0791661274605434, 0.879142973793104);
    const mat3 agxOut = mat3( 1.19687900512017,   -0.0528968517574562, -0.0529716355144438,
                             -0.0980208811401368,  1.15190312990417,   -0.0980434501171241,
                             -0.0990297440797205, -0.0989611768448433,  1.15107367264116);
    const float minEv = -12.47393, maxEv = 4.026069;
    val = agxIn * val;
    val = clamp(log2(max(val, 1e-10)), minEv, maxEv);
    val = (val - minEv) / (maxEv - minEv);
    val = agxContrast(val);
    val = agxOut * val;
    return clamp(pow(max(val, 0.0), vec3(2.2)), 0.0, 1.0); // AgX EOTF -> display-linear
}

vec3 tonemap(vec3 c, int op) {
    if (op == 2) return agx(c);
    if (op == 1) return acesFitted(c);
    return acesNarkowicz(c);
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
    // Screen-space ambient occlusion: darken the scene by the (blurred) AO factor before adding
    // bloom, so occluded creases go dark but bright bloom still blooms. Strength 0 leaves it untouched.
    float ao = texture(uAO, vUv).r;
    scene *= mix(1.0, ao, pc.ssao.x);

    vec3 bloom = texture(uBloom, vUv).rgb;
    vec3 color = scene + bloom * pc.params.x;

    // Optional HDR tonemap: exposure-scale then the selected operator. Off (w <= 0.5) = untouched.
    if (pc.params.w > 0.5) {
        color = tonemap(color * pc.params.z, int(pc.extra.w + 0.5));
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

    // Optional animated film grain: hashed per-pixel noise that shifts each frame. 0 = off.
    float grain = pc.extra.y;
    if (grain > 0.0) {
        float n = fract(sin(dot(vUv + pc.extra.z, vec2(12.9898, 78.233))) * 43758.5453);
        color += (n - 0.5) * grain;
    }

    outColor = vec4(color, 1.0);
}
