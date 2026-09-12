// tests/render/meshripple.cpp — verifies the ripple/wave deformer (render::rippleMesh). Ground truths: each
// vertex is pushed along the chosen axis by amplitude*sin(radial*frequency - phase) where radial is the distance
// from the centre in the perpendicular plane; the perpendicular coords are untouched; amplitude 0 is a no-op;
// phase shifts the wave; a different centre re-measures the radius. Pure CPU, headless.
#include "maz/render/MeshRipple.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

static MeshVertex at(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.ny = 1.0f; v.r = v.g = v.b = 1.0f; return v;
}

int main() {
    const float kPi = 3.14159265358979323846f;

    // Vertices along +X at radii 0,1,2,3 in the XZ plane (y=0). Ripple along Y (axis 1).
    shapes::MeshData m;
    m.vertices = {at(0, 0, 0), at(1, 0, 0), at(2, 0, 0), at(3, 0, 0), at(0, 0, 2)};
    m.indices = {0, 1, 2};

    const float amp = 0.5f, freq = kPi / 2.0f; // sin(radial * pi/2)
    const shapes::MeshData r = rippleMesh(m, /*axis=Y*/1, amp, freq, /*phase=*/0.0f);

    // --- 1. sin at each integer radius: 0->0, 1->+amp, 2->0, 3->-amp. ---
    {
        CHECK(near(r.vertices[0].py, 0.0f, 1e-5f), "radius 0: sin(0)=0 -> no offset");
        CHECK(near(r.vertices[1].py, amp, 1e-5f), "radius 1: sin(pi/2)=1 -> +amplitude");
        CHECK(near(r.vertices[2].py, 0.0f, 1e-5f), "radius 2: sin(pi)=0 -> no offset");
        CHECK(near(r.vertices[3].py, -amp, 1e-5f), "radius 3: sin(3pi/2)=-1 -> -amplitude");
    }

    // --- 2. The rings are circular: a vertex at radius 2 along Z gets the same offset as radius 2 along X. ---
    {
        CHECK(near(r.vertices[4].py, r.vertices[2].py, 1e-5f), "same radius (in Z) gives the same offset");
    }

    // --- 3. The perpendicular coordinates (X, Z) are untouched. ---
    {
        bool xzFixed = true;
        for (std::size_t i = 0; i < m.vertices.size(); ++i)
            if (!near(r.vertices[i].px, m.vertices[i].px, 1e-6f) || !near(r.vertices[i].pz, m.vertices[i].pz, 1e-6f))
                xzFixed = false;
        CHECK(xzFixed, "only the ripple axis moves; the plane coords stay put");
    }

    // --- 4. A phase shift moves the wave: phase pi/2 makes the centre trough (sin(-pi/2) = -1). ---
    {
        const shapes::MeshData rp = rippleMesh(m, 1, amp, freq, /*phase=*/kPi / 2.0f);
        CHECK(near(rp.vertices[0].py, -amp, 1e-5f), "phase pi/2 puts a trough at the centre");
    }

    // --- 5. Amplitude 0 is a no-op. ---
    {
        const shapes::MeshData z = rippleMesh(m, 1, 0.0f, freq);
        bool same = true;
        for (std::size_t i = 0; i < m.vertices.size(); ++i)
            if (!near(z.vertices[i].py, m.vertices[i].py, 1e-6f)) same = false;
        CHECK(same, "amplitude 0 leaves the mesh flat");
    }

    // --- 6. A different centre re-measures the radius (centre at x=1 -> vertex 1 is now radius 0). ---
    {
        const shapes::MeshData rc = rippleMesh(m, 1, amp, freq, 0.0f, maz::math::vec3(1, 0, 0));
        CHECK(near(rc.vertices[1].py, 0.0f, 1e-5f), "vertex at the new centre has radius 0 -> no offset");
    }

    // --- 7. Empty mesh is safe. ---
    {
        CHECK(rippleMesh(shapes::MeshData{}, 1, amp, freq).vertices.empty(), "empty -> empty");
    }

    if (g_fail == 0) {
        std::printf("meshripple: OK — concentric sine offset along the axis, circular rings, phase + centre honoured.\n");
        return 0;
    }
    std::printf("meshripple: %d failure(s).\n", g_fail);
    return 1;
}
