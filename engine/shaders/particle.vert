#version 450
// Billboard particle: expand a quad around its world-space center along the camera's right/up axes,
// so it always faces the camera.

layout(location = 0) in vec3 inCenter;
layout(location = 1) in vec2 inCorner; // offset along right/up, pre-scaled by size
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec4 inColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
    vec4 right; // camera right in world space
    vec4 up;    // camera up in world space
} pc;

layout(location = 0) out vec2 vUV;
layout(location = 1) out vec4 vColor;

void main() {
    vec3 world = inCenter + pc.right.xyz * inCorner.x + pc.up.xyz * inCorner.y;
    gl_Position = pc.viewProj * vec4(world, 1.0);
    vUV = inUV;
    vColor = inColor;
}
