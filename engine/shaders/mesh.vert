#version 450
// 3D mesh vertex shader. Per-frame camera/light matrices come from the scene UBO (set 2); only the
// per-draw model matrix and material live in the push constant, keeping it well under the 128-byte
// guaranteed push-constant limit.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inUV;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 material0; // rgb = emissive, w = roughness
    vec4 material1; // x = specular strength
} pc;

struct PointLight {
    vec4 posRange;
    vec4 color;
    vec4 spot;
};
layout(set = 2, binding = 0) uniform Scene {
    mat4 viewProj; // camera view*projection for this frame
    mat4 lightVP;  // directional light's view*projection (for shadow lookup)
    vec4 camPos;   // world-space camera position
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
    vec4 world = pc.model * vec4(inPos, 1.0);
    gl_Position = S.viewProj * world;
    vNormal = mat3(pc.model) * inNormal;
    vColor = inColor;
    vUV = inUV;
    vLightPos = S.lightVP * world;
    vWorldPos = world.xyz;
}
