#version 450
// Simple hemispherical + directional shading so the mesh reads as a 3D form.

layout(location = 0) in vec3 vNormal;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(0.4, 0.9, 0.5));   // key light direction
    float diffuse = max(dot(N, L), 0.0);
    float ambient = 0.25;                       // fill so back faces aren't pure black

    vec3 baseColor = vec3(0.85, 0.35, 0.25);    // warm clay
    vec3 color = baseColor * (ambient + (1.0 - ambient) * diffuse);
    outColor = vec4(color, 1.0);
}
