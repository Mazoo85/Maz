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

    // Multi-part export (per-part pbrMetallicRoughness) re-imports: cgltf accepts it and merges the
    // primitives; total geometry is the sum and each part's COLOR_0 tint survives.
    {
        const render::shapes::MeshData a = render::shapes::makeBox(1.0f, render::Color{1, 1, 1, 1});
        const render::shapes::MeshData c = render::shapes::makeBox(0.5f, render::Color{1, 1, 1, 1});
        render::GlbPart p0;
        p0.mesh = &a;
        p0.baseColor[0] = 0.9f; p0.baseColor[1] = 0.2f; p0.baseColor[2] = 0.1f;
        render::GlbPart p1;
        p1.mesh = &c;
        p1.baseColor[0] = 0.15f; p1.baseColor[1] = 0.55f; p1.baseColor[2] = 0.9f;
        p1.metallic = 1.0f; p1.roughness = 0.25f;
        const std::vector<std::uint8_t> multi = render::encodeGlbParts({p0, p1});
        CHECK(io::writeFile("roundtrip_multi.glb", multi), "wrote multi-part glb");

        render::ModelData mm;
        CHECK(render::loadGltf("roundtrip_multi.glb", mm), "cgltf re-imports the multi-part .glb");
        CHECK(mm.mesh.vertices.size() == a.vertices.size() + c.vertices.size(),
              "multi-part vertex count is the sum");
        CHECK(mm.mesh.indices.size() == a.indices.size() + c.indices.size(),
              "multi-part index count is the sum");
        // First part's vertices carry its red tint; the second part's carry its blue tint.
        bool sawRed = false, sawBlue = false;
        for (const render::MeshVertex& v : mm.mesh.vertices) {
            if (v.r > 0.8f && v.b < 0.3f) sawRed = true;
            if (v.b > 0.8f && v.r < 0.3f) sawBlue = true;
        }
        CHECK(sawRed && sawBlue, "both parts' colours survive the round-trip");
    }

    if (g_fail == 0) {
        std::printf("gltf_roundtrip: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
