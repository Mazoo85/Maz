#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

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

} // namespace maz::anim
