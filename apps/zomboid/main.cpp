// ZOMBOID — the flagship native game, rendered on the Maz Engine.
//
// Every rule of this game (the survivor's needs, the zombie chase-and-attack AI, loot) lives in
// maz::script and runs on a scene::SceneTree — see game.hpp, which the unit tests exercise
// headless. This file is the *presentation* layer only: it walks the same SceneTree each frame
// and draws a sprite per node, plus a HUD. That split — logic in script, rendering in the app —
// is exactly how you build a game in Godot, and it means the entire simulation is verified in CI
// without a GPU while this app renders it on a real machine.
//
// Controls: WASD / arrows move the survivor. E eats a ration. ESC quits.
// --headless / --frames N run the render loop with no window (CI); --demo autopilots the survivor.

#include "maz/Engine.hpp"

#include "game.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

// A solid RGBA square with a dark 1px border — the stand-in sprite for a scene node. Colour tells
// the three kinds apart at a glance (survivor / zombie / loot).
std::vector<uint8_t> makeSquare(uint8_t r, uint8_t g, uint8_t b) {
    const uint32_t s = 16;
    std::vector<uint8_t> px(static_cast<size_t>(s) * s * 4, 0);
    for (uint32_t y = 0; y < s; ++y) {
        for (uint32_t x = 0; x < s; ++x) {
            const bool border = x == 0 || y == 0 || x == s - 1 || y == s - 1;
            const size_t i = (static_cast<size_t>(y) * s + x) * 4;
            px[i + 0] = border ? 18 : r;
            px[i + 1] = border ? 18 : g;
            px[i + 2] = border ? 22 : b;
            px[i + 3] = 255;
        }
    }
    return px;
}

// Read a numeric field off a node's attached script instance (0 if absent/unscripted).
double field(const scene::SceneNode* n, const char* name) {
    if (!n || n->script().type != script::Value::Type::Object || !n->script().instance) {
        return 0.0;
    }
    const script::Value* v = n->script().instance->findField(name);
    return v ? v->number : 0.0;
}
bool fieldBool(const scene::SceneNode* n, const char* name) {
    if (!n || n->script().type != script::Value::Type::Object || !n->script().instance) {
        return false;
    }
    const script::Value* v = n->script().instance->findField(name);
    return v && v->boolean;
}

} // namespace

int main(int argc, char** argv) {
    // Install the crash reporter first thing: any fatal signal now dumps a labelled backtrace to
    // stderr and to zomboid.crash.log, so a crash on a player's machine leaves a diagnosable trace.
    platform::CrashHandler::install(platform::CrashConfig{"ZOMBOID", "1.0.0", "zomboid.crash.log"});

    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("ZOMBOID (Maz Engine) headless=%d frames=%d autopilot=%d", cfg.headless,
                 cfg.frames, autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "ZOMBOID — Maz Engine";
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

    // The whole game world: built and simulated in maz::script on a SceneTree.
    scene::SceneTree tree;
    scene::SceneNode* survivor = zomboid::buildScene(tree);

    // One sprite texture per node kind.
    render::TextureHandle texSurvivor = renderer->createTexture(16, 16, makeSquare(240, 205, 70).data());
    render::TextureHandle texZombie = renderer->createTexture(16, 16, makeSquare(90, 170, 80).data());
    render::TextureHandle texLoot = renderer->createTexture(16, 16, makeSquare(210, 120, 200).data());
    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle whiteTex = renderer->createTexture(1, 1, white);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        if (!font.load(*renderer, fontPath.c_str(), 32.0f)) {
            MAZ_LOG_WARN("HUD font failed to load; continuing without text");
        }
    }

    const float moveSpeed = 18.0f; // world units / second (survivor)
    const float worldToPx = 6.0f;  // scene units -> screen pixels for the camera zoom
    int rendered = 0;
    bool ateLast = false;

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());

            // --- player input drives the survivor node's transform directly ---
            if (survivor && fieldBool(survivor, "alive")) {
                float dx = 0.0f, dy = 0.0f;
                if (autopilot) {
                    const float t = static_cast<float>(clock.elapsed());
                    dx = std::cos(t * 0.9f);
                    dy = std::sin(t * 1.3f);
                } else {
                    if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT)) dx -= 1;
                    if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT)) dx += 1;
                    if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP)) dy -= 1;
                    if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN)) dy += 1;
                }
                const float len = std::sqrt(dx * dx + dy * dy);
                if (len > 0.0001f) {
                    survivor->setPosition(survivor->x() + dx / len * moveSpeed * dt,
                                          survivor->y() + dy / len * moveSpeed * dt);
                }

                // E eats a ration (calls the script method on the survivor instance).
                const bool eatNow = input.keyPressed(SDL_SCANCODE_E);
                if (eatNow && !ateLast) {
                    script::Value self = survivor->script();
                    std::vector<script::Value> none;
                    tree.scripts().vm().callOn(self, "eat", none);
                }
                ateLast = eatNow;
            }

            // --- advance the whole simulation one fixed step (zombie AI, needs, combat) ---
            tree.process(dt);
        }

        // Camera centred on the survivor, converting scene units to pixels.
        render::Camera2D cam;
        cam.usePixelSpace = false;
        cam.zoom = worldToPx;
        cam.centerX = survivor ? static_cast<float>(survivor->x()) : 0.0f;
        cam.centerY = survivor ? static_cast<float>(survivor->y()) : 0.0f;
        renderer->setCamera2D(cam);

        // Day/night: read the shared clock the survivor script advances (g_phase over g_day_len)
        // and darken the world toward night, so the tension of the night ramp is visible.
        float nightT = 0.0f; // 0 = day, 1 = deep night
        if (const auto* phase = tree.scripts().vm().getGlobal("g_phase")) {
            const auto* len = tree.scripts().vm().getGlobal("g_day_len");
            const double dayLen = (len && len->number > 0.0) ? len->number : 60.0;
            const double frac = phase->number / dayLen;              // 0..1 through the cycle
            nightT = static_cast<float>(frac < 0.5 ? 0.0 : (frac - 0.5) * 2.0); // night half ramps
        }
        const float lum = 1.0f - 0.75f * nightT;
        renderer->setClearColor(render::Color{0.05f * lum, 0.06f * lum, 0.10f * lum, 1.0f});
        if (renderer->beginFrame()) {
            // Draw every scene node as a sprite by its group.
            auto drawNode = [&](scene::SceneNode* n, render::TextureHandle tex, float size) {
                render::SpriteDesc s;
                s.width = size;
                s.height = size;
                s.x = static_cast<float>(n->x()) - size * 0.5f;
                s.y = static_cast<float>(n->y()) - size * 0.5f;
                renderer->drawSprite(tex, s);
            };
            for (scene::SceneNode* l : tree.nodesInGroup("loot")) {
                if (!fieldBool(l, "taken")) drawNode(l, texLoot, 2.0f);
            }
            for (scene::SceneNode* z : tree.nodesInGroup("zombies")) drawNode(z, texZombie, 2.5f);
            if (survivor && fieldBool(survivor, "alive")) drawNode(survivor, texSurvivor, 3.0f);

            // --- HUD (pixel-space overlay) ---
            render::Camera2D uiCam;
            uiCam.usePixelSpace = true;
            renderer->setCamera2D(uiCam);

            uint32_t bw = 0, bh = 0;
            window.drawableSize(bw, bh);
            const float sh = static_cast<float>(bh);
            auto rect = [&](float x, float y, float w, float h, render::Color c) {
                render::SpriteDesc s;
                s.x = x;
                s.y = y;
                s.width = w;
                s.height = h;
                s.color = c;
                renderer->drawSprite(whiteTex, s);
            };

            const render::Color kWhite{1, 1, 1, 1};
            const render::Color kDim{0.75f, 0.8f, 0.85f, 1};
            const bool alive = survivor && fieldBool(survivor, "alive");
            font.drawText(*renderer, 16.0f, 12.0f, "ZOMBOID  -  MAZ ENGINE", kWhite, 0.8f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          autopilot ? "AUTOPILOT" : "WASD MOVE   E EAT   ESC QUIT", kDim, 0.5f);

            const float health = static_cast<float>(field(survivor, "health"));
            const float hunger = static_cast<float>(field(survivor, "hunger"));
            const int food = static_cast<int>(field(survivor, "food"));

            // Health bar (green->amber).
            const float bx = 16.0f, by = sh - 80.0f, bw2 = 240.0f, bhh = 16.0f;
            font.drawText(*renderer, bx, by - 24.0f, "HEALTH", kWhite, 0.5f);
            rect(bx - 2, by - 2, bw2 + 4, bhh + 4, render::Color{0, 0, 0, 0.55f});
            rect(bx, by, bw2, bhh, render::Color{0.15f, 0.15f, 0.18f, 1});
            const float hf = health / 100.0f;
            rect(bx, by, bw2 * (hf < 0 ? 0 : hf), bhh,
                 hf > 0.5f ? render::Color{0.30f, 0.90f, 0.40f, 1}
                           : render::Color{0.95f, 0.55f, 0.20f, 1});

            // Hunger bar (fills toward danger).
            const float hy = sh - 32.0f;
            font.drawText(*renderer, bx, hy - 22.0f, "HUNGER", kWhite, 0.5f);
            rect(bx - 2, hy - 2, bw2 + 4, bhh + 4, render::Color{0, 0, 0, 0.55f});
            rect(bx, hy, bw2, bhh, render::Color{0.15f, 0.15f, 0.18f, 1});
            rect(bx, hy, bw2 * (hunger / 100.0f), bhh, render::Color{0.85f, 0.35f, 0.30f, 1});

            char buf[64];
            const int looted = static_cast<int>(field(survivor, "loot_collected"));
            std::snprintf(buf, sizeof(buf), "RATIONS %d   LOOTED %d", food, looted);
            font.drawText(*renderer, bx + bw2 + 16.0f, by - 4.0f, buf, kWhite, 0.55f);

            // Day/night readout, top-right.
            const bool night = nightT > 0.001f;
            font.drawText(*renderer, static_cast<float>(bw) - 180.0f, 16.0f,
                          night ? "NIGHT - HORDE ENRAGED" : "DAY",
                          night ? render::Color{0.95f, 0.5f, 0.45f, 1} : render::Color{0.9f, 0.9f, 0.6f, 1},
                          0.5f);
            if (!alive) {
                font.drawText(*renderer, static_cast<float>(bw) * 0.5f - 90.0f, sh * 0.5f - 20.0f,
                              "YOU DIED", render::Color{0.95f, 0.25f, 0.25f, 1}, 1.2f);
            }

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ZOMBOID shutting down after %d frames (%.2fs, renderer %s)", rendered,
                 clock.elapsed(), renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
