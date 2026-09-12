// Maz Engine — "SLEEP" (body sleeping / islands, toward Godot's can_sleep)
// A settled pile shouldn't keep burning CPU. This adds opt-in body sleeping (PhysicsWorld2D::allowSleep):
// bodies connected by contacts/joints form an island, and once the whole island stays quiet for
// sleepTime it goes to sleep — skipped by integration and the solver until something wakes it. This
// demo settles two identical towers up front: the LEFT with sleeping ON (its boxes have gone to sleep,
// drawn dimmed with a "Z") and the RIGHT with sleeping OFF (still actively simulated every frame, drawn
// bright). Same rest pose, different cost. Simulated once with a fixed timestep -> deterministic golden.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::array<render::Point2, 4> boxQuad(const game::Body2D& b, float pad = 0.0f) {
    const float c = std::cos(b.angle), s = std::sin(b.angle);
    const float hx = b.half.x - pad, hy = b.half.y - pad;
    const math::vec2 ax(c, s), ay(-s, c);
    auto corner = [&](float sx, float sy) {
        const math::vec2 p = b.pos + ax * (sx * hx) + ay * (sy * hy);
        return render::Point2{p.x, p.y};
    };
    return {corner(-1, -1), corner(1, -1), corner(1, 1), corner(-1, 1)};
}

game::PhysicsWorld2D settleTower(float cx, float floorTop, int n, bool sleep) {
    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 900.0f);
    w.warmStarting = true;
    w.allowSleep = sleep;
    w.sleepTime = 0.5f;

    game::Body2D floor;
    floor.shape = game::Body2D::Box;
    floor.pos = math::vec2(cx, floorTop + 16.0f);
    floor.half = math::vec2(170.0f, 16.0f);
    floor.invMass = 0.0f;
    floor.friction = 0.9f;
    w.add(floor);

    const float bh = 22.0f;
    for (int i = 0; i < n; ++i) {
        game::Body2D box;
        box.shape = game::Body2D::Box;
        box.half = math::vec2(54.0f, bh);
        box.pos = math::vec2(cx, floorTop - bh - static_cast<float>(i) * (bh * 2.0f + 1.0f));
        box.invMass = 1.0f;
        box.friction = 0.9f;
        box.restitution = 0.0f;
        box.enableRotation();
        w.add(box);
    }
    for (int s = 0; s < 300; ++s) {
        w.step(1.0f / 60.0f, 8);
    }
    return w;
}

void drawTower(render::Renderer& r, ui::Font& font, const game::PhysicsWorld2D& w) {
    const render::Color palette[6] = {{0.95f, 0.45f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                      {0.5f, 0.65f, 0.95f, 1},   {0.95f, 0.8f, 0.4f, 1},
                                      {0.85f, 0.55f, 0.95f, 1},  {0.45f, 0.9f, 0.9f, 1}};
    for (std::size_t i = 0; i < w.bodies.size(); ++i) {
        const game::Body2D& b = w.bodies[i];
        if (i == 0) {
            const auto q = boxQuad(b);
            r.drawConvexPolygon(q.data(), 4, render::Color{0.28f, 0.30f, 0.36f, 1.0f});
            continue;
        }
        const auto outer = boxQuad(b);
        r.drawConvexPolygon(outer.data(), 4, render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        render::Color col = palette[(i - 1) % 6];
        if (b.sleeping) {
            col = render::Color{col.r * 0.4f, col.g * 0.4f, col.b * 0.45f, 1.0f}; // dimmed = asleep
        }
        const auto inner = boxQuad(b, 2.5f);
        r.drawConvexPolygon(inner.data(), 4, col);
        if (b.sleeping) {
            font.drawText(r, b.pos.x - 8.0f, b.pos.y - 12.0f, "Z",
                          render::Color{0.75f, 0.8f, 0.95f, 1}, 0.4f);
        }
    }
}

int countAsleep(const game::PhysicsWorld2D& w) {
    int c = 0;
    for (std::size_t i = 1; i < w.bodies.size(); ++i) {
        if (w.bodies[i].sleeping) {
            ++c;
        }
    }
    return c;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SLEEP (body sleeping / islands) starting");

    const float floorTop = 560.0f;
    const int n = 6;
    const game::PhysicsWorld2D sleepy = settleTower(340.0f, floorTop, n, true);
    const game::PhysicsWorld2D awake = settleTower(940.0f, floorTop, n, false);
    const int asleep = countAsleep(sleepy);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Body Sleeping (islands)";
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

    char status[64];
    std::snprintf(status, sizeof(status), "%d / %d boxes asleep", asleep, n);

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  BODY SLEEPING (ISLANDS)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "a settled island sleeps as a unit and costs zero CPU until something wakes "
                          "it (Godot can_sleep)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            drawTower(*renderer, font, sleepy);
            drawTower(*renderer, font, awake);

            font.drawText(*renderer, 210.0f, 604.0f, "allowSleep = TRUE",
                          render::Color{0.6f, 0.7f, 0.95f, 1}, 0.46f);
            font.drawText(*renderer, 240.0f, 636.0f, status, render::Color{0.7f, 0.78f, 0.92f, 1}, 0.36f);
            font.drawText(*renderer, 830.0f, 604.0f, "allowSleep = FALSE",
                          render::Color{0.95f, 0.85f, 0.5f, 1}, 0.46f);
            font.drawText(*renderer, 800.0f, 636.0f, "simulated every frame (bright)",
                          render::Color{0.9f, 0.85f, 0.7f, 1}, 0.36f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SLEEP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
