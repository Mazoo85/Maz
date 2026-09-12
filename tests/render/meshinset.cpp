// tests/render/meshinset.cpp — verifies per-triangle inset (render::insetFaces). Ground truths: every triangle
// becomes 3 private vertices (unwelded); at amount 0 nothing moves; each corner moves a fraction `amount` toward
// its triangle's centroid, which is itself preserved; area shrinks by (1-amount)^2; amount 1 collapses the
// triangle to its centroid; each corner carries the flat face normal. Pure CPU, headless.
#include "maz/render/MeshInset.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
// Triangle area via half the cross-product magnitude.
static float triArea(const MeshVertex& a, const MeshVertex& b, const MeshVertex& c) {
    const float ux = b.px - a.px, uy = b.py - a.py, uz = b.pz - a.pz;
    const float vx = c.px - a.px, vy = c.py - a.py, vz = c.pz - a.pz;
    const float cx = uy * vz - uz * vy, cy = uz * vx - ux * vz, cz = ux * vy - uy * vx;
    return 0.5f * std::sqrt(cx * cx + cy * cy + cz * cz);
}

// One triangle in the XZ plane (normal +Y): corners (0,0,0), (3,0,0), (0,0,3). Centroid (1,0,1).
static shapes::MeshData tri() {
    shapes::MeshData m;
    m.vertices = {at(0, 0, 0), at(3, 0, 0), at(0, 0, 3)};
    m.indices = {0, 1, 2};
    return m;
}

int main() {
    const shapes::MeshData base = tri();
    const float baseArea = triArea(base.vertices[0], base.vertices[1], base.vertices[2]); // = 4.5

    // --- 1. Unwelding + amount 0 is the identity in shape. ---
    {
        const shapes::MeshData f = insetFaces(base, 0.0f);
        CHECK(f.vertices.size() == 3 && f.indices.size() == 3, "one triangle stays one triangle (3 own verts)");
        CHECK(near(f.vertices[0].px, 0.0f, 1e-5f) && near(f.vertices[1].px, 3.0f, 1e-5f), "amount 0 leaves corners put");
    }

    // --- 2. amount 0.5: each corner moves halfway to the centroid (1,0,1). ---
    {
        const shapes::MeshData f = insetFaces(base, 0.5f);
        // Corner 0 (0,0,0) -> centroid(1,0,1) halfway = (0.5, 0, 0.5).
        CHECK(near(f.vertices[0].px, 0.5f, 1e-5f) && near(f.vertices[0].pz, 0.5f, 1e-5f), "corner 0 moves halfway in");
        // Corner 1 (3,0,0) -> (2, 0, 0.5).
        CHECK(near(f.vertices[1].px, 2.0f, 1e-5f) && near(f.vertices[1].pz, 0.5f, 1e-5f), "corner 1 moves halfway in");
    }

    // --- 3. The centroid is preserved (scaling about it), and area shrinks by (1-amount)^2. ---
    {
        const shapes::MeshData f = insetFaces(base, 0.5f);
        const float gx = (f.vertices[0].px + f.vertices[1].px + f.vertices[2].px) / 3.0f;
        const float gz = (f.vertices[0].pz + f.vertices[1].pz + f.vertices[2].pz) / 3.0f;
        CHECK(near(gx, 1.0f, 1e-5f) && near(gz, 1.0f, 1e-5f), "the inset triangle keeps the original centroid");
        const float area = triArea(f.vertices[0], f.vertices[1], f.vertices[2]);
        CHECK(near(area, baseArea * 0.25f, 1e-4f), "area shrinks by (1-0.5)^2 = 1/4");
    }

    // --- 4. Each corner carries the flat face normal (this winding faces -Y; that's the geometric truth). ---
    {
        const shapes::MeshData f = insetFaces(base, 0.3f);
        for (const MeshVertex& v : f.vertices)
            CHECK(near(v.ny, -1.0f, 1e-5f) && near(v.nx, 0.0f, 1e-5f), "corners carry the flat face normal");
    }

    // --- 5. amount 1 collapses the triangle onto its centroid. ---
    {
        const shapes::MeshData f = insetFaces(base, 1.0f);
        for (const MeshVertex& v : f.vertices)
            CHECK(near(v.px, 1.0f, 1e-5f) && near(v.pz, 1.0f, 1e-5f), "amount 1 puts every corner at the centroid");
    }

    // --- 6. Empty mesh is safe. ---
    {
        CHECK(insetFaces(shapes::MeshData{}, 0.5f).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshinset: OK — faces unweld + shrink toward their centroid, centroid kept, area x(1-a)^2.\n");
        return 0;
    }
    std::printf("meshinset: %d failure(s).\n", g_fail);
    return 1;
}
