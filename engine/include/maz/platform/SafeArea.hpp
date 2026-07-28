#pragma once

#include "maz/math/Rect2.hpp"           // math::Rect2, math::vec2
#include "maz/platform/PlatformBackend.hpp" // platform::SafeAreaInsets (the POD the backend reports)

#include <algorithm>
#include <cmath>

// maz::platform safe-area geometry — the pure math for keeping UI clear of a phone/tablet's display
// intrusions: the notch / camera cutout, the rounded screen corners, the status bar up top, and the
// home-indicator / gesture bar along the bottom. The OS reports these as four inset thicknesses (in
// PHYSICAL pixels) eating into each edge of the drawable; the "safe area" is the rectangle that
// remains. A game that anchors its virtual joystick, action buttons, and HUD to the raw screen edges
// will put them UNDER the notch or the home bar on a real device — this is exactly what Godot's
// `DisplayServer.get_display_safe_area()` and CSS `env(safe-area-inset-*)` exist to prevent.
//
// The live inset values come from the platform (iOS `UIView.safeAreaInsets`, Android
// `WindowInsets.getDisplayCutout()` + system bars) via `PlatformBackend::safeAreaInsets()`; desktop
// and headless report zero, so on those targets every helper here is a harmless no-op that leaves
// layout untouched. This header is the deterministic, unit-tested consumer of those numbers — the same
// split as DisplayScale.hpp (pure math) vs. Window::contentScale() (the live query).
namespace maz::platform {

// Insets can arrive negative or absurdly large from a buggy source, or overlap (top+bottom exceeding
// the display height). Clamp each edge to [0, extent] and, when opposite edges together exceed the
// display, scale that pair down proportionally so the safe area never inverts to a negative size.
inline SafeAreaInsets sanitizeInsets(SafeAreaInsets in, int displayW, int displayH) {
    const float w = static_cast<float>(displayW > 0 ? displayW : 0);
    const float h = static_cast<float>(displayH > 0 ? displayH : 0);
    auto nonneg = [](float v) { return (std::isfinite(v) && v > 0.0f) ? v : 0.0f; };
    SafeAreaInsets out{nonneg(in.left), nonneg(in.top), nonneg(in.right), nonneg(in.bottom)};

    // Clamp a colliding pair (e.g. left+right > width) proportionally to the available extent.
    auto fitPair = [](float& a, float& b, float extent) {
        const float sum = a + b;
        if (sum > extent && sum > 0.0f) {
            const float k = extent / sum;
            a *= k;
            b *= k;
        }
    };
    fitPair(out.left, out.right, w);
    fitPair(out.top, out.bottom, h);
    return out;
}

// The usable rectangle after the insets are carved out of a `displayW x displayH` drawable, in the
// same physical-pixel space as touch/draw coordinates (origin top-left, +Y down). Godot's
// get_display_safe_area analogue. Always non-negative in size (insets are sanitized first).
inline math::Rect2 safeAreaRect(int displayW, int displayH, SafeAreaInsets in) {
    const SafeAreaInsets s = sanitizeInsets(in, displayW, displayH);
    const float w = static_cast<float>(displayW > 0 ? displayW : 0);
    const float h = static_cast<float>(displayH > 0 ? displayH : 0);
    return math::Rect2(s.left, s.top, std::max(0.0f, w - s.left - s.right),
                       std::max(0.0f, h - s.top - s.bottom));
}

// Pull a point inside the safe rectangle (min-inclusive, max-inclusive so an on-edge anchor is legal).
// Use it to keep a floating stick's rest position or a button center off the cutout. If the safe rect
// has no area the point is clamped to its origin.
inline math::vec2 clampPointToSafeArea(math::vec2 p, const math::Rect2& safe) {
    const float x = std::min(std::max(p.x, safe.left()), safe.right());
    const float y = std::min(std::max(p.y, safe.top()), safe.bottom());
    return math::vec2(x, y);
}

// Fit a HUD panel / control rect inside the safe area: first shrink it if it is larger than the safe
// area, then shift it so it lies fully within. Preserves size where it fits; matches how you'd re-home
// a status bar or minimap that would otherwise sit under the notch. Returns the adjusted rect.
inline math::Rect2 fitRectInSafeArea(const math::Rect2& r, const math::Rect2& safe) {
    math::vec2 size(std::min(r.size.x, safe.size.x), std::min(r.size.y, safe.size.y));
    float x = r.position.x;
    float y = r.position.y;
    // Shift inward from whichever edge it overflows (left/top take priority once size fits).
    if (x + size.x > safe.right()) x = safe.right() - size.x;
    if (y + size.y > safe.bottom()) y = safe.bottom() - size.y;
    if (x < safe.left()) x = safe.left();
    if (y < safe.top()) y = safe.top();
    return math::Rect2(math::vec2(x, y), size);
}

// Remap insets under a quarter-turn device rotation (clockwise). When you only know the native-portrait
// insets but the app is running rotated, the notch/home-bar move to different edges: a 90° CW turn
// sends top->right, right->bottom, bottom->left, left->top. `quarterTurnsCW` is taken mod 4 (negative
// allowed). Most platforms report insets already in the current orientation — this is for the case
// where you cache the portrait values and rotate them yourself.
inline SafeAreaInsets rotateInsets(SafeAreaInsets in, int quarterTurnsCW) {
    int q = quarterTurnsCW % 4;
    if (q < 0) q += 4;
    SafeAreaInsets r = in;
    for (int i = 0; i < q; ++i) {
        // 90° CW: new.top = old.left, new.right = old.top, new.bottom = old.right, new.left = old.bottom.
        r = SafeAreaInsets{r.bottom, r.left, r.top, r.right};
    }
    return r;
}

} // namespace maz::platform
