#pragma once

#include "maz/anim/AnimClip.hpp"

#include <string>
#include <vector>

namespace maz::anim {

// The animation controller — the stateful layer a game actually drives. It holds a library of named
// clips and plays one at a time; play() starts a timed CROSS-FADE from whatever is currently playing
// to a new clip, and both clips keep advancing during the fade so the blend is smooth (a run doesn't
// freeze while it eases into a jump). update() advances time + the fade; pose() returns the blended
// per-joint result to hand to Skeleton::computeSkinning. Pure logic on top of AnimClip — no GPU, so
// it unit-tests headless and stays deterministic under the fixed timestep.
class Animator {
public:
    // Rest pose (per joint) used as the fallback for clip tracks with no keyframes. Set once.
    void setRestPose(std::vector<JointPose> rest) { m_rest = std::move(rest); }

    // Register a clip under a name; returns its id (index). Names need not be unique but findClip
    // returns the first match.
    int addClip(std::string name, AnimClip clip) {
        m_names.push_back(std::move(name));
        m_clips.push_back(std::move(clip));
        return static_cast<int>(m_clips.size() - 1);
    }
    int findClip(const std::string& name) const {
        for (size_t i = 0; i < m_names.size(); ++i) {
            if (m_names[i] == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }
    int clipCount() const { return static_cast<int>(m_clips.size()); }

    // Cross-fade to clip `id` over `fadeDuration` seconds. Playing the clip that is already the
    // active target is a no-op (it won't restart). The first clip played snaps in (no from-clip).
    void play(int id, float fadeDuration = 0.25f) {
        if (id < 0 || id >= clipCount() || id == m_to.clip) {
            return;
        }
        m_from = m_to;
        m_to = Track{id, 0.0f};
        m_fadeT = 0.0f;
        m_fadeDur = m_from.clip >= 0 ? fadeDuration : 0.0f;
        evaluate();
    }
    void play(const std::string& name, float fadeDuration = 0.25f) {
        play(findClip(name), fadeDuration);
    }

    void update(float dt) {
        if (m_to.clip >= 0) {
            m_to.time += dt;
        }
        if (m_fadeDur > 0.0f && m_fadeT < m_fadeDur) {
            m_from.time += dt;
            m_fadeT += dt;
        }
        evaluate();
    }

    const std::vector<JointPose>& pose() const { return m_pose; }
    int currentClip() const { return m_to.clip; }
    bool isFading() const { return m_from.clip >= 0 && m_fadeDur > 0.0f && m_fadeT < m_fadeDur; }
    // Fade progress 0..1 (1 when not fading / fade complete).
    float fadeProgress() const {
        if (m_fadeDur <= 0.0f) {
            return 1.0f;
        }
        const float p = m_fadeT / m_fadeDur;
        return p < 0.0f ? 0.0f : (p > 1.0f ? 1.0f : p);
    }

private:
    struct Track {
        int clip = -1;
        float time = 0.0f;
    };

    void evaluate() {
        const float w = fadeProgress();
        if (m_from.clip >= 0 && w < 1.0f) {
            m_clips[static_cast<size_t>(m_from.clip)].sample(m_from.time, m_rest, m_a);
            m_clips[static_cast<size_t>(m_to.clip)].sample(m_to.time, m_rest, m_b);
            blendPoses(m_a, m_b, w, m_pose);
        } else if (m_to.clip >= 0) {
            m_clips[static_cast<size_t>(m_to.clip)].sample(m_to.time, m_rest, m_pose);
        } else {
            m_pose = m_rest;
        }
    }

    std::vector<std::string> m_names;
    std::vector<AnimClip> m_clips;
    std::vector<JointPose> m_rest;
    Track m_from, m_to;
    float m_fadeT = 0.0f, m_fadeDur = 0.0f;
    std::vector<JointPose> m_pose, m_a, m_b;
};

} // namespace maz::anim
