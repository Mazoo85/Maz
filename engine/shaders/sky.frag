#version 450
// Sky background: reconstruct the world-space view ray per pixel from the inverse view-projection
// and shade a vertical gradient (zenith -> horizon -> ground haze) plus a sun glow.

layout(location = 0) in vec2 vNdc;

// Push block kept to 128 bytes (the mobile/MoltenVK maxPushConstantsSize floor): the sun-glow color is
// packed into the unused w channels of the three sky-gradient colors instead of a 5th vec4 (which pushed
// the block to 144 bytes and would fail to create a pipeline on many mobile GPUs).
layout(push_constant) uniform Push {
    mat4 invViewProj;
    vec4 zenith;   // rgb sky color overhead;   w = sunColor.r
    vec4 horizon;  // rgb sky color at horizon;  w = sunColor.g
    vec4 ground;   // rgb below the horizon;     w = sunColor.b
    vec4 sunDir;   // xyz direction toward the sun
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 nearP = pc.invViewProj * vec4(vNdc, 0.0, 1.0);
    vec4 farP = pc.invViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    vec3 sky = dir.y >= 0.0 ? mix(pc.horizon.rgb, pc.zenith.rgb, pow(dir.y, 0.55))
                            : mix(pc.horizon.rgb, pc.ground.rgb, clamp(-dir.y * 3.0, 0.0, 1.0));

    vec3 sunColor = vec3(pc.zenith.w, pc.horizon.w, pc.ground.w);
    vec3 sun = normalize(pc.sunDir.xyz);
    float s = max(dot(dir, sun), 0.0);
    sky += sunColor * (pow(s, 250.0) * 0.9 + pow(s, 8.0) * 0.15);

    outColor = vec4(sky, 1.0);
}
