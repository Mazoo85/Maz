// Maz Engine — "EQUALIZER" (audio::Equalizer, toward Godot's AudioEffectEQ10)
// A2 of the audio deep-dive: a multiband graphic equalizer — a bank of peaking bands at fixed center
// frequencies, each with its own gain in dB, that combine into one response curve. This demo dials a
// classic "smiley" curve into a 10-band EQ (bass and treble lifted, mids scooped) and draws two things
// against the same dB / log-frequency chart: the per-band FADERS (one bar per band at its center
// frequency, height = its gain) and the resulting COMPOSITE magnitude response the whole EQ applies. You
// can read the faders producing the smooth curve above them. Analytic + deterministic -> golden-stable.
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

void fillCircle(render::Renderer& r, math::vec2 c, float radius, render::Color col, int seg = 20) {
    std::vector<render::Point2> pts;
    pts.reserve(static_cast<std::size_t>(seg));
    for (int i = 0; i < seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        pts.push_back({c.x + std::cos(a) * radius, c.y + std::sin(a) * radius});
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(pts.size()), col);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

// Composite magnitude of the whole EQ at angular frequency w.
float eqMag(const audio::Equalizer& eq, float w) {
    const float c1 = std::cos(w), s1 = std::sin(w);
    const float c2 = std::cos(2.0f * w), s2 = std::sin(2.0f * w);
    float m = 1.0f;
    for (std::size_t i = 0; i < eq.bands.size(); ++i) {
        const audio::Biquad& f = eq.bands[i].filter;
        const float nr = f.b0 + f.b1 * c1 + f.b2 * c2;
        const float ni = -(f.b1 * s1 + f.b2 * s2);
        const float dr = 1.0f + f.a1 * c1 + f.a2 * c2;
        const float di = -(f.a1 * s1 + f.a2 * s2);
        m *= std::sqrt(nr * nr + ni * ni) / std::sqrt(dr * dr + di * di);
    }
    return m;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("EQUALIZER (audio::Equalizer) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Graphic Equalizer";
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
    audio::Equalizer eq = audio::Equalizer::eq10(sr);
    // A classic "smiley" curve: lift the extremes, scoop the mids.
    const float smiley[10] = {9.0f, 7.0f, 3.0f, -3.0f, -7.0f, -7.0f, -3.0f, 3.0f, 7.0f, 10.0f};
    for (std::size_t i = 0; i < eq.bandCount(); ++i) {
        eq.setBandGain(i, smiley[i]);
    }

    const float x0 = 120.0f, x1 = 1180.0f, y0 = 96.0f, y1 = 610.0f;
    const float fLo = 20.0f, fHi = 20000.0f;
    const float dbTop = 15.0f, dbBot = -15.0f;
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  GRAPHIC EQUALIZER (10-band)",
                          rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::Equalizer (Godot AudioEffectEQ10): per-band faders (bars) + the "
                          "composite response they produce. A \"smiley\" curve.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // dB grid.
            for (float db = dbTop; db >= dbBot; db -= 5.0f) {
                const float y = yOfDb(db);
                const bool zero = std::fabs(db) < 0.01f;
                thickLine(*renderer, {x0, y}, {x1, y}, zero ? 1.6f : 0.8f,
                          zero ? rgba(0.5f, 0.55f, 0.65f, 1) : rgba(0.2f, 0.22f, 0.28f, 1));
                char buf[16];
                std::snprintf(buf, sizeof(buf), "%+d dB", static_cast<int>(db));
                font.drawText(*renderer, 60.0f, y - 10.0f, buf, rgba(0.55f, 0.6f, 0.7f, 1), 0.28f);
            }

            // Per-band faders: a bar at each band center from 0 dB to its gain.
            const float zeroY = yOfDb(0.0f);
            for (std::size_t i = 0; i < eq.bandCount(); ++i) {
                const float bx = xOfHz(eq.bandFreq(i));
                const float g = eq.bandGain(i);
                const float by = yOfDb(g);
                const bool boost = g >= 0.0f;
                const render::Color barC =
                    boost ? rgba(0.35f, 0.6f, 0.95f, 0.55f) : rgba(0.95f, 0.5f, 0.4f, 0.55f);
                fillRect(*renderer, bx - 14.0f, std::min(by, zeroY), bx + 14.0f, std::max(by, zeroY),
                         barC);
                // Fader cap.
                fillCircle(*renderer, {bx, by}, 6.0f, boost ? rgba(0.6f, 0.82f, 1.0f, 1) : rgba(1.0f, 0.7f, 0.6f, 1));
                // Frequency label under the axis.
                char fb[16];
                const float hz = eq.bandFreq(i);
                if (hz >= 1000.0f) {
                    std::snprintf(fb, sizeof(fb), "%.0fk", hz / 1000.0f);
                } else {
                    std::snprintf(fb, sizeof(fb), "%.0f", hz);
                }
                font.drawText(*renderer, bx - 12.0f, y1 + 8.0f, fb, rgba(0.55f, 0.6f, 0.7f, 1), 0.26f);
            }

            // Composite response curve.
            const int samples = 260;
            math::vec2 prev(0, 0);
            bool have = false;
            for (int i = 0; i <= samples; ++i) {
                const float t = static_cast<float>(i) / static_cast<float>(samples);
                const float lg = lgLo + t * (lgHi - lgLo);
                const float hz = std::pow(10.0f, lg);
                const float w = 2.0f * 3.14159265f * hz / sr;
                float db = audio::linearToDb(eqMag(eq, w));
                db = std::min(std::max(db, dbBot), dbTop);
                const math::vec2 p(xOfHz(hz), yOfDb(db));
                if (have) {
                    thickLine(*renderer, prev, p, 2.4f, rgba(1.0f, 0.85f, 0.4f, 1));
                }
                prev = p;
                have = true;
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "bars = each band's gain fader   gold line = the combined EQ response applied "
                          "to the audio",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("EQUALIZER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
