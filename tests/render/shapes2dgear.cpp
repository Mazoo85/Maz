// tests/render/shapes2dgear.cpp — verifies the gear/cog outline (render::shapes2d::gear). Ground truths: a gear with
// T teeth has 5*T points, all with radius between root and outer; exactly 2 tip points per tooth sit on the outer
// radius; the outline is CCW; it extrudes into a valid closed prism (a real cog); bad params are safe. Pure CPU.
#include "maz/render/Shapes2D.hpp"
#include "maz/render/PolyTriangulate.hpp"   // polygonArea, polygonSignedArea2
#include "maz/render/MeshExtrudePolygon.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool rel(float a, float b, float tolFrac) { return std::fabs(a - b) <= tolFrac * std::fabs(b) + 1e-4f; }

int main() {
    const int teeth = 12;
    const float outer = 3.0f, root = 2.0f;
    const auto g = shapes2d::gear(teeth, outer, root, 0.5f);

    // --- 1. Point count = 5 per tooth. ---
    {
        CHECK(g.size() == static_cast<std::size_t>(teeth) * 5u, "gear has 5 points per tooth");
    }

    // --- 2. Every vertex radius is within [root, outer]; exactly 2 tips per tooth on the outer circle. ---
    {
        bool inBand = true;
        int onOuter = 0, onRoot = 0;
        for (const maz::math::vec2& p : g) {
            const float r = std::sqrt(p.x * p.x + p.y * p.y);
            if (r < root - 1e-3f || r > outer + 1e-3f) inBand = false;
            if (rel(r, outer, 1e-4f)) ++onOuter;
            if (rel(r, root, 1e-4f)) ++onRoot;
        }
        CHECK(inBand, "every gear vertex sits between the root and outer radius");
        CHECK(onOuter == teeth * 2, "exactly two tip vertices per tooth on the outer circle");
        CHECK(onRoot == teeth * 3, "three root vertices per tooth (valley + two flank bases)");
    }

    // --- 3. CCW winding, and area is between the root disc and outer disc. ---
    {
        CHECK(polygonSignedArea2(g) > 0.0f, "gear outline is CCW");
        const float pi = 3.14159265f;
        const float a = polygonArea(g);
        CHECK(a > pi * root * root && a < pi * outer * outer, "gear area lies between the root and tip discs");
    }

    // --- 4. Composability: the gear extrudes into a valid closed cog (volume == area * depth). ---
    {
        const shapes::MeshData cog = extrudePolygon(g, 0.5f);
        CHECK(!cog.vertices.empty(), "gear extrudes into a mesh");
        double vol = 0.0;
        for (std::size_t t = 0; t < cog.indices.size(); t += 3) {
            const MeshVertex& a = cog.vertices[cog.indices[t]];
            const MeshVertex& b = cog.vertices[cog.indices[t + 1]];
            const MeshVertex& c = cog.vertices[cog.indices[t + 2]];
            const double cx = static_cast<double>(b.py) * c.pz - static_cast<double>(b.pz) * c.py;
            const double cy = static_cast<double>(b.pz) * c.px - static_cast<double>(b.px) * c.pz;
            const double cz = static_cast<double>(b.px) * c.py - static_cast<double>(b.py) * c.px;
            vol += static_cast<double>(a.px) * cx + static_cast<double>(a.py) * cy + static_cast<double>(a.pz) * cz;
        }
        CHECK(rel(static_cast<float>(vol / 6.0), polygonArea(g) * 0.5f, 1e-3f), "extruded cog volume == area * depth");
    }

    // --- 5. Bad params are safe. ---
    {
        CHECK(shapes2d::gear(2, 3.0f, 2.0f).empty(), "fewer than 3 teeth -> empty");
        CHECK(shapes2d::gear(10, 2.0f, 2.0f).empty(), "root >= outer -> empty");
        CHECK(shapes2d::gear(10, 3.0f, 0.0f).empty(), "zero root radius -> empty");
    }

    if (g_fail == 0) {
        std::printf("shapes2dgear: OK — 5/tooth, radii banded, 2 tips/tooth, CCW, extrudes to a cog, safe.\n");
        return 0;
    }
    std::printf("shapes2dgear: %d failure(s).\n", g_fail);
    return 1;
}
