// tests/render/meshextrudepolygon.cpp — verifies linear polygon extrude (render::extrudePolygon). Ground truths:
// an n-gon extruded makes 4n-4 triangles (2 caps of n-2 + 2 walls per edge); the prism runs z=-depth/2..+depth/2
// with the polygon's XY extent; it is a closed, consistently-wound solid so its signed volume == area*depth (works
// for CW or CCW input); cap normals are +/-Z and wall normals lie in-plane; degenerate inputs are safe. Pure CPU.
#include "maz/render/MeshExtrudePolygon.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// Signed volume via the divergence theorem (sum of tetra volumes) — positive for an outward-wound closed mesh.
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
    // --- 1. A unit square (area 1) extruded depth 2: counts, bbox, volume 2. ---
    {
        const std::vector<maz::math::vec2> sq = {{-0.5f, -0.5f}, {0.5f, -0.5f}, {0.5f, 0.5f}, {-0.5f, 0.5f}};
        const shapes::MeshData m = extrudePolygon(sq, 2.0f);
        CHECK(m.indices.size() / 3 == static_cast<std::size_t>(4 * 4 - 4), "n-gon -> 4n-4 triangles (n=4 -> 12)");
        float minz = 1e9f, maxz = -1e9f, minx = 1e9f, maxx = -1e9f;
        for (const MeshVertex& v : m.vertices) {
            minz = std::fmin(minz, v.pz); maxz = std::fmax(maxz, v.pz);
            minx = std::fmin(minx, v.px); maxx = std::fmax(maxx, v.px);
        }
        CHECK(near(minz, -1.0f, 1e-6f) && near(maxz, 1.0f, 1e-6f), "prism runs z=-depth/2..+depth/2");
        CHECK(near(minx, -0.5f, 1e-6f) && near(maxx, 0.5f, 1e-6f), "XY extent matches the polygon");
        CHECK(near(static_cast<float>(signedVolume(m)), 2.0f, 1e-4f), "closed solid: signed volume == area*depth");
    }

    // --- 2. Winding auto-normalises: a CLOCKWISE square still yields a positive-volume solid. ---
    {
        const std::vector<maz::math::vec2> cw = {{-0.5f, 0.5f}, {0.5f, 0.5f}, {0.5f, -0.5f}, {-0.5f, -0.5f}};
        CHECK(near(static_cast<float>(signedVolume(extrudePolygon(cw, 2.0f))), 2.0f, 1e-4f),
              "clockwise input is normalised (still +area*depth)");
    }

    // --- 3. A triangle (area 2) extruded depth 3: volume 6, 4*3-4=8 triangles. ---
    {
        const std::vector<maz::math::vec2> tri = {{0, 0}, {2, 0}, {0, 2}};
        const shapes::MeshData m = extrudePolygon(tri, 3.0f);
        CHECK(m.indices.size() / 3 == 8, "triangle prism has 4*3-4 = 8 triangles");
        CHECK(near(static_cast<float>(signedVolume(m)), 6.0f, 1e-4f), "triangle prism volume == area*depth");
    }

    // --- 4. Cap normals are +/-Z; wall normals lie in the XY plane (nz==0). ---
    {
        const std::vector<maz::math::vec2> sq = {{-1, -1}, {1, -1}, {1, 1}, {-1, 1}};
        const shapes::MeshData m = extrudePolygon(sq, 1.0f);
        int caps = 0, walls = 0;
        for (const MeshVertex& v : m.vertices) {
            if (near(std::fabs(v.nz), 1.0f, 1e-4f)) ++caps;
            else if (near(v.nz, 0.0f, 1e-4f)) ++walls;
        }
        CHECK(caps > 0 && walls > 0, "some faces are caps (n=+/-Z), others walls (n in XY)");
    }

    // --- 5. Degenerate inputs are safe. ---
    {
        CHECK(extrudePolygon({{0, 0}, {1, 1}}, 1.0f).vertices.empty(), "fewer than 3 points -> empty");
        CHECK(extrudePolygon({{0, 0}, {1, 0}, {0.5f, 0.5f}, {1, 1}, {0, 1}}, 0.0f).vertices.empty(), "zero depth -> empty");
        CHECK(extrudePolygon({{0, 0}, {1, 0}, {2, 0}}, 1.0f).vertices.empty(), "collinear (zero-area) outline -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshextrudepolygon: OK — 4n-4 tris, centred prism, volume=area*depth, CW ok, caps/walls, safe.\n");
        return 0;
    }
    std::printf("meshextrudepolygon: %d failure(s).\n", g_fail);
    return 1;
}
