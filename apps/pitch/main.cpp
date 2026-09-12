// Maz Engine — "PITCH" (audio::PitchShifter, toward Godot's AudioEffectPitchShift)
// A8 of the audio deep-dive: raise or lower pitch WITHOUT changing playback speed. This demo runs one
// 220 Hz tone through the granular pitch shifter at three settings — an octave down, unchanged, an octave
// up — and draws each output over the SAME time window. Because only the pitch changes (not the speed),
// the octave-up panel packs twice as many cycles into the window and the octave-down half as many, while
// all three span the same duration. Deterministic -> golden-stable. Run --headless / --frames N for CI.

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

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

struct Panel {
    std::string label;
    render::Color color;
    std::vector<float> samples;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PITCH (audio::PitchShifter) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Pitch Shifter";
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
    const float freq = 220.0f;
    const int warmup = 2600;
    const int win = 1100; // samples shown per panel (~25 ms)

    auto run = [&](float ratio) {
        audio::PitchShifter ps;
        ps.configure(ratio, 1024);
        std::vector<float> out;
        out.reserve(static_cast<std::size_t>(warmup + win));
        for (int i = 0; i < warmup + win; ++i) {
            const float x = std::sin(2.0f * 3.14159265f * freq * static_cast<float>(i) / sr);
            const float y = ps.process(x);
            if (i >= warmup) {
                out.push_back(y);
            }
        }
        return out;
    };

    std::vector<Panel> panels = {
        {"down an octave  (x0.5)", rgba(0.55f, 0.82f, 1.0f, 1), run(0.5f)},
        {"original  (x1.0)", rgba(0.75f, 0.8f, 0.85f, 1), run(1.0f)},
        {"up an octave  (x2.0)", rgba(1.0f, 0.7f, 0.4f, 1), run(2.0f)},
    };

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  PITCH SHIFTER", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::PitchShifter (Godot AudioEffectPitchShift): same 220 Hz tone, same time "
                          "window - only the pitch changes, not the speed.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            const float px = 70.0f, pw = 1140.0f;
            const float top = 120.0f, laneH = 150.0f, gap = 40.0f;
            for (std::size_t i = 0; i < panels.size(); ++i) {
                const Panel& p = panels[i];
                const float midY = top + static_cast<float>(i) * (laneH + gap) + laneH * 0.5f;

                // Frame + zero line.
                thickLine(*renderer, {px, midY - laneH * 0.5f}, {px + pw, midY - laneH * 0.5f}, 1.0f,
                          rgba(0.2f, 0.22f, 0.28f, 1));
                thickLine(*renderer, {px, midY + laneH * 0.5f}, {px + pw, midY + laneH * 0.5f}, 1.0f,
                          rgba(0.2f, 0.22f, 0.28f, 1));
                thickLine(*renderer, {px, midY}, {px + pw, midY}, 1.0f, rgba(0.3f, 0.32f, 0.4f, 1));

                // Waveform.
                const float amp = laneH * 0.42f;
                math::vec2 prev(0, 0);
                bool have = false;
                for (std::size_t s = 0; s < p.samples.size(); ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(p.samples.size() - 1);
                    const float sx = px + t * pw;
                    const float sy = midY - p.samples[s] * amp;
                    const math::vec2 sp(sx, sy);
                    if (have) {
                        thickLine(*renderer, prev, sp, 1.8f, p.color);
                    }
                    prev = sp;
                    have = true;
                }

                font.drawText(*renderer, px + 6.0f, midY - laneH * 0.5f - 26.0f, p.label.c_str(), p.color,
                              0.34f);
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "twice the cycles = an octave up; half the cycles = an octave down - all in the "
                          "same duration (speed unchanged)",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PITCH shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
