// Maz Engine — "CONTACTS" (contact begin/persist/end events, toward Godot body_entered/body_exited)
// A game needs to KNOW when things touch — to play a thud, deal damage, trip a trigger. This adds
// contact-event reporting to the world (PhysicsWorld2D::trackContacts): every step it emits Begin /
// Persist / End events with the contact point, normal and impulse. This demo settles a small pile with
// tracking on and marks every active contact point with a bright cross — the engine's own view of
// where the pile is touching. Simulated once up front with a fixed timestep -> deterministic golden.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <array>
#include <cmath>
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

void drawDisc(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int N = 18;
    std::array<render::Point2, 18> pts{};
    for (int i = 0; i < N; ++i) {
        const float t = 6.2831853f * static_cast<float>(i) / static_cast<float>(N);
        pts[static_cast<std::size_t>(i)] =
            render::Point2{c.x + std::cos(t) * radius, c.y + std::sin(t) * radius};
    }
    r.drawConvexPolygon(pts.data(), static_cast<uint32_t>(N), col);
}

void drawCross(render::Renderer& r, math::vec2 c, float s, render::Color col) {
    const render::Point2 h[4] = {{c.x - s, c.y - 2}, {c.x + s, c.y - 2}, {c.x + s, c.y + 2},
                                 {c.x - s, c.y + 2}};
    const render::Point2 v[4] = {{c.x - 2, c.y - s}, {c.x + 2, c.y - s}, {c.x + 2, c.y + s},
                                 {c.x - 2, c.y + s}};
    r.drawConvexPolygon(h, 4, col);
    r.drawConvexPolygon(v, 4, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CONTACTS (contact events) starting");

    game::PhysicsWorld2D w;
    w.gravity = math::vec2(0.0f, 900.0f);
    w.warmStarting = true;
    w.trackContacts = true;

    game::Body2D floor;
    floor.shape = game::Body2D::Box;
    floor.half = math::vec2(320.0f, 16.0f);
    floor.pos = math::vec2(640.0f, 560.0f);
    floor.invMass = 0.0f;
    floor.friction = 0.9f;
    w.add(floor);
    // A small pyramid of boxes + a couple of circles nestled on top.
    const float bh = 26.0f;
    for (int row = 0; row < 3; ++row) {
        const int count = 4 - row;
        for (int i = 0; i < count; ++i) {
            game::Body2D b;
            b.shape = game::Body2D::Box;
            b.half = math::vec2(40.0f, bh);
            const float rowW = static_cast<float>(count) * 84.0f;
            b.pos = math::vec2(640.0f - rowW * 0.5f + 42.0f + static_cast<float>(i) * 84.0f,
                               544.0f - bh - static_cast<float>(row) * (bh * 2.0f + 1.0f));
            b.invMass = 1.0f;
            b.friction = 0.9f;
            b.restitution = 0.0f;
            b.enableRotation();
            w.add(b);
        }
    }
    for (int i = 0; i < 2; ++i) {
        game::Body2D c;
        c.shape = game::Body2D::Circle;
        c.radius = 26.0f;
        c.pos = math::vec2(600.0f + static_cast<float>(i) * 84.0f, 380.0f);
        c.invMass = 1.0f;
        c.friction = 0.9f;
        c.restitution = 0.0f;
        c.enableRotation();
        w.add(c);
    }
    for (int s = 0; s < 360; ++s) {
        w.step(1.0f / 60.0f, 10);
    }
    // Capture the settled frame's active contact points (Begin + Persist).
    std::vector<math::vec2> points;
    for (const game::ContactEvent& e : w.contactEvents) {
        if (e.phase != game::ContactPhase::End) {
            points.push_back(e.point);
        }
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Contact Events";
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
    std::snprintf(status, sizeof(status), "%zu active contact points", points.size());

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

            font.drawText(*renderer, 16.0f, 12.0f,
                          "MAZ ENGINE  -  CONTACT EVENTS (Godot body_entered / body_exited)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "the crosses are the engine's reported contact points for this settled pile",
                          render::Color{0.8f, 0.86f, 0.95f, 1}, 0.4f);

            for (std::size_t i = 0; i < w.bodies.size(); ++i) {
                const game::Body2D& b = w.bodies[i];
                if (b.shape == game::Body2D::Box) {
                    const auto q = boxQuad(b);
                    const render::Color col = (b.invMass == 0.0f)
                                                  ? render::Color{0.28f, 0.30f, 0.36f, 1.0f}
                                                  : render::Color{0.42f, 0.52f, 0.68f, 1.0f};
                    renderer->drawConvexPolygon(q.data(), 4, render::Color{0.08f, 0.09f, 0.12f, 1});
                    const auto q2 = boxQuad(b, 2.5f);
                    renderer->drawConvexPolygon(q2.data(), 4, col);
                } else {
                    drawDisc(*renderer, b.pos, b.radius, render::Color{0.08f, 0.09f, 0.12f, 1});
                    drawDisc(*renderer, b.pos, b.radius - 2.5f, render::Color{0.5f, 0.62f, 0.8f, 1});
                }
            }
            // Contact markers on top.
            for (const math::vec2& p : points) {
                drawCross(*renderer, p, 10.0f, render::Color{1.0f, 0.85f, 0.3f, 1.0f});
            }

            font.drawText(*renderer, 16.0f, 664.0f, status, render::Color{1.0f, 0.85f, 0.3f, 1}, 0.4f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CONTACTS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
