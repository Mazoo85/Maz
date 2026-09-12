#pragma once

#include <functional>

// maz::game Timer — Godot's Timer node: a countdown that fires a "timeout" when it elapses. Set a
// wait time, start it, and tick(delta) each frame; when the countdown crosses zero it fires (via
// return count and an optional callback). one_shot stops after the first fire; otherwise it repeats,
// carrying the leftover time so the cadence never drifts (a big delta can fire it several times in
// one tick). Pausable and restartable. Pure logic — no clock; the caller supplies delta — so it is
// deterministic and unit-testable. Matches Godot's defaults: one_shot is false (repeating).
namespace maz::game {

class Timer {
  public:
    void setWaitTime(double t) { m_wait = t > 0.0 ? t : 0.0001; } // Godot requires wait_time > 0
    double waitTime() const { return m_wait; }

    void setOneShot(bool o) { m_oneShot = o; }
    bool oneShot() const { return m_oneShot; }

    void setAutostart(bool a) { m_autostart = a; }
    bool autostart() const { return m_autostart; }

    // Start (or restart) the countdown. A positive override also sets the wait time (Godot's
    // start(time_sec) behavior).
    void start(double waitOverride = -1.0) {
        if (waitOverride > 0.0) {
            m_wait = waitOverride;
        }
        m_timeLeft = m_wait;
        m_running = true;
        m_paused = false;
    }

    void stop() {
        m_running = false;
        m_timeLeft = 0.0;
    }

    void setPaused(bool p) { m_paused = p; }
    bool isPaused() const { return m_paused; }
    bool isStopped() const { return !m_running; }
    double timeLeft() const { return m_timeLeft; }

    // Advance the countdown by delta. Returns how many times it fired this tick (0+). A repeating
    // timer carries the remainder so its average rate stays exact; a one-shot stops on the first.
    int tick(double delta, const std::function<void()>& onTimeout = {}) {
        if (!m_running || m_paused || delta <= 0.0) {
            return 0;
        }
        int fires = 0;
        m_timeLeft -= delta;
        while (m_timeLeft <= 0.0) {
            ++fires;
            if (onTimeout) {
                onTimeout();
            }
            if (m_oneShot) {
                m_running = false;
                m_timeLeft = 0.0;
                break;
            }
            m_timeLeft += m_wait; // repeat, preserving the overshoot
        }
        return fires;
    }

  private:
    double m_wait = 1.0;
    double m_timeLeft = 0.0;
    bool m_oneShot = false; // Godot Timer default: repeating
    bool m_autostart = false;
    bool m_running = false;
    bool m_paused = false;
};

} // namespace maz::game
