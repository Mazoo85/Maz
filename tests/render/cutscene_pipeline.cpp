// tests/render/cutscene_pipeline.cpp — end-to-end exercise of the cutscene export pipeline in-process
// (the same chain apps/cutscene_export drives): compose a scene -> prefab text -> reload -> bake ->
// render an orbit of CPU frames -> animated GIF -> decode. Headless, pure CPU, no file paths.
#include "maz/anim/FrameSequence.hpp"
#include "maz/anim/Timeline.hpp"
#include "maz/editor/Composite.hpp"
#include "maz/io/PrefabText.hpp"
#include "maz/math/Projection.hpp"
#include "maz/render/ImageCodecGif.hpp"
#include "maz/render/SoftwareRender.hpp"

#include <glm/gtc/matrix_transform.hpp>

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

// A unit cube centred at the origin: 8 shared vertices (outward-ish averaged normals), 12 triangles.
// Visible from any orbit angle, so the render is non-empty regardless of camera yaw.
static render::shapes::MeshData cube() {
    render::shapes::MeshData m;
    for (int i = 0; i < 8; ++i) {
        const float x = (i & 1) ? 0.5f : -0.5f;
        const float y = (i & 2) ? 0.5f : -0.5f;
        const float z = (i & 4) ? 0.5f : -0.5f;
        const float inv = 1.0f / std::sqrt(0.75f);
        m.vertices.push_back(
            render::MeshVertex{x, y, z, x * inv, y * inv, z * inv, 1, 1, 1, 0, 0});
    }
    const uint32_t f[36] = {0, 2, 1, 1, 2, 3, 4, 5, 6, 5, 7, 6, 0, 1, 4, 1, 5, 4,
                            2, 6, 3, 3, 6, 7, 0, 4, 2, 2, 4, 6, 1, 3, 5, 3, 7, 5};
    m.indices.assign(f, f + 36);
    return m;
}

static std::size_t nonBackground(const render::Image& img, const render::Color& bg) {
    std::size_t n = 0;
    for (int y = 0; y < img.height(); ++y) {
        for (int x = 0; x < img.width(); ++x) {
            const render::Color c = img.getPixel(x, y);
            if (std::fabs(c.r - bg.r) > 0.02f || std::fabs(c.g - bg.g) > 0.02f ||
                std::fabs(c.b - bg.b) > 0.02f) {
                ++n;
            }
        }
    }
    return n;
}

int main() {
    // 1) Compose a 2-part scene and round-trip it through the prefab TEXT (the load path the app uses).
    editor::Scene src;
    {
        editor::Node torso;
        torso.name = "Torso";
        torso.meshId = 0;
        torso.position = math::vec3(0, 0.5f, 0);
        editor::Node head;
        head.name = "Head";
        head.meshId = 1;
        head.position = math::vec3(0, 1.4f, 0);
        head.scale = math::vec3(0.6f);
        src.nodes = {torso, head};
    }
    const std::string text = io::savePrefabText(editor::sceneToPrefab(src, "Fig"));
    scene::Prefab prefab;
    CHECK(io::loadPrefabText(text, prefab), "prefab text reloads");
    editor::Scene scene;
    editor::prefabToScene(prefab, scene);
    CHECK(scene.nodes.size() == 2, "scene reloaded with 2 parts");

    // 2) Bake with a hand-built palette (meshId 0 and 1 both a cube here).
    const std::vector<render::shapes::MeshData> palette = {cube(), cube()};
    const render::shapes::MeshData baked = editor::bakeComposite(scene, palette);
    CHECK(baked.vertices.size() == 16 && baked.indices.size() == 72, "baked two cubes");

    // 3) Render an orbit of frames on the CPU (reusing Timeline + FrameSequence + SoftwareRender).
    const int size = 48;
    const render::Color bg{0.1f, 0.1f, 0.12f, 1.0f};
    math::vec3 lo(baked.vertices[0].px, baked.vertices[0].py, baked.vertices[0].pz), hi = lo;
    for (const render::MeshVertex& v : baked.vertices) {
        lo = glm::min(lo, math::vec3(v.px, v.py, v.pz));
        hi = glm::max(hi, math::vec3(v.px, v.py, v.pz));
    }
    const math::vec3 centre = (lo + hi) * 0.5f;
    const float radius = glm::length(hi - lo) * 0.5f;
    const float fovY = glm::radians(45.0f);
    const float dist = radius / std::sin(fovY * 0.5f) * 1.2f;
    const math::mat4 proj =
        math::Projection::perspective(fovY, 1.0f, 0.05f, dist + radius * 4.0f).m;
    const math::vec3 light = glm::normalize(math::vec3(-0.4f, -0.8f, -0.5f));

    anim::Timeline tl;
    tl.track("yaw").add(0.0f, 0.0f);
    tl.track("yaw").add(1.0f, 360.0f);
    anim::FrameSequence seq{4.0f, 1.0f}; // 4 fps over 1s -> 5 frames
    std::vector<render::Image> frames;
    std::size_t totalLit = 0;
    for (int i = 0; i < seq.count(); ++i) {
        const float ang = glm::radians(tl.valueAt("yaw", seq.timeAt(i)));
        const math::vec3 eye = centre + dist * math::vec3(std::sin(ang), 0.35f, std::cos(ang));
        const math::mat4 view = glm::lookAt(eye, centre, math::vec3(0, 1, 0));
        render::Image f = render::renderMeshPreview(baked, proj * view, light, size, size, bg);
        totalLit += nonBackground(f, bg);
        frames.push_back(std::move(f));
    }
    CHECK(frames.size() == 5, "5 frames rendered");
    CHECK(totalLit > 0, "the asset actually rendered (non-background pixels present)");

    // 4) Encode the sequence and decode it back.
    const std::vector<std::uint8_t> gif = render::encodeGifAnimation(frames, 25, 0);
    CHECK(!gif.empty(), "gif encoded");
    const render::Image decoded = render::decodeGif(gif);
    CHECK(decoded.width() == size && decoded.height() == size, "decoded gif matches frame size");

    if (g_fail == 0) {
        std::printf("cutscene_pipeline: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
