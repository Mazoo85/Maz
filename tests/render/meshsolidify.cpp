// tests/render/meshsolidify.cpp — verifies surface thickening (render::solidifyMesh). Ground truths: a one-sided
// quad becomes a closed solid (front + back + rim) with double the vertices, a watertight edge structure (every
// edge used exactly twice), and a back face offset inward by `thickness`; a closed input needs no rim. Pure CPU.
#include "maz/render/MeshSolidify.hpp"

#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// A unit quad in the z=0 plane, normals +Z, wound so the front faces +Z.
static shapes::MeshData quad() {
    shapes::MeshData m;
    auto v = [](float x, float y) { MeshVertex q{}; q.px = x; q.py = y; q.pz = 0; q.nz = 1; q.r = q.g = q.b = 1; return q; };
    m.vertices = {v(0, 0), v(1, 0), v(1, 1), v(0, 1)};
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

// A closed tetrahedron (every edge shared by two faces already).
static shapes::MeshData tetra() {
    shapes::MeshData m;
    auto v = [](float x, float y, float z) { MeshVertex q{}; q.px = x; q.py = y; q.pz = z; q.r = q.g = q.b = 1; return q; };
    m.vertices = {v(0, 0, 0), v(1, 0, 0), v(0, 1, 0), v(0, 0, 1)};
    m.indices = {0, 2, 1, 0, 1, 3, 0, 3, 2, 1, 2, 3};
    return m;
}

// Every undirected edge in an index buffer must be used exactly twice for a closed 2-manifold.
static bool isClosed(const shapes::MeshData& m) {
    std::unordered_map<std::uint64_t, int> ec;
    auto add = [&](std::uint32_t a, std::uint32_t b) {
        const std::uint32_t lo = a < b ? a : b, hi = a < b ? b : a;
        ec[(static_cast<std::uint64_t>(lo) << 32) | hi]++;
    };
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        add(m.indices[t], m.indices[t + 1]);
        add(m.indices[t + 1], m.indices[t + 2]);
        add(m.indices[t + 2], m.indices[t]);
    }
    for (const auto& kv : ec)
        if (kv.second != 2) return false;
    return true;
}

int main() {
    // --- 1. A quad thickens into a closed solid with doubled vertices and a rim. ---
    {
        const SolidifyResult r = solidifyMesh(quad(), 0.5f);
        CHECK(r.mesh.vertices.size() == 8, "front + back layers double the 4 vertices to 8");
        CHECK(r.hadBoundary, "the open quad has boundary edges, so a rim is built");
        CHECK(r.rimTriangles == 8, "4 boundary edges -> 8 rim triangles");
        // front 2 + back 2 + rim 8 = 12 triangles.
        CHECK(r.mesh.indices.size() == 12 * 3, "12 triangles total (2 front + 2 back + 8 rim)");
        CHECK(isClosed(r.mesh), "the solidified quad is watertight (every edge used exactly twice)");
    }

    // --- 2. The back layer is offset inward by `thickness` along -normal (+Z front -> back at z=-0.5). ---
    {
        const SolidifyResult r = solidifyMesh(quad(), 0.5f);
        // Vertices 0..3 are the front (z=0), 4..7 the back (z=-0.5).
        CHECK(near(r.mesh.vertices[0].pz, 0.0f, 1e-6f), "front stays at z=0");
        CHECK(near(r.mesh.vertices[4].pz, -0.5f, 1e-5f), "back is pushed to z=-0.5");
        CHECK(near(r.mesh.vertices[4].nz, -1.0f, 1e-5f), "the back face normal points the other way");
    }

    // --- 3. A negative thickness pushes the shell the other way (back at z=+0.5). ---
    {
        const SolidifyResult r = solidifyMesh(quad(), -0.5f);
        CHECK(near(r.mesh.vertices[4].pz, 0.5f, 1e-5f), "negative thickness offsets the back to +z");
        CHECK(isClosed(r.mesh), "still watertight");
    }

    // --- 4. A closed input needs no rim (just a second inner shell). ---
    {
        const SolidifyResult r = solidifyMesh(tetra(), 0.1f);
        CHECK(!r.hadBoundary && r.rimTriangles == 0, "a closed mesh has no boundary edges -> no rim");
        CHECK(r.mesh.vertices.size() == 8, "still front + back = 8 vertices");
        CHECK(r.mesh.indices.size() == 8 * 3, "4 front + 4 back = 8 triangles, no rim");
    }

    // --- 5. Degenerate / empty inputs are safe. ---
    {
        CHECK(solidifyMesh(shapes::MeshData{}, 0.5f).mesh.vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshsolidify: OK — quad thickens to a watertight slab, back offset inward, closed input skips rim.\n");
        return 0;
    }
    std::printf("meshsolidify: %d failure(s).\n", g_fail);
    return 1;
}
