// tests/render/meshsolidity.cpp — verifies the mesh solidity (convexity) ratio (render::analyzeSolidity).
// Ground truths: a convex cube has solidity ~1 (mesh volume == hull volume); a cube with an inward dimple in its
// top face reads solidity < 1 with the hull staying the full cube; an open shell reports invalid. Pure CPU.
#include "maz/render/MeshSolidity.hpp"
#include "maz/render/MeshWinding.hpp" // makeWindingConsistent — prepare the hand-authored concave mesh

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static shapes::MeshData cube() { // unit cube [0,1]^3, closed, outward-wound
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,1,0), vtx(0,1,0),
                  vtx(0,0,1), vtx(1,0,1), vtx(1,1,1), vtx(0,1,1)};
    m.indices = {0,2,1, 0,3,2,  4,5,6, 4,6,7,  0,1,5, 0,5,4,
                 3,7,6, 3,6,2,  0,4,7, 0,7,3,  1,2,6, 1,6,5};
    return m;
}

// Unit cube whose flat top (y=1) is replaced by a cone dipping to an apex at the centre (0.5,0.5,0.5). The
// dimple removes a pyramid of volume 1/3 * 1 * 0.5 = 1/6, so the solid is ~0.833; the hull stays the unit cube.
static shapes::MeshData dimpledCube() {
    shapes::MeshData m;
    m.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(1,0,1), vtx(0,0,1),   // 0..3 bottom (y=0)
                  vtx(0,1,0), vtx(1,1,0), vtx(1,1,1), vtx(0,1,1),   // 4..7 top rim (y=1)
                  vtx(0.5f,0.5f,0.5f)};                            // 8 inward apex
    m.indices = {
        0,1,2, 0,2,3,           // bottom
        4,5,8, 5,6,8, 6,7,8, 7,4,8, // top dimple (rim -> apex)
        0,1,5, 0,5,4,           // side z=0
        3,2,6, 3,6,7,           // side z=1
        0,4,7, 0,7,3,           // side x=0
        1,2,6, 1,6,5,           // side x=1
    };
    // Make winding consistent so the closed-mesh volume integral is well-defined (sign is auto-corrected).
    return makeWindingConsistent(m);
}

int main() {
    // --- 1. Convex cube: solidity ~1, mesh volume == hull volume == 1. ---
    {
        const SolidityReport r = analyzeSolidity(cube());
        CHECK(r.valid, "the cube produces a valid solidity report");
        CHECK(near(r.meshVolume, 1.0, 1e-4), "cube mesh volume is 1");
        CHECK(near(r.hullVolume, 1.0, 1e-4), "cube hull volume is 1");
        CHECK(near(r.solidity, 1.0, 1e-3), "a convex cube has solidity ~1");
    }

    // --- 2. Dimpled cube: concave, solidity clearly below 1, hull still the full cube. ---
    {
        const SolidityReport r = analyzeSolidity(dimpledCube());
        CHECK(r.valid, "the dimpled cube produces a valid report");
        CHECK(near(r.hullVolume, 1.0, 1e-4), "the hull ignores the inward dimple and stays the unit cube");
        CHECK(near(r.meshVolume, 5.0 / 6.0, 0.02), "the dimple removes ~1/6, leaving ~0.833 volume");
        CHECK(r.solidity < 0.9, "a dented shape reads less than fully solid");
        CHECK(r.solidity > 0.7, "but the solidity is still the sensible ~0.83, not near zero");
        CHECK(r.meshVolume < r.hullVolume, "a concave mesh always fits inside its hull");
    }

    // --- 3. An open shell (single triangle) is invalid — no enclosed volume. ---
    {
        shapes::MeshData open;
        open.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0), vtx(0,0,1)};
        open.indices = {0,1,2, 0,1,3, 0,2,3}; // 3 of the 4 tetra faces -> not closed
        const SolidityReport r = analyzeSolidity(open);
        CHECK(!r.valid, "an open (non-watertight) shell reports invalid");
    }

    // --- 4. Empty / tiny meshes are safe. ---
    {
        CHECK(!analyzeSolidity(shapes::MeshData{}).valid, "empty mesh -> invalid");
    }

    if (g_fail == 0) {
        std::printf("meshsolidity: OK — cube solidity ~1, dimpled cube ~0.83 inside its hull, open shell invalid.\n");
        return 0;
    }
    std::printf("meshsolidity: %d failure(s).\n", g_fail);
    return 1;
}
