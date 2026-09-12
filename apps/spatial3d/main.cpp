// Maz Engine — "SPATIAL3D" (3D positional audio, toward Godot's AudioStreamPlayer3D)
// You can't hear a golden image, so this VISUALISES what audio::Spatial3D computes for a set of moving
// sound sources heard by one listener. Top-down radar: the listener sits at the centre facing up; each
// source is placed by its world x (left/right) and z (front/back). For every source the demo draws a
// disc sized by its distance ATTENUATION (nearer = louder = bigger), tinted by its DOPPLER pitch (warm =
// approaching / raising pitch, cool = receding / lowering pitch), a velocity arrow, and a two-bar L/R
// stereo meter showing the constant-power PAN (a source on the right drives the right bar taller). A
// readout lists each source's L/R gain and pitch. Everything is computed once and drawn statically ->
// deterministic, golden-stable. Run --headless / --frames for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>
#include <algorithm>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    if (rad < 1.0f) {
        rad = 1.0f;
    }
    const int seg = 26;
    render::Point2 p[28];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

void ringCircle(render::Renderer& r, math::vec2 c, float rad, float w, render::Color col) {
    const int seg = 36;
    for (int i = 0; i < seg; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(seg);
        const render::Point2 q[4] = {{c.x + std::cos(a0) * rad, c.y + std::sin(a0) * rad},
                                     {c.x + std::cos(a1) * rad, c.y + std::sin(a1) * rad},
                                     {c.x + std::cos(a1) * (rad - w), c.y + std::sin(a1) * (rad - w)},
                                     {c.x + std::cos(a0) * (rad - w), c.y + std::sin(a0) * (rad - w)}};
        r.drawConvexPolygon(q, 4, col);
    }
}

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
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

struct Src {
    const char* name;
    audio::Source3D s;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SPATIAL3D (3D positional audio) starting");

    // Listener at the origin, facing -z (into the screen / "up" on the radar), up = +y.
    audio::Listener3D listener;
    listener.pos = math::vec3(0, 0, 0);
    listener.forward = math::vec3(0, 0, -1);
    listener.up = math::vec3(0, 1, 0);
    listener.velocity = math::vec3(0, 0, 0);

    audio::SpatialConfig scfg;
    scfg.baseVolume = 1.0f;
    scfg.refDistance = 2.0f;
    scfg.maxDistance = 42.0f;
    scfg.rolloff = 1.0f;
    scfg.model = audio::Attenuation3D::Inverse;
    scfg.doppler = true;

    // Six sources around the listener with various positions + velocities (world units).
    std::vector<Src> srcs = {
        {"FRONT-LEFT (static)", {math::vec3(-9, 0, -11), math::vec3(0, 0, 0)}},
        {"RIGHT (approaching)", {math::vec3(14, 0, -3), math::vec3(-70, 0, 0)}},
        {"FAR-BEHIND (static)", {math::vec3(6, 0, 16), math::vec3(0, 0, 0)}},
        {"FRONT-RIGHT (receding)", {math::vec3(11, 0, -9), math::vec3(55, 0, -20)}},
        {"CLOSE-LEFT (static)", {math::vec3(-5, 0, 3), math::vec3(0, 0, 0)}},
        {"AHEAD (fast approach)", {math::vec3(0, 0, -18), math::vec3(0, 0, 95)}},
    };

    std::vector<audio::SpatialMix> mixes;
    for (const Src& sr : srcs) {
        mixes.push_back(audio::computeSpatialMix(listener, sr.s, scfg));
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 3D Spatial Audio";
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

    const float cx = 470.0f, cy = 320.0f; // radar centre on screen
    const float scale = 13.0f;            // px per world unit
    auto toScreen = [&](math::vec3 world) {
        // world x -> screen right; world z -> screen down (so -z / "in front" is up on the radar).
        return math::vec2(cx + world.x * scale, cy + world.z * scale);
    };

    // Colour a source by its doppler pitch: warm (red) when raising pitch, cool (blue) when lowering.
    auto dopplerColor = [](float pitch) {
        const float k = std::min(std::max((pitch - 1.0f) * 6.0f, -1.0f), 1.0f); // -1..+1
        if (k > 0.0f) {
            return render::Color{1.0f, 0.75f - 0.55f * k, 0.55f - 0.5f * k, 1.0f}; // toward red
        }
        return render::Color{0.55f + 0.45f * k, 0.75f + 0.2f * k, 1.0f, 1.0f}; // toward blue
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  3D SPATIAL AUDIO",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "distance attenuation + listener-relative stereo pan + doppler pitch "
                          "(audio::Spatial3D)",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.36f);

            // Radar range rings (ref + max distance).
            ringCircle(*renderer, math::vec2(cx, cy), scfg.refDistance * scale, 1.5f,
                       render::Color{0.3f, 0.35f, 0.42f, 0.7f});
            ringCircle(*renderer, math::vec2(cx, cy), scfg.maxDistance * scale, 1.5f,
                       render::Color{0.22f, 0.26f, 0.32f, 0.6f});

            // Sources.
            for (std::size_t i = 0; i < srcs.size(); ++i) {
                const audio::SpatialMix& m = mixes[i];
                const math::vec2 sp = toScreen(srcs[i].s.pos);
                const float total = m.left + m.right;

                // Line from listener to source.
                thickLine(*renderer, math::vec2(cx, cy), sp, 1.2f,
                          render::Color{0.35f, 0.4f, 0.48f, 0.5f});

                // Disc sized by loudness (attenuation), tinted by doppler.
                const float rad = 6.0f + total * 22.0f;
                const render::Color col = dopplerColor(m.pitch);
                fillCircle(*renderer, sp, rad, col);
                fillCircle(*renderer, sp, rad * 0.5f, render::Color{1, 1, 1, 0.85f});

                // Velocity arrow (world velocity mapped to the radar).
                const math::vec3 v = srcs[i].s.velocity;
                if (std::sqrt(v.x * v.x + v.z * v.z) > 1e-3f) {
                    const math::vec2 tip(sp.x + v.x * 0.35f, sp.y + v.z * 0.35f);
                    thickLine(*renderer, sp, tip, 2.5f, render::Color{1.0f, 0.9f, 0.5f, 0.9f});
                }

                // L/R stereo meter just below the disc.
                const float mx = sp.x - 9.0f, my = sp.y + rad + 6.0f, bw = 7.0f, bh = 34.0f;
                fillRect(*renderer, mx, my, bw, bh, render::Color{0.18f, 0.2f, 0.24f, 1});
                fillRect(*renderer, mx + 10.0f, my, bw, bh, render::Color{0.18f, 0.2f, 0.24f, 1});
                fillRect(*renderer, mx, my + bh * (1.0f - m.left), bw, bh * m.left,
                         render::Color{0.5f, 0.8f, 1.0f, 1});
                fillRect(*renderer, mx + 10.0f, my + bh * (1.0f - m.right), bw, bh * m.right,
                         render::Color{1.0f, 0.8f, 0.4f, 1});
            }

            // Listener triangle at the centre, pointing up (forward).
            const render::Point2 tri[3] = {{cx, cy - 15.0f}, {cx - 11.0f, cy + 11.0f},
                                           {cx + 11.0f, cy + 11.0f}};
            renderer->drawConvexPolygon(tri, 3, render::Color{0.9f, 0.95f, 1.0f, 1.0f});

            // Readout panel on the right.
            float ty = 96.0f;
            font.drawText(*renderer, 940.0f, ty, "per-source mix:", render::Color{0.85f, 0.88f, 0.95f, 1},
                          0.4f);
            ty += 34.0f;
            for (std::size_t i = 0; i < srcs.size(); ++i) {
                const audio::SpatialMix& m = mixes[i];
                font.drawText(*renderer, 940.0f, ty, srcs[i].name, dopplerColor(m.pitch), 0.32f);
                char buf[96];
                std::snprintf(buf, sizeof(buf), "L %.2f  R %.2f   pitch %.2f", static_cast<double>(m.left),
                              static_cast<double>(m.right), static_cast<double>(m.pitch));
                font.drawText(*renderer, 940.0f, ty + 22.0f, buf, render::Color{0.72f, 0.76f, 0.84f, 1},
                              0.3f);
                ty += 54.0f;
            }

            font.drawText(*renderer, 60.0f, 636.0f,
                          "L/R bars = constant-power pan   disc size = attenuation   warm = pitch up "
                          "(approaching), cool = pitch down",
                          render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SPATIAL3D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
