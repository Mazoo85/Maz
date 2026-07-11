// Maz Engine — "WORLD" (explorable 3D demo)
// A textured ground and a field of 3D blocks you fly through with a first-person camera
// (WASD move, Space/Shift up-down, mouse look). Shows interactive 3D navigation via
// maz::game::FlyCamera. --demo autopilots a fly-through (for capture); --headless / --frames N
// for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

using namespace maz;

namespace {

struct Block {
    glm::vec3 pos;
    glm::vec3 scale;
    render::MeshHandle mesh;
};

std::vector<uint8_t> makeChecker(uint32_t size, uint32_t cell, uint8_t a, uint8_t b) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4, 255);
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const bool on = ((x / cell) + (y / cell)) % 2 == 0;
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            const uint8_t c = on ? a : b;
            px[i + 0] = c;
            px[i + 1] = c;
            px[i + 2] = c;
        }
    }
    return px;
}

render::MeshHandle upload(render::Renderer& r, const render::shapes::MeshData& m) {
    return r.createMesh(m.vertices.data(), static_cast<uint32_t>(m.vertices.size()),
                        m.indices.data(), static_cast<uint32_t>(m.indices.size()));
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    bool startWireframe = false;  // --wireframe starts in wireframe debug draw (also F4 at runtime)
    bool startColliders = false;  // --colliders starts with the collider overlay on (also F5)
    bool shakeTest = false;       // --shaketest pins camera shake on (for verification)
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--wireframe") == 0) startWireframe = true;
        if (std::strcmp(argv[i], "--colliders") == 0) startColliders = true;
        if (std::strcmp(argv[i], "--shaketest") == 0) shakeTest = true;
    }
    MAZ_LOG_INFO("WORLD (explorable 3D) starting (autopilot=%d)", autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 3D World";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }
    if (!cfg.headless && !autopilot) {
        window.setRelativeMouse(true); // mouse-look
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

    namespace sh = render::shapes;
    render::MeshHandle groundMesh = upload(*renderer, sh::makePlane(40.0f, render::Color{0.5f, 0.55f, 0.6f, 1}));
    render::TextureHandle groundTex = renderer->createTexture(64, 64, makeChecker(64, 32, 210, 150).data());
    const uint8_t whitePixel[4] = {255, 255, 255, 255};
    render::TextureHandle whiteTex = renderer->createTexture(1, 1, whitePixel);

    const render::Color palette[] = {
        {0.80f, 0.45f, 0.45f, 1}, {0.45f, 0.70f, 0.55f, 1}, {0.45f, 0.55f, 0.80f, 1},
        {0.80f, 0.70f, 0.45f, 1}, {0.65f, 0.55f, 0.75f, 1},
    };
    std::vector<render::MeshHandle> blockMeshes;
    for (const render::Color& c : palette) {
        blockMeshes.push_back(upload(*renderer, sh::makeBox(1.0f, c)));
    }

    // A field of blocks on a grid, with random heights and some gaps to walk between.
    std::mt19937 rng(2024u);
    auto frand = [&](float lo, float hi) {
        return std::uniform_real_distribution<float>(lo, hi)(rng);
    };
    std::vector<Block> blocks;
    for (int gx = -5; gx <= 5; ++gx) {
        for (int gz = -5; gz <= 5; ++gz) {
            if (frand(0.0f, 1.0f) < 0.35f) {
                continue; // leave a gap
            }
            const float h = frand(1.5f, 7.0f);
            Block b;
            b.pos = glm::vec3(static_cast<float>(gx) * 6.0f + frand(-1.0f, 1.0f), h * 0.5f,
                              static_cast<float>(gz) * 6.0f + frand(-1.0f, 1.0f));
            b.scale = glm::vec3(frand(2.0f, 3.0f), h, frand(2.0f, 3.0f));
            b.mesh = blockMeshes[static_cast<size_t>((gx * 7 + gz * 3 + 100)) % blockMeshes.size()];
            blocks.push_back(b);
        }
    }

    // Solid boxes for collision, and pickups to collect in the open spaces between them.
    std::vector<game::Aabb> solids;
    for (const Block& b : blocks) {
        solids.push_back(game::Aabb::fromCenterSize(b.pos, b.scale));
    }
    render::MeshHandle pickupMesh =
        upload(*renderer, sh::makeSphere(0.6f, 14, 20, render::Color{1.0f, 0.92f, 0.35f, 1}));
    const int kPickups = 10;
    std::vector<glm::vec3> pickups;
    std::vector<bool> collected(static_cast<size_t>(kPickups), false);
    for (int i = 0; i < kPickups; ++i) {
        glm::vec3 chosen(0.0f, 2.5f, 0.0f);
        for (int tries = 0; tries < 60; ++tries) {
            const glm::vec3 p(frand(-28.0f, 28.0f), 2.5f, frand(-28.0f, 28.0f));
            const game::Aabb probe = game::Aabb::fromCenterSize(p, glm::vec3(1.8f));
            bool clear = true;
            for (const game::Aabb& s : solids) {
                if (probe.overlaps(s)) {
                    clear = false;
                    break;
                }
            }
            if (clear) {
                chosen = p;
                break;
            }
        }
        pickups.push_back(chosen);
    }
    int collectedCount = 0;
    const glm::vec3 playerHalf(0.7f, 1.2f, 0.7f);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // Daytime lighting (defaults) plus distance fog fading the far blocks into the sky horizon.
    render::SceneLighting lighting;
    lighting.fogColor[0] = 0.72f;
    lighting.fogColor[1] = 0.82f;
    lighting.fogColor[2] = 0.95f;
    lighting.fogDensity = 0.018f;
    renderer->setLighting(lighting);
    renderer->setBloom(0.35f, 0.75f); // soft glow on the emissive gold pickups

    game::FlyCamera camera;
    camera.setPosition(glm::vec3(0.0f, 3.0f, 24.0f));
    camera.setYawPitch(-1.5708f, -0.12f);
    float avoidYaw = 0.0f; // autopilot: steer around blocks it bumps into

    ui::DebugOverlay overlay;
    overlay.setEnabled(true); // on by default here; toggle with F3

    bool wireframe = startWireframe; // F4 toggles wireframe debug draw (--wireframe starts on)
    renderer->setWireframe(wireframe);
    bool showColliders = startColliders; // F5 draws the collision AABBs as debug lines
    game::Shake shake; // camera juice — a jolt each time a pickup is collected

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }
        if (input.keyPressed(SDL_SCANCODE_F3)) {
            overlay.toggle();
        }
        if (input.keyPressed(SDL_SCANCODE_F4)) {
            wireframe = !wireframe;
            renderer->setWireframe(wireframe);
        }
        if (input.keyPressed(SDL_SCANCODE_F5)) {
            showColliders = !showColliders;
        }
        overlay.update(clock.frameDelta());
        shake.update(static_cast<float>(clock.frameDelta()));
        if (shakeTest) {
            shake.addTrauma(1.0f); // hold max trauma so the camera visibly shakes for verification
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        // Facing: autopilot turns toward the nearest uncollected pickup; else mouse-look.
        if (autopilot) {
            int target = -1;
            float best = 1e18f;
            for (size_t i = 0; i < pickups.size(); ++i) {
                if (collected[i]) {
                    continue;
                }
                const float dx = pickups[i].x - camera.position().x;
                const float dz = pickups[i].z - camera.position().z;
                const float d = dx * dx + dz * dz;
                if (d < best) {
                    best = d;
                    target = static_cast<int>(i);
                }
            }
            if (target >= 0) {
                const glm::vec3 dir = pickups[static_cast<size_t>(target)] - camera.position();
                camera.setYawPitch(std::atan2(dir.z, dir.x) + avoidYaw, -0.05f);
            } else {
                camera.look(0.004f, 0.0f);
            }
        } else {
            const float sens = 0.0025f;
            camera.look(input.mouseDX() * sens, -input.mouseDY() * sens);
            const float padLook = 0.05f;
            camera.look(input.gamepadAxis(platform::pad::RightX) * padLook,
                        -input.gamepadAxis(platform::pad::RightY) * padLook);
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            float fwd = 0.0f, right = 0.0f, up = 0.0f, speed = 9.0f;
            if (autopilot) {
                fwd = 1.0f;
                speed = 5.5f;
            } else {
                if (input.keyDown(SDL_SCANCODE_W)) fwd += 1.0f;
                if (input.keyDown(SDL_SCANCODE_S)) fwd -= 1.0f;
                if (input.keyDown(SDL_SCANCODE_D)) right += 1.0f;
                if (input.keyDown(SDL_SCANCODE_A)) right -= 1.0f;
                if (input.keyDown(SDL_SCANCODE_SPACE)) up += 1.0f;
                if (input.keyDown(SDL_SCANCODE_LSHIFT)) up -= 1.0f;
                // Gamepad: left stick moves, triggers rise/descend.
                fwd -= input.gamepadAxis(platform::pad::LeftY);
                right += input.gamepadAxis(platform::pad::LeftX);
                up += input.gamepadAxis(platform::pad::RightTrigger) -
                      input.gamepadAxis(platform::pad::LeftTrigger);
            }
            // Move, resolved against the solid blocks so you slide instead of passing through.
            const glm::vec3 delta = camera.moveDelta(fwd, right, up, dt, speed);
            const glm::vec3 before = camera.position();
            camera.setPosition(game::slideMove(before, delta, playerHalf, solids));

            if (autopilot) {
                // If a block blocked most of the intended move, accumulate a turn to steer round.
                const float intended = glm::length(delta);
                const float moved = glm::length(camera.position() - before);
                if (intended > 1e-4f && moved < 0.4f * intended) {
                    avoidYaw += 0.12f;
                } else {
                    avoidYaw *= 0.85f;
                }
            }

            for (size_t i = 0; i < pickups.size(); ++i) {
                if (collected[i]) {
                    continue;
                }
                const glm::vec3 d = pickups[i] - camera.position();
                if (glm::dot(d, d) < 2.6f * 2.6f) {
                    collected[i] = true;
                    ++collectedCount;
                    shake.addTrauma(0.6f); // jolt the camera on pickup
                }
            }
        }

        const glm::mat4 proj = math::perspective(glm::radians(60.0f), aspect, 0.1f, 200.0f);
        // Apply camera shake by temporarily offsetting the eye, then restore so gameplay/collision
        // see the true position.
        const glm::vec3 basePos = camera.position();
        camera.setPosition(basePos + shake.offset(static_cast<float>(clock.elapsed()), 0.6f));
        const glm::mat4 viewProj = proj * camera.view();
        camera.setPosition(basePos);

        renderer->setClearColor(render::Color{0.55f, 0.68f, 0.85f, 1.0f}); // sky
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(camera.position()));

            renderer->drawMesh(groundMesh, glm::value_ptr(glm::mat4(1.0f)), groundTex);
            for (const Block& b : blocks) {
                glm::mat4 model = glm::translate(glm::mat4(1.0f), b.pos);
                model = glm::scale(model, b.scale);
                renderer->drawMesh(b.mesh, glm::value_ptr(model), whiteTex);
            }

            // F5: overlay each collision box as green debug lines (verifies colliders match geometry).
            if (showColliders) {
                const float green[4] = {0.2f, 1.0f, 0.35f, 0.9f};
                for (const game::Aabb& s : solids) {
                    renderer->drawAabb(glm::value_ptr(s.min), glm::value_ptr(s.max), green);
                }
            }

            const float bob = 0.2f * std::sin(static_cast<float>(clock.elapsed()) * 2.0f);
            // Emissive gold that pulses — self-illuminated so the pickups glow and bloom.
            const float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(clock.elapsed()) * 3.0f);
            const float glow[3] = {1.15f * pulse, 0.85f * pulse, 0.20f * pulse};
            for (size_t i = 0; i < pickups.size(); ++i) {
                if (collected[i]) {
                    continue;
                }
                glm::vec3 p = pickups[i];
                p.y += bob;
                renderer->drawMeshEmissive(pickupMesh,
                                           glm::value_ptr(glm::translate(glm::mat4(1.0f), p)),
                                           whiteTex, render::kInvalidTexture, glow);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  3D WORLD",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 44.0f,
                          autopilot ? "AUTOPILOT" : "WASD move   mouse look   SPACE/SHIFT up-down",
                          render::Color{0.9f, 0.95f, 1.0f, 1}, 0.5f);
            char buf[48];
            if (collectedCount >= kPickups) {
                std::snprintf(buf, sizeof(buf), "ALL COLLECTED!");
            } else {
                std::snprintf(buf, sizeof(buf), "COLLECTED  %d / %d", collectedCount, kPickups);
            }
            font.drawText(*renderer, 16.0f, 74.0f, buf, render::Color{1, 0.95f, 0.5f, 1}, 0.6f);

            // Profiling overlay (F3): FPS / frame time / draw counts.
            overlay.draw(*renderer, font, 16.0f, static_cast<float>(bh) - 66.0f, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("WORLD shutting down (%zu blocks, renderer %s)", blocks.size(),
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
