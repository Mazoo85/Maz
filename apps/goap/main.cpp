// Maz Engine — "GOAP" (goal-oriented action planning, toward planner-grade AI beyond Godot's BT)
// A survival agent is given only a GOAL ("have fire") and a LIBRARY of actions with preconditions,
// effects and costs — no hand-authored tree. game::goap::plan runs A* over world-states and returns the
// CHEAPEST action sequence. This demo draws the whole thing: on the left, the action library (the two
// actions the plan skips are dimmed, including an expensive "scavenge wood" decoy); along the bottom, the
// computed plan as a left-to-right flow, START -> action -> ... -> GOAL, with the five boolean facts drawn
// as a dot-strip under each step so you can watch the world-state change fact-by-fact until fire is lit.
// The plan is solved ONCE at startup and drawn statically, so the render is deterministic and golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace maz;
namespace goap = maz::game::goap;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

void outlinedRect(render::Renderer& r, float x, float y, float w, float h, render::Color fill,
                  render::Color line, float t) {
    fillRect(r, x - t, y - t, w + 2 * t, h + 2 * t, line);
    fillRect(r, x, y, w, h, fill);
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

// The five world facts, in bit order.
enum { HasAxe = 0, AtForest = 1, HasWood = 2, AtCamp = 3, HasFire = 4 };
const char* kFactNames[5] = {"axe", "forest", "wood", "camp", "FIRE"};

// Draw the 5-fact state as a labelled dot strip (green = true, dark = false) centred at (cx, y).
void drawState(render::Renderer& r, ui::Font& font, float cx, float y, goap::State s) {
    const float spacing = 46.0f;
    const float x0 = cx - spacing * 2.0f;
    for (int i = 0; i < 5; ++i) {
        const bool on = (s & goap::bit(i)) != 0;
        const float x = x0 + spacing * static_cast<float>(i);
        const render::Color col = on ? render::Color{0.32f, 0.80f, 0.44f, 1.0f}
                                      : render::Color{0.20f, 0.22f, 0.28f, 1.0f};
        outlinedRect(r, x - 8.0f, y - 8.0f, 16.0f, 16.0f, col,
                     render::Color{0.08f, 0.09f, 0.12f, 1.0f}, 2.0f);
        const render::Color txt = on ? render::Color{0.75f, 0.95f, 0.80f, 1.0f}
                                     : render::Color{0.45f, 0.48f, 0.55f, 1.0f};
        font.drawText(r, x - 18.0f, y + 12.0f, kFactNames[i], txt, 0.28f);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("GOAP (goal-oriented action planning) starting");

    // --- Build the action library and plan once. -----------------------------------------------------
    std::vector<goap::Action> lib;
    lib.push_back(goap::Action{"Get Axe"}.sets(HasAxe).withCost(2.0f));
    lib.push_back(goap::Action{"Go to Forest"}.sets(AtForest).clears(AtCamp).withCost(1.0f));
    lib.push_back(
        goap::Action{"Chop Wood"}.needs(HasAxe, true).needs(AtForest, true).sets(HasWood).withCost(3.0f));
    lib.push_back(goap::Action{"Go to Camp"}.sets(AtCamp).clears(AtForest).withCost(1.0f));
    lib.push_back(
        goap::Action{"Build Fire"}.needs(HasWood, true).needs(AtCamp, true).sets(HasFire).withCost(1.0f));
    lib.push_back(goap::Action{"Scavenge Wood"}.sets(HasWood).withCost(10.0f)); // pricey decoy

    const goap::State start = goap::bit(AtCamp);
    const goap::Condition goal = goap::Condition{}.require(HasFire, true);
    const goap::Plan result = goap::plan(start, goal, lib);

    // Which library actions the plan uses (for dimming the rest).
    std::vector<bool> used(lib.size(), false);
    for (int idx : result.steps) {
        used[static_cast<std::size_t>(idx)] = true;
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — GOAP Planner";
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  GOAP  (GOAL-ORIENTED ACTION PLANNER)",
                          render::Color{1, 1, 1, 1}, 0.62f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "goal: HAVE FIRE.  the agent plans the cheapest action sequence itself - no "
                          "hand-authored tree",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            // --- Action library (left column). ---------------------------------------------------------
            font.drawText(*renderer, 40.0f, 96.0f, "ACTION LIBRARY", render::Color{0.7f, 0.8f, 1.0f, 1},
                          0.44f);
            const float ax = 40.0f, aw = 300.0f, ah = 46.0f;
            float ay = 128.0f;
            for (std::size_t i = 0; i < lib.size(); ++i) {
                const bool u = used[i];
                const render::Color fill = u ? render::Color{0.20f, 0.34f, 0.52f, 1.0f}
                                             : render::Color{0.15f, 0.16f, 0.20f, 1.0f};
                const render::Color line = u ? render::Color{0.45f, 0.70f, 1.0f, 1.0f}
                                             : render::Color{0.24f, 0.26f, 0.32f, 1.0f};
                outlinedRect(*renderer, ax, ay, aw, ah, fill, line, 2.0f);
                const render::Color txt =
                    u ? render::Color{0.95f, 0.97f, 1.0f, 1} : render::Color{0.55f, 0.58f, 0.66f, 1};
                font.drawText(*renderer, ax + 12.0f, ay + 8.0f, lib[i].name.c_str(), txt, 0.40f);
                const std::string cost = "cost " + std::to_string(static_cast<int>(lib[i].cost));
                font.drawText(*renderer, ax + aw - 76.0f, ay + 10.0f, cost.c_str(), txt, 0.34f);
                if (!u) {
                    font.drawText(*renderer, ax + aw + 12.0f, ay + 12.0f, "(skipped)",
                                  render::Color{0.5f, 0.45f, 0.42f, 1}, 0.32f);
                }
                ay += ah + 10.0f;
            }

            // --- The plan (right side + bottom flow). ---------------------------------------------------
            font.drawText(*renderer, 470.0f, 96.0f, "OPTIMAL PLAN", render::Color{0.7f, 1.0f, 0.8f, 1},
                          0.44f);
            {
                const std::string sum = "found " + std::to_string(result.steps.size()) +
                                        " steps,  total cost " +
                                        std::to_string(static_cast<int>(result.cost));
                font.drawText(*renderer, 680.0f, 100.0f, sum.c_str(), render::Color{0.75f, 0.82f, 0.92f, 1},
                              0.38f);
            }

            // Horizontal flow of step boxes with the world-state strip under each node.
            const float bw = 132.0f, bh = 58.0f, gap = 34.0f;
            const float rowY = 486.0f;
            float x = 62.0f;
            goap::State s = start;

            // START node.
            outlinedRect(*renderer, x, rowY, bw, bh, render::Color{0.16f, 0.20f, 0.26f, 1},
                         render::Color{0.4f, 0.44f, 0.52f, 1}, 2.0f);
            font.drawText(*renderer, x + 34.0f, rowY + 18.0f, "START", render::Color{0.85f, 0.9f, 1.0f, 1},
                          0.42f);
            drawState(*renderer, font, x + bw * 0.5f, rowY + bh + 40.0f, s);

            for (std::size_t k = 0; k < result.steps.size(); ++k) {
                const goap::Action& a = lib[static_cast<std::size_t>(result.steps[k])];
                const float fromX = x + bw;
                x += bw + gap;
                // Arrow.
                thickLine(*renderer, math::vec2(fromX, rowY + bh * 0.5f),
                          math::vec2(x, rowY + bh * 0.5f), 3.0f, render::Color{0.5f, 0.7f, 0.95f, 1});
                // Step box.
                const bool isGoalStep = (a.set & goap::bit(HasFire)) != 0;
                const render::Color fill = isGoalStep ? render::Color{0.20f, 0.42f, 0.26f, 1}
                                                      : render::Color{0.20f, 0.30f, 0.44f, 1};
                const render::Color line = isGoalStep ? render::Color{0.45f, 0.85f, 0.55f, 1}
                                                      : render::Color{0.45f, 0.65f, 0.95f, 1};
                outlinedRect(*renderer, x, rowY, bw, bh, fill, line, 2.0f);
                font.drawText(*renderer, x + 8.0f, rowY + 10.0f, a.name.c_str(),
                              render::Color{0.96f, 0.98f, 1.0f, 1}, 0.32f);
                const std::string step = std::to_string(k + 1) + ".  +" + std::to_string(static_cast<int>(a.cost));
                font.drawText(*renderer, x + 8.0f, rowY + 34.0f, step.c_str(),
                              render::Color{0.75f, 0.82f, 0.95f, 1}, 0.30f);

                s = goap::apply(s, a);
                drawState(*renderer, font, x + bw * 0.5f, rowY + bh + 40.0f, s);
            }

            // Legend.
            font.drawText(*renderer, 40.0f, 636.0f,
                          "each dot strip is the world-state AFTER that step; green = fact true.  "
                          "the plan lights the last fact (FIRE) at the cheapest total cost.",
                          render::Color{0.72f, 0.78f, 0.88f, 1}, 0.34f);
            font.drawText(*renderer, 40.0f, 668.0f,
                          "the agent chose the axe route (forest -> get axe -> chop -> camp -> build) over "
                          "the pricier 'scavenge wood' shortcut - A* found it minimal.",
                          render::Color{0.72f, 0.78f, 0.88f, 1}, 0.34f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("GOAP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
