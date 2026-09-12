// tests/render/meshnormalize.cpp — verifies normalize-to-box (render::normalizeToBox). Ground truths: a big
// off-centre box scales so its longest side becomes 1 and its centre lands at the origin; proportions are
// preserved (uniform scale); a custom target size/centre is honoured; a flat sheet scales by its longest side;
// empty meshes are safe. Pure CPU, headless.
#include "maz/render/MeshNormalize.hpp"

#include <algorithm>
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

static void bounds(const shapes::MeshData& m, maz::math::vec3& lo, maz::math::vec3& hi) {
    lo = maz::math::vec3(m.vertices[0].px, m.vertices[0].py, m.vertices[0].pz);
    hi = lo;
    for (const auto& v : m.vertices) {
        lo = maz::math::vec3(std::min(lo.x, v.px), std::min(lo.y, v.py), std::min(lo.z, v.pz));
        hi = maz::math::vec3(std::max(hi.x, v.px), std::max(hi.y, v.py), std::max(hi.z, v.pz));
    }
}

int main() {
    // --- 1. A big off-centre box (10 x 4 x 2 at corner (100,50,20)) -> longest side 1, centred at origin. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(100,50,20), vtx(110,50,20), vtx(110,54,20), vtx(100,54,20),
                      vtx(100,50,22), vtx(110,50,22), vtx(110,54,22), vtx(100,54,22)};
        const NormalizeResult r = normalizeToBox(m); // default: unit cube at origin
        CHECK(near(r.scale, 1.0f / 10.0f, 1e-6f), "scale is 1/longest-side (1/10)");
        maz::math::vec3 lo, hi;
        bounds(r.mesh, lo, hi);
        CHECK(near(hi.x - lo.x, 1.0f, 1e-5f), "the longest side is now 1");
        // Proportions preserved: original 10:4:2 -> 1:0.4:0.2.
        CHECK(near(hi.y - lo.y, 0.4f, 1e-5f) && near(hi.z - lo.z, 0.2f, 1e-5f), "uniform scale keeps proportions");
        CHECK(near((lo.x + hi.x) * 0.5f, 0.0f, 1e-5f) && near((lo.y + hi.y) * 0.5f, 0.0f, 1e-5f),
              "the centre is moved to the origin");
        CHECK(near(r.sourceCentre.x, 105.0f, 1e-4f), "the reported source centre is the box's centre");
    }

    // --- 2. Custom target size + centre. ---
    {
        shapes::MeshData m;
        m.vertices = {vtx(0,0,0), vtx(2,0,0), vtx(2,2,0), vtx(0,2,0)}; // 2x2 square
        const NormalizeResult r = normalizeToBox(m, maz::math::vec3(4, 4, 4), maz::math::vec3(10, 0, 0));
        CHECK(near(r.scale, 4.0f / 2.0f, 1e-6f), "scale fits the 2-unit side into the 4-unit target (x2)");
        maz::math::vec3 lo, hi;
        bounds(r.mesh, lo, hi);
        CHECK(near(hi.x - lo.x, 4.0f, 1e-5f), "the square is now 4 units wide");
        CHECK(near((lo.x + hi.x) * 0.5f, 10.0f, 1e-5f), "and centred at the target centre x=10");
    }

    // --- 3. A flat sheet normalizes by its longest side and stays flat. ---
    {
        shapes::MeshData m;
        for (int z = 0; z < 4; ++z)
            for (int x = 0; x < 8; ++x) // 7 wide (x) x 3 deep (z), zero-thickness in y
                m.vertices.push_back(vtx(static_cast<float>(x), 0.0f, static_cast<float>(z)));
        const NormalizeResult r = normalizeToBox(m);
        maz::math::vec3 lo, hi;
        bounds(r.mesh, lo, hi);
        CHECK(near(hi.x - lo.x, 1.0f, 1e-5f), "the 7-unit width becomes 1");
        CHECK(near(hi.y - lo.y, 0.0f, 1e-6f), "the sheet stays flat (zero thickness)");
        CHECK(near(r.scale, 1.0f / 7.0f, 1e-6f), "scaled by the longest side (7)");
    }

    // --- 4. A single point / empty mesh is safe. ---
    {
        shapes::MeshData one; one.vertices.push_back(vtx(5, 5, 5));
        const NormalizeResult r = normalizeToBox(one);
        CHECK(near(r.scale, 1.0f, 1e-6f), "a zero-extent mesh keeps unit scale");
        CHECK(near(r.mesh.vertices[0].px, 0.0f, 1e-6f), "its point is translated to the target centre");
        CHECK(normalizeToBox(shapes::MeshData{}).mesh.vertices.empty(), "empty mesh -> empty result");
    }

    if (g_fail == 0) {
        std::printf("meshnormalize: OK — longest side to 1 centred at origin, proportions kept, custom box, flat sheet.\n");
        return 0;
    }
    std::printf("meshnormalize: %d failure(s).\n", g_fail);
    return 1;
}
