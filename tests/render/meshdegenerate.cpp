// tests/render/meshdegenerate.cpp — verifies the degenerate/sliver-triangle classifier
// (render::analyzeDegenerate). Ground truths: a healthy triangle is Ok; three collinear or two coincident
// vertices are ZeroArea; a near-180-degree triangle is a Cap; a thin spike is a Needle; classification order is
// zero-area, then cap, then needle. Pure CPU, headless.
#include "maz/render/MeshDegenerate.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

int main() {
    // Build one mesh with a healthy triangle (0), a collinear zero-area (1), a coincident zero-area (2),
    // a cap (3, near-180 apex), and a needle (4, thin spike). Each triangle uses its own 3 vertices.
    shapes::MeshData m;
    auto addTri = [&](MeshVertex a, MeshVertex b, MeshVertex c) {
        const std::uint32_t base = static_cast<std::uint32_t>(m.vertices.size());
        m.vertices.push_back(a);
        m.vertices.push_back(b);
        m.vertices.push_back(c);
        m.indices.insert(m.indices.end(), {base, base + 1u, base + 2u});
    };
    addTri(vtx(0,0,0), vtx(1,0,0), vtx(0,1,0));        // 0: healthy right triangle
    addTri(vtx(0,0,0), vtx(1,0,0), vtx(2,0,0));        // 1: collinear -> zero area
    addTri(vtx(0,0,0), vtx(1,0,0), vtx(1,0,0));        // 2: two coincident -> zero area
    addTri(vtx(0,0,0), vtx(2,0,0), vtx(1,0.01f,0));    // 3: near-180 apex -> cap
    addTri(vtx(0,0,0), vtx(0,0.01f,0), vtx(5,0,0));    // 4: thin spike -> needle

    const DegenerateReport r = analyzeDegenerate(m);

    CHECK(r.kind[0] == TriDefect::Ok, "the healthy triangle is Ok");
    CHECK(r.kind[1] == TriDefect::ZeroArea, "three collinear vertices are zero-area");
    CHECK(r.kind[2] == TriDefect::ZeroArea, "two coincident vertices are zero-area");
    CHECK(r.kind[3] == TriDefect::Cap, "a near-180-degree apex is a cap");
    CHECK(r.kind[4] == TriDefect::Needle, "a thin spike is a needle");

    CHECK(r.zeroArea.size() == 2, "two zero-area triangles collected");
    CHECK(r.caps.size() == 1, "one cap collected");
    CHECK(r.needles.size() == 1, "one needle collected");
    CHECK(r.badCount == 4, "four defective triangles in total");
    CHECK(r.maxAngleDegrees > 150.0f, "the widest angle reflects the cap (near 180)");
    CHECK(r.minAngleDegrees < 5.0f, "the sharpest angle reflects the needle (near 0)");

    // --- Threshold tuning: a mild 40-degree-max triangle is not a cap at the default 150 threshold. ---
    {
        shapes::MeshData mild;
        mild.vertices = {vtx(0,0,0), vtx(1,0,0), vtx(0.5f,0.8f,0)}; // roughly equilateral-ish, no wide angle
        mild.indices = {0,1,2};
        const DegenerateReport rr = analyzeDegenerate(mild);
        CHECK(rr.badCount == 0 && rr.kind[0] == TriDefect::Ok, "a well-shaped triangle is not flagged");
    }

    // --- Empty mesh is safe. ---
    {
        const DegenerateReport rr = analyzeDegenerate(shapes::MeshData{});
        CHECK(rr.kind.empty() && rr.badCount == 0, "empty mesh -> empty report");
    }

    if (g_fail == 0) {
        std::printf("meshdegenerate: OK — healthy/zero-area/cap/needle classified, counts + extreme angles correct.\n");
        return 0;
    }
    std::printf("meshdegenerate: %d failure(s).\n", g_fail);
    return 1;
}
