// Maz Engine — "RESTEXT" (text resource save/load, toward Godot's .tscn / .tres)
// A prefab isn't only an in-memory template — Godot stores scenes as human-readable TEXT you can read,
// diff, and version-control. This authors a small "Enemy" prefab, serializes it with io::savePrefabText
// into a .tscn-style block ([node …] sections + typed property lines), renders that exact text, then
// PARSES it back with io::loadPrefabText and reports that the reconstructed tree matches. Everything is
// static → deterministic, golden-stable. Run --headless / --frames N for CI.

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

// Split text on '\n' (the Font renderer draws single lines only).
std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i <= s.size()) {
        const std::size_t nl = s.find('\n', i);
        out.push_back(s.substr(i, nl == std::string::npos ? std::string::npos : nl - i));
        if (nl == std::string::npos) {
            break;
        }
        i = nl + 1;
    }
    return out;
}

scene::Prefab makeEnemyPrefab() {
    scene::Prefab pf;
    pf.root.name = "Enemy";
    scene::setProp(pf.root.props, "hp", scene::PropValue::makeInt(120));
    scene::setProp(pf.root.props, "speed", scene::PropValue::makeFloat(3.5f));
    scene::setProp(pf.root.props, "pos", scene::PropValue::makeVec2(math::vec2(64.0f, 48.0f)));
    scene::setProp(pf.root.props, "name", scene::PropValue::makeText("grunt"));

    scene::PrefabNode sprite;
    sprite.name = "Sprite";
    scene::setProp(sprite.props, "frame", scene::PropValue::makeInt(0));
    scene::setProp(sprite.props, "tint", scene::PropValue::makeColor(math::vec4(0.8f, 0.3f, 0.3f, 1.0f)));

    scene::PrefabNode hitbox;
    hitbox.name = "Hitbox";
    scene::setProp(hitbox.props, "radius", scene::PropValue::makeFloat(14.0f));
    scene::setProp(hitbox.props, "solid", scene::PropValue::makeBool(true));

    pf.root.children.push_back(sprite);
    pf.root.children.push_back(hitbox);
    return pf;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("RESTEXT (text resource save/load) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Text Resources (.tscn/.tres)";
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

    // Author → serialize → parse back.
    const scene::Prefab prefab = makeEnemyPrefab();
    const std::string text = io::savePrefabText(prefab);
    const std::vector<std::string> lines = splitLines(text);

    scene::Prefab loaded;
    const bool ok = io::loadPrefabText(text, loaded);
    const int nodes = scene::nodeCount(loaded.root);
    const int hp = scene::getInt(loaded.root.props, "hp");
    const bool idempotent = (io::savePrefabText(loaded) == text);

    char status[128];
    std::snprintf(status, sizeof(status), "parsed back: %s   %d nodes   hp=%d   re-serialize identical: %s",
                  ok ? "ok" : "FAILED", nodes, hp, idempotent ? "yes" : "no");

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TEXT RESOURCES (.tscn / .tres)",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "author a prefab -> savePrefabText -> parse back with loadPrefabText",
                          render::Color{0.78f, 0.83f, 0.95f, 1}, 0.34f);

            // The serialized text in a panel (monospace-ish column).
            fillRect(*renderer, white, 40.0f, 92.0f, 780.0f, 560.0f, render::Color{0.11f, 0.12f, 0.16f, 1.0f});
            float ty = 108.0f;
            for (const std::string& ln : lines) {
                if (ln.empty()) {
                    ty += 12.0f;
                    continue;
                }
                const bool header = !ln.empty() && ln[0] == '[';
                const render::Color col = header ? render::Color{0.62f, 0.82f, 1.0f, 1.0f}
                                                 : render::Color{0.82f, 0.86f, 0.7f, 1.0f};
                font.drawText(*renderer, 58.0f, ty, ln.c_str(), col, 0.32f);
                ty += 26.0f;
            }

            // Round-trip readout on the right.
            fillRect(*renderer, white, 856.0f, 92.0f, 384.0f, 200.0f, render::Color{0.11f, 0.14f, 0.12f, 1.0f});
            font.drawText(*renderer, 872.0f, 108.0f, "round-trip", render::Color{0.7f, 0.9f, 0.75f, 1}, 0.36f);
            font.drawText(*renderer, 872.0f, 150.0f, ok ? "save -> text -> load: OK" : "load FAILED",
                          render::Color{0.85f, 0.9f, 0.85f, 1}, 0.3f);
            char l2[64];
            std::snprintf(l2, sizeof(l2), "%d nodes reconstructed", nodes);
            font.drawText(*renderer, 872.0f, 182.0f, l2, render::Color{0.8f, 0.84f, 0.8f, 1}, 0.3f);
            char l3[64];
            std::snprintf(l3, sizeof(l3), "Enemy.hp = %d", hp);
            font.drawText(*renderer, 872.0f, 214.0f, l3, render::Color{0.8f, 0.84f, 0.8f, 1}, 0.3f);
            font.drawText(*renderer, 872.0f, 246.0f, idempotent ? "re-serialize: identical" : "re-serialize: DIFF",
                          render::Color{0.8f, 0.84f, 0.8f, 1}, 0.3f);

            font.drawText(*renderer, 40.0f, 668.0f, status, render::Color{0.62f, 0.68f, 0.76f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("RESTEXT shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
