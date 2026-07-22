// tests/render/meshextrudering.cpp — verifies hollow-prism extrude (render::extrudeRing). Ground truths: a square
// frame (outer half-2, inner half-1) extruded depth 2 is a closed solid whose signed volume equals (outerArea -
// innerArea)*depth = 24; the prism is centred in z; the mesh has 8 triangles per segment (2 caps + 2 outer wall + 2
// inner wall, doubled across both caps... = 8/segment); mismatched loop sizes and <3 points are safe. Pure CPU.
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
    const std::vector<maz::math::vec2> outer = {{-2, -2}, {2, -2}, {2, 2}, {-2, 2}}; // area 16
    const std::vector<maz::math::vec2> inner = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}}; // area 4
    const float depth = 2.0f;

    // --- 1. Hollow square frame: closed solid, volume = (outerArea - innerArea) * depth. ---
    {
        const shapes::MeshData m = extrudeRing(outer, inner, depth);
        CHECK(!m.vertices.empty(), "ring extrudes into a mesh");
        const float expect = (polygonArea(outer) - polygonArea(inner)) * depth; // (16-4)*2 = 24
        CHECK(rel(static_cast<float>(signedVolume(m)), expect, 1e-4f), "volume = (outerArea - innerArea) * depth");
    }

    // --- 2. Triangle count = 8 per segment (2 back-cap + 2 front-cap + 2 outer wall + 2 inner wall). ---
    {
        const shapes::MeshData m = extrudeRing(outer, inner, depth);
        CHECK(m.indices.size() / 3 == outer.size() * 8, "8 triangles per segment (caps + both walls)");
    }

    // --- 3. Prism is centred in z. ---
    {
        const shapes::MeshData m = extrudeRing(outer, inner, depth);
        float minz = 1e9f, maxz = -1e9f;
        for (const MeshVertex& v : m.vertices) { minz = std::fmin(minz, v.pz); maxz = std::fmax(maxz, v.pz); }
        CHECK(rel(minz, -1.0f, 1e-5f) && rel(maxz, 1.0f, 1e-5f), "prism centred z=-depth/2..+depth/2");
    }

    // --- 4. A round washer (matched circle loops) also closes correctly. ---
    {
        std::vector<maz::math::vec2> ro, ri;
        const int seg = 16;
        const float twoPi = 6.28318530717958647692f;
        for (int i = 0; i < seg; ++i) {
            const float a = twoPi * static_cast<float>(i) / static_cast<float>(seg);
            ro.push_back(maz::math::vec2(std::cos(a) * 3.0f, std::sin(a) * 3.0f));
            ri.push_back(maz::math::vec2(std::cos(a) * 1.0f, std::sin(a) * 1.0f));
        }
        const shapes::MeshData w = extrudeRing(ro, ri, 1.0f);
        const float expect = (polygonArea(ro) - polygonArea(ri)) * 1.0f;
        CHECK(rel(static_cast<float>(signedVolume(w)), expect, 1e-3f), "round washer volume = (outer - inner) area * depth");
    }

    // --- 5. Degenerate inputs are safe. ---
    {
        CHECK(extrudeRing(outer, {{-1, -1}, {1, -1}, {1, 1}}, depth).vertices.empty(), "mismatched loop sizes -> empty");
        CHECK(extrudeRing({{0, 0}, {1, 1}}, {{0, 0}, {1, 1}}, depth).vertices.empty(), "<3 points -> empty");
        CHECK(extrudeRing(outer, inner, 0.0f).vertices.empty(), "zero depth -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshextrudering: OK — frame volume, 8 tris/seg, centred, round washer, safe.\n");
        return 0;
    }
    std::printf("meshextrudering: %d failure(s).\n", g_fail);
    return 1;
}
