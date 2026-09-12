// Maz Engine — "RANDOMIZER" (audio::StreamRandomizer, toward Godot's AudioStreamRandomizer)
// Repetitive one-shots (footsteps, hits, UI blips) sound robotic when the identical clip plays every
// time. A randomizer picks from a pool of interchangeable clips and jitters pitch + volume so no two
// triggers sound the same. This demo runs 300 triggers of a 5-clip pool in Random-No-Repeat mode and
// shows the result three ways: a weighted pick-count histogram (LEFT), a pitch×volume variance scatter
// (RIGHT), and the first ~48 picks as a tick strip (BOTTOM) where no two neighbours share a colour —
// proof of the no-repeat rule. Seeded -> deterministic, golden-stable. Run --headless / --frames N.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <array>
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

void dot(render::Renderer& r, math::vec2 c, float radius, render::Color col) {
    const int n = 12;
    render::Point2 pts[12];
    for (int i = 0; i < n; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
        pts[i] = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius};
    }
    r.drawConvexPolygon(pts, static_cast<uint32_t>(n), col);
}

void rect(render::Renderer& r, float x0, float y0, float x1, float y1, render::Color c) {
    const render::Point2 q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    r.drawConvexPolygon(q, 4, c);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RANDOMIZER (audio::StreamRandomizer) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Audio Stream Randomizer";
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

    // ---- Run the randomizer once (deterministic) and record the picks. ------------------------------
    constexpr int kStreams = 5;
    const std::array<float, kStreams> kWeights = {3.0f, 1.0f, 2.0f, 1.0f, 1.5f};
    const std::array<render::Color, kStreams> kPalette = {
        rgba(0.55f, 0.8f, 1.0f, 1), rgba(0.6f, 0.95f, 0.65f, 1), rgba(1.0f, 0.8f, 0.45f, 1),
        rgba(1.0f, 0.6f, 0.75f, 1), rgba(0.8f, 0.7f, 1.0f, 1)};

    audio::StreamRandomizer rnd;
    rnd.mode = audio::RandomizerMode::RandomNoRepeat;
    rnd.randomPitch = 2.0f;            // pitch within [0.5, 2]
    rnd.randomVolumeOffsetDb = 6.0f;   // volume within [-6, 6] dB
    for (int i = 0; i < kStreams; ++i) {
        rnd.addStream(kWeights[static_cast<std::size_t>(i)]);
    }
    rnd.setSeed(0x5EED1234);

    constexpr int kDraws = 300;
    std::array<int, kStreams> counts{};
    std::vector<audio::RandomPick> picks;
    picks.reserve(kDraws);
    for (int i = 0; i < kDraws; ++i) {
        const audio::RandomPick p = rnd.next();
        picks.push_back(p);
        if (p.index >= 0 && p.index < kStreams) {
            ++counts[static_cast<std::size_t>(p.index)];
        }
    }
    int maxCount = 1;
    for (int c : counts) {
        maxCount = std::max(maxCount, c);
    }

    const render::Color kPanel{0.12f, 0.13f, 0.17f, 1.0f};
    const render::Color kAxis{0.3f, 0.34f, 0.42f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  AUDIO STREAM RANDOMIZER",
                          rgba(1, 1, 1, 1), 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "300 triggers of a 5-clip pool (weighted, random-no-repeat) with pitch & "
                          "volume jitter (audio::StreamRandomizer, Godot AudioStreamRandomizer)",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            // ---- LEFT: weighted pick-count histogram ----------------------------------------------
            const float hx0 = 80.0f, hx1 = 600.0f, hy0 = 120.0f, hy1 = 430.0f;
            rect(*renderer, hx0, hy0, hx1, hy1, kPanel);
            thickLine(*renderer, {hx0, hy1}, {hx1, hy1}, 1.5f, kAxis);
            const float slot = (hx1 - hx0) / static_cast<float>(kStreams);
            for (int i = 0; i < kStreams; ++i) {
                const float frac = static_cast<float>(counts[static_cast<std::size_t>(i)]) /
                                   static_cast<float>(maxCount);
                const float bh = frac * (hy1 - hy0 - 30.0f);
                const float bx0 = hx0 + slot * static_cast<float>(i) + slot * 0.2f;
                const float bx1 = hx0 + slot * static_cast<float>(i) + slot * 0.8f;
                rect(*renderer, bx0, hy1 - bh, bx1, hy1, kPalette[static_cast<std::size_t>(i)]);
                const std::string lbl = "#" + std::to_string(i) + " x" +
                                        std::to_string(counts[static_cast<std::size_t>(i)]);
                font.drawText(*renderer, bx0 - 4.0f, hy1 + 6.0f, lbl.c_str(),
                              kPalette[static_cast<std::size_t>(i)], 0.26f);
            }
            font.drawText(*renderer, hx0, hy0 - 26.0f, "pick counts (bar height ~ weight)",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            // ---- RIGHT: pitch x volume scatter ----------------------------------------------------
            const float sx0 = 700.0f, sx1 = 1200.0f, sy0 = 120.0f, sy1 = 430.0f;
            rect(*renderer, sx0, sy0, sx1, sy1, kPanel);
            const float midY = (sy0 + sy1) * 0.5f;
            thickLine(*renderer, {sx0, midY}, {sx1, midY}, 1.0f, kAxis); // 0 dB
            const float midX = (sx0 + sx1) * 0.5f;
            thickLine(*renderer, {midX, sy0}, {midX, sy1}, 1.0f, kAxis); // pitch 1.0
            for (const audio::RandomPick& p : picks) {
                const float px = sx0 + (p.pitchScale - 0.5f) / 1.5f * (sx1 - sx0); // pitch 0.5..2
                const float py = midY - (p.volumeDb / 6.0f) * (sy1 - sy0) * 0.5f;  // vol -6..6
                render::Color c = kPalette[static_cast<std::size_t>(p.index)];
                c.a = 0.7f;
                dot(*renderer, {px, py}, 2.6f, c);
            }
            font.drawText(*renderer, sx0, sy0 - 26.0f, "pitch (x: 0.5-2) x volume (y: +/-6 dB)",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            // ---- BOTTOM: no-repeat tick strip (adjacent colours always differ) --------------------
            const float tx0 = 80.0f, ty0 = 500.0f, ty1 = 560.0f;
            const int kTicks = 48;
            const float tw = (1200.0f - tx0) / static_cast<float>(kTicks);
            for (int i = 0; i < kTicks && i < static_cast<int>(picks.size()); ++i) {
                const int idx = picks[static_cast<std::size_t>(i)].index;
                rect(*renderer, tx0 + tw * static_cast<float>(i) + 1.0f, ty0,
                     tx0 + tw * static_cast<float>(i + 1) - 1.0f, ty1,
                     kPalette[static_cast<std::size_t>(idx)]);
            }
            font.drawText(*renderer, tx0, ty0 - 26.0f,
                          "first 48 picks in order — no two neighbours share a clip (no-repeat)",
                          rgba(0.75f, 0.8f, 0.9f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RANDOMIZER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
