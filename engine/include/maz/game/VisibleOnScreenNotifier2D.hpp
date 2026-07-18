#pragma once

#include "maz/math/Rect2.hpp"

// maz::game VisibleOnScreenNotifier2D — Godot's node of the same name: watches whether an object's
// rectangle overlaps the camera's view and fires an edge event the moment it enters or leaves the
// screen. Games use it to wake/sleep behaviour cheaply — spawn an enemy only once its region scrolls
// into view, pause an off-screen animation, free a far-away emitter. It holds the object's world-space
// rect; update(view) compares it to the current camera rect and returns entered/exited transitions
// (not the steady state), tracking the on-screen flag between calls. Pure rect math, deterministic,
// header-only, unit-tested — no renderer or camera object required, just the two rectangles.
namespace maz::game {

struct ScreenNotifierEvents {
    bool entered = false; // rect became visible on THIS update
    bool exited = false;  // rect became hidden on THIS update
};

class VisibleOnScreenNotifier2D {
  public:
    VisibleOnScreenNotifier2D() = default;
    explicit VisibleOnScreenNotifier2D(const math::Rect2& rect) : m_rect(rect) {}

    void setRect(const math::Rect2& r) { m_rect = r; }
    const math::Rect2& rect() const { return m_rect; }
    bool isOnScreen() const { return m_onScreen; }

    // Reset the tracked state (e.g. when the object is re-pooled) without emitting an event.
    void reset(bool onScreen = false) { m_onScreen = onScreen; }

    // Compare the object's rect to the camera/view rect. Returns the transition (if any); the
    // steady on/off-screen state is available via isOnScreen(). Border touches count as visible so
    // an object flush against the screen edge is treated as on-screen (Godot's behaviour).
    ScreenNotifierEvents update(const math::Rect2& view) {
        const bool now = m_rect.intersects(view, /*includeBorders=*/true);
        ScreenNotifierEvents ev;
        if (now && !m_onScreen) {
            ev.entered = true;
        } else if (!now && m_onScreen) {
            ev.exited = true;
        }
        m_onScreen = now;
        return ev;
    }

  private:
    math::Rect2 m_rect;
    bool m_onScreen = false;
};

} // namespace maz::game
