#pragma once

#include "maz/render/ColorOps.hpp" // Color, luminance
#include "maz/render/Image.hpp"     // Image

#include <algorithm> // std::clamp

// maz::render COLOUR-BLINDNESS SIMULATION — an accessibility dev-tool: preview how the game's UI, minimap, team
// colours, or status effects look to players with colour-vision deficiency, so you can catch red/green pairs that
// become indistinguishable BEFORE shipping. Applies the widely-used Wickline dichromat transforms for the three
// common types (protanopia = red-weak, deuteranopia = green-weak, tritanopia = blue-weak) plus achromatopsia
// (total colour blindness → luminance grey). Works on a single `Color` or a whole `Image` (alpha preserved). Pair
// it with the procedural texture pipeline or run it over a UI screenshot. Header-only, deterministic, headless.
//
// Scope note (honest): the dichromat matrices operate on non-linear sRGB channels directly (the common, fast
// approximation used by web colour-blindness filters), not a physically-exact LMS/Brettel simulation; results are
// clamped to [0,1]. Rows of each matrix sum to 1, so a pure grey is left unchanged. An empty image → empty.
namespace maz::render {

// The colour-vision deficiency to simulate.
enum class ColorVision {
    Protanopia,     // red cones absent — reds darken, red/green confusable
    Deuteranopia,   // green cones absent — the most common; red/green confusable
    Tritanopia,     // blue cones absent — blue/yellow confusable
    Achromatopsia   // no colour at all — everything collapses to luminance grey
};

namespace detail {
// Wickline dichromat matrices (row-major 3x3, applied to sRGB). Each row sums to 1.
inline const float* cvdMatrix(ColorVision mode) {
    static const float protan[9]  = {0.567f, 0.433f, 0.000f, 0.558f, 0.442f, 0.000f, 0.000f, 0.242f, 0.758f};
    static const float deutan[9]  = {0.625f, 0.375f, 0.000f, 0.700f, 0.300f, 0.000f, 0.000f, 0.300f, 0.700f};
    static const float tritan[9]  = {0.950f, 0.050f, 0.000f, 0.000f, 0.433f, 0.567f, 0.000f, 0.475f, 0.525f};
    switch (mode) {
        case ColorVision::Protanopia:   return protan;
        case ColorVision::Deuteranopia: return deutan;
        case ColorVision::Tritanopia:   return tritan;
        default:                        return deutan;
    }
}
} // namespace detail

// Map a single colour through the chosen deficiency (alpha unchanged).
inline Color simulateColorVision(const Color& c, ColorVision mode) {
    if (mode == ColorVision::Achromatopsia) {
        const float l = std::clamp(luminance(c), 0.0f, 1.0f);
        return Color{l, l, l, c.a};
    }
    const float* m = detail::cvdMatrix(mode);
    const float r = std::clamp(m[0] * c.r + m[1] * c.g + m[2] * c.b, 0.0f, 1.0f);
    const float g = std::clamp(m[3] * c.r + m[4] * c.g + m[5] * c.b, 0.0f, 1.0f);
    const float b = std::clamp(m[6] * c.r + m[7] * c.g + m[8] * c.b, 0.0f, 1.0f);
    return Color{r, g, b, c.a};
}

// Map a whole image through the chosen deficiency (returns a new same-size image, alpha preserved).
inline Image simulateColorVision(const Image& src, ColorVision mode) {
    Image out(src.width(), src.height());
    if (out.empty()) return out;
    for (int y = 0; y < src.height(); ++y)
        for (int x = 0; x < src.width(); ++x)
            out.setPixel(x, y, simulateColorVision(src.getPixel(x, y), mode));
    return out;
}

} // namespace maz::render
