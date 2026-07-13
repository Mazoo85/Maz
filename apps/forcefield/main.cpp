// Maz Engine — "FORCEFIELD" (fx::ForceField2D composable particle force field, toward Godot's
// GPUParticlesAttractor2D family + wind). A field of 800 particles is seeded across the screen, then a
// fixed number of simulation steps is run at startup under: a strong CENTRE attractor with a tangential
// SWIRL (a vortex, drawn cyan), a REPULSOR on the right (drawn red) that carves an empty bubble, and a
// gentle leftward WIND, all bled by DRAG. The settled swarm is drawn as dots coloured by speed
// (slow = blue, fast = warm). Fixed seed + fixed step count -> deterministic, golden-stable. Run
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

void dot(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 8;
    render::Point2 pts[8];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

void ring(render::Renderer& r, math::vec2 c, float radius, float w, render::Color col) {
    const int n = 40;
    math::vec2 prev;
    for (int i = 0; i <= n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        const math::vec2 p(c.x + std::cos(a) * radius, c.y + std::sin(a) * radius);
        if (i > 0) {
            math::vec2 d = p - prev;
            const float len = std::sqrt(d.x * d.x + d.y * d.y);
            if (len > 1e-4f) {
                d /= len;
                const math::vec2 nrm(-d.y * w * 0.5f, d.x * w * 0.5f);
                const render::Point2 q[4] = {{prev.x + nrm.x, prev.y + nrm.y},
                                             {p.x + nrm.x, p.y + nrm.y},
                                             {p.x - nrm.x, p.y - nrm.y},
                                             {prev.x - nrm.x, prev.y - nrm.y}};
                r.drawConvexPolygon(q, 4, col);
            }
        }
        prev = p;
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("FORCEFIELD (fx::ForceField2D) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — ForceField2D";
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

    using math::vec2;

    // Build the field: a strong swirling centre well (Linear falloff over a wide radius so the whole
    // region orbits it into a spiral) + a repulsor that carves a clear empty bubble + gentle wind + drag.
    const vec2 wellPos(500.0f, 380.0f);
    const vec2 repelPos(950.0f, 340.0f);
    fx::ForceField2D field;
    field.wind = vec2(-10.0f, 0.0f);
    field.drag = 2.6f;
    field.addAttractor(wellPos, 380.0f, 460.0f, fx::Attractor2D::Linear, 900.0f);
    field.addAttractor(repelPos, -2600.0f, 240.0f, fx::Attractor2D::Linear);

    // Seed particles deterministically, then pre-simulate a FIXED number of steps so the frame is
    // identical every run (golden-stable).
    core::Random rng(0x5EED1234ULL);
    std::vector<fx::FieldParticle> particles;
    particles.reserve(800);
    for (int i = 0; i < 800; ++i) {
        fx::FieldParticle p;
        p.pos = vec2(rng.range(80.0f, 1200.0f), rng.range(150.0f, 620.0f));
        p.vel = vec2(rng.range(-20.0f, 20.0f), rng.range(-20.0f, 20.0f));
        particles.push_back(p);
    }
    for (int step = 0; step < 220; ++step) {
        field.step(particles, 1.0f / 60.0f, 2);
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.1f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  FORCEFIELD2D", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "attractors + swirl + wind + drag on a particle field "
                          "(fx::ForceField2D, Godot GPUParticlesAttractor2D)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // Particles, coloured by speed.
            for (const fx::FieldParticle& p : particles) {
                const float sp = std::sqrt(p.vel.x * p.vel.x + p.vel.y * p.vel.y);
                const float t = sp > 600.0f ? 1.0f : sp / 600.0f;
                const render::Color col =
                    rgba(0.35f + 0.6f * t, 0.55f + 0.2f * t, 0.95f - 0.55f * t, 0.9f);
                dot(*renderer, p.pos, 2.2f, col);
            }

            // Markers: cyan swirl well, red repulsor (with its influence radius).
            ring(*renderer, wellPos, 16.0f, 2.5f, rgba(0.4f, 0.9f, 1.0f, 1));
            dot(*renderer, wellPos, 5.0f, rgba(0.55f, 0.95f, 1.0f, 1));
            ring(*renderer, repelPos, 16.0f, 2.5f, rgba(1.0f, 0.4f, 0.4f, 1));
            ring(*renderer, repelPos, 260.0f, 1.0f, rgba(0.8f, 0.35f, 0.35f, 0.5f));
            dot(*renderer, repelPos, 5.0f, rgba(1.0f, 0.5f, 0.5f, 1));

            font.drawText(*renderer, 16.0f, 686.0f,
                          "cyan = swirling attractor   red = repulsor   dots warm->fast",
                          rgba(0.7f, 0.75f, 0.85f, 1), 0.25f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("FORCEFIELD shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
