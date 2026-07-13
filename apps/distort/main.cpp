// Maz Engine — "DISTORT" (audio::MultiDistortion, toward Godot's AudioEffectDistortion.Mode)
// A9 (final audio milestone): the full distortion-mode set. The LEFT chart is the transfer curve of each
// mode — output (y) vs input (x) over [-1,1], the fingerprint of a waveshaper: the faint diagonal is
// "no change", Clip is a hard flat-top brick wall, Tanh/ATan are smooth soft-clip S-curves, LoFi is a
// bit-crush staircase, and Overdrive is an asymmetric curve (top and bottom differ). The RIGHT column
// shows a sine wave pushed through each mode so you can see how the shape mangles the tone. Analytic +
// deterministic -> golden-stable. Run --headless / --frames N for CI.

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

struct ModeInfo {
    audio::DistortionMode mode;
    std::string label;
    render::Color color;
};

audio::MultiDistortion make(audio::DistortionMode m, float drive) {
    audio::MultiDistortion md;
    md.mode = m;
    md.drive = drive;
    md.bits = 3;
    md.rateHz = 44100.0f; // pure bit-crush for a clean deterministic transfer/scope
    md.sampleRate = 44100.0f;
    return md;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("DISTORT (audio::MultiDistortion) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Distortion Modes";
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

    const std::vector<ModeInfo> modes = {
        {audio::DistortionMode::Tanh, "Tanh", rgba(0.55f, 0.82f, 1.0f, 1)},
        {audio::DistortionMode::Clip, "Clip", rgba(1.0f, 0.5f, 0.45f, 1)},
        {audio::DistortionMode::ATan, "ATan", rgba(0.55f, 0.95f, 0.6f, 1)},
        {audio::DistortionMode::LoFi, "LoFi", rgba(1.0f, 0.75f, 0.4f, 1)},
        {audio::DistortionMode::Overdrive, "Overdrive", rgba(0.85f, 0.6f, 1.0f, 1)},
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  DISTORTION MODES", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::MultiDistortion (Godot AudioEffectDistortion.Mode): transfer curves "
                          "(left) + a sine through each mode (right).",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // ---- Transfer-curve chart (left) ----
            const float cx0 = 90.0f, cx1 = 520.0f, cy0 = 110.0f, cy1 = 540.0f;
            const float ccx = (cx0 + cx1) * 0.5f, ccy = (cy0 + cy1) * 0.5f;
            const float halfW = (cx1 - cx0) * 0.5f, halfH = (cy1 - cy0) * 0.5f;
            // Frame + axes.
            thickLine(*renderer, {cx0, cy0}, {cx0, cy1}, 1.0f, rgba(0.25f, 0.28f, 0.35f, 1));
            thickLine(*renderer, {cx0, cy1}, {cx1, cy1}, 1.0f, rgba(0.25f, 0.28f, 0.35f, 1));
            thickLine(*renderer, {cx0, ccy}, {cx1, ccy}, 1.0f, rgba(0.3f, 0.33f, 0.42f, 1));
            thickLine(*renderer, {ccx, cy0}, {ccx, cy1}, 1.0f, rgba(0.3f, 0.33f, 0.42f, 1));
            // Identity diagonal (no change).
            thickLine(*renderer, {cx0, cy1}, {cx1, cy0}, 1.0f, rgba(0.4f, 0.42f, 0.5f, 0.6f));
            font.drawText(*renderer, cx0, cy0 - 26.0f, "transfer curve  (out vs in)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            const int NP = 400;
            for (const ModeInfo& mi : modes) {
                audio::MultiDistortion md = make(mi.mode, mi.mode == audio::DistortionMode::LoFi ? 1.0f
                                                                                                 : 2.5f);
                math::vec2 prev(0, 0);
                bool have = false;
                for (int i = 0; i <= NP; ++i) {
                    const float x = -1.0f + 2.0f * static_cast<float>(i) / static_cast<float>(NP);
                    float y = md.process(x);
                    if (y > 1.2f) {
                        y = 1.2f;
                    }
                    if (y < -1.2f) {
                        y = -1.2f;
                    }
                    const math::vec2 p(ccx + x * halfW, ccy - y * halfH);
                    if (have) {
                        thickLine(*renderer, prev, p, 1.8f, mi.color);
                    }
                    prev = p;
                    have = true;
                }
            }

            // ---- Distorted sine per mode (right column) ----
            const float sx0 = 600.0f, sx1 = 1210.0f;
            const float rowTop = 96.0f, rowH = 96.0f, rowGap = 14.0f;
            for (std::size_t k = 0; k < modes.size(); ++k) {
                const ModeInfo& mi = modes[k];
                const float baseY = rowTop + static_cast<float>(k) * (rowH + rowGap) + rowH * 0.5f;
                thickLine(*renderer, {sx0, baseY}, {sx1, baseY}, 1.0f, rgba(0.24f, 0.26f, 0.33f, 1));

                audio::MultiDistortion md = make(mi.mode, 4.0f);
                const int NS = 480;
                math::vec2 prev(0, 0);
                bool have = false;
                for (int i = 0; i < NS; ++i) {
                    const float in = std::sin(2.0f * 3.14159265f * 3.0f * static_cast<float>(i) /
                                              static_cast<float>(NS));
                    const float y = md.process(in);
                    const float px = sx0 + (sx1 - sx0) * static_cast<float>(i) / static_cast<float>(NS - 1);
                    const float py = baseY - y * (rowH * 0.42f);
                    const math::vec2 p(px, py);
                    if (have) {
                        thickLine(*renderer, prev, p, 1.6f, mi.color);
                    }
                    prev = p;
                    have = true;
                }
                font.drawText(*renderer, sx0, baseY - rowH * 0.5f - 4.0f, mi.label.c_str(), mi.color,
                              0.3f);
            }

            // Legend under the transfer chart.
            for (std::size_t k = 0; k < modes.size(); ++k) {
                const float lx = cx0 + static_cast<float>(k) * 88.0f;
                const float ly = cy1 + 20.0f;
                thickLine(*renderer, {lx, ly + 6.0f}, {lx + 20.0f, ly + 6.0f}, 3.0f, modes[k].color);
                font.drawText(*renderer, lx + 26.0f, ly - 2.0f, modes[k].label.c_str(), modes[k].color,
                              0.26f);
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "flat-top = hard clip; smooth S = soft clip; staircase = bit-crush LoFi; "
                          "lopsided = asymmetric overdrive",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("DISTORT shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
