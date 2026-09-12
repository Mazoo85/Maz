#pragma once

#include "maz/math/Geometry3D.hpp" // math::Aabb3
#include "maz/math/Math.hpp"       // vec3

#include <algorithm>
#include <cmath>

// maz::math swept AABB vs AABB — CONTINUOUS collision of a MOVING axis-aligned box against a STATIC one over
// one time step, the workhorse behind a tunnelling-free platformer / block-world character controller. A fast
// body stepped discretely can pass straight THROUGH a thin wall between frames ("tunnelling"); this finds the
// exact fraction of the step `t` in [0,1] at which the moving box first touches the static box, plus the
// contact face normal, so the mover can be advanced to the contact and slid along the surface. It is the
// box analogue of the engine's ray/AABB slab test and its 2D circle `game::ShapeCast2D` / conservative
// `game::sphereCast`: a per-axis entry/exit-time intersection (the Minkowski-sum / relative-motion form).
// Pure vec3 math, header-only, deterministic — exactly unit-testable against hand-computed contact times.
//
// Semantics: the boxes are assumed SEPARATED at t=0 and the mover travels by displacement `d` (velocity ×
// dt). Returns hit=false when they never touch during the step, when the mover moves away, or when the
// contact would happen beyond t=1. Boxes already overlapping at t=0 report hit=false (there is no *first*
// contact to find in the step) — use a static overlap test (`Aabb3::intersects`) for that case.
namespace maz::math {

struct SweptAabbHit {
    bool hit = false;
    float t = 1.0f;                    // fraction of the step at first contact, in [0,1]
    vec3 normal{0.0f, 0.0f, 0.0f};     // contact face normal (points from the static box toward the mover)
};

inline SweptAabbHit sweptAabbAabb(const Aabb3& mover, const vec3& d, const Aabb3& stat) {
    SweptAabbHit out;

    float entryT = -std::numeric_limits<float>::infinity(); // latest axis entry
    float exitT = std::numeric_limits<float>::infinity();   // earliest axis exit
    int entryAxis = -1;
    float entrySign = 0.0f;

    const float mn0[3] = {mover.min.x, mover.min.y, mover.min.z};
    const float mx0[3] = {mover.max.x, mover.max.y, mover.max.z};
    const float sn[3] = {stat.min.x, stat.min.y, stat.min.z};
    const float sx[3] = {stat.max.x, stat.max.y, stat.max.z};
    const float dd[3] = {d.x, d.y, d.z};

    for (int i = 0; i < 3; ++i) {
        if (dd[i] == 0.0f) {
            // No motion on this axis: if the boxes are separated here, they can never touch during the step.
            if (mx0[i] < sn[i] || mn0[i] > sx[i]) {
                return out; // hit = false
            }
            continue; // overlapping on this axis — it does not constrain the entry/exit window
        }
        // Signed gaps to the near and far faces, in the direction of travel.
        float invEntry, invExit;
        if (dd[i] > 0.0f) {
            invEntry = sn[i] - mx0[i];
            invExit = sx[i] - mn0[i];
        } else {
            invEntry = sx[i] - mn0[i];
            invExit = sn[i] - mx0[i];
        }
        const float axisEntry = invEntry / dd[i];
        const float axisExit = invExit / dd[i];
        if (axisEntry > entryT) {
            entryT = axisEntry;
            entryAxis = i;
            entrySign = dd[i] > 0.0f ? -1.0f : 1.0f; // face normal opposes travel
        }
        exitT = std::fmin(exitT, axisExit);
    }

    // No overlap window, contact already behind us, or contact only beyond this step.
    if (entryAxis < 0 || entryT > exitT || entryT < 0.0f || entryT > 1.0f) {
        return out;
    }

    out.hit = true;
    out.t = entryT;
    if (entryAxis == 0) out.normal = vec3(entrySign, 0.0f, 0.0f);
    else if (entryAxis == 1) out.normal = vec3(0.0f, entrySign, 0.0f);
    else out.normal = vec3(0.0f, 0.0f, entrySign);
    return out;
}

} // namespace maz::math
