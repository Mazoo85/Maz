// tests/render/meshthickness.cpp — verifies the wall-thickness probe (render::computeThickness /
// analyzeThickness). Ground truths: a slab of two parallel outward-facing sheets a known gap apart reports that
// gap as its thickness; a thinner gap reports a proportionally smaller thickness; a vertex whose inward ray
// escapes is reported as open; and the summary picks out the thinnest wall. Pure CPU, headless.
#include "maz/render/MeshThickness.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// A slab: a 3x3 top sheet at y=`top` (outward normal +Y) directly above one big bottom sheet at y=0 (outward
// normal −Y). The gap `top` is the wall thickness a downward ray from any top vertex should measure. The bottom
// sheet is oversized so every top vertex's inward ray lands well inside it. Returns the center top vertex index.
static shapes::MeshData slab(float top, std::size_t& centerTop) {
    shapes::MeshData m;
    // Top 3x3 grid over x,z in {0,1,2}. CCW seen from above -> normal +Y.
    const float g[3] = {0.0f, 1.0f, 2.0f};
    for (int rz = 0; rz < 3; ++rz)
        for (int cx = 0; cx < 3; ++cx)
            m.vertices.push_back(vtx(g[cx], top, g[rz]));
    for (int rz = 0; rz < 2; ++rz) {
        for (int cx = 0; cx < 2; ++cx) {
            const std::uint32_t a = static_cast<std::uint32_t>(rz * 3 + cx);
            const std::uint32_t b = static_cast<std::uint32_t>((rz + 1) * 3 + cx);
            const std::uint32_t c = a + 1u, d = b + 1u;
            m.indices.insert(m.indices.end(), {a, b, c, c, b, d}); // CCW from above -> +Y
        }
    }
    centerTop = 4; // (row1,col1) center of the top grid
    // Big bottom quad over x,z in [-2,4], y=0. CCW seen from BELOW -> normal −Y (outward, downward).
    const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back(vtx(-2, 0, -2)); // base+0
    m.vertices.push_back(vtx(4, 0, -2));  // base+1
    m.vertices.push_back(vtx(4, 0, 4));   // base+2
    m.vertices.push_back(vtx(-2, 0, 4));  // base+3
    // Winding so the face normal points −Y: cross(v1-v0, v2-v0) must be (0,-,0).
    m.indices.insert(m.indices.end(), {base + 0u, base + 1u, base + 2u, base + 0u, base + 2u, base + 3u});
    return m;
}

int main() {
    // --- 1. Slab gap 1.0: top surface measures thickness ~1. ---
    {
        std::size_t center = 0;
        const shapes::MeshData m = slab(1.0f, center);
        const std::vector<float> th = computeThickness(m, 100.0f);
        CHECK(near(th[center], 1.0f, 0.02f), "center of the top sheet measures the 1.0 gap");
        // Every top-grid vertex (indices 0..8) should read ~1 (all land inside the oversized bottom quad).
        bool allTop = true;
        for (std::size_t i = 0; i < 9; ++i) if (!near(th[i], 1.0f, 0.02f)) allTop = false;
        CHECK(allTop, "the whole top sheet reads the uniform 1.0 wall thickness");
    }

    // --- 2. Thinner slab (gap 0.25) reads proportionally thinner. ---
    {
        std::size_t center = 0;
        const shapes::MeshData m = slab(0.25f, center);
        const std::vector<float> th = computeThickness(m, 100.0f);
        CHECK(near(th[center], 0.25f, 0.02f), "a 0.25 gap reads as 0.25 thickness");
    }

    // --- 3. Summary report: measured top sheet + open bottom corners, min thickness = the gap. ---
    {
        std::size_t center = 0;
        const shapes::MeshData m = slab(1.0f, center);
        const ThicknessReport rep = analyzeThickness(m, 100.0f);
        CHECK(rep.measured >= 9, "at least the nine top-sheet vertices measured a wall");
        CHECK(rep.open >= 1, "some bottom-sheet corners shoot into open space and are flagged open");
        CHECK(near(rep.minThickness, 1.0f, 0.05f), "the thinnest measured wall is the 1.0 gap");
        CHECK(rep.meanThickness < 100.0f, "the mean thickness is a finite measured value");
    }

    // --- 4. A single one-sided sheet: every inward ray escapes -> all open. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        m.indices = {0,1,2};
        const std::vector<float> th = computeThickness(m, 50.0f);
        bool allOpen = true;
        for (float d : th) if (d < 50.0f) allOpen = false;
        CHECK(allOpen, "a lone sheet has no opposing wall -> everything reads open");
    }

    // --- 5. Empty mesh is safe. ---
    {
        CHECK(computeThickness(shapes::MeshData{}).empty(), "empty mesh -> no thickness values");
        const ThicknessReport rep = analyzeThickness(shapes::MeshData{}, 10.0f);
        CHECK(rep.measured == 0 && rep.open == 0, "empty mesh -> nothing measured, nothing open");
    }

    if (g_fail == 0) {
        std::printf("meshthickness: OK — slab gap measured, thinner reads thinner, open flagged, summary min correct.\n");
        return 0;
    }
    std::printf("meshthickness: %d failure(s).\n", g_fail);
    return 1;
}
