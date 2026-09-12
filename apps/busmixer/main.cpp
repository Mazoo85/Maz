// Maz Engine — "BUSMIXER" (audio::BusGraph, toward Godot's AudioServer bus layout)
// A3 of the audio deep-dive: the multi-bus router. Every voice plays into a named bus; each bus has a
// dB volume fader, mute / solo, an effect chain, and a SEND that routes its output into another bus,
// all rooted at Master. This demo builds a small mixer — Music, SFX, Voice (nested into SFX), Ambience
// (with effects, muted), and Master — and draws it as channel strips: fader knob at the volume, M/S
// state, effect chips, a live level meter, and the send target under each strip. The meter levels are
// computed by running a fixed test signal through the real BusGraph to steady state, so they reflect the
// actual routing (Voice folds into SFX; the muted Ambience reads silent). Static config + deterministic
// levels -> golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>
#include <algorithm>

#include <cmath>
#include <cstdint>
#include <cstdio>
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

void fillRect(render::Renderer& r, float x0, float y0, float x1, float y1, render::Color c) {
    const render::Point2 q[4] = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}};
    r.drawConvexPolygon(q, 4, c);
}

void outlineRect(render::Renderer& r, float x0, float y0, float x1, float y1, float w, render::Color c) {
    thickLine(r, {x0, y0}, {x1, y0}, w, c);
    thickLine(r, {x1, y0}, {x1, y1}, w, c);
    thickLine(r, {x1, y1}, {x0, y1}, w, c);
    thickLine(r, {x0, y1}, {x0, y0}, w, c);
}

render::Color rgba(float r, float g, float b, float a) { return render::Color{r, g, b, a}; }

struct Strip {
    std::string name;
    float volumeDb;
    bool mute;
    bool solo;
    std::string sendName;
    std::vector<std::string> effects;
    int busIndex;
    float level; // filled after running the graph
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BUSMIXER (audio::BusGraph) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Audio Bus Mixer";
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

    const float sr = 44100.0f;

    // Build the mixer graph.
    audio::BusGraph graph;
    const int master = 0;
    const int music = graph.addBus("Music");
    const int sfx = graph.addBus("SFX");
    const int voice = graph.addBus("Voice", sfx); // nested: Voice -> SFX -> Master
    const int amb = graph.addBus("Ambience");
    graph.setVolumeDb(music, -3.0f);
    graph.setVolumeDb(sfx, -1.0f);
    graph.setVolumeDb(voice, -5.0f);
    graph.setVolumeDb(amb, -8.0f);
    graph.setMute(amb, true);
    // Ambience carries an effect chain (low-pass then reverb).
    graph.addEffect(amb, std::make_unique<audio::BiquadEffect>(audio::Biquad::lowpass(1200.0f, 0.707f, sr)));
    {
        audio::Reverb rv;
        graph.addEffect(amb, std::make_unique<audio::ReverbEffect>(rv));
    }

    // Run a fixed test signal to steady state and capture per-bus meter levels.
    for (int i = 0; i < 6000; ++i) {
        graph.pushInput(music, 0.55f);
        graph.pushInput(sfx, 0.5f);
        graph.pushInput(voice, 0.45f);
        graph.pushInput(amb, 0.6f);
        graph.process();
    }

    std::vector<Strip> strips = {
        {"Music", graph.volumeDb(music), graph.muted(music), graph.soloed(music), "Master", {}, music, 0},
        {"SFX", graph.volumeDb(sfx), graph.muted(sfx), graph.soloed(sfx), "Master", {}, sfx, 0},
        {"Voice", graph.volumeDb(voice), graph.muted(voice), graph.soloed(voice), "SFX", {}, voice, 0},
        {"Ambience", graph.volumeDb(amb), graph.muted(amb), graph.soloed(amb), "Master",
         {"LowPass", "Reverb"}, amb, 0},
        {"Master", graph.volumeDb(master), graph.muted(master), graph.soloed(master), "Output", {},
         master, 0},
    };
    for (Strip& s : strips) {
        s.level = graph.busLevel(s.busIndex);
    }

    // dB fader mapping.
    const float dbTop = 6.0f, dbBot = -30.0f;
    const float faderTop = 150.0f, faderBot = 470.0f;
    auto yOfDb = [&](float db) {
        const float t = (dbTop - db) / (dbTop - dbBot);
        return faderTop + t * (faderBot - faderTop);
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.09f, 0.10f, 0.13f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  AUDIO BUS MIXER", rgba(1, 1, 1, 1),
                          0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "audio::BusGraph (Godot AudioServer): named buses with dB fader, mute/solo, "
                          "effect chain, and a send routed to Master.",
                          rgba(0.8f, 0.86f, 0.95f, 1), 0.3f);

            const float stripW = 210.0f, gap = 24.0f, x0 = 60.0f, top = 96.0f, bot = 620.0f;
            for (std::size_t i = 0; i < strips.size(); ++i) {
                const Strip& s = strips[i];
                const float sx = x0 + static_cast<float>(i) * (stripW + gap);
                const bool isMaster = (s.name == "Master");
                const render::Color panel =
                    isMaster ? rgba(0.17f, 0.19f, 0.26f, 1) : rgba(0.13f, 0.14f, 0.19f, 1);
                fillRect(*renderer, sx, top, sx + stripW, bot, panel);
                outlineRect(*renderer, sx, top, sx + stripW, bot, 1.5f,
                            isMaster ? rgba(0.5f, 0.62f, 0.85f, 1) : rgba(0.25f, 0.28f, 0.36f, 1));

                // Name header.
                font.drawText(*renderer, sx + 14.0f, top + 10.0f, s.name.c_str(), rgba(1, 1, 1, 1), 0.42f);

                // Effect chips.
                float chipY = top + 48.0f;
                for (const std::string& e : s.effects) {
                    fillRect(*renderer, sx + 14.0f, chipY, sx + stripW - 14.0f, chipY + 26.0f,
                             rgba(0.28f, 0.34f, 0.5f, 1));
                    font.drawText(*renderer, sx + 22.0f, chipY + 4.0f, e.c_str(),
                                  rgba(0.85f, 0.9f, 1.0f, 1), 0.3f);
                    chipY += 32.0f;
                }

                // Fader track + knob.
                const float trackX = sx + 58.0f;
                thickLine(*renderer, {trackX, faderTop}, {trackX, faderBot}, 3.0f,
                          rgba(0.3f, 0.32f, 0.4f, 1));
                for (float db = dbTop; db >= dbBot; db -= 6.0f) {
                    const float ty = yOfDb(db);
                    thickLine(*renderer, {trackX - 8.0f, ty}, {trackX - 2.0f, ty}, 1.5f,
                              rgba(0.4f, 0.43f, 0.52f, 1));
                }
                const float knobY = yOfDb(s.volumeDb);
                fillRect(*renderer, trackX - 18.0f, knobY - 7.0f, trackX + 18.0f, knobY + 7.0f,
                         rgba(0.7f, 0.8f, 0.95f, 1));
                char vb[16];
                std::snprintf(vb, sizeof(vb), "%+.0f dB", static_cast<double>(s.volumeDb));
                font.drawText(*renderer, sx + 14.0f, faderBot + 12.0f, vb, rgba(0.75f, 0.82f, 0.95f, 1),
                              0.3f);

                // Level meter (to the right of the fader).
                const float meterX0 = sx + 120.0f, meterX1 = sx + 150.0f;
                fillRect(*renderer, meterX0, faderTop, meterX1, faderBot, rgba(0.06f, 0.07f, 0.09f, 1));
                const float lv = std::min(s.level, 1.0f);
                const float fillY = faderBot - lv * (faderBot - faderTop);
                const render::Color meterC = lv > 0.85f ? rgba(1.0f, 0.4f, 0.35f, 1)
                                             : lv > 0.6f ? rgba(1.0f, 0.85f, 0.35f, 1)
                                                         : rgba(0.4f, 0.9f, 0.5f, 1);
                if (lv > 0.001f) {
                    fillRect(*renderer, meterX0 + 2.0f, fillY, meterX1 - 2.0f, faderBot, meterC);
                }
                outlineRect(*renderer, meterX0, faderTop, meterX1, faderBot, 1.0f,
                            rgba(0.3f, 0.33f, 0.4f, 1));

                // Mute / Solo indicators.
                const float msY = faderBot + 44.0f;
                const render::Color mOn = s.mute ? rgba(1.0f, 0.45f, 0.4f, 1) : rgba(0.25f, 0.27f, 0.34f, 1);
                const render::Color sOn = s.solo ? rgba(1.0f, 0.85f, 0.35f, 1) : rgba(0.25f, 0.27f, 0.34f, 1);
                fillRect(*renderer, sx + 14.0f, msY, sx + 52.0f, msY + 30.0f, mOn);
                font.drawText(*renderer, sx + 26.0f, msY + 4.0f, "M", rgba(0.05f, 0.05f, 0.08f, 1), 0.34f);
                fillRect(*renderer, sx + 62.0f, msY, sx + 100.0f, msY + 30.0f, sOn);
                font.drawText(*renderer, sx + 74.0f, msY + 4.0f, "S", rgba(0.05f, 0.05f, 0.08f, 1), 0.34f);

                // Send target.
                std::string arrow = std::string("-> ") + s.sendName;
                font.drawText(*renderer, sx + 14.0f, bot - 30.0f, arrow.c_str(),
                              rgba(0.6f, 0.72f, 0.62f, 1), 0.32f);
            }

            font.drawText(*renderer, 16.0f, 664.0f,
                          "green/amber/red bar = live level  |  Voice folds into SFX  |  Ambience is "
                          "muted (silent) with a LowPass+Reverb chain",
                          rgba(0.75f, 0.82f, 0.92f, 1), 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BUSMIXER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
