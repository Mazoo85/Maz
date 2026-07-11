#version 450
// Texture x vertex color, lit by a directional (Lambert) term + ambient.

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;
layout(location = 2) in vec2 vUV;

layout(set = 0, binding = 0) uniform sampler2D uTexture;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(N, L), 0.0);
    float ambient = 0.30;
    vec3 albedo = texture(uTexture, vUV).rgb * vColor;
    outColor = vec4(albedo * (ambient + 0.85 * diffuse), 1.0);
}
