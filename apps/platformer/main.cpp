// Maz Engine — "SKIP" — a complete side-scrolling PLATFORMER built from the engine's 2D
// primitives: the polygon renderer, a follow camera (Camera2D center + world scroll), the font HUD,
// and the fixed-timestep loop. A gravity-driven character runs and jumps across solid tiles, the
// camera tracks it through a level wider than the screen, coins are collected on touch, and
// reaching the flag wins. Run with A/D or arrow keys to move, Space / W / Up to jump.
//
// --demo drives the character with a deterministic autopilot (run right, auto-jump at ledges/walls)
// and then FREEZES the simulation after a fixed number of steps, so an offscreen capture is
// pixel-identical every run (golden-image friendly). --headless / --frames N for CI.
//
// The level is a little ASCII map: '#' solid, 'o' coin, 'P' player start, 'G' goal flag, ' ' air.
// Everything here uses only the public engine API — a compact, readable example of a real genre.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

void quad(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

// The level map, top row first. Width is uniform; edit freely — the loader scans for 'P'/'G'/'o'.
const std::vector<std::string> kLevel = {
    "                                                            ",
    "                                                            ",
    "                                          o o o             ",
    "                              ####       #######           G",
    "                    o o                                  ###",
    "          o        ######            o                     ",
    "         ###                        ###          ####      ",
    "   P                     o o                                ",
    "  ###          o o      #####                    o o o      ",
    "               ####                    ###      ########    ",
    "        ###                   o o                           ",
    "                    ###      #####       o o                ",
    "   o o                                  #####        o o    ",
    "  #####                ###                            ###   ",
    "############################################################",
    "############################################################",
};

constexpr float kTile = 40.0f;

struct Level {
    int cols = 0, rows = 0;
    std::vector<std::string> map;
    float startX = 0, startY = 0;
    float goalX = 0, goalY = 0;

    bool solidAt(int col, int row) const {
        if (row < 0 || row >= rows || col < 0 || col >= cols)
            return false;
        return map[static_cast<size_t>(row)][static_cast<size_t>(col)] == '#';
    }
    // True if the world-space AABB [x,x+w]×[y,y+h] overlaps any solid tile.
    bool overlapsSolid(float x, float y, float w, float h) const {
        const int c0 = static_cast<int>(x / kTile), c1 = static_cast<int>((x + w) / kTile);
        const int r0 = static_cast<int>(y / kTile), r1 = static_cast<int>((y + h) / kTile);
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                if (solidAt(c, r))
                    return true;
        return false;
    }
};

Level loadLevel() {
    Level lv;
    lv.map = kLevel;
    lv.rows = static_cast<int>(kLevel.size());
    lv.cols = lv.rows ? static_cast<int>(kLevel[0].size()) : 0;
    for (int r = 0; r < lv.rows; ++r) {
        for (int c = 0; c < lv.cols; ++c) {
            const char ch = lv.map[static_cast<size_t>(r)][static_cast<size_t>(c)];
            if (ch == 'P') {
                lv.startX = static_cast<float>(c) * kTile;
                lv.startY = static_cast<float>(r) * kTile;
            } else if (ch == 'G') {
                lv.goalX = static_cast<float>(c) * kTile;
                lv.goalY = static_cast<float>(r) * kTile;
            }
        }
    }
    return lv;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("PLATFORMER headless=%d frames=%d autopilot=%d", cfg.headless, cfg.frames,
                 autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — SKIP";
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
        font.load(*renderer, fontPath.c_str(), 48.0f);
    }

    Level lv = loadLevel();
    const float viewW = static_cast<float>(cfg.width);
    const float viewH = static_cast<float>(cfg.height);
    const float worldW = static_cast<float>(lv.cols) * kTile;
    const float worldH = static_cast<float>(lv.rows) * kTile;

    // Character state (world space; origin top-left, y down).
    const float pw = 26.0f, ph = 34.0f;
    float px = lv.startX + (kTile - pw) * 0.5f;
    float py = lv.startY;
    float vx = 0.0f, vy = 0.0f;
    bool onGround = false;
    bool facing = true; // true = right

    const float kGravity = 0.6f, kMove = 4.2f, kJump = -13.2f, kMaxFall = 16.0f;

    // Coins: collect on overlap. Track which cells are still uncollected.
    int coinsTotal = 0, coinsGot = 0;
    for (const std::string& row : lv.map)
        for (char ch : row)
            if (ch == 'o')
                ++coinsTotal;

    bool won = false;
    int simSteps = 0;
    const int kFreezeAt = 170; // autopilot: freeze the sim so the golden capture is deterministic

    float camX = px, camY = py;

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const bool frozen = autopilot && simSteps >= kFreezeAt;
            if (!frozen && !won) {
                // --- Input / intent ---
                float moveDir = 0.0f;
                bool jump = false;
                if (autopilot) {
                    moveDir = 1.0f; // always press right
                    // Auto-jump when a wall is directly ahead or the ground ahead drops away.
                    const float aheadX = px + pw + 2.0f;
                    const bool wallAhead = lv.overlapsSolid(aheadX, py, 1.0f, ph - 2.0f);
                    const bool groundAhead =
                        lv.overlapsSolid(px + pw + kTile * 0.5f, py + ph + 2.0f, 1.0f, 2.0f);
                    if (onGround && (wallAhead || !groundAhead))
                        jump = true;
                } else {
                    if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT))
                        moveDir -= 1.0f;
                    if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT))
                        moveDir += 1.0f;
                    jump = input.keyPressed(SDL_SCANCODE_SPACE) ||
                           input.keyPressed(SDL_SCANCODE_W) || input.keyPressed(SDL_SCANCODE_UP);
                }
                if (moveDir > 0)
                    facing = true;
                if (moveDir < 0)
                    facing = false;
                vx = moveDir * kMove;
                if (jump && onGround) {
                    vy = kJump;
                    onGround = false;
                }

                // --- Integrate + resolve against solid tiles (X then Y) ---
                vy += kGravity;
                if (vy > kMaxFall)
                    vy = kMaxFall;

                px += vx;
                if (lv.overlapsSolid(px, py, pw, ph)) {
                    // Step back to the tile boundary along X.
                    px -= vx;
                    while (!lv.overlapsSolid(px + (vx > 0 ? 1.0f : -1.0f), py, pw, ph) &&
                           (vx > 0 ? px < worldW : px > 0)) {
                        px += (vx > 0 ? 1.0f : -1.0f);
                    }
                    vx = 0.0f;
                }
                if (px < 0)
                    px = 0;
                if (px > worldW - pw)
                    px = worldW - pw;

                onGround = false;
                py += vy;
                if (lv.overlapsSolid(px, py, pw, ph)) {
                    py -= vy;
                    while (!lv.overlapsSolid(px, py + (vy > 0 ? 1.0f : -1.0f), pw, ph) &&
                           (vy > 0 ? py < worldH : py > 0)) {
                        py += (vy > 0 ? 1.0f : -1.0f);
                    }
                    if (vy > 0)
                        onGround = true;
                    vy = 0.0f;
                }
                if (py > worldH + 200.0f) { // fell out of the world → respawn
                    px = lv.startX;
                    py = lv.startY;
                    vx = vy = 0.0f;
                }

                // --- Coin pickup ---
                const int cc = static_cast<int>((px + pw * 0.5f) / kTile);
                const int cr = static_cast<int>((py + ph * 0.5f) / kTile);
                if (cr >= 0 && cr < lv.rows && cc >= 0 && cc < lv.cols &&
                    lv.map[static_cast<size_t>(cr)][static_cast<size_t>(cc)] == 'o') {
                    lv.map[static_cast<size_t>(cr)][static_cast<size_t>(cc)] = ' ';
                    ++coinsGot;
                }

                // --- Goal ---
                if (px + pw > lv.goalX && px < lv.goalX + kTile && py + ph > lv.goalY &&
                    py < lv.goalY + kTile) {
                    won = true;
                }

                ++simSteps;
            }

            // Camera eases toward the character, clamped to the level bounds.
            const float targetCx = px + pw * 0.5f;
            const float targetCy = py + ph * 0.5f;
            camX += (targetCx - camX) * 0.12f;
            camY += (targetCy - camY) * 0.12f;
            const float minCx = viewW * 0.5f, maxCx = worldW - viewW * 0.5f;
            const float minCy = viewH * 0.5f, maxCy = worldH - viewH * 0.5f;
            if (camX < minCx)
                camX = minCx;
            if (maxCx > minCx && camX > maxCx)
                camX = maxCx;
            if (camY < minCy)
                camY = minCy;
            if (maxCy > minCy && camY > maxCy)
                camY = maxCy;
        }

        renderer->setClearColor(render::Color{0.07f, 0.10f, 0.16f, 1.0f});
        if (renderer->beginFrame()) {
            // World pass: follow camera (1 unit = 1 pixel, centered on the character).
            render::Camera2D cam;
            cam.usePixelSpace = false;
            cam.centerX = camX;
            cam.centerY = camY;
            cam.zoom = 1.0f;
            renderer->setCamera2D(cam);

            // Only draw tiles within the visible window (simple cull).
            const int c0 = static_cast<int>((camX - viewW * 0.5f) / kTile) - 1;
            const int c1 = static_cast<int>((camX + viewW * 0.5f) / kTile) + 1;
            const int r0 = static_cast<int>((camY - viewH * 0.5f) / kTile) - 1;
            const int r1 = static_cast<int>((camY + viewH * 0.5f) / kTile) + 1;
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    if (r < 0 || r >= lv.rows || c < 0 || c >= lv.cols)
                        continue;
                    const char ch = lv.map[static_cast<size_t>(r)][static_cast<size_t>(c)];
                    const float x = static_cast<float>(c) * kTile,
                                y = static_cast<float>(r) * kTile;
                    if (ch == '#') {
                        quad(*renderer, x, y, kTile, kTile,
                             render::Color{0.20f, 0.28f, 0.38f, 1.0f});
                        quad(*renderer, x, y, kTile, 5.0f,
                             render::Color{0.32f, 0.45f, 0.60f, 1.0f});
                    } else if (ch == 'o') {
                        quad(*renderer, x + kTile * 0.35f, y + kTile * 0.30f, kTile * 0.30f,
                             kTile * 0.40f, render::Color{1.0f, 0.83f, 0.30f, 1.0f});
                    }
                }
            }

            // Goal flag.
            quad(*renderer, lv.goalX + kTile * 0.45f, lv.goalY - kTile, 5.0f, kTile * 2.0f,
                 render::Color{0.7f, 0.75f, 0.8f, 1.0f});
            quad(*renderer, lv.goalX + kTile * 0.5f, lv.goalY - kTile, kTile * 0.5f, kTile * 0.35f,
                 render::Color{0.35f, 0.85f, 0.45f, 1.0f});

            // Character (a little directional notch shows facing).
            const render::Color body{0.95f, 0.55f, 0.25f, 1.0f};
            quad(*renderer, px, py, pw, ph, body);
            quad(*renderer, facing ? px + pw - 7.0f : px + 3.0f, py + 7.0f, 4.0f, 4.0f,
                 render::Color{0.06f, 0.08f, 0.12f, 1.0f});

            // HUD pass: fixed to the screen (pixel space).
            render::Camera2D hud;
            hud.usePixelSpace = true;
            renderer->setCamera2D(hud);
            const render::Color kInk{0.92f, 0.95f, 1.0f, 1.0f};
            char line[64];
            std::snprintf(line, sizeof(line), "COINS  %d / %d", coinsGot, coinsTotal);
            font.drawText(*renderer, 20.0f, 18.0f, line, kInk, 0.7f);
            font.drawText(*renderer, 20.0f, viewH - 40.0f,
                          autopilot ? "SKIP  -  AUTOPILOT"
                                    : "SKIP  -  A/D OR ARROWS   SPACE JUMP   ESC QUIT",
                          render::Color{0.55f, 0.62f, 0.72f, 1.0f}, 0.45f);
            if (won) {
                font.drawText(*renderer, viewW * 0.5f - 150.0f, viewH * 0.4f, "LEVEL CLEAR!",
                              render::Color{0.45f, 0.9f, 0.5f, 1.0f}, 1.1f);
            }

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PLATFORMER shutting down coins=%d/%d won=%d after %d frames (renderer %s)",
                 coinsGot, coinsTotal, won, rendered, renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
