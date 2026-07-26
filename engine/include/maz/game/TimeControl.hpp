#pragma once

#include <algorithm> // std::max

// maz::game time control — the single authority that answers "what time step does GAMEPLAY get this frame?"
// given the real frame delta. It layers the two effects action games lean on and that Godot's flat
// Engine.time_scale cannot express: a SMOOTHLY RAMPED global time scale (ease into bullet-time / slow-mo /
// fast-forward / a soft pause, and ease back out) and transient HIT-STOP freezes (the few-frame full stop
// on a heavy impact that sells the hit in fighting and action games). You call `advance(realDt)` once per
// frame with the unscaled delta; it updates the internal timers and RETURNS the scaled delta to drive all
// gameplay, physics, and animation.
//
// This is distinct from `core::GameClock`, which just accumulates seconds at a FIXED scale for a day/night
// style clock — it has no ramp, no hit-stop, and does not hand back a per-frame delta. Hit-stop is measured
// in REAL time (so a freeze lasts the same wall-clock duration no matter the current slow-mo), overrides
// everything to a full stop while active, and a new request extends but never shortens an ongoing freeze
// (it takes the max). A freeze pauses the ramp too, so bullet-time resumes exactly where it left off. Model
// note: a frame that begins inside a freeze is frozen whole (returns 0) — hit-stop is a handful of frames,
// so this is the standard, deterministic choice. Header-only, std-only, deterministic.
namespace maz::game {

class TimeControl {
public:
    explicit TimeControl(float scale = 1.0f)
        : m_scale(scale < 0.0f ? 0.0f : scale), m_target(m_scale) {}

    // Set the time scale immediately (also cancels any in-progress ramp). 1 = normal, 0.5 = half speed,
    // 0 = soft pause, 2 = double speed. Clamped to >= 0 (time does not run backwards here).
    void setScale(float s) {
        m_scale = s < 0.0f ? 0.0f : s;
        m_target = m_scale;
    }

    // Ease the time scale toward `target` at `ratePerSecond` scale-units per REAL second (a rate of 0 snaps
    // immediately). Use this for bullet-time ramp-in / ramp-out. Both are clamped to >= 0.
    void rampTo(float target, float ratePerSecond) {
        m_target = target < 0.0f ? 0.0f : target;
        m_rampRate = ratePerSecond < 0.0f ? 0.0f : ratePerSecond;
        if (m_rampRate == 0.0f) m_scale = m_target;
    }

    // Request a hit-stop freeze of `seconds` of REAL time. Extends an ongoing freeze but never shortens it.
    void hitStop(float seconds) {
        if (seconds > m_hitStop) m_hitStop = seconds;
    }

    // Advance one frame by the real (unscaled) delta and return the gameplay delta to use this frame:
    //   * while a hit-stop is active the whole frame is frozen (returns 0) and the freeze counts down;
    //   * otherwise the scale eases toward its ramp target and the return is realDt * scale.
    // Non-positive realDt returns 0 and changes nothing.
    float advance(float realDt) {
        if (realDt <= 0.0f) return 0.0f;
        if (m_hitStop > 0.0f) {
            m_hitStop = std::max(0.0f, m_hitStop - realDt);
            return 0.0f; // frozen: gameplay, physics and animation all stand still
        }
        // Ease the scale toward the target without overshooting (constant real-time rate).
        if (m_scale != m_target && m_rampRate > 0.0f) {
            const float step = m_rampRate * realDt;
            if (m_scale < m_target) m_scale = std::min(m_target, m_scale + step);
            else m_scale = std::max(m_target, m_scale - step);
        }
        return realDt * m_scale;
    }

    float scale() const { return m_scale; }            // current (possibly mid-ramp) time scale
    float target() const { return m_target; }          // scale the ramp is heading toward
    bool isFrozen() const { return m_hitStop > 0.0f; }  // inside a hit-stop this frame
    bool isRamping() const { return m_scale != m_target; }
    float hitStopRemaining() const { return m_hitStop; }

private:
    float m_scale;
    float m_target;
    float m_rampRate = 0.0f;
    float m_hitStop = 0.0f;
};

} // namespace maz::game
