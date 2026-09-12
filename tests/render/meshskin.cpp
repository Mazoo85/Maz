// tests/render/meshskin.cpp — verifies loft/skin across sections (render::skinSections). Ground truths: bridging N
// ribs of P points makes N*P vertices and (N-1)*P quads (closed rings); a funnel of a big square rib and a small
// square rib skins a taper whose rib centroids and per-rib radii are preserved; closedPath adds the wrap band;
// open rings drop one edge; ragged/short inputs are safe. Pure CPU, headless.
#include "maz/render/MeshSkin.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// A square ring of 4 points at height y, half-width w, centred on (cx,*,cz).
static std::vector<maz::math::vec3> square(float w, float y, float cx, float cz) {
    return {maz::math::vec3(cx - w, y, cz - w), maz::math::vec3(cx + w, y, cz - w),
            maz::math::vec3(cx + w, y, cz + w), maz::math::vec3(cx - w, y, cz + w)};
}

int main() {
    // --- 1. Two identical square ribs -> a straight tube band: counts + closed loop. ---
    {
        std::vector<std::vector<maz::math::vec3>> secs = {square(1, 0, 0, 0), square(1, 4, 0, 0)};
        const shapes::MeshData m = skinSections(secs); // closedRings=true, closedPath=false
        CHECK(m.vertices.size() == 2u * 4u, "2 ribs * 4 pts -> 8 vertices");
        // (ribs-1) bands * P edges * 2 tris = 1 * 4 * 2 = 8 triangles.
        CHECK(m.indices.size() == 1u * 4u * 2u * 3u, "(ribs-1)*P*2 triangles for closed rings");
    }

    // --- 2. A funnel (big rib -> small rib): each rib's points keep its own half-width; ribs stay at their y. ---
    {
        std::vector<std::vector<maz::math::vec3>> secs = {square(2, 0, 0, 0), square(0.5f, 3, 0, 0)};
        const shapes::MeshData m = skinSections(secs);
        // Rib 0 vertices (indices 0..3) at y=0, |x|=|z|=2; rib 1 (4..7) at y=3, |x|=|z|=0.5.
        bool ok = true;
        for (int j = 0; j < 4; ++j) {
            const MeshVertex& a = m.vertices[static_cast<std::size_t>(j)];
            if (!near(a.py, 0.0f, 1e-6f) || !near(std::fabs(a.px), 2.0f, 1e-5f) || !near(std::fabs(a.pz), 2.0f, 1e-5f)) ok = false;
            const MeshVertex& b = m.vertices[static_cast<std::size_t>(4 + j)];
            if (!near(b.py, 3.0f, 1e-6f) || !near(std::fabs(b.px), 0.5f, 1e-5f) || !near(std::fabs(b.pz), 0.5f, 1e-5f)) ok = false;
        }
        CHECK(ok, "funnel keeps each rib's own size and height (taper, not resample)");
    }

    // --- 3. closedPath adds the wrap band (last rib -> first): one extra band of faces. ---
    {
        std::vector<std::vector<maz::math::vec3>> secs = {square(1, 0, 0, 0), square(1, 2, 0, 0), square(1, 4, 0, 0)};
        const shapes::MeshData open = skinSections(secs, true, false); // 2 bands
        const shapes::MeshData loop = skinSections(secs, true, true);  // 3 bands (wraps)
        CHECK(open.indices.size() == 2u * 4u * 2u * 3u, "3 ribs open path -> 2 bands");
        CHECK(loop.indices.size() == 3u * 4u * 2u * 3u, "closedPath -> 3 bands (wrap included)");
    }

    // --- 4. Open rings drop one edge per band vs closed rings. ---
    {
        std::vector<std::vector<maz::math::vec3>> secs = {square(1, 0, 0, 0), square(1, 2, 0, 0)};
        const shapes::MeshData closed = skinSections(secs, true, false);  // P=4 edges
        const shapes::MeshData openR = skinSections(secs, false, false);  // P-1=3 edges
        CHECK(closed.indices.size() == 1u * 4u * 2u * 3u, "closed rings: P edges");
        CHECK(openR.indices.size() == 1u * 3u * 2u * 3u, "open rings: P-1 edges");
    }

    // --- 5. Degenerate inputs are safe. ---
    {
        CHECK(skinSections({square(1, 0, 0, 0)}).vertices.empty(), "single rib -> empty");
        CHECK(skinSections({}).vertices.empty(), "no ribs -> empty");
        // Ragged ribs (different point counts) are rejected.
        std::vector<std::vector<maz::math::vec3>> ragged = {square(1, 0, 0, 0), {{0, 1, 0}, {1, 1, 0}}};
        CHECK(skinSections(ragged).vertices.empty(), "ragged ribs (unequal point counts) -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshskin: OK — rib counts, taper preserved, closedPath wrap, open rings, safe.\n");
        return 0;
    }
    std::printf("meshskin: %d failure(s).\n", g_fail);
    return 1;
}
