// Maz Engine — "GRID3D" (3D editor-viewport reference demo)
// The ground grid + RGB origin gizmo every 3D editor draws (Godot's Node3D viewport): a world-space
// XZ grid so you can read scale/position, the X=red / Y=green / Z=blue axis marker at the origin, and
// a wireframe bounding box placed out in the scene. All built by render::buildGrid / buildWireBox as
// plain colored line lists and drawn through the engine's debug-line path. Fixed camera (no orbit) so
// the golden is deterministic. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GRID3D (3D reference demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 3D Grid & Gizmo";
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

    // Precompute the reference geometry ONCE (deterministic — nothing animates).
    render::GridSpec gs;
    gs.divisions = 8;
    gs.spacing = 1.0f;
    gs.axisLength = 3.0f;
    const std::vector<render::Line3> grid = render::buildGrid(gs);
    // A wireframe box sitting on the grid, one cell in from a corner.
    const std::vector<render::Line3> box =
        render::buildWireBox(math::vec3(1.5f, 0.0f, 1.5f), math::vec3(3.5f, 2.0f, 3.5f),
                             math::vec4(0.95f, 0.75f, 0.30f, 1.0f));

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 36.0f);
    }

    auto drawLines = [&](const std::vector<render::Line3>& lines) {
        for (const auto& l : lines) {
            const float a[3] = {l.a.x, l.a.y, l.a.z};
            const float b[3] = {l.b.x, l.b.y, l.b.z};
            const float c[4] = {l.color.x, l.color.y, l.color.z, l.color.w};
            renderer->drawLine(a, b, c);
        }
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            // nothing animates — fixed reference frame
        }

        // Fixed editor-style camera looking down at the grid from one corner.
        const glm::vec3 eye(9.0f, 7.0f, 11.0f);
        const glm::mat4 proj = math::perspective(glm::radians(45.0f), aspect, 0.1f, 200.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.5f, 0.5f, 0.5f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.10f, 0.11f, 0.14f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));
            drawLines(grid);
            drawLines(box);

            // 2D HUD over the 3D scene (same frame).
            render::Camera2D uicam;
            uicam.usePixelSpace = true;
            renderer->setCamera2D(uicam);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  3D GRID + GIZMO",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f, "ground grid  -  RGB axes  -  wire box",
                          render::Color{0.75f, 0.8f, 0.9f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GRID3D shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
