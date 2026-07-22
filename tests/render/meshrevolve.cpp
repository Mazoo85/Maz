// tests/render/meshrevolve.cpp — verifies solid-of-revolution build (render::revolveProfile). Ground truths:
// spinning a straight vertical profile (constant radius) makes a cylinder whose every side vertex sits at that
// radius and whose normals point radially OUTWARD; the vertex/triangle counts follow segments and profile length;
// a profile point on the axis (radius 0) produces a single clean cone tip (no degenerate faces); too-small inputs
// are safe. Pure CPU, headless.
#include "maz/render/MeshRevolve.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // A straight vertical profile at radius 2, from y=0 to y=4 -> a cylinder side wall.
    const std::vector<ProfilePoint> cyl = {{2.0f, 0.0f}, {2.0f, 4.0f}};
    const int segs = 8;
    const shapes::MeshData m = revolveProfile(cyl, segs);

    // --- 1. Vertex count = (segments+1) rings * profile length. ---
    {
        CHECK(m.vertices.size() == static_cast<std::size_t>(segs + 1) * 2, "verts = (segments+1) * profile size");
        CHECK(!m.indices.empty() && m.indices.size() % 3 == 0, "a whole number of triangles");
        // A closed cylinder side: segments quads * 2 tris = 2*segs triangles.
        CHECK(m.indices.size() == static_cast<std::size_t>(segs) * 2u * 3u, "segments*2 triangles for a cylinder wall");
    }

    // --- 2. Every vertex lies on the cylinder: sqrt(x^2+z^2) == radius, y in [0,4]. ---
    {
        bool ok = true;
        for (const MeshVertex& v : m.vertices) {
            const float r = std::sqrt(v.px * v.px + v.pz * v.pz);
            if (!near(r, 2.0f, 1e-4f)) ok = false;
            if (v.py < -1e-4f || v.py > 4.0f + 1e-4f) ok = false;
        }
        CHECK(ok, "all side vertices sit on the cylinder of radius 2, height 0..4");
    }

    // --- 3. Normals point radially OUTWARD (dot with the outward radial direction > 0). ---
    {
        bool ok = true;
        for (const MeshVertex& v : m.vertices) {
            const float rlen = std::sqrt(v.px * v.px + v.pz * v.pz);
            if (rlen < 1e-5f) continue;                 // pole (none here, but be safe)
            const float rx = v.px / rlen, rz = v.pz / rlen; // outward radial unit (xz)
            if (v.nx * rx + v.nz * rz < 0.5f) ok = false;   // normal agrees with outward radial
        }
        CHECK(ok, "side normals point radially outward");
    }

    // --- 4. A profile that reaches the axis (radius 0) makes a clean cone tip: no degenerate triangles. ---
    {
        const std::vector<ProfilePoint> cone = {{0.0f, 3.0f}, {2.0f, 0.0f}}; // tip on axis at top, rim at bottom
        const shapes::MeshData c = revolveProfile(cone, 6);
        // Every emitted triangle must have positive area (poles skipped, not emitted as slivers).
        bool ok = !c.indices.empty();
        for (std::size_t t = 0; t < c.indices.size() / 3; ++t) {
            const MeshVertex& a = c.vertices[c.indices[t * 3 + 0]];
            const MeshVertex& b = c.vertices[c.indices[t * 3 + 1]];
            const MeshVertex& d = c.vertices[c.indices[t * 3 + 2]];
            const float ex1 = b.px - a.px, ey1 = b.py - a.py, ez1 = b.pz - a.pz;
            const float ex2 = d.px - a.px, ey2 = d.py - a.py, ez2 = d.pz - a.pz;
            const float cx = ey1 * ez2 - ez1 * ey2, cy = ez1 * ex2 - ex1 * ez2, cz = ex1 * ey2 - ey1 * ex2;
            if (cx * cx + cy * cy + cz * cz < 1e-12f) ok = false;
        }
        // A cone from 6 segments: only the rim ring makes real quads (the tip ring is a pole) -> 6 triangles.
        CHECK(ok, "cone tip on the axis produces only non-degenerate triangles");
        CHECK(c.indices.size() == 6u * 3u, "cone: one non-degenerate triangle per segment at the rim");
    }

    // --- 5. A partial sweep (half turn) spans only a semicircle: some z stays ~0-side, x reaches -radius. ---
    {
        const shapes::MeshData half = revolveProfile(cyl, 8, 3.14159265f);
        float minX = 1e9f, maxX = -1e9f;
        for (const MeshVertex& v : half.vertices) { minX = std::fmin(minX, v.px); maxX = std::fmax(maxX, v.px); }
        CHECK(near(maxX, 2.0f, 1e-3f) && near(minX, -2.0f, 1e-3f), "a half turn sweeps from +radius round to -radius");
    }

    // --- 6. Degenerate inputs are safe. ---
    {
        CHECK(revolveProfile({}, 8).vertices.empty(), "empty profile -> empty mesh");
        CHECK(revolveProfile({{1, 0}}, 8).vertices.empty(), "single-point profile -> empty mesh");
        CHECK(revolveProfile(cyl, 2).vertices.empty(), "fewer than 3 segments -> empty mesh");
    }

    if (g_fail == 0) {
        std::printf("meshrevolve: OK — cylinder/cone spins, outward normals, clean poles, partial sweep, safe.\n");
        return 0;
    }
    std::printf("meshrevolve: %d failure(s).\n", g_fail);
    return 1;
}
