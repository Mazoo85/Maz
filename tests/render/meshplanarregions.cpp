// tests/render/meshplanarregions.cpp — verifies coplanar-region segmentation
// (render::segmentCoplanarRegions). A correct segmentation: a welded cube yields exactly six regions (one per
// face, two triangles each), a single flat quad is one region, two quads meeting at a 90 degrees ridge are
// two regions even though they are edge-connected (the angle gate splits them), the tolerance actually gates
// (a gentle bend under tolerance merges, over tolerance splits), every triangle gets a label, and it is
// deterministic. Pure CPU, headless.
#include "maz/render/MeshPlanarRegions.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

static shapes::MeshData weldedCube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {
        0,1,2, 2,3,0,   1,5,6, 6,2,1,   5,4,7, 7,6,5,
        4,0,3, 3,7,4,   3,2,6, 6,7,3,   4,5,1, 1,0,4,
    };
    return m;
}

// Two quads sharing edge (p1,p2). Quad A in the XZ plane; quad B rotated `deg` degrees up about that edge.
static shapes::MeshData ridge(float deg) {
    const float r = deg * 3.14159265358979323846f / 180.0f;
    shapes::MeshData m;
    // Shared edge along X at z=0: p1=(0,0,0), p2=(1,0,0).
    m.vertices.push_back(vtx(0, 0, 0)); // 0
    m.vertices.push_back(vtx(1, 0, 0)); // 1
    m.vertices.push_back(vtx(1, 0, 1)); // 2  quad A far edge (z=+1)
    m.vertices.push_back(vtx(0, 0, 1)); // 3
    // Quad B far edge rotated up about the X axis by `r`: (x, sin r, -cos r).
    m.vertices.push_back(vtx(1, std::sin(r), -std::cos(r))); // 4
    m.vertices.push_back(vtx(0, std::sin(r), -std::cos(r))); // 5
    // Quad A: 0,1,2, 0,2,3 ; Quad B shares edge 0-1: 1,0,5, 1,5,4
    m.indices = {0,1,2, 0,2,3,  1,0,5, 1,5,4};
    return m;
}

int main() {
    // --- 1. Cube -> six regions, two triangles each. ---
    {
        const CoplanarRegions r = segmentCoplanarRegions(weldedCube(), 5.0f);
        CHECK(r.count == 6, "cube segments into six coplanar faces");
        std::vector<int> sizes(r.count, 0);
        for (std::uint32_t l : r.triLabel) sizes[l]++;
        bool allTwo = r.triLabel.size() == 12;
        for (int s : sizes) if (s != 2) allTwo = false;
        CHECK(allTwo, "each cube face region has exactly two triangles");
    }

    // --- 2. A single flat quad is one region. ---
    {
        shapes::MeshData q;
        q.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1)};
        q.indices = {0,1,2, 0,2,3};
        const CoplanarRegions r = segmentCoplanarRegions(q, 5.0f);
        CHECK(r.count == 1, "flat quad is a single region");
    }

    // --- 3. A 90 degrees ridge splits into two regions despite being edge-connected. ---
    {
        const CoplanarRegions r = segmentCoplanarRegions(ridge(90.0f), 5.0f);
        CHECK(r.count == 2, "90-degree ridge -> two regions");
    }

    // --- 4. Tolerance gates: a 3 degrees bend merges at 10 degrees tol, splits at 1 degrees tol. ---
    {
        CHECK(segmentCoplanarRegions(ridge(3.0f), 10.0f).count == 1, "gentle bend merges under a loose tol");
        CHECK(segmentCoplanarRegions(ridge(3.0f), 1.0f).count == 2, "same bend splits under a tight tol");
    }

    // --- 5. Every triangle labeled; deterministic. ---
    {
        const shapes::MeshData cube = weldedCube();
        const CoplanarRegions a = segmentCoplanarRegions(cube, 5.0f);
        const CoplanarRegions b = segmentCoplanarRegions(cube, 5.0f);
        CHECK(a.triLabel == b.triLabel, "segmentation is deterministic");
        bool labeled = a.triLabel.size() == 12;
        for (std::uint32_t l : a.triLabel) if (l >= a.count) labeled = false;
        CHECK(labeled, "every triangle receives a valid region label");
        CHECK(a.regionNormal.size() == a.count, "one seed normal per region");
    }

    if (g_fail == 0) {
        std::printf("meshplanarregions: OK — cube->6 faces, flat->1, ridge->2, tolerance gates, deterministic.\n");
        return 0;
    }
    std::printf("meshplanarregions: %d failure(s).\n", g_fail);
    return 1;
}
