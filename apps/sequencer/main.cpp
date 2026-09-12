// Maz Engine — "SEQUENCER" (call-method / trigger tracks, toward Godot AnimationPlayer method tracks)
// M100's Timeline gave VALUE tracks (interpolate a property); this shows the other half — METHOD tracks
// that FIRE at a keyframe time (anim::MethodTimeline). Four lanes (kick / snare / hat / clap) each carry
// a trigger track; as one shared playhead sweeps the 4-beat loop, each marker fires exactly once when the
// head passes it — a drum machine. The whole run is stepped ONCE at startup with a fixed timestep: we
// draw the lanes with their markers, the playhead at its final position (markers behind it just fired,
// bright; ahead of it pending, dim), the per-lane fire counts, and a strip of the most recent fires in
// order. Deterministic -> golden-stable.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

namespace {

void fillRect(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

void fillCircle(render::Renderer& r, math::vec2 c, float rad, render::Color col) {
    const int seg = 16;
    render::Point2 p[18];
    p[0] = {c.x, c.y};
    for (int i = 0; i <= seg; ++i) {
        const float a = 6.2831853f * static_cast<float>(i) / static_cast<float>(seg);
        p[i + 1] = {c.x + std::cos(a) * rad, c.y + std::sin(a) * rad};
    }
    r.drawConvexPolygon(p, static_cast<uint32_t>(seg + 2), col);
}

struct Lane {
    const char* name;
    render::Color color;
    std::vector<float> times; // trigger times within the loop
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SEQUENCER (trigger / method tracks) starting");

    const float loopLen = 4.0f; // 4 beats
    std::vector<Lane> lanes = {
        {"KICK", {0.95f, 0.45f, 0.35f, 1.0f}, {0.0f, 2.0f}},
        {"SNARE", {0.95f, 0.82f, 0.35f, 1.0f}, {1.0f, 3.0f}},
        {"HAT", {0.40f, 0.85f, 0.95f, 1.0f}, {0.0f, 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f}},
        {"CLAP", {0.85f, 0.45f, 0.95f, 1.0f}, {3.5f}},
    };

    // Build a MethodTimeline per lane (id = lane index) and step them all in lockstep.
    std::vector<anim::MethodTimeline> mts(lanes.size());
    for (std::size_t i = 0; i < lanes.size(); ++i) {
        mts[i].length = loopLen;
        mts[i].loop = anim::Loop::Repeat;
        for (float t : lanes[i].times) {
            mts[i].track.add(t, static_cast<int>(i));
        }
    }

    std::vector<int> counts(lanes.size(), 0);
    std::vector<int> fireLog; // lane ids in fire order
    const float dt = 1.0f / 60.0f;
    const int steps = 558; // 558 * (1/60) = 9.3s -> playhead ends at fmod(9.3,4)=1.3
    for (int s = 0; s < steps; ++s) {
        for (std::size_t i = 0; i < mts.size(); ++i) {
            std::vector<int> fired;
            mts[i].update(dt, fired);
            for (int id : fired) {
                counts[static_cast<std::size_t>(id)]++;
                fireLog.push_back(id);
            }
        }
    }
    const float totalTime = static_cast<float>(steps) * dt;
    const float finalHead = std::fmod(totalTime, loopLen);
    const int loopsDone = static_cast<int>(totalTime / loopLen);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Trigger / Method Tracks";
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

    const float trackX0 = 160.0f, trackX1 = 1080.0f;
    auto timeToX = [&](float t) {
        return trackX0 + (t / loopLen) * (trackX1 - trackX0);
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.06f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TRIGGER / METHOD TRACKS",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 50.0f,
                          "each lane fires its markers as the playhead sweeps - a method track, the event "
                          "half of Godot's AnimationPlayer",
                          render::Color{0.8f, 0.85f, 0.95f, 1}, 0.4f);

            const float laneY0 = 150.0f, laneH = 88.0f;

            // Beat grid behind the lanes.
            for (int b = 0; b <= 4; ++b) {
                const float x = timeToX(static_cast<float>(b));
                fillRect(*renderer, x - 0.5f, laneY0 - 10.0f, 1.0f,
                         laneH * static_cast<float>(lanes.size()) + 12.0f,
                         render::Color{0.16f, 0.17f, 0.22f, 1.0f});
            }

            for (std::size_t i = 0; i < lanes.size(); ++i) {
                const Lane& L = lanes[i];
                const float cy = laneY0 + static_cast<float>(i) * laneH + laneH * 0.5f - 20.0f;
                // Lane label + track baseline.
                font.drawText(*renderer, 30.0f, cy - 10.0f, L.name, L.color, 0.44f);
                fillRect(*renderer, trackX0, cy - 1.0f, trackX1 - trackX0, 2.0f,
                         render::Color{0.22f, 0.23f, 0.28f, 1.0f});

                // Markers: bright if the playhead has already passed them this loop (just fired), dim if
                // still ahead (pending).
                for (float t : L.times) {
                    const math::vec2 m(timeToX(t), cy);
                    const bool fired = t < finalHead;
                    const render::Color base = L.color;
                    if (fired) {
                        fillCircle(*renderer, m, 12.0f,
                                   render::Color{base.r, base.g, base.b, 0.30f}); // glow
                        fillCircle(*renderer, m, 7.0f, base);
                    } else {
                        fillCircle(*renderer, m, 6.0f,
                                   render::Color{base.r * 0.45f, base.g * 0.45f, base.b * 0.45f, 1.0f});
                    }
                }

                // Per-lane fire count.
                const std::string cnt = std::to_string(counts[i]) + "x";
                font.drawText(*renderer, trackX1 + 24.0f, cy - 10.0f, cnt.c_str(),
                              render::Color{0.82f, 0.86f, 0.94f, 1}, 0.4f);
            }

            // Playhead.
            const float phx = timeToX(finalHead);
            fillRect(*renderer, phx - 1.5f, laneY0 - 14.0f, 3.0f,
                     laneH * static_cast<float>(lanes.size()) + 8.0f,
                     render::Color{1.0f, 1.0f, 1.0f, 0.9f});
            fillCircle(*renderer, math::vec2(phx, laneY0 - 16.0f), 6.0f,
                       render::Color{1, 1, 1, 1});

            // Recent-fire strip: the last N fired lane events, oldest -> newest left to right.
            font.drawText(*renderer, 30.0f, 560.0f, "recent fires:",
                          render::Color{0.7f, 0.75f, 0.85f, 1}, 0.36f);
            const int show = 22;
            const int start = static_cast<int>(fireLog.size()) > show
                                  ? static_cast<int>(fireLog.size()) - show
                                  : 0;
            for (int k = start; k < static_cast<int>(fireLog.size()); ++k) {
                const Lane& L = lanes[static_cast<std::size_t>(fireLog[static_cast<std::size_t>(k)])];
                const float x = 210.0f + static_cast<float>(k - start) * 24.0f;
                fillRect(*renderer, x, 552.0f, 18.0f, 24.0f, L.color);
            }

            const std::string head = "playhead: loop " + std::to_string(loopsDone + 1) + ", beat " +
                                     std::to_string(finalHead).substr(0, 4) + "   (markers behind it just "
                                     "fired; ahead are pending)";
            font.drawText(*renderer, 30.0f, 610.0f, head.c_str(),
                          render::Color{0.72f, 0.78f, 0.88f, 1}, 0.34f);
            font.drawText(*renderer, 30.0f, 642.0f,
                          "loop repeats, so each marker fires once per pass - fire-once, no double-trigger "
                          "at the wrap (anim::MethodTimeline)",
                          render::Color{0.6f, 0.65f, 0.75f, 1}, 0.32f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SEQUENCER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
