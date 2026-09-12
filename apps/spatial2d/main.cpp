// Maz Engine — "SPATIAL2D" (2D positional audio, toward Godot's AudioStreamPlayer2D)
// A listener sits in a room with several sound sources around it. For each source, audio::spatialize()
// computes a distance ATTENUATION (fades with range, past maxDistance it's silent) and a constant-power
// stereo PAN (a source off to one side is louder in that ear). This demo VISUALIZES that field: each
// source shows a halo whose brightness is its gain and a little L|R bar showing its pan, the listener's
// two range rings are drawn, and a master stereo meter (left vs right) shows the summed mix. The audio
// mixer itself is now stereo and pans each voice, so on a real device the blips actually move; here the
// picture is the verification. Static + deterministic, so the render is golden-stable. --headless/--frames.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>
#include <algorithm>

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col, int sides = 24) {
    std::vector<render::Point2> p(static_cast<size_t>(sides));
    for (int i = 0; i < sides; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        p[static_cast<size_t>(i)] = render::Point2{c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p.data(), static_cast<uint32_t>(sides), col);
}

void ringOutline(render::Renderer& r, math::vec2 c, float rad, float thick, render::Color col,
                 int sides = 48) {
    for (int i = 0; i < sides; ++i) {
        const float a0 = 6.2831853f * static_cast<float>(i) / static_cast<float>(sides);
        const float a1 = 6.2831853f * static_cast<float>(i + 1) / static_cast<float>(sides);
        const math::vec2 p0(c.x + std::cos(a0) * rad, c.y + std::sin(a0) * rad);
        const math::vec2 p1(c.x + std::cos(a1) * rad, c.y + std::sin(a1) * rad);
        math::vec2 d = p1 - p0;
        const float len = std::sqrt(d.x * d.x + d.y * d.y);
        if (len < 1e-4f) continue;
        d /= len;
        const math::vec2 n(-d.y * thick * 0.5f, d.x * thick * 0.5f);
        const render::Point2 q[4] = {{p0.x + n.x, p0.y + n.y},
                                     {p1.x + n.x, p1.y + n.y},
                                     {p1.x - n.x, p1.y - n.y},
                                     {p0.x - n.x, p0.y - n.y}};
        r.drawConvexPolygon(q, 4, col);
    }
}

void rect(render::Renderer& r, float x, float y, float w, float h, render::Color col) {
    const render::Point2 q[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(q, 4, col);
}

struct Source {
    math::vec2 pos;
    render::Color color;
    float freq;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SPATIAL2D (2D positional audio) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — 2D Positional Audio";
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

    audio::Audio sound;
    const bool haveAudio = sound.init();

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

    audio::Listener2D listener;
    listener.pos = math::vec2(sw * 0.34f, sh * 0.56f);
    listener.right = math::vec2(1.0f, 0.0f); // faces up; right is +x

    const float refDist = 70.0f;
    const float maxDist = 430.0f;

    const std::vector<Source> sources = {
        {math::vec2(sw * 0.34f - 300.0f, sh * 0.56f - 40.0f), {0.95f, 0.5f, 0.5f, 1}, 330.0f}, // far left
        {math::vec2(sw * 0.34f + 250.0f, sh * 0.56f + 60.0f), {0.55f, 0.7f, 0.95f, 1}, 440.0f}, // right
        {math::vec2(sw * 0.34f + 60.0f, sh * 0.56f - 150.0f), {0.6f, 0.9f, 0.55f, 1}, 550.0f},  // near-ish up
        {math::vec2(sw * 0.34f - 120.0f, sh * 0.56f + 180.0f), {0.9f, 0.8f, 0.4f, 1}, 392.0f},  // lower-left
        {math::vec2(sw * 0.34f + 470.0f, sh * 0.56f - 10.0f), {0.85f, 0.6f, 0.95f, 1}, 660.0f},  // far right (faint)
    };

    double blipTimer = 0.0;
    size_t blipIdx = 0;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        // On a real device, cycle a spatialized blip through the sources (inaudible in CI; no visual effect).
        if (haveAudio) {
            blipTimer += clock.frameDelta();
            if (blipTimer > 0.5) {
                blipTimer = 0.0;
                const Source& s = sources[blipIdx % sources.size()];
                const auto g = audio::spatialize(listener, s.pos, 0.6f, refDist, maxDist);
                if (g.left + g.right > 0.001f) {
                    audio::SoundDesc d;
                    d.wave = audio::Wave::Sine;
                    d.freq = s.freq;
                    d.duration = 0.25f;
                    d.volume = 1.0f;
                    d.leftGain = g.left;
                    d.rightGain = g.right;
                    sound.play(d);
                }
                ++blipIdx;
            }
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Listener range rings (reference = full volume, max = silence beyond).
            ringOutline(*renderer, listener.pos, refDist, 2.0f, render::Color{0.35f, 0.6f, 0.5f, 1});
            ringOutline(*renderer, listener.pos, maxDist, 2.0f, render::Color{0.3f, 0.32f, 0.4f, 1});

            float sumL = 0.0f, sumR = 0.0f;
            for (const Source& s : sources) {
                const auto g = audio::spatialize(listener, s.pos, 0.9f, refDist, maxDist);
                const float gain = g.left + g.right; // ~ overall loudness
                sumL += g.left;
                sumR += g.right;

                // Halo: brightness/size scale with the source's gain.
                render::Color halo = s.color;
                halo.a = 0.10f + 0.5f * std::min(1.0f, gain);
                fillCircle(*renderer, s.pos, 16.0f + 34.0f * std::min(1.0f, gain), halo);
                fillCircle(*renderer, s.pos, 9.0f, s.color);

                // Pan bar under the source: left (blue) vs right (red) split.
                const float bw = 60.0f, bh = 7.0f;
                const float total = g.left + g.right;
                const float lf = total > 1e-5f ? g.left / total : 0.5f;
                rect(*renderer, s.pos.x - bw * 0.5f, s.pos.y + 26.0f, bw * lf, bh,
                     render::Color{0.45f, 0.6f, 0.95f, 1});
                rect(*renderer, s.pos.x - bw * 0.5f + bw * lf, s.pos.y + 26.0f, bw * (1.0f - lf), bh,
                     render::Color{0.95f, 0.5f, 0.45f, 1});
            }

            // Listener: a triangle pointing "up" (its facing) + a dot.
            const render::Point2 tri[3] = {{listener.pos.x, listener.pos.y - 18.0f},
                                           {listener.pos.x - 12.0f, listener.pos.y + 12.0f},
                                           {listener.pos.x + 12.0f, listener.pos.y + 12.0f}};
            renderer->drawConvexPolygon(tri, 3, render::Color{1.0f, 0.95f, 0.7f, 1});

            // Master stereo meter (top-right): two vertical bars for the summed L and R.
            const float mx = sw - 150.0f, my = 120.0f, mh = 180.0f, mw = 46.0f;
            rect(*renderer, mx, my, mw, mh, render::Color{0.16f, 0.17f, 0.22f, 1});
            rect(*renderer, mx + mw + 14.0f, my, mw, mh, render::Color{0.16f, 0.17f, 0.22f, 1});
            const float lFill = mh * std::min(1.0f, sumL);
            const float rFill = mh * std::min(1.0f, sumR);
            rect(*renderer, mx, my + (mh - lFill), mw, lFill, render::Color{0.45f, 0.6f, 0.95f, 1});
            rect(*renderer, mx + mw + 14.0f, my + (mh - rFill), mw, rFill,
                 render::Color{0.95f, 0.5f, 0.45f, 1});
            font.drawText(*renderer, mx + 14.0f, my + mh + 8.0f, "L", render::Color{0.7f, 0.8f, 1, 1}, 0.5f);
            font.drawText(*renderer, mx + mw + 26.0f, my + mh + 8.0f, "R",
                          render::Color{1, 0.8f, 0.75f, 1}, 0.5f);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  2D POSITIONAL AUDIO",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "per-source distance attenuation + constant-power stereo pan (audio::spatialize)",
                          render::Color{0.8f, 0.9f, 1.0f, 1}, 0.44f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SPATIAL2D shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    if (haveAudio) {
        sound.shutdown();
    }
    renderer->shutdown();
    window.shutdown();
    return 0;
}
