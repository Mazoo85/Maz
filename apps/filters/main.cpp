// Maz Engine — "FILTERS" (audio::Biquad gallery, toward Godot's AudioEffectFilter + dB units)
// A1 of the audio deep-dive completes the RBJ biquad set (low/high/band-pass already existed; this adds
// notch, peaking, low-shelf, high-shelf, all-pass) and adds decibel <-> linear helpers so gains read in
// dB like Godot's mixer. This demo plots the exact magnitude response |H(e^jw)| of each filter type in
// decibels across a log-frequency axis (20 Hz .. 20 kHz) — the classic EQ-curve view. You can read each
// shape directly: the low/high-pass roll-offs, the band-pass hump, the peaking boost, the shelves lifting
// one end, the notch's deep null, and the all-pass sitting flat at 0 dB (it changes only phase). All
// curves are computed analytically -> deterministic, golden-stable. Run --headless / --frames N for CI.

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

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

// Exact magnitude response of a biquad at angular frequency w.
float magAt(const audio::Biquad& f, float w) {
    const float c1 = std::cos(w), s1 = std::sin(w);
    const float c2 = std::cos(2.0f * w), s2 = std::sin(2.0f * w);
    const float numRe = f.b0 + f.b1 * c1 + f.b2 * c2;
    const float numIm = -(f.b1 * s1 + f.b2 * s2);
    const float denRe = 1.0f + f.a1 * c1 + f.a2 * c2;
    const float denIm = -(f.a1 * s1 + f.a2 * s2);
    return std::sqrt(numRe * numRe + numIm * numIm) / std::sqrt(denRe * denRe + denIm * denIm);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("FILTERS (audio::Biquad gallery) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Biquad Filter Gallery";
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
    struct Curve {
        std::string name;
        audio::Biquad f;
        render::Color color;
    };
    const std::vector<Curve> curves = {
        {"low-pass 1k", audio::Biquad::lowpass(1000.0f, 0.707f, sr), rgba(0.45f, 0.72f, 1.0f, 1)},
        {"high-pass 1k", audio::Biquad::highpass(1000.0f, 0.707f, sr), rgba(0.45f, 0.95f, 0.95f, 1)},
        {"band-pass 1k Q2", audio::Biquad::bandpass(1000.0f, 2.0f, sr), rgba(0.55f, 0.95f, 0.55f, 1)},
        {"notch 1k Q4", audio::Biquad::notch(1000.0f, 4.0f, sr), rgba(0.95f, 0.85f, 0.35f, 1)},
        {"peak 1k +12dB", audio::Biquad::peaking(1000.0f, 3.0f, 12.0f, sr), rgba(1.0f, 0.6f, 0.3f, 1)},
        {"low-shelf 300 +9", audio::Biquad::lowShelf(300.0f, 9.0f, sr), rgba(0.85f, 0.55f, 1.0f, 1)},
        {"high-shelf 4k -9", audio::Biquad::highShelf(4000.0f, -9.0f, sr), rgba(1.0f, 0.5f, 0.5f, 1)},
        {"all-pass 1k", audio::Biquad::allpass(1000.0f, 0.707f, sr), rgba(0.7f, 0.72f, 0.78f, 1)},
    };

    // Plot area + axis mapping (log frequency x, decibel y).
    const float x0 = 120.0f, x1 = 1180.0f, y0 = 96.0f, y1 = 628.0f;
    const float fLo = 20.0f, fHi = 20000.0f;
    const float dbTop = 18.0f, dbBot = -30.0f;
    const float lgLo = std::log10(fLo), lgHi = std::log10(fHi);
    auto xOfHz = [&](float hz) { return x0 + (std::log10(hz) - lgLo) / (lgHi - lgLo) * (x1 - x0); };
    auto yOfDb = [&](float db) { return y0 + (dbTop - db) / (dbTop - dbBot) * (y1 - y0); };

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  BIQUAD FILTER GALLERY",
                          rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::Biquad (Godot AudioEffectFilter): exact magnitude response in dB, "
                          "20 Hz - 20 kHz. Gains via audio::linearToDb.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // dB gridlines + labels.
            for (float db = dbTop; db >= dbBot; db -= 6.0f) {
                const float y = yOfDb(db);
                const bool zero = std::fabs(db) < 0.01f;
                thickLine(*renderer, {x0, y}, {x1, y}, zero ? 1.6f : 0.8f,
                          zero ? rgba(0.5f, 0.55f, 0.65f, 1) : rgba(0.2f, 0.22f, 0.28f, 1));
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%+d dB", static_cast<int>(db));
                font.drawText(*renderer, 60.0f, y - 10.0f, buf, rgba(0.55f, 0.6f, 0.7f, 1), 0.28f);
            }
            // Decade gridlines + labels.
            const float decades[3] = {100.0f, 1000.0f, 10000.0f};
            const char* decLbl[3] = {"100 Hz", "1 kHz", "10 kHz"};
            for (int i = 0; i < 3; ++i) {
                const float x = xOfHz(decades[i]);
                thickLine(*renderer, {x, y0}, {x, y1}, 0.8f, rgba(0.2f, 0.22f, 0.28f, 1));
                font.drawText(*renderer, x - 24.0f, y1 + 8.0f, decLbl[i], rgba(0.55f, 0.6f, 0.7f, 1),
                              0.28f);
            }

            // Each filter's response curve.
            const int samples = 240;
            for (const Curve& c : curves) {
                math::vec2 prev(0, 0);
                bool have = false;
                for (int i = 0; i <= samples; ++i) {
                    const float t = static_cast<float>(i) / static_cast<float>(samples);
                    const float lg = lgLo + t * (lgHi - lgLo);
                    const float hz = std::pow(10.0f, lg);
                    const float w = 2.0f * 3.14159265f * hz / sr;
                    float db = audio::linearToDb(magAt(c.f, w));
                    if (db > dbTop) {
                        db = dbTop;
                    }
                    if (db < dbBot) {
                        db = dbBot;
                    }
                    const math::vec2 p(xOfHz(hz), yOfDb(db));
                    if (have) {
                        thickLine(*renderer, prev, p, 2.0f, c.color);
                    }
                    prev = p;
                    have = true;
                }
            }

            // Legend (two columns).
            for (std::size_t i = 0; i < curves.size(); ++i) {
                const float lx = 150.0f + static_cast<float>(i % 2) * 330.0f;
                const float ly = 664.0f + static_cast<float>(i / 2) * 24.0f;
                thickLine(*renderer, {lx, ly + 6.0f}, {lx + 26.0f, ly + 6.0f}, 3.0f, curves[i].color);
                font.drawText(*renderer, lx + 34.0f, ly - 4.0f, curves[i].name.c_str(), curves[i].color,
                              0.3f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("FILTERS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
