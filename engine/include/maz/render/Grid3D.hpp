#pragma once

#include "maz/math/Math.hpp"

#include <vector>

namespace maz::render {

// 3D reference grid + gizmo axes — the ground grid and RGB axis marker every 3D editor viewport draws
// (Godot's Node3D editor). Maz could draw lit meshes and debug lines, but had no builder for the two
// spatial-reference primitives you constantly want when placing things in 3D: a WORLD-SPACE GROUND GRID
// (so you can read scale and position on the XZ plane) and an ORIGIN GIZMO (the X=red / Y=green / Z=blue
// axes that show which way is which). This is pure geometry — it emits a list of colored line segments —
// so it has no renderer dependency and unit-tests headlessly; the app draws each segment through the
// existing debug-line path (Renderer::drawLine).

struct Line3 {
    math::vec3 a{0.0f, 0.0f, 0.0f};
    math::vec3 b{0.0f, 0.0f, 0.0f};
    math::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
};

struct GridSpec {
    int divisions = 10;    // grid lines run from -divisions..+divisions cells out from the center
    float spacing = 1.0f;  // world units per cell
    math::vec4 minorColor{0.34f, 0.38f, 0.44f, 1.0f}; // ordinary grid lines
    math::vec4 axisColor{0.5f, 0.55f, 0.62f, 1.0f};   // the two lines through the origin (x=0, z=0)
    bool gizmoAxes = true; // add the RGB origin axes
    float axisLength = 3.0f;
};

// Build an XZ-plane ground grid (+ optional RGB origin axes). Center lines get `axisColor`; the R/G/B
// gizmo axes run from the origin along +X (red), +Y (green), +Z (blue).
inline std::vector<Line3> buildGrid(const GridSpec& spec) {
    std::vector<Line3> out;
    const int n = spec.divisions < 0 ? 0 : spec.divisions;
    const float ext = static_cast<float>(n) * spec.spacing;
    for (int i = -n; i <= n; ++i) {
        const float c = static_cast<float>(i) * spec.spacing;
        const math::vec4 col = (i == 0) ? spec.axisColor : spec.minorColor;
        // Line parallel to X (varying x) at z = c.
        out.push_back(Line3{math::vec3(-ext, 0.0f, c), math::vec3(ext, 0.0f, c), col});
        // Line parallel to Z (varying z) at x = c.
        out.push_back(Line3{math::vec3(c, 0.0f, -ext), math::vec3(c, 0.0f, ext), col});
    }
    if (spec.gizmoAxes) {
        const float L = spec.axisLength;
        out.push_back(Line3{math::vec3(0, 0, 0), math::vec3(L, 0, 0), math::vec4(0.9f, 0.25f, 0.25f, 1)});
        out.push_back(Line3{math::vec3(0, 0, 0), math::vec3(0, L, 0), math::vec4(0.3f, 0.85f, 0.35f, 1)});
        out.push_back(Line3{math::vec3(0, 0, 0), math::vec3(0, 0, L), math::vec4(0.35f, 0.5f, 0.95f, 1)});
    }
    return out;
}

// Build the 12 edges of an axis-aligned box (min..max) as colored line segments — a wireframe bounding
// box you can place anywhere (unlike a filled AABB draw).
inline std::vector<Line3> buildWireBox(math::vec3 mn, math::vec3 mx, math::vec4 color) {
    const math::vec3 c[8] = {
        {mn.x, mn.y, mn.z}, {mx.x, mn.y, mn.z}, {mx.x, mn.y, mx.z}, {mn.x, mn.y, mx.z},
        {mn.x, mx.y, mn.z}, {mx.x, mx.y, mn.z}, {mx.x, mx.y, mx.z}, {mn.x, mx.y, mx.z},
    };
    const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0},  // bottom
                              {4, 5}, {5, 6}, {6, 7}, {7, 4},  // top
                              {0, 4}, {1, 5}, {2, 6}, {3, 7}}; // verticals
    std::vector<Line3> out;
    out.reserve(12);
    for (const auto& e : edges) {
        out.push_back(Line3{c[e[0]], c[e[1]], color});
    }
    return out;
}

} // namespace maz::render
