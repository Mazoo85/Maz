#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <vector>

// maz::game WEAPON SPREAD — perturb an aim direction into a cone/fan for shotgun pellets, bullet inaccuracy, spray
// weapons, and particle emission. `spreadDirection2D`/`spreadDirection3D` jitter a direction randomly within a
// half-angle (using any RNG with `rangef(lo,hi)`, e.g. `core::Pcg32`); `spreadFan2D` returns a DETERMINISTIC,
// evenly-spaced fan of directions for a fixed multi-pellet pattern. 3D sampling is uniform over the cone's solid
// angle (no clustering at the axis). Godot has no spread helper, so this is a beyond-Godot gameplay utility.
// Header-only, deterministic (randomness lives in the caller's seeded RNG). Pass unit directions in.
namespace maz::game {

// 2D: rotate `dir` by a random angle in [-halfAngle, halfAngle] (radians). Preserves the input's length.
template <typename Rng>
inline math::vec2 spreadDirection2D(const math::vec2& dir, float halfAngle, Rng& rng) {
    const float a = rng.rangef(-halfAngle, halfAngle);
    const float c = std::cos(a), s = std::sin(a);
    return math::vec2(dir.x * c - dir.y * s, dir.x * s + dir.y * c);
}

// 2D: `count` directions fanned evenly across the full spread [-halfAngle, +halfAngle]. A deterministic shotgun
// pattern — `count` 1 returns just `dir`; odd counts include `dir` dead-centre. `count` < 1 → empty.
inline std::vector<math::vec2> spreadFan2D(const math::vec2& dir, float halfAngle, int count) {
    std::vector<math::vec2> out;
    if (count < 1) return out;
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float t = (count == 1) ? 0.0f
                                     : (2.0f * static_cast<float>(i) / static_cast<float>(count - 1) - 1.0f); // -1..1
        const float a = t * halfAngle;
        const float c = std::cos(a), s = std::sin(a);
        out.push_back(math::vec2(dir.x * c - dir.y * s, dir.x * s + dir.y * c));
    }
    return out;
}

// 3D: a random unit direction inside a cone of half-angle `coneHalfAngle` (radians) around `axis`, distributed
// uniformly over the cone's solid angle. `coneHalfAngle` 0 returns the axis exactly.
template <typename Rng>
inline math::vec3 spreadDirection3D(const math::vec3& axis, float coneHalfAngle, Rng& rng) {
    const math::vec3 n = math::normalize(axis);
    const float cosT = rng.rangef(std::cos(coneHalfAngle), 1.0f); // uniform in cos -> uniform solid angle
    const float sinT = std::sqrt(cosT * cosT < 1.0f ? 1.0f - cosT * cosT : 0.0f);
    const float phi = rng.rangef(0.0f, 6.28318530717958647692f);
    // An orthonormal basis (t, b, n) around the axis.
    const math::vec3 ref = std::fabs(n.x) < 0.9f ? math::vec3(1, 0, 0) : math::vec3(0, 1, 0);
    const math::vec3 t = math::normalize(math::cross(n, ref));
    const math::vec3 b = math::cross(n, t);
    return sinT * std::cos(phi) * t + sinT * std::sin(phi) * b + cosT * n;
}

} // namespace maz::game
