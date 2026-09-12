#pragma once

#include "maz/render/ColorOps.hpp" // Color, srgbToLinear

// maz::render WCAG CONTRAST RATIO — the companion accessibility check to `simulateColorVision`: is text (or an icon)
// actually legible against its background? Implements the WCAG 2.x relative-luminance + contrast-ratio formula
// (ratio in [1, 21]) and the pass/fail thresholds for AA and AAA, normal and large text. Use it while theming a UI
// to guarantee readable HUDs, menus, and subtitles, or to auto-pick the more legible of black/white for a label on
// a coloured button. Header-only, deterministic, headless — pure colour math.
//
// Scope note (honest): follows the WCAG 2.x definition exactly (sRGB → linear via the standard EOTF, luminance
// weights 0.2126/0.7152/0.0722, ratio = (Llight+0.05)/(Ldark+0.05)); alpha is ignored (contrast is defined for
// opaque colours — composite over the real backdrop first if your text is translucent).
namespace maz::render {

// WCAG relative luminance of a colour (0 = black, 1 = white): linearise sRGB, then apply the luminance weights.
inline float relativeLuminance(const Color& c) {
    return 0.2126f * srgbToLinear(c.r) + 0.7152f * srgbToLinear(c.g) + 0.0722f * srgbToLinear(c.b);
}

// WCAG contrast ratio between two colours: (Llighter + 0.05) / (Ldarker + 0.05). 1 = identical, 21 = black/white.
inline float contrastRatio(const Color& a, const Color& b) {
    const float la = relativeLuminance(a), lb = relativeLuminance(b);
    const float hi = la > lb ? la : lb;
    const float lo = la > lb ? lb : la;
    return (hi + 0.05f) / (lo + 0.05f);
}

// WCAG 2.x AA legibility: normal text needs >= 4.5, large/bold text needs >= 3.0.
inline bool passesAA(const Color& fg, const Color& bg, bool largeText = false) {
    return contrastRatio(fg, bg) >= (largeText ? 3.0f : 4.5f);
}

// WCAG 2.x AAA legibility: normal text needs >= 7.0, large/bold text needs >= 4.5.
inline bool passesAAA(const Color& fg, const Color& bg, bool largeText = false) {
    return contrastRatio(fg, bg) >= (largeText ? 4.5f : 7.0f);
}

// Pick whichever of `dark` (default black) / `light` (default white) reads best on `bg` — the standard
// "auto text colour" trick for a label on an arbitrary background swatch.
inline Color bestTextColor(const Color& bg, const Color& dark = Color{0, 0, 0, 1},
                           const Color& light = Color{1, 1, 1, 1}) {
    return contrastRatio(dark, bg) >= contrastRatio(light, bg) ? dark : light;
}

} // namespace maz::render
