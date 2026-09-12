// tests/render/meshorient.cpp — verifies that every procedural primitive winds its triangles so the
// right-hand-rule face normal points OUTWARD, i.e. it agrees with the stored (outward) vertex normals.
// This is the ground truth for back-face culling: with VK_FRONT_FACE_COUNTER_CLOCKWISE and back-face
// culling on the opaque pipelines, a viewer outside the mesh must see the front (CCW) side. The glTF
// importer (Model.cpp) uses the same convention, so imported and procedural meshes cull identically.
//
// For each generator and each triangle, we compute the RHR normal cross(v1-v0, v2-v0) and require a
// POSITIVE dot with the average of the three vertices' stored normals. Degenerate triangles (cone apex,
// sphere/capsule poles, where the RHR normal collapses) are skipped via an epsilon on its length.
#include "maz/render/Shapes.hpp"
#include "maz/render/Shapes3D.hpp"

#include "maz/math/Math.hpp"

#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::shapes::MeshData;
using maz::render::MeshVertex;

namespace {

maz::math::vec3 pos(const MeshVertex& v) { return maz::math::vec3(v.px, v.py, v.pz); }
maz::math::vec3 nrm(const MeshVertex& v) { return maz::math::vec3(v.nx, v.ny, v.nz); }

// Every triangle's RHR normal must agree (positive dot) with the averaged stored outward normals.
// Returns the number of non-degenerate triangles checked so we also confirm the mesh isn't empty.
int checkOutward(const MeshData& m, const char* name) {
    CHECK(m.indices.size() % 3u == 0u, name);
    int checked = 0;
    for (std::size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        const MeshVertex& a = m.vertices[m.indices[t + 0]];
        const MeshVertex& b = m.vertices[m.indices[t + 1]];
        const MeshVertex& c = m.vertices[m.indices[t + 2]];
        const maz::math::vec3 rhr = maz::math::cross(pos(b) - pos(a), pos(c) - pos(a));
        // Skip zero-area / degenerate triangles (apex fans, pole rings) via an epsilon on |RHR|^2.
        if (maz::math::dot(rhr, rhr) < 1e-16f) {
            continue;
        }
        const maz::math::vec3 storedAvg = nrm(a) + nrm(b) + nrm(c);
        const float agreement = maz::math::dot(rhr, storedAvg);
        if (!(agreement > 0.0f)) {
            std::printf("FAIL: %s triangle %zu winds inward (RHR . stored = %g)\n", name,
                        t / 3u, static_cast<double>(agreement));
            ++g_fail;
        }
        ++checked;
    }
    CHECK(checked > 0, name);
    return checked;
}

} // namespace

int main() {
    using namespace maz::render::shapes;
    const maz::render::Color white{1.0f, 1.0f, 1.0f, 1.0f};

    checkOutward(makeBox(2.0f, white), "makeBox");
    checkOutward(makeSphere(1.0f, 12, 16, white), "makeSphere");
    checkOutward(makePlane(3.0f, white), "makePlane");
    checkOutward(makeCylinder(1.0f, 2.0f, 20, white), "makeCylinder");
    checkOutward(makeCone(1.0f, 2.0f, 20, white), "makeCone");
    checkOutward(makeTorus(2.0f, 0.5f, 24, 12, white), "makeTorus");
    checkOutward(makeCapsule(0.5f, 1.5f, 16, 6, white), "makeCapsule");

    if (g_fail == 0) {
        std::printf("meshorient: OK — all seven primitives wind CCW-outward (RHR normal agrees with stored normals).\n");
        return 0;
    }
    std::printf("meshorient: %d failure(s).\n", g_fail);
    return 1;
}
