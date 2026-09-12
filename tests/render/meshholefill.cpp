// tests/render/meshholefill.cpp — verifies hole filling (render::fillHoles). Ground truths: capping the hole of
// a cube-with-a-face-removed makes it watertight again with a consistently-wound patch; two holes fill to two
// caps; a maxEdges limit skips holes larger than the limit; an already-closed mesh is unchanged. Pure CPU.
#include "maz/render/MeshHoleFill.hpp"
#include "maz/render/MeshWatertight.hpp" // analyzeWatertight — confirm the seal
#include "maz/render/MeshWinding.hpp"    // analyzeWinding — confirm the patch is not flipped

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

static shapes::MeshData cube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

int main() {
    // --- 1. Cube with one face removed: filling seals it back to watertight with a matched-winding patch. ---
    {
        shapes::MeshData m = cube();
        m.indices.erase(m.indices.begin(), m.indices.begin() + 6); // remove the z=0 face
        CHECK(!analyzeWatertight(m).watertight, "the holed cube starts non-watertight");

        const HoleFillResult r = fillHoles(m);
        CHECK(r.holesFilled == 1, "one hole is filled");
        CHECK(r.trianglesAdded == 4, "a 4-edge hole gets a 4-triangle centre fan");
        CHECK(analyzeWatertight(r.mesh).watertight, "the filled cube is watertight again");
        CHECK(analyzeWatertight(r.mesh).holeCount == 0, "no holes remain");
        CHECK(analyzeWinding(r.mesh).consistent, "the patch is wound consistently with the rest of the cube");
    }

    // --- 2. Cube with two opposite faces removed: two caps, watertight. ---
    {
        shapes::MeshData m = cube();
        m.indices.erase(m.indices.begin(), m.indices.begin() + 12); // remove z=0 and z=1 faces
        const HoleFillResult r = fillHoles(m);
        CHECK(r.holesFilled == 2, "both holes are filled");
        CHECK(r.trianglesAdded == 8, "two 4-edge holes add eight triangles");
        CHECK(analyzeWatertight(r.mesh).watertight, "the twice-holed cube is sealed");
    }

    // --- 3. maxEdges limit skips the too-big hole. ---
    {
        shapes::MeshData m = cube();
        m.indices.erase(m.indices.begin(), m.indices.begin() + 6);
        const HoleFillResult r = fillHoles(m, 3); // the hole has 4 edges > 3
        CHECK(r.holesFilled == 0 && r.trianglesAdded == 0, "a hole larger than maxEdges is left open");
        CHECK(!analyzeWatertight(r.mesh).watertight, "so the mesh stays non-watertight");
    }

    // --- 4. An already-closed cube is unchanged. ---
    {
        const shapes::MeshData m = cube();
        const HoleFillResult r = fillHoles(m);
        CHECK(r.holesFilled == 0, "a closed mesh has nothing to fill");
        CHECK(r.mesh.vertices.size() == m.vertices.size() && r.mesh.indices.size() == m.indices.size(),
              "the closed mesh is returned unchanged");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const HoleFillResult r = fillHoles(shapes::MeshData{});
        CHECK(r.holesFilled == 0 && r.mesh.vertices.empty(), "empty mesh -> nothing filled");
    }

    if (g_fail == 0) {
        std::printf("meshholefill: OK — face-hole sealed watertight + consistent, two holes capped, maxEdges honoured.\n");
        return 0;
    }
    std::printf("meshholefill: %d failure(s).\n", g_fail);
    return 1;
}
