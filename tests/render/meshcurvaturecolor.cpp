// tests/render/meshcurvaturecolor.cpp — verifies the curvature heatmap (render::curvatureHeatmap). Ground truths:
// the blue->green->red ramp hits its exact stops; a flat mesh comes out all blue (zero curvature); on a pyramid
// the sharp apex reads redder (more curved) than the flat base corners; positions are untouched. Pure CPU.
#include "maz/render/MeshCurvatureColor.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 0.5f; return v;
}

int main() {
    // --- 1. The ramp hits blue / green / red at 0 / 0.5 / 1. ---
    {
        float r, g, b;
        detail::curvatureColor(0.0f, r, g, b);
        CHECK(near(r, 0, 1e-6f) && near(g, 0, 1e-6f) && near(b, 1, 1e-6f), "t=0 -> blue");
        detail::curvatureColor(0.5f, r, g, b);
        CHECK(near(r, 0, 1e-6f) && near(g, 1, 1e-6f) && near(b, 0, 1e-6f), "t=0.5 -> green");
        detail::curvatureColor(1.0f, r, g, b);
        CHECK(near(r, 1, 1e-6f) && near(g, 0, 1e-6f) && near(b, 0, 1e-6f), "t=1 -> red");
    }

    // --- 2. A flat grid (zero curvature) comes out all blue. ---
    {
        shapes::MeshData m;
        const int n = 4;
        for (int x = 0; x <= n; ++x)
            for (int z = 0; z <= n; ++z) m.vertices.push_back(at(static_cast<float>(x), 0.0f, static_cast<float>(z)));
        auto idx = [](int x, int z) { return static_cast<std::uint32_t>(x * (4 + 1) + z); };
        for (int x = 0; x < n; ++x)
            for (int z = 0; z < n; ++z) {
                const std::uint32_t a = idx(x, z), b = idx(x, z + 1), c = idx(x + 1, z), d = idx(x + 1, z + 1);
                m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(c);
                m.indices.push_back(c); m.indices.push_back(b); m.indices.push_back(d);
            }
        const shapes::MeshData h = curvatureHeatmap(m, CurvatureKind::Mean);
        // The interior vertex (2,0,2) at index idx(2,2) should be flat -> blue.
        const MeshVertex& v = h.vertices[idx(2, 2)];
        CHECK(near(v.b, 1.0f, 1e-3f) && near(v.r, 0.0f, 1e-3f), "a flat interior vertex is blue (no curvature)");
    }

    // --- 3. On a pyramid the sharp apex reads redder than a flat base corner. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0, 1, 0),        // 0: apex (interior, high curvature)
                      at(-1, 0, -1), at(1, 0, -1), at(1, 0, 1), at(-1, 0, 1)}; // 1..4: base corners (boundary)
        m.indices = {0, 1, 2, 0, 2, 3, 0, 3, 4, 0, 4, 1}; // four side faces around the apex
        const shapes::MeshData h = curvatureHeatmap(m, CurvatureKind::Mean);
        const MeshVertex& apex = h.vertices[0];
        const MeshVertex& corner = h.vertices[1];
        CHECK(apex.r > corner.r, "the apex is redder (more curved) than a base corner");
        CHECK(apex.b < corner.b, "the apex is less blue than a base corner");
    }

    // --- 4. Positions are untouched. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0, 1, 0), at(-1, 0, -1), at(1, 0, -1), at(1, 0, 1)};
        m.indices = {0, 1, 2, 0, 2, 3};
        const shapes::MeshData h = curvatureHeatmap(m);
        bool posSame = true;
        for (std::size_t i = 0; i < m.vertices.size(); ++i)
            if (!near(h.vertices[i].px, m.vertices[i].px, 1e-6f) || !near(h.vertices[i].py, m.vertices[i].py, 1e-6f))
                posSame = false;
        CHECK(posSame, "only colour changes, not geometry");
    }

    // --- 5. Empty mesh is safe. ---
    {
        CHECK(curvatureHeatmap(shapes::MeshData{}).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshcurvaturecolor: OK — ramp stops exact, flat=blue, sharp apex reads red.\n");
        return 0;
    }
    std::printf("meshcurvaturecolor: %d failure(s).\n", g_fail);
    return 1;
}
