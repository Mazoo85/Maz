// tests/render/gltf_roundtrip.cpp — the strict end-to-end check for render::encodeGlb: export a mesh
// to .glb, then re-import it with the real cgltf-backed loadGltf and confirm the geometry and
// per-vertex colour survive. This proves the emitted glTF is spec-correct (cgltf validates on load),
// which the pure gltfwriter test cannot. Writes a temp .glb into the test working directory.
#include "maz/io/Serialize.hpp"      // io::writeFile
#include "maz/render/GltfWriter.hpp" // render::encodeGlb
#include "maz/render/Model.hpp"      // render::loadGltf, ModelData
#include "maz/render/Shapes.hpp"     // shapes::makeBox

#include <cmath>
#include <cstdio>

using namespace maz;

static int g_fail = 0;
#define CHECK(c, m)                                                                                  \
    do {                                                                                             \
        if (!(c)) {                                                                                  \
            std::printf("FAIL: %s\n", (m));                                                          \
            ++g_fail;                                                                                \
        }                                                                                            \
    } while (0)

int main() {
    const render::shapes::MeshData box =
        render::shapes::makeBox(2.0f, render::Color{0.8f, 0.4f, 0.2f, 1.0f});
    const std::size_t n = box.vertices.size();
    const std::size_t m = box.indices.size();

    const std::vector<std::uint8_t> glb = render::encodeGlb(box);
    CHECK(!glb.empty(), "encodeGlb produced bytes");
    CHECK(io::writeFile("roundtrip.glb", glb), "wrote roundtrip.glb");

    render::ModelData model;
    CHECK(render::loadGltf("roundtrip.glb", model), "cgltf re-imports the exported .glb");
    CHECK(model.mesh.vertices.size() == n, "vertex count round-trips");
    CHECK(model.mesh.indices.size() == m, "index count round-trips");
    if (!model.mesh.vertices.empty()) {
        // glTF carries COLOR_0 natively, so the baked tint survives (unlike OBJ through the loader).
        const render::MeshVertex& v = model.mesh.vertices[0];
        CHECK(std::fabs(v.r - 0.8f) < 0.02f && std::fabs(v.g - 0.4f) < 0.02f &&
                  std::fabs(v.b - 0.2f) < 0.02f,
              "vertex colour round-trips through glTF");
    }

    if (g_fail == 0) {
        std::printf("gltf_roundtrip: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
