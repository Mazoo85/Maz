// ZOMBOID — the flagship native game, a top-down twin-stick zombie SHOOTER on the Maz Engine.
//
// Every rule of this game (the survivor, aiming/firing, bullets, zombie health & death, endless waves,
// loot, day/night) lives in maz::script and runs on a scene::SceneTree — see game.hpp, which the unit
// tests exercise headless. This file is the *presentation* layer only: each frame it feeds the mouse
// aim and fire button into the survivor's script fields, ticks the simulation, then walks the same
// SceneTree and draws a sprite per node plus a HUD. Logic in script, rendering in the app — exactly how
// you build a game in Godot, so the whole simulation is CI-verified without a GPU while this app renders
// it on a real machine.
//
// Controls: WASD / arrows move. Mouse aims. Hold LEFT MOUSE to fire. E eats a ration. ESC quits.
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

// A solid RGBA square with a dark 1px border — the stand-in sprite for a scene node. Colour tells the
// kinds apart at a glance (survivor / zombie / bullet / loot).
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
// Write a numeric / boolean field on a node's script instance (the app driving the survivor each frame).
void setField(scene::SceneNode* n, const char* name, double value) {
    if (!n || n->script().type != script::Value::Type::Object || !n->script().instance) return;
    if (script::Value* v = n->script().instance->findField(name)) v->number = value;
}
void setFieldBool(scene::SceneNode* n, const char* name, bool value) {
    if (!n || n->script().type != script::Value::Type::Object || !n->script().instance) return;
    if (script::Value* v = n->script().instance->findField(name)) v->boolean = value;
}

double globalNum(scene::SceneTree& tree, const char* name) {
    const script::Value* v = tree.scripts().vm().getGlobal(name);
    return v ? v->number : 0.0;
}

} // namespace

int main(int argc, char** argv) {
    platform::CrashHandler::install(platform::CrashConfig{"ZOMBOID", "1.1.0", "zomboid.crash.log"});

    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("ZOMBOID shooter (Maz Engine) headless=%d frames=%d autopilot=%d", cfg.headless,
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
    render::TextureHandle texZombieHurt = renderer->createTexture(16, 16, makeSquare(150, 110, 60).data());
    render::TextureHandle texBullet = renderer->createTexture(16, 16, makeSquare(255, 240, 120).data());
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
    const float worldToPx = 7.0f;  // scene units -> screen pixels for the camera zoom
    int rendered = 0;
    bool ateLast = false;
    double simTime = 0.0;
    // A bounded run (headless or --frames N) steps a deterministic fixed dt per frame so the sim
    // actually advances and is reproducible; interactive play uses the wall-clock fixed-step accumulator.
    const bool deterministic = cfg.headless || cfg.frames >= 0;

    // One simulation step: apply movement/eat intent, then advance the whole SceneTree by dt.
    auto stepSim = [&](float dt) {
        if (survivor && fieldBool(survivor, "alive")) {
            float dx = 0.0f, dy = 0.0f;
            if (autopilot) {
                dx = std::cos(static_cast<float>(simTime) * 0.7f);
                dy = std::sin(static_cast<float>(simTime) * 1.1f);
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
            const bool eatNow = input.keyPressed(SDL_SCANCODE_E);
            if (eatNow && !ateLast) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "eat", none);
            }
            ateLast = eatNow;
        }
        tree.process(dt);
        simTime += dt;
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        // --- Per-frame intent: aim toward the mouse (or the nearest zombie on autopilot), fire. ---
        const bool alive = survivor && fieldBool(survivor, "alive");
        double aimX = field(survivor, "aim_x"), aimY = field(survivor, "aim_y");
        bool firing = false;
        if (alive) {
            uint32_t bw0 = 0, bh0 = 0;
            window.drawableSize(bw0, bh0);
            if (autopilot) {
                // Aim at the nearest live zombie and always fire.
                double best = 1e18, bx = 1.0, by = 0.0;
                for (scene::SceneNode* z : tree.nodesInGroup("zombies")) {
                    if (!fieldBool(z, "alive")) continue;
                    const double dx = z->x() - survivor->x(), dy = z->y() - survivor->y();
                    const double d2 = dx * dx + dy * dy;
                    if (d2 < best) { best = d2; bx = dx; by = dy; }
                }
                aimX = bx;
                aimY = by;
                firing = true;
            } else {
                // Mouse position -> world, relative to the survivor (camera centre).
                const double wx = survivor->x() + (input.mouseX() - bw0 * 0.5) / worldToPx;
                const double wy = survivor->y() + (input.mouseY() - bh0 * 0.5) / worldToPx;
                aimX = wx - survivor->x();
                aimY = wy - survivor->y();
                firing = input.mouseDown(0) || input.mouseDown(1); // left button (index varies by platform)
            }
            const double m = std::sqrt(aimX * aimX + aimY * aimY);
            if (m > 1e-4) { aimX /= m; aimY /= m; }
            setField(survivor, "aim_x", aimX);
            setField(survivor, "aim_y", aimY);
            setFieldBool(survivor, "firing", firing);
        }

        // Advance the simulation (weapon cadence, bullets, zombie AI, waves).
        if (deterministic) {
            stepSim(1.0f / 60.0f);
        } else {
            clock.beginFrame();
            while (clock.consumeFixedStep()) {
                stepSim(static_cast<float>(clock.fixedDelta()));
            }
        }

        // Camera centred on the survivor, converting scene units to pixels.
        render::Camera2D cam;
        cam.usePixelSpace = false;
        cam.zoom = worldToPx;
        cam.centerX = survivor ? static_cast<float>(survivor->x()) : 0.0f;
        cam.centerY = survivor ? static_cast<float>(survivor->y()) : 0.0f;
        renderer->setCamera2D(cam);

        // Day/night: darken toward night so the night ramp is felt.
        float nightT = 0.0f;
        {
            const double phase = globalNum(tree, "g_phase");
            const double dayLen = globalNum(tree, "g_day_len");
            const double dl = dayLen > 0.0 ? dayLen : 60.0;
            const double frac = phase / dl;
            nightT = static_cast<float>(frac < 0.5 ? 0.0 : (frac - 0.5) * 2.0);
        }
        const float lum = 1.0f - 0.7f * nightT;
        renderer->setClearColor(render::Color{0.05f * lum, 0.06f * lum, 0.10f * lum, 1.0f});

        if (renderer->beginFrame()) {
            auto drawAt = [&](double wx, double wy, render::TextureHandle tex, float size,
                              render::Color col) {
                render::SpriteDesc s;
                s.width = size;
                s.height = size;
                s.x = static_cast<float>(wx) - size * 0.5f;
                s.y = static_cast<float>(wy) - size * 0.5f;
                s.color = col;
                renderer->drawSprite(tex, s);
            };
            const render::Color kNoTint{1, 1, 1, 1};

            // Loot.
            for (scene::SceneNode* l : tree.nodesInGroup("loot")) {
                if (!fieldBool(l, "taken")) drawAt(l->x(), l->y(), texLoot, 2.0f, kNoTint);
            }
            // Zombies (only the live ones); tint toward "hurt" as health drops.
            for (scene::SceneNode* z : tree.nodesInGroup("zombies")) {
                if (!fieldBool(z, "alive")) continue;
                const double hp = field(z, "health"), mhp = field(z, "max_health");
                const float f = mhp > 0.0 ? static_cast<float>(hp / mhp) : 1.0f;
                drawAt(z->x(), z->y(), f > 0.5f ? texZombie : texZombieHurt, 2.5f, kNoTint);
            }
            // Bullets in flight.
            for (scene::SceneNode* b : tree.nodesInGroup("bullets")) {
                if (fieldBool(b, "active")) drawAt(b->x(), b->y(), texBullet, 0.7f, kNoTint);
            }
            // Aim tracer: a few dots from the survivor along the aim vector.
            if (alive) {
                for (int i = 1; i <= 5; ++i) {
                    const double d = 2.0 + i * 1.6;
                    drawAt(survivor->x() + aimX * d, survivor->y() + aimY * d, texBullet, 0.35f,
                           render::Color{1.0f, 0.9f, 0.4f, 0.5f});
                }
            }
            // Survivor on top.
            if (alive) drawAt(survivor->x(), survivor->y(), texSurvivor, 3.0f, kNoTint);

            // --- HUD (pixel-space overlay) ---
            render::Camera2D uiCam;
            uiCam.usePixelSpace = true;
            renderer->setCamera2D(uiCam);

            uint32_t bw = 0, bh = 0;
            window.drawableSize(bw, bh);
            const float sw = static_cast<float>(bw);
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
            font.drawText(*renderer, 16.0f, 12.0f, "ZOMBOID", kWhite, 0.8f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          autopilot ? "AUTOPILOT" : "WASD MOVE   MOUSE AIM   LMB FIRE   E EAT",
                          kDim, 0.45f);

            // Wave / score / kills, top-centre-ish.
            char buf[96];
            const int wave = static_cast<int>(globalNum(tree, "g_wave"));
            const int score = static_cast<int>(globalNum(tree, "g_score"));
            const int kills = static_cast<int>(globalNum(tree, "g_kills"));
            std::snprintf(buf, sizeof(buf), "WAVE %d      SCORE %d      KILLS %d", wave, score, kills);
            font.drawText(*renderer, sw * 0.5f - 220.0f, 14.0f, buf, kWhite, 0.55f);

            const float health = static_cast<float>(field(survivor, "health"));
            const float hunger = static_cast<float>(field(survivor, "hunger"));
            const int food = static_cast<int>(field(survivor, "food"));

            const float bx = 16.0f, by = sh - 80.0f, bw2 = 240.0f, bhh = 16.0f;
            font.drawText(*renderer, bx, by - 24.0f, "HEALTH", kWhite, 0.5f);
            rect(bx - 2, by - 2, bw2 + 4, bhh + 4, render::Color{0, 0, 0, 0.55f});
            rect(bx, by, bw2, bhh, render::Color{0.15f, 0.15f, 0.18f, 1});
            const float hf = health / 100.0f;
            rect(bx, by, bw2 * (hf < 0 ? 0 : hf), bhh,
                 hf > 0.5f ? render::Color{0.30f, 0.90f, 0.40f, 1}
                           : render::Color{0.95f, 0.55f, 0.20f, 1});

            const float hy = sh - 32.0f;
            font.drawText(*renderer, bx, hy - 22.0f, "HUNGER", kWhite, 0.5f);
            rect(bx - 2, hy - 2, bw2 + 4, bhh + 4, render::Color{0, 0, 0, 0.55f});
            rect(bx, hy, bw2, bhh, render::Color{0.15f, 0.15f, 0.18f, 1});
            rect(bx, hy, bw2 * (hunger / 100.0f), bhh, render::Color{0.85f, 0.35f, 0.30f, 1});

            std::snprintf(buf, sizeof(buf), "RATIONS %d", food);
            font.drawText(*renderer, bx + bw2 + 16.0f, by - 4.0f, buf, kWhite, 0.55f);

            const bool night = nightT > 0.001f;
            font.drawText(*renderer, sw - 190.0f, 16.0f,
                          night ? "NIGHT - HORDE ENRAGED" : "DAY",
                          night ? render::Color{0.95f, 0.5f, 0.45f, 1}
                                : render::Color{0.9f, 0.9f, 0.6f, 1},
                          0.5f);

            // Reticle at the mouse (crosshair) when playing.
            if (!autopilot && alive) {
                const float mx = input.mouseX(), my = input.mouseY();
                const render::Color ret{1.0f, 0.95f, 0.5f, 0.9f};
                rect(mx - 10, my - 1, 8, 2, ret);
                rect(mx + 2, my - 1, 8, 2, ret);
                rect(mx - 1, my - 10, 2, 8, ret);
                rect(mx - 1, my + 2, 2, 8, ret);
            }

            if (!alive) {
                font.drawText(*renderer, sw * 0.5f - 90.0f, sh * 0.5f - 20.0f, "YOU DIED",
                              render::Color{0.95f, 0.25f, 0.25f, 1}, 1.2f);
                std::snprintf(buf, sizeof(buf), "REACHED WAVE %d   -   SCORE %d", wave, score);
                font.drawText(*renderer, sw * 0.5f - 150.0f, sh * 0.5f + 24.0f, buf, kWhite, 0.5f);
            }

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ZOMBOID shutting down after %d frames (%.2fs, renderer %s) — wave %d, %d kills, score %d",
                 rendered, simTime, renderer->isActive() ? "active" : "inactive",
                 static_cast<int>(globalNum(tree, "g_wave")), static_cast<int>(globalNum(tree, "g_kills")),
                 static_cast<int>(globalNum(tree, "g_score")));
    renderer->shutdown();
    window.shutdown();
    return 0;
}
