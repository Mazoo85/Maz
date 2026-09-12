#version 450
// Screen-space ambient occlusion. Reconstructs world position from the camera depth prepass (via the
// inverse view-projection, matching the sky pass), derives a face normal from neighbouring positions,
// samples a hemisphere of points around each pixel, and counts how many are occluded by nearer
// geometry. Output is a single-channel occlusion factor in [0,1] (1 = fully lit, 0 = fully occluded)
// that the composite pass multiplies into the scene. A range check prevents haloing across depth
// discontinuities. Kernel directions are hashed per-pixel (interleaved-gradient style) then blurred.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uDepth;
layout(set = 0, binding = 1) uniform Cam {
    mat4 viewProj;
    mat4 invViewProj;
    vec4 camPos;   // xyz world-space camera position
    vec4 params;   // x = radius (world units), y = strength, z = bias, w = power
} C;

layout(location = 0) out float outAO;

vec3 worldFromDepth(vec2 uv, float d) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, d, 1.0);
    vec4 w = C.invViewProj * ndc;
    return w.xyz / w.w;
}
float hash12(vec2 p) {
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}
vec3 hash31(float n) {
    return fract(sin(vec3(n, n + 1.7, n + 3.4)) * vec3(43758.5453, 22578.145, 19642.349));
}

void main() {
    float d = texture(uDepth, vUv).r;
    if (d >= 1.0) {            // sky / cleared depth => never occluded
        outAO = 1.0;
        return;
    }
    vec3 P = worldFromDepth(vUv, d);
    vec2 texel = 1.0 / vec2(textureSize(uDepth, 0));
    // Face normal from depth: for each axis take whichever neighbour is closer in depth (so we don't
    // straddle a silhouette edge), cross the two world-space derivatives, then force the normal to
    // face the camera — a robust fix for winding/orientation on any surface.
    float dl = texture(uDepth, vUv - vec2(texel.x, 0.0)).r;
    float dr = texture(uDepth, vUv + vec2(texel.x, 0.0)).r;
    float dd = texture(uDepth, vUv - vec2(0.0, texel.y)).r;
    float du = texture(uDepth, vUv + vec2(0.0, texel.y)).r;
    vec3 hDeriv = abs(dr - d) < abs(d - dl) ? worldFromDepth(vUv + vec2(texel.x, 0.0), dr) - P
                                            : P - worldFromDepth(vUv - vec2(texel.x, 0.0), dl);
    vec3 vDeriv = abs(du - d) < abs(d - dd) ? worldFromDepth(vUv + vec2(0.0, texel.y), du) - P
                                            : P - worldFromDepth(vUv - vec2(0.0, texel.y), dd);
    vec3 N = normalize(cross(hDeriv, vDeriv));
    vec3 V = normalize(C.camPos.xyz - P);
    if (dot(N, V) < 0.0) N = -N;

    float radius = C.params.x;
    float bias = C.params.z;
    vec3 origin = P + N * bias; // lift off the surface to avoid self-occlusion acne
    float rot = hash12(gl_FragCoord.xy) * 6.2831853;
    mat2 rm = mat2(cos(rot), -sin(rot), sin(rot), cos(rot));

    const int K = 16;
    float occ = 0.0;
    for (int i = 0; i < K; ++i) {
        // A hemisphere direction: random vector flipped into the normal's hemisphere, weighted so
        // samples cluster near the origin (a = i/K distributes radii).
        vec3 r3 = hash31(float(i) * 1.7 + 0.5) * 2.0 - 1.0;
        r3.xy = rm * r3.xy;                       // per-pixel rotation to decorrelate
        r3 = normalize(r3) * (dot(r3, N) < 0.0 ? -1.0 : 1.0);
        float t = float(i) / float(K);
        vec3 samplePos = origin + r3 * radius * mix(0.15, 1.0, t * t);

        vec4 clip = C.viewProj * vec4(samplePos, 1.0);
        if (clip.w <= 0.0) continue;
        vec2 su = (clip.xy / clip.w) * 0.5 + 0.5;
        if (su.x < 0.0 || su.x > 1.0 || su.y < 0.0 || su.y > 1.0) continue;

        float sd = texture(uDepth, su).r;
        if (sd >= 1.0) continue;
        vec3 storedPos = worldFromDepth(su, sd);
        float sampleDist = distance(C.camPos.xyz, samplePos);
        float storedDist = distance(C.camPos.xyz, storedPos);
        // Occluded when real geometry at that pixel is closer to the camera than the sample point;
        // only count occluders within `radius` of P so distant surfaces behind a gap don't darken it.
        float rangeCheck = smoothstep(0.0, 1.0, radius / max(distance(P, storedPos), 1e-4));
        occ += (storedDist < sampleDist - bias ? 1.0 : 0.0) * rangeCheck;
    }
    float ao = 1.0 - (occ / float(K)) * C.params.y;
    outAO = clamp(pow(clamp(ao, 0.0, 1.0), C.params.w), 0.0, 1.0);
}
