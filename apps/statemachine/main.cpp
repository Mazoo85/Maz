// Maz Engine — "STATEMACHINE" (animation state machine, toward Godot's AnimationNodeStateMachine)
// A locomotion state machine — idle / move / jump — is stepped over a scripted "speed + jump" timeline
// and its output drawn two ways. TOP: a weight-band timeline — at each moment the machine reports the
// active state(s) and their blend weights (which sum to 1), stacked as coloured bands across time, so
// every cross-fade shows up as one colour smoothly giving way to another (idle->move, ->jump, ->idle).
// The `move` band is itself a walk<->run blend space, shaded from dark (walk) to bright (run) by speed —
// a state machine OVER a blend space. BOTTOM: the state graph with transition arrows, the final state
// lit. The whole sequence is precomputed once, so the render is deterministic and golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

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

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

// One recorded frame of the machine's output.
struct Frame {
    float idle, move, jump; // active-state weights (sum to 1)
    float run;              // walk(0)->run(1) blend inside the move state
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("STATEMACHINE (animation state machine) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Animation State Machine";
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

    // ---- Build the state machine + a scripted parameter timeline, then simulate once ---------------
    float speed = 0.0f;
    bool jumpReq = false, jumpDone = false;

    anim::AnimStateMachine sm;
    sm.addState("idle", 0);
    sm.addState("move", 1);
    sm.addState("jump", 2);
    sm.addTransition("idle", "move", 0.25f, [&] { return speed > 0.15f; });
    sm.addTransition("move", "idle", 0.25f, [&] { return speed <= 0.15f; });
    sm.addTransition("idle", "jump", 0.10f, [&] { return jumpReq; });
    sm.addTransition("move", "jump", 0.10f, [&] { return jumpReq; });
    sm.addTransition("jump", "idle", 0.20f, [&] { return jumpDone; });
    sm.setStart("idle");

    const float dt = 1.0f / 60.0f;
    const int steps = 300; // 5 seconds
    std::vector<Frame> frames;
    frames.reserve(static_cast<size_t>(steps));
    float jumpTimer = 0.0f;
    for (int s = 0; s < steps; ++s) {
        const float t = static_cast<float>(s) * dt;
        // Scripted speed: rise, cruise, fall.
        if (t < 0.7f) {
            speed = 0.0f;
        } else if (t < 1.6f) {
            speed = (t - 0.7f) / 0.9f; // ramp 0 -> 1
        } else if (t < 3.0f) {
            speed = 1.0f;
        } else if (t < 3.9f) {
            speed = 1.0f - (t - 3.0f) / 0.9f; // ramp 1 -> 0
        } else {
            speed = 0.0f;
        }
        // A single jump around t=2.3s: request for one frame, then a 0.45s air timer.
        jumpReq = (s == 138);
        if (sm.currentName() == "jump") {
            jumpTimer += dt;
        } else {
            jumpTimer = 0.0f;
        }
        jumpDone = jumpTimer > 0.45f;

        sm.update(dt);

        Frame f{0.0f, 0.0f, 0.0f, speed < 0.0f ? 0.0f : (speed > 1.0f ? 1.0f : speed)};
        for (const auto& a : sm.active()) {
            if (a.id == 0) f.idle += a.weight;
            else if (a.id == 1) f.move += a.weight;
            else f.jump += a.weight;
        }
        frames.push_back(f);
    }

    const float W = static_cast<float>(cfg.width);
    const float bandX = 40.0f, bandW = W - 80.0f;
    const float bandTop = 110.0f, bandH = 240.0f;

    const render::Color colIdle{0.40f, 0.55f, 0.95f, 1.0f}; // blue
    const render::Color colJump{1.00f, 0.62f, 0.28f, 1.0f}; // orange

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

            // Timeline background.
            fillRect(*renderer, bandX, bandTop, bandW, bandH, render::Color{0.11f, 0.12f, 0.16f, 1});

            // One vertical column per sampled frame: stack idle (bottom) / move / jump (top).
            const int cols = static_cast<int>(frames.size());
            const float cw = bandW / static_cast<float>(cols);
            for (int i = 0; i < cols; ++i) {
                const Frame& f = frames[static_cast<size_t>(i)];
                const float x = bandX + static_cast<float>(i) * cw;
                float y = bandTop + bandH;
                // idle band (bottom).
                float h = f.idle * bandH;
                y -= h;
                fillRect(*renderer, x, y, cw + 0.5f, h, colIdle);
                // move band (middle), shaded walk(dark)->run(bright) by speed.
                h = f.move * bandH;
                y -= h;
                const float g = 0.45f + 0.5f * f.run;
                fillRect(*renderer, x, y, cw + 0.5f, h, render::Color{0.25f, g, 0.4f, 1});
                // jump band (top).
                h = f.jump * bandH;
                y -= h;
                fillRect(*renderer, x, y, cw + 0.5f, h, colJump);
            }

            // ---- State graph below ----
            struct Node {
                float x, y;
                const char* label;
                render::Color col;
            };
            const float gy = bandTop + bandH + 120.0f;
            const Node nodes[3] = {
                {160.0f, gy, "idle", colIdle},
                {560.0f, gy, "move", render::Color{0.3f, 0.8f, 0.45f, 1}},
                {960.0f, gy, "jump", colJump},
            };
            // Arrows idle<->move, idle->jump, move->jump, jump->idle.
            const int arrows[5][2] = {{0, 1}, {1, 0}, {0, 2}, {1, 2}, {2, 0}};
            for (const auto& e : arrows) {
                thickLine(*renderer, {nodes[e[0]].x + 60.0f, nodes[e[0]].y + 20.0f},
                          {nodes[e[1]].x, nodes[e[1]].y + 20.0f}, 2.0f,
                          render::Color{0.4f, 0.43f, 0.5f, 1});
            }
            for (int i = 0; i < 3; ++i) {
                const bool lit = (nodes[i].label == sm.currentName());
                fillRect(*renderer, nodes[i].x, nodes[i].y, 130.0f, 42.0f,
                         lit ? nodes[i].col : render::Color{0.2f, 0.22f, 0.27f, 1});
                font.drawText(*renderer, nodes[i].x + 16.0f, nodes[i].y + 12.0f, nodes[i].label,
                              render::Color{0.05f, 0.06f, 0.09f, 1}, 0.44f);
            }

            // Legend.
            const char* lnames[3] = {"idle", "move (walk->run)", "jump"};
            const render::Color lcol[3] = {colIdle, render::Color{0.25f, 0.7f, 0.4f, 1}, colJump};
            for (int i = 0; i < 3; ++i) {
                const float x = 40.0f + static_cast<float>(i) * 260.0f;
                fillRect(*renderer, x, bandTop + bandH + 24.0f, 20.0f, 20.0f, lcol[i]);
                font.drawText(*renderer, x + 28.0f, bandTop + bandH + 27.0f, lnames[i],
                              render::Color{0.85f, 0.88f, 0.95f, 1}, 0.36f);
            }

            font.drawText(*renderer, 16.0f, 12.0f,
                          "MAZ ENGINE  -  ANIMATION STATE MACHINE (cross-faded states over time)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "active-state weights across a scripted speed+jump timeline; each cross-fade "
                          "blends one state into the next",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("STATEMACHINE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
