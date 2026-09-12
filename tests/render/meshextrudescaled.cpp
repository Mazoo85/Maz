// tests/render/meshextrudescaled.cpp — verifies tapered extrude (render::extrudePolygonScaled). Ground truths:
// topScale=1 reproduces a straight prism (volume=area*depth); a tapered block's top cap is scaled about the centroid;
// the closed solid's signed volume follows the prismatoid rule A*h*(s^2+s+1)/3; bbox stays centred in z; a flare
// (s>1) has a bigger top than bottom; degenerate inputs are safe. Pure CPU, headless.
#include "maz/render/MeshExtrudePolygon.hpp"
#include "maz/render/PolyTriangulate.hpp" // polygonArea

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool rel(float a, float b, float tolFrac) { return std::fabs(a - b) <= tolFrac * std::fabs(b) + 1e-4f; }

static double signedVolume(const shapes::MeshData& m) {
    double v = 0.0;
    for (std::size_t t = 0; t < m.indices.size(); t += 3) {
        const MeshVertex& a = m.vertices[m.indices[t]];
        const MeshVertex& b = m.vertices[m.indices[t + 1]];
        const MeshVertex& c = m.vertices[m.indices[t + 2]];
        const double cx = static_cast<double>(b.py) * c.pz - static_cast<double>(b.pz) * c.py;
        const double cy = static_cast<double>(b.pz) * c.px - static_cast<double>(b.px) * c.pz;
        const double cz = static_cast<double>(b.px) * c.py - static_cast<double>(b.py) * c.px;
        v += static_cast<double>(a.px) * cx + static_cast<double>(a.py) * cy + static_cast<double>(a.pz) * cz;
    }
    return v / 6.0;
}

int main() {
    // A unit square centred on the origin (area 1).
    const std::vector<maz::math::vec2> sq = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
    const float A = 1.0f, depth = 2.0f;

    // --- 1. topScale=1 reproduces a straight prism (volume = area*depth). ---
    {
        const shapes::MeshData m = extrudePolygonScaled(sq, depth, 1.0f);
        CHECK(rel(static_cast<float>(signedVolume(m)), A * depth, 1e-4f), "topScale=1 is a straight prism");
    }

    // --- 2. A taper to 0.5: top cap is half-size; volume follows the prismatoid rule. ---
    {
        const float s = 0.5f;
        const shapes::MeshData m = extrudePolygonScaled(sq, depth, s);
        // Top cap (z=+1) extent: scaled square spans +/-0.25; bottom (z=-1) spans +/-0.5.
        float topMaxX = 0.0f, botMaxX = 0.0f;
        for (const MeshVertex& v : m.vertices) {
            if (v.pz > 0.9f) topMaxX = std::fmax(topMaxX, std::fabs(v.px));
            if (v.pz < -0.9f) botMaxX = std::fmax(botMaxX, std::fabs(v.px));
        }
        CHECK(rel(topMaxX, 0.25f, 1e-4f) && rel(botMaxX, 0.5f, 1e-4f), "top cap scaled to 0.5, base unchanged");
        const float expect = A * depth * (s * s + s + 1.0f) / 3.0f; // prismatoid volume
        CHECK(rel(static_cast<float>(signedVolume(m)), expect, 1e-3f), "tapered volume follows A*h*(s^2+s+1)/3");
    }

    // --- 3. A flare to 2.0: top bigger than bottom, still a valid closed solid (positive volume). ---
    {
        const float s = 2.0f;
        const shapes::MeshData m = extrudePolygonScaled(sq, depth, s);
        const float expect = A * depth * (s * s + s + 1.0f) / 3.0f;
        CHECK(rel(static_cast<float>(signedVolume(m)), expect, 1e-3f), "flare (s>1) volume follows the prismatoid rule");
    }

    // --- 4. bbox is centred in z. ---
    {
        const shapes::MeshData m = extrudePolygonScaled(sq, depth, 0.5f);
        float minz = 1e9f, maxz = -1e9f;
        for (const MeshVertex& v : m.vertices) { minz = std::fmin(minz, v.pz); maxz = std::fmax(maxz, v.pz); }
        CHECK(rel(minz, -1.0f, 1e-5f) && rel(maxz, 1.0f, 1e-5f), "prism centred z=-depth/2..+depth/2");
    }

    // --- 5. Degenerate inputs are safe. ---
    {
        CHECK(extrudePolygonScaled({{0, 0}, {1, 1}}, 1.0f, 0.5f).vertices.empty(), "<3 points -> empty");
        CHECK(extrudePolygonScaled(sq, 0.0f, 0.5f).vertices.empty(), "zero depth -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshextrudescaled: OK — prism at s=1, taper+flare prismatoid volume, top scaled, centred, safe.\n");
        return 0;
    }
    std::printf("meshextrudescaled: %d failure(s).\n", g_fail);
    return 1;
}
