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
    render::TextureHandle texRunner = renderer->createTexture(16, 16, makeSquare(80, 200, 180).data());
    render::TextureHandle texBrute = renderer->createTexture(16, 16, makeSquare(165, 55, 55).data());
    render::TextureHandle texBoss = renderer->createTexture(16, 16, makeSquare(195, 70, 195).data());
    render::TextureHandle texExploder = renderer->createTexture(16, 16, makeSquare(235, 130, 30).data());
    render::TextureHandle texSpitter = renderer->createTexture(16, 16, makeSquare(150, 200, 40).data());
    render::TextureHandle texSplitter = renderer->createTexture(16, 16, makeSquare(210, 90, 180).data());
    render::TextureHandle texSummoner = renderer->createTexture(16, 16, makeSquare(120, 60, 190).data());
    render::TextureHandle texArmored = renderer->createTexture(16, 16, makeSquare(110, 120, 140).data());
    render::TextureHandle texLeaper = renderer->createTexture(16, 16, makeSquare(150, 200, 90).data());
    render::TextureHandle texBloater = renderer->createTexture(16, 16, makeSquare(90, 140, 60).data());
    render::TextureHandle texScreamer = renderer->createTexture(16, 16, makeSquare(230, 150, 40).data());
    render::TextureHandle texSpit = renderer->createTexture(16, 16, makeSquare(180, 230, 60).data());
    render::TextureHandle texPowRapid = renderer->createTexture(16, 16, makeSquare(255, 200, 40).data());
    render::TextureHandle texPowDamage = renderer->createTexture(16, 16, makeSquare(255, 70, 70).data());
    render::TextureHandle texPowShield = renderer->createTexture(16, 16, makeSquare(70, 180, 255).data());
    render::TextureHandle texPowPierce = renderer->createTexture(16, 16, makeSquare(180, 90, 255).data());
    render::TextureHandle texPowCryo = renderer->createTexture(16, 16, makeSquare(120, 230, 255).data());
    render::TextureHandle texPowVamp = renderer->createTexture(16, 16, makeSquare(200, 30, 70).data());
    render::TextureHandle texPowOverflow = renderer->createTexture(16, 16, makeSquare(255, 210, 90).data());
    render::TextureHandle texCrate = renderer->createTexture(16, 16, makeSquare(200, 160, 90).data());
    render::TextureHandle texMine = renderer->createTexture(16, 16, makeSquare(150, 40, 40).data());
    render::TextureHandle texSentry = renderer->createTexture(16, 16, makeSquare(90, 150, 220).data());
    render::TextureHandle texFire = renderer->createTexture(16, 16, makeSquare(255, 120, 30).data());
    render::TextureHandle texAcid = renderer->createTexture(16, 16, makeSquare(120, 210, 40).data());
    render::TextureHandle texBarrel = renderer->createTexture(16, 16, makeSquare(200, 60, 40).data());
    render::TextureHandle texBullet = renderer->createTexture(16, 16, makeSquare(255, 240, 120).data());
    render::TextureHandle texBlood = renderer->createTexture(16, 16, makeSquare(170, 30, 30).data());
    render::TextureHandle texGrenade = renderer->createTexture(16, 16, makeSquare(70, 90, 70).data());
    render::TextureHandle texMedkit = renderer->createTexture(16, 16, makeSquare(70, 210, 90).data());
    render::TextureHandle texAmmo = renderer->createTexture(16, 16, makeSquare(220, 190, 80).data());
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

    // Persistent best run (best wave + best score), saved to the platform pref dir.
    core::KeyValueStore store;
    store.load(platform::prefPath("MazEngine", "Zomboid", "zomboid.ini"));
    int bestWave = store.getInt("best_wave", 0);
    int bestScore = store.getInt("best_score", 0);
    bool savedThisDeath = false;
    bool newBestThisRun = false;

    // Procedural sound effects (no device in headless -> play() is a no-op). Fired by watching
    // simulation state change frame-to-frame, so no script hooks are needed.
    audio::Audio audio;
    audio.init();
    const audio::SoundDesc sfxPistol{audio::Wave::Square, 240.0f, 90.0f, 0.06f, 0.16f};
    const audio::SoundDesc sfxShotgun{audio::Wave::Noise, 320.0f, 0.0f, 0.13f, 0.24f};
    const audio::SoundDesc sfxSmg{audio::Wave::Square, 320.0f, 130.0f, 0.04f, 0.11f};
    const audio::SoundDesc sfxKill{audio::Wave::Square, 170.0f, 40.0f, 0.12f, 0.20f};
    const audio::SoundDesc sfxReload{audio::Wave::Square, 520.0f, 300.0f, 0.09f, 0.15f};
    const audio::SoundDesc sfxBoom{audio::Wave::Noise, 140.0f, 0.0f, 0.34f, 0.34f};
    const audio::SoundDesc sfxWave{audio::Wave::Square, 300.0f, 620.0f, 0.28f, 0.24f};
    const audio::SoundDesc sfxHurt{audio::Wave::Square, 150.0f, 60.0f, 0.10f, 0.22f};
    const audio::SoundDesc sfxDeath{audio::Wave::Square, 300.0f, 55.0f, 0.60f, 0.30f};
    double aPrevShots = 0, aPrevKills = 0, aPrevWave = 0, aPrevHealth = 100, aPrevShake = 0;
    bool aPrevReloading = false, aPrevAlive = true, resyncAudio = true;

    const float moveSpeed = 18.0f; // world units / second (survivor)
    const float worldToPx = 7.0f;  // scene units -> screen pixels for the camera zoom
    int rendered = 0;
    bool ateLast = false;
    double simTime = 0.0;
    double nextGrenade = 4.0; // autopilot's next grenade toss time
    double nextMine = 7.0;    // autopilot's next proximity-mine deploy time
    double nextSentry = 5.0;  // autopilot's next sentry deploy time
    double nextMolotov = 8.0; // autopilot's next molotov throw time
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

        // On death: save the best run once, then allow a restart with Enter.
        if (!alive) {
            if (!savedThisDeath) {
                const int w = static_cast<int>(globalNum(tree, "g_wave"));
                const int sc = static_cast<int>(globalNum(tree, "g_score"));
                if (zomboid::beatsBest(w, sc, bestWave, bestScore)) {
                    if (w > bestWave) bestWave = w;
                    if (sc > bestScore) bestScore = sc;
                    store.set("best_wave", bestWave);
                    store.set("best_score", bestScore);
                    store.save();
                    newBestThisRun = true;
                }
                savedThisDeath = true;
            }
            if (!autopilot && input.keyPressed(SDL_SCANCODE_RETURN)) {
                tree = scene::SceneTree{};
                survivor = zomboid::buildScene(tree);
                simTime = 0.0;
                ateLast = false;
                nextGrenade = 4.0;
                savedThisDeath = false;
                newBestThisRun = false;
                resyncAudio = true; // don't fire SFX from the reset's state jump
            }
        }
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

            // Weapon switching: 1/2/3 while playing; autopilot cycles them every 8 s to show them off.
            int want = static_cast<int>(field(survivor, "weapon"));
            if (autopilot) {
                want = static_cast<int>(simTime / 8.0) % 5;
            } else {
                if (input.keyPressed(SDL_SCANCODE_1)) want = 0;
                if (input.keyPressed(SDL_SCANCODE_2)) want = 1;
                if (input.keyPressed(SDL_SCANCODE_3)) want = 2;
                if (input.keyPressed(SDL_SCANCODE_4)) want = 3;
                if (input.keyPressed(SDL_SCANCODE_5)) want = 4;
            }
            if (want != static_cast<int>(field(survivor, "weapon"))) {
                script::Value self = survivor->script();
                std::vector<script::Value> a = {script::Value::fromNum(static_cast<double>(want))};
                tree.scripts().vm().callOn(self, "set_weapon", a);
            }

            // Manual reload (R).
            if (!autopilot && input.keyPressed(SDL_SCANCODE_R)) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "reload", none);
            }

            // Salvage shop: spend banked cash on ammo (6), a grenade (7), or a heal (8).
            if (!autopilot) {
                int buyKind = -1;
                if (input.keyPressed(SDL_SCANCODE_6)) buyKind = 0;
                else if (input.keyPressed(SDL_SCANCODE_7)) buyKind = 1;
                else if (input.keyPressed(SDL_SCANCODE_8)) buyKind = 2;
                else if (input.keyPressed(SDL_SCANCODE_9)) buyKind = 3;
                if (buyKind >= 0) {
                    script::Value self = survivor->script();
                    std::vector<script::Value> a = {script::Value::fromNum(static_cast<double>(buyKind))};
                    tree.scripts().vm().callOn(self, "buy", a);
                }
            }

            // Throw a grenade (G while playing; autopilot lobs one every ~5 s).
            bool throwNow = !autopilot && input.keyPressed(SDL_SCANCODE_G);
            if (autopilot && simTime >= nextGrenade && field(survivor, "grenades") > 0.0) {
                throwNow = true;
                nextGrenade = simTime + 5.0;
            }
            if (throwNow) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "throw_grenade", none);
            }

            // Overcharge ultimate (Q while playing; autopilot fires it the moment it's ready).
            bool ultNow = !autopilot && input.keyPressed(SDL_SCANCODE_Q);
            if (autopilot && fieldBool(survivor, "ult_ready")) ultNow = true;
            if (ultNow && fieldBool(survivor, "ult_ready")) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "detonate", none);
            }

            // Dodge-roll (SPACE while playing; autopilot rolls away when a zombie crowds it).
            bool dashNow = false;
            double dashX = 0.0, dashY = 0.0;
            if (autopilot) {
                // If the nearest zombie is close and the dodge is ready, roll away from it.
                double best = 1e18, bx = 0.0, by = 0.0;
                for (scene::SceneNode* z : tree.nodesInGroup("zombies")) {
                    if (!fieldBool(z, "alive")) continue;
                    const double dx = z->x() - survivor->x(), dy = z->y() - survivor->y();
                    const double d2 = dx * dx + dy * dy;
                    if (d2 < best) { best = d2; bx = dx; by = dy; }
                }
                if (best < 25.0 && field(survivor, "dash_cd") <= 0.0) {
                    dashNow = true; dashX = -bx; dashY = -by;
                }
            } else if (input.keyPressed(SDL_SCANCODE_SPACE)) {
                double dx = 0.0, dy = 0.0;
                if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT)) dx -= 1;
                if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT)) dx += 1;
                if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP)) dy -= 1;
                if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN)) dy += 1;
                if (dx == 0.0 && dy == 0.0) { dx = aimX; dy = aimY; } // no move keys: roll where you aim
                dashNow = true; dashX = dx; dashY = dy;
            }
            if (dashNow) {
                script::Value self = survivor->script();
                std::vector<script::Value> a = {script::Value::fromNum(dashX), script::Value::fromNum(dashY)};
                tree.scripts().vm().callOn(self, "dash", a);
            }

            // Melee shove (F while playing; autopilot swings when a zombie is point-blank).
            bool meleeNow = false;
            if (autopilot) {
                if (field(survivor, "melee_cd") <= 0.0) {
                    const double r = field(survivor, "melee_range");
                    for (scene::SceneNode* z : tree.nodesInGroup("zombies")) {
                        if (!fieldBool(z, "alive")) continue;
                        const double dx = z->x() - survivor->x(), dy = z->y() - survivor->y();
                        if (dx * dx + dy * dy <= r * r) { meleeNow = true; break; }
                    }
                }
            } else {
                meleeNow = input.keyPressed(SDL_SCANCODE_F);
            }
            if (meleeNow) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "melee", none);
            }

            // Deploy a proximity mine (T while playing; autopilot lays one every ~9 s if it has any).
            bool mineNow = !autopilot && input.keyPressed(SDL_SCANCODE_T);
            if (autopilot && simTime >= nextMine && field(survivor, "mines") > 0.0) {
                mineNow = true;
                nextMine = simTime + 9.0;
            }
            if (mineNow) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "place_mine", none);
            }

            // Deploy an auto-turret sentry (Y while playing; autopilot lays one every ~11 s if it has any).
            bool sentryNow = !autopilot && input.keyPressed(SDL_SCANCODE_Y);
            if (autopilot && simTime >= nextSentry && field(survivor, "sentries") > 0.0) {
                sentryNow = true;
                nextSentry = simTime + 11.0;
            }
            if (sentryNow) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "place_sentry", none);
            }

            // Throw a molotov (X while playing; autopilot lobs one every ~10 s if it has any).
            bool molotovNow = !autopilot && input.keyPressed(SDL_SCANCODE_X);
            if (autopilot && simTime >= nextMolotov && field(survivor, "molotovs") > 0.0) {
                molotovNow = true;
                nextMolotov = simTime + 10.0;
            }
            if (molotovNow) {
                script::Value self = survivor->script();
                std::vector<script::Value> none;
                tree.scripts().vm().callOn(self, "throw_molotov", none);
            }
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

        // --- Sound effects: detect state changes since last frame and fire one-shots. ---
        {
            const double shots = field(survivor, "shots");
            const double kills = globalNum(tree, "g_kills");
            const double wv = globalNum(tree, "g_wave");
            const double health = field(survivor, "health");
            const double shake = globalNum(tree, "g_shake");
            const bool reloading = fieldBool(survivor, "is_reloading");
            const bool aliveA = fieldBool(survivor, "alive");
            const int weap = static_cast<int>(field(survivor, "weapon"));
            if (resyncAudio) {
                resyncAudio = false; // first frame (or just after a restart): sync without firing
            } else {
                if (shots > aPrevShots) audio.play(weap == 1 ? sfxShotgun : (weap == 2 ? sfxSmg : sfxPistol));
                if (kills > aPrevKills) audio.play(sfxKill);
                if (reloading && !aPrevReloading) audio.play(sfxReload);
                if (shake - aPrevShake > 1.0) audio.play(sfxBoom);          // grenade / boss kill
                if (wv > aPrevWave) audio.play(sfxWave);
                if (health < aPrevHealth - 2.0) audio.play(sfxHurt);        // a bite (not the hunger drip)
                if (!aliveA && aPrevAlive) audio.play(sfxDeath);
            }
            aPrevShots = shots;
            aPrevKills = kills;
            aPrevWave = wv;
            aPrevHealth = health;
            aPrevShake = shake;
            aPrevReloading = reloading;
            aPrevAlive = aliveA;
        }

        // Camera centred on the survivor, converting scene units to pixels.
        render::Camera2D cam;
        cam.usePixelSpace = false;
        cam.zoom = worldToPx;
        // Screen shake: jitter the camera centre by the decaying g_shake magnitude.
        const float shake = static_cast<float>(globalNum(tree, "g_shake"));
        const float shX = shake * 0.5f * std::cos(static_cast<float>(simTime) * 47.0f);
        const float shY = shake * 0.5f * std::sin(static_cast<float>(simTime) * 41.0f);
        cam.centerX = (survivor ? static_cast<float>(survivor->x()) : 0.0f) + shX;
        cam.centerY = (survivor ? static_cast<float>(survivor->y()) : 0.0f) + shY;
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
            // Medkits (dropped health), blinking as they near expiry.
            for (scene::SceneNode* m : tree.nodesInGroup("medkits")) {
                if (!fieldBool(m, "active")) continue;
                const float life = static_cast<float>(field(m, "life"));
                const float blink = (life > 3.0f || std::sin(static_cast<float>(simTime) * 10.0f) > 0.0f)
                                        ? 1.0f
                                        : 0.35f;
                drawAt(m->x(), m->y(), texMedkit, 1.8f, render::Color{0.5f * blink, blink, 0.6f * blink, 1.0f});
            }
            // Dropped ammo boxes (brass), blinking as they near expiry.
            for (scene::SceneNode* am : tree.nodesInGroup("ammo")) {
                if (!fieldBool(am, "active")) continue;
                const float life = static_cast<float>(field(am, "life"));
                const float blink = (life > 3.0f || std::sin(static_cast<float>(simTime) * 10.0f) > 0.0f)
                                        ? 1.0f
                                        : 0.35f;
                drawAt(am->x(), am->y(), texAmmo, 1.6f,
                       render::Color{blink, 0.85f * blink, 0.35f * blink, 1.0f});
            }
            // Supply crates (periodic care packages), blinking as they near expiry.
            for (scene::SceneNode* c : tree.nodesInGroup("crates")) {
                if (!fieldBool(c, "active")) continue;
                const float life = static_cast<float>(field(c, "life"));
                const float blink = (life > 4.0f || std::sin(static_cast<float>(simTime) * 8.0f) > 0.0f)
                                        ? 1.0f
                                        : 0.4f;
                drawAt(c->x(), c->y(), texCrate, 2.4f, render::Color{blink, blink * 0.9f, blink * 0.6f, 1.0f});
            }
            // Proximity mines: dim while arming (safety fuse), then a steady red pulse once armed.
            for (scene::SceneNode* mn : tree.nodesInGroup("mines")) {
                if (!fieldBool(mn, "active")) continue;
                const bool armed = fieldBool(mn, "armed");
                const float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(simTime) * 9.0f);
                const render::Color mc = armed ? render::Color{1.0f, 0.25f * pulse, 0.2f * pulse, 1.0f}
                                               : render::Color{0.6f, 0.6f, 0.65f, 0.8f};
                drawAt(mn->x(), mn->y(), texMine, 1.6f, mc);
            }
            // Auto-turret sentries: a steady blue block that flashes brighter as it fires; dims near expiry.
            for (scene::SceneNode* st : tree.nodesInGroup("sentries")) {
                if (!fieldBool(st, "active")) continue;
                const float slife = static_cast<float>(field(st, "life"));
                const float sammo = static_cast<float>(field(st, "ammo"));
                const float flash = 0.8f + 0.2f * std::sin(static_cast<float>(simTime) * 18.0f);
                // Dims as it nears expiry OR as the magazine runs low, so a spent sentry reads at a glance.
                const float dim = (slife < 3.0f || sammo <= 6.0f) ? 0.5f : 1.0f;
                drawAt(st->x(), st->y(), texSentry, 2.0f,
                       render::Color{0.6f * flash * dim, 0.85f * flash * dim, 1.0f * dim, 1.0f});
            }
            // Power-up pickups (rapid-fire / damage / shield), blinking as they near expiry.
            for (scene::SceneNode* p : tree.nodesInGroup("powerups")) {
                if (!fieldBool(p, "active")) continue;
                const int pk = static_cast<int>(field(p, "kind"));
                render::TextureHandle ptex = pk == 1 ? texPowDamage
                                             : (pk == 2 ? texPowShield
                                                : (pk == 3 ? texPowPierce
                                                   : (pk == 4 ? texPowCryo
                                                      : (pk == 5 ? texPowVamp
                                                         : (pk == 6 ? texPowOverflow : texPowRapid)))));
                const float life = static_cast<float>(field(p, "life"));
                const float blink = (life > 3.0f || std::sin(static_cast<float>(simTime) * 12.0f) > 0.0f)
                                        ? 1.0f
                                        : 0.4f;
                drawAt(p->x(), p->y(), ptex, 1.8f, render::Color{blink, blink, blink, 1.0f});
            }
            // Zombies (only the live ones); sprite + size by kind, tinted red as health drops.
            // Molotov fire patches — drawn under the zombies as a flickering ground blaze.
            for (scene::SceneNode* fp : tree.nodesInGroup("fires")) {
                if (!fieldBool(fp, "active")) continue;
                const float fr = static_cast<float>(field(fp, "radius")) * 2.0f;
                const float flick = 0.6f + 0.4f * std::sin(static_cast<float>(simTime) * 20.0f +
                                                           static_cast<float>(fp->x()));
                drawAt(fp->x(), fp->y(), texFire, fr,
                       render::Color{1.0f, 0.5f * flick + 0.1f, 0.1f, 0.5f});
            }
            // Spitter acid puddles — a bubbling green caustic patch under the zombies.
            for (scene::SceneNode* ap : tree.nodesInGroup("acid")) {
                if (!fieldBool(ap, "active")) continue;
                const float ar = static_cast<float>(field(ap, "radius")) * 2.0f;
                const float bub = 0.7f + 0.3f * std::sin(static_cast<float>(simTime) * 9.0f +
                                                         static_cast<float>(ap->y()));
                drawAt(ap->x(), ap->y(), texAcid, ar,
                       render::Color{0.4f * bub + 0.1f, 0.85f * bub, 0.15f, 0.5f});
            }
            // Explosive barrels — a rusty hazard; flashes as its hull is chipped low.
            for (scene::SceneNode* bl : tree.nodesInGroup("barrels")) {
                if (!fieldBool(bl, "active")) continue;
                const float hpf = static_cast<float>(field(bl, "hp")) / 30.0f;
                const render::Color bc = hpf < 0.5f
                    ? render::Color{1.0f, 0.4f + 0.4f * hpf, 0.2f, 1.0f}   // damaged: hotter
                    : render::Color{0.8f, 0.3f, 0.2f, 1.0f};
                drawAt(bl->x(), bl->y(), texBarrel, 2.6f, bc);
            }
            for (scene::SceneNode* z : tree.nodesInGroup("zombies")) {
                if (!fieldBool(z, "alive")) continue;
                const int zk = static_cast<int>(field(z, "kind"));
                render::TextureHandle ztex = texZombie;
                if (zk == 1) ztex = texRunner;
                else if (zk == 2) ztex = texBrute;
                else if (zk == 3) ztex = texBoss;
                else if (zk == 4) ztex = texExploder;
                else if (zk == 5) ztex = texSpitter;
                else if (zk == 6) ztex = texSplitter;
                else if (zk == 7) ztex = texSummoner;
                else if (zk == 8) ztex = texArmored;
                else if (zk == 9) ztex = texLeaper;
                else if (zk == 10) ztex = texBloater;
                else if (zk == 11) ztex = texScreamer;
                const float size = static_cast<float>(field(z, "radius")) * 2.4f;
                const double hp = field(z, "health"), mhp = field(z, "max_health");
                const float f = mhp > 0.0 ? static_cast<float>(hp / mhp) : 1.0f;
                const bool elite = fieldBool(z, "elite");
                render::Color tint =
                    f < 0.4f ? render::Color{1.0f, 0.55f, 0.55f, 1.0f} : kNoTint;
                float esize = size;
                if (elite) {
                    tint = f < 0.4f ? render::Color{1.0f, 0.75f, 0.35f, 1.0f}   // wounded gold
                                    : render::Color{1.0f, 0.85f, 0.25f, 1.0f};  // champion gold
                    esize = size * 1.3f; // elites are visibly bigger
                } else if (field(z, "slow_timer") > 0.0) {
                    tint = render::Color{0.55f, 0.75f, 1.0f, 1.0f}; // chilled — icy blue
                } else if (field(z, "shield") > 0.0) {
                    tint = render::Color{0.8f, 0.9f, 1.0f, 1.0f};   // armored — steel sheen
                }
                // Frenzied zombies (whipped up by a screamer) flush hot orange while they surge.
                if (field(z, "frenzy_timer") > 0.0) {
                    tint = render::Color{1.0f, 0.55f, 0.2f, 1.0f};
                }
                // Bleeding shows a dark crimson wash that deepens with the stack count, so a
                // hemorrhaging body reads at a glance (fire and rage still override it below).
                const double bleed = field(z, "bleed_stacks");
                if (bleed > 0.0) {
                    const float bt = std::min(1.0f, static_cast<float>(bleed) / 5.0f);
                    tint = render::Color{0.85f, 0.15f + 0.2f * (1.0f - bt), 0.2f, 1.0f};
                }
                // A flinching (staggered) zombie flashes pale — a quick "that landed" tell.
                if (field(z, "stagger_timer") > 0.0) {
                    tint = render::Color{1.0f, 1.0f, 0.75f, 1.0f};
                }
                // An enraged boss pulses an angry red, whatever its wound tint.
                if (fieldBool(z, "enraged")) {
                    const float rp = 0.7f + 0.3f * std::sin(static_cast<float>(simTime) * 16.0f);
                    tint = render::Color{1.0f, 0.25f * rp, 0.2f * rp, 1.0f};
                }
                // Burning overrides other tints: a flickering ember glow.
                if (field(z, "burn_timer") > 0.0) {
                    const float fl = 0.7f + 0.3f * std::sin(static_cast<float>(simTime) * 24.0f +
                                                            static_cast<float>(z->x()));
                    tint = render::Color{1.0f, 0.5f * fl, 0.15f * fl, 1.0f};
                }
                drawAt(z->x(), z->y(), ztex, esize, tint);
            }
            // Bullets in flight.
            for (scene::SceneNode* b : tree.nodesInGroup("bullets")) {
                if (fieldBool(b, "active")) drawAt(b->x(), b->y(), texBullet, 0.7f, kNoTint);
            }
            // Grenades in flight (blink faster as the fuse runs down).
            for (scene::SceneNode* g : tree.nodesInGroup("grenades")) {
                if (!fieldBool(g, "active")) continue;
                const float blink = 0.6f + 0.4f * std::sin(static_cast<float>(simTime) * 20.0f);
                drawAt(g->x(), g->y(), texGrenade, 1.1f, render::Color{blink, 1.0f, blink, 1.0f});
            }
            // Acid globs in flight (spitter projectiles).
            for (scene::SceneNode* s : tree.nodesInGroup("spits")) {
                if (fieldBool(s, "active")) drawAt(s->x(), s->y(), texSpit, 0.9f, kNoTint);
            }
            // Impact particles (sparks + blood), fading with life.
            for (scene::SceneNode* p : tree.nodesInGroup("particles")) {
                if (!fieldBool(p, "active")) continue;
                const int pk = static_cast<int>(field(p, "kind"));
                const double life = field(p, "life"), ml = field(p, "max_life");
                const float a = ml > 0.0 ? static_cast<float>(life / ml) : 0.0f;
                const render::TextureHandle ptex = pk == 1 ? texBlood : texBullet;
                const render::Color pc = pk == 1 ? render::Color{0.85f, 0.15f, 0.15f, a}
                                                 : render::Color{1.0f, 0.9f, 0.5f, a};
                drawAt(p->x(), p->y(), ptex, pk == 1 ? 0.55f : 0.4f, pc);
            }
            // Aim tracer: a few dots from the survivor along the aim vector.
            if (alive) {
                for (int i = 1; i <= 5; ++i) {
                    const double d = 2.0 + i * 1.6;
                    drawAt(survivor->x() + aimX * d, survivor->y() + aimY * d, texBullet, 0.35f,
                           render::Color{1.0f, 0.9f, 0.4f, 0.5f});
                }
            }
            // Muzzle flash while firing (flickers a touch).
            if (alive && firing) {
                const float flick = 1.0f + 0.3f * std::sin(static_cast<float>(simTime) * 60.0f);
                drawAt(survivor->x() + aimX * 2.2, survivor->y() + aimY * 2.2, texBullet, 1.3f * flick,
                       render::Color{1.0f, 0.95f, 0.6f, 0.9f});
            }
            // Survivor on top; tinted while a power-up buff is active (shield = blue halo).
            if (alive) {
                render::Color body = kNoTint;
                const int bk = static_cast<int>(field(survivor, "buff_kind"));
                if (field(survivor, "buff_timer") > 0.0) {
                    if (bk == 2) body = render::Color{0.5f, 0.8f, 1.0f, 1.0f};       // shield
                    else if (bk == 1) body = render::Color{1.0f, 0.6f, 0.6f, 1.0f};  // damage
                    else if (bk == 0) body = render::Color{1.0f, 0.95f, 0.5f, 1.0f}; // rapid fire
                    else if (bk == 3) body = render::Color{0.8f, 0.5f, 1.0f, 1.0f};  // piercing rounds
                    else if (bk == 4) body = render::Color{0.6f, 0.9f, 1.0f, 1.0f};  // cryo nova
                    else if (bk == 5) body = render::Color{0.85f, 0.3f, 0.45f, 1.0f}; // vampiric leech
                    else if (bk == 6) body = render::Color{1.0f, 0.85f, 0.4f, 1.0f};  // overflow ammo
                } else if (fieldBool(survivor, "adrenaline")) {
                    // Last-stand: pulse red-hot while critically wounded.
                    const float pulse = 0.6f + 0.4f * std::sin(static_cast<float>(simTime) * 14.0f);
                    body = render::Color{1.0f, 0.3f * pulse, 0.2f * pulse, 1.0f};
                } else if (field(survivor, "armor") > 0.0) {
                    body = render::Color{0.7f, 0.75f, 0.8f, 1.0f};   // steel plate sheen
                }
                // Dodge-roll i-frames: ghost the survivor translucent-blue while invulnerable.
                if (field(survivor, "iframes") > 0.0) {
                    const float g = 0.5f + 0.5f * std::sin(static_cast<float>(simTime) * 40.0f);
                    body = render::Color{0.6f, 0.85f, 1.0f, 0.35f + 0.35f * g};
                }
                drawAt(survivor->x(), survivor->y(), texSurvivor, 3.0f, body);
            }

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
                          autopilot ? "AUTOPILOT" : "WASD  AIM  FIRE  1-4 GUN  SPACE DODGE  F MELEE  T MINE  G NADE  E EAT",
                          kDim, 0.45f);
            // Active weapon name.
            const char* kWeaponNames[5] = {"PISTOL", "SHOTGUN", "SMG", "RAILGUN", "FLAME"};
            int wi = static_cast<int>(field(survivor, "weapon"));
            if (wi < 0) wi = 0;
            if (wi > 4) wi = 4;
            font.drawText(*renderer, 16.0f, 72.0f, kWeaponNames[wi],
                          render::Color{1.0f, 0.85f, 0.4f, 1.0f}, 0.55f);
            // Ammo readout / reloading indicator.
            char ammoBuf[64];
            if (fieldBool(survivor, "is_reloading")) {
                std::snprintf(ammoBuf, sizeof(ammoBuf), "RELOADING...");
            } else {
                std::snprintf(ammoBuf, sizeof(ammoBuf), "AMMO %d / %d",
                              static_cast<int>(field(survivor, "cur_ammo")),
                              static_cast<int>(field(survivor, "cur_reserve")));
            }
            const bool lowAmmo = field(survivor, "cur_ammo") <= 0.0 || fieldBool(survivor, "is_reloading");
            font.drawText(*renderer, 16.0f, 98.0f, ammoBuf,
                          lowAmmo ? render::Color{0.95f, 0.5f, 0.35f, 1.0f}
                                  : render::Color{0.85f, 0.9f, 0.95f, 1.0f},
                          0.5f);
            char nadeBuf[128];
            std::snprintf(nadeBuf, sizeof(nadeBuf),
                          "GRENADES %d (G)  MINES %d (T)  SENTRY %d (Y)  MOLOTOV %d (X)",
                          static_cast<int>(field(survivor, "grenades")),
                          static_cast<int>(field(survivor, "mines")),
                          static_cast<int>(field(survivor, "sentries")),
                          static_cast<int>(field(survivor, "molotovs")));
            font.drawText(*renderer, 16.0f, 122.0f, nadeBuf, render::Color{0.7f, 0.85f, 0.7f, 1.0f},
                          0.45f);
            // Salvage cash + shop hotkeys (below the ultimate meter to avoid the revive/ult lines).
            char cashBuf[128];
            const int armorNow = static_cast<int>(field(survivor, "armor"));
            std::snprintf(cashBuf, sizeof(cashBuf),
                          "$%d  ARMOR %d   BUY: AMMO 50(6) NADE 40(7) HEAL 60(8) ARMOR 80(9)",
                          static_cast<int>(globalNum(tree, "g_cash")), armorNow);
            font.drawText(*renderer, 16.0f, 170.0f, cashBuf, render::Color{0.95f, 0.85f, 0.35f, 1.0f},
                          0.45f);
            const int revives = static_cast<int>(field(survivor, "revives"));
            if (revives > 0) {
                char revBuf[48];
                std::snprintf(revBuf, sizeof(revBuf), "SECOND WIND x%d", revives);
                const float rp = 0.7f + 0.3f * std::sin(static_cast<float>(simTime) * 4.0f);
                font.drawText(*renderer, 16.0f, 140.0f, revBuf,
                              render::Color{0.95f, 0.85f, 0.35f * rp + 0.2f, 1.0f}, 0.42f);
            }
            // Overcharge ultimate meter.
            const bool ultReady = fieldBool(survivor, "ult_ready");
            char ultBuf[48];
            if (ultReady) {
                std::snprintf(ultBuf, sizeof(ultBuf), "OVERCHARGE READY  (Q)");
            } else {
                std::snprintf(ultBuf, sizeof(ultBuf), "OVERCHARGE %d/%d",
                              static_cast<int>(field(survivor, "ult")),
                              static_cast<int>(field(survivor, "ult_max")));
            }
            const float ultPulse = ultReady ? 0.6f + 0.4f * std::sin(static_cast<float>(simTime) * 8.0f)
                                            : 1.0f;
            font.drawText(*renderer, 16.0f, 146.0f, ultBuf,
                          ultReady ? render::Color{1.0f, 0.8f * ultPulse, 0.2f * ultPulse, 1.0f}
                                   : render::Color{0.6f, 0.7f, 0.9f, 1.0f},
                          0.45f);

            // Wave / score / kills, top-centre-ish.
            char buf[96];
            const int wave = static_cast<int>(globalNum(tree, "g_wave"));
            const int score = static_cast<int>(globalNum(tree, "g_score"));
            const int kills = static_cast<int>(globalNum(tree, "g_kills"));
            std::snprintf(buf, sizeof(buf), "WAVE %d      SCORE %d      KILLS %d", wave, score, kills);
            font.drawText(*renderer, sw * 0.5f - 220.0f, 14.0f, buf, kWhite, 0.55f);
            // Between-wave "cleared" banner: the field is empty and a wave has started.
            if (alive && wave >= 1) {
                int aliveZ = 0;
                for (scene::SceneNode* z : tree.nodesInGroup("zombies"))
                    if (fieldBool(z, "alive")) { ++aliveZ; break; }
                if (aliveZ == 0) {
                    const float pulse = 0.7f + 0.3f * std::sin(static_cast<float>(simTime) * 5.0f);
                    std::snprintf(buf, sizeof(buf), "WAVE %d CLEARED   +%d", wave, wave * 50);
                    font.drawText(*renderer, sw * 0.5f - 150.0f, sh * 0.5f - 120.0f, buf,
                                  render::Color{1.0f, 0.9f, 0.35f * pulse + 0.2f, 1.0f}, 0.7f);
                }
            }
            // Upgrade progression.
            std::snprintf(buf, sizeof(buf), "UPGRADES %d   DMG x%.1f   RATE x%.1f   CRIT %d%%",
                          static_cast<int>(field(survivor, "upgrades")), field(survivor, "dmg_mult"),
                          field(survivor, "rate_mult"),
                          static_cast<int>(field(survivor, "crit_chance") * 100.0 + 0.5));
            font.drawText(*renderer, sw * 0.5f - 220.0f, 44.0f, buf,
                          render::Color{0.7f, 0.95f, 0.75f, 1.0f}, 0.42f);
            // Combo multiplier — flashes big when it's live.
            const int mult = static_cast<int>(globalNum(tree, "g_mult"));
            const int combo = static_cast<int>(globalNum(tree, "g_combo"));
            if (mult > 1) {
                char cbuf[48];
                std::snprintf(cbuf, sizeof(cbuf), "COMBO x%d  (%d streak)", mult, combo);
                const float pulse = 0.7f + 0.3f * std::sin(static_cast<float>(simTime) * 8.0f);
                font.drawText(*renderer, sw * 0.5f - 130.0f, 74.0f, cbuf,
                              render::Color{1.0f, 0.85f * pulse, 0.2f, 1.0f}, 0.6f);
            }

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
            // Threat multiplier tracks the smooth danger() ramp: 1.0 by day up to 1.7 at midnight.
            const double phaseNow = globalNum(tree, "g_phase");
            const double dayLenNow = globalNum(tree, "g_day_len");
            const double threat =
                1.0 + 0.7 * (1.0 - std::cos(phaseNow / dayLenNow * 6.2831853)) * 0.5;
            char threatBuf[48];
            std::snprintf(threatBuf, sizeof(threatBuf), "%s  THREAT x%.1f",
                          night ? "NIGHT" : "DAY", threat);
            font.drawText(*renderer, sw - 210.0f, 16.0f, threatBuf,
                          night ? render::Color{0.95f, 0.5f, 0.45f, 1}
                                : render::Color{0.9f, 0.9f, 0.6f, 1},
                          0.5f);

            // Active wave mutator, under the best readout (only when one is in effect).
            const int mut = static_cast<int>(globalNum(tree, "g_mutator"));
            if (mut > 0) {
                const char* kMutNames[4] = {"", "FERAL HORDE", "HULKING HORDE", "FRENZIED HORDE"};
                font.drawText(*renderer, sw - 300.0f, 68.0f, kMutNames[mut],
                              render::Color{1.0f, 0.55f, 0.85f, 1.0f}, 0.5f);
            }

            // Reticle at the mouse (crosshair) when playing.
            if (!autopilot && alive) {
                const float mx = input.mouseX(), my = input.mouseY();
                const render::Color ret{1.0f, 0.95f, 0.5f, 0.9f};
                rect(mx - 10, my - 1, 8, 2, ret);
                rect(mx + 2, my - 1, 8, 2, ret);
                rect(mx - 1, my - 10, 2, 8, ret);
                rect(mx - 1, my + 2, 2, 8, ret);
            }

            // Persistent best, top-right under the day/night readout.
            std::snprintf(buf, sizeof(buf), "BEST  WAVE %d   SCORE %d", bestWave, bestScore);
            font.drawText(*renderer, sw - 300.0f, 44.0f, buf, render::Color{0.8f, 0.8f, 0.9f, 1.0f}, 0.4f);

            if (!alive) {
                font.drawText(*renderer, sw * 0.5f - 90.0f, sh * 0.5f - 40.0f, "YOU DIED",
                              render::Color{0.95f, 0.25f, 0.25f, 1}, 1.2f);
                std::snprintf(buf, sizeof(buf), "REACHED WAVE %d   -   SCORE %d", wave, score);
                font.drawText(*renderer, sw * 0.5f - 150.0f, sh * 0.5f + 8.0f, buf, kWhite, 0.5f);
                // Run summary: time survived (m:ss) and shot accuracy.
                const int secs = static_cast<int>(field(survivor, "time_survived"));
                const double shotsF = field(survivor, "shots");
                int acc = shotsF > 0.0
                              ? static_cast<int>(field(survivor, "hits") / shotsF * 100.0 + 0.5)
                              : 0;
                if (acc > 100) acc = 100;
                std::snprintf(buf, sizeof(buf), "SURVIVED %d:%02d   -   ACCURACY %d%%   -   %d KILLS",
                              secs / 60, secs % 60, acc, kills);
                font.drawText(*renderer, sw * 0.5f - 190.0f, sh * 0.5f + 34.0f, buf,
                              render::Color{0.75f, 0.85f, 0.95f, 1.0f}, 0.42f);
                // Performance grade: a single S/A/B/C/D letter from wave + kills + accuracy.
                const int tier = zomboid::runRank(wave, kills, acc);
                char rbuf[24];
                std::snprintf(rbuf, sizeof(rbuf), "RANK  %s", zomboid::runRankLetter(tier));
                const render::Color kRankCol[5] = {
                    render::Color{0.7f, 0.7f, 0.75f, 1.0f},   // D grey
                    render::Color{0.6f, 0.8f, 0.6f, 1.0f},    // C green
                    render::Color{0.5f, 0.8f, 1.0f, 1.0f},    // B blue
                    render::Color{0.8f, 0.5f, 1.0f, 1.0f},    // A purple
                    render::Color{1.0f, 0.82f, 0.2f, 1.0f}};  // S gold
                font.drawText(*renderer, sw * 0.5f - 70.0f, sh * 0.5f + 62.0f, rbuf, kRankCol[tier], 0.9f);
                if (newBestThisRun) {
                    font.drawText(*renderer, sw * 0.5f - 90.0f, sh * 0.5f + 100.0f, "NEW BEST!",
                                  render::Color{1.0f, 0.85f, 0.2f, 1.0f}, 0.6f);
                }
                if (!autopilot) {
                    font.drawText(*renderer, sw * 0.5f - 150.0f, sh * 0.5f + 132.0f,
                                  "PRESS ENTER TO RESTART", render::Color{0.85f, 0.9f, 0.95f, 1}, 0.5f);
                }
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
