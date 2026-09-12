// tests/render/meshbend.cpp — verifies the bend deformer (render::bendMesh). Ground truths: the along-axis coord
// becomes a swept angle (along/radius) around a bend centre `radius` up the up-axis; each vertex's distance from
// that centre equals radius − up; the hinge column (along=0) stays put; the third axis is untouched. Pure CPU.
#include "maz/render/MeshBend.hpp"

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

int main() {
    const float kPi = 3.14159265358979323846f;
    const float R = 2.0f;                        // bend radius
    const maz::math::vec3 C(0, R, 0);            // bend centre: R up the up-axis (Y) from origin
    auto distC = [C](const MeshVertex& v) {
        const float dx = v.px - C.x, dy = v.py - C.y, dz = v.pz - C.z;
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    };

    // A bar along X (alongAxis=0), bending toward Y (upAxis=1).
    shapes::MeshData m;
    m.vertices = {
        at(0, 0, 0),              // 0: hinge, neutral
        at(kPi, 0, 0),            // 1: angle = pi/R = pi/2 at R=2 -> lands at (R, R)
        at(0, 0.5f, 0),           // 2: hinge column, up=0.5
        at(R, 0, 0),              // 3: angle = 1 rad, neutral
        at(0, 0, 3),              // 4: third-axis passenger
    };
    m.indices = {0, 1, 2};

    const shapes::MeshData b = bendMesh(m, /*along=X*/0, /*up=Y*/1, R);

    // --- 1. The hinge column (along=0) stays put. ---
    {
        CHECK(near(b.vertices[0].px, 0.0f, 1e-5f) && near(b.vertices[0].py, 0.0f, 1e-5f), "hinge vertex (0,0) unmoved");
        CHECK(near(b.vertices[2].px, 0.0f, 1e-5f) && near(b.vertices[2].py, 0.5f, 1e-5f), "hinge column keeps its up value");
    }

    // --- 2. x = pi at R=2 sweeps a quarter turn -> lands at (R, R). ---
    {
        CHECK(near(b.vertices[1].px, R, 1e-4f) && near(b.vertices[1].py, R, 1e-4f), "quarter-turn point lands at (R,R)");
    }

    // --- 3. Distance from the bend centre equals radius − up for every vertex. ---
    {
        CHECK(near(distC(b.vertices[0]), R - 0.0f, 1e-4f), "neutral vertex is at distance R from the centre");
        CHECK(near(distC(b.vertices[2]), R - 0.5f, 1e-4f), "an up=0.5 vertex is at distance R-0.5");
        CHECK(near(distC(b.vertices[3]), R - 0.0f, 1e-4f), "another neutral vertex is at distance R");
    }

    // --- 4. The swept angle is along/radius: vertex 3 (x=2,R=2) is at 1 radian. ---
    {
        // Its position: (sin(1)*R, R - cos(1)*R). Verify against the formula directly.
        const float ex = std::sin(1.0f) * R, ey = R - std::cos(1.0f) * R;
        CHECK(near(b.vertices[3].px, ex, 1e-4f) && near(b.vertices[3].py, ey, 1e-4f), "swept angle = along/radius");
    }

    // --- 5. The third axis (Z) rides through untouched. ---
    {
        CHECK(near(b.vertices[4].pz, 3.0f, 1e-6f), "the third axis is unchanged by the bend");
    }

    // --- 6. A near-zero radius is a safe no-op; empty is safe. ---
    {
        const shapes::MeshData none = bendMesh(m, 0, 1, 0.0f);
        CHECK(near(none.vertices[1].px, kPi, 1e-6f), "radius 0 leaves the mesh unbent");
        CHECK(bendMesh(shapes::MeshData{}, 0, 1, R).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshbend: OK — along becomes a swept angle, radius=R-up preserved, hinge + third axis fixed.\n");
        return 0;
    }
    std::printf("meshbend: %d failure(s).\n", g_fail);
    return 1;
}
