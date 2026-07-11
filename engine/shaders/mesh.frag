#version 450
// Texture x vertex color, directional (Lambert) lighting + ambient, modulated by a shadow map.

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec4 vLightPos;

layout(set = 0, binding = 0) uniform sampler2D uTexture;
layout(set = 1, binding = 0) uniform sampler2D uShadow;

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
    vec3 L = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(N, L), 0.0);
    float ambient = 0.30;
    vec3 albedo = texture(uTexture, vUV).rgb * vColor;
    outColor = vec4(albedo * (ambient + 0.85 * diffuse * shadowFactor()), 1.0);
}
