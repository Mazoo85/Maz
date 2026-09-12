// tests/render/meshmassproperties.cpp — verifies solid mass properties from a closed mesh
// (render::computeMassProperties) against closed-form values. Ground truths: a unit cube [0,1]^3 has volume
// 1, centroid (0.5,0.5,0.5), and (unit density) a diagonal inertia tensor of 1/6 about its centre with zero
// products of inertia; a 2x cube has volume 8 and inertia scaling as m*s^2 (= 8 * 4 / 6); translating the
// mesh moves only the centroid, not the (centroid-relative) inertia; winding sign is auto-corrected; an open
// mesh is reported invalid. Pure CPU, headless.
#include "maz/render/MeshMassProperties.hpp"

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// A welded axis-aligned cube from (ox,oy,oz) of side `s`, outward-wound.
static shapes::MeshData cube(float ox, float oy, float oz, float s) {
    shapes::MeshData m;
    const float c[8][3] = {{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    for (const auto& p : c) m.vertices.push_back(vtx(ox + p[0]*s, oy + p[1]*s, oz + p[2]*s));
    m.indices = {
        0,1,2, 2,3,0,   1,5,6, 6,2,1,   5,4,7, 7,6,5,
        4,0,3, 3,7,4,   3,2,6, 6,7,3,   4,5,1, 1,0,4,
    };
    return m;
}

static bool near(double a, double b, double tol) { return std::fabs(a - b) <= tol; }

static bool diagInertia(const MassProperties& mp, double diag, double tol) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) {
            const double want = (i == j) ? diag : 0.0;
            if (!near(mp.inertia[i][j], want, tol)) return false;
        }
    return true;
}

int main() {
    // --- 1. Unit cube: volume 1, centroid at its centre, inertia 1/6 on the diagonal. ---
    {
        const MassProperties mp = computeMassProperties(cube(0, 0, 0, 1.0f));
        CHECK(mp.valid, "unit cube is a valid closed solid");
        CHECK(near(mp.volume, 1.0, 1e-9), "unit cube volume is 1");
        CHECK(near(mp.centroid[0], 0.5, 1e-9) && near(mp.centroid[1], 0.5, 1e-9) &&
              near(mp.centroid[2], 0.5, 1e-9), "unit cube centroid is (0.5,0.5,0.5)");
        CHECK(diagInertia(mp, 1.0 / 6.0, 1e-9), "unit cube inertia is 1/6 diagonal, zero products");
    }

    // --- 2. Side-2 cube: volume 8, inertia = m*s^2/6 = 8*4/6. ---
    {
        const MassProperties mp = computeMassProperties(cube(0, 0, 0, 2.0f));
        CHECK(near(mp.volume, 8.0, 1e-8), "side-2 cube volume is 8");
        CHECK(diagInertia(mp, 8.0 * 4.0 / 6.0, 1e-7), "side-2 cube inertia is m*s^2/6");
    }

    // --- 3. Translating the mesh moves only the centroid; centroid-relative inertia is unchanged. ---
    {
        const MassProperties a = computeMassProperties(cube(0, 0, 0, 1.0f));
        const MassProperties b = computeMassProperties(cube(100.0f, -50.0f, 7.0f, 1.0f));
        CHECK(near(b.centroid[0], 100.5, 1e-6) && near(b.centroid[1], -49.5, 1e-6) &&
              near(b.centroid[2], 7.5, 1e-6), "translated centroid tracks the translation");
        bool sameI = true;
        for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j)
            if (!near(a.inertia[i][j], b.inertia[i][j], 1e-6)) sameI = false;
        CHECK(sameI, "centroid-relative inertia is translation-invariant");
        CHECK(near(b.volume, 1.0, 1e-6), "translated volume unchanged");
    }

    // --- 4. Inward (reversed) winding is auto-corrected to a positive volume + same inertia. ---
    {
        shapes::MeshData inward = cube(0, 0, 0, 1.0f);
        for (std::size_t t = 0; t + 2 < inward.indices.size(); t += 3)
            std::swap(inward.indices[t + 1], inward.indices[t + 2]); // reverse each triangle
        const MassProperties mp = computeMassProperties(inward);
        CHECK(near(mp.volume, 1.0, 1e-9), "reversed winding still gives positive volume 1");
        CHECK(diagInertia(mp, 1.0 / 6.0, 1e-9), "reversed winding gives the same inertia");
    }

    // --- 5. An open mesh (single triangle) has no enclosed volume -> invalid. ---
    {
        shapes::MeshData open;
        open.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0,1,0)};
        open.indices = {0,1,2};
        const MassProperties mp = computeMassProperties(open);
        CHECK(!mp.valid, "open mesh reports invalid (no enclosed volume)");
    }

    if (g_fail == 0) {
        std::printf("meshmassproperties: OK — cube volume/centroid/inertia exact, translation + winding safe.\n");
        return 0;
    }
    std::printf("meshmassproperties: %d failure(s).\n", g_fail);
    return 1;
}
