// Maz Engine — "ENVELOPE" (ADSR envelope generator, for synth voice shaping)
// The amplitude contour every synth note is shaped by: on note-on the level ramps up over ATTACK, falls
// to the SUSTAIN level over DECAY, holds while the key is down, then over RELEASE falls back to 0 on
// note-off. This shows three presets side by side — a plucky blip, a slow-swelling pad, and a percussive
// stab — each as its ADSR CURVE (top) and the TONE shaped by it (bottom): a sine whose amplitude is the
// envelope, so you can see the note swell in, hold, and fade. All precomputed at 44.1 kHz -> deterministic.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

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

struct Preset {
    const char* name;
    render::Color color;
    audio::ADSR adsr;
};

// Sample the envelope over `total` seconds with the key held from `onT` to `offT`, at `n` points.
std::vector<float> sampleEnvelope(audio::ADSR adsr, float total, float onT, float offT, int n) {
    std::vector<float> out(static_cast<std::size_t>(n), 0.0f);
    const float dt = total / static_cast<float>(n);
    adsr.reset();
    bool on = false, off = false;
    for (int i = 0; i < n; ++i) {
        const float t = static_cast<float>(i) * dt;
        if (!on && t >= onT) {
            adsr.noteOn();
            on = true;
        }
        if (!off && t >= offT) {
            adsr.noteOff();
            off = true;
        }
        out[static_cast<std::size_t>(i)] = adsr.process(dt);
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ENVELOPE (ADSR) starting");

    auto mk = [](float a, float d, float s, float r) {
        audio::ADSR e;
        e.attack = a;
        e.decay = d;
        e.sustain = s;
        e.release = r;
        return e;
    };
    std::vector<Preset> presets(3);
    presets[0].name = "PLUCK";
    presets[0].color = render::Color{0.95f, 0.55f, 0.45f, 1.0f};
    presets[0].adsr = mk(0.02f, 0.18f, 0.18f, 0.25f);
    presets[1].name = "PAD";
    presets[1].color = render::Color{0.45f, 0.75f, 0.98f, 1.0f};
    presets[1].adsr = mk(0.40f, 0.20f, 0.80f, 0.55f);
    presets[2].name = "STAB";
    presets[2].color = render::Color{0.75f, 0.6f, 0.98f, 1.0f};
    presets[2].adsr = mk(0.006f, 0.10f, 0.0f, 0.06f);

    // Shared timeline: note-on at 0.12 s, note-off at 0.78 s, 1.2 s total shown.
    const float total = 1.2f, onT = 0.12f, offT = 0.78f;
    const int samples = 360;
    std::vector<std::vector<float>> env(presets.size());
    for (std::size_t i = 0; i < presets.size(); ++i) {
        env[i] = sampleEnvelope(presets[i].adsr, total, onT, offT, samples);
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — ADSR Envelope";
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

    const float colW = 400.0f, colGap = 20.0f, x0 = 30.0f;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ADSR ENVELOPE",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "attack / decay / sustain / release shapes a raw tone into a note - same tone, "
                          "three envelopes",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            for (std::size_t p = 0; p < presets.size(); ++p) {
                const Preset& pr = presets[p];
                const float cx = x0 + static_cast<float>(p) * (colW + colGap);
                const render::Color col = pr.color;

                // Header + ADSR values.
                font.drawText(*renderer, cx + 6.0f, 92.0f, pr.name, col, 0.5f);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "A %.0f  D %.0f  S %.2f  R %.0f ms",
                              pr.adsr.attack * 1000.0f, pr.adsr.decay * 1000.0f, pr.adsr.sustain,
                              pr.adsr.release * 1000.0f);
                font.drawText(*renderer, cx + 6.0f, 122.0f, buf, render::Color{0.7f, 0.74f, 0.82f, 1},
                              0.3f);

                // --- Envelope curve panel. ---
                const float ex = cx + 6.0f, ew = colW - 12.0f;
                const float etop = 156.0f, ebot = 320.0f, eh = ebot - etop;
                fillRect(*renderer, ex, etop, ew, eh, render::Color{0.10f, 0.11f, 0.15f, 1.0f});
                // Gate region (key held) shaded.
                const float gx0 = ex + (onT / total) * ew;
                const float gx1 = ex + (offT / total) * ew;
                fillRect(*renderer, gx0, etop, gx1 - gx0, eh, render::Color{col.r, col.g, col.b, 0.10f});
                // Sustain reference line.
                const float sy = ebot - pr.adsr.sustain * eh;
                fillRect(*renderer, ex, sy - 0.5f, ew, 1.0f, render::Color{0.35f, 0.37f, 0.44f, 1.0f});
                // The envelope curve.
                for (int i = 1; i < samples; ++i) {
                    const float x1 = ex + (static_cast<float>(i - 1) / static_cast<float>(samples)) * ew;
                    const float x2 = ex + (static_cast<float>(i) / static_cast<float>(samples)) * ew;
                    const float y1 = ebot - env[p][static_cast<std::size_t>(i - 1)] * eh;
                    const float y2 = ebot - env[p][static_cast<std::size_t>(i)] * eh;
                    thickLine(*renderer, math::vec2(x1, y1), math::vec2(x2, y2), 2.4f, col);
                }
                // note-on / note-off ticks.
                fillRect(*renderer, gx0 - 1.0f, etop, 2.0f, eh, render::Color{0.55f, 0.85f, 0.55f, 0.7f});
                fillRect(*renderer, gx1 - 1.0f, etop, 2.0f, eh, render::Color{0.9f, 0.55f, 0.5f, 0.7f});

                // --- Shaped-tone waveform panel (the envelope × a sine). ---
                const float wtop = 366.0f, wbot = 590.0f, wmid = (wtop + wbot) * 0.5f, wamp = (wbot - wtop)
                                                                                              * 0.46f;
                fillRect(*renderer, ex, wtop, ew, wbot - wtop, render::Color{0.09f, 0.10f, 0.13f, 1.0f});
                fillRect(*renderer, ex, wmid - 0.5f, ew, 1.0f, render::Color{0.25f, 0.27f, 0.33f, 1.0f});
                const float cyclesPerPanel = 26.0f;
                for (int i = 1; i < samples; ++i) {
                    const float f0 = static_cast<float>(i - 1) / static_cast<float>(samples);
                    const float f1 = static_cast<float>(i) / static_cast<float>(samples);
                    const float s0 = std::sin(f0 * cyclesPerPanel * 6.2831853f) *
                                     env[p][static_cast<std::size_t>(i - 1)];
                    const float s1 = std::sin(f1 * cyclesPerPanel * 6.2831853f) *
                                     env[p][static_cast<std::size_t>(i)];
                    thickLine(*renderer, math::vec2(ex + f0 * ew, wmid - s0 * wamp),
                              math::vec2(ex + f1 * ew, wmid - s1 * wamp), 1.4f,
                              render::Color{col.r, col.g, col.b, 0.95f});
                }
            }

            // Segment legend under the first panel.
            font.drawText(*renderer, x0 + 6.0f, 326.0f,
                          "green tick = key down (attack->decay->sustain)      red tick = key up "
                          "(release)      grey line = sustain level",
                          render::Color{0.65f, 0.7f, 0.8f, 1}, 0.3f);
            font.drawText(*renderer, x0 + 6.0f, 600.0f,
                          "below: a single sine tone multiplied by the envelope above - the note swells "
                          "in, holds, and fades (audio::ADSR)",
                          render::Color{0.65f, 0.7f, 0.8f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ENVELOPE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
