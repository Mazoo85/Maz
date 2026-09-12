// tests/render/meshslicelayers.cpp — verifies slab slicing (render::sliceLayers). Ground truths: a closed box
// sliced into N layers along Y yields N contours at the layer-centre heights, each a closed rectangular loop whose
// points all lie at that layer's height; the heights stay strictly inside the box; count<1 or a zero-extent axis
// yields nothing. Pure CPU, headless.
#include "maz/render/MeshSliceLayers.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

// A WATERTIGHT axis-aligned box: 8 SHARED corner vertices, 12 triangles. Sharing the corner indices between
// faces is what lets sliceMesh close each cross-section into a single ring (it chains by mesh-edge, so coincident
// corners must be the same index — a per-face vertex soup would slice into open chains, not a closed loop).
static shapes::MeshData box(float hx, float hy, float hz) {
    shapes::MeshData m;
    auto v = [](float x, float y, float z) { MeshVertex p{}; p.px = x; p.py = y; p.pz = z; p.r = p.g = p.b = 1; return p; };
    m.vertices = {
        v(-hx,-hy,-hz), v( hx,-hy,-hz), v( hx,-hy, hz), v(-hx,-hy, hz), // 0-3 bottom (y=-hy)
        v(-hx, hy,-hz), v( hx, hy,-hz), v( hx, hy, hz), v(-hx, hy, hz), // 4-7 top    (y=+hy)
    };
    m.indices = {
        0,1,2, 0,2,3, // bottom
        4,6,5, 4,7,6, // top
        3,2,6, 3,6,7, // +Z
        1,0,4, 1,4,5, // -Z
        2,1,5, 2,5,6, // +X
        0,3,7, 0,7,4, // -X
    };
    return m;
}

int main() {
    // Box: y from -3..3. Slice into 3 layers along Y.
    const MeshLayers L = sliceLayers(box(2.0f, 3.0f, 2.0f), /*axis=Y*/1, 3);

    // --- 1. Three layers at the layer-centre heights -2, 0, +2. ---
    {
        CHECK(L.layers.size() == 3 && L.heights.size() == 3, "3 layers with 3 heights");
        CHECK(near(L.heights[0], -2.0f, 1e-4f) && near(L.heights[1], 0.0f, 1e-4f) && near(L.heights[2], 2.0f, 1e-4f),
              "heights land at the layer centres (-2, 0, 2)");
    }

    // --- 2. Each layer is a closed loop and every contour point sits at that layer's height. ---
    {
        bool ok = true;
        for (std::size_t i = 0; i < L.layers.size(); ++i) {
            const SliceContour& c = L.layers[i];
            if (c.loops.empty() || c.points.empty()) { ok = false; continue; }
            if (!c.loopClosed.empty() && c.loopClosed[0] != 1) ok = false; // a box cut is a closed ring
            for (const maz::math::vec3& p : c.points)
                if (!near(p.y, L.heights[i], 1e-4f)) ok = false; // points lie on the slice plane
        }
        CHECK(ok, "each layer is a closed loop whose points lie exactly on its plane");
    }

    // --- 3. The rectangular cross-section has (at least) the 4 corners of the box footprint. ---
    {
        const SliceContour& mid = L.layers[1];
        CHECK(mid.points.size() >= 4, "a box cross-section has at least 4 contour points");
        // Every point's X and Z are on the box footprint boundary (|x|~2 or |z|~2).
        bool onRim = true;
        for (const maz::math::vec3& p : mid.points)
            if (!(near(std::fabs(p.x), 2.0f, 1e-3f) || near(std::fabs(p.z), 2.0f, 1e-3f))) onRim = false;
        CHECK(onRim, "contour points lie on the box's side walls");
    }

    // --- 4. Degenerate inputs. ---
    {
        CHECK(sliceLayers(box(1, 1, 1), 1, 0).layers.empty(), "count 0 -> no layers");
        CHECK(sliceLayers(shapes::MeshData{}, 1, 3).layers.empty(), "empty mesh -> no layers");
        // A flat mesh has no Y extent -> no layers along Y.
        shapes::MeshData flat;
        flat.vertices = {}; // handled by empty check; build a real zero-extent one:
        MeshVertex a{}, b{}, c{}; a.px = 0; b.px = 1; c.pz = 1; // all y=0
        flat.vertices = {a, b, c}; flat.indices = {0, 1, 2};
        CHECK(sliceLayers(flat, 1, 3).layers.empty(), "a zero-Y-extent mesh -> no Y layers");
    }

    if (g_fail == 0) {
        std::printf("meshslicelayers: OK — N layers at centre heights, closed rings on the planes, degenerates safe.\n");
        return 0;
    }
    std::printf("meshslicelayers: %d failure(s).\n", g_fail);
    return 1;
}
