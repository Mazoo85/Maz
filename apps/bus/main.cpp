// Maz Engine — "BUS" (audio DSP effects + mix buses, toward Godot's AudioEffectFilter/Delay + bus layout)
// A signal-processing scope: one rich source "note" (a plucked sawtooth burst — many harmonics, then
// silence) is run through several DSP effects and each result drawn as its own waveform band. You can
// SEE what each effect does: the low-pass rounds off the buzzy harmonics, the high-pass keeps only the
// bright edges, and a bus that chains low-pass -> delay produces a filtered note plus decaying echoes.
// Everything is computed once at startup at 44.1 kHz (physically correct filter math), then decimated
// to the band width and drawn statically — so the render is fully deterministic and golden-stable.

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

// Draw a sample buffer as a waveform inside a horizontal band centred on cy, decimated to ~samples cols.
void drawWave(render::Renderer& r, const std::vector<float>& buf, float x0, float cy, float w, float amp,
              render::Color col) {
    const int cols = 560;
    math::vec2 prev(0.0f, 0.0f);
    bool have = false;
    for (int i = 0; i <= cols; ++i) {
        const float f = static_cast<float>(i) / static_cast<float>(cols);
        const size_t idx =
            buf.empty() ? 0 : static_cast<size_t>(f * static_cast<float>(buf.size() - 1));
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

// Apply a single biquad (fresh state) over a copy of the buffer.
std::vector<float> filtered(const std::vector<float>& src, audio::Biquad f) {
    std::vector<float> out(src.size());
    for (size_t i = 0; i < src.size(); ++i) {
        out[i] = f.process(src[i]);
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BUS (audio DSP effects + mix buses) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Audio DSP Buses";
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

    // ---- Synthesize the source note and process it through the effects (once) -------------------
    const float sr = 44100.0f;
    const int total = 22050; // 0.5 s
    const int noteLen = 5292; // ~0.12 s
    const float noteFreq = 150.0f;
    std::vector<float> src(static_cast<size_t>(total), 0.0f);
    for (int i = 0; i < noteLen; ++i) {
        const float t = static_cast<float>(i) / sr;
        const float ph = t * noteFreq;
        const float saw = 2.0f * (ph - std::floor(ph + 0.5f)); // naive sawtooth, [-1,1], rich harmonics
        const float env = std::exp(-3.0f * static_cast<float>(i) / static_cast<float>(noteLen));
        src[static_cast<size_t>(i)] = saw * env * 0.9f;
    }

    const std::vector<float> low = filtered(src, audio::Biquad::lowpass(600.0f, 0.707f, sr));
    const std::vector<float> high = filtered(src, audio::Biquad::highpass(1500.0f, 0.707f, sr));

    std::vector<float> busOut = src;
    {
        audio::Bus bus;
        bus.add(std::make_unique<audio::BiquadEffect>(audio::Biquad::lowpass(1200.0f, 0.707f, sr)));
        audio::Delay echo;
        echo.configure(static_cast<int>(0.09f * sr), 0.55f, 0.9f); // ~90 ms echo, decaying
        bus.add(std::make_unique<audio::DelayEffect>(echo));
        bus.processBuffer(busOut);
    }

    struct Band {
        const std::vector<float>* buf;
        render::Color col;
        const char* label;
    };
    const Band bands[4] = {
        {&src, rgb(0.75f, 0.78f, 0.85f), "SOURCE  -  plucked sawtooth (rich harmonics, then silence)"},
        {&low, rgb(0.40f, 0.85f, 1.00f), "LOW-PASS 600 Hz  -  harmonics smoothed away (AudioEffectFilter)"},
        {&high, rgb(1.00f, 0.62f, 0.35f), "HIGH-PASS 1.5 kHz  -  only the bright edges remain"},
        {&busOut, rgb(0.55f, 1.00f, 0.55f), "BUS: low-pass -> delay  -  filtered note + decaying echoes"},
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
                const float amp = bandH * 0.38f;
                // Panel background + zero line.
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  AUDIO DSP EFFECTS + MIX BUSES",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 52.0f,
                          "one source note routed through biquad filters and a low-pass -> delay bus",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BUS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
