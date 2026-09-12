// Maz Engine — "EMITTER" (a particle emitter RESOURCE, toward Godot's CPUParticles2D)
// The old fx::ParticleSystem emits point bursts with a linear start->end colour/size. A real emitter is a
// RESOURCE with an emission SHAPE (point / disk / ring / rectangle), per-lifetime CURVES for scale & alpha,
// and a multi-stop colour GRADIENT — authored once, reused, and simulated deterministically. This demo
// stands up three emitters — a gravity FOUNTAIN, an omnidirectional BURST from a ring, and RECT rain — and
// simulates each to a fixed time with fx::simulate(), drawing every live particle at its size and gradient
// colour. Deterministic (fixed seed + fixed time) -> the render is golden-stable. Run --headless / --frames.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    if (rad < 0.4f) {
        rad = 0.4f;
    }
    const int seg = 16;
    render::Point2 p[18];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

// Build the three emitter resources. Kept in one place so the demo reads as "author resource, simulate".
fx::Emitter makeFountain() {
    fx::Emitter e;
    e.position = math::vec2(300.0f, 560.0f);
    e.count = 150;
    e.duration = 1.6f;
    e.explosiveness = 0.0f; // continuous stream
    e.lifeMin = 1.1f;
    e.lifeMax = 1.6f;
    e.shape.type = fx::EmitShape::Point;
    e.direction = math::vec2(0.0f, -1.0f); // up
    e.spread = 0.32f;
    e.speedMin = 250.0f;
    e.speedMax = 340.0f;
    e.gravity = math::vec2(0.0f, 520.0f);
    e.sizeBase = 9.0f;
    e.scale.addPoint(0.0f, 0.5f);
    e.scale.addPoint(0.2f, 1.2f);
    e.scale.addPoint(1.0f, 0.4f);
    e.color.addStop(0.0f, render::Color{1.0f, 0.95f, 0.7f, 1.0f});  // hot core
    e.color.addStop(0.5f, render::Color{1.0f, 0.55f, 0.15f, 1.0f}); // orange
    e.color.addStop(1.0f, render::Color{0.7f, 0.1f, 0.05f, 1.0f});  // ember red
    e.alpha.addPoint(0.0f, 1.0f);
    e.alpha.addPoint(0.75f, 1.0f);
    e.alpha.addPoint(1.0f, 0.0f); // fade out at the end of life
    return e;
}

fx::Emitter makeBurst() {
    fx::Emitter e;
    e.position = math::vec2(660.0f, 340.0f);
    e.count = 130;
    e.duration = 0.001f;
    e.explosiveness = 1.0f; // all at once
    e.lifeMin = 0.7f;
    e.lifeMax = 0.9f;
    e.shape.type = fx::EmitShape::Ring;
    e.shape.radius = 14.0f;
    e.shape.innerRadius = 6.0f;
    e.direction = math::vec2(0.0f, -1.0f);
    e.spread = 3.14159f; // full circle -> omnidirectional
    e.speedMin = 120.0f;
    e.speedMax = 240.0f;
    e.gravity = math::vec2(0.0f, 110.0f);
    e.sizeBase = 8.0f;
    e.scale.addPoint(0.0f, 1.1f);
    e.scale.addPoint(1.0f, 0.25f);
    e.color.addStop(0.0f, render::Color{0.85f, 1.0f, 1.0f, 1.0f}); // white-cyan
    e.color.addStop(0.5f, render::Color{0.3f, 0.7f, 1.0f, 1.0f});  // blue
    e.color.addStop(1.0f, render::Color{0.15f, 0.2f, 0.6f, 1.0f}); // deep blue
    e.alpha.addPoint(0.0f, 1.0f);
    e.alpha.addPoint(0.7f, 0.9f);
    e.alpha.addPoint(1.0f, 0.0f);
    return e;
}

fx::Emitter makeRain() {
    fx::Emitter e;
    e.position = math::vec2(1010.0f, 120.0f);
    e.count = 150;
    e.duration = 0.9f;
    e.explosiveness = 0.0f;
    e.lifeMin = 0.85f;
    e.lifeMax = 1.0f;
    e.shape.type = fx::EmitShape::Rect;
    e.shape.half = math::vec2(180.0f, 6.0f);
    e.direction = math::vec2(0.12f, 1.0f); // down, slightly angled
    e.spread = 0.04f;
    e.speedMin = 230.0f;
    e.speedMax = 300.0f;
    e.gravity = math::vec2(0.0f, 80.0f);
    e.sizeBase = 4.0f;
    e.scale.addPoint(0.0f, 1.0f);
    e.scale.addPoint(1.0f, 0.8f);
    e.color.addStop(0.0f, render::Color{0.7f, 0.85f, 1.0f, 1.0f});
    e.color.addStop(1.0f, render::Color{0.45f, 0.6f, 0.95f, 1.0f});
    e.alpha.addPoint(0.0f, 0.0f);
    e.alpha.addPoint(0.15f, 0.9f);
    e.alpha.addPoint(1.0f, 0.9f);
    return e;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("EMITTER (particle emitter resource) starting");

    // Author the resources and simulate each to a fixed time (deterministic snapshot).
    const fx::Emitter fountain = makeFountain();
    const fx::Emitter burst = makeBurst();
    const fx::Emitter rain = makeRain();
    const std::vector<fx::ParticleState> pFountain = fx::simulate(fountain, 11u, 1.6f);
    const std::vector<fx::ParticleState> pBurst = fx::simulate(burst, 23u, 0.5f);
    const std::vector<fx::ParticleState> pRain = fx::simulate(rain, 37u, 0.9f);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Particle Emitter Resource";
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

    auto drawParticles = [&](const std::vector<fx::ParticleState>& ps) {
        for (const fx::ParticleState& p : ps) {
            fillCircle(*renderer, p.pos, p.size, p.color);
        }
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.05f, 0.05f, 0.08f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PARTICLE EMITTER RESOURCE",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "emission shape + per-lifetime scale/alpha curves + colour gradient, simulated "
                          "deterministically (fx::Emitter)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.36f);

            drawParticles(pRain);     // behind
            drawParticles(pFountain);
            drawParticles(pBurst);

            char buf[96];
            std::snprintf(buf, sizeof(buf), "FOUNTAIN  (point, gravity, %d live)",
                          static_cast<int>(pFountain.size()));
            font.drawText(*renderer, 190.0f, 628.0f, buf, render::Color{1.0f, 0.7f, 0.4f, 1}, 0.34f);
            std::snprintf(buf, sizeof(buf), "BURST  (ring, omni, %d live)", static_cast<int>(pBurst.size()));
            font.drawText(*renderer, 585.0f, 628.0f, buf, render::Color{0.5f, 0.8f, 1.0f, 1}, 0.34f);
            std::snprintf(buf, sizeof(buf), "RAIN  (rect, %d live)", static_cast<int>(pRain.size()));
            font.drawText(*renderer, 930.0f, 628.0f, buf, render::Color{0.7f, 0.82f, 1.0f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("EMITTER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
