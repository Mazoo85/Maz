// tests/render/meshselfintersect.cpp — verifies mesh self-intersection detection (render::findSelfIntersections
// / hasSelfIntersection). Ground truths on hand-built meshes:
//   * a flat quad (two edge-adjacent triangles) is clean — adjacency is excluded, not flagged;
//   * adding a triangle that stabs through the quad (sharing no vertex) reports exactly that crossing;
//   * a closed convex icosphere has no self-intersections (a strong real-mesh check);
//   * results are deterministic and ordered (triA < triB);
//   * hasSelfIntersection agrees with findSelfIntersections being non-empty;
//   * an empty / single-triangle mesh is clean.
#include "maz/render/MeshIcosphere.hpp"
#include "maz/render/MeshSelfIntersect.hpp"

#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vert(float x, float y, float z) {
    MeshVertex v{};
    v.px = x; v.py = y; v.pz = z;
    v.r = v.g = v.b = 1.0f;
    return v;
}

int main() {
    // A flat quad in the XY plane: 4 vertices, 2 triangles sharing the diagonal edge 0-2.
    shapes::MeshData quad;
    quad.vertices = {vert(0, 0, 0), vert(2, 0, 0), vert(2, 2, 0), vert(0, 2, 0)};
    quad.indices = {0, 1, 2, 0, 2, 3};

    // --- 1. The quad alone is clean (the two triangles are edge-adjacent, not intersecting). ---
    {
        const auto hits = findSelfIntersections(quad);
        CHECK(hits.empty(), "an edge-adjacent quad has no self-intersections");
        CHECK(!hasSelfIntersection(quad), "hasSelfIntersection agrees the quad is clean");
    }

    // --- 2. Add a vertical triangle that stabs through the quad's interior (shares no vertex). ---
    {
        shapes::MeshData m = quad;
        m.vertices.push_back(vert(1.0f, 1.0f, 1.0f));   // 4: above
        m.vertices.push_back(vert(0.8f, 1.0f, -1.0f));  // 5: below
        m.vertices.push_back(vert(1.2f, 1.0f, -1.0f));  // 6: below
        m.indices.insert(m.indices.end(), {4, 5, 6});   // triangle index 2

        const auto hits = findSelfIntersections(m);
        CHECK(!hits.empty(), "a triangle piercing the quad is detected");
        CHECK(hasSelfIntersection(m), "hasSelfIntersection detects the piercing triangle");
        // Every reported pair must involve the piercer (triangle 2) and be ordered triA < triB.
        bool involvesPiercer = true, ordered = true;
        for (const SelfIntersection& s : hits) {
            if (s.triA >= s.triB) ordered = false;
            if (s.triB != 2 && s.triA != 2) involvesPiercer = false;
        }
        CHECK(ordered, "reported pairs are ordered triA < triB");
        CHECK(involvesPiercer, "every reported pair involves the piercing triangle");

        // Determinism: identical input yields an identical list.
        const auto hits2 = findSelfIntersections(m);
        bool same = hits.size() == hits2.size();
        for (std::size_t i = 0; i < hits.size() && same; ++i)
            if (hits[i].triA != hits2[i].triA || hits[i].triB != hits2[i].triB) same = false;
        CHECK(same, "self-intersection results are deterministic");
    }

    // --- 3. A closed convex icosphere has no self-intersections (real-mesh sanity). ---
    {
        const shapes::MeshData sphere = makeIcosphere(1.5f, 2); // ~320 triangles, watertight, convex
        CHECK(!hasSelfIntersection(sphere), "a clean icosphere has no self-intersections");
        CHECK(findSelfIntersections(sphere).empty(), "icosphere self-intersection list is empty");
    }

    // --- 4. Empty and single-triangle meshes are clean, not crashes. ---
    {
        CHECK(findSelfIntersections(shapes::MeshData{}).empty(), "empty mesh has no self-intersections");
        CHECK(!hasSelfIntersection(shapes::MeshData{}), "empty mesh hasSelfIntersection is false");
        shapes::MeshData one;
        one.vertices = {vert(0, 0, 0), vert(1, 0, 0), vert(0, 1, 0)};
        one.indices = {0, 1, 2};
        CHECK(findSelfIntersections(one).empty(), "a single triangle cannot self-intersect");
    }

    if (g_fail == 0) {
        std::printf("meshselfintersect: OK — detects piercing faces, excludes adjacency, clean on convex meshes.\n");
        return 0;
    }
    std::printf("meshselfintersect: %d failure(s).\n", g_fail);
    return 1;
}
