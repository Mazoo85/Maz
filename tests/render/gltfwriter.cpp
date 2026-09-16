// tests/render/gltfwriter.cpp — verifies render::encodeGlb produces a well-formed binary glTF: the
// GLB container re-parses (parseGlb), the BIN blob is the expected size (POSITION+NORMAL+COLOR_0+
// indices), and the JSON declares the four accessors / one mesh. Pure bytes, headless — the strict
// cgltf re-import round-trip is a separate CI test (gltf_roundtrip).
#include "maz/render/GlbContainer.hpp"
#include "maz/render/GltfWriter.hpp"
#include "maz/render/Shapes.hpp"

#include <cstdio>
#include <string>

using namespace maz;

static int g_fail = 0;
#define CHECK(c, m)                                                                                  \
    do {                                                                                             \
        if (!(c)) {                                                                                  \
            std::printf("FAIL: %s\n", (m));                                                          \
            ++g_fail;                                                                                \
        }                                                                                            \
    } while (0)

static bool has(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

int main() {
    const render::shapes::MeshData box = render::shapes::makeBox(1.0f, render::Color{0.8f, 0.4f, 0.2f, 1});
    const std::size_t n = box.vertices.size();
    const std::size_t m = box.indices.size();

    const std::vector<std::uint8_t> glb = render::encodeGlb(box);
    CHECK(!glb.empty(), "encodeGlb produced bytes");
    CHECK(glb.size() % 4 == 0, "GLB is 4-byte aligned");

    render::GlbChunks chunks;
    CHECK(render::parseGlb(glb, chunks), "emitted GLB re-parses");
    CHECK(chunks.version == 2, "glTF 2.0");
    // BIN = positions(n*12) + normals(n*12) + colors(n*16) + indices(m*4).
    CHECK(chunks.bin.size() == n * 40u + m * 4u, "BIN blob is the expected size");

    CHECK(has(chunks.json, "\"version\":\"2.0\""), "asset version present");
    CHECK(has(chunks.json, "\"POSITION\":0") && has(chunks.json, "\"NORMAL\":1") &&
              has(chunks.json, "\"COLOR_0\":2"),
          "primitive attributes wired to accessors");
    CHECK(has(chunks.json, "\"componentType\":5125"), "indices are UNSIGNED_INT");
    CHECK(has(chunks.json, "\"min\":[") && has(chunks.json, "\"max\":["),
          "POSITION accessor carries min/max");
    CHECK(has(chunks.json, "\"target\":34963"), "index buffer view targets ELEMENT_ARRAY_BUFFER");

    // Empty / degenerate mesh yields no bytes.
    CHECK(render::encodeGlb(render::shapes::MeshData{}).empty(), "empty mesh -> empty GLB");

    if (g_fail == 0) {
        std::printf("gltfwriter: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
