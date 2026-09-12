// tests/render/meshicosphere.cpp — verifies the geodesic sphere (render::makeIcosphere). Ground truths: subdivision
// 0 is the raw 20-face icosahedron (12 verts); each level multiplies faces by 4 and follows V=10*4^n+2; every vertex
// sits exactly on the sphere of the given radius; normals equal the unit position; the mesh is a closed watertight
// shell (Euler V-E+F==2, every edge shared by exactly 2 faces); triangles are near-uniform (no pole pinch). Pure CPU.
#include "maz/render/MeshIcosphere.hpp"

#include <cmath>
#include <cstdio>
#include <unordered_map>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// Count unique undirected edges and confirm each is shared by exactly two triangles.
static bool manifoldClosed(const shapes::MeshData& m, std::size_t& edgeCount) {
    std::unordered_map<std::uint64_t, int> edges;
    for (std::size_t f = 0; f < m.indices.size(); f += 3) {
        const std::uint32_t v[3] = {m.indices[f], m.indices[f + 1], m.indices[f + 2]};
        for (int e = 0; e < 3; ++e) {
            std::uint32_t a = v[e], b = v[(e + 1) % 3];
            const std::uint64_t key = a < b ? (static_cast<std::uint64_t>(a) << 32) | b
                                            : (static_cast<std::uint64_t>(b) << 32) | a;
            ++edges[key];
        }
    }
    edgeCount = edges.size();
    for (const auto& kv : edges) if (kv.second != 2) return false; // every edge shared by exactly 2 faces
    return true;
}

int main() {
    // --- 1. Subdivision 0: raw icosahedron — 12 verts, 20 faces, radius 2. ---
    {
        const shapes::MeshData m = makeIcosphere(2.0f, 0);
        CHECK(m.vertices.size() == 12, "subdiv 0 -> 12 vertices");
        CHECK(m.indices.size() == 20u * 3u, "subdiv 0 -> 20 faces");
        bool onR = true;
        for (const MeshVertex& v : m.vertices) {
            const float r = std::sqrt(v.px * v.px + v.py * v.py + v.pz * v.pz);
            if (!near(r, 2.0f, 1e-4f)) onR = false;
            // normal is the unit position
            if (!near(v.nx * 2.0f, v.px, 1e-4f) || !near(v.ny * 2.0f, v.py, 1e-4f) || !near(v.nz * 2.0f, v.pz, 1e-4f)) onR = false;
        }
        CHECK(onR, "all vertices on the radius-2 sphere with position-aligned normals");
    }

    // --- 2. Subdivision counts: faces = 20*4^n, verts = 10*4^n + 2. ---
    {
        for (int n = 0; n <= 3; ++n) {
            const shapes::MeshData m = makeIcosphere(1.0f, n);
            const std::size_t f4 = static_cast<std::size_t>(std::lround(std::pow(4.0, n)));
            CHECK(m.indices.size() == 20u * f4 * 3u, "faces = 20*4^n");
            CHECK(m.vertices.size() == 10u * f4 + 2u, "verts = 10*4^n + 2");
        }
    }

    // --- 3. Watertight closed shell: Euler V - E + F == 2, every edge shared by 2 faces. ---
    {
        const shapes::MeshData m = makeIcosphere(1.0f, 2);
        std::size_t E = 0;
        const bool ok = manifoldClosed(m, E);
        const std::size_t V = m.vertices.size(), F = m.indices.size() / 3;
        CHECK(ok, "every edge shared by exactly two faces (closed manifold)");
        CHECK(static_cast<long>(V) - static_cast<long>(E) + static_cast<long>(F) == 2, "Euler characteristic V-E+F==2 (a sphere)");
    }

    // --- 4. Near-uniform triangles: no pole pinch — max edge length is not wildly bigger than the min. ---
    {
        const shapes::MeshData m = makeIcosphere(1.0f, 3);
        float minE = 1e9f, maxE = 0.0f;
        for (std::size_t f = 0; f < m.indices.size(); f += 3) {
            for (int e = 0; e < 3; ++e) {
                const MeshVertex& a = m.vertices[m.indices[f + static_cast<std::size_t>(e)]];
                const MeshVertex& b = m.vertices[m.indices[f + static_cast<std::size_t>((e + 1) % 3)]];
                const float dx = a.px - b.px, dy = a.py - b.py, dz = a.pz - b.pz;
                const float len = std::sqrt(dx * dx + dy * dy + dz * dz);
                minE = std::fmin(minE, len); maxE = std::fmax(maxE, len);
            }
        }
        // A UV sphere's pole triangles collapse (ratio -> huge); an icosphere stays tight (< 2x in practice).
        CHECK(maxE / minE < 2.0f, "edge lengths stay near-uniform (no pole pinch)");
    }

    // --- 5. Negative subdivisions clamp to 0 (raw icosahedron), never crash. ---
    {
        const shapes::MeshData m = makeIcosphere(1.0f, -3);
        CHECK(m.vertices.size() == 12 && m.indices.size() == 60, "negative subdivisions clamp to the base icosahedron");
    }

    if (g_fail == 0) {
        std::printf("meshicosphere: OK — counts, on-sphere, watertight (Euler 2), uniform triangles, safe.\n");
        return 0;
    }
    std::printf("meshicosphere: %d failure(s).\n", g_fail);
    return 1;
}
