#version 450
// Sky background: reconstruct the world-space view ray per pixel from the inverse view-projection
// and shade a vertical gradient (zenith -> horizon -> ground haze) plus a sun glow.

layout(location = 0) in vec2 vNdc;

layout(push_constant) uniform Push {
    mat4 invViewProj;
} pc;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 nearP = pc.invViewProj * vec4(vNdc, 0.0, 1.0);
    vec4 farP = pc.invViewProj * vec4(vNdc, 1.0, 1.0);
    vec3 dir = normalize(farP.xyz / farP.w - nearP.xyz / nearP.w);

    const vec3 horizon = vec3(0.72, 0.82, 0.95);
    const vec3 zenith = vec3(0.24, 0.44, 0.82);
    const vec3 ground = vec3(0.42, 0.45, 0.50);
    vec3 sky = dir.y >= 0.0 ? mix(horizon, zenith, pow(dir.y, 0.55))
                            : mix(horizon, ground, clamp(-dir.y * 3.0, 0.0, 1.0));

    vec3 sun = normalize(vec3(0.4, 0.8, 0.6));
    float s = max(dot(dir, sun), 0.0);
    sky += vec3(1.0, 0.95, 0.8) * (pow(s, 250.0) * 0.9 + pow(s, 8.0) * 0.15);

    outColor = vec4(sky, 1.0);
}
