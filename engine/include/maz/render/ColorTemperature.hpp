#pragma once

#include <algorithm>
#include <cmath>

#include "maz/render/Renderer.hpp" // for render::Color

// maz::render::kelvinToColor — convert a color temperature in Kelvin to an approximate RGB tint, using
// the well-known Tanner Helland blackbody fit (valid roughly 1000-40000 K). Low temperatures are warm
// (candle ~1900K, tungsten ~2700-3200K, orange), ~6500K is neutral daylight white, and high
// temperatures are cool/blue (overcast/shade ~7000-10000K). The tool for physically-plausible light
// tints — day/night cycles that shift the sun from dawn-orange to noon-white, lamp/torch/fire glows,
// and camera white-balance-style grading. Godot has no built-in Kelvin->RGB helper. Header-only,
// std-only. Returns a Color with components in [0,1] and alpha 1.
namespace maz::render {

inline Color kelvinToColor(float kelvin) {
    const float clamped = std::clamp(kelvin, 1000.0f, 40000.0f);
    const double t = static_cast<double>(clamped) / 100.0;

    double r;
    double g;
    double b;

    // Red.
    if (t <= 66.0) {
        r = 255.0;
    } else {
        r = 329.698727446 * std::pow(t - 60.0, -0.1332047592);
    }

    // Green.
    if (t <= 66.0) {
        g = 99.4708025861 * std::log(t) - 161.1195681661;
    } else {
        g = 288.1221695283 * std::pow(t - 60.0, -0.0755148492);
    }

    // Blue.
    if (t >= 66.0) {
        b = 255.0;
    } else if (t <= 19.0) {
        b = 0.0;
    } else {
        b = 138.5177312231 * std::log(t - 10.0) - 305.0447927307;
    }

    const auto to01 = [](double v) {
        return static_cast<float>(std::clamp(v, 0.0, 255.0) / 255.0);
    };
    return Color{to01(r), to01(g), to01(b), 1.0f};
}

} // namespace maz::render
