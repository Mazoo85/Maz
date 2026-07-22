#pragma once

#include "maz/render/Shapes.hpp" // shapes::MeshData, MeshVertex

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render UV / TEXEL-DENSITY analysis — the asset-QA pass that checks a mesh's texture coordinates are
// SANE before it ships: is the texel density uniform (so the texture looks equally sharp everywhere, no
// stretched or blurry patches), and how much of the UV square does the layout actually use? Every DCC and
// Godot's lightmap importer offer a "texel density / UV stretch" checker for exactly this. Per triangle it
// computes the ratio of UV area to 3D (world) area — constant across the mesh means consistent density; an
// outlier means a stretched or over-sampled face. It also totals the world and UV areas (UV coverage of the
// [0,1] square) and counts degenerate faces. Pure CPU, header-only, headless.
namespace maz::render {

struct UvDensityStats {
    std::vector<float> triDensity;    // per triangle: uvArea / worldArea (0 for a world-degenerate triangle)
    float minDensity = 0.0f;          // over non-degenerate triangles
    float maxDensity = 0.0f;
    float avgDensity = 0.0f;          // world-area-weighted mean (the texture-relevant average)
    float totalWorldArea = 0.0f;
    float totalUvArea = 0.0f;         // UV coverage; > 1 means overlap/tiling, < 1 means wasted atlas space
    std::uint32_t degenerateCount = 0; // triangles with ~zero 3D area
    std::uint32_t triangleCount = 0;
    // Ratio of the largest to smallest per-triangle density; 1.0 = perfectly uniform, higher = more stretch.
    float uniformityRatio() const { return minDensity > 0.0f ? maxDensity / minDensity : 0.0f; }
};

// Analyze the per-triangle UV vs world area of `mesh`. Density is the UV/world AREA ratio (the squared texel
// density); a constant value across the mesh means uniform texel density.
inline UvDensityStats analyzeUvDensity(const shapes::MeshData& mesh) {
    UvDensityStats s;
    s.triangleCount = static_cast<std::uint32_t>(mesh.indices.size() / 3);
    s.triDensity.assign(s.triangleCount, 0.0f);
    if (s.triangleCount == 0) return s;

    float mn = 1e30f, mx = -1e30f, weightedSum = 0.0f, weight = 0.0f;
    for (std::uint32_t t = 0; t < s.triangleCount; ++t) {
        const MeshVertex& a = mesh.vertices[mesh.indices[t * 3]];
        const MeshVertex& b = mesh.vertices[mesh.indices[t * 3 + 1]];
        const MeshVertex& c = mesh.vertices[mesh.indices[t * 3 + 2]];

        // 3D area = 0.5 * |(b-a) x (c-a)|.
        const float ux = b.px - a.px, uy = b.py - a.py, uz = b.pz - a.pz;
        const float vx = c.px - a.px, vy = c.py - a.py, vz = c.pz - a.pz;
        const float cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
        const float worldArea = 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);

        // UV area = 0.5 * |(uvB-uvA) x (uvC-uvA)| (2D cross).
        const float du1 = b.u - a.u, dv1 = b.v - a.v;
        const float du2 = c.u - a.u, dv2 = c.v - a.v;
        const float uvArea = 0.5f * std::fabs(du1 * dv2 - du2 * dv1);

        s.totalWorldArea += worldArea;
        s.totalUvArea += uvArea;
        if (worldArea < 1e-12f) {
            ++s.degenerateCount; // no meaningful density (leave triDensity[t] = 0)
            continue;
        }
        const float density = uvArea / worldArea;
        s.triDensity[t] = density;
        mn = std::min(mn, density);
        mx = std::max(mx, density);
        weightedSum += density * worldArea;
        weight += worldArea;
    }

    if (weight > 0.0f) {
        s.minDensity = mn;
        s.maxDensity = mx;
        s.avgDensity = weightedSum / weight;
    }
    return s;
}

} // namespace maz::render
