// Maz Engine — "TENTACLE" (multi-bone FABRIK IK, toward Godot's SkeletonModification2DFABRIK)
// A row of many-jointed chains anchored along the floor, each reaching for its own target with the
// FABRIK solver (anim::solveFabrik). Chains whose target is within reach CURL naturally to touch it
// (ringed green); chains whose target is too far STRAIGHTEN and point at it, fully extended (ringed
// red). Every bone keeps its length. The solve runs once at startup and the settled poses are drawn
// statically, so the render is deterministic and golden-stable. Run --headless / --frames N for CI.

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

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col, int sides = 18) {
    std::vector<render::Point2> p(static_cast<size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        p[static_cast<size_t>(i)] = render::Point2{c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p.data(), static_cast<uint32_t>(sides), col);
}

render::Color hue(float h) {
    h -= std::floor(h);
    const float r = std::fabs(h * 6.0f - 3.0f) - 1.0f;
    const float g = 2.0f - std::fabs(h * 6.0f - 2.0f);
    const float b = 2.0f - std::fabs(h * 6.0f - 4.0f);
    auto cl = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    return render::Color{cl(r), cl(g), cl(b), 1.0f};
}

struct Chain {
    std::vector<math::vec2> joints;
    math::vec2 target;
    render::Color col;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TENTACLE (multi-bone FABRIK IK) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — FABRIK IK Chains";
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

    const float W = static_cast<float>(cfg.width);
    const float H = static_cast<float>(cfg.height);
    const float floorY = H - 70.0f;

    // Build a row of chains anchored along the floor, each reaching for a fanned-out target.
    const int count = 7;
    const int segs = 8;             // bones per chain
    const float boneLen = 34.0f;    // each bone's length (total reach = segs * boneLen)
    std::vector<Chain> chains;
    for (int c = 0; c < count; ++c) {
        Chain ch;
        const float fx = static_cast<float>(c) / static_cast<float>(count - 1);
        const math::vec2 base(W * (0.10f + 0.80f * fx), floorY);
        // Start straight up.
        for (int j = 0; j <= segs; ++j) {
            ch.joints.push_back(base - math::vec2(0.0f, static_cast<float>(j) * boneLen));
        }
        // Targets arranged in an arc across the upper area; the ends aim far (out of reach) so they
        // straighten, while the middle ones sit close and curl.
        const float ta = 3.14159265f * (0.15f + 0.7f * fx); // 27deg .. 153deg
        const float reach = static_cast<float>(segs) * boneLen;
        const float tdist = (c == 0 || c == count - 1) ? reach * 1.35f : reach * (0.45f + 0.4f * std::fabs(fx - 0.5f) * 2.0f);
        ch.target = base + math::vec2(std::cos(ta), -std::sin(ta)) * tdist;
        ch.col = hue(fx * 0.85f);
        anim::solveFabrik(ch.joints, ch.target, 16, 0.5f);
        chains.push_back(std::move(ch));
    }

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

            // Floor line.
            thickLine(*renderer, {0.0f, floorY}, {W, floorY}, 2.0f, render::Color{0.25f, 0.27f, 0.33f, 1});

            for (const Chain& ch : chains) {
                // Did the tip reach the target? (within a bone length)
                const math::vec2 d = ch.joints.back() - ch.target;
                const bool reached = std::sqrt(d.x * d.x + d.y * d.y) < boneLen * 0.6f;
                // Bones, tapering slightly toward the tip.
                for (size_t j = 1; j < ch.joints.size(); ++j) {
                    const float w = 9.0f - 5.0f * static_cast<float>(j) / static_cast<float>(ch.joints.size());
                    thickLine(*renderer, ch.joints[j - 1], ch.joints[j], w, ch.col);
                }
                // Joints.
                for (const math::vec2& p : ch.joints) {
                    fillCircle(*renderer, p, 3.0f, render::Color{0.1f, 0.11f, 0.14f, 1});
                }
                // Anchor + target marker (green if reached, red if out of reach).
                fillCircle(*renderer, ch.joints.front(), 6.0f, render::Color{0.85f, 0.88f, 0.95f, 1});
                const render::Color tc =
                    reached ? render::Color{0.4f, 0.9f, 0.5f, 1} : render::Color{0.95f, 0.4f, 0.4f, 1};
                fillCircle(*renderer, ch.target, 7.0f, tc);
                fillCircle(*renderer, ch.target, 3.0f, render::Color{0.08f, 0.09f, 0.12f, 1});
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  MULTI-BONE FABRIK IK",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "8-bone chains reach for their targets; reachable ones curl (green), out-of-"
                          "reach ones straighten (red)",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TENTACLE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
