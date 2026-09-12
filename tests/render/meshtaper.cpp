// tests/render/meshtaper.cpp — verifies the taper deformer (render::taperMesh). Ground truths: the perpendicular
// cross-section scales linearly along the axis (start factor at the low end, end factor at the high end); the axis
// coordinate is untouched; a factor of 0 collapses that end onto the axis; equal factors are a uniform scale;
// factor 1/1 is the identity. Pure CPU, headless.
#include "maz/render/MeshTaper.hpp"

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

// A vertical bar: bottom ring (y=0) and top ring (y=4), corners x,z in {-1,+1}, plus a mid ring (y=2).
static shapes::MeshData bar() {
    shapes::MeshData m;
    m.vertices = {
        at(-1, 0, -1), at(1, 0, -1), at(1, 0, 1), at(-1, 0, 1), // bottom 0..3
        at(-1, 2, -1), at(1, 2, -1), at(1, 2, 1), at(-1, 2, 1), // mid    4..7
        at(-1, 4, -1), at(1, 4, -1), at(1, 4, 1), at(-1, 4, 1), // top    8..11
    };
    m.indices = {0, 1, 2, 8, 9, 10}; // arbitrary valid triangles; taper ignores topology
    return m;
}

int main() {
    const shapes::MeshData base = bar();

    // Taper about Y from 1.0 at the bottom to 0.0 at the top -> a pyramid/cone. Auto-fit range [0,4].
    const shapes::MeshData tp = taperMesh(base, /*axis=Y*/1, /*start=*/1.0f, /*end=*/0.0f);

    // --- 1. Bottom ring (t=0, scale 1) is unchanged. ---
    {
        bool same = true;
        for (int i = 0; i < 4; ++i) {
            if (!near(tp.vertices[static_cast<std::size_t>(i)].px, base.vertices[static_cast<std::size_t>(i)].px, 1e-5f)) same = false;
            if (!near(tp.vertices[static_cast<std::size_t>(i)].pz, base.vertices[static_cast<std::size_t>(i)].pz, 1e-5f)) same = false;
        }
        CHECK(same, "the bottom ring (scale 1) is unchanged");
    }

    // --- 2. Top ring (t=1, scale 0) collapses onto the axis. ---
    {
        bool collapsed = true;
        for (int i = 8; i < 12; ++i)
            if (!near(tp.vertices[static_cast<std::size_t>(i)].px, 0.0f, 1e-5f) ||
                !near(tp.vertices[static_cast<std::size_t>(i)].pz, 0.0f, 1e-5f)) collapsed = false;
        CHECK(collapsed, "the top ring (scale 0) collapses to the axis (cone tip)");
    }

    // --- 3. Mid ring (t=0.5, scale 0.5) is halved; axis coordinate untouched. ---
    {
        // Vertex 6 is (1,2,1) -> (0.5, 2, 0.5).
        const MeshVertex& v = tp.vertices[6];
        CHECK(near(v.px, 0.5f, 1e-5f) && near(v.pz, 0.5f, 1e-5f), "the middle ring is scaled by 0.5");
        CHECK(near(v.py, 2.0f, 1e-6f), "the axis coordinate (y) is untouched");
    }

    // --- 4. Equal factors are a plain uniform cross-section scale (2x here). ---
    {
        const shapes::MeshData up = taperMesh(base, 1, 2.0f, 2.0f);
        const MeshVertex& v = up.vertices[6]; // (1,2,1) -> (2,2,2)
        CHECK(near(v.px, 2.0f, 1e-5f) && near(v.pz, 2.0f, 1e-5f), "equal factors scale the whole cross-section");
        CHECK(near(v.py, 2.0f, 1e-6f), "axis coordinate still untouched under uniform taper");
    }

    // --- 5. Factor 1/1 is the identity. ---
    {
        const shapes::MeshData id = taperMesh(base, 1, 1.0f, 1.0f);
        bool same = true;
        for (std::size_t i = 0; i < base.vertices.size(); ++i)
            if (!near(id.vertices[i].px, base.vertices[i].px, 1e-6f) ||
                !near(id.vertices[i].pz, base.vertices[i].pz, 1e-6f)) same = false;
        CHECK(same, "taper 1->1 leaves the mesh unchanged");
    }

    // --- 6. Empty mesh is safe. ---
    {
        CHECK(taperMesh(shapes::MeshData{}, 1, 1.0f, 0.5f).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshtaper: OK — cross-section scales linearly along the axis, tip collapses, axis coord fixed.\n");
        return 0;
    }
    std::printf("meshtaper: %d failure(s).\n", g_fail);
    return 1;
}
