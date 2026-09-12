#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::game {

// A first-person "fly" camera: a position plus yaw/pitch, driven by move()/look(), producing a
// view matrix. Engine-agnostic (math only) — the app maps its input to the move/look axes.
class FlyCamera {
public:
    // Rotate by yaw/pitch deltas (radians). Pitch is clamped to avoid flipping over.
    void look(float dYaw, float dPitch) {
        m_yaw += dYaw;
        m_pitch += dPitch;
        const float limit = 1.55f;
        m_pitch = m_pitch < -limit ? -limit : (m_pitch > limit ? limit : m_pitch);
    }

    // The world-space displacement for a move this frame, without applying it — so callers can
    // run it through collision before committing via setPosition().
    math::vec3 moveDelta(float forwardAmt, float rightAmt, float upAmt, float dt,
                         float speed = 6.0f) const {
        const math::vec3 f = forward();
        const math::vec3 r = math::normalize(math::cross(f, worldUp()));
        return (f * forwardAmt + r * rightAmt + worldUp() * upAmt) * (speed * dt);
    }

    // Move along the camera's own axes: +forward, +right (strafe), +up (world up). Amounts are
    // in [-1,1]; scaled by speed*dt.
    void move(float forwardAmt, float rightAmt, float upAmt, float dt, float speed = 6.0f) {
        m_pos += moveDelta(forwardAmt, rightAmt, upAmt, dt, speed);
    }

    math::vec3 forward() const {
        return math::vec3(std::cos(m_pitch) * std::cos(m_yaw), std::sin(m_pitch),
                          std::cos(m_pitch) * std::sin(m_yaw));
    }

    math::mat4 view() const { return glm::lookAt(m_pos, m_pos + forward(), worldUp()); }

    math::vec3 position() const { return m_pos; }
    void setPosition(const math::vec3& p) { m_pos = p; }
    void setYawPitch(float yaw, float pitch) {
        m_yaw = yaw;
        m_pitch = pitch;
    }

private:
    static math::vec3 worldUp() { return math::vec3(0.0f, 1.0f, 0.0f); }

    math::vec3 m_pos{0.0f, 2.0f, 8.0f};
    float m_yaw = -1.5708f; // radians; facing -Z
    float m_pitch = -0.15f;
};

} // namespace maz::game
