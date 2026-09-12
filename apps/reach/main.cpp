// Maz Engine — "REACH" (2-bone inverse kinematics, toward Godot's SkeletonModification2DTwoBoneIK)
// A grid of two-bone arms. Each arm has a fixed shoulder (root) and two bone lengths; anim::solveTwoBoneIK
// places the elbow (via the law of cosines) so the hand lands on that cell's target. Targets fan out at
// varying angles and distances, and the elbow bend direction alternates, so you can read the solver
// working across the reachable space — including a few targets placed out of reach, where the arm points
// straight at them, fully extended. Static + deterministic, so the render is golden-stable. --headless/--frames.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
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

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col, int sides = 18) {
    std::vector<render::Point2> p(static_cast<size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        p[static_cast<size_t>(i)] = render::Point2{c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p.data(), static_cast<uint32_t>(sides), col);
}

void ringOutline(render::Renderer& r, math::vec2 c, float rad, float thick, render::Color col,
                 int sides = 20) {
    for (int i = 0; i < sides; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(sides);
        thickLine(r, math::vec2(c.x + std::cos(a0) * rad, c.y + std::sin(a0) * rad),
                  math::vec2(c.x + std::cos(a1) * rad, c.y + std::sin(a1) * rad), thick, col);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("REACH (2-bone IK) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2-Bone IK";
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

    const float sw = static_cast<float>(cfg.width);
    const float sh = static_cast<float>(cfg.height);
    const int cols = 6, rows = 3;
    const float marginX = 110.0f, top = 150.0f, bot = 70.0f;
    const float cellW = (sw - 2.0f * marginX) / static_cast<float>(cols);
    const float cellH = (sh - top - bot) / static_cast<float>(rows);
    const float l1 = 52.0f, l2 = 44.0f; // upper, lower bone lengths

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            for (int cy = 0; cy < rows; ++cy) {
                for (int cx = 0; cx < cols; ++cx) {
                    const math::vec2 root(marginX + (static_cast<float>(cx) + 0.5f) * cellW,
                                          top + (static_cast<float>(cy) + 0.55f) * cellH);
                    // Deterministic target: fan angle across columns, distance grows down the rows
                    // (bottom row overreaches a couple of cells -> straight arms).
                    const float ang = -2.15f + static_cast<float>(cx) * 0.24f + static_cast<float>(cy) * 0.06f;
                    const float dist = (l1 + l2) * (0.55f + 0.18f * static_cast<float>(cy) +
                                                    0.05f * static_cast<float>(cx));
                    const math::vec2 target(root.x + std::cos(ang) * dist,
                                            root.y + std::sin(ang) * dist);
                    const float bend = ((cx + cy) % 2 == 0) ? 1.0f : -1.0f;

                    const anim::IKResult s = anim::solveTwoBoneIK(root, l1, l2, target, bend);

                    const float hx = static_cast<float>(cx) / static_cast<float>(cols - 1);
                    const render::Color upper{0.95f, 0.6f + 0.25f * hx, 0.4f, 1.0f};
                    const render::Color lower{0.45f, 0.7f, 0.95f, 1.0f};
                    const render::Color tgtCol =
                        s.reachable ? render::Color{0.55f, 0.9f, 0.6f, 1} : render::Color{0.95f, 0.45f, 0.45f, 1};

                    thickLine(*renderer, root, s.mid, 8.0f, upper);
                    thickLine(*renderer, s.mid, s.end, 7.0f, lower);
                    fillCircle(*renderer, root, 6.5f, render::Color{0.85f, 0.87f, 0.95f, 1}); // shoulder
                    fillCircle(*renderer, s.mid, 5.0f, render::Color{1.0f, 0.95f, 0.75f, 1});  // elbow
                    ringOutline(*renderer, target, 9.0f, 2.0f, tgtCol);                        // target
                    fillCircle(*renderer, s.end, 4.0f, render::Color{1, 1, 1, 1});             // hand
                }
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2-BONE INVERSE KINEMATICS",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "each arm's elbow is solved (law of cosines) so the hand reaches its target",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("REACH shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
