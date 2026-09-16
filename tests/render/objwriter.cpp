// tests/render/objwriter.cpp — verifies render::encodeObj: it emits the expected OBJ records, and a
// load->save->load round-trip through ObjLoader reproduces the geometry (position + uv, with the V
// flip cancelling). Pure text, headless.
#include "maz/render/ObjLoader.hpp"
#include "maz/render/ObjWriter.hpp"
#include "maz/render/Shapes.hpp"

#include <cmath>
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

static bool contains(const std::string& hay, const char* needle) {
    return hay.find(needle) != std::string::npos;
}

int main() {
    // One triangle with distinct positions, normals, colours, and uvs.
    render::shapes::MeshData tri;
    tri.vertices = {
        render::MeshVertex{-0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.9f, 0.1f, 0.2f, 0.0f, 0.25f},
        render::MeshVertex{0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.1f, 0.8f, 0.3f, 1.0f, 0.25f},
        render::MeshVertex{0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.2f, 0.3f, 0.9f, 0.5f, 0.75f},
    };
    tri.indices = {0, 1, 2};

    const std::string obj = render::encodeObj(tri);
    CHECK(contains(obj, "\nv "), "emits vertex positions");
    CHECK(contains(obj, "\nvt "), "emits texcoords");
    CHECK(contains(obj, "\nvn "), "emits normals");
    CHECK(contains(obj, "\nf 1/1/1 2/2/2 3/3/3"), "emits a triangle face with v/vt/vn indices");
    CHECK(contains(obj, "0.9 0.1 0.2"), "vertex colour extension is written on the position line");

    // load -> save -> load: the loader flips V (top-left) and the writer flips it back, so uv round-trips.
    render::shapes::MeshData back;
    CHECK(render::parseObj(obj, back), "encoded OBJ re-parses");
    CHECK(back.indices.size() == 3, "round-trip keeps one triangle (3 indices)");
    CHECK(back.vertices.size() == 3, "round-trip keeps 3 unique vertices");
    if (back.vertices.size() == 3) {
        const render::MeshVertex& v0 = back.vertices[0];
        CHECK(std::fabs(v0.px + 0.5f) < 1e-4f && std::fabs(v0.py + 0.5f) < 1e-4f,
              "first vertex position round-trips");
        CHECK(std::fabs(v0.v - 0.25f) < 1e-4f, "uv V round-trips (writer flip cancels loader flip)");
        CHECK(std::fabs(v0.nz - 1.0f) < 1e-4f, "normal round-trips");
    }

    // includeNormals=false yields v/vt faces (no //vn) and no vn records.
    render::ObjWriteOptions noNormals;
    noNormals.includeNormals = false;
    const std::string obj2 = render::encodeObj(tri, noNormals);
    CHECK(!contains(obj2, "\nvn "), "no normals emitted when disabled");
    CHECK(contains(obj2, "\nf 1/1 2/2 3/3"), "faces use v/vt when normals are disabled");

    // A real primitive exports and re-imports with a matching triangle count.
    const render::shapes::MeshData box = render::shapes::makeBox(1.0f, render::Color{1, 1, 1, 1});
    render::shapes::MeshData boxBack;
    CHECK(render::parseObj(render::encodeObj(box), boxBack), "box OBJ re-parses");
    CHECK(boxBack.indices.size() == box.indices.size(), "box triangle count round-trips");

    if (g_fail == 0) {
        std::printf("objwriter: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
