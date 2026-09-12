#pragma once

#include "maz/render/ColorOps.hpp" // Color, Hsv, toHsv, fromHsv

#include <vector>

// maz::render COLOUR HARMONY — generate coordinated palettes from a single base colour by rotating its hue on the
// colour wheel: complementary, analogous, triadic, split-complementary, tetradic (square), and monochromatic
// shades. This is the "pick colours that go together" helper for procedural UI theming, generative art, team/faction
// colours, and data-viz legends — the tasteful companion to `gradientMap` and `simulateColorVision`. All rotations
// happen in HSV (hue in [0,1)); saturation, value, and alpha are preserved. Header-only, deterministic, headless.
namespace maz::render {

// Rotate a colour's hue by `turns` around the wheel (1.0 = full circle), preserving saturation/value/alpha.
inline Color rotateHue(const Color& c, float turns) {
    Hsv h = toHsv(c);
    h.h += turns;
    return fromHsv(h); // fromHsv wraps hue into [0,1)
}

// [base, opposite] — the base and its complement (hue +180°) for maximum contrast.
inline std::vector<Color> complementary(const Color& base) {
    return {base, rotateHue(base, 0.5f)};
}

// [base-spread, base, base+spread] — neighbours on the wheel for a calm, cohesive set. `spread` in turns
// (default 30° = 1/12).
inline std::vector<Color> analogous(const Color& base, float spread = 1.0f / 12.0f) {
    return {rotateHue(base, -spread), base, rotateHue(base, spread)};
}

// [base, +120°, +240°] — three evenly spaced hues; vivid but balanced.
inline std::vector<Color> triadic(const Color& base) {
    return {base, rotateHue(base, 1.0f / 3.0f), rotateHue(base, 2.0f / 3.0f)};
}

// [base, opposite-spread, opposite+spread] — the base plus the two hues flanking its complement; high contrast,
// less harsh than a straight complement. `spread` in turns (default 30° = 1/12).
inline std::vector<Color> splitComplementary(const Color& base, float spread = 1.0f / 12.0f) {
    return {base, rotateHue(base, 0.5f - spread), rotateHue(base, 0.5f + spread)};
}

// [base, +90°, +180°, +270°] — four hues on a square; a rich four-colour scheme.
inline std::vector<Color> tetradic(const Color& base) {
    return {base, rotateHue(base, 0.25f), rotateHue(base, 0.5f), rotateHue(base, 0.75f)};
}

// `count` shades of the base hue: same hue and saturation, brightness (value) stepped from dark up to the base's
// value. `count` < 1 → empty. Great for depth ramps, elevation tints, and single-hue UI themes.
inline std::vector<Color> monochromatic(const Color& base, int count) {
    std::vector<Color> out;
    if (count < 1) return out;
    const Hsv h = toHsv(base);
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        const float t = static_cast<float>(i + 1) / static_cast<float>(count); // (0,1]
        out.push_back(fromHsv(h.h, h.s, h.v * t, h.a));
    }
    return out;
}

} // namespace maz::render
