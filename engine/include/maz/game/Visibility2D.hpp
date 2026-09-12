#pragma once

#include "maz/math/Math.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace maz::game {

// 2D visibility / light occlusion: from a point light, compute the polygon of everything it can see
// given a set of blocking segments (occluders) inside a bounding rectangle — the geometry behind 2D
// lights + shadows (Godot's Light2D + LightOccluder2D). The result is a star-shaped polygon around the
// light: fill it (a triangle fan from the light) to render the lit region; the notches carved out
// behind occluders ARE the shadows. Uses the classic angle-sweep algorithm — cast a ray toward every
// occluder endpoint (and just past each side of it) and keep the nearest hit. Header-only, dependency-
// free (2D math only), so it unit-tests without a GPU.

struct Segment2 {
    math::vec2 a, b;
};

class Visibility2D {
public:
    // Compute the visibility polygon from `light`, blocked by `occluders`, clipped to the box
    // [boundsMin, boundsMax]. The box edges are added as occluders so every ray hits something. The
    // returned vertices are in angular order around the light (a simple star-shaped polygon).
    static std::vector<math::vec2> compute(math::vec2 light, const std::vector<Segment2>& occluders,
                                           math::vec2 boundsMin, math::vec2 boundsMax) {
        std::vector<Segment2> segs = occluders;
        // Bounding box edges.
        const math::vec2 c0{boundsMin.x, boundsMin.y}, c1{boundsMax.x, boundsMin.y};
        const math::vec2 c2{boundsMax.x, boundsMax.y}, c3{boundsMin.x, boundsMax.y};
        segs.push_back(Segment2{c0, c1});
        segs.push_back(Segment2{c1, c2});
        segs.push_back(Segment2{c2, c3});
        segs.push_back(Segment2{c3, c0});

        // Cast a ray toward each endpoint, nudged just to either side to slip past corners.
        std::vector<float> angles;
        angles.reserve(segs.size() * 6);
        for (const Segment2& s : segs) {
            for (const math::vec2& p : {s.a, s.b}) {
                const float base = std::atan2(p.y - light.y, p.x - light.x);
                angles.push_back(base - 0.00015f);
                angles.push_back(base);
                angles.push_back(base + 0.00015f);
            }
        }
        std::sort(angles.begin(), angles.end());

        std::vector<math::vec2> poly;
        poly.reserve(angles.size());
        for (float ang : angles) {
            const math::vec2 dir{std::cos(ang), std::sin(ang)};
            float best = 1e30f;
            for (const Segment2& s : segs) {
                const float t = raySegment(light, dir, s.a, s.b);
                if (t > 1e-4f && t < best) best = t;
            }
            if (best < 1e30f) poly.push_back(math::vec2{light.x + dir.x * best, light.y + dir.y * best});
        }
        return poly;
    }

    // Distance t along the ray (origin O, unit dir D) to segment AB, or -1 if it doesn't hit.
    static float raySegment(math::vec2 O, math::vec2 D, math::vec2 A, math::vec2 B) {
        const math::vec2 sd{B.x - A.x, B.y - A.y};
        const float denom = sd.x * D.y - sd.y * D.x;
        if (std::fabs(denom) < 1e-9f) return -1.0f; // parallel
        const float t2 = (D.x * (A.y - O.y) + D.y * (O.x - A.x)) / denom; // fraction along segment
        if (t2 < 0.0f || t2 > 1.0f) return -1.0f;
        // t1 = distance along ray; use whichever component of D is larger to avoid /0.
        const float t1 = std::fabs(D.x) > std::fabs(D.y) ? (A.x + sd.x * t2 - O.x) / D.x
                                                         : (A.y + sd.y * t2 - O.y) / D.y;
        return t1 > 0.0f ? t1 : -1.0f;
    }

    // Is point p inside the (star-shaped, angle-ordered) visibility polygon? Ray-cast even-odd test.
    static bool contains(const std::vector<math::vec2>& poly, math::vec2 p) {
        const size_t n = poly.size();
        if (n < 3) return false;
        bool inside = false;
        for (size_t i = 0, j = n - 1; i < n; j = i++) {
            const math::vec2& vi = poly[i];
            const math::vec2& vj = poly[j];
            if (((vi.y > p.y) != (vj.y > p.y)) &&
                (p.x < (vj.x - vi.x) * (p.y - vi.y) / (vj.y - vi.y) + vi.x)) {
                inside = !inside;
            }
        }
        return inside;
    }
};

} // namespace maz::game
