// Maz Engine — "LIMITER" (audio::Limiter, toward Godot's AudioEffectHardLimiter)
// A4 of the audio deep-dive: a brickwall lookahead limiter — an absolute ceiling the output can never
// cross, used on a master bus to push loudness without clipping. This demo feeds a sine whose amplitude
// swells from below the ceiling to well above it, runs it through the limiter, and draws the INPUT
// waveform (which overshoots the ceiling on the right) against the LIMITED output (pinned to the ceiling)
// with the +/-ceiling lines marked, plus a gain-reduction trace showing how much the limiter pulled the
// gain down over time. The output is time-aligned to the input by the limiter's lookahead so the two
// curves overlay. Fixed signal -> deterministic, golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

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

void polyline(render::Renderer& r, const std::vector<math::vec2>& pts, float w, render::Color c) {
    for (std::size_t i = 1; i < pts.size(); ++i) {
        thickLine(r, pts[i - 1], pts[i], w, c);
    }
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LIMITER (audio::Limiter) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Brickwall Limiter";
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
    const float ceilDb = -0.3f;
    const float ceilLin = audio::dbToLinear(ceilDb);

    // Build a swelling sine and run it through the limiter.
    const int N = 1100;
    audio::Limiter lim;
    lim.configure(ceilDb, 60.0f, 2.0f, sr);
    const int look = lim.lookaheadSamples();
    std::vector<float> in(static_cast<std::size_t>(N), 0.0f);
    std::vector<float> out(static_cast<std::size_t>(N), 0.0f);
    std::vector<float> gr(static_cast<std::size_t>(N), 1.0f);
    for (int i = 0; i < N; ++i) {
        const float amp = 0.2f + 1.5f * static_cast<float>(i) / static_cast<float>(N);
        const float x = amp * std::sin(2.0f * 3.14159265f * 200.0f * static_cast<float>(i) / sr);
        in[static_cast<std::size_t>(i)] = x;
        out[static_cast<std::size_t>(i)] = lim.process(x);
        gr[static_cast<std::size_t>(i)] = lim.gainReduction();
    }

    // Plot geometry.
    const float x0 = 70.0f, x1 = 1210.0f;
    const float wavMidY = 300.0f, wavAmp = 170.0f; // amplitude 2.0 maps to wavAmp*... use scale
    const float ampScale = wavAmp / 2.0f;           // input reaches ~1.7, ceiling ~0.97
    auto xOf = [&](int i) {
        return x0 + static_cast<float>(i) / static_cast<float>(N - 1) * (x1 - x0);
    };
    auto yOfAmp = [&](float a) { return wavMidY - a * ampScale; };

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  BRICKWALL LIMITER", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::Limiter (Godot AudioEffectHardLimiter): input swells past the ceiling; "
                          "the limited output is pinned to it. Ceiling -0.3 dB.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // Zero line + ceiling lines.
            thickLine(*renderer, {x0, wavMidY}, {x1, wavMidY}, 1.0f, rgba(0.3f, 0.32f, 0.4f, 1));
            for (float s : {1.0f, -1.0f}) {
                const float cy = yOfAmp(s * ceilLin);
                thickLine(*renderer, {x0, cy}, {x1, cy}, 1.4f, rgba(0.9f, 0.4f, 0.4f, 0.9f));
            }
            font.drawText(*renderer, x1 - 120.0f, yOfAmp(ceilLin) - 26.0f, "ceiling",
                          rgba(0.95f, 0.55f, 0.55f, 1), 0.3f);

            // Input waveform (faint), overshoots the ceiling on the right.
            {
                std::vector<math::vec2> pts;
                pts.reserve(static_cast<std::size_t>(N));
                for (int i = 0; i < N; ++i) {
                    pts.push_back({xOf(i), yOfAmp(in[static_cast<std::size_t>(i)])});
                }
                polyline(*renderer, pts, 1.4f, rgba(0.55f, 0.6f, 0.72f, 0.7f));
            }
            // Limited output (bright), time-aligned back by the lookahead so it overlays the input.
            {
                std::vector<math::vec2> pts;
                pts.reserve(static_cast<std::size_t>(N));
                for (int i = 0; i + look < N; ++i) {
                    pts.push_back({xOf(i), yOfAmp(out[static_cast<std::size_t>(i + look)])});
                }
                polyline(*renderer, pts, 2.0f, rgba(0.45f, 0.95f, 0.6f, 1));
            }

            // Gain-reduction trace (dB) along the bottom.
            const float grTop = 470.0f, grBot = 620.0f;
            const float grFloorDb = -12.0f;
            thickLine(*renderer, {x0, grTop}, {x1, grTop}, 1.0f, rgba(0.3f, 0.32f, 0.4f, 1));
            font.drawText(*renderer, 16.0f, grTop - 12.0f, "gain reduction", rgba(0.8f, 0.86f, 0.95f, 1),
                          0.3f);
            font.drawText(*renderer, 20.0f, grTop - 2.0f, "0 dB", rgba(0.55f, 0.6f, 0.7f, 1), 0.26f);
            font.drawText(*renderer, 20.0f, grBot - 14.0f, "-12", rgba(0.55f, 0.6f, 0.7f, 1), 0.26f);
            {
                std::vector<math::vec2> pts;
                pts.reserve(static_cast<std::size_t>(N));
                for (int i = 0; i + look < N; ++i) {
                    float db = audio::linearToDb(gr[static_cast<std::size_t>(i + look)]);
                    if (db < grFloorDb) {
                        db = grFloorDb;
                    }
                    const float t = (0.0f - db) / (0.0f - grFloorDb); // 0..1 downward
                    pts.push_back({xOf(i), grTop + t * (grBot - grTop)});
                }
                polyline(*renderer, pts, 2.0f, rgba(1.0f, 0.7f, 0.35f, 1));
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "grey = raw input (exceeds ceiling)   green = limited output (never crosses "
                          "it)   amber = gain the limiter removed",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LIMITER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
