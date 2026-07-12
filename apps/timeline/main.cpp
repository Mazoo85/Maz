// Maz Engine — "TIMELINE" (keyframe timeline / sequencer, toward Godot's AnimationPlayer)
// One animation, many named tracks. A little arrow is driven by keyframed x / y / rotation / scale /
// colour tracks at once: it sweeps across an arc, spins, pulses, and shifts colour. The top half shows
// the whole animation as an onion-skin trail (the arrow sampled at evenly spaced times, faint -> bright),
// and the bottom half is an editor-style track panel — each track's curve with its keyframe dots and a
// playhead line at a fixed time. Every value is sampled from anim::Timeline at fixed times, so the whole
// render is deterministic and golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::Point2 toP(math::vec2 v) { return render::Point2{v.x, v.y}; }

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

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col, int sides = 16) {
    std::vector<render::Point2> p(static_cast<size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        p[static_cast<size_t>(i)] = render::Point2{c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p.data(), static_cast<uint32_t>(sides), col);
}

// A pointing arrow (convex triangle) at center, scaled by size, rotated by ang.
void drawArrow(render::Renderer& r, math::vec2 center, float size, float ang, render::Color col) {
    const float c = std::cos(ang);
    const float s = std::sin(ang);
    auto rot = [&](float x, float y) {
        return math::vec2(center.x + x * c - y * s, center.y + x * s + y * c);
    };
    const render::Point2 p[3] = {toP(rot(size, 0.0f)), toP(rot(-size * 0.7f, size * 0.62f)),
                                 toP(rot(-size * 0.7f, -size * 0.62f))};
    r.drawConvexPolygon(p, 3, col);
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TIMELINE (keyframe sequencer) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Keyframe Timeline";
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
    const float marginX = 48.0f;

    // Preview band (onion-skin) up top; track editor fills the rest.
    const float previewTop = 92.0f;
    const float previewBot = H * 0.52f;
    const float previewMidY = (previewTop + previewBot) * 0.5f;

    // ---- Author the animation: named keyframe tracks (Godot AnimationPlayer style) ---------------
    using anim::Ease;
    anim::Timeline tl;
    tl.duration = 3.0f;
    tl.track("x").add(0.0f, marginX + 70.0f, Ease::QuadOut);
    tl.track("x").add(1.5f, W * 0.5f, Ease::QuadIn);
    tl.track("x").add(3.0f, W - marginX - 70.0f);
    tl.track("y").add(0.0f, previewMidY + 74.0f, Ease::SineInOut);
    tl.track("y").add(1.5f, previewMidY - 74.0f, Ease::SineInOut);
    tl.track("y").add(3.0f, previewMidY + 74.0f);
    tl.track("rot").add(0.0f, 0.0f);
    tl.track("rot").add(3.0f, 12.566371f); // two full turns
    tl.track("scale").add(0.0f, 0.7f, Ease::BackOut);
    tl.track("scale").add(0.75f, 1.5f);
    tl.track("scale").add(1.5f, 0.85f, Ease::BackOut);
    tl.track("scale").add(2.25f, 1.5f);
    tl.track("scale").add(3.0f, 0.7f);
    tl.track("r").add(0.0f, 1.0f);
    tl.track("r").add(1.5f, 0.25f);
    tl.track("r").add(3.0f, 1.0f);
    tl.track("g").add(0.0f, 0.35f);
    tl.track("g").add(1.5f, 1.0f);
    tl.track("g").add(3.0f, 0.35f);
    tl.track("b").add(0.0f, 0.45f);
    tl.track("b").add(1.5f, 0.55f);
    tl.track("b").add(3.0f, 1.0f);

    const float dur = tl.length();
    const float playT = 1.9f; // fixed playhead for the editor + the bright "current" pose

    auto poseAt = [&](float t) {
        const float sc = tl.valueAt("scale", t);
        return std::pair<math::vec2, float>(math::vec2(tl.valueAt("x", t), tl.valueAt("y", t)), sc);
    };
    auto colorAt = [&](float t, float a) {
        return render::Color{tl.valueAt("r", t), tl.valueAt("g", t), tl.valueAt("b", t), a};
    };

    // Editor lanes.
    struct Lane {
        const char* track;
        const char* label;
        render::Color col;
    };
    const Lane lanes[4] = {
        {"x", "x  (position)", render::Color{0.45f, 0.80f, 1.00f, 1}},
        {"y", "y  (position)", render::Color{1.00f, 0.55f, 0.55f, 1}},
        {"rot", "rotation", render::Color{0.70f, 1.00f, 0.55f, 1}},
        {"scale", "scale", render::Color{1.00f, 0.85f, 0.40f, 1}},
    };

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

            // ---- Onion-skin trail: the arrow sampled across the whole clip -----------------------
            const int ghosts = 13;
            for (int i = 0; i < ghosts; ++i) {
                const float t = dur * static_cast<float>(i) / static_cast<float>(ghosts - 1);
                const auto [pos, sc] = poseAt(t);
                const float a = 0.14f + 0.5f * static_cast<float>(i) / static_cast<float>(ghosts - 1);
                drawArrow(*renderer, pos, 26.0f * sc, tl.valueAt("rot", t), colorAt(t, a));
            }
            // The bright "current" pose at the playhead.
            {
                const auto [pos, sc] = poseAt(playT);
                drawArrow(*renderer, pos, 26.0f * sc, tl.valueAt("rot", playT), colorAt(playT, 1.0f));
                fillCircle(*renderer, pos, 3.0f, render::Color{1, 1, 1, 1});
            }

            // ---- Track editor ---------------------------------------------------------------------
            const float panelTop = previewBot + 20.0f;
            const float panelH = H - panelTop - 20.0f;
            const float laneH = panelH / 4.0f;
            const float laneW = W - 2.0f * marginX;
            for (int l = 0; l < 4; ++l) {
                const Lane& ln = lanes[l];
                const float y0 = panelTop + laneH * static_cast<float>(l);
                const float cyTop = y0 + 22.0f;
                const float cyBot = y0 + laneH - 10.0f;
                // Lane background.
                const render::Point2 bg[4] = {{marginX, y0 + 4.0f},
                                              {marginX + laneW, y0 + 4.0f},
                                              {marginX + laneW, y0 + laneH - 4.0f},
                                              {marginX, y0 + laneH - 4.0f}};
                renderer->drawConvexPolygon(bg, 4, render::Color{0.12f, 0.13f, 0.17f, 1.0f});

                // Value range for this track (from its keyframes) for vertical normalization.
                const anim::Track& tr = tl.track(ln.track);
                float lo = 1e30f, hi = -1e30f;
                for (const auto& k : tr.keys) {
                    lo = std::min(lo, k.value);
                    hi = std::max(hi, k.value);
                }
                if (hi - lo < 1e-6f) {
                    lo -= 1.0f;
                    hi += 1.0f;
                }
                auto mapY = [&](float v) {
                    const float f = (v - lo) / (hi - lo);
                    return cyBot - f * (cyBot - cyTop);
                };
                auto mapX = [&](float t) { return marginX + (t / dur) * laneW; };

                // Curve.
                math::vec2 prev(0, 0);
                bool have = false;
                const int cols = 320;
                for (int i = 0; i <= cols; ++i) {
                    const float t = dur * static_cast<float>(i) / static_cast<float>(cols);
                    const math::vec2 p(mapX(t), mapY(tr.sample(t)));
                    if (have) {
                        thickLine(*renderer, prev, p, 2.0f, ln.col);
                    }
                    prev = p;
                    have = true;
                }
                // Keyframe dots.
                for (const auto& k : tr.keys) {
                    fillCircle(*renderer, math::vec2(mapX(k.time), mapY(k.value)), 4.0f, ln.col);
                }
                // Playhead line + its sampled point.
                thickLine(*renderer, {mapX(playT), y0 + 6.0f}, {mapX(playT), y0 + laneH - 6.0f}, 1.5f,
                          render::Color{0.9f, 0.9f, 0.95f, 0.5f});
                fillCircle(*renderer, math::vec2(mapX(playT), mapY(tr.sample(playT))), 4.5f,
                           render::Color{1, 1, 1, 1});

                font.drawText(*renderer, marginX + 8.0f, y0 + 6.0f, ln.label, ln.col, 0.36f);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  KEYFRAME TIMELINE / SEQUENCER",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 52.0f,
                          "one arrow driven by keyed x / y / rotation / scale / colour tracks; editor "
                          "lanes below with a playhead",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.42f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TIMELINE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
