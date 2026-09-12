// Maz Engine — "KINEMATIC" (game::moveAndSlide, toward Godot's CharacterBody2D.move_and_slide)
// A kinematic character is driven directly by a velocity and must slide along the static world without
// tunneling or sticking. This demo runs one character through a fixed obstacle course under gravity +
// a constant rightward drive for ~4 simulated seconds and draws its whole path, colouring each step by
// what moveAndSlide reported: BLUE airborne, GREEN on-floor, ORANGE on-wall. You can read the cascade —
// fall onto a platform, run off its edge, arc into a floating wall and slide DOWN it, drop off, then run
// along the ground. Fixed sim -> deterministic, golden-stable. Run --headless / --frames N for CI.

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

void fillAabb(render::Renderer& r, const game::Aabb2& b, render::Color c) {
    const render::Point2 q[4] = {
        {b.min.x, b.min.y}, {b.max.x, b.min.y}, {b.max.x, b.max.y}, {b.min.x, b.max.y}};
    r.drawConvexPolygon(q, 4, c);
}

void outlineAabb(render::Renderer& r, const game::Aabb2& b, float w, render::Color c) {
    thickLine(r, {b.min.x, b.min.y}, {b.max.x, b.min.y}, w, c);
    thickLine(r, {b.max.x, b.min.y}, {b.max.x, b.max.y}, w, c);
    thickLine(r, {b.max.x, b.max.y}, {b.min.x, b.max.y}, w, c);
    thickLine(r, {b.min.x, b.max.y}, {b.min.x, b.min.y}, w, c);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

enum class State { Air, Floor, Wall };

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("KINEMATIC (game::moveAndSlide) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Kinematic Character (move_and_slide)";
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

    // ---- Static world (pixel space, +y = down) ------------------------------------------------------
    const std::vector<game::Aabb2> solids = {
        game::Aabb2{{60.0f, 600.0f}, {1200.0f, 640.0f}},  // ground
        game::Aabb2{{120.0f, 300.0f}, {280.0f, 332.0f}},  // upper-left platform (walk off its right edge)
        game::Aabb2{{360.0f, 140.0f}, {402.0f, 560.0f}},  // wall across a gap (arc into it + slide down)
    };

    // ---- Simulate the character once (deterministic) and record path + state ------------------------
    const math::vec2 half(12.0f, 16.0f);
    math::vec2 pos(170.0f, 120.0f);
    math::vec2 vel(0.0f, 0.0f);
    const float g = 1200.0f;   // gravity (px/s^2, down)
    const float drive = 165.0f; // constant rightward drive (px/s)
    const float dt = 1.0f / 60.0f;

    std::vector<math::vec2> path;
    std::vector<State> states;
    path.push_back(pos);
    states.push_back(State::Air);
    for (int step = 0; step < 260; ++step) {
        vel.x = drive;
        vel.y += g * dt;
        const game::SlideResult r = game::moveAndSlide(pos, half, vel, dt, solids);
        pos = r.position;
        vel = r.velocity;
        State s = State::Air;
        if (r.onWall) {
            s = State::Wall;
        } else if (r.onFloor) {
            s = State::Floor;
        }
        path.push_back(pos);
        states.push_back(s);
    }

    const render::Color kAir{0.5f, 0.72f, 1.0f, 1.0f};
    const render::Color kFloor{0.5f, 0.95f, 0.6f, 1.0f};
    const render::Color kWall{1.0f, 0.75f, 0.4f, 1.0f};
    const render::Color kSolid{0.22f, 0.25f, 0.32f, 1.0f};

    auto stateColor = [&](State s) {
        return s == State::Floor ? kFloor : (s == State::Wall ? kWall : kAir);
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  KINEMATIC CHARACTER", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "game::moveAndSlide (Godot CharacterBody2D.move_and_slide): swept motion that "
                          "slides along contacts + floor/wall/ceiling detection",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            for (const game::Aabb2& s : solids) {
                fillAabb(*renderer, s, kSolid);
            }

            // The character's path, coloured by contact state.
            for (std::size_t i = 1; i < path.size(); ++i) {
                thickLine(*renderer, path[i - 1], path[i], 3.0f, stateColor(states[i]));
            }

            // Start marker + final body outline.
            outlineAabb(*renderer, game::Aabb2::fromCenterHalf(path.front(), half), 1.5f,
                        rgba(0.6f, 0.6f, 0.7f, 1));
            outlineAabb(*renderer, game::Aabb2::fromCenterHalf(path.back(), half), 2.5f,
                        rgba(1, 1, 1, 1));

            // Legend.
            thickLine(*renderer, {80.0f, 680.0f}, {120.0f, 680.0f}, 4.0f, kAir);
            font.drawText(*renderer, 128.0f, 670.0f, "airborne", kAir, 0.3f);
            thickLine(*renderer, {300.0f, 680.0f}, {340.0f, 680.0f}, 4.0f, kFloor);
            font.drawText(*renderer, 348.0f, 670.0f, "on floor", kFloor, 0.3f);
            thickLine(*renderer, {520.0f, 680.0f}, {560.0f, 680.0f}, 4.0f, kWall);
            font.drawText(*renderer, 568.0f, 670.0f, "on wall (slides down)", kWall, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("KINEMATIC shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
