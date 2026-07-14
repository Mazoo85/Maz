#pragma once

#include <cstdint>
#include <functional>
#include <cmath>  // std::fmod

#include "maz/math/Interpolation.hpp"

// A single-value float tween — animate from->to over a duration with an easing
// curve (iter6 maz::math::Easing/ease/lerp), advanced by update(dt); the Godot
// Tween analog. LoopMode Once (clamps at the end + fires onComplete once), Loop
// (wraps to the start of the next cycle), and PingPong (triangle back-and-forth)
// are all driven by fmod phase math, so any dt — including a dt that spans
// multiple cycles — is handled without an internal loop. dt is assumed >= 0.
// NOT thread-safe. A generic Tween<T> (vec3/color), sequences/timelines, a start
// delay, and a speed scale are future refinements (not built here).

namespace maz::anim {

enum class LoopMode : std::uint8_t { Once, Loop, PingPong };

class Tween {
  public:
    // Inactive default: from=to=duration=0, value=0, finished=false.
    Tween() = default;

    Tween(float from, float to, float duration,
          maz::math::Easing easing = maz::math::Easing::Linear,
          LoopMode loop = LoopMode::Once) {
        restart(from, to, duration, easing, loop);
    }

    // Stores the animation, resets elapsed=0 and finished=false, then recomputes
    // so value() == from at t=0 (or == to when duration <= 0).
    void restart(float from, float to, float duration,
                 maz::math::Easing easing = maz::math::Easing::Linear,
                 LoopMode loop = LoopMode::Once) {
        m_from = from;
        m_to = to;
        m_duration = duration;
        m_easing = easing;
        m_loop = loop;
        m_elapsed = 0.0f;
        m_finished = false;
        recompute();
    }

    // Rewinds to the start (elapsed=0, finished=false) keeping from/to/duration/
    // easing/loop, and recomputes value at t=0 (== from).
    void reset() {
        m_elapsed = 0.0f;
        m_finished = false;
        recompute();
    }

    // Advances elapsed by dt (assumed >= 0), recomputes, and returns the new
    // value. In Once mode, once finished this is a no-op that returns the held
    // end value (onComplete does NOT re-fire).
    float update(float dt) {
        if (m_finished) {
            return m_value;
        }
        m_elapsed += dt;
        recompute();
        return m_value;
    }

    float value() const { return m_value; }

    // Once: saturate(elapsed/duration) (1.0 once done). Loop/PingPong: the
    // current phase t in [0,1] (the same t recompute() feeds into the easing).
    float progress() const {
        if (m_loop == LoopMode::Once) {
            return maz::math::saturate(m_duration > 0.0f ? m_elapsed / m_duration : 1.0f);
        }
        return phase();
    }

    // True only in Once mode after reaching the end; Loop/PingPong never finish.
    bool finished() const { return m_finished; }

    LoopMode loop() const { return m_loop; }

    void onComplete(std::function<void()> cb) { m_onComplete = std::move(cb); }

  private:
    // Computes the phase parameter t in [0,1] from m_elapsed for the current
    // loop mode. A degenerate duration (<= 0) reports t=1 (end of the curve).
    // This is the SINGLE source of truth for the phase parameter: both
    // progress() and recompute() derive t from here so they cannot diverge.
    float phase() const {
        if (m_duration <= 0.0f) {
            return 1.0f;
        }
        if (m_loop == LoopMode::Loop) {
            // fmod of an exact multiple (e.g. elapsed == duration) yields 0 -> t=0,
            // i.e. the start of the next cycle. That wrap is intended.
            return std::fmod(m_elapsed, m_duration) / m_duration;
        }
        if (m_loop == LoopMode::PingPong) {
            const float period = 2.0f * m_duration;
            const float ph = std::fmod(m_elapsed, period);
            return (ph <= m_duration) ? (ph / m_duration) : (2.0f - ph / m_duration);
        }
        // Once
        const float raw = m_elapsed / m_duration;
        return (raw >= 1.0f) ? 1.0f : maz::math::saturate(raw);
    }

    // Recomputes m_value from m_elapsed. The phase parameter t comes from
    // phase() (the single source of truth), so this shares its exact math with
    // progress(). recompute() adds only the side effects a const helper cannot:
    // in Once mode it latches m_finished and fires m_onComplete exactly once on
    // the transition to finished, and it snaps to 'to' for a degenerate
    // duration (<= 0), which also guards div-by-zero / fmod-by-zero.
    void recompute() {
        // Degenerate duration: snap to the end value and latch (Once) once.
        if (m_duration <= 0.0f) {
            m_value = m_to;
            if (m_loop == LoopMode::Once && !m_finished) {
                m_finished = true;
                if (m_onComplete) { m_onComplete(); }
            }
            return;
        }
        // Once finished-latch: the transition happens when elapsed >= duration,
        // matching phase()'s Once branch (raw >= 1 -> t == 1).
        if (m_loop == LoopMode::Once && !m_finished && m_elapsed / m_duration >= 1.0f) {
            m_finished = true;
            if (m_onComplete) { m_onComplete(); }
        }
        const float t = phase();
        m_value = maz::math::lerp(m_from, m_to, maz::math::ease(m_easing, t));
    }

    float m_from = 0.0f, m_to = 0.0f, m_duration = 0.0f, m_elapsed = 0.0f, m_value = 0.0f;
    maz::math::Easing m_easing = maz::math::Easing::Linear;
    LoopMode m_loop = LoopMode::Once;
    bool m_finished = false;
    std::function<void()> m_onComplete;
};

} // namespace maz::anim
