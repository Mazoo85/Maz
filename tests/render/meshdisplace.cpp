// tests/render/meshdisplace.cpp — verifies noise displacement (render::displaceMesh). Ground truths: vertices
// move only along their normals by at most |amplitude|; a +Y grid moves only in Y (X/Z fixed); amplitude 0 is a
// no-op; the result is fully deterministic (same seed -> identical) and seed-sensitive; the coherent noise is not
// all-zero on a real grid. Pure CPU, headless.
#include "maz/render/MeshDisplace.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// An NxN grid in the XZ plane (y=0). Winding {a,b,c,c,b,d} with a=(x,z) b=(x,z+1) c=(x+1,z) gives +Y normals.
static shapes::MeshData grid(int cells) {
    shapes::MeshData m;
    const int w = cells + 1;
    for (int x = 0; x <= cells; ++x)
        for (int z = 0; z <= cells; ++z) {
            MeshVertex v{}; v.px = static_cast<float>(x); v.py = 0.0f; v.pz = static_cast<float>(z);
            v.ny = 1.0f; v.r = v.g = v.b = 1.0f;
            m.vertices.push_back(v);
        }
    auto idx = [w](int x, int z) { return static_cast<std::uint32_t>(x * w + z); };
    for (int x = 0; x < cells; ++x)
        for (int z = 0; z < cells; ++z) {
            const std::uint32_t a = idx(x, z), b = idx(x, z + 1), c = idx(x + 1, z), d = idx(x + 1, z + 1);
            m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(c);
            m.indices.push_back(c); m.indices.push_back(b); m.indices.push_back(d);
        }
    return m;
}

int main() {
    const shapes::MeshData base = grid(8); // 81 vertices

    // --- 1. A +Y grid displaces only in Y; X and Z are untouched; |dy| <= amplitude. ---
    {
        const DisplaceResult r = displaceMesh(base, 0.5f, 0.5f, 123u);
        bool onlyY = true, bounded = true;
        for (std::size_t i = 0; i < base.vertices.size(); ++i) {
            if (!near(r.mesh.vertices[i].px, base.vertices[i].px, 1e-6f)) onlyY = false;
            if (!near(r.mesh.vertices[i].pz, base.vertices[i].pz, 1e-6f)) onlyY = false;
            if (std::fabs(r.mesh.vertices[i].py) > 0.5f + 1e-5f) bounded = false;
        }
        CHECK(onlyY, "vertices move only along the +Y normal (X and Z unchanged)");
        CHECK(bounded, "no vertex moved more than |amplitude|");
        CHECK(r.maxOffset <= 0.5f + 1e-5f && r.maxOffset > 0.0f, "maxOffset is within amplitude and non-zero");
    }

    // --- 2. Deterministic: the same inputs give a byte-identical result. ---
    {
        const DisplaceResult a = displaceMesh(base, 0.7f, 1.3f, 42u);
        const DisplaceResult b = displaceMesh(base, 0.7f, 1.3f, 42u);
        bool same = true;
        for (std::size_t i = 0; i < a.mesh.vertices.size(); ++i)
            if (a.mesh.vertices[i].py != b.mesh.vertices[i].py) same = false;
        CHECK(same, "same mesh+amplitude+frequency+seed -> identical displacement");
    }

    // --- 3. A different seed produces a different field. ---
    {
        const DisplaceResult a = displaceMesh(base, 0.7f, 1.3f, 1u);
        const DisplaceResult b = displaceMesh(base, 0.7f, 1.3f, 2u);
        bool different = false;
        for (std::size_t i = 0; i < a.mesh.vertices.size(); ++i)
            if (!near(a.mesh.vertices[i].py, b.mesh.vertices[i].py, 1e-6f)) different = true;
        CHECK(different, "a different seed changes the noise field");
    }

    // --- 4. Amplitude 0 is a no-op. ---
    {
        const DisplaceResult r = displaceMesh(base, 0.0f, 1.0f, 5u);
        bool unchanged = true;
        for (std::size_t i = 0; i < base.vertices.size(); ++i)
            if (r.mesh.vertices[i].py != base.vertices[i].py) unchanged = false;
        CHECK(unchanged && r.maxOffset == 0.0f, "amplitude 0 leaves the mesh untouched");
    }

    // --- 5. The coherent noise actually moves the surface (not all zero). ---
    {
        const DisplaceResult r = displaceMesh(base, 1.0f, 0.7f, 9u);
        bool moved = false;
        for (std::size_t i = 0; i < base.vertices.size(); ++i)
            if (std::fabs(r.mesh.vertices[i].py) > 1e-3f) moved = true;
        CHECK(moved, "the displacement is non-trivial across the grid");
    }

    // --- 6. Empty / degenerate mesh is safe. ---
    {
        CHECK(displaceMesh(shapes::MeshData{}, 1.0f).mesh.vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshdisplace: OK — moves along normals within amplitude, deterministic, seed-sensitive.\n");
        return 0;
    }
    std::printf("meshdisplace: %d failure(s).\n", g_fail);
    return 1;
}
