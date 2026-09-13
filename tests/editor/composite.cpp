// tests/editor/composite.cpp — verifies editor composite authoring (bakeComposite + scene<->prefab
// round-trip), all pure CPU / headless. bakeComposite takes a caller-supplied palette, so the palette
// meshes are built by hand here rather than via the GPU-bound shape generators.
#include "maz/editor/Composite.hpp"
#include "maz/io/PrefabText.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

static int g_fail = 0;
#define CHECK(c, m)                                                                                  \
    do {                                                                                             \
        if (!(c)) {                                                                                  \
            std::printf("FAIL: %s\n", (m));                                                          \
            ++g_fail;                                                                                \
        }                                                                                            \
    } while (0)

static bool near(float a, float b, float eps = 1e-5f) { return std::fabs(a - b) < eps; }

// A minimal unit quad on the XZ plane: 4 vertices, 6 indices, normals +Y, white.
static render::shapes::MeshData quad() {
    render::shapes::MeshData m;
    const float h = 0.5f;
    m.vertices = {
        {-h, 0, -h, 0, 1, 0, 1, 1, 1, 0, 0},
        {h, 0, -h, 0, 1, 0, 1, 1, 1, 1, 0},
        {h, 0, h, 0, 1, 0, 1, 1, 1, 1, 1},
        {-h, 0, h, 0, 1, 0, 1, 1, 1, 0, 1},
    };
    m.indices = {0, 1, 2, 0, 2, 3};
    return m;
}

static void bounds(const render::shapes::MeshData& m, math::vec3& lo, math::vec3& hi) {
    lo = math::vec3(1e30f);
    hi = math::vec3(-1e30f);
    for (const render::MeshVertex& v : m.vertices) {
        lo = glm::min(lo, math::vec3(v.px, v.py, v.pz));
        hi = glm::max(hi, math::vec3(v.px, v.py, v.pz));
    }
}

int main() {
    const std::vector<render::shapes::MeshData> palette = {quad(), quad()};

    // --- bakeComposite: vertex/index sums over visible, in-range nodes ---
    {
        editor::Scene s;
        editor::Node a;
        a.meshId = 0; // quad
        editor::Node b;
        b.meshId = 1;
        b.position = math::vec3(10.0f, 0.0f, 0.0f); // translated +X
        editor::Node hidden;
        hidden.meshId = 0;
        hidden.visible = false; // skipped
        editor::Node bad;
        bad.meshId = 99; // out of range -> skipped
        s.nodes = {a, b, hidden, bad};

        render::shapes::MeshData baked = editor::bakeComposite(s, palette);
        CHECK(baked.vertices.size() == 8, "baked vertex count = 2 visible quads");
        CHECK(baked.indices.size() == 12, "baked index count = 2 quads");

        math::vec3 lo, hi;
        bounds(baked, lo, hi);
        // quad a spans x[-0.5,0.5]; quad b translated +10 spans x[9.5,10.5]; union is [-0.5,10.5].
        CHECK(near(lo.x, -0.5f) && near(hi.x, 10.5f), "translated part shifts bounds");
    }

    // --- bakeComposite: optional per-part vertex tint by colorIndex ---
    {
        editor::Scene s;
        editor::Node a;
        a.meshId = 0;
        a.colorIndex = 1;
        s.nodes = {a};
        const std::vector<math::vec3> tints = {math::vec3(0, 0, 0), math::vec3(0.25f, 0.5f, 0.75f)};
        render::shapes::MeshData baked = editor::bakeComposite(s, palette, &tints);
        CHECK(!baked.vertices.empty() && near(baked.vertices[0].r, 0.25f) &&
                  near(baked.vertices[0].g, 0.5f) && near(baked.vertices[0].b, 0.75f),
              "tint applied to baked vertices");
    }

    // --- scene<->prefab in-memory round-trip over every editable field ---
    {
        editor::Scene s;
        editor::Node n;
        n.name = "Torso";
        n.meshId = 3;
        n.colorIndex = 2;
        n.position = math::vec3(1.5f, -2.0f, 3.0f);
        n.euler = math::vec3(90.0f, 0.0f, 45.0f);
        n.scale = math::vec3(2.0f, 1.0f, 0.5f);
        n.localMin = math::vec3(-0.5f, -0.75f, -0.25f);
        n.localMax = math::vec3(0.5f, 0.75f, 0.25f);
        n.emissive = math::vec3(0.25f, 0.0f, 0.0f);
        n.roughness = 0.5f;
        n.metallic = 0.25f;
        n.specular = 0.75f;
        n.visible = false;
        s.nodes = {n};

        scene::Prefab pf = editor::sceneToPrefab(s, "Hero");
        CHECK(pf.root.name == "Hero", "prefab root named after asset");
        CHECK(pf.root.children.size() == 1, "one child per node");

        editor::Scene back;
        editor::prefabToScene(pf, back);
        CHECK(back.nodes.size() == 1, "round-trip node count");
        CHECK(back.nodes.size() == 1 && back.nodes[0] == n, "round-trip all Node fields (in memory)");
        CHECK(back.selected == -1, "selection reset after load");
    }

    // --- text round-trip: sceneToPrefab -> savePrefabText -> loadPrefabText -> prefabToScene ---
    {
        editor::Scene s;
        editor::Node a;
        a.name = "Head";
        a.meshId = 1;
        a.position = math::vec3(0.0f, 1.5f, 0.0f);
        editor::Node b;
        b.name = "Body";
        b.meshId = 0;
        b.scale = math::vec3(1.0f, 2.0f, 1.0f);
        s.nodes = {a, b};

        const std::string text = io::savePrefabText(editor::sceneToPrefab(s, "Avatar"));
        scene::Prefab loaded;
        CHECK(io::loadPrefabText(text, loaded), "prefab text parses");
        editor::Scene back;
        editor::prefabToScene(loaded, back);
        CHECK(back.nodes.size() == 2, "text round-trip node count");
        CHECK(back.nodes.size() == 2 && back.nodes[0] == a && back.nodes[1] == b,
              "text round-trip preserves fields & order");
    }

    if (g_fail == 0) {
        std::printf("composite: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
