// tests/render/meshao.cpp — verifies per-vertex ambient-occlusion baking (render::bakeVertexAO). Physical
// expectations: on a bare flat floor with no occluders every vertex is near-fully open (AO ~ 0); adding a
// pillar standing on the floor makes the floor vertices right beside its base MORE occluded than a vertex far
// out in the open; the result is in [0,1] and deterministic. Pure CPU, headless.
#include "maz/render/MeshAmbientOcclusion.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static MeshVertex vtx(float x, float y, float z) {
    MeshVertex v{}; v.px = x; v.py = y; v.pz = z; v.r = v.g = v.b = 1.0f; return v;
}

// Append an axis-aligned box [x0,x1]x[y0,y1]x[z0,z1] (12 tris) to `m`.
static void addBox(shapes::MeshData& m, float x0, float y0, float z0, float x1, float y1, float z1) {
    const std::uint32_t b = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back(vtx(x0,y0,z0)); m.vertices.push_back(vtx(x1,y0,z0));
    m.vertices.push_back(vtx(x1,y1,z0)); m.vertices.push_back(vtx(x0,y1,z0));
    m.vertices.push_back(vtx(x0,y0,z1)); m.vertices.push_back(vtx(x1,y0,z1));
    m.vertices.push_back(vtx(x1,y1,z1)); m.vertices.push_back(vtx(x0,y1,z1));
    const std::uint32_t f[36] = {0,1,2, 2,3,0,  1,5,6, 6,2,1,  5,4,7, 7,6,5,
                                 4,0,3, 3,7,4,  3,2,6, 6,7,3,  4,5,1, 1,0,4};
    for (std::uint32_t i : f) m.indices.push_back(b + i);
}

int main() {
    // A flat floor over [0,6] x [0,6] at y=0, two probe vertices explicit: index 0 = near the pillar base,
    // index 1 = far open corner. The floor quad uses verts 2..5.
    shapes::MeshData m;
    m.vertices.push_back(vtx(2.0f, 0.0f, 2.0f)); // 0: probe NEAR (pillar will sit at x,z in [2.2,3.2])
    m.vertices.push_back(vtx(6.0f, 0.0f, 6.0f)); // 1: probe FAR
    const std::uint32_t f0 = static_cast<std::uint32_t>(m.vertices.size());
    m.vertices.push_back(vtx(0,0,0)); m.vertices.push_back(vtx(6,0,0));
    m.vertices.push_back(vtx(6,0,6)); m.vertices.push_back(vtx(0,0,6));
    m.indices.insert(m.indices.end(), {f0, f0+1, f0+2, f0, f0+2, f0+3}); // floor, normal +Y

    // --- 1. Bare floor: both probes are near-fully open. ---
    {
        const std::vector<float> ao = bakeVertexAO(m, 64, 20.0f);
        CHECK(ao[0] < 0.05f && ao[1] < 0.05f, "bare floor vertices are nearly unoccluded");
        for (float a : ao) CHECK(a >= 0.0f && a <= 1.0f, "AO is within [0,1]");
    }

    // --- 2. Add a pillar next to probe 0: the near vertex becomes more occluded than the far one. ---
    {
        shapes::MeshData withPillar = m;
        addBox(withPillar, 2.2f, 0.0f, 2.2f, 3.2f, 3.0f, 3.2f); // tall pillar hugging probe 0
        const std::vector<float> ao = bakeVertexAO(withPillar, 128, 20.0f);
        CHECK(ao[0] > ao[1] + 0.1f, "vertex beside the pillar is markedly more occluded than the far one");
        CHECK(ao[1] < 0.05f, "the far vertex stays open");
        CHECK(ao[0] > 0.1f, "the near vertex picks up real occlusion");
    }

    // --- 3. Deterministic: same input -> identical bake. ---
    {
        const std::vector<float> a = bakeVertexAO(m, 32, 20.0f);
        const std::vector<float> b = bakeVertexAO(m, 32, 20.0f);
        CHECK(a == b, "AO bake is deterministic");
    }

    // --- 4. Degenerate inputs are safe. ---
    {
        CHECK(bakeVertexAO(shapes::MeshData{}, 16, 10.0f).empty(), "empty mesh -> empty AO");
    }

    if (g_fail == 0) {
        std::printf("meshao: OK — open floor unoccluded, pillar darkens its neighbour, [0,1], deterministic.\n");
        return 0;
    }
    std::printf("meshao: %d failure(s).\n", g_fail);
    return 1;
}
