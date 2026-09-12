#pragma once

#include "maz/input/Analog.hpp"
#include "maz/math/Math.hpp"
#include "maz/platform/Input.hpp"

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace maz::input {

// On-screen virtual controls — the touch equivalent of a gamepad, so a phone/tablet build of a game that
// was designed around a stick+buttons plays without any physical controller. Two widget kinds:
//
//   * VirtualStick — a thumb region that yields a normalized [-1,1] vector, conditioned with the same radial
//     deadzone maths as a real thumbstick (input::analogVector). Two placement modes matching every mobile
//     twin-stick shipping today:
//       - Fixed:    the base sits at a set screen position; the vector is (touch - base) / radius.
//       - Floating: the base recenters to wherever the thumb first lands inside an activation zone, so the
//                   player never has to find a printed ring. This is the default for movement sticks.
//   * VirtualButton — a circular hit region yielding held / pressed (edge down) / released (edge up),
//     matching platform::Input's button edge semantics so gameplay reads it like pad::Button.
//
// The whole thing is a thin consumer of platform::Input's touch points (WS1): update() walks the active
// contacts once per frame, claims one finger per stick/button by id (so multi-touch — move with the left
// thumb while the right thumb aims — just works), and computes edges from the previous frame's claim. It is
// render-free: the game draws the rings/buttons with its own 2D renderer using the exposed base/knob
// positions. Coordinates are screen-space PIXELS, exactly what Input's touch points and mouse use, so a
// desktop build can drive the same controls with a synthesized touch for testing. Header-only, std + math.

// A resolved stick reading for this frame.
struct StickState {
    math::vec2 value{0.0f, 0.0f}; // conditioned [-1,1] vector (radial deadzone, clamped to unit circle)
    bool active = false;          // a finger is currently driving this stick
    math::vec2 base{0.0f, 0.0f};  // ring center in pixels (for drawing; moves when a floating stick recenters)
    math::vec2 knob{0.0f, 0.0f};  // thumb position clamped to the ring (for drawing)
};

// A configurable virtual thumbstick. `floating` recenters the base to first-touch; otherwise the base is
// fixed at `center`. `radius` is the throw distance in pixels that maps to full tilt. `activationRadius`
// (floating only) is how far from `center` a touch may land and still grab this stick; 0 means "anywhere".
class VirtualStick {
public:
    VirtualStick() = default;
    VirtualStick(math::vec2 center, float radius, bool floating = true)
        : m_center(center), m_radius(radius > 1.0f ? radius : 1.0f), m_floating(floating) {}

    void setCenter(math::vec2 c) { m_center = c; }
    void setRadius(float r) { m_radius = r > 1.0f ? r : 1.0f; }
    void setFloating(bool f) { m_floating = f; }
    void setDeadzone(float dz) { m_deadzone = dz; }
    void setActivationRadius(float r) { m_activationRadius = r < 0.0f ? 0.0f : r; }

    // Advance one frame against the current touch state. `claimed` marks finger ids already taken by an
    // earlier widget this frame so no two widgets react to the same contact.
    void update(const platform::Input& in, std::vector<int64_t>& claimed) {
        // If we already own a finger, keep tracking it until it lifts.
        if (m_activeId >= 0) {
            const platform::Touch* t = in.touchById(m_activeId);
            if (t) {
                m_knobPos = math::vec2(t->x, t->y);
                claimed.push_back(m_activeId);
                return;
            }
            m_activeId = -1; // finger lifted
        }
        // Look for an unclaimed contact that started this frame inside our activation zone.
        for (int i = 0; i < in.touchCount(); ++i) {
            const platform::Touch& t = in.touch(i);
            if (isClaimed(claimed, t.id)) {
                continue;
            }
            if (!in.touchPressed(i)) {
                continue; // only grab on the frame a finger goes down, so it doesn't steal a held aim finger
            }
            const math::vec2 p(t.x, t.y);
            if (m_floating) {
                if (m_activationRadius > 0.0f && dist(p, m_center) > m_activationRadius) {
                    continue; // landed outside this stick's half of the screen
                }
                m_base = p; // recenter under the thumb
            } else {
                if (dist(p, m_center) > m_radius) {
                    continue; // fixed stick: must touch within the ring
                }
                m_base = m_center;
            }
            m_activeId = t.id;
            m_knobPos = p;
            claimed.push_back(t.id);
            return;
        }
    }

    // Resolve the reading. For a fixed, inactive stick the base is `center`; floating rests at last base.
    StickState state() const {
        StickState s;
        s.active = m_activeId >= 0;
        s.base = (m_floating ? m_base : m_center);
        if (!s.active) {
            s.knob = s.base;
            return s;
        }
        math::vec2 off(m_knobPos.x - s.base.x, m_knobPos.y - s.base.y);
        // Screen Y grows downward; negate so "up" is +Y in the returned vector, like a thumbstick.
        const math::vec2 raw(off.x / m_radius, -off.y / m_radius);
        s.value = analogVector(raw, m_deadzone);
        // Clamp the drawn knob to the ring.
        const float len = std::sqrt(off.x * off.x + off.y * off.y);
        if (len > m_radius && len > 0.0f) {
            const float k = m_radius / len;
            off = math::vec2(off.x * k, off.y * k);
        }
        s.knob = math::vec2(s.base.x + off.x, s.base.y + off.y);
        return s;
    }

    math::vec2 center() const { return m_center; }
    float radius() const { return m_radius; }
    bool active() const { return m_activeId >= 0; }

private:
    static float dist(math::vec2 a, math::vec2 b) {
        const float dx = a.x - b.x, dy = a.y - b.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    static bool isClaimed(const std::vector<int64_t>& claimed, int64_t id) {
        for (int64_t c : claimed) {
            if (c == id) {
                return true;
            }
        }
        return false;
    }

    math::vec2 m_center{0.0f, 0.0f};
    math::vec2 m_base{0.0f, 0.0f};   // live ring center (== center for fixed; recenters for floating)
    math::vec2 m_knobPos{0.0f, 0.0f};
    float m_radius = 100.0f;
    float m_deadzone = 0.15f;
    float m_activationRadius = 0.0f; // 0 = unrestricted
    bool m_floating = true;
    int64_t m_activeId = -1;
};

// A circular on-screen button. Held while any unclaimed contact sits inside its radius; pressed/released are
// one-frame edges derived from the previous frame's held state.
class VirtualButton {
public:
    VirtualButton() = default;
    VirtualButton(math::vec2 center, float radius) : m_center(center), m_radius(radius > 1.0f ? radius : 1.0f) {}

    void setCenter(math::vec2 c) { m_center = c; }
    void setRadius(float r) { m_radius = r > 1.0f ? r : 1.0f; }

    void update(const platform::Input& in, std::vector<int64_t>& claimed) {
        m_prevHeld = m_held;
        m_held = false;
        // Prefer to keep the finger we already own (so a slide off-and-on doesn't flicker mid-press).
        if (m_activeId >= 0) {
            const platform::Touch* t = in.touchById(m_activeId);
            if (t && inside(math::vec2(t->x, t->y))) {
                m_held = true;
                claimed.push_back(m_activeId);
                return;
            }
            m_activeId = -1;
        }
        for (int i = 0; i < in.touchCount(); ++i) {
            const platform::Touch& t = in.touch(i);
            if (isClaimed(claimed, t.id)) {
                continue;
            }
            if (inside(math::vec2(t.x, t.y))) {
                m_activeId = t.id;
                m_held = true;
                claimed.push_back(t.id);
                return;
            }
        }
    }

    bool held() const { return m_held; }
    bool pressed() const { return m_held && !m_prevHeld; }   // edge down this frame
    bool released() const { return !m_held && m_prevHeld; }  // edge up this frame
    math::vec2 center() const { return m_center; }
    float radius() const { return m_radius; }

private:
    bool inside(math::vec2 p) const {
        const float dx = p.x - m_center.x, dy = p.y - m_center.y;
        return dx * dx + dy * dy <= m_radius * m_radius;
    }
    static bool isClaimed(const std::vector<int64_t>& claimed, int64_t id) {
        for (int64_t c : claimed) {
            if (c == id) {
                return true;
            }
        }
        return false;
    }

    math::vec2 m_center{0.0f, 0.0f};
    float m_radius = 60.0f;
    bool m_held = false;
    bool m_prevHeld = false;
    int64_t m_activeId = -1;
};

// A small twin-stick layout convenience: a movement stick, an aim stick, and a set of action buttons, all
// sharing one claim list so a finger drives exactly one widget per frame. A game can use the widgets directly
// instead; this just bundles the common ZOMBOID-style config and the correct per-frame update order.
class VirtualControls {
public:
    VirtualStick moveStick;
    VirtualStick aimStick;
    std::vector<VirtualButton> buttons;

    // Update every widget against this frame's touch state. Sticks are updated first (they only grab on the
    // press frame), then buttons, so a press inside a stick's ring drives the stick, not a co-located button.
    void update(const platform::Input& in) {
        m_claimed.clear();
        moveStick.update(in, m_claimed);
        aimStick.update(in, m_claimed);
        for (VirtualButton& b : buttons) {
            b.update(in, m_claimed);
        }
    }

    // How many contacts were consumed by controls this frame (diagnostics / tests).
    int claimedCount() const { return static_cast<int>(m_claimed.size()); }

private:
    std::vector<int64_t> m_claimed;
};

} // namespace maz::input
