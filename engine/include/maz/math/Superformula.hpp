#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math Gielis superformula — a single polar equation that produces an enormous family of natural-looking
// closed shapes: circles, superellipses, polygons, stars, flowers, starfish, snowflakes and diatom outlines,
// all from six numbers. It generalises the superellipse (Superellipse.hpp) by adding an m-fold angular term,
// so it is the go-to procedural generator for organic sprites, petals, gems, shields and shockwave rings —
// none of which Godot offers as a primitive. The radius at polar angle θ is
//     r(θ) = ( |cos(mθ/4)/a|^n2 + |sin(mθ/4)/b|^n3 ) ^ (-1/n1),
// where `m` sets the angular symmetry / lobe count, `n1,n2,n3` the "pinch" of the lobes, and `a,b` the axis
// scales. m = 4, a = b = 1, n1 = n2 = n3 = 2 degenerates to the unit circle; with m = 4 and n1=n2=n3=n it is
// exactly the unit superellipse |x|^n + |y|^n = 1. Header-only, std-only, deterministic.
namespace maz::math {

// The superformula radius r(θ) for the given parameters. `a`,`b`,`n1` must be non-zero.
inline float superformulaRadius(float m, float n1, float n2, float n3, float a, float b, float theta) {
    const float t = m * theta / 4.0f;
    const float p1 = std::pow(std::fabs(std::cos(t) / a), n2);
    const float p2 = std::pow(std::fabs(std::sin(t) / b), n3);
    return std::pow(p1 + p2, -1.0f / n1);
}

// A point on the superformula curve centred at `center`, at polar angle `theta`: center + r(θ)·(cosθ, sinθ).
inline vec2 superformulaPoint(const vec2& center, float m, float n1, float n2, float n3, float a, float b,
                              float theta) {
    const float r = superformulaRadius(m, n1, n2, n3, a, b, theta);
    return vec2(center.x + r * std::cos(theta), center.y + r * std::sin(theta));
}

// A closed polyline of `samples` points sampling the full curve over θ ∈ [0, 2π) (the last point does NOT
// duplicate the first — connect back to index 0 to close it). `samples` is clamped to at least 3.
inline std::vector<vec2> superformulaPolyline(const vec2& center, float m, float n1, float n2, float n3,
                                              float a, float b, int samples) {
    if (samples < 3) {
        samples = 3;
    }
    const float twoPi = 6.28318530717958648f;
    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(samples));
    for (int i = 0; i < samples; ++i) {
        const float theta = twoPi * static_cast<float>(i) / static_cast<float>(samples);
        out.push_back(superformulaPoint(center, m, n1, n2, n3, a, b, theta));
    }
    return out;
}

} // namespace maz::math
