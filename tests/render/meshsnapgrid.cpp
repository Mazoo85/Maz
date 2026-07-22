// tests/render/meshsnapgrid.cpp — verifies world-grid vertex snapping (render::snapVerticesToGrid). Ground
// truths: each position rounds to the nearest multiple of the grid step; tiny drift cleans up to exact values;
// a per-axis step of 0 leaves that axis free; the report counts moved vertices and the max displacement; an
// offset origin shifts the grid lines. Pure CPU, headless.
#include "maz/render/MeshSnapGrid.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.ny = 1.0f; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Drift cleanup: near-integer values snap to exact integers on a step-1 grid. ---
    {
        shapes::MeshData m;
        m.vertices = {at(1.0000001f, 2.9999998f, -0.0000002f)};
        const SnapResult r = snapVerticesToGrid(m, 1.0f);
        CHECK(near(r.mesh.vertices[0].px, 1.0f, 1e-6f), "1.0000001 -> 1");
        CHECK(near(r.mesh.vertices[0].py, 3.0f, 1e-6f), "2.9999998 -> 3");
        CHECK(near(r.mesh.vertices[0].pz, 0.0f, 1e-6f), "-0.0000002 -> 0");
        CHECK(r.movedVertices == 1, "the drifted vertex counts as moved");
        CHECK(r.maxDisplacement < 1e-3f, "the displacement was tiny (drift cleanup)");
    }

    // --- 2. Rounding to nearest on a quarter grid, midpoint rounds away from zero. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0.30f, 0.10f, 0.125f)}; // step 0.25 -> 0.25, 0.0, 0.25 (0.125 is the midpoint)
        const SnapResult r = snapVerticesToGrid(m, 0.25f);
        CHECK(near(r.mesh.vertices[0].px, 0.25f, 1e-6f), "0.30 -> 0.25 (nearest quarter)");
        CHECK(near(r.mesh.vertices[0].py, 0.00f, 1e-6f), "0.10 -> 0.00");
        CHECK(near(r.mesh.vertices[0].pz, 0.25f, 1e-6f), "0.125 midpoint rounds up to 0.25");
    }

    // --- 3. A per-axis step of 0 leaves that axis untouched (snap ground plane, keep height). ---
    {
        shapes::MeshData m;
        m.vertices = {at(0.4f, 3.7f, 0.4f)};
        const SnapResult r = snapVerticesToGrid(m, maz::math::vec3(1.0f, 0.0f, 1.0f));
        CHECK(near(r.mesh.vertices[0].px, 0.0f, 1e-6f), "X snaps to 0");
        CHECK(near(r.mesh.vertices[0].py, 3.7f, 1e-6f), "Y (step 0) is left free");
        CHECK(near(r.mesh.vertices[0].pz, 0.0f, 1e-6f), "Z snaps to 0");
    }

    // --- 4. Already-on-grid vertices don't move; the report says so. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0.0f, 1.0f, 2.0f), at(-3.0f, 0.0f, 5.0f)};
        const SnapResult r = snapVerticesToGrid(m, 1.0f);
        CHECK(r.movedVertices == 0 && r.maxDisplacement == 0.0f, "on-grid vertices are unchanged");
    }

    // --- 5. An offset origin shifts the grid lines (grid through 0.5 with step 1). ---
    {
        shapes::MeshData m;
        m.vertices = {at(0.6f, 0, 0)};
        const SnapResult r = snapVerticesToGrid(m, maz::math::vec3(1, 0, 0), maz::math::vec3(0.5f, 0, 0));
        CHECK(near(r.mesh.vertices[0].px, 0.5f, 1e-6f), "0.6 snaps to the nearest offset line 0.5");
    }

    // --- 6. maxDisplacement reports the largest single move. ---
    {
        shapes::MeshData m;
        m.vertices = {at(0.05f, 0, 0), at(0, 0.4f, 0)}; // moves 0.05 and 0.4 on a step-1 grid
        const SnapResult r = snapVerticesToGrid(m, 1.0f);
        CHECK(r.movedVertices == 2, "both vertices moved");
        CHECK(near(r.maxDisplacement, 0.4f, 1e-6f), "max displacement is the larger move (0.4)");
    }

    // --- 7. Empty mesh is safe. ---
    {
        const SnapResult r = snapVerticesToGrid(shapes::MeshData{}, 1.0f);
        CHECK(r.mesh.vertices.empty() && r.movedVertices == 0, "empty -> empty, nothing moved");
    }

    if (g_fail == 0) {
        std::printf("meshsnapgrid: OK — positions round to the world grid, per-axis free, moved-count + max report.\n");
        return 0;
    }
    std::printf("meshsnapgrid: %d failure(s).\n", g_fail);
    return 1;
}
