#pragma once

#include "maz/math/Rect2.hpp" // math::Rect2, math::vec2

#include <algorithm>
#include <cmath>

// maz::platform thumb-zone control placement — the pure math for putting on-screen controls where a player's
// thumbs actually reach. Holding a phone in two hands, the thumbs sweep arcs anchored at the BOTTOM-LEFT and
// BOTTOM-RIGHT corners; the top of the screen and the far corners are awkward or impossible to hit one-handed.
// Every mobile game answers the same question — "where do the movement stick and the action buttons go?" — and
// the good answer is "in the bottom thumb zones, inside the safe area, sized for a fingertip." This header is
// the layout layer that sits between SafeArea.hpp (the usable rectangle) and TouchTarget.hpp (a comfortable
// control size): give it the safe rect and your control sizes and it returns the rects to draw and hit-test.
// Pure geometry, unit-tested headlessly; coordinates are drawable pixels with +y DOWN (top-left origin), the
// same space SafeArea.hpp produces. `apps/_template` anchors its controls this way.
namespace maz::platform {

// Which hand drives the primary ACTION buttons. Right-handed (the common default) puts the action cluster on
// the right and the movement stick on the left; left-handed swaps them.
enum class Handedness { RightHanded, LeftHanded };

// A resolved on-screen control layout: the movement stick and the action-button cluster, each a square-ish
// rect in drawable pixels ready to draw and hit-test.
struct TouchControlLayout {
    math::Rect2 moveStick;     // the movement stick / d-pad region
    math::Rect2 actionCluster; // the action-button cluster region
};

// Anchor a `w`×`h` control to the bottom-LEFT of the safe rect, `margin` pixels in from the left and bottom
// edges. The size is clamped so the control never spills outside the safe rectangle on a small screen.
inline math::Rect2 bottomLeftZone(const math::Rect2& safe, float w, float h, float margin = 0.0f) {
    const float cw = std::clamp(w, 0.0f, std::max(0.0f, safe.size.x - 2.0f * margin));
    const float ch = std::clamp(h, 0.0f, std::max(0.0f, safe.size.y - 2.0f * margin));
    return math::Rect2{math::vec2{safe.left() + margin, safe.bottom() - margin - ch}, math::vec2{cw, ch}};
}

// Anchor a `w`×`h` control to the bottom-RIGHT of the safe rect, `margin` pixels in from the right and bottom.
inline math::Rect2 bottomRightZone(const math::Rect2& safe, float w, float h, float margin = 0.0f) {
    const float cw = std::clamp(w, 0.0f, std::max(0.0f, safe.size.x - 2.0f * margin));
    const float ch = std::clamp(h, 0.0f, std::max(0.0f, safe.size.y - 2.0f * margin));
    return math::Rect2{math::vec2{safe.right() - margin - cw, safe.bottom() - margin - ch}, math::vec2{cw, ch}};
}

// Place a twin-control layout (a movement stick + an action-button cluster) in the natural bottom thumb zones
// inside the safe area. Right-handed: stick bottom-left, actions bottom-right; left-handed swaps them. Sizes
// are the square edge lengths in pixels (pair with TouchTarget.hpp so buttons inside the cluster stay tappable).
inline TouchControlLayout thumbControlLayout(const math::Rect2& safe, float stickPx, float clusterPx,
                                             float margin = 0.0f, Handedness hand = Handedness::RightHanded) {
    const math::Rect2 left = bottomLeftZone(safe, stickPx, stickPx, margin);
    const math::Rect2 right = bottomRightZone(safe, clusterPx, clusterPx, margin);
    TouchControlLayout out;
    if (hand == Handedness::RightHanded) {
        out.moveStick = left;      // move with the left thumb
        out.actionCluster = right; // act with the right thumb
    } else {
        // Left-handed: action buttons under the left thumb, movement under the right.
        out.actionCluster = bottomLeftZone(safe, clusterPx, clusterPx, margin);
        out.moveStick = bottomRightZone(safe, stickPx, stickPx, margin);
    }
    return out;
}

// A heuristic thumb-reach radius from a bottom corner: roughly how far the thumb comfortably sweeps, as a
// fraction of the safe area's SHORTER edge (default ~0.62 covers the classic reachable arc). Use it to keep
// important controls inside the arc, or to fade hint UI that sits beyond it.
inline float thumbReachRadius(const math::Rect2& safe, float fraction = 0.62f) {
    return std::min(safe.size.x, safe.size.y) * std::max(0.0f, fraction);
}

// Is `point` within `radius` of the given bottom corner (true = comfortably reachable by that thumb)?
inline bool withinThumbReach(const math::vec2& point, const math::vec2& corner, float radius) {
    const float dx = point.x - corner.x;
    const float dy = point.y - corner.y;
    return (dx * dx + dy * dy) <= radius * radius;
}

} // namespace maz::platform
