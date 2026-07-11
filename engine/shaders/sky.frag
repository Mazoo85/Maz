#version 450
// Sky background: reconstruct the world-space view ray per pixel from the inverse view-projection
// and shade a vertical gradient (zenith -> horizon -> ground haze) plus a sun glow.

layout(location = 0) in vec2 vNdc;

layout(push_constant) uniform Push {
    mat4 invViewProj;
    vec4 zenith;   // rgb sky color overhead
    vec4 horizon;  // rgb sky color at the horizon
    vec4 ground;   // rgb below the horizon
    vec4 sunDir;   // xyz direction toward the sun
    vec4 sunColor; // rgb sun-glow color
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 nearP = pc.invViewProj * vec4(vNdc, 0.0, 1.0);
    vec4 farP = pc.invViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    vec3 sky = dir.y >= 0.0 ? mix(pc.horizon.rgb, pc.zenith.rgb, pow(dir.y, 0.55))
                            : mix(pc.horizon.rgb, pc.ground.rgb, clamp(-dir.y * 3.0, 0.0, 1.0));

    vec3 sun = normalize(pc.sunDir.xyz);
    float s = max(dot(dir, sun), 0.0);
    sky += pc.sunColor.rgb * (pow(s, 250.0) * 0.9 + pow(s, 8.0) * 0.15);

    outColor = vec4(sky, 1.0);
}
