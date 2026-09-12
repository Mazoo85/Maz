#pragma once

#include "maz/math/Math.hpp" // vec2

#include <cmath>
#include <cstddef>
#include <vector>

// maz::math polyline stroking — turn an OPEN path (a list of points) into a filled polygon outline of a given
// width, the CPU-side of what Godot's Line2D does on the GPU. This is how you render thick lines, drawn
// strokes/gestures, trails, roads/rivers, wires and route ribbons as an actual fillable/collidable polygon
// (feed the result to a triangulator or a polygon collider). The existing Geometry2D.offsetPolygonConvex
// inflates a CLOSED convex polygon; this handles the open-path case with end caps (butt / square / round)
// that the closed-polygon offset can't express. Bevel joins at interior vertices keep the outline valid for
// any turn angle. Header-only, std-only, deterministic.
namespace maz::math {

enum class StrokeCap { Butt, Square, Round };

struct StrokeOptions {
    float halfWidth = 1.0f;      // the stroke extends `halfWidth` to each side of the centreline
    StrokeCap cap = StrokeCap::Butt;
    int roundSegments = 8;       // arc subdivisions per round cap (>= 1)
};

// Build a closed polygon (CCW-ish ring, first point NOT duplicated at the end) that outlines the polyline
// `pts` stroked to `opt.halfWidth`. Returns empty for fewer than 2 points or a zero-length path.
inline std::vector<vec2> strokePolyline(const std::vector<vec2>& pts, const StrokeOptions& opt) {
    std::vector<vec2> out;
    if (pts.size() < 2 || opt.halfWidth <= 0.0f) {
        return out;
    }
    const float w = opt.halfWidth;
    const float pi = 3.14159265358979324f;

    // Per-segment left/right offset endpoints (left normal = rotate dir +90°).
    std::vector<vec2> left, right;    // left[2*i],left[2*i+1] = offsets of segment i's two ends
    std::vector<vec2> dir;            // unit direction of each segment
    for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
        vec2 d = pts[i + 1] - pts[i];
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1e-9f) {
            continue; // skip zero-length segment
        }
        d = d * (1.0f / len);
        const vec2 n(-d.y, d.x); // left normal
        dir.push_back(d);
        left.push_back(pts[i] + n * w);
        left.push_back(pts[i + 1] + n * w);
        right.push_back(pts[i] - n * w);
        right.push_back(pts[i + 1] - n * w);
    }
    if (dir.empty()) {
        return out;
    }
    const std::size_t nseg = dir.size();

    auto addCap = [&](const vec2& fromEdge, const vec2& toEdge, const vec2& centre, const vec2& outward) {
        // Connect the two edge points around the path end. `outward` points away from the path along the
        // centreline; used for square/round caps.
        switch (opt.cap) {
            case StrokeCap::Butt:
                // Nothing: the straight fromEdge->toEdge closes the end.
                break;
            case StrokeCap::Square: {
                out.push_back(fromEdge + outward * w);
                out.push_back(toEdge + outward * w);
                break;
            }
            case StrokeCap::Round: {
                const int segs = opt.roundSegments < 1 ? 1 : opt.roundSegments;
                const float a0 = std::atan2(fromEdge.y - centre.y, fromEdge.x - centre.x);
                float a1 = std::atan2(toEdge.y - centre.y, toEdge.x - centre.x);
                // Sweep through the `outward` side (the short way that bulges outward).
                float delta = a1 - a0;
                while (delta <= -pi) delta += 2.0f * pi;
                while (delta > pi) delta -= 2.0f * pi;
                // Ensure the arc bulges toward `outward`: midpoint of the arc should be on the outward side.
                const float amid = a0 + delta * 0.5f;
                const vec2 mid(std::cos(amid), std::sin(amid));
                if (mid.x * outward.x + mid.y * outward.y < 0.0f) {
                    delta = delta > 0.0f ? delta - 2.0f * pi : delta + 2.0f * pi;
                }
                for (int k = 1; k < segs; ++k) {
                    const float a = a0 + delta * static_cast<float>(k) / static_cast<float>(segs);
                    out.push_back(centre + vec2(std::cos(a), std::sin(a)) * w);
                }
                break;
            }
        }
    };

    // Forward down the left side (with bevel joins between consecutive segments).
    for (std::size_t i = 0; i < nseg; ++i) {
        out.push_back(left[2 * i]);
        out.push_back(left[2 * i + 1]);
    }
    // End cap: from the last left point around to the last right point.
    addCap(left[2 * (nseg - 1) + 1], right[2 * (nseg - 1) + 1], pts.back(), dir[nseg - 1]);
    // Backward up the right side.
    for (std::size_t i = nseg; i-- > 0;) {
        out.push_back(right[2 * i + 1]);
        out.push_back(right[2 * i]);
    }
    // Start cap: from the first right point around to the first left point.
    addCap(right[0], left[0], pts.front(), dir[0] * -1.0f);
    return out;
}

} // namespace maz::math
