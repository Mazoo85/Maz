// tests/render/meshsymmetry.cpp — verifies axis-aligned mirror-symmetry detection (render::detectSymmetryPlanes).
// Ground truths: a box centred at the origin is symmetric across all three axes; a point set mirrored only in X
// scores 1 on X and lower on Y/Z with X reported best; adding a lone unpaired vertex drops every axis below the
// threshold; and empty meshes are safe. Pure CPU, headless.
#include "maz/render/MeshSymmetry.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

int main() {
    // --- 1. Origin-centred box: symmetric across all three axes. ---
    {
        shapes::MeshData m;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    m.vertices.push_back(vtx(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sz)));
        const SymmetryReport r = detectSymmetryPlanes(m);
        CHECK(r.perAxis[0].symmetric && r.perAxis[1].symmetric && r.perAxis[2].symmetric,
              "a centred box is symmetric on X, Y and Z");
        CHECK(r.perAxis[0].score > 0.99f && r.perAxis[2].score > 0.99f, "all axis scores are ~1");
        CHECK(r.anySymmetric, "the box is reported symmetric");
    }

    // --- 2. Mirrored only in X: X scores 1, Y/Z lower, X reported best. ---
    {
        shapes::MeshData m;
        // Pairs (x,y,z)/(-x,y,z) with y,z arranged so no Y or Z mirror partner exists.
        m.vertices = {vtx(1,0,0),  vtx(-1,0,0),
                      vtx(2,1,0),  vtx(-2,1,0),
                      vtx(1,3,2),  vtx(-1,3,2)};
        const SymmetryReport r = detectSymmetryPlanes(m);
        CHECK(r.perAxis[0].score > 0.99f, "the X plane matches every vertex (score ~1)");
        CHECK(r.perAxis[0].symmetric, "the X axis is symmetric");
        CHECK(!r.perAxis[1].symmetric, "the Y axis is not symmetric");
        CHECK(r.best.axis == SymmetryAxis::X, "the best plane is the X plane");
        CHECK(r.best.score >= r.perAxis[1].score && r.best.score >= r.perAxis[2].score, "best has the top score");
    }

    // --- 3. A lone unpaired vertex breaks symmetry on every axis. ---
    {
        shapes::MeshData m;
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sz = -1; sz <= 1; sz += 2)
                    m.vertices.push_back(vtx(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sz)));
        m.vertices.push_back(vtx(1.7f, 0.3f, 0.9f)); // arbitrary point with no mirror on any axis
        const SymmetryReport r = detectSymmetryPlanes(m);
        CHECK(!r.perAxis[0].symmetric && !r.perAxis[1].symmetric && !r.perAxis[2].symmetric,
              "the odd vertex knocks every axis below the acceptance threshold");
        CHECK(!r.anySymmetric, "the mesh is reported not symmetric");
        // The odd vertex also shifts the centroid, so the plane no longer bisects the box -> score falls well
        // below the acceptance threshold (an honest consequence of centroid-based plane placement).
        CHECK(r.best.score < 0.98f, "the unmatched vertex drops the best score below the threshold");
    }

    // --- 4. Off-centre box is still symmetric about its OWN centre. ---
    {
        shapes::MeshData m;
        for (int sx = 0; sx <= 2; sx += 2)   // x in {0,2}, centre 1
            for (int sy = 0; sy <= 2; sy += 2)
                for (int sz = 0; sz <= 2; sz += 2)
                    m.vertices.push_back(vtx(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sz)));
        const SymmetryReport r = detectSymmetryPlanes(m);
        CHECK(r.anySymmetric, "symmetry is measured through the centroid, not the world origin");
        CHECK(r.perAxis[0].position > 0.9f && r.perAxis[0].position < 1.1f, "the X plane sits at the box centre (1)");
    }

    // --- 5. Empty mesh is safe. ---
    {
        const SymmetryReport r = detectSymmetryPlanes(shapes::MeshData{});
        CHECK(!r.anySymmetric && r.best.score == 0.0f, "empty mesh -> no symmetry");
    }

    if (g_fail == 0) {
        std::printf("meshsymmetry: OK — centred box all-axes, X-only mirror, lone vertex breaks it, off-centre via centroid.\n");
        return 0;
    }
    std::printf("meshsymmetry: %d failure(s).\n", g_fail);
    return 1;
}
