#version 450
// Simple directional (Lambert) lighting + ambient over the interpolated vertex color.

layout(location = 0) in vec3 vNormal;
layout(location = 1) in vec3 vColor;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.4, 0.8, 0.6));
    float diffuse = max(dot(N, L), 0.0);
    float ambient = 0.30;
    outColor = vec4(vColor * (ambient + 0.85 * diffuse), 1.0);
}
