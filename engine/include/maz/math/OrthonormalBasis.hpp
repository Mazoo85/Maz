#pragma once

#include "maz/math/Math.hpp" // vec3, cross, dot

#include <cmath>

// maz::math orthonormal basis from a single direction — given a unit normal, build a full right-handed frame
// (tangent, bitangent, normal) with no branches and no trig. This is the glue between the engine's sampling
// warps (Sampling.hpp emits directions in a +Z-up LOCAL frame — cosine/uniform hemisphere, disk, sphere) and
// the WORLD: to scatter AO/GI rays over a surface, jitter a disk light, orient a decal or an impostor
// billboard, or spawn particles across a face, you need a consistent tangent frame around that surface's
// normal, and `fromLocal` rotates a local sample into it. Uses Duff, Cigolle, Tokuyoshi, Harada & Genk's
// 2017 "Building an Orthonormal Basis, Revisited" — numerically stable across the whole sphere (the naive
// cross-with-a-fixed-axis method degenerates when the normal nears that axis). Header-only, deterministic.
namespace maz::math {

struct Basis3 {
    vec3 tangent{1.0f, 0.0f, 0.0f};
    vec3 bitangent{0.0f, 1.0f, 0.0f};
    vec3 normal{0.0f, 0.0f, 1.0f};
};

// Right-handed orthonormal frame around unit `n` (cross(tangent, bitangent) == normal). `n` is assumed
// normalized; a near-zero vector falls back to the canonical axis frame.
inline Basis3 orthonormalBasis(const vec3& n) {
    Basis3 b;
    if (dot(n, n) < 1e-24f) {
        return b; // degenerate input: canonical x/y/z frame
    }
    b.normal = n;
    const float s = std::copysign(1.0f, n.z); // +1 for n.z >= 0, -1 otherwise
    const float a = -1.0f / (s + n.z);
    const float d = n.x * n.y * a;
    b.tangent = vec3(1.0f + s * n.x * n.x * a, s * d, -s * n.x);
    b.bitangent = vec3(d, s + n.y * n.y * a, -n.y);
    return b;
}

// Rotate a direction expressed in the LOCAL frame (x=tangent, y=bitangent, z=normal) into world space.
// A local +Z maps to the normal; length is preserved (the basis is orthonormal). This is what turns a
// Sampling.hpp hemisphere/disk sample (z-up) into a world-space direction around a surface normal.
inline vec3 fromLocal(const Basis3& b, const vec3& local) {
    return b.tangent * local.x + b.bitangent * local.y + b.normal * local.z;
}

} // namespace maz::math
