// tests/render/meshtwist.cpp — verifies the twist deformer (render::twistMesh). Ground truths: a vertex's angle
// about the axis is proportional to its distance along the axis (bottom unchanged, top rotated the full amount);
// height along the axis and distance from the axis are preserved exactly; angle 0 is identity. Pure CPU.
#include "maz/render/MeshTwist.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// A vertical bar: 4 bottom corners (y=0) and 4 top corners (y=4), x,z in {-1,+1}.
static shapes::MeshData bar() {
    shapes::MeshData m;
    m.vertices = {
        at(-1, 0, -1), at(1, 0, -1), at(1, 0, 1), at(-1, 0, 1), // bottom (indices 0..3)
        at(-1, 4, -1), at(1, 4, -1), at(1, 4, 1), at(-1, 4, 1), // top    (indices 4..7)
    };
    m.indices = {0, 1, 2, 4, 5, 6}; // two arbitrary (valid) triangles; twist ignores topology
    return m;
}

static float axisDist(const MeshVertex& v) { return std::sqrt(v.px * v.px + v.pz * v.pz); } // dist from Y axis

int main() {
    const shapes::MeshData base = bar();
    const float kPi = 3.14159265358979323846f;

    // Twist about Y by pi/8 per unit -> the top (y=4) turns a full pi/2 (90 degrees).
    const shapes::MeshData tw = twistMesh(base, /*axis=Y*/1, kPi / 8.0f);

    // --- 1. Bottom ring (y=0, zero distance along axis) is unchanged. ---
    {
        bool bottomFixed = true;
        for (int i = 0; i < 4; ++i) {
            if (!near(tw.vertices[static_cast<std::size_t>(i)].px, base.vertices[static_cast<std::size_t>(i)].px, 1e-5f)) bottomFixed = false;
            if (!near(tw.vertices[static_cast<std::size_t>(i)].pz, base.vertices[static_cast<std::size_t>(i)].pz, 1e-5f)) bottomFixed = false;
        }
        CHECK(bottomFixed, "the bottom ring (y=0) does not move");
    }

    // --- 2. Top corner (1,4,1) rotates 90 degrees about Y -> (1,4,-1). ---
    {
        // Vertex 6 is (1,4,1). Under a 90-degree twist about Y it maps to (px=1, pz=-1).
        const MeshVertex& v = tw.vertices[6];
        CHECK(near(v.px, 1.0f, 1e-4f) && near(v.pz, -1.0f, 1e-4f), "top corner (1,4,1) turns 90deg to (1,4,-1)");
        CHECK(near(v.py, 4.0f, 1e-6f), "its height along the axis is preserved");
    }

    // --- 3. Every vertex keeps its height (Y) and its distance from the axis (rigid per ring). ---
    {
        bool preserved = true;
        for (std::size_t i = 0; i < base.vertices.size(); ++i) {
            if (!near(tw.vertices[i].py, base.vertices[i].py, 1e-6f)) preserved = false;
            if (!near(axisDist(tw.vertices[i]), axisDist(base.vertices[i]), 1e-5f)) preserved = false;
        }
        CHECK(preserved, "height and radius from the axis are unchanged for every vertex");
    }

    // --- 4. Zero twist is the identity. ---
    {
        const shapes::MeshData id = twistMesh(base, 1, 0.0f);
        bool same = true;
        for (std::size_t i = 0; i < base.vertices.size(); ++i)
            if (!near(id.vertices[i].px, base.vertices[i].px, 1e-6f) ||
                !near(id.vertices[i].pz, base.vertices[i].pz, 1e-6f)) same = false;
        CHECK(same, "twist of 0 leaves the mesh unchanged");
    }

    // --- 5. Negative twist winds the other way (top corner mirrors across the axis). ---
    {
        const shapes::MeshData back = twistMesh(base, 1, -kPi / 8.0f);
        // Vertex 6 (1,4,1) under -90deg maps to (px=-1, pz=1).
        const MeshVertex& v = back.vertices[6];
        CHECK(near(v.px, -1.0f, 1e-4f) && near(v.pz, 1.0f, 1e-4f), "negative twist winds the opposite way");
    }

    // --- 6. Empty mesh is safe. ---
    {
        CHECK(twistMesh(shapes::MeshData{}, 1, 1.0f).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshtwist: OK — angle scales along the axis, height + radius preserved, 0 is identity.\n");
        return 0;
    }
    std::printf("meshtwist: %d failure(s).\n", g_fail);
    return 1;
}
