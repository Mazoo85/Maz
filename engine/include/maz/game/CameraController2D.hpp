#pragma once

#include "maz/math/Math.hpp"

#include <cmath>

namespace maz::game {

// A 2D follow camera: the controller every 2D game needs but the engine only had the pieces for (a
// raw Camera2D data struct + a separate Shake). It tracks a target with three standard behaviors
// layered together:
//   * Deadzone — a box around the current focus the target can move within WITHOUT scrolling the
//     camera; only when the target leaves the box does the camera move (to put it back on the edge).
//     Kills jitter from tiny target motion.
//   * Smoothing — the camera eases toward its desired focus with a frame-rate-independent exponential
//     approach (higher = snappier), so scrolling feels weighty instead of locked.
//   * World bounds — the visible rectangle is clamped inside the level so the camera never shows past
//     the edges; if the world is smaller than the view on an axis, that axis is centered.
// A shake offset can be added on top without feeding back into the follow position. This is math-only
// (no renderer dependency): the app reads center()/zoom() to fill a render::Camera2D. Header-only.

class CameraController2D {
public:
    // Viewport in world units: half-extent = (width/2, height/2) / zoom (matches the sprite camera).
    void setViewport(float width, float height, float zoom = 1.0f) {
        m_zoom = zoom;
        m_viewHalf = math::vec2{width * 0.5f / zoom, height * 0.5f / zoom};
    }
    void setBounds(math::vec2 mn, math::vec2 mx) {
        m_hasBounds = true;
        m_min = mn;
        m_max = mx;
    }
    void clearBounds() { m_hasBounds = false; }
    void setDeadzone(math::vec2 halfExtents) { m_deadzone = halfExtents; }
    void setSmoothing(float perSecond) { m_smoothing = perSecond; }
    void follow(math::vec2 target) { m_target = target; }
    void setShakeOffset(math::vec2 offset) { m_shake = offset; }

    // Jump the focus straight to where it wants to be (use on scene load / teleport).
    void snap() { m_position = clampToBounds(desired()); }

    // Advance the focus toward the target for a frame of length dt.
    void update(float dt) {
        const math::vec2 d = desired();
        const float a = m_smoothing <= 0.0f ? 1.0f : 1.0f - std::exp(-m_smoothing * dt);
        m_position = m_position + (d - m_position) * a;
        m_position = clampToBounds(m_position);
    }

    // The smoothed focus (no shake) and the final camera center (with shake).
    math::vec2 position() const { return m_position; }
    math::vec2 center() const { return m_position + m_shake; }
    float zoom() const { return m_zoom; }
    math::vec2 viewHalf() const { return m_viewHalf; }

    // Map a world point to screen-pixel coordinates for the given framebuffer size (HUD markers,
    // off-screen culling). Accounts for shake and zoom.
    math::vec2 worldToScreen(math::vec2 world, float screenW, float screenH) const {
        const math::vec2 c = center();
        return math::vec2{(world.x - c.x) * m_zoom + screenW * 0.5f,
                          (world.y - c.y) * m_zoom + screenH * 0.5f};
    }

private:
    // The camera center that places the target at the edge of the deadzone (or leaves it put if the
    // target is inside the box).
    math::vec2 desired() const {
        const math::vec2 delta = m_target - m_position;
        const math::vec2 clamped{clampf(delta.x, -m_deadzone.x, m_deadzone.x),
                                 clampf(delta.y, -m_deadzone.y, m_deadzone.y)};
        return m_target - clamped;
    }

    math::vec2 clampToBounds(math::vec2 c) const {
        if (!m_hasBounds) return c;
        const float lox = m_min.x + m_viewHalf.x, hix = m_max.x - m_viewHalf.x;
        const float loy = m_min.y + m_viewHalf.y, hiy = m_max.y - m_viewHalf.y;
        math::vec2 out = c;
        out.x = lox <= hix ? clampf(c.x, lox, hix) : (m_min.x + m_max.x) * 0.5f;
        out.y = loy <= hiy ? clampf(c.y, loy, hiy) : (m_min.y + m_max.y) * 0.5f;
        return out;
    }

    static float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    math::vec2 m_position{0.0f, 0.0f};
    math::vec2 m_target{0.0f, 0.0f};
    math::vec2 m_deadzone{0.0f, 0.0f};
    math::vec2 m_viewHalf{0.0f, 0.0f};
    math::vec2 m_shake{0.0f, 0.0f};
    math::vec2 m_min{0.0f, 0.0f};
    math::vec2 m_max{0.0f, 0.0f};
    float m_smoothing = 10.0f;
    float m_zoom = 1.0f;
    bool m_hasBounds = false;
};

} // namespace maz::game
