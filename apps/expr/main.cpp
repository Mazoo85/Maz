// Maz Engine — "EXPR" (core::Expression runtime formula parser/evaluator, toward Godot's Expression)
// Data-driven math: parse a formula STRING once, then evaluate it thousands of times. LEFT: three curves
// each PARSED FROM TEXT ("sin(x*tau*2)", a damped "sin(x*tau*4)*exp(-x*3)", a clipped
// "clamp(sin(x*tau)+0.4,-1,1)") and plotted by sampling f(x) across the panel — the engine never sees the
// formulas at compile time. RIGHT: a "formula card" showing a designer-style damage rule
// "base*(1+rate*lvl)" evaluated with named variables to a concrete number + bar. Fixed inputs ->
// deterministic, golden-stable. Run --headless / --frames N for CI.

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

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

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

void panelBox(render::Renderer& r, float x0, float y0, float x1, float y1, render::Color c) {
    const render::Point2 q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    r.drawConvexPolygon(q, 4, c);
}

// Plot y = f(x) for x in [0,1] mapped into a rectangle; y in [-1,1] maps top..bottom (y up on screen).
void plot(render::Renderer& r, const core::Expression& e, float x0, float y0, float x1, float y1,
          render::Color col) {
    const int N = 220;
    math::vec2 prev;
    bool have = false;
    for (int i = 0; i <= N; ++i) {
        const float fx = static_cast<float>(i) / static_cast<float>(N);
        double y = e.execute({fx});
        if (y > 1.0) {
            y = 1.0;
        }
        if (y < -1.0) {
            y = -1.0;
        }
        const float sx = x0 + fx * (x1 - x0);
        const float cy = (y0 + y1) * 0.5f;
        const float sy = cy - static_cast<float>(y) * (y1 - y0) * 0.5f;
        const math::vec2 p(sx, sy);
        if (have) {
            thickLine(r, prev, p, 2.5f, col);
        }
        prev = p;
        have = true;
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("EXPR (core::Expression) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Expression";
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

    // Parse the plotted formulas up front (once). Variable: x in [0,1].
    struct Curve {
        std::string text;
        core::Expression expr;
        render::Color col;
    };
    std::vector<Curve> curves;
    curves.push_back({"sin(x*tau*2)", {}, rgba(0.45f, 0.8f, 1.0f, 1)});
    curves.push_back({"sin(x*tau*4)*exp(-x*3)", {}, rgba(1.0f, 0.72f, 0.3f, 1)});
    curves.push_back({"clamp(sin(x*tau)+0.4,-1,1)", {}, rgba(0.55f, 0.95f, 0.6f, 1)});
    for (Curve& c : curves) {
        c.expr.parse(c.text, {"x"});
    }

    // Formula card: a designer damage rule, evaluated with named variables.
    core::Expression dmg;
    dmg.parse("base*(1+rate*lvl)", {"base", "rate", "lvl"});
    const double dmgBase = 10.0, dmgRate = 0.5, dmgLvl = 4.0;
    const double dmgValue = dmg.execute({dmgBase, dmgRate, dmgLvl}); // 10*(1+0.5*4)=30

    char dmgStr[64];
    std::snprintf(dmgStr, sizeof(dmgStr), "= %.1f", dmgValue);

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  EXPRESSION", rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "runtime formula parser + evaluator (core::Expression, Godot Expression)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // ---- LEFT: function plotter -------------------------------------------------------
            const float px0 = 40.0f, py0 = 120.0f, px1 = 820.0f, py1 = 600.0f;
            panelBox(*renderer, px0, py0, px1, py1, rgba(0.12f, 0.13f, 0.17f, 1));
            // Axes: x baseline (y=0) and left edge.
            const float midY = (py0 + py1) * 0.5f;
            thickLine(*renderer, math::vec2(px0 + 10, midY), math::vec2(px1 - 10, midY), 1.5f,
                      rgba(0.3f, 0.34f, 0.42f, 1));
            for (int g = 1; g < 8; ++g) {
                const float gx = px0 + 10 + (px1 - px0 - 20) * static_cast<float>(g) / 8.0f;
                thickLine(*renderer, math::vec2(gx, py0 + 14), math::vec2(gx, py1 - 14), 1.0f,
                          rgba(0.18f, 0.2f, 0.26f, 1));
            }
            for (std::size_t i = 0; i < curves.size(); ++i) {
                plot(*renderer, curves[i].expr, px0 + 14, py0 + 20, px1 - 14, py1 - 20, curves[i].col);
                font.drawText(*renderer, px0 + 20.0f, py0 + 12.0f + static_cast<float>(i) * 30.0f,
                              curves[i].text.c_str(), curves[i].col, 0.2f);
            }
            font.drawText(*renderer, px0 + 8.0f, 96.0f, "y = f(x) parsed from text, sampled over x",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            // ---- RIGHT: formula card ----------------------------------------------------------
            const float cx0 = 860.0f, cy0 = 120.0f, cx1 = 1240.0f, cy1 = 600.0f;
            panelBox(*renderer, cx0, cy0, cx1, cy1, rgba(0.12f, 0.13f, 0.17f, 1));
            font.drawText(*renderer, cx0 + 8.0f, 96.0f, "data-driven formula card",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);
            font.drawText(*renderer, cx0 + 24.0f, cy0 + 40.0f, "damage rule:", rgba(0.7f, 0.75f, 0.85f, 1),
                          0.25f);
            font.drawText(*renderer, cx0 + 24.0f, cy0 + 78.0f, "base*(1+rate*lvl)",
                          rgba(0.55f, 0.8f, 1.0f, 1), 0.35f);
            font.drawText(*renderer, cx0 + 24.0f, cy0 + 140.0f, "base = 10", rgba(0.85f, 0.88f, 0.95f, 1),
                          0.25f);
            font.drawText(*renderer, cx0 + 24.0f, cy0 + 172.0f, "rate = 0.5",
                          rgba(0.85f, 0.88f, 0.95f, 1), 0.25f);
            font.drawText(*renderer, cx0 + 24.0f, cy0 + 204.0f, "lvl  = 4", rgba(0.85f, 0.88f, 0.95f, 1),
                          0.25f);
            font.drawText(*renderer, cx0 + 24.0f, cy0 + 268.0f, dmgStr, rgba(1.0f, 0.72f, 0.3f, 1), 0.6f);
            // A bar scaled to the value (0..40).
            const float barW = static_cast<float>(dmgValue) / 40.0f * (cx1 - cx0 - 48.0f);
            panelBox(*renderer, cx0 + 24.0f, cy0 + 320.0f, cx0 + 24.0f + barW, cy0 + 352.0f,
                     rgba(1.0f, 0.72f, 0.3f, 1));
            panelBox(*renderer, cx0 + 24.0f + barW, cy0 + 320.0f, cx1 - 24.0f, cy0 + 352.0f,
                     rgba(0.2f, 0.22f, 0.28f, 1));

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("EXPR shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
