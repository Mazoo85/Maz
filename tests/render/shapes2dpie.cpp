// tests/render/shapes2dpie.cpp — verifies the pie-slice / sector outline (render::shapes2d::pieSlice). Ground
// truths: a sector has segments+2 points (centre + segments+1 arc points); the first point is the centre and every
// arc point is on the radius; a quarter sector (sweep pi/2) has area 0.5*r^2*(pi/2); it is CCW; it extrudes into a
// valid closed wedge; bad params are safe. Pure CPU, headless.
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
    const float pi = 3.14159265358979323846f;
    const float r = 3.0f;
    const int seg = 16;
    const auto pie = shapes2d::pieSlice(r, 0.0f, pi * 0.5f, seg); // a quarter, from angle 0

    // --- 1. Point count = segments + 2 (centre + arc). ---
    {
        CHECK(pie.size() == static_cast<std::size_t>(seg) + 2u, "pie slice has segments+2 points");
    }

    // --- 2. First point is the centre; every arc point is on the radius. ---
    {
        CHECK(rel(pie[0].x, 0.0f, 1e-4f) && rel(pie[0].y, 0.0f, 1e-4f), "first point is the centre");
        bool onR = true;
        for (std::size_t i = 1; i < pie.size(); ++i)
            if (!rel(std::sqrt(pie[i].x * pie[i].x + pie[i].y * pie[i].y), r, 1e-4f)) onR = false;
        CHECK(onR, "every arc point sits on the radius");
    }

    // --- 3. CCW winding; area = 0.5 * r^2 * sweep (circular-sector area). ---
    {
        CHECK(polygonSignedArea2(pie) > 0.0f, "pie slice is wound CCW");
        const float expect = 0.5f * r * r * (pi * 0.5f);
        CHECK(rel(polygonArea(pie), expect, 3e-3f), "sector area = 0.5 * r^2 * sweep");
    }

    // --- 4. Composability: the pie slice extrudes into a valid closed wedge (volume = area * depth). ---
    {
        const shapes::MeshData wedge = extrudePolygon(pie, 0.5f);
        CHECK(!wedge.vertices.empty(), "pie slice extrudes into a mesh");
        double vol = 0.0;
        for (std::size_t t = 0; t < wedge.indices.size(); t += 3) {
            const MeshVertex& a = wedge.vertices[wedge.indices[t]];
            const MeshVertex& b = wedge.vertices[wedge.indices[t + 1]];
            const MeshVertex& c = wedge.vertices[wedge.indices[t + 2]];
            const double cx = static_cast<double>(b.py) * c.pz - static_cast<double>(b.pz) * c.py;
            const double cy = static_cast<double>(b.pz) * c.px - static_cast<double>(b.px) * c.pz;
            const double cz = static_cast<double>(b.px) * c.py - static_cast<double>(b.py) * c.px;
            vol += static_cast<double>(a.px) * cx + static_cast<double>(a.py) * cy + static_cast<double>(a.pz) * cz;
        }
        CHECK(rel(static_cast<float>(vol / 6.0), polygonArea(pie) * 0.5f, 2e-3f), "extruded wedge volume = area * depth");
    }

    // --- 5. Bad params are safe. ---
    {
        CHECK(shapes2d::pieSlice(0.0f, 0.0f, 1.0f, 8).empty(), "zero radius -> empty");
        CHECK(shapes2d::pieSlice(2.0f, 0.0f, 0.0f, 8).empty(), "zero sweep -> empty");
        CHECK(shapes2d::pieSlice(2.0f, 0.0f, 1.0f, 0).empty(), "zero segments -> empty");
    }

    if (g_fail == 0) {
        std::printf("shapes2dpie: OK — segments+2 pts, centre+radius, sector area, CCW, extrudes, safe.\n");
        return 0;
    }
    std::printf("shapes2dpie: %d failure(s).\n", g_fail);
    return 1;
}
