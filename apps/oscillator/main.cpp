// Maz Engine — "OSCILLATOR" (audio::Oscillator, toward Godot's AudioStreamGenerator)
// A6 of the audio deep-dive: the raw waveforms a synth voice is built from. This demo is a scope gallery
// — one panel per waveform (sine, band-limited sawtooth, band-limited square, triangle, white noise, and
// a 2-operator FM tone) drawn as a couple of cycles on a zeroed grid. The saw and square use PolyBLEP so
// their sharp edges are band-limited (anti-aliased) rather than infinitely steep; you can see the tiny
// rounding at each edge. All waveforms are generated deterministically -> golden-stable. Run --headless /
// --frames N for CI.

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
    MAZ_LOG_INFO("OSCILLATOR (audio::Oscillator) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Oscillator / Synth Waveforms";
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

    const int M = 360;         // samples per panel (== inner pixel width)
    const float sr = 44100.0f; // arbitrary; frequency chosen for cycles-per-panel
    const float cycles = 2.5f;
    const float f = cycles * sr / static_cast<float>(M);

    auto genWave = [&](audio::Waveform wf) {
        audio::Oscillator o;
        o.sampleRate = sr;
        o.freq = f;
        o.waveform = wf;
        o.reset(0xC0FFEEu);
        std::vector<float> v;
        v.reserve(static_cast<std::size_t>(M));
        for (int i = 0; i < M; ++i) {
            v.push_back(o.next());
        }
        return v;
    };

    std::vector<Panel> panels;
    panels.push_back({"sine", rgba(0.55f, 0.82f, 1.0f, 1), genWave(audio::Waveform::Sine)});
    panels.push_back({"saw (band-limited)", rgba(0.6f, 0.95f, 0.6f, 1), genWave(audio::Waveform::Saw)});
    panels.push_back(
        {"square (band-limited)", rgba(1.0f, 0.75f, 0.4f, 1), genWave(audio::Waveform::Square)});
    panels.push_back({"triangle", rgba(1.0f, 0.6f, 0.75f, 1), genWave(audio::Waveform::Triangle)});
    panels.push_back({"white noise", rgba(0.8f, 0.8f, 0.85f, 1), genWave(audio::Waveform::Noise)});
    {
        // 2-operator FM tone.
        audio::FMVoice fm;
        fm.carrier.sampleRate = sr;
        fm.carrier.freq = f;
        fm.carrier.waveform = audio::Waveform::Sine;
        fm.modulator.sampleRate = sr;
        fm.modulator.freq = f * 3.0f;
        fm.modulator.waveform = audio::Waveform::Sine;
        fm.index = 0.6f;
        fm.reset();
        std::vector<float> v;
        v.reserve(static_cast<std::size_t>(M));
        for (int i = 0; i < M; ++i) {
            v.push_back(fm.next());
        }
        panels.push_back({"FM (2-op)", rgba(0.75f, 0.7f, 1.0f, 1), v});
    }

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  OSCILLATOR / SYNTH WAVEFORMS",
                          rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::Oscillator (Godot AudioStreamGenerator): saw + square are PolyBLEP "
                          "band-limited (anti-aliased). Plus a 2-op FM tone.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            const float panelW = 380.0f, panelH = 230.0f;
            const float ox = 40.0f, oy = 96.0f, gx = 30.0f, gy = 40.0f;
            for (std::size_t i = 0; i < panels.size(); ++i) {
                const int col = static_cast<int>(i % 3);
                const int row = static_cast<int>(i / 3);
                const float px = ox + static_cast<float>(col) * (panelW + gx);
                const float py = oy + static_cast<float>(row) * (panelH + gy);
                const float midY = py + panelH * 0.5f;

                // Panel frame + zero line.
                const render::Color frame = rgba(0.2f, 0.22f, 0.28f, 1);
                thickLine(*renderer, {px, py}, {px + panelW, py}, 1.0f, frame);
                thickLine(*renderer, {px, py + panelH}, {px + panelW, py + panelH}, 1.0f, frame);
                thickLine(*renderer, {px, py}, {px, py + panelH}, 1.0f, frame);
                thickLine(*renderer, {px + panelW, py}, {px + panelW, py + panelH}, 1.0f, frame);
                thickLine(*renderer, {px, midY}, {px + panelW, midY}, 1.0f,
                          rgba(0.3f, 0.32f, 0.4f, 1));

                // Waveform trace (amplitude +/-1 maps to +/- 0.42*panelH).
                const Panel& p = panels[i];
                const float amp = panelH * 0.42f;
                const float innerW = panelW - 20.0f;
                math::vec2 prev(0, 0);
                bool have = false;
                for (std::size_t s = 0; s < p.samples.size(); ++s) {
                    const float t = static_cast<float>(s) / static_cast<float>(p.samples.size() - 1);
                    const float sx = px + 10.0f + t * innerW;
                    const float sy = midY - p.samples[s] * amp;
                    const math::vec2 sp(sx, sy);
                    if (have) {
                        thickLine(*renderer, prev, sp, 1.8f, p.color);
                    }
                    prev = sp;
                    have = true;
                }

                font.drawText(*renderer, px + 8.0f, py + 6.0f, p.label.c_str(), p.color, 0.34f);
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "these are the source tones a synth voice shapes with an envelope + filter; the "
                          "saw/square edges are rounded, not aliased",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("OSCILLATOR shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
