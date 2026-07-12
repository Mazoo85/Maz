// Maz Engine — Sandbox
// The M0 walking skeleton: open a window, run a fixed-timestep loop, clear the screen to an
// animated color, and shut down cleanly. Run with --headless (or --frames N) for CI.

#include "maz/Engine.hpp"
#include "maz/assets/Model.hpp"
#include "maz/scene/Camera.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <string>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("Maz Engine sandbox starting (headless=%d, frames=%d)", cfg.headless, cfg.frames);

    // Blender pipeline: load a glTF/GLB exported from Blender. On a real GPU it's drawn below as
    // a spinning, lit mesh; headless (CI/tests) we just report it. See docs/BLENDER_PIPELINE.md.
    assets::Model model;
    bool haveModel = false;
    if (cfg.modelPath != nullptr) {
        std::string err;
        if (!assets::loadModel(cfg.modelPath, model, &err)) {
            MAZ_LOG_ERROR("failed to load model '%s': %s", cfg.modelPath, err.c_str());
            return 1;
        }
        haveModel = true;
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

    // Hand the loaded model to the GPU. Returns false headless / with no mesh pipeline, in which
    // case drawModel() below simply no-ops and we still run the clear loop.
    const bool drawMesh = haveModel && renderer->uploadModel(model);

    // Camera looking at the origin from slightly above and back.
    scene::Camera camera;
    camera.setPosition({0.0f, 1.4f, 3.0f});
    camera.setTarget({0.0f, 0.0f, 0.0f});

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    // Simulation state advanced on the fixed step (frame-rate independent).
    float hue = 0.0f;
    float spin = 0.0f;

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
            const float dt = static_cast<float>(clock.fixedDelta());
            hue += dt;              // one unit ~= 1 rad/sec
            spin += dt * 0.9f;      // model rotation
        }

        // Keep the projection matched to the current drawable aspect ratio.
        uint32_t dw = 0, dh = 0;
        window.drawableSize(dw, dh);
        const float aspect = dh > 0 ? static_cast<float>(dw) / static_cast<float>(dh) : 1.0f;
        camera.setPerspective(glm::radians(60.0f), aspect, 0.1f, 100.0f);

        // Spin the model about a tilted axis.
        const math::mat4 modelMatrix =
            glm::rotate(math::mat4(1.0f), spin, math::normalize(math::vec3(0.3f, 1.0f, 0.15f)));
        const math::mat4 mvp = camera.viewProjection() * modelMatrix;

        render::Color clear;
        clear.r = 0.5f + 0.5f * std::sin(hue);
        clear.g = 0.5f + 0.5f * std::sin(hue + 2.094f); // +120 deg
        clear.b = 0.5f + 0.5f * std::sin(hue + 4.188f); // +240 deg
        renderer->setClearColor(clear);
        if (renderer->beginFrame()) {
            if (drawMesh) {
                renderer->drawModel(mvp, modelMatrix);
            }
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
