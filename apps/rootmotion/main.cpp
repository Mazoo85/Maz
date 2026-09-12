// Maz Engine — "ROOTMOTION" (root motion, toward Godot's AnimationMixer root-motion track)
// A walk clip that TRAVELS: the character's forward motion and turning are read straight out of the
// animation (anim::RootMotionTrack) each fixed step and applied to its world pose — rotating the clip-
// local step into the direction it currently faces — so it sweeps a smooth arc across the screen with
// its feet planted along the path (no foot sliding). The sim is a fixed initial pose + fixed step count,
// so it's deterministic and golden-stable. Footprints are dropped as the clip loops; the arrow is the
// character. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillPoly(render::Renderer& r, const std::vector<math::vec2>& pts, render::Color c) {
    std::vector<render::Point2> p(pts.size());
    for (std::size_t i = 0; i < pts.size(); ++i) {
        p[i] = {pts[i].x, pts[i].y};
    }
    r.drawConvexPolygon(p.data(), static_cast<uint32_t>(p.size()), c);
}

void thickLine(render::Renderer& r, math::vec2 a, math::vec2 b, float w, render::Color c) {
    math::vec2 d = b - a;
    const float len = std::sqrt(d.x * d.x + d.y * d.y);
    if (len < 1e-4f) {
        return;
    }
    d /= len;
    const math::vec2 n(-d.y * w * 0.5f, d.x * w * 0.5f);
    std::vector<math::vec2> q = {{a.x + n.x, a.y + n.y},
                                 {b.x + n.x, b.y + n.y},
                                 {b.x - n.x, b.y - n.y},
                                 {a.x - n.x, a.y - n.y}};
    fillPoly(r, q, c);
}

// A little footprint: a short capsule-ish quad centred at p, long axis along `dir`.
void footprint(render::Renderer& r, math::vec2 p, math::vec2 dir, render::Color c) {
    const math::vec2 perp(-dir.y, dir.x);
    const float fl = 7.0f, fw = 3.5f;
    std::vector<math::vec2> q = {p + dir * fl + perp * fw, p + dir * fl - perp * fw,
                                 p - dir * fl - perp * fw, p - dir * fl + perp * fw};
    fillPoly(r, q, c);
}

struct Footstep {
    math::vec2 pos;
    math::vec2 dir;
    int side; // +1 / -1
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ROOTMOTION starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Root Motion";
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

    // ---- The walk clip: over one 1 s loop the root travels +x locally and turns a little. ------------
    anim::RootMotionTrack walk;
    walk.addKey(0.0f, math::vec2(0.0f, 0.0f), 0.0f);
    walk.addKey(1.0f, math::vec2(185.0f, 0.0f), 0.16f); // ~185 px forward + 0.16 rad turn per loop

    // ---- Run the deterministic fixed-step simulation up front. ---------------------------------------
    const float dt = 1.0f / 60.0f;
    const int steps = 300; // 5 s
    math::vec2 pos(150.0f, 300.0f);
    float head = -0.28f; // initial facing (slightly up-right)
    float clipT = 0.0f;

    std::vector<math::vec2> trail;
    std::vector<Footstep> steps_fp;
    int footSide = 1;
    float footTimer = 0.0f;
    const float footInterval = 0.34f; // plant a foot roughly every third of a second
    trail.push_back(pos);
    for (int i = 0; i < steps; ++i) {
        const float prevT = clipT;
        clipT += dt;
        bool looped = false;
        if (clipT >= walk.endTime()) {
            clipT -= walk.duration();
            looped = true;
        }
        walk.advance(pos, head, prevT, clipT, looped);
        trail.push_back(pos);

        footTimer += dt;
        if (footTimer >= footInterval) {
            footTimer -= footInterval;
            const math::vec2 dir(std::cos(head), std::sin(head));
            const math::vec2 perp(-dir.y, dir.x);
            steps_fp.push_back({pos + perp * (10.0f * static_cast<float>(footSide)), dir, footSide});
            footSide = -footSide;
        }
    }

    const render::Color kBg{0.09f, 0.10f, 0.13f, 1.0f};
    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.6f, 0.66f, 0.78f, 1};
    const render::Color kTrail{0.30f, 0.40f, 0.52f, 1.0f};
    const render::Color kFootL{0.45f, 0.80f, 1.00f, 1.0f};
    const render::Color kFootR{1.00f, 0.72f, 0.42f, 1.0f};
    const render::Color kBody{0.60f, 1.00f, 0.68f, 1.0f};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(kBg);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ROOT MOTION", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "the walk clip drives the travel: the character sweeps an arc with its feet "
                          "planted along the path (anim::RootMotionTrack)",
                          kDim, 0.32f);

            // The travelled path.
            for (std::size_t i = 1; i < trail.size(); ++i) {
                thickLine(*renderer, trail[i - 1], trail[i], 2.0f, kTrail);
            }

            // Footprints, alternating left/right and oriented to the heading.
            for (const Footstep& f : steps_fp) {
                footprint(*renderer, f.pos, f.dir, f.side > 0 ? kFootL : kFootR);
            }

            // The character: an arrow pointing along its final heading.
            const math::vec2 dir(std::cos(head), std::sin(head));
            const math::vec2 perp(-dir.y, dir.x);
            std::vector<math::vec2> body = {pos + dir * 20.0f, pos - dir * 12.0f + perp * 11.0f,
                                            pos - dir * 12.0f - perp * 11.0f};
            fillPoly(*renderer, body, kBody);

            font.drawText(*renderer, 40.0f, 662.0f,
                          "blue/orange = left/right footfalls; the arrow is the character — no foot "
                          "sliding because the animation itself carries the motion",
                          render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ROOTMOTION shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
