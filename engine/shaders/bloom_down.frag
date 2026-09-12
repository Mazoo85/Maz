#version 450
// Bloom bright-pass + 2x2 box downsample: sample the HDR scene, keep only the part brighter than
// the threshold (soft knee), and write it to the half-res bloom target.

layout(location = 0) in vec2 vUv;

layout(set = 0, binding = 0) uniform sampler2D uSrc;

layout(push_constant) uniform Push {
    vec4 p; // x = brightness threshold
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec2 t = 1.0 / vec2(textureSize(uSrc, 0));
    vec3 c = texture(uSrc, vUv + t * vec2(-0.5, -0.5)).rgb +
             texture(uSrc, vUv + t * vec2( 0.5, -0.5)).rgb +
             texture(uSrc, vUv + t * vec2(-0.5,  0.5)).rgb +
             texture(uSrc, vUv + t * vec2( 0.5,  0.5)).rgb;
    c *= 0.25;
    float b = max(max(c.r, c.g), c.b);
    c *= smoothstep(pc.p.x, pc.p.x + 0.25, b);
    outColor = vec4(c, 1.0);
}
