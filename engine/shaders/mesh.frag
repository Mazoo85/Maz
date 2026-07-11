#version 450
// Texture x vertex color, directional (Lambert) lighting + ambient, modulated by a shadow map.

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec4 vLightPos;
layout(location = 4) in vec3 vWorldPos;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 1, binding = 0) uniform sampler2D uShadow;

// Scene lighting (set = 2): ambient + one directional "sun" (shadow-mapped) + up to 8 point lights.
struct PointLight {
    vec4 posRange; // xyz = world position, w = range
    vec4 color;    // rgb = color * intensity
};
layout(set = 2, binding = 0) uniform Lights {
    vec4 ambient;  // rgb = ambient, w = active point-light count
    vec4 sunDir;   // xyz = direction toward the sun
    vec4 sunColor; // rgb = directional color
    PointLight points[8];
} L;

layout(location = 0) out vec4 outColor;

float shadowFactor() {
    vec3 p = vLightPos.xyz / vLightPos.w;      // light-space NDC (z in [0,1] on Vulkan)
    vec2 uv = vec2(p.x * 0.5 + 0.5, p.y * 0.5 + 0.5);
    if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0 || p.z > 1.0) {
        return 1.0; // outside the shadow map => lit
    }
    const float bias = 0.0025;
    // 2x2 percentage-closer filtering for softer edges.
    float shadow = 0.0;
    vec2 texel = 1.0 / vec2(textureSize(uShadow, 0));
    for (int y = 0; y <= 1; ++y) {
        for (int x = 0; x <= 1; ++x) {
            float closest = texture(uShadow, uv + vec2(x, y) * texel).r;
            shadow += (p.z - bias > closest) ? 0.35 : 1.0;
        }
    }
    return shadow * 0.25;
}

void main() {
    vec3 N = normalize(vNormal);
    vec3 albedo = texture(uTexture, vUV).rgb * vColor;

    // Ambient + shadow-mapped directional sun.
    vec3 lit = L.ambient.rgb;
    float ndl = max(dot(N, normalize(L.sunDir.xyz)), 0.0);
    lit += L.sunColor.rgb * ndl * shadowFactor();

    // Point lights: distance attenuation with a smooth range cutoff.
    int count = int(L.ambient.w + 0.5);
    for (int i = 0; i < count; ++i) {
        vec3 d = L.points[i].posRange.xyz - vWorldPos;
        float dist = length(d);
        float range = max(L.points[i].posRange.w, 1e-3);
        float atten = clamp(1.0 - dist / range, 0.0, 1.0);
        atten *= atten;
        float ndl2 = max(dot(N, d / max(dist, 1e-4)), 0.0);
        lit += L.points[i].color.rgb * ndl2 * atten;
    }

    outColor = vec4(albedo * lit, 1.0);
}
