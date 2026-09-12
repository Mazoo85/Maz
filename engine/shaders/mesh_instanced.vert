#version 450
// Instanced 3D mesh vertex shader: identical to mesh.vert but the model matrix comes from four
// per-instance vertex attributes (binding 1) instead of the push constant, so hundreds of copies of
// one mesh render in a single draw call. The fragment shader (mesh.frag) is shared.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;
// Per-instance model matrix (binding 1), one mat4 as four vec4 rows.
layout(location = 4) in vec4 inModel0;
layout(location = 5) in vec4 inModel1;
layout(location = 6) in vec4 inModel2;
layout(location = 7) in vec4 inModel3;

layout(push_constant) uniform Push {
    mat4 model;     // unused here (the instance provides its own); kept for layout compatibility
    vec4 material0; // rgb = emissive, w = roughness
    vec4 material1; // x = specular strength
} pc;

struct PointLight {
    vec4 posRange;
    vec4 color;
    vec4 spot;
};
layout(set = 2, binding = 0) uniform Scene {
    mat4 viewProj;
    mat4 lightVP;
    vec4 camPos;
    vec4 ambient;
    vec4 sunDir;
    vec4 sunColor;
    vec4 fog;
    PointLight points[8];
} S;

layout(location = 0) out vec3 vNormal;
layout(location = 1) out vec3 vColor;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec4 vLightPos;
layout(location = 4) out vec3 vWorldPos;

void main() {
    mat4 model = mat4(inModel0, inModel1, inModel2, inModel3);
    vec4 world = model * vec4(inPos, 1.0);
    gl_Position = S.viewProj * world;
    vNormal = mat3(model) * inNormal;
    vColor = inColor;
    vUV = inUV;
    vLightPos = S.lightVP * world;
    vWorldPos = world.xyz;
}
