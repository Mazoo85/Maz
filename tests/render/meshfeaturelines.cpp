// tests/render/meshfeaturelines.cpp — verifies feature-line extraction (render::extractFeatureLines). Ground
// truths: a convex tent fold classifies its crease as a Ridge; a concave valley fold classifies its crease as a
// Valley; a flat sheet has no feature lines; a cube's twelve 90-degree edges all read as convex ridges and chain
// into closed loops; and creases below the angle threshold are ignored. Pure CPU, headless.
#include "maz/render/MeshFeatureLines.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// Two quads hinged along the crease line x=0..2 at z=0. `apexY` is the crease height; wings drop/rise to y=0 at
// z=-1 and z=+1. apexY>0 => tent (convex ridge); apexY<0 => valley (concave). Winding gives upward-ish outward
// normals. The crease is the shared edge between vertices (0,apexY,0) and (2,apexY,0).
static shapes::MeshData fold(float apexY) {
    shapes::MeshData m;
    m.vertices = {
        vtx(0, apexY, 0), vtx(2, apexY, 0),   // 0,1 crease
        vtx(0, 0, -1), vtx(2, 0, -1),         // 2,3 back wing (z=-1)
        vtx(0, 0, 1),  vtx(2, 0, 1),          // 4,5 front wing (z=+1)
    };
    // Back wing (crease -> z=-1): CCW from above so normal tilts +Y. Front wing likewise.
    m.indices = {
        0, 3, 2, 0, 1, 3,   // back quad (wound for upward/outward normals)
        0, 5, 1, 0, 4, 5,   // front quad
    };
    return m;
}

static shapes::MeshData cube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

// Does any feature edge connect vertices a and b?
static bool hasEdge(const FeatureLines& fl, std::uint32_t a, std::uint32_t b) {
    for (const FeatureEdge& e : fl.edges)
        if ((e.a == a && e.b == b) || (e.a == b && e.b == a)) return true;
    return false;
}

int main() {
    // --- 1. Tent fold: the crease is a convex Ridge. ---
    {
        const FeatureLines fl = extractFeatureLines(fold(1.0f), 30.0f);
        CHECK(fl.edges.size() == 1, "the tent has exactly one crease feature edge");
        CHECK(hasEdge(fl, 0, 1), "the crease edge (0,1) is the feature");
        CHECK(fl.ridgeCount == 1 && fl.valleyCount == 0, "the tent crease is classified convex (Ridge)");
    }

    // --- 2. Valley fold: the crease is a concave Valley. ---
    {
        const FeatureLines fl = extractFeatureLines(fold(-1.0f), 30.0f);
        CHECK(fl.edges.size() == 1, "the valley has exactly one crease feature edge");
        CHECK(fl.valleyCount == 1 && fl.ridgeCount == 0, "the valley crease is classified concave (Valley)");
    }

    // --- 3. Flat sheet: no feature lines. ---
    {
        shapes::MeshData flat;
        flat.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1)};
        flat.indices = {0,1,2, 0,2,3};
        const FeatureLines fl = extractFeatureLines(flat, 30.0f);
        CHECK(fl.edges.empty() && fl.lines.empty(), "a flat sheet has no creases");
    }

    // --- 4. Cube: twelve 90-degree edges, all convex ridges, chained into closed loops. ---
    {
        const FeatureLines fl = extractFeatureLines(cube(), 30.0f);
        CHECK(fl.edges.size() == 12, "a cube exposes its twelve convex edges (face diagonals stay flat)");
        CHECK(fl.ridgeCount == 12 && fl.valleyCount == 0, "every cube edge is a convex ridge");
        // Each vertex sits on 3 cube edges (a junction), so chaining yields several short polylines covering all.
        std::size_t totalSegs = 0;
        for (const auto& line : fl.lines) if (line.size() >= 2) totalSegs += line.size() - 1;
        CHECK(totalSegs == 12, "the chained polylines cover all twelve edges exactly once");
    }

    // --- 5. Threshold gating: a gentle fold below the angle is ignored. ---
    {
        // apexY = 0.02 over a 1-unit half-width -> ~1.1 degree fold, far under a 30-degree threshold.
        const FeatureLines fl = extractFeatureLines(fold(0.02f), 30.0f);
        CHECK(fl.edges.empty(), "a fold gentler than the threshold is not a feature");
        // The same fold IS a feature under a tiny threshold.
        const FeatureLines fl2 = extractFeatureLines(fold(0.02f), 0.5f);
        CHECK(fl2.edges.size() == 1, "the same gentle fold registers once the threshold drops");
    }

    // --- 6. Empty mesh is safe. ---
    {
        const FeatureLines fl = extractFeatureLines(shapes::MeshData{});
        CHECK(fl.edges.empty() && fl.lines.empty(), "empty mesh -> no features");
    }

    if (g_fail == 0) {
        std::printf("meshfeaturelines: OK — tent ridge, valley concave, flat none, cube 12 ridges, threshold gating.\n");
        return 0;
    }
    std::printf("meshfeaturelines: %d failure(s).\n", g_fail);
    return 1;
}
