// apps/cutscene_export — headless "full motion video" exporter. Loads a composed character/item
// (.mazprefab), bakes it into one mesh, orbits a camera around it over a timeline, renders each frame
// on the CPU (no GPU/window), and writes the sequence as a single looping animated GIF.
//
//   cutscene_export <input.mazprefab> [--out file.gif] [--fps 30] [--seconds 3]
//                   [--size 256] [--frames N]
//
// Pure CPU: reuses editor::bakeComposite + render::renderMeshPreview + anim::Timeline/FrameSequence +
// render::encodeGifAnimation, so it runs anywhere the engine compiles (including CI, with no display).

#include "maz/anim/FrameSequence.hpp"
#include "maz/anim/Timeline.hpp"
#include "maz/editor/Composite.hpp"
#include "maz/io/Json.hpp"         // io::readTextFile
#include "maz/io/PrefabText.hpp"   // io::loadPrefabText
#include "maz/io/Serialize.hpp"    // io::writeFile
#include "maz/math/Projection.hpp" // math::Projection
#include "maz/render/ImageCodecGif.hpp"
#include "maz/render/Shapes.hpp"
#include "maz/render/Shapes3D.hpp"
#include "maz/render/SoftwareRender.hpp"

#include <glm/gtc/matrix_transform.hpp> // glm::lookAt

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace maz;

namespace {

// The same primitive palette the editor uploads (index order must match editor Node::meshId).
std::vector<render::shapes::MeshData> buildPalette() {
    namespace sh = render::shapes;
    const render::Color white{1, 1, 1, 1};
    return {
        sh::makeBox(1.0f, white),             // 0 box
        sh::makeSphere(0.5f, 32, 40, white),  // 1 sphere
        sh::makeCylinder(0.5f, 1.0f, 32, white), // 2 cylinder
        sh::makeCone(0.5f, 1.0f, 32, white),     // 3 cone
        sh::makeTorus(0.5f, 0.2f, 32, 20, white),// 4 torus
        sh::makeCapsule(0.35f, 0.6f, 24, 8, white), // 5 capsule
    };
}

// The editor's swatch palette as RGB, so baked parts keep their colours (matches apps/editor/main.cpp).
std::vector<math::vec3> swatchRgb() {
    return {math::vec3(210.0f, 90.0f, 80.0f) / 255.0f,   math::vec3(90.0f, 170.0f, 220.0f) / 255.0f,
            math::vec3(120.0f, 200.0f, 120.0f) / 255.0f, math::vec3(225.0f, 200.0f, 110.0f) / 255.0f,
            math::vec3(210.0f, 210.0f, 215.0f) / 255.0f};
}

} // namespace

int main(int argc, char** argv) {
    std::string input, out = "cutscene.gif";
    float fps = 30.0f, seconds = 3.0f;
    int size = 256, framesCap = 0;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        auto next = [&](const char* def) -> const char* { return i + 1 < argc ? argv[++i] : def; };
        if (std::strcmp(a, "--out") == 0) {
            out = next(out.c_str());
        } else if (std::strcmp(a, "--fps") == 0) {
            fps = static_cast<float>(std::atof(next("30")));
        } else if (std::strcmp(a, "--seconds") == 0) {
            seconds = static_cast<float>(std::atof(next("3")));
        } else if (std::strcmp(a, "--size") == 0) {
            size = std::atoi(next("256"));
        } else if (std::strcmp(a, "--frames") == 0) {
            framesCap = std::atoi(next("0"));
        } else if (a[0] != '-' && input.empty()) {
            input = a;
        }
    }
    if (input.empty()) {
        std::fprintf(stderr, "usage: cutscene_export <input.mazprefab> [--out f.gif] [--fps N] "
                             "[--seconds N] [--size N] [--frames N]\n");
        return 2;
    }
    if (size < 1) {
        size = 256;
    }
    if (fps <= 0.0f) {
        fps = 30.0f;
    }

    // Load + bake the asset.
    std::string text;
    if (!io::readTextFile(input, text)) {
        std::fprintf(stderr, "cutscene_export: cannot read %s\n", input.c_str());
        return 1;
    }
    scene::Prefab prefab;
    if (!io::loadPrefabText(text, prefab)) {
        std::fprintf(stderr, "cutscene_export: %s is not a valid prefab\n", input.c_str());
        return 1;
    }
    editor::Scene scene;
    editor::prefabToScene(prefab, scene);
    const std::vector<render::shapes::MeshData> palette = buildPalette();
    const std::vector<math::vec3> swatches = swatchRgb();
    const render::shapes::MeshData baked = editor::bakeComposite(scene, palette, &swatches);
    if (baked.vertices.empty()) {
        std::fprintf(stderr, "cutscene_export: %s baked to an empty mesh (no visible parts?)\n",
                     input.c_str());
        return 1;
    }

    // Bounds -> a centre and radius to frame the orbit camera.
    math::vec3 lo(baked.vertices[0].px, baked.vertices[0].py, baked.vertices[0].pz), hi = lo;
    for (const render::MeshVertex& v : baked.vertices) {
        lo = glm::min(lo, math::vec3(v.px, v.py, v.pz));
        hi = glm::max(hi, math::vec3(v.px, v.py, v.pz));
    }
    const math::vec3 centre = (lo + hi) * 0.5f;
    float radius = glm::length(hi - lo) * 0.5f;
    if (radius < 1e-4f) {
        radius = 1.0f;
    }

    // A yaw track 0->360 over the clip drives the orbit (reuses the keyframe Timeline + FrameSequence).
    anim::Timeline tl;
    tl.track("yaw").add(0.0f, 0.0f);
    tl.track("yaw").add(seconds > 0.0f ? seconds : 1.0f, 360.0f);
    anim::FrameSequence seq{fps, seconds};
    int frameCount = seq.count();
    if (framesCap > 0 && frameCount > framesCap) {
        frameCount = framesCap;
    }

    const float fovY = glm::radians(45.0f);
    const float dist = radius / std::sin(fovY * 0.5f) * 1.15f; // fit the bounds sphere with margin
    const float elev = glm::radians(20.0f);
    const math::vec3 light = glm::normalize(math::vec3(-0.4f, -0.8f, -0.5f));
    const render::Color bg{0.10f, 0.11f, 0.13f, 1.0f};
    const math::mat4 proj = math::Projection::perspective(fovY, 1.0f, std::max(0.01f, dist - radius * 2.0f),
                                                          dist + radius * 2.0f + 10.0f)
                                .m;

    std::vector<render::Image> frames;
    frames.reserve(static_cast<std::size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i) {
        const float ang = glm::radians(tl.valueAt("yaw", seq.timeAt(i)));
        const math::vec3 eye =
            centre + dist * math::vec3(std::sin(ang) * std::cos(elev), std::sin(elev),
                                       std::cos(ang) * std::cos(elev));
        const math::mat4 view = glm::lookAt(eye, centre, math::vec3(0, 1, 0));
        frames.push_back(render::renderMeshPreview(baked, proj * view, light, size, size, bg));
    }

    const int delayCentis = std::max(1, static_cast<int>(std::lround(100.0 / static_cast<double>(fps))));
    const std::vector<std::uint8_t> gif = render::encodeGifAnimation(frames, delayCentis, 0);
    if (gif.empty() || !io::writeFile(out, gif)) {
        std::fprintf(stderr, "cutscene_export: failed to write %s\n", out.c_str());
        return 1;
    }
    std::printf("cutscene_export: wrote %s (%d frames, %dx%d, %.3g s @ %.3g fps)\n", out.c_str(),
                frameCount, size, size, static_cast<double>(seconds), static_cast<double>(fps));
    return 0;
}
