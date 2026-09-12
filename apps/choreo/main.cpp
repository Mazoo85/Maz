// Maz Engine — "CHOREO" (tween sequencer / property animator, toward Godot's SceneTreeTween)
// A TweenPlayer choreographs bound values over time: property tweens chained in sequence, some running
// in parallel, with delays and callbacks, optionally looping. You can't see motion in a still, so this
// advances five different choreographies to the SAME fixed time and draws each dot where its player put
// it — a snapshot of five machines mid-run. Each lane's dot position (and, for the parallel lane, its
// radius) is written every step by the player through a bound setter; nothing is hand-placed. Fixed
// time + fixed step -> deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    if (rad < 1.0f) {
        rad = 1.0f;
    }
    const int seg = 24;
    render::Point2 p[26];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

struct Lane {
    float p = 0.0f;  // normalized position along the track [0,1]
    float r = 12.0f; // dot radius
    anim::TweenPlayer tp;
    const char* label = "";
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CHOREO (tween sequencer) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Tween Choreography";
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
    render::TextureHandle white = whiteTex(*renderer);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // Build five choreographies. Setters bind to each lane's fields (lanes has a stable address).
    Lane lanes[5];
    lanes[0].label = "sequential: 0->1 ease-out, then hold";
    lanes[0].tp.appendProperty([&](float v) { lanes[0].p = v; }, 0.0f, 1.0f, 1.2f, anim::Ease::CubicOut)
        .appendInterval(0.4f);

    lanes[1].label = "parallel: move + grow at once";
    lanes[1].tp.appendProperty([&](float v) { lanes[1].p = v; }, 0.0f, 1.0f, 1.2f, anim::Ease::SineInOut)
        .parallelProperty([&](float v) { lanes[1].r = v; }, 8.0f, 26.0f, 1.2f, anim::Ease::SineInOut);

    lanes[2].label = "delay then move (interval + property)";
    lanes[2].tp.appendInterval(0.5f).appendProperty([&](float v) { lanes[2].p = v; }, 0.0f, 1.0f, 1.0f);

    lanes[3].label = "looping: 0->1 every 0.5s (loops forever)";
    lanes[3].tp.appendProperty([&](float v) { lanes[3].p = v; }, 0.0f, 1.0f, 0.5f).setLoops(0);

    lanes[4].label = "bounce: 0->1 with a settle at the end";
    lanes[4].tp.appendProperty([&](float v) { lanes[4].p = v; }, 0.0f, 1.0f, 1.2f, anim::Ease::BounceOut);

    // Advance every player to the SAME fixed time with a fixed step → deterministic snapshot.
    const float kT = 0.7f;
    const float kDt = 1.0f / 240.0f;
    for (float t = 0.0f; t < kT - 1e-6f; t += kDt) {
        for (Lane& ln : lanes) {
            ln.tp.update(kDt);
        }
    }

    const float xL = 372.0f, xR = 1200.0f;
    const render::Color kDot{0.42f, 0.72f, 1.0f, 1.0f};
    const render::Color kDot2{1.0f, 0.66f, 0.34f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TWEEN CHOREOGRAPHY",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "five property-tween sequences snapshotted at t = 0.70s (anim::TweenPlayer)",
                          render::Color{0.78f, 0.83f, 0.93f, 1}, 0.34f);

            const float y0 = 108.0f, laneH = 112.0f;
            for (int i = 0; i < 5; ++i) {
                const Lane& ln = lanes[static_cast<std::size_t>(i)];
                const float midY = y0 + static_cast<float>(i) * laneH + 66.0f;

                // Lane label + track.
                font.drawText(*renderer, 24.0f, midY - 44.0f, ln.label,
                              render::Color{0.82f, 0.86f, 0.93f, 1}, 0.32f);
                fillRect(*renderer, white, xL, midY - 2.0f, xR - xL, 4.0f,
                         render::Color{0.24f, 0.27f, 0.33f, 1.0f});
                // Start + end ticks.
                fillRect(*renderer, white, xL - 2.0f, midY - 14.0f, 4.0f, 28.0f,
                         render::Color{0.4f, 0.44f, 0.5f, 1.0f});
                fillRect(*renderer, white, xR - 2.0f, midY - 14.0f, 4.0f, 28.0f,
                         render::Color{0.4f, 0.44f, 0.5f, 1.0f});

                // Trail from the start to the dot, then the dot.
                const float dotX = xL + ln.p * (xR - xL);
                fillRect(*renderer, white, xL, midY - 1.0f, dotX - xL, 2.0f,
                         render::Color{0.3f, 0.5f, 0.7f, 0.6f});
                const render::Color col = (i == 1) ? kDot2 : kDot;
                fillCircle(*renderer, math::vec2(dotX, midY), ln.r, col);
                fillCircle(*renderer, math::vec2(dotX, midY), ln.r * 0.45f,
                           render::Color{1, 1, 1, 0.85f});
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CHOREO shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
