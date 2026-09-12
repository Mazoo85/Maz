// tests/render/meshsilhouette.cpp — verifies view-dependent outline edges (render::silhouetteEdges /
// silhouetteEdgesFromEye). Ground truths: a watertight cube viewed corner-on has a 6-edge hexagonal silhouette; a
// flat quad has no interior silhouette but its 4 open-boundary edges are all outline; flipping the view flips
// nothing about the silhouette SET (front/back boundary is orientation-invariant); the perspective (eye) query
// agrees with the directional one for a distant eye. Pure CPU, headless.
#include "maz/render/MeshSilhouette.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

// Watertight axis-aligned unit cube: 8 shared corners, 12 triangles (outward winding).
static shapes::MeshData cube(float h) {
    shapes::MeshData m;
    auto v = [](float x, float y, float z) { MeshVertex p{}; p.px = x; p.py = y; p.pz = z; p.r = p.g = p.b = 1; return p; };
    m.vertices = {
        v(-h,-h,-h), v(h,-h,-h), v(h,-h,h), v(-h,-h,h),  // 0-3 bottom
        v(-h, h,-h), v(h, h,-h), v(h, h,h), v(-h, h,h),  // 4-7 top
    };
    m.indices = {
        0,1,2, 0,2,3,   4,6,5, 4,7,6,   // bottom(-Y), top(+Y)
        3,2,6, 3,6,7,   1,0,4, 1,4,5,   // +Z, -Z
        2,1,5, 2,5,6,   0,3,7, 0,7,4,   // +X, -X
    };
    return m;
}

int main() {
    // --- 1. A cube viewed corner-on has a 6-edge (hexagonal) silhouette. ---
    {
        const shapes::MeshData c = cube(1.0f);
        // Looking from the +X+Y+Z corner toward the origin: three faces front, three back -> hexagon.
        const SilhouetteResult s = silhouetteEdges(c, maz::math::vec3(-1, -1, -1), /*includeBoundary=*/true);
        CHECK(s.edges.size() == 6, "corner-on cube silhouette is a 6-edge hexagon");
        CHECK(s.boundaryCount == 0, "a closed cube has no boundary edges");
    }

    // --- 2. Silhouette SET is invariant to reversing the view direction. ---
    {
        const shapes::MeshData c = cube(1.0f);
        const SilhouetteResult a = silhouetteEdges(c, maz::math::vec3(-1, -1, -1));
        const SilhouetteResult b = silhouetteEdges(c, maz::math::vec3(1, 1, 1));
        CHECK(a.edges.size() == b.edges.size() && a.edges.size() == 6, "reversing the view keeps the same 6 edges");
    }

    // --- 3. A flat quad: no interior silhouette; its 4 boundary edges are all outline. ---
    {
        shapes::MeshData q;
        auto v = [](float x, float z) { MeshVertex p{}; p.px = x; p.py = 0; p.pz = z; p.r = p.g = p.b = 1; return p; };
        q.vertices = {v(-1, -1), v(1, -1), v(1, 1), v(-1, 1)};
        q.indices = {0, 1, 2, 0, 2, 3};
        const SilhouetteResult s = silhouetteEdges(q, maz::math::vec3(0, -1, 0), /*includeBoundary=*/true);
        CHECK(s.edges.size() == 4 && s.boundaryCount == 4, "a quad's 4 open edges are the whole outline");
        // The shared interior diagonal (0-2) is NOT a silhouette edge (both tris coplanar).
        bool diagonal = false;
        for (const auto& e : s.edges) if ((e.first == 0 && e.second == 2) || (e.first == 2 && e.second == 0)) diagonal = true;
        CHECK(!diagonal, "the interior diagonal is not on the silhouette");
        // With boundary excluded, a single flat quad has no silhouette at all.
        CHECK(silhouetteEdges(q, maz::math::vec3(0, -1, 0), /*includeBoundary=*/false).edges.empty(),
              "excluding boundary, a flat quad has an empty silhouette");
    }

    // --- 4. Perspective (eye) query agrees with the directional one for a distant eye. ---
    {
        const shapes::MeshData c = cube(1.0f);
        const SilhouetteResult e = silhouetteEdgesFromEye(c, maz::math::vec3(1000, 1000, 1000));
        CHECK(e.edges.size() == 6, "distant-eye perspective silhouette matches the orthographic hexagon");
    }

    // --- 5. Empty mesh is safe. ---
    {
        CHECK(silhouetteEdges(shapes::MeshData{}, maz::math::vec3(0, 0, -1)).edges.empty(), "empty mesh -> no edges");
    }

    if (g_fail == 0) {
        std::printf("meshsilhouette: OK — cube hexagon, view-reversal invariant, quad boundary, eye agrees, safe.\n");
        return 0;
    }
    std::printf("meshsilhouette: %d failure(s).\n", g_fail);
    return 1;
}
