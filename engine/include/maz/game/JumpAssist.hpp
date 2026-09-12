#pragma once

// maz::game jump-assist — the two small timers that make a platformer feel responsive instead of stiff:
// COYOTE TIME (you can still jump for a brief moment after walking off a ledge) and JUMP BUFFERING (a jump
// pressed just before landing fires the instant you touch the ground). Feed it the frame delta and whether
// the character is grounded via `update`, record button presses with `pressJump`, and each frame call
// `tryJump` — it returns true (consuming the buffered press) exactly when a jump should start: there is a
// live buffered press AND the character is grounded or still inside the coyote window. Both windows are
// configurable. This forgiving-input tuning is something Godot leaves entirely to the game -> beyond-Godot
// gameplay utility. Header-only, std-only, deterministic.
namespace maz::game {

class JumpAssist {
public:
    // `coyoteTime`: seconds after leaving the ground a jump is still allowed. `bufferTime`: seconds a jump
    // press stays queued waiting to become valid. Both clamped to >= 0.
    explicit JumpAssist(double coyoteTime = 0.1, double bufferTime = 0.1)
        : m_coyoteTime(coyoteTime < 0.0 ? 0.0 : coyoteTime),
          m_bufferTime(bufferTime < 0.0 ? 0.0 : bufferTime) {}

    // Advance timers by `dt`. When grounded, the coyote window is refreshed to full; when airborne it
    // counts down. The jump buffer always counts down. A non-positive dt still applies the grounded state.
    void update(double dt, bool grounded) {
        m_grounded = grounded;
        if (grounded) {
            m_coyote = m_coyoteTime;
        } else if (dt > 0.0) {
            m_coyote -= dt;
            if (m_coyote < 0.0) m_coyote = 0.0;
        }
        if (dt > 0.0) {
            m_buffer -= dt;
            if (m_buffer < 0.0) m_buffer = 0.0;
        }
    }

    // Queue a jump press (starts the buffer window).
    void pressJump() { m_buffer = m_bufferTime; }

    // True while the character may still jump (grounded or within the coyote window).
    bool canJump() const { return m_grounded || m_coyote > 0.0; }

    // True while a queued jump press is still live.
    bool hasBufferedJump() const { return m_buffer > 0.0; }

    // If a buffered press is live AND the character can jump, consume both windows and return true (the
    // frame the jump should start). Otherwise false.
    bool tryJump() {
        if (m_buffer > 0.0 && canJump()) {
            m_buffer = 0.0;
            m_coyote = 0.0;
            return true;
        }
        return false;
    }

    bool grounded() const { return m_grounded; }
    double coyoteRemaining() const { return m_coyote; }
    double bufferRemaining() const { return m_buffer; }

    // Clear both timers (e.g. after a forced state change).
    void reset() {
        m_coyote = 0.0;
        m_buffer = 0.0;
        m_grounded = false;
    }

private:
    double m_coyoteTime;
    double m_bufferTime;
    double m_coyote = 0.0;  // remaining coyote window
    double m_buffer = 0.0;  // remaining buffered-press window
    bool m_grounded = false;
};

} // namespace maz::game
