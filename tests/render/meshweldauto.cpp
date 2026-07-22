// tests/render/meshweldauto.cpp — verifies weld-tolerance auto-detect (render::suggestWeldTolerance / autoWeld).
// Ground truths: a clean grid with no duplicates suggests an epsilon too small to weld anything; a grid with
// near-coincident seam duplicates suggests an epsilon between the duplicate gap and the real spacing (so welding
// removes exactly the duplicates); exact duplicates are counted and welded; and autoWeld collapses a seam-split
// mesh back to its unique vertices. Pure CPU, headless.
#include "maz/render/MeshWeldAuto.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// An N x N grid of points spaced 1.0 apart in the z=0 plane (no indices needed for the analysis).
static shapes::MeshData grid(int n) {
    shapes::MeshData m;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x)
            m.vertices.push_back(vtx(static_cast<float>(x), static_cast<float>(y), 0.0f));
    return m;
}

int main() {
    // --- 1. Clean grid, no duplicates: suggested epsilon is far below the 1.0 spacing, welds nothing. ---
    {
        const shapes::MeshData m = grid(5);
        const WeldSuggestion s = suggestWeldTolerance(m);
        CHECK(s.exactDuplicateVertices == 0, "a clean grid has no exact duplicates");
        CHECK(!s.bimodal, "a clean grid shows no duplicate cluster");
        CHECK(s.suggestedEpsilon < 1.0f, "suggested epsilon stays below the real spacing");
        CHECK(s.weldableVertices == 0, "nothing is weldable at the suggested epsilon");
        CHECK(s.medianGap > 0.9f && s.medianGap < 1.5f, "median gap reflects the ~1.0 grid spacing");
    }

    // --- 2. Grid + near-coincident seam duplicates (0.001 apart): epsilon sits between 0.001 and 1.0. ---
    {
        shapes::MeshData m = grid(5);
        const std::size_t before = m.vertices.size();
        // Duplicate three grid points, each nudged by 0.001 (a seam split an importer would leave behind).
        m.vertices.push_back(vtx(0.001f, 0.0f, 0.0f));
        m.vertices.push_back(vtx(1.0f, 1.001f, 0.0f));
        m.vertices.push_back(vtx(2.0f, 2.0f, 0.001f));
        const WeldSuggestion s = suggestWeldTolerance(m);
        CHECK(s.bimodal, "a clear duplicate-vs-real gap is detected");
        CHECK(s.suggestedEpsilon > 0.001f, "epsilon is large enough to weld the 0.001 seam pairs");
        CHECK(s.suggestedEpsilon < 1.0f, "epsilon is small enough to keep the 1.0 grid apart");
        CHECK(s.weldableVertices >= 6, "the three duplicate pairs (six vertices) are flagged weldable");
        // autoWeld should remove exactly the three duplicates.
        const WeldedMesh w = autoWeld(m);
        CHECK(w.positions.size() == before, "autoWeld collapses the seam duplicates back to the unique grid");
    }

    // --- 3. Exact duplicates (same spot): counted and welded away. ---
    {
        shapes::MeshData m = grid(4);
        const std::size_t before = m.vertices.size();
        m.vertices.push_back(vtx(0.0f, 0.0f, 0.0f)); // exact copy of grid corner
        m.vertices.push_back(vtx(3.0f, 3.0f, 0.0f)); // exact copy of far corner
        const WeldSuggestion s = suggestWeldTolerance(m);
        CHECK(s.exactDuplicateVertices >= 2, "exact duplicates are counted");
        CHECK(s.bimodal, "exact duplicates register as a weldable cluster");
        const WeldedMesh w = autoWeld(m);
        CHECK(w.positions.size() == before, "autoWeld removes the exact duplicates");
    }

    // --- 4. All vertices identical: every one is an exact duplicate; welds down to a single point. ---
    {
        shapes::MeshData m;
        for (int i = 0; i < 5; ++i) m.vertices.push_back(vtx(2.0f, 2.0f, 2.0f));
        const WeldSuggestion s = suggestWeldTolerance(m);
        CHECK(s.exactDuplicateVertices == 5, "all five coincident vertices are exact duplicates");
        const WeldedMesh w = autoWeld(m);
        CHECK(w.positions.size() == 1, "autoWeld collapses coincident vertices to one");
    }

    // --- 5. Empty / single vertex are safe. ---
    {
        const WeldSuggestion s0 = suggestWeldTolerance(shapes::MeshData{});
        CHECK(s0.suggestedEpsilon == 0.0f && s0.weldableVertices == 0, "empty mesh -> zeroed suggestion");
        shapes::MeshData one; one.vertices.push_back(vtx(0,0,0));
        const WeldSuggestion s1 = suggestWeldTolerance(one);
        CHECK(s1.weldableVertices == 0, "single vertex -> nothing to weld");
    }

    if (g_fail == 0) {
        std::printf("meshweldauto: OK — clean grid welds nothing, seam dups detected+welded, exact dups counted, coincident collapse.\n");
        return 0;
    }
    std::printf("meshweldauto: %d failure(s).\n", g_fail);
    return 1;
}
