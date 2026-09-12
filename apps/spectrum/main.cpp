// Maz Engine — "SPECTRUM" (audio::SpectrumAnalyzer, toward Godot's AudioEffectSpectrumAnalyzer)
// A spectrum analyzer turns audio samples into a frequency spectrum so a game can *see* sound — the maths
// behind rhythm games, equalizer / VU visualizers, and beat-reactive lights and particles. This demo
// synthesizes a short chord (a fundamental + a fifth + an octave + a faint high partial), runs it through a
// Hann-windowed FFT (audio::SpectrumAnalyzer), and plots the magnitude spectrum as a frequency bar graph
// with the loudest bin labelled and axis ticks. Below it, three band meters (bass / mid / treble) are read
// straight from `magnitudeForRange` — Godot's `get_magnitude_for_frequency_range`. Static signal ->
// deterministic, golden-stable.

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

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color col) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SPECTRUM (audio::SpectrumAnalyzer) starting");

    // Synthesize a chord at 8 kHz: 250 (fundamental) + 375 (fifth) + 500 (octave) + a faint 1000 Hz partial.
    const float sr = 8000.0f;
    const std::size_t N = 1024;
    std::vector<float> sig(N);
    const float pi = 3.14159265358979323846f;
    for (std::size_t i = 0; i < N; ++i) {
        const float t = static_cast<float>(i) / sr;
        sig[i] = 0.90f * std::sin(2.0f * pi * 250.0f * t) + 0.55f * std::sin(2.0f * pi * 375.0f * t) +
                 0.70f * std::sin(2.0f * pi * 500.0f * t) + 0.20f * std::sin(2.0f * pi * 1000.0f * t);
    }

    audio::SpectrumAnalyzer analyzer(sr, N, audio::SpectrumWindow::Hann);
    analyzer.analyze(sig);

    // Visible range: 0..1300 Hz.
    const float maxHz = 1300.0f;
    std::size_t maxBin = 0;
    float peakMag = 1e-6f;
    for (std::size_t i = 0; i < analyzer.binCount(); ++i) {
        if (analyzer.binFrequency(i) > maxHz) {
            break;
        }
        maxBin = i;
        peakMag = std::max(peakMag, analyzer.magnitude(i));
    }

    // Band energies (Godot get_magnitude_for_frequency_range).
    const float bass = analyzer.magnitudeForRange(0.0f, 300.0f);
    const float mid = analyzer.magnitudeForRange(300.0f, 700.0f);
    const float treble = analyzer.magnitudeForRange(700.0f, 2000.0f);
    const float peakBandMax = std::max(bass, std::max(mid, treble)) + 1e-6f;

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Spectrum";
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

    const render::Color label{0.82f, 0.86f, 0.94f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SPECTRUM",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "FFT frequency spectrum of a chord - Godot's AudioEffectSpectrumAnalyzer "
                          "(audio::SpectrumAnalyzer)",
                          label, 0.4f);

            // --- Spectrum bar graph ---
            const float gx = 70.0f, gTop = 120.0f, gBase = 470.0f;
            const float gW = 1140.0f;
            fillRect(*renderer, gx - 8.0f, gTop - 8.0f, gW + 16.0f, gBase - gTop + 40.0f,
                     render::Color{0.1f, 0.11f, 0.15f, 1.0f});

            const std::size_t bins = maxBin + 1;
            const float bw = gW / static_cast<float>(bins);
            for (std::size_t i = 0; i < bins; ++i) {
                const float m = analyzer.magnitude(i) / peakMag; // 0..1
                const float bh = m * (gBase - gTop);
                const float x = gx + static_cast<float>(i) * bw;
                // Colour by frequency band for a spectrum look.
                const float f = analyzer.binFrequency(i);
                render::Color c = f < 300.0f ? render::Color{0.45f, 0.7f, 0.95f, 1.0f}
                                  : f < 700.0f ? render::Color{0.5f, 0.9f, 0.55f, 1.0f}
                                               : render::Color{0.95f, 0.7f, 0.4f, 1.0f};
                if (m < 0.02f) {
                    c = render::Color{0.2f, 0.22f, 0.28f, 1.0f};
                }
                fillRect(*renderer, x, gBase - bh, bw > 2.0f ? bw - 1.0f : bw, bh, c);
            }

            // Frequency axis ticks every 250 Hz.
            for (int hz = 0; hz <= 1250; hz += 250) {
                const float x = gx + (static_cast<float>(hz) / maxHz) * gW;
                fillRect(*renderer, x, gBase, 1.5f, 8.0f, render::Color{0.5f, 0.55f, 0.65f, 1});
                char t[16];
                std::snprintf(t, sizeof(t), "%dHz", hz);
                font.drawText(*renderer, x - 10.0f, gBase + 12.0f, t, label, 0.28f);
            }

            // Mark the loudest bin.
            {
                const std::size_t pk = analyzer.peakBin();
                const float x = gx + static_cast<float>(pk) * bw;
                char t[32];
                std::snprintf(t, sizeof(t), "peak %.0f Hz", static_cast<double>(analyzer.binFrequency(pk)));
                font.drawText(*renderer, x - 20.0f, gTop - 4.0f, t, render::Color{1, 1, 0.7f, 1}, 0.32f);
            }

            // --- Band-energy meters (magnitudeForRange). ---
            font.drawText(*renderer, gx, 512.0f, "band energy  (magnitudeForRange - Godot's beat / VU query)",
                          render::Color{0.7f, 0.8f, 1.0f, 1}, 0.4f);
            struct Band {
                const char* name;
                float v;
                render::Color col;
            };
            const Band bands[] = {{"BASS  0-300 Hz", bass, {0.45f, 0.7f, 0.95f, 1}},
                                  {"MID   300-700 Hz", mid, {0.5f, 0.9f, 0.55f, 1}},
                                  {"TREBLE 700-2000 Hz", treble, {0.95f, 0.7f, 0.4f, 1}}};
            float by = 548.0f;
            for (const Band& b : bands) {
                font.drawText(*renderer, gx, by, b.name, label, 0.32f);
                const float barX = gx + 260.0f, barW = 880.0f, barH = 28.0f;
                fillRect(*renderer, barX, by - 2.0f, barW, barH, render::Color{0.14f, 0.15f, 0.2f, 1.0f});
                fillRect(*renderer, barX, by - 2.0f, barW * (b.v / peakBandMax), barH, b.col);
                by += 46.0f;
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SPECTRUM shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
