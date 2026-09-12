// Maz Engine — "MUSIC" (audio::MusicSequencer, toward Godot's AudioStreamInteractive)
// A7 of the audio deep-dive: adaptive/interactive music. Game music is a set of SEGMENTS the game
// switches between as the action changes, and the switch must land on a musical boundary (the next bar)
// with a crossfade or it sounds broken. This demo runs a scripted arrangement — Intro -> Explore ->
// Combat, each transition requested mid-bar but scheduled AtNextBar with a 0.6 s crossfade — through the
// real sequencer and draws it as an arrangement timeline: one gain ribbon per segment, with bar/beat
// tick marks. You can see each segment hold at full level, then cross-fade to the next exactly on a bar
// line (never mid-bar). Deterministic timing -> golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void thickLine(render::Renderer& r, math::vec2 a, math::vec2 b, float w, render::Color c) {
    math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y * w * 0.5f, d.x * w * 0.5f);
    const render::Point2 q[4] = {{a.x + n.x, a.y + n.y},
                                 {b.x + n.x, b.y + n.y},
                                 {b.x - n.x, b.y - n.y},
                                 {a.x - n.x, a.y - n.y}};
    r.drawConvexPolygon(q, 4, c);
}

void fillRect(render::Renderer& r, float x0, float y0, float x1, float y1, render::Color c) {
    const render::Point2 q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    r.drawConvexPolygon(q, 4, c);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MUSIC (audio::MusicSequencer) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Interactive Music Sequencer";
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

    const float sr = 44100.0f;
    const float bpm = 120.0f; // beat 0.5 s, bar 2.0 s
    const float totalSec = 7.0f;
    const int W = 1120; // timeline pixel columns

    audio::MusicSequencer seq(sr);
    const int intro = seq.addSegment("Intro", bpm, 4);
    const int explore = seq.addSegment("Explore", bpm, 4);
    const int combat = seq.addSegment("Combat", bpm, 4);
    seq.play(intro);

    struct Event {
        float atSec;
        int target;
        bool fired;
    };
    std::vector<Event> events = {{0.4f, explore, false}, {3.0f, combat, false}};

    // Sample the sequencer across the timeline; record each segment's gain per column.
    std::vector<std::array<float, 3>> gains(static_cast<std::size_t>(W), {0.0f, 0.0f, 0.0f});
    {
        const double step = static_cast<double>(totalSec) * static_cast<double>(sr) /
                            static_cast<double>(W - 1);
        for (int x = 0; x < W; ++x) {
            const double tSamples = static_cast<double>(x) * step;
            for (Event& e : events) {
                if (!e.fired && tSamples >= static_cast<double>(e.atSec) * static_cast<double>(sr)) {
                    seq.transitionTo(e.target, audio::TransitionMode::AtNextBar, 0.6f);
                    e.fired = true;
                }
            }
            seq.advance(tSamples - seq.clock());
            const audio::MusicMix m = seq.mix();
            std::array<float, 3> g = {0.0f, 0.0f, 0.0f};
            if (m.a >= 0 && m.a < 3) {
                g[static_cast<std::size_t>(m.a)] = m.aGain;
            }
            if (m.b >= 0 && m.b < 3) {
                g[static_cast<std::size_t>(m.b)] = m.bGain;
            }
            gains[static_cast<std::size_t>(x)] = g;
        }
    }

    const float tx0 = 70.0f, tx1 = 1210.0f;
    auto xOfSec = [&](float s) { return tx0 + (s / totalSec) * (tx1 - tx0); };
    auto xOfCol = [&](int c) { return tx0 + static_cast<float>(c) / static_cast<float>(W - 1) * (tx1 - tx0); };

    const char* names[3] = {"Intro", "Explore", "Combat"};
    const render::Color laneColor[3] = {rgba(0.5f, 0.7f, 1.0f, 1), rgba(0.5f, 0.9f, 0.55f, 1),
                                        rgba(1.0f, 0.55f, 0.45f, 1)};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  INTERACTIVE MUSIC (bar-synced)",
                          rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::MusicSequencer (Godot AudioStreamInteractive): switch requested mid-bar "
                          "fires on the next BAR line with a 0.6 s crossfade.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            const float laneTop = 120.0f, laneH = 130.0f, laneGap = 26.0f;
            const float chartBot = laneTop + 3.0f * laneH + 2.0f * laneGap;

            // Beat + bar tick marks spanning the whole chart.
            for (float t = 0.0f; t <= totalSec + 1e-3f; t += 0.5f) {
                const float x = xOfSec(t);
                const bool bar = std::fabs(std::fmod(t, 2.0f)) < 1e-3f;
                thickLine(*renderer, {x, laneTop - 12.0f}, {x, chartBot}, bar ? 1.6f : 0.7f,
                          bar ? rgba(0.5f, 0.55f, 0.68f, 1) : rgba(0.24f, 0.26f, 0.33f, 1));
                if (bar) {
                    char b[24];
                    std::snprintf(b, sizeof(b), "bar %d", static_cast<int>(t / 2.0f) + 1);
                    font.drawText(*renderer, x + 4.0f, laneTop - 30.0f, b, rgba(0.55f, 0.6f, 0.72f, 1),
                                  0.26f);
                }
            }

            // One gain ribbon per segment lane.
            for (int lane = 0; lane < 3; ++lane) {
                const float baseY = laneTop + static_cast<float>(lane) * (laneH + laneGap) + laneH;
                // Lane backdrop + label.
                fillRect(*renderer, tx0, baseY - laneH, tx1, baseY, rgba(0.11f, 0.12f, 0.16f, 1));
                font.drawText(*renderer, 12.0f, baseY - laneH * 0.5f - 10.0f, names[lane],
                              laneColor[lane], 0.32f);
                // Ribbon: a filled column per pixel, height = gain.
                for (int x = 0; x + 1 < W; ++x) {
                    const float g = gains[static_cast<std::size_t>(x)][static_cast<std::size_t>(lane)];
                    if (g > 0.001f) {
                        const float xa = xOfCol(x);
                        const float xb = xOfCol(x + 1);
                        const render::Color c = rgba(laneColor[lane].r, laneColor[lane].g,
                                                     laneColor[lane].b, 0.85f);
                        fillRect(*renderer, xa, baseY - g * (laneH - 8.0f), xb + 0.5f, baseY, c);
                    }
                }
                // Baseline.
                thickLine(*renderer, {tx0, baseY}, {tx1, baseY}, 1.0f, rgba(0.3f, 0.32f, 0.4f, 1));
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "each ribbon = that segment's live volume; transitions land exactly on bar "
                          "lines and cross-fade (equal power), never mid-bar",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MUSIC shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
