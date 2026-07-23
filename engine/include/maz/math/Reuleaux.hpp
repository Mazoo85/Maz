#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math Reuleaux polygon — a curve of CONSTANT WIDTH: no matter which direction you measure it, the
// distance between the two parallel lines that just touch it is the same, exactly like a circle (but it is
// not a circle). Built from a regular polygon with an ODD number of vertices by replacing every edge with a
// circular arc centred on the OPPOSITE vertex. The Reuleaux triangle (3 sides) is the guitar-pick / Wankel-
// rotor shape and the reason some manhole covers can't fall through their hole; higher odd counts (5, 7, …)
// give rounder constant-width shapes (the shape of the UK 20p/50p coins). Use it for distinctive procedural
// sprites/icons, rollers, cams and mechanisms, and gameplay props. Godot has no constant-width primitive.
// Returns a closed CCW polyline ready for the polygon fill / triangulator. Header-only, std-only,
// deterministic.
namespace maz::math {

// A closed CCW polyline of a Reuleaux polygon with `sides` vertices (must be ODD and ≥ 3; even/invalid
// values are bumped up to the next odd ≥ 3), the given constant `width`, centred at `center`, using
// `segmentsPerArc` points along each of the `sides` arcs. The first vertex sits at angle `phase` (radians).
inline std::vector<vec2> reuleauxPolygon(int sides, float width, const vec2& center = vec2(0.0f, 0.0f),
                                         int segmentsPerArc = 24, float phase = 1.5707963f) {
    if (sides < 3) {
        sides = 3;
    }
    if ((sides & 1) == 0) {
        sides += 1; // constant width requires an odd number of sides
    }
    if (segmentsPerArc < 2) {
        segmentsPerArc = 2;
    }
    const int n = sides;
    const float twoPi = 6.28318530717958648f;

    // Circumradius so the "long" chord (spanning (n-1)/2 edges) equals the requested width.
    const float m = static_cast<float>((n - 1) / 2);
    const float R = width / (2.0f * std::sin(twoPi * m / (2.0f * static_cast<float>(n))));

    std::vector<vec2> V(static_cast<std::size_t>(n));
    for (int k = 0; k < n; ++k) {
        const float a = phase + twoPi * static_cast<float>(k) / static_cast<float>(n);
        V[static_cast<std::size_t>(k)] = vec2(center.x + R * std::cos(a), center.y + R * std::sin(a));
    }

    std::vector<vec2> out;
    out.reserve(static_cast<std::size_t>(n * segmentsPerArc));
    const int opp = (n + 1) / 2; // step to the vertex opposite an edge
    for (int k = 0; k < n; ++k) {
        const vec2 start = V[static_cast<std::size_t>(k)];
        const vec2 end = V[static_cast<std::size_t>((k + 1) % n)];
        const vec2 c = V[static_cast<std::size_t>((k + opp) % n)]; // arc centre (opposite vertex)
        float a0 = std::atan2(start.y - c.y, start.x - c.x);
        float a1 = std::atan2(end.y - c.y, end.x - c.x);
        // Take the short way around (the outward-bulging minor arc).
        float d = a1 - a0;
        while (d > 3.14159265358979324f) d -= twoPi;
        while (d < -3.14159265358979324f) d += twoPi;
        for (int s = 0; s < segmentsPerArc; ++s) {
            const float t = static_cast<float>(s) / static_cast<float>(segmentsPerArc);
            const float a = a0 + d * t;
            out.push_back(vec2(c.x + width * std::cos(a), c.y + width * std::sin(a)));
        }
    }
    return out;
}

} // namespace maz::math
