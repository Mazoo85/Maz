// Maz Engine — Sandbox
// The M0 walking skeleton: open a window, run a fixed-timestep loop, clear the screen to an
// animated color, and shut down cleanly. Run with --headless (or --frames N) for CI.

#include "maz/Engine.hpp"
#include "maz/assets/Model.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <string>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("Maz Engine sandbox starting (headless=%d, frames=%d)", cfg.headless, cfg.frames);

    // Blender pipeline demo: load a glTF/GLB exported from Blender and report it. The mesh
    // can't be drawn yet (the mesh renderer is a later milestone), but this proves the
    // asset path end to end. See docs/BLENDER_PIPELINE.md.
    if (cfg.modelPath != nullptr) {
        assets::Model model;
        std::string err;
        if (!assets::loadModel(cfg.modelPath, model, &err)) {
            MAZ_LOG_ERROR("failed to load model '%s': %s", cfg.modelPath, err.c_str());
            return 1;
        }
        MAZ_LOG_INFO("loaded model '%s': %zu mesh(es), %zu verts, %zu tris; bounds "
                     "min=(%.3f, %.3f, %.3f) max=(%.3f, %.3f, %.3f)",
                     cfg.modelPath, model.meshes.size(), model.vertexCount(),
                     model.triangleCount(), model.bounds.min[0], model.bounds.min[1],
                     model.bounds.min[2], model.bounds.max[0], model.bounds.max[1],
                     model.bounds.max[2]);
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = cfg.title;
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        MAZ_LOG_ERROR("window init failed");
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
#if defined(MAZ_DEBUG)
    rc.enableValidation = !cfg.headless; // no ICD under the dummy driver
#endif
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        MAZ_LOG_ERROR("renderer init failed");
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    // Simulation state advanced on the fixed step (frame-rate independent).
    float hue = 0.0f;

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (window.consumeResized()) {
            uint32_t w = 0, h = 0;
            window.drawableSize(w, h);
            renderer->onResize(w, h);
        }
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            hue += static_cast<float>(clock.fixedDelta()); // one unit ~= 1 rad/sec
        }

        render::Color clear;
        clear.r = 0.5f + 0.5f * std::sin(hue);
        clear.g = 0.5f + 0.5f * std::sin(hue + 2.094f); // +120 deg
        clear.b = 0.5f + 0.5f * std::sin(hue + 4.188f); // +240 deg
        renderer->setClearColor(clear);
        if (renderer->beginFrame()) {
            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            MAZ_LOG_INFO("reached frame cap (%d); exiting", cfg.frames);
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("shutting down after %d frames (%.2fs, renderer %s)", rendered, clock.elapsed(),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
