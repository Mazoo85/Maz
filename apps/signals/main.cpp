// Maz Engine — "SIGNALS" (per-object named signals, toward Godot's signal/connect/emit)
// Each object owns named channels that carry typed args; others connect handlers to a SPECIFIC emitter's
// signal. This wires a tiny scenario — a Button.pressed drives Player.hpChanged(int) and Player.died — and
// runs a fixed script, capturing an event LOG that shows the three behaviours: immediate handlers fire in
// order, a ONE-SHOT "GAME OVER" fires only once, and a DEFERRED audit is queued on emit and only runs at
// flushDeferred(). The left panel draws the connection graph (emitters → handlers, connectors via
// render::buildPolyline); the right panel is the log. Static → deterministic golden. Run --headless.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

void box(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
         render::Color fill, render::Color border) {
    fillRect(r, white, x, y, w, h, fill);
    fillRect(r, white, x, y, w, 2.0f, border);
    fillRect(r, white, x, y + h - 2.0f, w, 2.0f, border);
    fillRect(r, white, x, y, 2.0f, h, border);
    fillRect(r, white, x + w - 2.0f, y, 2.0f, h, border);
}

// A bowed connector from a to b, stroked with the Line2D polyline builder.
void connector(render::Renderer& r, math::vec2 a, math::vec2 b, render::Color col) {
    const math::vec2 mid((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f - 14.0f);
    render::PolylineStyle s;
    s.width = 3.0f;
    s.joint = render::JointMode::Round;
    s.cap = render::CapMode::Round;
    const std::vector<math::vec2> tris = render::buildPolyline({a, mid, b}, s);
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const render::Point2 t[3] = {{tris[i].x, tris[i].y},
                                     {tris[i + 1].x, tris[i + 1].y},
                                     {tris[i + 2].x, tris[i + 2].y}};
        r.drawConvexPolygon(t, 3, col);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SIGNALS (named signals) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Signals";
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
    render::TextureHandle white = whiteTex(*renderer);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // --- Wire the scenario and run the fixed script, capturing an event log. ---
    core::Signal<> pressed;
    core::Signal<int> hpChanged;
    core::Signal<> died;
    int hp = 100;
    std::vector<std::string> log;

    pressed.connect([&]() {
        hp -= 25;
        if (hp < 0) {
            hp = 0;
        }
        hpChanged.emit(hp);
        if (hp == 0) {
            died.emit();
        }
    });
    hpChanged.connect([&](int v) { log.push_back("hpChanged -> HP " + std::to_string(v)); });
    hpChanged.connectDeferred([&](int v) { log.push_back("  [deferred] audit HP=" + std::to_string(v)); });
    const core::ConnectionId diedConn = died.connectOnce([&]() { log.push_back("died -> GAME OVER (one-shot)"); });

    for (int i = 0; i < 4; ++i) {
        pressed.emit();
    }
    died.emit(); // one-shot already fired → no second GAME OVER
    log.push_back("-- flushDeferred --");
    hpChanged.flushDeferred();

    const bool diedStill = died.isConnected(diedConn);
    const std::size_t pC = pressed.connectionCount();
    const std::size_t hC = hpChanged.connectionCount();
    const std::size_t dC = died.connectionCount();

    struct Node {
        float x, y, w, h;
        const char* label;
    };
    const Node emitters[3] = {
        {60.0f, 130.0f, 240.0f, 46.0f, "Button.pressed"},
        {60.0f, 214.0f, 240.0f, 46.0f, "Player.hpChanged(int)"},
        {60.0f, 298.0f, 240.0f, 46.0f, "Player.died"},
    };
    const Node handlers[4] = {
        {520.0f, 108.0f, 250.0f, 42.0f, "damage()"},
        {520.0f, 176.0f, 250.0f, 42.0f, "update bar + log"},
        {520.0f, 244.0f, 250.0f, 42.0f, "audit  (deferred)"},
        {520.0f, 312.0f, 250.0f, 42.0f, "GAME OVER  (one-shot)"},
    };
    // Which emitter feeds which handler (index pairs) + a colour.
    struct Wire {
        int e, h;
        render::Color col;
    };
    const Wire wires[4] = {
        {0, 0, {0.5f, 0.8f, 1.0f, 1.0f}},
        {1, 1, {0.5f, 0.85f, 0.55f, 1.0f}},
        {1, 2, {0.9f, 0.75f, 0.4f, 1.0f}},
        {2, 3, {0.95f, 0.5f, 0.5f, 1.0f}},
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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SIGNALS (connect / emit)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "per-object named signals: immediate + one-shot + deferred (core::Signal)",
                          render::Color{0.78f, 0.83f, 0.95f, 1}, 0.34f);

            // Connectors first (behind the boxes).
            for (const Wire& w : wires) {
                const Node& e = emitters[w.e];
                const Node& h = handlers[w.h];
                connector(*renderer, math::vec2(e.x + e.w, e.y + e.h * 0.5f),
                          math::vec2(h.x, h.y + h.h * 0.5f), w.col);
            }

            // Emitter + handler boxes.
            for (const Node& e : emitters) {
                box(*renderer, white, e.x, e.y, e.w, e.h, render::Color{0.16f, 0.2f, 0.28f, 1.0f},
                    render::Color{0.4f, 0.55f, 0.8f, 1.0f});
                font.drawText(*renderer, e.x + 12.0f, e.y + 13.0f, e.label,
                              render::Color{0.85f, 0.9f, 1.0f, 1}, 0.3f);
            }
            for (const Node& h : handlers) {
                box(*renderer, white, h.x, h.y, h.w, h.h, render::Color{0.18f, 0.22f, 0.2f, 1.0f},
                    render::Color{0.45f, 0.65f, 0.5f, 1.0f});
                font.drawText(*renderer, h.x + 12.0f, h.y + 11.0f, h.label,
                              render::Color{0.85f, 0.95f, 0.88f, 1}, 0.3f);
            }
            font.drawText(*renderer, 60.0f, 100.0f, "EMITTERS", render::Color{0.6f, 0.7f, 0.85f, 1}, 0.28f);
            font.drawText(*renderer, 520.0f, 82.0f, "HANDLERS", render::Color{0.6f, 0.8f, 0.68f, 1}, 0.28f);

            // Event log panel.
            box(*renderer, white, 40.0f, 392.0f, 1200.0f, 232.0f, render::Color{0.11f, 0.12f, 0.16f, 1.0f},
                render::Color{0.3f, 0.34f, 0.4f, 1.0f});
            font.drawText(*renderer, 56.0f, 402.0f, "event log (fired: 4x pressed, then flush)",
                          render::Color{0.75f, 0.8f, 0.9f, 1}, 0.3f);
            float ly = 434.0f;
            for (const std::string& line : log) {
                const bool deferred = !line.empty() && line[0] == ' ';
                font.drawText(*renderer, 62.0f, ly, line.c_str(),
                              deferred ? render::Color{0.85f, 0.78f, 0.5f, 1}
                                       : render::Color{0.8f, 0.86f, 0.8f, 1},
                              0.3f);
                ly += 22.0f;
            }

            char st[160];
            std::snprintf(st, sizeof(st),
                          "final HP %d   died one-shot still connected: %s   live connections: pressed %zu / "
                          "hpChanged %zu / died %zu",
                          hp, diedStill ? "yes" : "no", pC, hC, dC);
            font.drawText(*renderer, 40.0f, 648.0f, st, render::Color{0.62f, 0.68f, 0.76f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SIGNALS shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
