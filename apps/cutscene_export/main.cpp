// apps/cutscene_export — headless "full motion video" exporter. Loads a composed character/item
// (.mazprefab), bakes it into one mesh, orbits a camera around it over a timeline, renders each frame
// on the CPU (no GPU/window), and writes the sequence as a single looping animated GIF — and,
// optionally, as a numbered per-frame image sequence (PPM or QOI) for import into ffmpeg / a video editor.
//
//   cutscene_export <input.mazprefab> [--out file.gif] [--fps 30] [--seconds 3] [--size 256]
//                   [--frames N] [--frames-dir DIR] [--frame-format ppm|qoi]
//
// Pure CPU: reuses editor::bakeComposite + render::renderMeshPreview + anim::Timeline/FrameSequence +
// render::encodeGifAnimation (+ encodePnmP6 / encodeQoi), so it runs anywhere the engine compiles.

#include "maz/anim/FrameSequence.hpp"
#include "maz/anim/Timeline.hpp"
#include "maz/editor/Composite.hpp"
#include "maz/io/Json.hpp"         // io::readTextFile
#include "maz/io/PrefabText.hpp"   // io::loadPrefabText
#include "maz/io/Serialize.hpp"    // io::writeFile
#include "maz/math/Math.hpp"       // math::perspective (Vulkan-correct, for the GPU path)
#include "maz/math/Projection.hpp" // math::Projection (CPU rasterizer path)
#include "maz/platform/Window.hpp" // platform::Window (headless, for the GPU path)
#include "maz/render/Image.hpp"    // render::Image
#include "maz/render/ImageCodecGif.hpp"
#include "maz/render/ImageCodecPnm.hpp" // render::encodePnmP6
#include "maz/render/ImageCodecQoi.hpp" // render::encodeQoi
#include "maz/render/ObjWriter.hpp"     // render::encodeObj (--obj mesh export)
#include "maz/render/Renderer.hpp"      // render::createVulkanRenderer (GPU offscreen path)
#include "maz/render/Shapes.hpp"
#include "maz/render/Shapes3D.hpp"
#include "maz/render/SoftwareRender.hpp"

#include <glm/gtc/matrix_transform.hpp> // glm::lookAt
#include <glm/gtc/type_ptr.hpp>         // glm::value_ptr

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <system_error>
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

// Render the orbit through the REAL Vulkan PBR frame graph (sky, shadows, tonemap, …) using the
// surfaceless offscreen renderer + captureImage — the GPU counterpart to the CPU renderMeshPreview
// loop. Returns the frames, or an empty vector when no Vulkan device is available (so the caller
// falls back to the CPU rasterizer). The camera orbit matches the CPU path exactly.
std::vector<render::Image> renderFramesGpu(const render::shapes::MeshData& baked,
                                           const math::vec3& centre, float dist, float elev,
                                           float fovY, float nearZ, float farZ,
                                           const anim::Timeline& tl, const anim::FrameSequence& seq,
                                           int frameCount, int size) {
    std::vector<render::Image> frames;
    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "cutscene_export (gpu)";
    wc.headless = true; // no display; the renderer captures instead of presenting
    if (!window.init(wc)) {
        return frames;
    }
    render::RendererConfig rc;
    rc.allowHeadless = true;
    rc.offscreenWidth = static_cast<uint32_t>(size);
    rc.offscreenHeight = static_cast<uint32_t>(size);
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc) || !renderer->isActive()) {
        renderer->shutdown();
        window.shutdown();
        return frames; // no GPU device -> caller uses the CPU rasterizer
    }

    const render::MeshHandle mesh =
        renderer->createMesh(baked.vertices.data(), static_cast<uint32_t>(baked.vertices.size()),
                             baked.indices.data(), static_cast<uint32_t>(baked.indices.size()));
    const uint8_t white[4] = {255, 255, 255, 255};
    const render::TextureHandle albedo = renderer->createTexture(1, 1, white);
    const glm::mat4 proj = math::perspective(fovY, 1.0f, nearZ, farZ); // Vulkan-correct (Y-flipped)
    const glm::mat4 model(1.0f);

    frames.reserve(static_cast<std::size_t>(frameCount));
    for (int i = 0; i < frameCount; ++i) {
        const float ang = glm::radians(tl.valueAt("yaw", seq.timeAt(i)));
        const math::vec3 eye =
            centre + dist * math::vec3(std::sin(ang) * std::cos(elev), std::sin(elev),
                                       std::cos(ang) * std::cos(elev));
        const glm::mat4 view = glm::lookAt(eye, centre, math::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;
        render::Image shot;
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));
            render::Renderer::Material mat;
            mat.albedo = albedo;
            mat.roughness = 0.5f;
            mat.specular = 1.0f;
            renderer->drawMeshMaterial(mesh, glm::value_ptr(model), mat);
            renderer->endFrame();
            renderer->captureImage(shot);
        }
        frames.push_back(std::move(shot));
    }
    renderer->shutdown();
    window.shutdown();
    return frames;
}

} // namespace

int main(int argc, char** argv) {
    std::string input, out = "cutscene.gif", framesDir, frameFormat = "ppm", objOut;
    float fps = 30.0f, seconds = 3.0f;
    int size = 256, framesCap = 0, aa = 2; // aa = supersample factor (anti-aliasing)
    bool useGpu = false; // --gpu: render the real PBR frame graph offscreen instead of the CPU preview

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
        } else if (std::strcmp(a, "--frames-dir") == 0) {
            framesDir = next("");
        } else if (std::strcmp(a, "--frame-format") == 0) {
            frameFormat = next("ppm");
        } else if (std::strcmp(a, "--aa") == 0) {
            aa = std::atoi(next("2"));
        } else if (std::strcmp(a, "--gpu") == 0) {
            useGpu = true;
        } else if (std::strcmp(a, "--obj") == 0) {
            objOut = next("");
        } else if (a[0] != '-' && input.empty()) {
            input = a;
        }
    }
    if (input.empty()) {
        std::fprintf(stderr, "usage: cutscene_export <input.mazprefab> [--out f.gif] [--fps N] "
                             "[--seconds N] [--size N] [--frames N] [--frames-dir DIR] "
                             "[--frame-format ppm|qoi] [--aa 1-4] [--gpu] [--obj mesh.obj]\n");
        return 2;
    }
    const bool qoiFrames = frameFormat == "qoi"; // any other value falls back to PPM
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

    // Optional: also export the baked composite as a Wavefront OBJ (for Blender / other engines).
    if (!objOut.empty()) {
        const std::string obj = render::encodeObj(baked);
        const std::vector<std::uint8_t> objBytes(obj.begin(), obj.end());
        if (!io::writeFile(objOut, objBytes)) {
            std::fprintf(stderr, "cutscene_export: failed to write %s\n", objOut.c_str());
            return 1;
        }
        std::printf("cutscene_export: wrote %s (%zu verts, %zu tris)\n", objOut.c_str(),
                    baked.vertices.size(), baked.indices.size() / 3);
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
    const float nearZ = std::max(0.01f, dist - radius * 2.0f);
    const float farZ = dist + radius * 2.0f + 10.0f;
    const render::PreviewLighting rig = render::threePointRig();
    const render::Color bg{0.10f, 0.11f, 0.13f, 1.0f};

    // GPU path (--gpu): render the real PBR frame graph offscreen. Falls back to the CPU rasterizer
    // when no Vulkan device is available, so the tool always produces output.
    std::vector<render::Image> frames;
    bool gpu = false;
    if (useGpu) {
        frames = renderFramesGpu(baked, centre, dist, elev, fovY, nearZ, farZ, tl, seq, frameCount, size);
        gpu = !frames.empty();
        if (!gpu) {
            std::fprintf(stderr, "cutscene_export: --gpu requested but no Vulkan device available; "
                                 "falling back to the CPU rasterizer\n");
        }
    }
    if (!gpu) {
        // CPU rasterizer path (default): renderMeshPreview with the studio three-point rig.
        const math::mat4 proj =
            math::Projection::perspective(fovY, 1.0f, nearZ, farZ).m;
        frames.clear();
        frames.reserve(static_cast<std::size_t>(frameCount));
        for (int i = 0; i < frameCount; ++i) {
            const float ang = glm::radians(tl.valueAt("yaw", seq.timeAt(i)));
            const math::vec3 eye =
                centre + dist * math::vec3(std::sin(ang) * std::cos(elev), std::sin(elev),
                                           std::cos(ang) * std::cos(elev));
            const math::mat4 view = glm::lookAt(eye, centre, math::vec3(0, 1, 0));
            frames.push_back(
                render::renderMeshPreview(baked, proj * view, eye, rig, size, size, bg, aa));
        }
    }

    const int delayCentis = std::max(1, static_cast<int>(std::lround(100.0 / static_cast<double>(fps))));
    const std::vector<std::uint8_t> gif = render::encodeGifAnimation(frames, delayCentis, 0);
    if (gif.empty() || !io::writeFile(out, gif)) {
        std::fprintf(stderr, "cutscene_export: failed to write %s\n", out.c_str());
        return 1;
    }

    // Optionally also write each frame as a numbered image (frame_0000.<ext>, …) for ffmpeg / a video
    // editor. Reuses the existing PPM (P6) and QOI encoders; the directory is created if needed.
    if (!framesDir.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(framesDir, ec);
        if (ec) {
            std::fprintf(stderr, "cutscene_export: cannot create %s: %s\n", framesDir.c_str(),
                         ec.message().c_str());
            return 1;
        }
        const char* ext = qoiFrames ? "qoi" : "ppm";
        for (std::size_t i = 0; i < frames.size(); ++i) {
            char name[32];
            std::snprintf(name, sizeof(name), "frame_%04zu.%s", i, ext);
            const std::string path = (std::filesystem::path(framesDir) / name).string();
            const std::vector<std::uint8_t> bytes =
                qoiFrames ? render::encodeQoi(frames[i]) : render::encodePnmP6(frames[i]);
            if (bytes.empty() || !io::writeFile(path, bytes)) {
                std::fprintf(stderr, "cutscene_export: failed to write %s\n", path.c_str());
                return 1;
            }
        }
        std::printf("cutscene_export: wrote %zu %s frames to %s/\n", frames.size(), ext,
                    framesDir.c_str());
    }

    std::printf("cutscene_export: wrote %s (%d frames, %dx%d, %.3g s @ %.3g fps, %s)\n", out.c_str(),
                frameCount, size, size, static_cast<double>(seconds), static_cast<double>(fps),
                gpu ? "GPU" : "CPU");
    return 0;
}
