// Maz Engine — "INTERP" (fixed-timestep render interpolation, toward Godot's physics interpolation)
// Physics runs on the fixed step; the display refreshes between steps. Drawing the raw state stutters, so
// we keep the PREVIOUS and CURRENT physics pose and blend them by the frame's alpha (Clock::interpolation-
// Alpha). This demo freezes one alpha (0.35) and, for four motions — translate, rotate, scale, combined —
// draws the previous pose (faint) and current pose (faint) as ghosts with the interpolated pose (solid)
// sitting between them, via core::interpolate(Transform2DState). Fixed poses -> deterministic golden.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>

using namespace maz;

namespace {

// Draw an axis-square (half-size `h`) transformed by a Transform2DState, filled with colour `c`.
void drawState(render::Renderer& r, const core::Transform2DState& s, float h, render::Color c) {
    const float cs = std::cos(s.rotation);
    const float sn = std::sin(s.rotation);
    const float hx = h * s.scale.x;
    const float hy = h * s.scale.y;
    const math::vec2 local[4] = {{-hx, -hy}, {hx, -hy}, {hx, hy}, {-hx, hy}};
    render::Point2 p[4];
    for (int i = 0; i < 4; ++i) {
        p[i] = {s.position.x + local[i].x * cs - local[i].y * sn,
                s.position.y + local[i].x * sn + local[i].y * cs};
    }
    r.drawConvexPolygon(p, 4, c);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("INTERP (render interpolation) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Render Interpolation";
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

    const float alpha = 0.35f;

    struct Panel {
        std::string title;
        core::Transform2DState prev;
        core::Transform2DState cur;
    };

    auto st = [](float x, float y, float rot, float sc) {
        core::Transform2DState s;
        s.position = math::vec2(x, y);
        s.rotation = rot;
        s.scale = math::vec2(sc, sc);
        return s;
    };

    Panel panels[4] = {
        {"translate", st(200.0f, 250.0f, 0.0f, 1.0f), st(500.0f, 250.0f, 0.0f, 1.0f)},
        {"rotate", st(950.0f, 250.0f, -0.5f, 1.0f), st(950.0f, 250.0f, 1.4f, 1.0f)},
        {"scale", st(340.0f, 520.0f, 0.0f, 0.6f), st(340.0f, 520.0f, 0.0f, 1.7f)},
        {"combined", st(760.0f, 520.0f, -0.4f, 0.8f), st(1060.0f, 520.0f, 1.1f, 1.3f)},
    };

    const render::Color kGhost{0.55f, 0.60f, 0.72f, 0.28f};
    const render::Color kSolid{0.45f, 0.85f, 1.0f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  RENDER INTERPOLATION", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "faint = previous & current fixed-step poses; solid = interpolated at alpha 0.35 "
                          "(core::interpolate / Interpolated<T>)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.32f);

            const float half = 46.0f;
            for (const Panel& p : panels) {
                // Ghost the two endpoint poses, then the interpolated pose on top.
                drawState(*renderer, p.prev, half, kGhost);
                drawState(*renderer, p.cur, half, kGhost);
                const core::Transform2DState mid = core::interpolate(p.prev, p.cur, alpha);
                drawState(*renderer, mid, half, kSolid);

                // Title under the panel's current pose.
                font.drawText(*renderer, p.cur.position.x - 40.0f, p.cur.position.y + 96.0f,
                              p.title.c_str(), rgba(0.7f, 0.76f, 0.86f, 1), 0.34f);
            }

            font.drawText(*renderer, 40.0f, 664.0f,
                          "rotation blends the SHORTEST arc; the solid pose sits 35% of the way from "
                          "previous to current",
                          rgba(0.6f, 0.64f, 0.72f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("INTERP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
