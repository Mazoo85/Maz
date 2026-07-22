// tests/render/shapes2d.cpp — verifies the 2D outline generators (render::shapes2d). Ground truths: a regular n-gon
// has n points all at the radius with the exact circumradius-polygon area; a star has 2*points vertices alternating
// outer/inner radius; a rounded rect fits width x height with area = w*h - (4-pi)*r^2; all outlines are CCW and
// simple, and they extrude into valid solids. Pure CPU, headless.
#include "maz/render/Shapes2D.hpp"
#include "maz/render/PolyTriangulate.hpp"   // polygonArea, polygonSignedArea2
#include "maz/render/MeshExtrudePolygon.hpp" // composability check

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool rel(float a, float b, float tolFrac) { return std::fabs(a - b) <= tolFrac * std::fabs(b) + 1e-4f; }

int main() {
    // --- 1. Regular hexagon: 6 points at radius 2, CCW, correct area. ---
    {
        const auto hex = shapes2d::regularPolygon(6, 2.0f);
        CHECK(hex.size() == 6, "hexagon has 6 points");
        bool onR = true;
        for (const maz::math::vec2& p : hex) if (!rel(std::sqrt(p.x * p.x + p.y * p.y), 2.0f, 1e-4f)) onR = false;
        CHECK(onR, "every hexagon vertex is on the radius");
        CHECK(polygonSignedArea2(hex) > 0.0f, "regular polygon is wound CCW");
        // Area of a regular hexagon with circumradius r is (3*sqrt(3)/2) r^2.
        const float expect = 1.5f * std::sqrt(3.0f) * 4.0f;
        CHECK(rel(polygonArea(hex), expect, 1e-3f), "hexagon area matches (3*sqrt3/2) r^2");
    }

    // --- 2. Star: 5 points -> 10 vertices alternating outer/inner radius. ---
    {
        const auto s = shapes2d::star(5, 2.0f, 1.0f);
        CHECK(s.size() == 10, "5-point star has 10 vertices");
        int outer = 0, inner = 0;
        for (std::size_t i = 0; i < s.size(); ++i) {
            const float r = std::sqrt(s[i].x * s[i].x + s[i].y * s[i].y);
            if (i % 2 == 0) { if (rel(r, 2.0f, 1e-4f)) ++outer; }
            else { if (rel(r, 1.0f, 1e-4f)) ++inner; }
        }
        CHECK(outer == 5 && inner == 5, "star alternates 5 tips (outer) and 5 valleys (inner)");
        CHECK(polygonSignedArea2(s) > 0.0f, "star is wound CCW");
    }

    // --- 3. Rounded rectangle: fits width x height, area = w*h - (4-pi) r^2. ---
    {
        const float w = 4.0f, h = 2.0f, r = 0.5f;
        const auto rr = shapes2d::roundedRect(w, h, r, 6);
        CHECK(rr.size() == static_cast<std::size_t>(4 * (6 + 1)), "rounded rect has 4*(seg+1) points");
        float minx = 1e9f, maxx = -1e9f, miny = 1e9f, maxy = -1e9f;
        for (const maz::math::vec2& p : rr) {
            minx = std::fmin(minx, p.x); maxx = std::fmax(maxx, p.x);
            miny = std::fmin(miny, p.y); maxy = std::fmax(maxy, p.y);
        }
        CHECK(rel(maxx - minx, w, 1e-4f) && rel(maxy - miny, h, 1e-4f), "rounded rect spans width x height");
        const float expect = w * h - (4.0f - 3.14159265f) * r * r;
        CHECK(rel(polygonArea(rr), expect, 2e-3f), "rounded rect area = w*h - (4-pi) r^2");
        CHECK(polygonSignedArea2(rr) > 0.0f, "rounded rect is wound CCW");
        // r=0 gives a plain 4-corner rectangle.
        CHECK(shapes2d::roundedRect(w, h, 0.0f).size() == 4, "zero-radius rounded rect is a plain rectangle");
    }

    // --- 4. Composability: a star extrudes into a valid closed prism (volume = area * depth). ---
    {
        const auto s = shapes2d::star(6, 2.0f, 0.8f);
        const shapes::MeshData prism = extrudePolygon(s, 0.5f);
        CHECK(!prism.vertices.empty(), "star extrudes into a mesh");
        // Signed volume of the closed prism should equal polygon area * depth.
        double vol = 0.0;
        for (std::size_t t = 0; t < prism.indices.size(); t += 3) {
            const MeshVertex& a = prism.vertices[prism.indices[t]];
            const MeshVertex& b = prism.vertices[prism.indices[t + 1]];
            const MeshVertex& c = prism.vertices[prism.indices[t + 2]];
            const double cx = static_cast<double>(b.py) * c.pz - static_cast<double>(b.pz) * c.py;
            const double cy = static_cast<double>(b.pz) * c.px - static_cast<double>(b.px) * c.pz;
            const double cz = static_cast<double>(b.px) * c.py - static_cast<double>(b.py) * c.px;
            vol += static_cast<double>(a.px) * cx + static_cast<double>(a.py) * cy + static_cast<double>(a.pz) * cz;
        }
        CHECK(rel(static_cast<float>(vol / 6.0), polygonArea(s) * 0.5f, 1e-3f), "extruded star volume = area * depth");
    }

    // --- 5. Degenerate inputs are safe. ---
    {
        CHECK(shapes2d::regularPolygon(2, 1.0f).empty(), "polygon with <3 sides -> empty");
        CHECK(shapes2d::star(1, 2.0f, 1.0f).empty(), "star with <2 points -> empty");
        CHECK(shapes2d::roundedRect(0.0f, 2.0f, 0.2f).empty(), "zero-size rounded rect -> empty");
    }

    if (g_fail == 0) {
        std::printf("shapes2d: OK — n-gon/star/rounded-rect counts+area+CCW, extrudes to a solid, safe.\n");
        return 0;
    }
    std::printf("shapes2d: %d failure(s).\n", g_fail);
    return 1;
}
