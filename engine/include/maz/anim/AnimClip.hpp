#pragma once

#include "maz/math/Math.hpp"

#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <vector>

namespace maz::anim {

// Keyframed animation clips — the playback layer on top of Skeleton. A clip stores, per joint, three
// keyframe tracks (translation, rotation, scale); sampling at a time interpolates each track
// (vec3 lerp, quaternion slerp) into a per-joint local pose, which feeds Skeleton::computeSkinning.
// blendPoses cross-fades two sampled poses, the basis of animation state blending (idle<->walk).
// Pure math — no GPU — so it unit-tests headless and stays deterministic under the fixed timestep.

// A single joint's local transform as translation/rotation/scale (glm::quat is w,x,y,z).
struct JointPose {
    math::vec3 translation{0.0f};
    math::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
    math::vec3 scale{1.0f};

    math::mat4 matrix() const {
        return glm::translate(math::mat4(1.0f), translation) * glm::mat4_cast(rotation) *
               glm::scale(math::mat4(1.0f), scale);
    }
};

template <typename T>
struct Key {
    float time = 0.0f;
    T value{};
};

// Sample a sorted vec3 keyframe track at `time` (clamps to the ends; empty -> fallback).
inline math::vec3 sampleVec3(const std::vector<Key<math::vec3>>& keys, float time,
                             const math::vec3& fallback) {
    if (keys.empty()) {
        return fallback;
    }
    if (time <= keys.front().time) {
        return keys.front().value;
    }
    if (time >= keys.back().time) {
        return keys.back().value;
    }
    for (size_t i = 1; i < keys.size(); ++i) {
        if (time < keys[i].time) {
            const float t0 = keys[i - 1].time, t1 = keys[i].time;
            const float u = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
            return glm::mix(keys[i - 1].value, keys[i].value, u);
        }
    }
    return keys.back().value;
}

// Sample a sorted quaternion track at `time` with slerp (clamps to the ends; empty -> fallback).
inline math::quat sampleQuat(const std::vector<Key<math::quat>>& keys, float time,
                             const math::quat& fallback) {
    if (keys.empty()) {
        return fallback;
    }
    if (time <= keys.front().time) {
        return glm::normalize(keys.front().value);
    }
    if (time >= keys.back().time) {
        return glm::normalize(keys.back().value);
    }
    for (size_t i = 1; i < keys.size(); ++i) {
        if (time < keys[i].time) {
            const float t0 = keys[i - 1].time, t1 = keys[i].time;
            const float u = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
            return glm::normalize(glm::slerp(keys[i - 1].value, keys[i].value, u));
        }
    }
    return glm::normalize(keys.back().value);
}

// One joint's animation: any subset of the three tracks may be empty (falls back to the rest pose).
struct JointTrack {
    std::vector<Key<math::vec3>> translation;
    std::vector<Key<math::quat>> rotation;
    std::vector<Key<math::vec3>> scale;
};

class AnimClip {
public:
    float duration = 0.0f;
    bool loop = true;
    std::vector<JointTrack> tracks; // one per joint

    // Sample every joint at `time` into `out`. `rest` supplies the fallback pose for joints/tracks
    // with no keyframes (pass the skeleton's rest local poses). When looping, `time` wraps by
    // `duration`. `out` is sized to the track count.
    void sample(float time, const std::vector<JointPose>& rest, std::vector<JointPose>& out) const {
        float t = time;
        if (loop && duration > 0.0f) {
            t = std::fmod(t, duration);
            if (t < 0.0f) {
                t += duration;
            }
        }
        out.resize(tracks.size());
        for (size_t i = 0; i < tracks.size(); ++i) {
            const JointTrack& tr = tracks[i];
            const JointPose& r = i < rest.size() ? rest[i] : JointPose{};
            out[i].translation = sampleVec3(tr.translation, t, r.translation);
            out[i].rotation = sampleQuat(tr.rotation, t, r.rotation);
            out[i].scale = sampleVec3(tr.scale, t, r.scale);
        }
    }
};

// Cross-fade two per-joint poses: lerp translation/scale, slerp rotation. weight 0 -> a, 1 -> b.
inline void blendPoses(const std::vector<JointPose>& a, const std::vector<JointPose>& b, float weight,
                       std::vector<JointPose>& out) {
    const float w = weight < 0.0f ? 0.0f : (weight > 1.0f ? 1.0f : weight);
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    out.resize(n);
    for (size_t i = 0; i < n; ++i) {
        out[i].translation = glm::mix(a[i].translation, b[i].translation, w);
        out[i].scale = glm::mix(a[i].scale, b[i].scale, w);
        out[i].rotation = glm::normalize(glm::slerp(a[i].rotation, b[i].rotation, w));
    }
}

// Blend N weighted poses into one (translation/scale lerp, rotation via incremental normalized slerp).
// Weights need not sum to 1 — they are normalized as they accumulate — so barycentric / blend-space
// weights feed straight in. Poses that share the joint count of the first are combined; the result is
// the `poses[0]` pose when only one is supplied. This is the N-way generalization of blendPoses and
// the core of animation blend spaces (Godot's AnimationTree BlendSpace1D/2D).
inline void blendPosesWeighted(const std::vector<const std::vector<JointPose>*>& poses,
                               const std::vector<float>& weights, std::vector<JointPose>& out) {
    if (poses.empty()) {
        out.clear();
        return;
    }
    out = *poses[0];
    float accum = weights.empty() ? 1.0f : weights[0];
    for (size_t i = 1; i < poses.size(); ++i) {
        const float wi = i < weights.size() ? weights[i] : 0.0f;
        const float total = accum + wi;
        if (total <= 1e-8f) {
            continue;
        }
        blendPoses(out, *poses[i], wi / total, out); // fold in pose i at its share of the running total
        accum = total;
    }
}

// Convenience: turn per-joint poses into the local matrices Skeleton::computeSkinning expects.
inline void posesToLocals(const std::vector<JointPose>& poses, std::vector<math::mat4>& out) {
    out.resize(poses.size());
    for (size_t i = 0; i < poses.size(); ++i) {
        out[i] = poses[i].matrix();
    }
}

} // namespace maz::anim
