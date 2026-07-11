#version 450
// Texture x vertex color, directional (Lambert) lighting + ambient, modulated by a shadow map.

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec4 vLightPos;
layout(location = 4) in vec3 vWorldPos;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 material0; // rgb = emissive (self-illumination, feeds bloom), w = roughness
    vec4 material1; // x = specular strength
} pc;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 1, binding = 0) uniform sampler2D uShadow;
layout(set = 3, binding = 0) uniform sampler2D uNormalMap;

// Per-frame scene UBO (set = 2): camera/light matrices + ambient + one directional "sun"
// (shadow-mapped) + up to 8 point lights. Shared by the vertex and fragment stages.
struct PointLight {
    vec4 posRange; // xyz = world position, w = range
    vec4 color;    // rgb = color * intensity, w = cos(inner cone) for spots, < -1.5 for omni
    vec4 spot;     // xyz = spot axis (normalized), w = cos(outer cone)
};
layout(set = 2, binding = 0) uniform Scene {
    mat4 viewProj;
    mat4 lightVP;
    vec4 camPos;   // xyz = world-space camera position
    vec4 ambient;  // rgb = ambient, w = active point-light count
    vec4 sunDir;   // xyz = direction toward the sun
    vec4 sunColor; // rgb = directional color
    vec4 fog;      // rgb = fog color, w = density (0 disables fog)
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
    // 5x5 percentage-closer filtering (25 taps, 1.5-texel spread) for a soft shadow penumbra.
    float shadow = 0.0;
    vec2 texel = 1.5 / vec2(textureSize(uShadow, 0));
    const int R = 2;
    for (int y = -R; y <= R; ++y) {
        for (int x = -R; x <= R; ++x) {
            float closest = texture(uShadow, uv + vec2(x, y) * texel).r;
            shadow += (p.z - bias > closest) ? 0.35 : 1.0;
        }
    }
    return shadow / float((2 * R + 1) * (2 * R + 1));
}

// Perturb the geometric normal by the tangent-space normal map, building the TBN frame from
// screen-space derivatives (no per-vertex tangents needed). A neutral (0,0,1) map is identity.
vec3 perturbNormal(vec3 N, vec3 worldPos, vec2 uv) {
    vec3 dp1 = dFdx(worldPos);
    vec3 dp2 = dFdy(worldPos);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    mat3 TBN = mat3(T * invmax, B * invmax, N);
    vec3 tsn = texture(uNormalMap, uv).xyz * 2.0 - 1.0;
    return normalize(TBN * tsn);
}

void main() {
    vec3 N = perturbNormal(normalize(vNormal), vWorldPos, vUV);
    vec3 albedo = texture(uTexture, vUV).rgb * vColor;

    // Ambient + shadow-mapped directional sun.
    vec3 lit = L.ambient.rgb;
    float ndl = max(dot(N, normalize(L.sunDir.xyz)), 0.0);
    lit += L.sunColor.rgb * ndl * shadowFactor();

    // Point lights: distance attenuation with a smooth range cutoff.
    int count = int(L.ambient.w + 0.5);
    for (int i = 0; i < count; ++i) {
        vec3 toL = L.points[i].posRange.xyz - vWorldPos;
        float dist = length(toL);
        vec3 Ldir = toL / max(dist, 1e-4);
        float range = max(L.points[i].posRange.w, 1e-3);
        float atten = clamp(1.0 - dist / range, 0.0, 1.0);
        atten *= atten;
        float ndl2 = max(dot(N, Ldir), 0.0);

        // Spotlight cone: fade between the outer and inner cone; omni lights skip this.
        float cone = 1.0;
        if (L.points[i].color.w > -1.5) {
            float cosA = dot(-Ldir, normalize(L.points[i].spot.xyz));
            cone = smoothstep(L.points[i].spot.w, L.points[i].color.w, cosA);
        }
        lit += L.points[i].color.rgb * ndl2 * atten * cone;
    }

    vec3 color = albedo * lit + pc.material0.rgb; // self-illumination (feeds bloom)

    // Blinn-Phong specular from the sun, gated by the material's specular strength (material1.x) and
    // roughness (material0.w). specStrength 0 (default) leaves matte meshes unchanged.
    float specStrength = pc.material1.x;
    if (specStrength > 0.0) {
        float roughness = clamp(pc.material0.w, 0.02, 1.0);
        float shininess = mix(4.0, 128.0, 1.0 - roughness);
        vec3 V = normalize(L.camPos.xyz - vWorldPos);
        vec3 Lsun = normalize(L.sunDir.xyz);
        vec3 H = normalize(Lsun + V);
        float spec = pow(max(dot(N, H), 0.0), shininess);
        color += L.sunColor.rgb * spec * specStrength * ndl * shadowFactor();
    }

    // Exponential distance fog: blend toward the fog color with camera distance.
    if (L.fog.w > 0.0) {
        float dist = length(L.camPos.xyz - vWorldPos);
        float f = clamp(1.0 - exp(-L.fog.w * dist), 0.0, 1.0);
        color = mix(color, L.fog.rgb, f);
    }

    outColor = vec4(color, 1.0);
}
