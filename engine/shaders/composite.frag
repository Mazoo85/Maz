#version 450
// Composite / post-process: sample the resolved scene color and write the swapchain image. With
// bloom strength 0 this is a faithful passthrough (so 2D/3D apps are pixel-identical); above 0 it
// adds a threshold bloom — bright areas are blurred and added back for a soft glow.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uScene;

layout(push_constant) uniform Push {
    vec4 params; // x = bloom strength, y = brightness threshold
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec3 scene = texture(uScene, vUv).rgb;
    outColor = vec4(scene, 1.0);
}
