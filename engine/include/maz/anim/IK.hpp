#pragma once

#include "maz/math/Math.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::anim {

// 2-bone inverse kinematics — Godot's SkeletonModification2DTwoBoneIK. Given a fixed root joint, two
// bone lengths (upper `len1`, lower `len2`), and a target, solve for the middle joint (elbow/knee) and
// the end effector so the chain reaches the target. Uses the law of cosines: the angle at the root
// between the root->target line and the upper bone is acos((len1²+d²-len2²)/(2·len1·d)). `bendSign`
// (+1 / -1) picks which side the elbow bends to. When the target is out of reach the chain points
// straight at it, fully extended. Pure 2D math — no skeleton, no GPU — so it unit-tests headlessly.

struct IKResult {
    math::vec2 mid{0.0f, 0.0f}; // middle joint (elbow / knee)
    math::vec2 end{0.0f, 0.0f}; // end effector (hand / foot)
    bool reachable = false;     // true when the target lies within [|len1-len2|, len1+len2] of the root
};

inline IKResult solveTwoBoneIK(math::vec2 root, float len1, float len2, math::vec2 target,
                               float bendSign = 1.0f) {
    IKResult r;
    math::vec2 to = target - root;
    float d = std::sqrt(to.x * to.x + to.y * to.y);
    if (d < 1e-6f) {
        // Degenerate: target on the root. Fold along +x so the result is well-defined.
        to = math::vec2(1.0f, 0.0f);
        d = 1e-6f;
    }
    const math::vec2 dir = to / d;
    const float reach = len1 + len2;
    const float minReach = std::fabs(len1 - len2);
    r.reachable = (d <= reach + 1e-4f) && (d >= minReach - 1e-4f);

    if (!r.reachable) {
        // Out of reach: extend straight toward (or away, if too close) the target.
        r.mid = root + dir * len1;
        r.end = root + dir * (d > reach ? reach : minReach); // too far -> extend; too close -> fold
        return r;
    }

    // Reachable: place the elbow by rotating the root->target direction by the root angle.
    float cosA = (len1 * len1 + d * d - len2 * len2) / (2.0f * len1 * d);
    cosA = cosA < -1.0f ? -1.0f : (cosA > 1.0f ? 1.0f : cosA);
    const float a = std::acos(cosA);
    const float baseAng = std::atan2(dir.y, dir.x);
    const float upperAng = baseAng + bendSign * a;
    r.mid = root + math::vec2(std::cos(upperAng), std::sin(upperAng)) * len1;
    r.end = target; // a 2-bone chain reaches the target exactly within its working range
    return r;
}

// Multi-bone FABRIK — Forward And Backward Reaching Inverse Kinematics (Godot's
// SkeletonModification2DFABRIK). `joints` is the chain in order, joints[0] the fixed base; the solver
// moves every joint so the last one reaches `target` while each bone keeps its original length. Each
// iteration does two passes: BACKWARD pins the tip to the target and drags the chain back toward the
// base (each joint re-placed on the line to its successor at the original bone length); FORWARD re-pins
// the base and pushes the chain back out. It converges in a few iterations; a target beyond the chain's
// total length is unreachable, so the chain simply straightens toward it (best effort). Pure 2D math —
// no skeleton, no GPU — so it unit-tests headlessly and stays deterministic.
inline void solveFabrik(std::vector<math::vec2>& joints, math::vec2 target, int iterations = 10,
                        float tolerance = 1e-3f) {
    const std::size_t n = joints.size();
    if (n < 2) {
        return;
    }
    // Capture original bone lengths (preserved) + the fixed base.
    std::vector<float> len(n - 1);
    float total = 0.0f;
    for (std::size_t i = 0; i + 1 < n; ++i) {
        const math::vec2 d = joints[i + 1] - joints[i];
        len[i] = std::sqrt(glm::dot(d, d));
        total += len[i];
    }
    const math::vec2 base = joints[0];

    // Re-place `to` at distance `l` from `from`, along the from->to direction (keeps the bone length).
    auto onLine = [](math::vec2 from, math::vec2 to, float l) {
        math::vec2 d = to - from;
        const float dl = std::sqrt(glm::dot(d, d));
        return from + (dl > 1e-8f ? d / dl : math::vec2(1.0f, 0.0f)) * l;
    };

    const math::vec2 toT = target - base;
    const float distT = std::sqrt(glm::dot(toT, toT));
    if (distT > total) {
        // Out of reach: straighten the whole chain toward the target.
        const math::vec2 dir = distT > 1e-8f ? toT / distT : math::vec2(1.0f, 0.0f);
        for (std::size_t i = 0; i + 1 < n; ++i) {
            joints[i + 1] = joints[i] + dir * len[i];
        }
        return;
    }

    for (int it = 0; it < iterations; ++it) {
        const math::vec2 err = joints[n - 1] - target;
        if (std::sqrt(glm::dot(err, err)) < tolerance) {
            break;
        }
        // Backward pass: tip -> target, drag the rest toward the base.
        joints[n - 1] = target;
        for (std::size_t i = n - 1; i-- > 0;) {
            joints[i] = onLine(joints[i + 1], joints[i], len[i]);
        }
        // Forward pass: re-pin the base, push the chain back out.
        joints[0] = base;
        for (std::size_t i = 0; i + 1 < n; ++i) {
            joints[i + 1] = onLine(joints[i], joints[i + 1], len[i]);
        }
    }
}

} // namespace maz::anim
