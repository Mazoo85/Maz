#version 450
// Animated gradient over the fullscreen triangle. Scaffolding for the Phase 3 pipeline.

layout(location = 0) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(push_constant) uniform Push {
    float time;
} pc;

void main() {
    vec3 c = 0.5 + 0.5 * cos(pc.time + vUV.xyx + vec3(0.0, 2.0, 4.0));
    outColor = vec4(c, 1.0);
}
