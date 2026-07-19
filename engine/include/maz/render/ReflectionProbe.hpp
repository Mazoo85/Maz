#pragma once

#include "maz/render/Cubemap.hpp"

#include <cmath>

// maz::render reflection probe influence + box projection — the CPU math behind Godot's ReflectionProbe.
// A probe captures the surroundings into a cubemap inside an axis-aligned box; reflective surfaces in that
// box sample it. Two pieces are pure math and testable without a GPU: (1) `influenceWeight`, how strongly a
// probe affects a point — full inside the box, fading to zero across a blend margin near the faces, so
// overlapping probes cross-fade; and (2) `boxProjectDirection`, the parallax correction that makes a
// box-captured cubemap look right off flat walls — it intersects the reflection ray with the box and
// re-aims the sample from the probe center to that hit point (without this, reflections slide as the camera
// moves). Combined with the M506 cubemap mapping, `probeSample` gives the face/UV a surface should read.
//
// Scope note (honest): axis-aligned box probes, linear face blend, box-projection parallax. It does not
// capture the cubemap (a GPU render pass), prefilter roughness mips, or blend probe *colors* (that is the
// shader compositing the weights); this provides the weights and directions it needs.
namespace maz::render {

struct ReflectionProbe {
    math::vec3 center{0.0f, 0.0f, 0.0f};
    math::vec3 halfExtents{1.0f, 1.0f, 1.0f};
    float blendDistance = 0.0f; // margin near the faces over which influence fades 1 -> 0
};

// How strongly `probe` influences `point`: 1 well inside the box, fading linearly to 0 at the surface across
// `blendDistance`, and 0 outside. With blendDistance <= 0 it is a hard 1 (inside) / 0 (outside) cutoff.
inline float influenceWeight(const ReflectionProbe& probe, const math::vec3& point) {
    const math::vec3 d = point - probe.center;
    const float px = probe.halfExtents.x - std::fabs(d.x);
    const float py = probe.halfExtents.y - std::fabs(d.y);
    const float pz = probe.halfExtents.z - std::fabs(d.z);
    if (px < 0.0f || py < 0.0f || pz < 0.0f) return 0.0f; // outside the box
    const float nearest = std::min(px, std::min(py, pz));  // distance to the closest face
    if (probe.blendDistance <= 0.0f) return 1.0f;
    const float w = nearest / probe.blendDistance;
    return w > 1.0f ? 1.0f : w;
}

inline bool probeContains(const ReflectionProbe& probe, const math::vec3& point) {
    return influenceWeight(probe, point) > 0.0f;
}

// Parallax-correct a reflection: intersect the ray (worldPos, dir) with the probe box and return the
// normalized direction from the probe center to that exit point — the direction to sample the cubemap with.
inline math::vec3 boxProjectDirection(const ReflectionProbe& probe, const math::vec3& worldPos,
                                      const math::vec3& dir) {
    const math::vec3 mn = probe.center - probe.halfExtents;
    const math::vec3 mx = probe.center + probe.halfExtents;

    float tExit = 3.4e38f;
    for (int axis = 0; axis < 3; ++axis) {
        const float o = worldPos[axis];
        const float dd = dir[axis];
        if (std::fabs(dd) < 1e-8f) continue; // ray parallel to this slab
        const float t1 = (mn[axis] - o) / dd;
        const float t2 = (mx[axis] - o) / dd;
        const float far = t1 > t2 ? t1 : t2; // the exit crossing of this slab
        if (far < tExit) tExit = far;
    }

    const math::vec3 hit = worldPos + dir * tExit;
    const math::vec3 fromCenter = hit - probe.center;
    const float len = std::sqrt(glm::dot(fromCenter, fromCenter));
    return len > 0.0f ? fromCenter / len : dir;
}

// Convenience: which cubemap face/UV a reflection at `worldPos` in direction `dir` reads from `probe`.
inline CubeSample probeSample(const ReflectionProbe& probe, const math::vec3& worldPos,
                              const math::vec3& dir) {
    return directionToCube(boxProjectDirection(probe, worldPos, dir));
}

} // namespace maz::render
