// Maz Engine — "STEREO" (audio::StereoEnhance + Panner, toward Godot's AudioEffectStereoEnhance/Panner)
// A5 of the audio deep-dive: stereo width + balance. This demo draws a GONIOMETER (a vectorscope, the
// stereo-metering plot audio engineers read): each L/R sample is plotted with the mono/mid content on
// the vertical axis and the stereo/side difference on the horizontal axis. A stereo signal traces a
// tilted ellipse; collapsing the width squashes it toward the vertical (mono) line, widening spreads it
// horizontally, and panning shifts the whole figure to one side. Four panels show the same source under:
// original width, narrowed (w=0.35), widened (w=1.8), and hard-ish pan (+0.6). Analytic + deterministic
// -> golden-stable. Run --headless / --frames N for CI.

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

const float kInvSqrt2 = 0.70710678f;

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("STEREO (audio::StereoEnhance + Panner) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Stereo Width & Pan";
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

    // A stereo source with real stereo content: R is a phase-shifted copy of L -> a tilted ellipse.
    const int N = 700;
    std::vector<audio::StereoFrame> src;
    src.reserve(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        const float th = 2.0f * 3.14159265f * 3.0f * static_cast<float>(i) / static_cast<float>(N);
        src.push_back(audio::StereoFrame{0.85f * std::sin(th), 0.85f * std::sin(th - 0.7f)});
    }

    struct Panel {
        std::string label;
        std::vector<audio::StereoFrame> pts;
        render::Color color;
    };
    std::vector<Panel> panels;
    {
        audio::StereoEnhance w1;
        w1.configure(1.0f, 0.0f, 44100.0f);
        audio::StereoEnhance wn;
        wn.configure(0.35f, 0.0f, 44100.0f);
        audio::StereoEnhance ww;
        ww.configure(1.8f, 0.0f, 44100.0f);
        audio::Panner pn;
        pn.pan = 0.6f;
        Panel p0{"original (w=1)", {}, rgba(0.55f, 0.85f, 1.0f, 1)};
        Panel p1{"narrow (w=0.35)", {}, rgba(0.6f, 0.95f, 0.6f, 1)};
        Panel p2{"wide (w=1.8)", {}, rgba(1.0f, 0.7f, 0.4f, 1)};
        Panel p3{"pan +0.6", {}, rgba(1.0f, 0.55f, 0.75f, 1)};
        for (const audio::StereoFrame& f : src) {
            p0.pts.push_back(w1.process(f));
            p1.pts.push_back(wn.process(f));
            p2.pts.push_back(ww.process(f));
            p3.pts.push_back(pn.process(f));
        }
        panels = {p0, p1, p2, p3};
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  STEREO WIDTH & PAN (goniometer)",
                          rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::StereoEnhance + Panner (Godot AudioEffectStereoEnhance/Panner): mono "
                          "content is vertical, stereo width is horizontal spread.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            const float scale = 120.0f;
            const float cy = 350.0f;
            for (std::size_t i = 0; i < panels.size(); ++i) {
                const Panel& p = panels[i];
                const float cx = 200.0f + static_cast<float>(i) * 300.0f;

                // Reference frame: vertical (mono) axis, horizontal (side) axis, and the ±1 diamond.
                thickLine(*renderer, {cx, cy - scale * 1.5f}, {cx, cy + scale * 1.5f}, 1.0f,
                          rgba(0.28f, 0.31f, 0.4f, 1));
                thickLine(*renderer, {cx - scale * 1.5f, cy}, {cx + scale * 1.5f, cy}, 1.0f,
                          rgba(0.28f, 0.31f, 0.4f, 1));
                const math::vec2 top{cx, cy - scale * kInvSqrt2 * 2.0f};
                const math::vec2 rgt{cx + scale * kInvSqrt2 * 2.0f, cy};
                const math::vec2 bot{cx, cy + scale * kInvSqrt2 * 2.0f};
                const math::vec2 lft{cx - scale * kInvSqrt2 * 2.0f, cy};
                const render::Color frame = rgba(0.22f, 0.24f, 0.3f, 1);
                thickLine(*renderer, top, rgt, 1.0f, frame);
                thickLine(*renderer, rgt, bot, 1.0f, frame);
                thickLine(*renderer, bot, lft, 1.0f, frame);
                thickLine(*renderer, lft, top, 1.0f, frame);
                font.drawText(*renderer, cx - 12.0f, cy - scale * 1.5f - 22.0f, "M",
                              rgba(0.5f, 0.55f, 0.65f, 1), 0.28f);

                // The goniometer trace: X = side = (L-R)/sqrt2, Y = mid = (L+R)/sqrt2.
                math::vec2 prev(0, 0);
                bool have = false;
                for (const audio::StereoFrame& f : p.pts) {
                    const float x = (f.left - f.right) * kInvSqrt2;
                    const float y = (f.left + f.right) * kInvSqrt2;
                    const math::vec2 sp(cx + x * scale, cy - y * scale);
                    if (have) {
                        thickLine(*renderer, prev, sp, 1.6f, p.color);
                    }
                    prev = sp;
                    have = true;
                }

                font.drawText(*renderer, cx - 80.0f, cy + scale * 1.5f + 14.0f, p.label.c_str(), p.color,
                              0.32f);
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "vertical line = mono; a wider horizontal spread = wider stereo; the tilt/shift "
                          "shows the balance",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("STEREO shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
