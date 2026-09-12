#pragma once

#include <algorithm> // std::clamp

// maz::game stealth detection meter — the awareness system every stealth game runs on but Godot ships
// nothing for: given a per-frame EXPOSURE signal (0 = the target is fully hidden, 1 = in plain sight,
// which the caller computes from a ViewCone/FieldOfView test, distance falloff, lighting, movement, etc.),
// it integrates that signal over TIME into a 0..1 awareness meter and classifies it into the classic
// Unaware -> Suspicious -> Alerted states (Metal Gear's "!"/"?", Splinter Cell, Assassin's Creed, Dishonored).
//
// This is deliberately the temporal layer ON TOP of the engine's existing geometry: ViewCone / FieldOfView
// answer "can the guard see that spot RIGHT NOW?", SoundPropagation answers "how loud is it here?", and
// AggroTable answers "who do I attack once combat has started?" — none of them remember a partial sighting,
// ramp suspicion while you linger in view, hold that suspicion briefly after you break line of sight, then
// slowly forget. That fill / linger / decay behaviour, plus hysteresis so the state does not flip-flop on
// the threshold, is what makes stealth feel fair, and it is exactly what this provides.
//
// Model: while exposed, awareness rises at `fillRate * exposure` per second (so a distant glimpse ramps
// slower than being caught in the open) and the linger timer is topped up. When exposure drops to zero the
// meter first HOLDS for `lingerTime` seconds (the guard keeps looking), then decays at `decayRate` per
// second. State thresholds have hysteresis: you must fill to `alertAt` (1.0 by default — fully spotted) to
// become Alerted and then fall below `relaxAt` to leave it; you cross `suspiciousAt` to rouse suspicion and
// must drain to `calmAt` to forget entirely. Header-only, std-only, deterministic — exactly unit-testable.
namespace maz::game {

enum class Awareness { Unaware, Suspicious, Alerted };

struct DetectionParams {
    float fillRate = 0.6f;      // awareness gained per second at full exposure (exposure == 1)
    float decayRate = 0.35f;    // awareness lost per second once the linger hold has elapsed
    float lingerTime = 1.0f;    // seconds the meter holds after exposure drops before it starts decaying
    float suspiciousAt = 0.25f; // meter level that rouses suspicion (rising)
    float alertAt = 1.0f;       // meter level that triggers full detection
    float relaxAt = 0.5f;       // meter must fall below this to leave the Alerted state (hysteresis)
    float calmAt = 0.0f;        // meter must fall to/below this to return to Unaware (hysteresis)
};

class DetectionMeter {
public:
    DetectionMeter() = default;
    explicit DetectionMeter(const DetectionParams& p) : m_p(p) {}

    // Advance one frame. `exposure` is how visible the target is this frame in [0,1] (values outside are
    // clamped); `dt` is the frame delta in seconds (non-positive dt is a no-op).
    void update(float exposure, float dt) {
        if (dt <= 0.0f) return;
        const float e = std::clamp(exposure, 0.0f, 1.0f);
        if (e > 0.0f) {
            m_meter += m_p.fillRate * e * dt;
            m_linger = m_p.lingerTime; // refresh the hold while still (partly) seen
        } else if (m_linger > 0.0f) {
            m_linger = std::max(0.0f, m_linger - dt); // hold: guard keeps looking, meter frozen
        } else {
            m_meter -= m_p.decayRate * dt; // forget slowly
        }
        m_meter = std::clamp(m_meter, 0.0f, 1.0f);
        updateState();
    }

    // Slam the meter to fully alerted (e.g. a body was found, an alarm was pulled). The linger is topped
    // up so it does not begin forgetting on the very next unexposed frame.
    void forceAlert() {
        m_meter = 1.0f;
        m_linger = m_p.lingerTime;
        m_state = Awareness::Alerted;
    }

    void reset() {
        m_meter = 0.0f;
        m_linger = 0.0f;
        m_state = Awareness::Unaware;
    }

    float meter() const { return m_meter; }           // raw 0..1 awareness (drives the on-screen bar)
    Awareness state() const { return m_state; }
    bool isAlerted() const { return m_state == Awareness::Alerted; }
    bool isSuspicious() const { return m_state == Awareness::Suspicious; }
    float lingerRemaining() const { return m_linger; }
    const DetectionParams& params() const { return m_p; }

private:
    void updateState() {
        // Hysteretic classification: rising crossings use suspiciousAt / alertAt; a state only relaxes
        // once the meter falls back past the (lower) relaxAt / calmAt levels, so it never flip-flops.
        if (m_meter >= m_p.alertAt) {
            m_state = Awareness::Alerted;
        } else if (m_state == Awareness::Alerted && m_meter >= m_p.relaxAt) {
            // hold Alerted through the hysteresis band
        } else if (m_meter >= m_p.suspiciousAt) {
            m_state = Awareness::Suspicious;
        } else if (m_state != Awareness::Unaware && m_meter > m_p.calmAt) {
            m_state = Awareness::Suspicious; // hold suspicion while draining between calmAt and suspiciousAt
        } else {
            m_state = Awareness::Unaware;
        }
    }

    DetectionParams m_p{};
    float m_meter = 0.0f;
    float m_linger = 0.0f;
    Awareness m_state = Awareness::Unaware;
};

} // namespace maz::game
