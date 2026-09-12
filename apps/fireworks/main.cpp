// Maz Engine — "FIREWORKS" (time scheduler + sequence demo)
// The whole show is driven by core::Scheduler timers: every 0.4s a rocket launches from the bottom;
// each rocket schedules its own explosion (a particle burst) with after(riseTime), and every couple
// of seconds a finale fires several rockets at once. Nothing polls elapsed time by hand — the
// scheduler fires the callbacks when they come due. A core::Sequence loops a pulsing title glow to
// exercise the span/loop path. Spawn positions come from a seeded RNG and everything advances on the
// fixed-step clock, so the render is deterministic and golden-stable.
// Run --headless / --frames N for CI.

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

struct Rng {
    uint64_t s = 0x9E3779B97F4A7C15ull;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<uint32_t>(s >> 32);
    }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
    float range(float a, float b) { return a + (b - a) * unit(); }
};

render::TextureHandle softDot(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d);
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

struct Rocket {
    float x, y, vy;
    float age, riseTime;
    render::Color color;
    bool alive;
};

render::Color hueColor(float h) {
    h = h - std::floor(h);
    const float r = std::fabs(h * 6.0f - 3.0f) - 1.0f;
    const float g = 2.0f - std::fabs(h * 6.0f - 2.0f);
    const float b = 2.0f - std::fabs(h * 6.0f - 4.0f);
    auto cl = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    return render::Color{cl(r), cl(g), cl(b), 1.0f};
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("FIREWORKS (scheduler demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Scheduler";
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

    render::TextureHandle dot = softDot(*renderer, 32);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);

    Rng rng;
    fx::ParticleSystem particles(8192);
    std::vector<Rocket> rockets;
    int launched = 0;
    int bursts = 0;

    // Explode a rocket into a colored particle burst at its apex.
    auto explode = [&](float x, float y, render::Color col) {
        fx::BurstDesc b;
        b.count = 90;
        b.x = x;
        b.y = y;
        b.speedMin = 60.0f;
        b.speedMax = 320.0f;
        b.lifeMin = 0.7f;
        b.lifeMax = 1.6f;
        b.sizeStart = 9.0f;
        b.sizeEnd = 1.0f;
        b.colorStart = col;
        b.colorEnd = render::Color{col.r, col.g, col.b, 0.0f};
        b.gravity = 90.0f;
        b.drag = 0.5f;
        particles.emit(b);
        ++bursts;
    };

    // Launch one rocket that rises, then schedules its own explosion after a rise time.
    core::Scheduler sched;
    auto launch = [&](float targetY) {
        Rocket r;
        r.x = rng.range(sw * 0.15f, sw * 0.85f);
        r.y = sh - 20.0f;
        const float apexY = targetY;
        const float rise = 1.0f; // seconds to apex
        r.vy = (apexY - r.y) / rise;
        r.age = 0.0f;
        r.riseTime = rise;
        r.color = hueColor(rng.unit());
        r.alive = true;
        rockets.push_back(r);
        ++launched;
        // The rocket removes itself at apex (age >= riseTime); this one-shot timer fires the burst at
        // the same moment. The callback captures the apex by value, so it needs no rocket reference.
        const float cx = r.x, cy = apexY;
        const render::Color col = r.color;
        sched.after(static_cast<double>(rise), [&, cx, cy, col] { explode(cx, cy, col); });
    };

    // Steady stream: a rocket every 0.4s.
    sched.every(0.4, [&] { launch(rng.range(sh * 0.18f, sh * 0.45f)); }, -1);
    // Finale: every 2s, a volley of five.
    sched.every(2.0, [&] {
        for (int i = 0; i < 5; ++i) launch(rng.range(sh * 0.12f, sh * 0.4f));
    }, -1);

    // A looping sequence that pulses the title glow (exercises span + loop).
    float titleGlow = 0.0f;
    core::Sequence titlePulse;
    titlePulse.span(0.8, [&](float p) { titleGlow = p; })
        .span(0.8, [&](float p) { titleGlow = 1.0f - p; })
        .loop();

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const double dt = clock.fixedDelta();
            sched.update(dt);
            titlePulse.update(dt);
            const float fdt = static_cast<float>(dt);
            for (Rocket& r : rockets) {
                if (!r.alive) continue;
                r.y += r.vy * fdt;
                r.age += fdt;
                if (r.age >= r.riseTime) r.alive = false; // reached apex -> its burst fires now
            }
            rockets.erase(std::remove_if(rockets.begin(), rockets.end(),
                                         [](const Rocket& r) { return !r.alive; }),
                          rockets.end());
            particles.update(fdt);
        }

        renderer->setClearColor(render::Color{0.03f, 0.04f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Rising rockets (bright dots with a short trail).
            for (const Rocket& r : rockets) {
                for (int k = 0; k < 4; ++k) {
                    render::SpriteDesc d;
                    const float trail = static_cast<float>(k) * 8.0f;
                    const float sz = 14.0f - static_cast<float>(k) * 2.5f;
                    d.x = r.x - sz * 0.5f;
                    d.y = r.y + trail - sz * 0.5f;
                    d.width = sz;
                    d.height = sz;
                    d.color = render::Color{r.color.r, r.color.g, r.color.b,
                                            1.0f - static_cast<float>(k) * 0.22f};
                    renderer->drawSprite(dot, d);
                }
            }

            particles.draw(*renderer, dot);

            const float g = 0.6f + 0.4f * titleGlow;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SCHEDULER / TIMERS",
                          render::Color{g, g, 1.0f, 1.0f}, 0.7f);
            char buf[160];
            std::snprintf(buf, sizeof(buf),
                          "timers drive the show   |   active:%zu   launched:%d   bursts:%d   particles:%u",
                          sched.count(), launched, bursts, particles.alive());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("FIREWORKS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
