#pragma once

#include "maz/math/Math.hpp"
#include "maz/platform/Input.hpp"

#include <cmath>
#include <cstdint>

namespace maz::input {

// Touch gesture recognition — the layer above raw touch points (platform::Input, WS1) that every mobile
// game wants: tap, double-tap, long-press, swipe/fling, and the two-finger pinch-zoom / rotate / pan. It is a
// pure per-frame state machine over the active contacts: update(input, dt) advances it, and one-frame edge
// queries (tapped(), swiped(), ...) report gestures the moment they complete. Deterministic and SDL-free —
// timing comes from the dt the caller passes, not a wall clock — so it unit-tests headlessly with synthetic
// touch sequences, exactly like input::VirtualControls. Coordinates are the same drawable pixels Input uses.

enum class SwipeDir { None, Left, Right, Up, Down };

// Thresholds tuning the recognizer. Defaults suit a ~1080p phone; scale with DPI if needed.
struct GestureConfig {
    float tapMaxTime = 0.30f;      // s: a contact released within this (and barely moved) is a tap
    float tapMaxMove = 24.0f;      // px: movement beyond this cancels a tap / long-press
    float doubleTapMaxGap = 0.30f; // s: max time between the two taps of a double-tap
    float doubleTapMaxDist = 48.0f;// px: the two taps must land near each other
    float longPressTime = 0.50f;   // s: a still contact held beyond this fires a long-press
    float swipeMinDist = 60.0f;    // px: total travel to count as a swipe/fling
    float swipeMaxTime = 0.40f;    // s: a swipe must be this quick (else it's just a drag)
};

class GestureDetector {
public:
    void setConfig(const GestureConfig& c) { m_cfg = c; }
    const GestureConfig& config() const { return m_cfg; }

    // Advance one frame: read the Input's touch points; dt is seconds since the last update.
    void update(const platform::Input& in, float dt) {
        // Clear one-frame edges from the previous update.
        m_tapped = m_doubleTapped = m_longPressed = m_swiped = false;
        m_swipeDir = SwipeDir::None;
        m_time += dt;

        const int count = in.touchCount();

        // --- Two-finger pinch / rotate / pan. Takes priority over single-finger tracking. ---
        if (count >= 2) {
            const platform::Touch& a = in.touch(0);
            const platform::Touch& b = in.touch(1);
            const math::vec2 pa(a.x, a.y), pb(b.x, b.y);
            const math::vec2 centroid((pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f);
            const float dist = length(pb - pa);
            const float angle = std::atan2(pb.y - pa.y, pb.x - pa.x);
            if (!m_pinchActive) {
                m_pinchActive = true;
                m_pinchStartDist = dist > 1e-3f ? dist : 1e-3f;
                m_pinchStartAngle = angle;
                m_pinchScale = 1.0f;
                m_pinchRotation = 0.0f;
                m_pinchPan = math::vec2(0.0f, 0.0f);
            } else {
                m_pinchScale = dist / m_pinchStartDist;
                m_pinchRotation = wrapAngle(angle - m_pinchStartAngle);
                m_pinchPan = centroid - m_pinchCentroid;
            }
            m_pinchCentroid = centroid;
            m_activeId = -1; // a pinch cancels any pending tap/long-press
            return;
        }
        if (m_pinchActive && count < 2) {
            m_pinchActive = false; // lifting to <2 fingers ends the pinch
        }

        // --- Single-finger: begin/track a contact, fire long-press while held & still. ---
        if (count == 1) {
            const platform::Touch& t = in.touch(0);
            if (m_activeId != t.id) {
                m_activeId = t.id;
                m_startPos = math::vec2(t.x, t.y);
                m_startTime = m_time;
                m_moved = false;
                m_longFired = false;
            } else {
                if (length(math::vec2(t.x, t.y) - m_startPos) > m_cfg.tapMaxMove) {
                    m_moved = true;
                }
                if (!m_moved && !m_longFired && (m_time - m_startTime) >= m_cfg.longPressTime) {
                    m_longPressed = true;
                    m_longFired = true;
                    m_longPressPos = math::vec2(t.x, t.y);
                }
            }
        }

        // --- Released contacts this frame: classify tap / double-tap / swipe. ---
        for (const platform::Touch& r : in.releasedTouches()) {
            if (r.id != m_activeId) {
                continue; // only the single finger we were tracking
            }
            const math::vec2 endPos(r.x, r.y);
            const float elapsed = m_time - m_startTime;
            const float travel = length(endPos - m_startPos);
            m_activeId = -1;

            if (m_longFired) {
                continue; // already consumed as a long-press
            }
            if (travel <= m_cfg.tapMaxMove && elapsed <= m_cfg.tapMaxTime) {
                // A tap. Is it the second of a double-tap?
                if (m_lastTapValid && (m_time - m_lastTapTime) <= m_cfg.doubleTapMaxGap &&
                    length(endPos - m_lastTapPos) <= m_cfg.doubleTapMaxDist) {
                    m_doubleTapped = true;
                    m_tapPos = endPos;
                    m_lastTapValid = false; // consume; a third tap starts fresh
                } else {
                    m_tapped = true;
                    m_tapPos = endPos;
                    m_lastTapValid = true;
                    m_lastTapTime = m_time;
                    m_lastTapPos = endPos;
                }
            } else if (travel >= m_cfg.swipeMinDist && elapsed <= m_cfg.swipeMaxTime &&
                       elapsed > 1e-4f) {
                m_swiped = true;
                const math::vec2 delta = endPos - m_startPos;
                m_swipeVel = math::vec2(delta.x / elapsed, delta.y / elapsed);
                // Dominant axis decides the cardinal direction (screen Y grows downward).
                if (std::fabs(delta.x) >= std::fabs(delta.y)) {
                    m_swipeDir = delta.x >= 0.0f ? SwipeDir::Right : SwipeDir::Left;
                } else {
                    m_swipeDir = delta.y >= 0.0f ? SwipeDir::Down : SwipeDir::Up;
                }
            }
        }
    }

    // --- one-frame edges (true only on the frame the gesture completes) ---
    bool tapped() const { return m_tapped; }
    bool doubleTapped() const { return m_doubleTapped; }
    bool longPressed() const { return m_longPressed; }
    bool swiped() const { return m_swiped; }
    math::vec2 tapPos() const { return m_tapPos; }
    math::vec2 longPressPos() const { return m_longPressPos; }
    SwipeDir swipeDir() const { return m_swipeDir; }
    math::vec2 swipeVelocity() const { return m_swipeVel; } // px/s

    // --- continuous two-finger gesture (valid while pinchActive()) ---
    bool pinchActive() const { return m_pinchActive; }
    float pinchScale() const { return m_pinchScale; }       // current/initial finger distance (1 = unchanged)
    float pinchRotation() const { return m_pinchRotation; } // radians, signed delta from the pinch start
    math::vec2 pinchPan() const { return m_pinchPan; }      // two-finger centroid movement this frame (px)

private:
    static float length(math::vec2 v) { return std::sqrt(v.x * v.x + v.y * v.y); }
    // Wrap an angle delta into (-pi, pi] so a pinch rotation past +/-180 doesn't jump.
    static float wrapAngle(float a) {
        const float twoPi = 6.28318530718f;
        while (a > 3.14159265359f) a -= twoPi;
        while (a <= -3.14159265359f) a += twoPi;
        return a;
    }

    GestureConfig m_cfg{};
    float m_time = 0.0f;

    // single-finger tracking
    int64_t m_activeId = -1;
    math::vec2 m_startPos{0.0f, 0.0f};
    float m_startTime = 0.0f;
    bool m_moved = false;
    bool m_longFired = false;

    // double-tap memory
    bool m_lastTapValid = false;
    float m_lastTapTime = 0.0f;
    math::vec2 m_lastTapPos{0.0f, 0.0f};

    // pinch state
    bool m_pinchActive = false;
    float m_pinchStartDist = 1.0f;
    float m_pinchStartAngle = 0.0f;
    math::vec2 m_pinchCentroid{0.0f, 0.0f};
    float m_pinchScale = 1.0f;
    float m_pinchRotation = 0.0f;
    math::vec2 m_pinchPan{0.0f, 0.0f};

    // one-frame edge results
    bool m_tapped = false, m_doubleTapped = false, m_longPressed = false, m_swiped = false;
    math::vec2 m_tapPos{0.0f, 0.0f};
    math::vec2 m_longPressPos{0.0f, 0.0f};
    SwipeDir m_swipeDir = SwipeDir::None;
    math::vec2 m_swipeVel{0.0f, 0.0f};
};

} // namespace maz::input
