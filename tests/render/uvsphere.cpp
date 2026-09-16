// tests/render/uvsphere.cpp — pins what shapes::makeSphere actually produces.
//
// A UV sphere is the primitive most likely to be taken on trust, and the textbook construction hides
// two things in it. The first is now fixed and is what this test guards: emitting two triangles per
// quad everywhere puts 2 * sectors FLAT triangles against the pole rows, where both corners on the
// pole are the same point. They drew nothing, so nobody noticed — but they break normal averaging,
// defeat decimation, and are exactly what a mesh-health pass exists to flag. The second is a property
// of lat/long spheres rather than a defect, and is asserted here so it cannot be quietly assumed away:
// the mesh is OPEN (a duplicated wrap seam and duplicated pole rows), and its triangles are far less
// evenly shaped than an icosphere's.
//
// Also pinned: the pole rows sit at exactly +/-radius. float pi makes sin(theta) at the south pole
// -8.7e-08 rather than 0, which used to spread that pole over a ring 1e-7 across — triangles that a
// zero-area test cannot see but a quality score scores at 7e-08.
#include "maz/render/MeshDegenerate.hpp"
#include "maz/render/MeshIcosphere.hpp"
#include "maz/render/MeshStats.hpp"
#include "maz/render/MeshTopology.hpp"
#include "maz/render/Shapes.hpp"
#include "maz/render/TriangleQuality.hpp"

#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

int main() {
    const Color white{1, 1, 1, 1};

    // --- 1. No degenerate triangles, at several tessellations including the coarsest allowed. ---
    for (int rings : {2, 3, 10, 12}) {
        const int sectors = rings * 2;
        const shapes::MeshData m = shapes::makeSphere(1.0f, rings, sectors, white);
        const DegenerateReport d = analyzeDegenerate(m);
        const TriangleQualityStats q = analyzeTriangleQuality(m);
        CHECK(d.zeroArea.empty(), "no zero-area triangles anywhere on a UV sphere");
        CHECK(d.badCount == 0, "no caps or needles either");
        CHECK(q.degenerateCount == 0, "the quality pass finds nothing degenerate");
        CHECK(q.minQuality > 0.1f, "the worst triangle is a real triangle, not a sliver");

        // Exactly the flat pair per quad is skipped: 2*rings*sectors total, less 2*sectors.
        const std::size_t expected =
            static_cast<std::size_t>(2 * rings * sectors) - static_cast<std::size_t>(2 * sectors);
        CHECK(m.indices.size() / 3 == expected, "two triangles per quad except against the pole rows");
        // Vertices are NOT dropped: the pole rows stay duplicated because their UVs differ.
        CHECK(m.vertices.size() == static_cast<std::size_t>((rings + 1) * (sectors + 1)),
              "every lat/long vertex is still there");
    }

    // --- 2. The poles are exactly at +/-radius, not 1e-7 off. ---
    {
        const int rings = 10, sectors = 20;
        const shapes::MeshData m = shapes::makeSphere(2.0f, rings, sectors, white);
        const std::size_t stride = static_cast<std::size_t>(sectors + 1);
        bool exact = true;
        for (std::size_t j = 0; j < stride; ++j) {
            const MeshVertex& north = m.vertices[j];
            const MeshVertex& south = m.vertices[static_cast<std::size_t>(rings) * stride + j];
            if (north.px != 0.0f || north.pz != 0.0f || north.py != 2.0f) exact = false;
            if (south.px != 0.0f || south.pz != 0.0f || south.py != -2.0f) exact = false;
        }
        CHECK(exact, "both pole rows sit exactly on the axis at +/-radius");
    }

    // --- 3. Documented, deliberately: it is OPEN, and less even than an icosphere. ---
    {
        const shapes::MeshData uv = shapes::makeSphere(1.0f, 10, 20, white);
        const MeshTopology topo = buildTopology(uv);
        CHECK(topo.boundaryEdgeCount > 0,
              "a UV sphere is open (wrap seam + pole rows) — use makeIcosphere for a closed shell");
        CHECK(topo.nonManifoldEdgeCount == 0, "open, but not non-manifold");

        const shapes::MeshData ico = makeIcosphere(1.0f, 2);
        CHECK(buildTopology(ico).boundaryEdgeCount == 0, "the icosphere IS closed");
        CHECK(analyzeTriangleQuality(ico).minQuality > analyzeTriangleQuality(uv).minQuality,
              "the icosphere's worst triangle beats the UV sphere's (no pole crowding)");
    }

    // --- 4. Removing the flat triangles did not change the shape. ---
    {
        const shapes::MeshData m = shapes::makeSphere(1.0f, 24, 48, white);
        const MeshStats s = analyzeMesh(m);
        // A fine UV sphere inscribed in the unit sphere: area approaches 4*pi from below.
        CHECK(s.surfaceArea < 4.0 * 3.14159265358979 && s.surfaceArea > 12.5,
              "surface area is just under the true sphere's, as an inscribed mesh must be");
        float worst = 0.0f;
        for (const MeshVertex& v : m.vertices) {
            worst = std::max(worst, std::fabs(std::sqrt(v.px * v.px + v.py * v.py + v.pz * v.pz) - 1.0f));
        }
        CHECK(worst < 1e-6f, "every vertex still sits on the unit sphere");
    }

    if (g_fail == 0) {
        std::printf("uvsphere: OK — no degenerate triangles, exact poles, open-by-design, shape intact.\n");
        return 0;
    }
    std::printf("uvsphere: %d failure(s).\n", g_fail);
    return 1;
}
