#pragma once

#include "maz/anim/AnimClip.hpp"
#include "maz/math/Math.hpp"

#include <glm/gtc/quaternion.hpp>

#include <vector>

namespace maz::anim {

// Additive / layered pose blending — Godot's AnimationNodeAdd2 (and the "additive" import flag). The
// existing blendPoses/blendPosesWeighted CROSS-FADE between whole poses (idle <-> walk): every joint is
// interpolated, so a walk pose fully replaces an idle pose at weight 1. Additive blending instead layers
// a *difference* on top of a base: an additive clip is stored relative to a REFERENCE pose, its per-joint
// DELTA (how far each joint moved from the reference) is computed, and that delta is applied on top of
// whatever base pose is playing — scaled by a weight. A joint that doesn't move in the additive clip has
// a zero delta and leaves the base untouched, so you can layer a "wave", "breathe", "aim", or "recoil"
// motion onto any locomotion without disturbing the unrelated joints. Pure math on JointPose (TRS with a
// quaternion), header-only, unit-testable without a skeleton or GPU.

// The delta of one joint: additive relative to reference. Translation subtracts, rotation is the
// reference->additive rotation, scale is the component ratio. (A delta equal to identity — zero
// translation, identity rotation, unit scale — means "no change".)
inline JointPose makeAdditiveDelta(const JointPose& additive, const JointPose& reference) {
    JointPose d;
    d.translation = additive.translation - reference.translation;
    d.rotation = glm::normalize(glm::inverse(reference.rotation) * additive.rotation);
    d.scale = math::vec3(
        reference.scale.x != 0.0f ? additive.scale.x / reference.scale.x : additive.scale.x,
        reference.scale.y != 0.0f ? additive.scale.y / reference.scale.y : additive.scale.y,
        reference.scale.z != 0.0f ? additive.scale.z / reference.scale.z : additive.scale.z);
    return d;
}

// Apply a joint delta on top of a base joint, scaled by `weight` (clamped to [0,1]). At weight 0 the
// result is exactly `base`; at weight 1 the full delta is applied. The rotation applies the fraction of
// the delta via slerp from identity; the scale interpolates its ratio from 1.
inline JointPose applyAdditiveDelta(const JointPose& base, const JointPose& delta, float weight) {
    const float w = weight < 0.0f ? 0.0f : (weight > 1.0f ? 1.0f : weight);
    JointPose out;
    out.translation = base.translation + delta.translation * w;
    const math::quat identity(1.0f, 0.0f, 0.0f, 0.0f);
    const math::quat partial = glm::normalize(glm::slerp(identity, delta.rotation, w));
    out.rotation = glm::normalize(base.rotation * partial);
    out.scale = base.scale * glm::mix(math::vec3(1.0f), delta.scale, w);
    return out;
}

// Convenience: layer `additive` (relative to `reference`) onto `base` at `weight` for one joint.
inline JointPose additiveBlendJoint(const JointPose& base, const JointPose& additive,
                                    const JointPose& reference, float weight) {
    return applyAdditiveDelta(base, makeAdditiveDelta(additive, reference), weight);
}

// Whole-pose delta: each joint's additive-vs-reference difference.
inline void makeAdditivePose(const std::vector<JointPose>& additive,
                             const std::vector<JointPose>& reference, std::vector<JointPose>& outDelta) {
    const size_t n = additive.size() < reference.size() ? additive.size() : reference.size();
    outDelta.resize(n);
    for (size_t i = 0; i < n; ++i) {
        outDelta[i] = makeAdditiveDelta(additive[i], reference[i]);
    }
}

// Apply a whole-pose delta on top of `base` at `weight`.
inline void applyAdditivePose(const std::vector<JointPose>& base, const std::vector<JointPose>& delta,
                              float weight, std::vector<JointPose>& out) {
    const size_t n = base.size() < delta.size() ? base.size() : delta.size();
    out.resize(n);
    for (size_t i = 0; i < n; ++i) {
        out[i] = applyAdditiveDelta(base[i], delta[i], weight);
    }
}

// One-shot: layer `additive` (relative to `reference`) onto `base` at `weight` for a whole pose.
inline void additiveBlend(const std::vector<JointPose>& base, const std::vector<JointPose>& additive,
                          const std::vector<JointPose>& reference, float weight,
                          std::vector<JointPose>& out) {
    std::vector<JointPose> delta;
    makeAdditivePose(additive, reference, delta);
    applyAdditivePose(base, delta, weight, out);
}

} // namespace maz::anim
