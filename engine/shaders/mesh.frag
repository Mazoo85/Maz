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
    vec4 material1; // x = specular strength, y = opacity (1 = opaque), z = metallic (0 = dielectric)
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
    vec4 skyZenith;  // sky gradient (mirrors the sky pass) for analytic image-based lighting
    vec4 skyHorizon;
    vec4 skyGround;
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

// Cook-Torrance microfacet specular for one light direction Ldir, view V, normal N.
// GGX normal distribution (D), Schlick-GGX geometry (G), Fresnel-Schlick (F). rough shapes the
// highlight; F0 is the reflectance at normal incidence (0.04 dielectric, albedo for metals).
vec3 cookTorrance(vec3 N, vec3 V, vec3 Ldir, float rough, vec3 F0) {
    vec3 H = normalize(Ldir + V);
    float NdV = max(dot(N, V), 1e-4);
    float NdL = max(dot(N, Ldir), 0.0);
    float NdH = max(dot(N, H), 0.0);
    float VdH = max(dot(V, H), 0.0);
    float a = rough * rough;
    float a2 = a * a;
    float dGGX = (NdH * NdH) * (a2 - 1.0) + 1.0;
    float D = a2 / (3.14159265 * dGGX * dGGX);            // GGX normal distribution
    float k = (rough + 1.0) * (rough + 1.0) / 8.0;        // Schlick-GGX geometry
    float gv = NdV / (NdV * (1.0 - k) + k);
    float gl = NdL / (NdL * (1.0 - k) + k);
    float G = gv * gl;
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdH, 5.0);       // Fresnel-Schlick
    return (D * G) * F / max(4.0 * NdV * NdL, 1e-4);      // Cook-Torrance
}

// The analytic sky gradient, matching the skybox pass so reflections agree with the drawn sky.
vec3 skyColor(vec3 dir) {
    return dir.y >= 0.0 ? mix(L.skyHorizon.rgb, L.skyZenith.rgb, pow(dir.y, 0.55))
                        : mix(L.skyHorizon.rgb, L.skyGround.rgb, clamp(-dir.y * 3.0, 0.0, 1.0));
}

// Diffuse irradiance from the sky: a hemispheric average biased toward the sky in the normal's
// direction — cheap stand-in for a convolved irradiance map. Returns the ambient radiance for N.
vec3 skyIrradiance(vec3 N) {
    vec3 avg = (L.skyZenith.rgb + L.skyHorizon.rgb + L.skyGround.rgb) * (1.0 / 3.0);
    return mix(avg, skyColor(N), 0.5);
}

// Karis' analytic environment-BRDF approximation (mobile) — the split-sum scale+bias for F0
// without a precomputed LUT. x scales F0, y is the additive bias.
vec2 envBRDFApprox(float NdV, float rough) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = rough * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

void main() {
    vec3 N = perturbNormal(normalize(vNormal), vWorldPos, vUV);
    vec3 albedo = texture(uTexture, vUV).rgb * vColor;

    // Metallic/roughness PBR parameters. When the material opts into specular (material1.x > 0) we
    // run the full metallic/roughness workflow: metals lose their diffuse albedo (energy is spent on
    // the reflection) and the Fresnel F0 tints toward the albedo. specStrength 0 (matte) leaves the
    // classic Lambert diffuse untouched, so existing scenes are unchanged.
    float specStrength = pc.material1.x;
    bool pbr = specStrength > 0.0;
    float rough = clamp(pc.material0.w, 0.045, 1.0);
    float metallic = clamp(pc.material1.z, 0.0, 1.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 V = normalize(L.camPos.xyz - vWorldPos);
    // Energy conservation: metals have (almost) no diffuse. Only applied on PBR materials.
    vec3 kd = pbr ? albedo * (1.0 - metallic) : albedo;

    // Ambient + shadow-mapped directional sun (diffuse + PBR specular).
    vec3 sunDir = normalize(L.sunDir.xyz);
    float ndl = max(dot(N, sunDir), 0.0);
    float sunShadow = shadowFactor();
    // Ambient diffuse: matte materials keep the flat scene ambient; PBR materials draw their
    // ambient from the sky (analytic image-based lighting) so surfaces pick up the environment color.
    vec3 ambientDiffuse = pbr ? skyIrradiance(N) : L.ambient.rgb;
    vec3 color = kd * ambientDiffuse;
    if (pbr) {
        // Specular image-based lighting: reflect the view about N, sample the sky in that direction
        // (blurred toward the diffuse irradiance as roughness rises — a stand-in for a prefiltered
        // environment), and weight by the analytic environment BRDF. This is what lets metals mirror
        // the sky instead of rendering black.
        vec3 Refl = reflect(-V, N);
        float NdVamb = max(dot(N, V), 1e-4);
        vec3 envSpec = mix(skyColor(Refl), skyIrradiance(N), rough);
        vec2 ab = envBRDFApprox(NdVamb, rough);
        color += envSpec * (F0 * ab.x + ab.y);
    }
    color += kd * L.sunColor.rgb * ndl * sunShadow;
    if (pbr) {
        color += L.sunColor.rgb * cookTorrance(N, V, sunDir, rough, F0)
                 * specStrength * ndl * sunShadow;
    }

    // Point / spot lights: distance attenuation with a smooth range cutoff, diffuse + PBR specular.
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
        vec3 radiance = L.points[i].color.rgb * atten * cone;
        color += kd * radiance * ndl2;
        if (pbr) {
            color += radiance * cookTorrance(N, V, Ldir, rough, F0) * specStrength * ndl2;
        }
    }

    color += pc.material0.rgb; // self-illumination (feeds bloom)

    // Exponential distance fog: blend toward the fog color with camera distance.
    if (L.fog.w > 0.0) {
        float dist = length(L.camPos.xyz - vWorldPos);
        float f = clamp(1.0 - exp(-L.fog.w * dist), 0.0, 1.0);
        color = mix(color, L.fog.rgb, f);
    }

    outColor = vec4(color, pc.material1.y); // material1.y = opacity (1 for opaque draws)
}
