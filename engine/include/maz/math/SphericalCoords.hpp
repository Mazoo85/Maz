#pragma once

#include "maz/math/Math.hpp" // vec2, vec3

#include <cmath>

// maz::math spherical coordinates — the conversion every orbit camera, sky sampler, and directional-light
// widget needs but that raw GLM does not spell: a 3D point as (radius, azimuth, elevation) and back. GLM
// gives dot/cross/normalize; it has no notion of "put the camera on a sphere around the target at this yaw
// and pitch" or "which pixel of the sky does this ray hit". Those are exactly the everyday jobs here.
//
// Convention (right-handed, +Y up, matching the engine's Camera3D and MeshUvRadial):
//   * radius    r >= 0            — distance from the origin.
//   * azimuth   theta             — yaw around +Y, measured in the XZ-plane from +Z toward +X. 0 faces +Z.
//   * elevation phi in [-pi/2,pi/2] — pitch up from the XZ-plane toward +Y. +pi/2 is straight up (+Y).
// So sphericalToCartesian(1, 0, 0) == (0,0,1) [+Z forward], (1, pi/2, 0) == (1,0,0) [+X right],
// (1, 0, pi/2) == (0,1,0) [+Y up]. cartesianToSpherical is its exact inverse (up to the r==0 / pole
// degeneracy where azimuth is arbitrary). Header-only, pure, deterministic.
namespace maz::math {

struct Spherical {
    float radius = 0.0f;
    float azimuth = 0.0f;   // radians, yaw around +Y from +Z toward +X
    float elevation = 0.0f; // radians, pitch from the XZ-plane toward +Y, in [-pi/2, pi/2]
};

// (radius, azimuth, elevation) -> Cartesian (x, y, z), using the convention documented above.
inline vec3 sphericalToCartesian(float radius, float azimuth, float elevation) {
    const float cosE = std::cos(elevation);
    return vec3(radius * cosE * std::sin(azimuth), // x
                radius * std::sin(elevation),       // y (up)
                radius * cosE * std::cos(azimuth)); // z (forward at azimuth 0)
}

inline vec3 sphericalToCartesian(const Spherical& s) {
    return sphericalToCartesian(s.radius, s.azimuth, s.elevation);
}

// Cartesian (x, y, z) -> (radius, azimuth, elevation). The exact inverse of sphericalToCartesian. At the
// origin, radius is 0 and the angles are 0; at a pole (x==z==0) azimuth is 0 (it is undefined there).
inline Spherical cartesianToSpherical(const vec3& p) {
    Spherical s;
    s.radius = std::sqrt(p.x * p.x + p.y * p.y + p.z * p.z);
    if (s.radius <= 0.0f) return s; // origin: angles left at 0.
    // Clamp the argument so a rounding overshoot past +/-1 can't feed asin a NaN.
    float sinE = p.y / s.radius;
    sinE = sinE < -1.0f ? -1.0f : (sinE > 1.0f ? 1.0f : sinE);
    s.elevation = std::asin(sinE);
    s.azimuth = std::atan2(p.x, p.z); // +Z -> 0, +X -> +pi/2, matching the forward convention.
    return s;
}

// Orbit-camera helper: the world position of an eye orbiting `target` at `radius`, `azimuth`, `elevation`.
// Point the camera at `target` (look direction = target - result) for a standard turntable/orbit camera.
inline vec3 orbitPosition(const vec3& target, float radius, float azimuth, float elevation) {
    return target + sphericalToCartesian(radius, azimuth, elevation);
}

// Equirectangular (lat-long) mapping for sampling a panoramic sky / HDRI from a view direction.
// `dir` need not be normalized. Returns UV in [0,1]x[0,1] with the image's top row (v=0) at straight up
// (+Y) and the seam (u=0 and u=1) behind at -Z. Inverse of equirectUVToDirection. This maps a single ray
// to a texel — distinct from MeshUvRadial, which assigns per-vertex UVs to a whole mesh.
inline vec2 directionToEquirectUV(const vec3& dir) {
    const Spherical s = cartesianToSpherical(dir);
    const float pi = 3.14159265358979323846f;
    const float u = s.azimuth / (2.0f * pi) + 0.5f;       // azimuth in [-pi,pi] -> [0,1]
    const float v = (pi * 0.5f - s.elevation) / pi;       // +pi/2 (up) -> 0 (top), -pi/2 -> 1 (bottom)
    return vec2(u, v);
}

// Inverse of directionToEquirectUV: a UV in [0,1]^2 back to a unit direction.
inline vec3 equirectUVToDirection(float u, float v) {
    const float pi = 3.14159265358979323846f;
    const float azimuth = (u - 0.5f) * 2.0f * pi;
    const float elevation = pi * 0.5f - v * pi;
    return sphericalToCartesian(1.0f, azimuth, elevation);
}

} // namespace maz::math
