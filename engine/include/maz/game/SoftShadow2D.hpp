#pragma once

#include "maz/game/Visibility2D.hpp" // Segment2

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::game {

// ---- Soft (penumbra) 2D shadows via area-light sampling ----------------------------------------
// A point light (game::Visibility2D) casts a razor-sharp shadow: every point is either lit or not.
// A real light has SIZE, so shadow edges are soft — an inner UMBRA that sees none of the light, an
// outer fully-lit region, and a PENUMBRA between them that sees only part of the light. Godot's
// Light2D approximates this with a shadow filter. Here we model the light as a small DISC and sample
// it: cast a hard shadow from each sample point and average. A point that can reach every sample is
// fully lit; one that reaches none is in umbra; the fraction it can reach IS the soft-shadow value.
// Pure 2D math (no GPU), so it unit-tests headless; the renderer approximates it by compositing one
// faint visibility fan per sample additively.

#if defined(_MSC_VER)
#pragma warning(push)
// C4723: the divisions by rxs below are guarded — segmentsIntersect returns early for any
// |rxs| < 1e-9f — but MSVC cannot see through std::fabs and reports a potential divide by zero.
// C4723 is raised by the code generator rather than the parser, so the suppression has to wrap the
// whole function: a #pragma inside the body is applied too late to take effect.
#pragma warning(disable : 4723)
#endif
// Proper segment-segment intersection (strict interior crossing; shared endpoints / grazes don't count).
inline bool segmentsIntersect(math::vec2 p1, math::vec2 p2, math::vec2 q1, math::vec2 q2) {
    auto cross = [](math::vec2 a, math::vec2 b) { return a.x * b.y - a.y * b.x; };
    const math::vec2 r{p2.x - p1.x, p2.y - p1.y};
    const math::vec2 s{q2.x - q1.x, q2.y - q1.y};
    const float rxs = cross(r, s);
    if (std::fabs(rxs) < 1e-9f) {
        return false; // parallel or colinear -> treat as no crossing
    }
    const math::vec2 qp{q1.x - p1.x, q1.y - p1.y};
    const float t = cross(qp, s) / rxs;
    const float u = cross(qp, r) / rxs;
    const float eps = 1e-4f;
    return t > eps && t < 1.0f - eps && u > eps && u < 1.0f - eps;
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// Is the line of sight from a to b blocked by any occluder segment?
inline bool lineBlocked(math::vec2 a, math::vec2 b, const std::vector<Segment2>& occluders) {
    for (const Segment2& seg : occluders) {
        if (segmentsIntersect(a, b, seg.a, seg.b)) {
            return true;
        }
    }
    return false;
}

// Deterministic sample points across a light's disc (Vogel / sunflower spiral — even coverage, no RNG).
// `count` <= 0 returns a single point at the centre (degenerates to a point light).
inline std::vector<math::vec2> diskSamples(math::vec2 center, float radius, int count) {
    std::vector<math::vec2> pts;
    if (count <= 1 || radius <= 0.0f) {
        pts.push_back(center);
        return pts;
    }
    pts.reserve(static_cast<std::size_t>(count));
    const float goldenAngle = 2.399963229728653f; // pi * (3 - sqrt(5))
    for (int i = 0; i < count; ++i) {
        const float t = (static_cast<float>(i) + 0.5f) / static_cast<float>(count);
        const float rr = radius * std::sqrt(t); // sqrt -> uniform areal density
        const float ang = static_cast<float>(i) * goldenAngle;
        pts.push_back(math::vec2{center.x + std::cos(ang) * rr, center.y + std::sin(ang) * rr});
    }
    return pts;
}

// Fraction in [0,1] of the area light (disc at lightCenter/lightRadius, sampled by `samples` points)
// that is visible from p: 1 = fully lit, 0 = umbra, in between = penumbra.
inline float softVisibility(math::vec2 p, math::vec2 lightCenter, float lightRadius,
                            const std::vector<Segment2>& occluders, int samples = 16) {
    const std::vector<math::vec2> pts = diskSamples(lightCenter, lightRadius, samples);
    if (pts.empty()) {
        return 1.0f;
    }
    int visible = 0;
    for (const math::vec2& s : pts) {
        if (!lineBlocked(p, s, occluders)) {
            ++visible;
        }
    }
    return static_cast<float>(visible) / static_cast<float>(pts.size());
}

} // namespace maz::game
