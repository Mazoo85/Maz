// tests/render/meshdominantplane.cpp — verifies the dominant-plane / flatness detector
// (render::fitDominantPlane). Ground truths: a flat grid in the y=0 plane fits a +/-Y normal with ~zero
// thickness and planarity ~1; a tilted flat sheet fits its true normal; a cube reads low planarity with
// thickness ~ its extent; empty/tiny meshes are safe. Pure CPU, headless.
#include "maz/render/MeshDominantPlane.hpp"

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

int main() {
    // --- 1. Flat grid in the y=0 plane: normal is +/-Y, flat, planarity ~1. ---
    {
        shapes::MeshData m;
        for (int z = 0; z < 5; ++z)
            for (int x = 0; x < 5; ++x)
                m.vertices.push_back(vtx(static_cast<float>(x), 0.0f, static_cast<float>(z)));
        m.indices = {0,1,2}; // one triangle just to be a valid mesh; the fit uses all vertices
        const MeshPlane pl = fitDominantPlane(m);
        CHECK(pl.valid, "a valid plane is produced");
        CHECK(near(std::fabs(pl.normal.y), 1.0f, 1e-4f), "the flat direction of an XZ sheet is Y");
        CHECK(near(pl.normal.x, 0.0f, 1e-4f) && near(pl.normal.z, 0.0f, 1e-4f), "no tilt in X or Z");
        CHECK(pl.thickness < 1e-4f && pl.rmsDistance < 1e-4f, "a flat sheet has zero thickness");
        CHECK(pl.planarity > 0.99f, "planarity is ~1 for a perfectly flat sheet");
        CHECK(near(pl.point.y, 0.0f, 1e-5f), "the plane passes through the sheet (y=0)");
    }

    // --- 2. A tilted flat sheet: the fitted normal matches the tilt. ---
    {
        // Points on the plane x + y = 0 (i.e. y = -x), spread in x and z. Normal ~ (1,1,0)/sqrt(2).
        shapes::MeshData m;
        for (int i = 0; i < 5; ++i)
            for (int z = 0; z < 5; ++z) {
                const float x = static_cast<float>(i);
                m.vertices.push_back(vtx(x, -x, static_cast<float>(z)));
            }
        m.indices = {0,1,2};
        const MeshPlane pl = fitDominantPlane(m);
        const float inv = 1.0f / std::sqrt(2.0f);
        // Normal is defined up to sign; compare |n . expected|.
        const float d = std::fabs(pl.normal.x * inv + pl.normal.y * inv + pl.normal.z * 0.0f);
        CHECK(d > 0.999f, "the fitted normal matches the tilted plane's (1,1,0) direction");
        CHECK(pl.thickness < 1e-3f, "a tilted flat sheet is still flat");
        CHECK(pl.planarity > 0.99f, "tilt doesn't change the planarity");
    }

    // --- 3. A cube point cloud: low planarity, thickness ~ its extent. ---
    {
        shapes::MeshData m;
        for (int sx = 0; sx <= 1; ++sx)
            for (int sy = 0; sy <= 1; ++sy)
                for (int sz = 0; sz <= 1; ++sz)
                    m.vertices.push_back(vtx(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sz)));
        m.indices = {0,1,2};
        const MeshPlane pl = fitDominantPlane(m);
        CHECK(pl.valid, "the cube produces a plane fit");
        CHECK(pl.planarity < 0.2f, "a cube is far from flat (low planarity)");
        CHECK(pl.thickness > 0.5f, "the cube has real thickness along any axis");
    }

    // --- 4. A thin slab (flat-ish box): high planarity, normal along the thin axis. ---
    {
        shapes::MeshData m;
        // 4x4 sheet in XZ at y=0 and a copy at y=0.05 -> thin in Y.
        for (float yy : {0.0f, 0.05f})
            for (int z = 0; z < 4; ++z)
                for (int x = 0; x < 4; ++x)
                    m.vertices.push_back(vtx(static_cast<float>(x), yy, static_cast<float>(z)));
        m.indices = {0,1,2};
        const MeshPlane pl = fitDominantPlane(m);
        CHECK(near(std::fabs(pl.normal.y), 1.0f, 1e-2f), "the thin axis (Y) is the slab's normal");
        CHECK(pl.planarity > 0.9f, "a thin slab reads as nearly flat");
        CHECK(near(pl.thickness, 0.05f, 1e-3f), "thickness equals the slab's gauge");
    }

    // --- 5. Empty / tiny meshes are safe. ---
    {
        CHECK(!fitDominantPlane(shapes::MeshData{}).valid, "empty mesh -> invalid");
    }

    if (g_fail == 0) {
        std::printf("meshdominantplane: OK — XZ sheet -> Y normal, tilt matched, cube low-planarity, slab thin-axis.\n");
        return 0;
    }
    std::printf("meshdominantplane: %d failure(s).\n", g_fail);
    return 1;
}
