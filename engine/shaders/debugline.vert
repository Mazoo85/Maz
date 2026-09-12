#version 450
// Debug line vertex: transform a world-space position by viewProj and pass the color through.
// Used by the DebugDraw renderer for world-space lines, AABBs, and gizmos.

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec4 inColor;

layout(push_constant) uniform Push {
    mat4 viewProj;
} pc;

layout(location = 0) out vec4 vColor;

void main() {
    gl_Position = pc.viewProj * vec4(inPos, 1.0);
    vColor = inColor;
}
