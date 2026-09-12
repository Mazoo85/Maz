#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

// maz::game::RecoilPattern — the climbing "spray" every first-person shooter needs: each shot kicks the aim
// by a DEFINED amount (so a weapon has a recognisable, learnable spray pattern like CS/Valorant), the kicks
// ACCUMULATE while firing, and the aim RECOVERS smoothly back toward centre when you stop. This is distinct
// from game::Spread (M632), which perturbs each shot by a RANDOM amount within a cone (bullet inaccuracy);
// recoil is the deterministic, per-shot, memorised climb that the player learns to counter by pulling down.
// You give it a pattern (a list of per-shot offset kicks — typically climbing up then drifting), call fire()
// per shot to advance and accumulate, update(dt) each frame to recover, and add offset() to the aim
// direction/crosshair. Firing past the pattern's end repeats the last kick (a sustained climb). Beyond Godot.
// Header-only, pure, deterministic.
namespace maz::game {

class RecoilPattern {
public:
    RecoilPattern() = default;
    explicit RecoilPattern(std::vector<math::vec2> pattern, float recoveryRate = 8.0f)
        : m_pattern(std::move(pattern)), m_recoveryRate(recoveryRate) {}

    void setPattern(std::vector<math::vec2> pattern) { m_pattern = std::move(pattern); }
    void setRecoveryRate(float rate) { m_recoveryRate = rate; } // exponential decay per second toward centre

    // Fire one shot: apply the next pattern kick (repeating the last if past the end), accumulate it into the
    // current offset, and advance. Returns the new offset. A no-op (returns the current offset) if no pattern.
    math::vec2 fire() {
        if (!m_pattern.empty()) {
            const std::size_t i = m_index < m_pattern.size() ? m_index : m_pattern.size() - 1;
            m_offset.x += m_pattern[i].x;
            m_offset.y += m_pattern[i].y;
            ++m_index;
        }
        return m_offset;
    }

    // Recover the accumulated offset toward centre. Exponential (frame-rate independent): after `dt`, the
    // offset is multiplied by exp(-recoveryRate * dt), so it eases to zero and never overshoots the sign.
    void update(float dt) {
        if (dt <= 0.0f || m_recoveryRate <= 0.0f) return;
        const float f = std::exp(-m_recoveryRate * dt);
        m_offset.x *= f;
        m_offset.y *= f;
    }

    // Stop firing: the pattern restarts from the beginning on the next fire(), while the current offset keeps
    // recovering via update(). Use when the trigger is released between bursts.
    void release() { m_index = 0; }

    // Clear everything: offset back to centre and the pattern index reset.
    void reset() {
        m_offset = math::vec2(0.0f, 0.0f);
        m_index = 0;
    }

    math::vec2 offset() const { return m_offset; }
    std::size_t shotIndex() const { return m_index; } // shots fired since the last release()/reset()
    bool empty() const { return m_pattern.empty(); }

private:
    std::vector<math::vec2> m_pattern;
    float m_recoveryRate = 8.0f;
    math::vec2 m_offset{0.0f, 0.0f};
    std::size_t m_index = 0;
};

} // namespace maz::game
