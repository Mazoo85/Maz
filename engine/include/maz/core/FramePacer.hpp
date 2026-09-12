#pragma once

#include <algorithm>
#include <cmath>

// maz::core frame-rate cap / power-save pacer — the CPU/GPU-and-battery counterpart to dynamic resolution.
// On a phone, running the loop as fast as the hardware allows drains the battery and cooks the SoC into a
// thermal throttle for no benefit past the display's refresh rate; and while the player sits on a menu or
// the app is backgrounded, there is no reason to render 60 fps at all. A pacer caps the loop to a target
// frame rate by telling the caller how long to sleep after each frame's work, and drops to a lower "idle"
// cap when the game isn't actively playing — exactly Godot's `Engine.max_fps` plus `low_processor_usage_mode`.
//
// This is the pure, deterministic policy (no real sleeping, no clock here — the caller measures its own
// frame-work time and does the sleep): feed the seconds spent updating+rendering this frame and get back
// the seconds to sleep to land on the target period. Heavy frames that blow the budget sleep zero and,
// with drift correction on, are paid back by slightly shorter sleeps afterward so the AVERAGE frame rate
// stays on target instead of drifting under. Header-only, std-only, unit-tested.
namespace maz::core {

struct FramePacerConfig {
    double activeFps = 60.0;       // cap while the game is active; <= 0 means uncapped (never sleep)
    double idleFps = 0.0;          // cap while idle (menu/paused/unfocused); <= 0 disables idle mode
    double maxFrameSeconds = 0.25; // clamp a stalled frame so a breakpoint/slow-load can't overcorrect
    bool driftCorrection = true;   // pay back over-budget frames so the average fps is exact
};

class FramePacer {
public:
    explicit FramePacer(FramePacerConfig cfg = {}) : m_cfg(sanitize(cfg)) {}

    // Given the seconds spent on this frame's work (update + render), return how long to sleep to hit the
    // current target period. Returns 0 when uncapped or when the frame already overran the budget.
    double sleepFor(double workSeconds) {
        const double period = targetPeriod();
        if (period <= 0.0) return 0.0; // uncapped: never sleep
        if (!(std::isfinite(workSeconds)) || workSeconds < 0.0) workSeconds = 0.0;
        workSeconds = std::min(workSeconds, m_cfg.maxFrameSeconds);

        // Budget available to sleep this frame = the period minus the work, plus any debt carried from a
        // previous over-budget frame (a negative carry shortens this sleep to catch back up).
        double avail = period - workSeconds + m_carry;
        double sleep;
        if (avail > 0.0) {
            sleep = avail;
            m_carry = 0.0;
        } else {
            sleep = 0.0;
            // Carry the deficit forward (drift correction), but bound it to one period so a single long
            // stall can't make the game sprint for many frames afterward.
            m_carry = m_cfg.driftCorrection ? std::max(avail, -period) : 0.0;
        }
        return sleep;
    }

    // Switch between the active and idle caps (e.g. idle on pause/menu/lost-focus to save battery).
    void setIdle(bool idle) { m_idle = idle; }
    bool idle() const { return m_idle; }

    void setActiveFps(double fps) { m_cfg.activeFps = fps; }
    void setIdleFps(double fps) { m_cfg.idleFps = fps; }

    // The cap in effect right now: the idle cap when idle mode is on and configured, else the active cap.
    double targetFps() const {
        if (m_idle && m_cfg.idleFps > 0.0) return m_cfg.idleFps;
        return m_cfg.activeFps;
    }

    // Seconds per frame at the current cap; 0 when uncapped (targetFps <= 0).
    double targetPeriod() const {
        const double fps = targetFps();
        return fps > 0.0 ? 1.0 / fps : 0.0;
    }

    // The frame rate you'd actually achieve given this much work: capped at the target, but lower when the
    // work alone already exceeds the budget (the cap can slow you down, never speed you up).
    double expectedFps(double workSeconds) const {
        if (!(std::isfinite(workSeconds)) || workSeconds < 0.0) workSeconds = 0.0;
        const double frame = std::max(targetPeriod(), workSeconds);
        return frame > 0.0 ? 1.0 / frame : 0.0;
    }

    // Clear the drift accumulator (call on a big discontinuity — scene load, resume from background).
    void reset() { m_carry = 0.0; }

    const FramePacerConfig& config() const { return m_cfg; }

private:
    static FramePacerConfig sanitize(FramePacerConfig c) {
        // Negative fps is meaningless; normalize to 0 (uncapped / disabled). A tiny positive maxFrame keeps
        // the clamp usable even if a caller zeroes it.
        if (!(std::isfinite(c.activeFps)) || c.activeFps < 0.0) c.activeFps = 0.0;
        if (!(std::isfinite(c.idleFps)) || c.idleFps < 0.0) c.idleFps = 0.0;
        if (!(std::isfinite(c.maxFrameSeconds)) || c.maxFrameSeconds <= 0.0) c.maxFrameSeconds = 0.25;
        return c;
    }

    FramePacerConfig m_cfg;
    double m_carry = 0.0; // drift debt (seconds), <= 0
    bool m_idle = false;
};

} // namespace maz::core
