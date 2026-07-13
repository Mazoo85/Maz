// Maz Engine — "MULTIMESH" (2D multi-mesh instancing, toward Godot's MultiMeshInstance2D)
// One base shape (a small dart) is stamped hundreds of times through a single render::MultiMesh2D — each
// instance carries only its position, rotation, scale, and colour. The instances form a swirl field: every
// dart points along a spiral and is tinted by its angle, so the whole grid reads as one flowing pattern
// drawn from one shape + a compact instance buffer. All instance data is a deterministic function of the
// grid index → golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

// A simple angle -> RGB palette (three phase-shifted cosines), so hue sweeps smoothly with rotation.
render::Color hue(float t) {
    const float k = 6.2831853f;
    return render::Color{0.5f + 0.5f * std::cos(k * t),
                         0.5f + 0.5f * std::cos(k * (t + 0.33f)),
                         0.5f + 0.5f * std::cos(k * (t + 0.66f)), 1.0f};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MULTIMESH (2D instancing) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — MultiMesh2D";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // One base shape (a small dart pointing +X), reused by every instance.
    render::MultiMesh2D mm;
    mm.baseVertices = {math::vec2(-7.0f, -6.0f), math::vec2(13.0f, 0.0f), math::vec2(-7.0f, 6.0f),
                       math::vec2(-3.0f, 0.0f)};

    // Populate a swirl field: instances on a grid, each rotated along a spiral about the centre and tinted by
    // its angle. Every value is a pure function of the grid index → deterministic.
    const int cols = 30, rows = 18;
    const float cx = 640.0f, cy = 384.0f;
    const float spacing = 40.0f;
    for (int gy = 0; gy < rows; ++gy) {
        for (int gx = 0; gx < cols; ++gx) {
            const float px = cx + (static_cast<float>(gx) - (cols - 1) * 0.5f) * spacing;
            const float py = cy + (static_cast<float>(gy) - (rows - 1) * 0.5f) * spacing + 40.0f;
            const float dx = px - cx;
            const float dy = py - (cy + 40.0f);
            const float dist = std::sqrt(dx * dx + dy * dy);
            const float baseAngle = std::atan2(dy, dx);
            const float angle = baseAngle + 1.5707963f + dist * 0.0035f; // tangential + spiral twist
            render::Instance2D inst;
            inst.position = math::vec2(px, py);
            inst.rotation = angle;
            const float s = 0.7f + 0.5f * std::sin(dist * 0.02f);
            inst.scale = math::vec2(s, s);
            inst.color = hue(baseAngle / 6.2831853f + 0.5f);
            mm.addInstance(inst);
        }
    }

    const render::Color kBg{0.06f, 0.07f, 0.10f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.6f, 0.66f, 0.78f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Draw every instance's transformed polygon with its own colour.
            for (std::size_t i = 0; i < mm.instanceCount(); ++i) {
                const std::vector<math::vec2> poly = mm.transformedPolygon(i);
                render::Point2 pts[8];
                const std::size_t n = poly.size() < 8 ? poly.size() : 8;
                for (std::size_t k = 0; k < n; ++k) {
                    pts[k] = {poly[k].x, poly[k].y};
                }
                renderer->drawConvexPolygon(pts, static_cast<uint32_t>(n), mm.instance(i).color);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  MULTIMESH2D (2D instancing)", kText, 0.55f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "one base shape stamped many times via render::MultiMesh2D (per-instance "
                          "pos/rot/scale/colour)",
                          kDim, 0.3f);

            char info[128];
            std::snprintf(info, sizeof(info), "%zu instances  ->  %zu triangles from 1 base dart",
                          mm.instanceCount(), mm.triangleCount());
            font.drawText(*renderer, 16.0f, 694.0f, info, render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MULTIMESH shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
