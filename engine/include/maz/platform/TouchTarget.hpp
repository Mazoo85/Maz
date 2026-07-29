#pragma once

#include "maz/math/Rect2.hpp" // math::Rect2, math::vec2

#include <algorithm>
#include <cmath>

// maz::platform touch-target sizing — the pure math for making tappable controls a COMFORTABLE physical size
// on any phone or tablet, regardless of how many pixels that device packs per inch. A button authored as
// "88 pixels" is a fat thumb-friendly target on a 160-dpi tablet but a near-impossible speck on a 560-dpi
// flagship; sizing in physical millimetres and converting through the display's DPI fixes that. The platform
// accessibility guidelines converge on ~9 mm (Google Material's 48 dp at 160 dpi ≈ 7.6 mm, Apple's 44 pt ≈
// 7 mm; 9 mm is a safe cross-platform floor). This is the deterministic, unit-tested consumer of a DPI value
// the caller reads from the display (SDL display content scale × 160, or an OS DPI query) — the same
// pure-policy split as DisplayScale.hpp (logical↔pixel) and SafeArea.hpp (insets→rect). Desktop mouse input
// has no such minimum, so a game simply passes a large DPI or skips these helpers there.
namespace maz::platform {

// Millimetres per inch — the constant that turns a DPI (dots/pixels per inch) into pixels-per-millimetre.
inline constexpr float kMmPerInch = 25.4f;

// The recommended minimum edge of a tappable control, in millimetres (cross-platform accessibility floor).
inline constexpr float kMinTouchTargetMm = 9.0f;

// Convert a physical length in millimetres to pixels on a display of the given DPI (pixels per inch).
// A non-positive dpi falls back to the canonical 160 dpi (1 dp == 1 px) so callers never divide by zero.
inline float mmToPixels(float mm, float dpi) {
    const float d = dpi > 0.0f ? dpi : 160.0f;
    return mm * (d / kMmPerInch);
}

// The recommended touch-target edge length in pixels for this display — size your buttons at least this big.
inline float recommendedTouchTargetPx(float dpi, float mm = kMinTouchTargetMm) {
    return mmToPixels(mm, dpi);
}

// Does a control of `widthPx` × `heightPx` meet the minimum touch target on this display? (Both edges must.)
inline bool meetsTouchTarget(float widthPx, float heightPx, float dpi, float mm = kMinTouchTargetMm) {
    const float minPx = recommendedTouchTargetPx(dpi, mm);
    return widthPx >= minPx && heightPx >= minPx;
}

// Grow a control rect to the minimum touch target if it is too small, keeping it centred on its current
// centre (so a small visual icon keeps its look but gains a comfortable hit area around it). A rect already
// at or above the minimum on an axis is left untouched on that axis.
inline math::Rect2 expandToTouchTarget(const math::Rect2& rect, float dpi, float mm = kMinTouchTargetMm) {
    const float minPx = recommendedTouchTargetPx(dpi, mm);
    const float w = std::max(rect.size.x, minPx);
    const float h = std::max(rect.size.y, minPx);
    const math::vec2 center{rect.position.x + rect.size.x * 0.5f, rect.position.y + rect.size.y * 0.5f};
    return math::Rect2{math::vec2{center.x - w * 0.5f, center.y - h * 0.5f}, math::vec2{w, h}};
}

// Derive a display's DPI from its pixel resolution and physical diagonal in inches — for when the OS reports
// screen size but not DPI directly. Returns 160 (the dp baseline) if the inputs are degenerate.
inline float dpiFromDiagonal(int widthPx, int heightPx, float diagonalInches) {
    if (widthPx <= 0 || heightPx <= 0 || diagonalInches <= 0.0f) {
        return 160.0f;
    }
    const float w = static_cast<float>(widthPx);
    const float h = static_cast<float>(heightPx);
    const float diagPx = std::sqrt(w * w + h * h);
    return diagPx / diagonalInches;
}

} // namespace maz::platform
