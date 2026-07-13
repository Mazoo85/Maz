// Maz Engine — "MODFX" (chorus / flanger / phaser modulated-delay effects, toward Godot's AudioEffect*)
// The signal scope (see apps/bus, apps/reverb) for the three time-modulation effects. One sustained
// sawtooth note is run through each and drawn as its own waveform band. CHORUS layers a few detuned
// delayed copies so the tone shimmers and thickens; FLANGER sweeps a short feedback delay so a comb of
// notches whooshes across the tone (periodic amplitude dips); PHASER sweeps a cascade of all-pass
// notches for a softer swirl. Everything is computed once at 44.1 kHz, decimated, and drawn statically,
// so the render is deterministic and golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
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

void drawWave(render::Renderer& r, const std::vector<float>& buf, float x0, float cy, float w, float amp,
              render::Color col) {
    const int cols = 560;
    math::vec2 prev(0.0f, 0.0f);
    bool have = false;
    for (int i = 0; i <= cols; ++i) {
        const float f = static_cast<float>(i) / static_cast<float>(cols);
        const std::size_t idx =
            buf.empty() ? 0 : static_cast<std::size_t>(f * static_cast<float>(buf.size() - 1));
        const float v = buf.empty() ? 0.0f : buf[idx];
        const math::vec2 p(x0 + f * w, cy - v * amp);
        if (have) {
            thickLine(r, prev, p, 2.0f, col);
        }
        prev = p;
        have = true;
    }
}

render::Color rgb(float r, float g, float b) { return render::Color{r, g, b, 1.0f}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("MODFX (chorus/flanger/phaser) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Chorus / Flanger / Phaser";
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

    // ---- Source: one sustained sawtooth note with a gentle attack/release so the edges don't click ---
    const float sr = 44100.0f;
    const int total = 26460; // 0.6 s
    const float noteFreq = 55.0f;
    std::vector<float> src(static_cast<std::size_t>(total), 0.0f);
    for (int i = 0; i < total; ++i) {
        const float t = static_cast<float>(i) / sr;
        const float ph = t * noteFreq;
        const float saw = 2.0f * (ph - std::floor(ph + 0.5f));
        const float f = static_cast<float>(i) / static_cast<float>(total);
        const float env = std::min(1.0f, std::min(f * 8.0f, (1.0f - f) * 8.0f)); // trapezoid
        src[static_cast<std::size_t>(i)] = saw * env * 0.7f;
    }

    auto through = [&](auto& fx) {
        std::vector<float> out(src.size());
        for (std::size_t i = 0; i < src.size(); ++i) {
            out[i] = fx.process(src[i]);
        }
        return out;
    };

    audio::Chorus chorus;
    chorus.configure(sr, 3, 22.0f, 6.0f, 0.8f, 0.6f);
    const std::vector<float> wetChorus = through(chorus);

    audio::Flanger flanger;
    flanger.configure(sr, 2.0f, 3.0f, 0.7f, 0.72f, 0.7f);
    const std::vector<float> wetFlanger = through(flanger);

    audio::Phaser phaser;
    phaser.configure(sr, 250.0f, 1600.0f, 0.6f, 0.6f, 0.7f);
    const std::vector<float> wetPhaser = through(phaser);

    struct Band {
        const std::vector<float>* buf;
        render::Color col;
        const char* label;
    };
    const Band bands[4] = {
        {&src, rgb(0.75f, 0.78f, 0.85f), "SOURCE  -  one sustained sawtooth note"},
        {&wetChorus, rgb(0.45f, 0.80f, 1.00f), "CHORUS  -  detuned delayed copies thicken/shimmer the tone"},
        {&wetFlanger, rgb(1.00f, 0.62f, 0.40f), "FLANGER  -  a swept feedback comb whooshes across it"},
        {&wetPhaser, rgb(0.60f, 1.00f, 0.65f), "PHASER  -  swept all-pass notches give a softer swirl"},
    };

    const float marginX = 40.0f;
    const float waveW = static_cast<float>(cfg.width) - 2.0f * marginX;
    const float top = 96.0f;
    const float bandH = (static_cast<float>(cfg.height) - top - 24.0f) / 4.0f;

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

            for (int b = 0; b < 4; ++b) {
                const float cy = top + bandH * (static_cast<float>(b) + 0.5f);
                const float amp = bandH * 0.40f;
                const render::Point2 panel[4] = {{marginX, cy - bandH * 0.5f + 6.0f},
                                                 {marginX + waveW, cy - bandH * 0.5f + 6.0f},
                                                 {marginX + waveW, cy + bandH * 0.5f - 6.0f},
                                                 {marginX, cy + bandH * 0.5f - 6.0f}};
                renderer->drawConvexPolygon(panel, 4, render::Color{0.12f, 0.13f, 0.17f, 1.0f});
                thickLine(*renderer, {marginX, cy}, {marginX + waveW, cy}, 1.0f,
                          render::Color{0.28f, 0.30f, 0.36f, 1.0f});
                drawWave(*renderer, *bands[b].buf, marginX, cy, waveW, amp, bands[b].col);
                font.drawText(*renderer, marginX + 6.0f, cy - bandH * 0.5f + 10.0f, bands[b].label,
                              bands[b].col, 0.40f);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CHORUS / FLANGER / PHASER",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 52.0f,
                          "one sustained note through three LFO-swept modulated-delay effects "
                          "(audio::Chorus / Flanger / Phaser)",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("MODFX shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
