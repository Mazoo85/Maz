// Maz Engine — "FLOATCURVE" (anim::Curve float-curve resource, toward Godot's Curve)
// A Curve maps one scalar to another (y = f(x) over [0,1]) — the editable shape behind particle size/alpha
// over lifetime, audio fades, and custom easing. This is NOT a path through space (that's math::Curve2D);
// it is a value profile. The demo plots four curves — a linear ramp, a cubic ease-in-out, a cubic ease-out,
// and a multi-point "particle size" profile — each sampled densely with its control points marked, all from
// anim::Curve::sample. Fixed curves -> deterministic, golden-stable. Run --headless / --frames N for CI.

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

void dot(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 14;
    render::Point2 pts[14];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

struct Panel {
    std::string title;
    anim::Curve curve;
    render::Color color;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("FLOATCURVE (anim::Curve) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Float Curve";
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

    // ---- Author the four curves. ---------------------------------------------------------------------
    std::vector<Panel> panels;
    {
        anim::Curve linear;
        linear.interp = anim::CurveInterp::Linear;
        linear.addPoint(0.0f, 0.0f);
        linear.addPoint(1.0f, 1.0f);
        panels.push_back({"linear", linear, rgba(0.55f, 0.8f, 1.0f, 1)});

        anim::Curve easeInOut;
        easeInOut.interp = anim::CurveInterp::Cubic;
        easeInOut.addPoint(0.0f, 0.0f, 0.0f, 0.0f);
        easeInOut.addPoint(1.0f, 1.0f, 0.0f, 0.0f);
        panels.push_back({"ease in-out (cubic, flat tangents)", easeInOut, rgba(0.6f, 0.95f, 0.65f, 1)});

        anim::Curve easeOut;
        easeOut.interp = anim::CurveInterp::Cubic;
        easeOut.addPoint(0.0f, 0.0f, 0.0f, 2.4f); // steep leaving 0
        easeOut.addPoint(1.0f, 1.0f, 0.0f, 0.0f); // flat arriving 1
        panels.push_back({"ease out (fast start, slow finish)", easeOut, rgba(1.0f, 0.8f, 0.45f, 1)});

        anim::Curve size;
        size.interp = anim::CurveInterp::Cubic;
        size.addPoint(0.0f, 0.0f, 0.0f, 0.0f);
        size.addPoint(0.2f, 1.0f, 0.0f, 0.0f);
        size.addPoint(1.0f, 0.15f, 0.0f, 0.0f);
        panels.push_back({"particle size over life", size, rgba(1.0f, 0.6f, 0.75f, 1)});
    }

    const render::Color kFrame{0.28f, 0.31f, 0.38f, 1.0f};
    const render::Color kPanel{0.12f, 0.13f, 0.17f, 1.0f};

    const float pw = 500.0f, ph = 220.0f;
    const float colX[2] = {90.0f, 690.0f};
    const float rowY[2] = {130.0f, 420.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  FLOAT CURVE", rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "value-over-parameter shapes sampled from anim::Curve (Godot Curve): linear / "
                          "cubic easing / a multi-point profile",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.32f);

            for (std::size_t i = 0; i < panels.size(); ++i) {
                const Panel& p = panels[i];
                const float x0 = colX[i % 2];
                const float y0 = rowY[i / 2];
                const float y1 = y0 + ph;

                // Panel background + frame.
                const render::Point2 bg[4] = {{x0, y0}, {x0 + pw, y0}, {x0 + pw, y1}, {x0, y1}};
                renderer->drawConvexPolygon(bg, 4, kPanel);
                thickLine(*renderer, {x0, y1}, {x0 + pw, y1}, 1.5f, kFrame); // x axis
                thickLine(*renderer, {x0, y0}, {x0, y1}, 1.5f, kFrame);      // y axis

                // Plot the curve.
                const int cols = 140;
                math::vec2 prev(0.0f, 0.0f);
                for (int s = 0; s <= cols; ++s) {
                    const float x = static_cast<float>(s) / static_cast<float>(cols);
                    const float v = p.curve.sample(x);
                    const math::vec2 pt(x0 + x * pw, y1 - v * ph);
                    if (s > 0) {
                        thickLine(*renderer, prev, pt, 2.5f, p.color);
                    }
                    prev = pt;
                }

                // Control points.
                for (std::size_t k = 0; k < p.curve.pointCount(); ++k) {
                    const anim::CurvePoint& cp = p.curve.point(k);
                    dot(*renderer, math::vec2(x0 + cp.pos * pw, y1 - cp.value * ph), 4.0f,
                        rgba(1, 1, 1, 0.9f));
                }

                font.drawText(*renderer, x0 + 8.0f, y0 + 8.0f, p.title.c_str(), p.color, 0.32f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("FLOATCURVE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
