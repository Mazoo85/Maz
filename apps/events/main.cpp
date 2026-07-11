// Maz Engine — "EVENTS" (event-bus / pub-sub demo)
// A single emitter fires an ImpactEvent on a timer; three INDEPENDENT subscribers react to it
// through maz::core::EventBus without any of them referencing each other: a particle system bursts,
// a scorekeeper tallies energy, and a ripple system spawns an expanding ring. The HUD shows each
// subscriber's own counter, proving the decoupling. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

struct ImpactEvent {
    float x, y;
    render::Color color;
};

render::TextureHandle softDot(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d);
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(size) +
                              static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

render::TextureHandle ringTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            // Bright annulus peaking near d=0.8, transparent at the center and outside.
            const float a = d > 1.0f ? 0.0f : std::exp(-((d - 0.8f) * (d - 0.8f)) * 40.0f);
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(size) +
                              static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("EVENTS (event-bus demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Event Bus";
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

    render::TextureHandle dotTex = softDot(*renderer, 32);
    render::TextureHandle ring = ringTex(*renderer, 64);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // --- The event bus and its three independent subscribers -----------------------------------
    core::EventBus bus;

    // Subscriber 1: particle bursts.
    fx::ParticleSystem particles(4096);
    bus.subscribe<ImpactEvent>([&](const ImpactEvent& e) {
        fx::BurstDesc b;
        b.count = 40;
        b.x = e.x;
        b.y = e.y;
        b.speedMin = 60.0f;
        b.speedMax = 260.0f;
        b.lifeMin = 0.4f;
        b.lifeMax = 1.0f;
        b.sizeStart = 14.0f;
        b.sizeEnd = 1.0f;
        b.colorStart = e.color;
        b.colorEnd = render::Color{e.color.r, e.color.g, e.color.b, 0.0f};
        b.gravity = 120.0f;
        b.drag = 0.6f;
        particles.emit(b);
    });

    // Subscriber 2: scorekeeper (its own state, knows nothing about particles).
    int eventCount = 0;
    float energy = 0.0f;
    bus.subscribe<ImpactEvent>([&](const ImpactEvent& e) {
        ++eventCount;
        energy += 10.0f + (e.color.r + e.color.g + e.color.b) * 5.0f;
    });

    // Subscriber 3: expanding rings.
    struct Ring {
        float x, y, age, maxAge;
        render::Color color;
    };
    std::vector<Ring> rings;
    bus.subscribe<ImpactEvent>(
        [&](const ImpactEvent& e) { rings.push_back({e.x, e.y, 0.0f, 0.9f, e.color}); });

    const render::Color palette[6] = {{1.0f, 0.5f, 0.4f, 1}, {0.5f, 0.9f, 0.6f, 1},
                                      {0.5f, 0.7f, 1.0f, 1}, {1.0f, 0.85f, 0.4f, 1},
                                      {0.85f, 0.5f, 1.0f, 1}, {0.4f, 0.95f, 0.95f, 1}};
    float fireTimer = 0.0f;
    int nextColor = 0;
    float t = 0.0f;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
        const float sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            t += dt;
            fireTimer += dt;
            // The ONE emitter: fire an impact at a scripted position every 0.35s.
            if (fireTimer >= 0.35f) {
                fireTimer -= 0.35f;
                ImpactEvent e;
                e.x = sw * (0.5f + 0.34f * std::sin(t * 1.7f));
                e.y = sh * (0.5f + 0.30f * std::sin(t * 2.3f + 1.0f));
                e.color = palette[nextColor % 6];
                nextColor++;
                bus.emit(e); // all three subscribers react, none knows about the others
            }
            particles.update(dt);
            for (Ring& r : rings) {
                r.age += dt;
            }
            rings.erase(std::remove_if(rings.begin(), rings.end(),
                                       [](const Ring& r) { return r.age >= r.maxAge; }),
                        rings.end());
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Rings (subscriber 3).
            for (const Ring& r : rings) {
                const float tt = r.age / r.maxAge;
                const float size = 30.0f + tt * 220.0f;
                render::SpriteDesc s;
                s.x = r.x - size * 0.5f;
                s.y = r.y - size * 0.5f;
                s.width = size;
                s.height = size;
                s.color = render::Color{r.color.r, r.color.g, r.color.b, (1.0f - tt) * 0.8f};
                renderer->drawSprite(ring, s);
            }

            // Particles (subscriber 1).
            particles.draw(*renderer, dotTex);

            // HUD showing each subscriber's independent counter.
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  EVENT BUS (PUB/SUB)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "1 emitter -> 3 subscribers   |   events:%d   particles:%u   energy:%d",
                          eventCount, particles.alive(), static_cast<int>(energy));
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("EVENTS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
