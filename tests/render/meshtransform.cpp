// tests/render/meshtransform.cpp — verifies baking a transform into a mesh (render::applyTransform). Ground
// truths: translation shifts positions and leaves normals; uniform scale scales positions and preserves
// normals; a 90-degree rotation rotates both positions and normals; NON-UNIFORM scale transforms normals by the
// inverse-transpose (not the same matrix as positions) so they stay perpendicular. Pure CPU, headless.
#include "maz/render/MeshTransform.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex nvtx(float x, float y, float z, float nx, float ny, float nz) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.nx = nx; v.ny = ny; v.nz = nz; v.r = v.g = v.b = 1.0f; return v;
}
static bool near(float a, float b, float tol) { return std::fabs(a - b) <= tol; }

int main() {
    // --- 1. Translation: positions shift, normals unchanged. ---
    {
        shapes::MeshData m;
        m.vertices = {nvtx(1, 2, 3, 0, 1, 0)};
        const shapes::MeshData r = applyTransform(m, translationMatrix(maz::math::vec3(10, 20, 30)));
        const MeshVertex& v = r.vertices[0];
        CHECK(near(v.px, 11, 1e-5f) && near(v.py, 22, 1e-5f) && near(v.pz, 33, 1e-5f), "position translated");
        CHECK(near(v.nx, 0, 1e-6f) && near(v.ny, 1, 1e-6f) && near(v.nz, 0, 1e-6f), "normal unchanged by translation");
    }

    // --- 2. Uniform scale x3: positions scale, normals preserved (renormalised). ---
    {
        shapes::MeshData m;
        m.vertices = {nvtx(1, 0, 0, 1, 0, 0)};
        const shapes::MeshData r = applyTransform(m, scaleMatrix(maz::math::vec3(3, 3, 3)));
        const MeshVertex& v = r.vertices[0];
        CHECK(near(v.px, 3, 1e-5f), "position scaled x3");
        CHECK(near(v.nx, 1, 1e-6f) && near(v.ny, 0, 1e-6f), "normal stays unit +X under uniform scale");
    }

    // --- 3. 90-degree rotation about Z: (1,0,0) -> (0,1,0) for both position and normal. ---
    {
        shapes::MeshData m;
        m.vertices = {nvtx(1, 0, 0, 1, 0, 0)};
        const float halfPi = 1.57079632679f;
        const shapes::MeshData r = applyTransform(m, rotationMatrix(maz::math::vec3(0, 0, 1), halfPi));
        const MeshVertex& v = r.vertices[0];
        CHECK(near(v.px, 0, 1e-5f) && near(v.py, 1, 1e-5f), "position rotated 90 deg about Z");
        CHECK(near(v.nx, 0, 1e-5f) && near(v.ny, 1, 1e-5f), "normal rotated 90 deg about Z");
    }

    // --- 4. Non-uniform scale (1,2,1): a 45-degree normal transforms by the inverse-transpose. ---
    {
        // A surface whose normal is (1,1,0)/sqrt(2). Scaling Y by 2 must transform the normal by diag(1,0.5,1)
        // (inverse-transpose of diag(1,2,1)), giving nx:ny = 2:1 — NOT the 1:2 a naive position-matrix would give.
        const float inv = 1.0f / std::sqrt(2.0f);
        shapes::MeshData m;
        m.vertices = {nvtx(0, 0, 0, inv, inv, 0)};
        const shapes::MeshData r = applyTransform(m, scaleMatrix(maz::math::vec3(1, 2, 1)));
        const MeshVertex& v = r.vertices[0];
        CHECK(near(v.nx * v.nx + v.ny * v.ny + v.nz * v.nz, 1.0f, 1e-5f), "the transformed normal is unit length");
        CHECK(v.nx > v.ny && near(v.nx / v.ny, 2.0f, 1e-4f),
              "non-uniform scale transforms the normal by the inverse-transpose (nx:ny = 2:1)");
    }

    // --- 5. Composed translate * rotate * scale on a known point. ---
    {
        shapes::MeshData m;
        m.vertices = {nvtx(1, 0, 0, 0, 0, 1)};
        const float halfPi = 1.57079632679f;
        const maz::math::mat4 t = translationMatrix(maz::math::vec3(5, 0, 0));
        const maz::math::mat4 rot = rotationMatrix(maz::math::vec3(0, 0, 1), halfPi);
        const maz::math::mat4 s = scaleMatrix(maz::math::vec3(2, 2, 2));
        const shapes::MeshData r = applyTransform(m, t * rot * s); // scale then rotate then translate
        const MeshVertex& v = r.vertices[0];
        // (1,0,0) -> scale x2 -> (2,0,0) -> rot90Z -> (0,2,0) -> translate +5x -> (5,2,0).
        CHECK(near(v.px, 5, 1e-4f) && near(v.py, 2, 1e-4f) && near(v.pz, 0, 1e-4f), "TRS composes correctly");
        CHECK(near(v.nz, 1, 1e-5f), "the +Z normal is unaffected by a Z-rotation and stays unit");
    }

    // --- 6. Empty mesh is safe. ---
    {
        CHECK(applyTransform(shapes::MeshData{}, scaleMatrix(maz::math::vec3(2, 2, 2))).vertices.empty(),
              "empty mesh -> empty result");
    }

    if (g_fail == 0) {
        std::printf("meshtransform: OK — translate/scale/rotate positions+normals, inverse-transpose normals, TRS compose.\n");
        return 0;
    }
    std::printf("meshtransform: %d failure(s).\n", g_fail);
    return 1;
}
