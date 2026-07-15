// Maz Engine — "GROTTO" — a complete top-down ADVENTURE built from the engine's 2D primitives: the
// polygon renderer, a follow camera (Camera2D center + world scroll), the font HUD, and the
// fixed-timestep loop. The hero walks freely in four directions through a walled dungeon, collects
// gems, dodges patrolling wisps (touching one costs a heart and warps you back to the start), and
// once every gem is gathered the locked exit rune lights up as the way out. Move with WASD / arrow
// keys.
//
// --demo drives the hero with a deterministic autopilot (greedy walk toward the nearest gem) and
// then FREEZES the simulation after a fixed number of steps, so an offscreen capture is
// pixel-identical every run (golden-image friendly). --headless / --frames N for CI.
//
// The dungeon is a little ASCII map: '#' wall, '.'/' ' floor, 'o' gem, 'E' wisp spawn, 'H' hero
// start, 'X' exit rune. Everything here uses only the public engine API — a compact, readable
// example of the top-down action-adventure genre.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
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

// The dungeon map, top row first. Uniform width; the loader scans for the marker cells.
const std::vector<std::string> kMap = {
    "############################################",
    "#H....#........#......o.....#..........o...#",
    "#.###.#.####.#.#.####.#.####.#.######.####.#",
    "#.#...#....#.#.#.#..#.#....#.#.#....#....#..#",
    "#.#.#####.##.#.#.#..#.###.##.#.#.##.####.##.#",
    "#.#.....#....#...#..#...#....#.#..#....#....#",
    "#.#####.####.####.###.#.#.####.##.####.####.#",
    "#o....#....#....E....#.#.#....#..#....#....o#",
    "#.###.####.####.####.#.#.####.##.####.####.#",
    "#...#....#....#....#..#.#....#..#....#......#",
    "###.####.####.####.##.#.#.##.####.####.####.#",
    "#...#..o....#....#..#.#.#..#....#....#.....X#",
    "#.###.####.##.#.####.#.#.####.##.####.####.#",
    "#.....#......#......#...#E...........#o.....#",
    "############################################",
};

constexpr float kTile = 40.0f;

struct Wisp {
    float x = 0, y = 0;   // top-left, world space
    float vx = 0, vy = 0; // one axis nonzero
};

struct World {
    int cols = 0, rows = 0;
    std::vector<std::string> map;
    float heroX = 0, heroY = 0;
    float exitX = 0, exitY = 0;
    std::vector<Wisp> wisps;

    bool wallAt(int col, int row) const {
        if (row < 0 || row >= rows || col < 0 || col >= cols)
            return true;
        return map[static_cast<size_t>(row)][static_cast<size_t>(col)] == '#';
    }
    // True if the world-space AABB [x,x+s]×[y,y+s] overlaps any wall tile.
    bool hitsWall(float x, float y, float s) const {
        const int c0 = static_cast<int>(x / kTile), c1 = static_cast<int>((x + s) / kTile);
        const int r0 = static_cast<int>(y / kTile), r1 = static_cast<int>((y + s) / kTile);
        for (int r = r0; r <= r1; ++r)
            for (int c = c0; c <= c1; ++c)
                if (wallAt(c, r))
                    return true;
        return false;
    }
};

World loadWorld() {
    World w;
    w.map = kMap;
    w.rows = static_cast<int>(kMap.size());
    w.cols = w.rows ? static_cast<int>(kMap[0].size()) : 0;
    for (int r = 0; r < w.rows; ++r) {
        for (int c = 0; c < w.cols; ++c) {
            const char ch = w.map[static_cast<size_t>(r)][static_cast<size_t>(c)];
            const float x = static_cast<float>(c) * kTile, y = static_cast<float>(r) * kTile;
            if (ch == 'H') {
                w.heroX = x + 6.0f;
                w.heroY = y + 6.0f;
                w.map[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
            } else if (ch == 'X') {
                w.exitX = x;
                w.exitY = y;
            } else if (ch == 'E') {
                Wisp e;
                e.x = x + 6.0f;
                e.y = y + 6.0f;
                e.vx = 2.2f; // patrols horizontally, bounces off walls
                w.wisps.push_back(e);
                w.map[static_cast<size_t>(r)][static_cast<size_t>(c)] = '.';
            }
        }
    }
    return w;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("ADVENTURE headless=%d frames=%d autopilot=%d", cfg.headless, cfg.frames,
                 autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — GROTTO";
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

    World w = loadWorld();
    const float viewW = static_cast<float>(cfg.width);
    const float viewH = static_cast<float>(cfg.height);
    const float worldW = static_cast<float>(w.cols) * kTile;
    const float worldH = static_cast<float>(w.rows) * kTile;

    const float heroSize = 28.0f, heroSpeed = 3.0f;
    const float startX = w.heroX, startY = w.heroY;
    float hx = w.heroX, hy = w.heroY;

    int gemsTotal = 0, gemsGot = 0;
    for (const std::string& row : w.map)
        for (char ch : row)
            if (ch == 'o')
                ++gemsTotal;

    int hearts = 3;
    bool escaped = false;
    int simSteps = 0;
    const int kFreezeAt = 220; // autopilot: freeze the sim so the golden capture is deterministic
    int hurtCooldown = 0;

    float camX = hx, camY = hy;

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const bool frozen = autopilot && simSteps >= kFreezeAt;
            if (!frozen && !escaped) {
                // --- Hero intent ---
                float mx = 0.0f, my = 0.0f;
                if (autopilot) {
                    // Greedy walk toward the nearest uncollected gem (or the exit once all
                    // gathered).
                    float tx = w.exitX + kTile * 0.5f, ty = w.exitY + kTile * 0.5f;
                    float bestD = 1e30f;
                    bool haveTarget = gemsGot >= gemsTotal;
                    for (int r = 0; r < w.rows; ++r) {
                        for (int c = 0; c < w.cols; ++c) {
                            if (w.map[static_cast<size_t>(r)][static_cast<size_t>(c)] != 'o')
                                continue;
                            const float gx = static_cast<float>(c) * kTile + kTile * 0.5f;
                            const float gy = static_cast<float>(r) * kTile + kTile * 0.5f;
                            const float d = std::fabs(gx - hx) + std::fabs(gy - hy);
                            if (d < bestD) {
                                bestD = d;
                                tx = gx;
                                ty = gy;
                                haveTarget = true;
                            }
                        }
                    }
                    if (haveTarget) {
                        // Move along the axis with the larger gap; nudge on the other to slip
                        // around walls (a light, deterministic maze-follower — the freeze
                        // guarantees the capture is stable regardless).
                        const float dx = tx - (hx + heroSize * 0.5f);
                        const float dy = ty - (hy + heroSize * 0.5f);
                        if (std::fabs(dx) > 4.0f)
                            mx = dx > 0 ? 1.0f : -1.0f;
                        if (std::fabs(dy) > 4.0f)
                            my = dy > 0 ? 1.0f : -1.0f;
                        // If fully blocked along the primary axis, prefer vertical to escape
                        // corridors.
                        if (mx != 0 && w.hitsWall(hx + mx * heroSpeed, hy, heroSize)) {
                            mx = 0;
                            if (my == 0)
                                my = 1.0f;
                        }
                    }
                } else {
                    if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT))
                        mx -= 1.0f;
                    if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT))
                        mx += 1.0f;
                    if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP))
                        my -= 1.0f;
                    if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN))
                        my += 1.0f;
                }

                // --- Move with per-axis wall resolution (slide along walls) ---
                const float nx = hx + mx * heroSpeed;
                if (!w.hitsWall(nx, hy, heroSize))
                    hx = nx;
                const float ny = hy + my * heroSpeed;
                if (!w.hitsWall(hx, ny, heroSize))
                    hy = ny;
                if (hx < 0)
                    hx = 0;
                if (hx > worldW - heroSize)
                    hx = worldW - heroSize;
                if (hy < 0)
                    hy = 0;
                if (hy > worldH - heroSize)
                    hy = worldH - heroSize;

                // --- Gem pickup (center cell) ---
                const int gc = static_cast<int>((hx + heroSize * 0.5f) / kTile);
                const int gr = static_cast<int>((hy + heroSize * 0.5f) / kTile);
                if (gr >= 0 && gr < w.rows && gc >= 0 && gc < w.cols &&
                    w.map[static_cast<size_t>(gr)][static_cast<size_t>(gc)] == 'o') {
                    w.map[static_cast<size_t>(gr)][static_cast<size_t>(gc)] = '.';
                    ++gemsGot;
                }

                // --- Wisps patrol; bounce off walls ---
                for (Wisp& e : w.wisps) {
                    float ex = e.x + e.vx, ey = e.y + e.vy;
                    if (w.hitsWall(ex, e.y, heroSize)) {
                        e.vx = -e.vx;
                        ex = e.x;
                    }
                    if (w.hitsWall(e.x, ey, heroSize)) {
                        e.vy = -e.vy;
                        ey = e.y;
                    }
                    e.x = ex;
                    e.y = ey;
                }

                // --- Wisp contact: lose a heart and warp back to the start ---
                if (hurtCooldown > 0)
                    --hurtCooldown;
                for (const Wisp& e : w.wisps) {
                    const bool overlap = hx < e.x + heroSize && hx + heroSize > e.x &&
                                         hy < e.y + heroSize && hy + heroSize > e.y;
                    if (overlap && hurtCooldown == 0) {
                        if (hearts > 0)
                            --hearts;
                        hx = startX;
                        hy = startY;
                        hurtCooldown = 45;
                        break;
                    }
                }

                // --- Exit rune (only active once every gem is collected) ---
                if (gemsGot >= gemsTotal && hx + heroSize > w.exitX && hx < w.exitX + kTile &&
                    hy + heroSize > w.exitY && hy < w.exitY + kTile) {
                    escaped = true;
                }

                ++simSteps;
            }

            // Camera eases toward the hero, clamped to the dungeon bounds.
            const float tcx = hx + heroSize * 0.5f, tcy = hy + heroSize * 0.5f;
            camX += (tcx - camX) * 0.15f;
            camY += (tcy - camY) * 0.15f;
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

        renderer->setClearColor(render::Color{0.03f, 0.03f, 0.05f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = false;
            cam.centerX = camX;
            cam.centerY = camY;
            renderer->setCamera2D(cam);

            const int c0 = static_cast<int>((camX - viewW * 0.5f) / kTile) - 1;
            const int c1 = static_cast<int>((camX + viewW * 0.5f) / kTile) + 1;
            const int r0 = static_cast<int>((camY - viewH * 0.5f) / kTile) - 1;
            const int r1 = static_cast<int>((camY + viewH * 0.5f) / kTile) + 1;
            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    if (r < 0 || r >= w.rows || c < 0 || c >= w.cols)
                        continue;
                    const char ch = w.map[static_cast<size_t>(r)][static_cast<size_t>(c)];
                    const float x = static_cast<float>(c) * kTile,
                                y = static_cast<float>(r) * kTile;
                    if (ch == '#') {
                        quad(*renderer, x, y, kTile, kTile,
                             render::Color{0.16f, 0.17f, 0.26f, 1.0f});
                        quad(*renderer, x + 2.0f, y + 2.0f, kTile - 4.0f, kTile - 4.0f,
                             render::Color{0.22f, 0.24f, 0.36f, 1.0f});
                    } else {
                        quad(*renderer, x, y, kTile, kTile,
                             render::Color{0.09f, 0.10f, 0.14f, 1.0f});
                        if (ch == 'o') {
                            quad(*renderer, x + kTile * 0.32f, y + kTile * 0.30f, kTile * 0.36f,
                                 kTile * 0.40f, render::Color{0.45f, 0.9f, 1.0f, 1.0f});
                        }
                    }
                }
            }

            // Exit rune: dim while locked, bright once all gems are collected.
            const bool open = gemsGot >= gemsTotal;
            quad(*renderer, w.exitX + 6.0f, w.exitY + 6.0f, kTile - 12.0f, kTile - 12.0f,
                 open ? render::Color{0.4f, 1.0f, 0.5f, 1.0f}
                      : render::Color{0.3f, 0.35f, 0.3f, 1.0f});

            // Wisps.
            for (const Wisp& e : w.wisps) {
                quad(*renderer, e.x, e.y, heroSize, heroSize,
                     render::Color{0.95f, 0.35f, 0.55f, 1.0f});
                quad(*renderer, e.x + 6.0f, e.y + 6.0f, heroSize - 12.0f, heroSize - 12.0f,
                     render::Color{1.0f, 0.7f, 0.8f, 1.0f});
            }

            // Hero (blinks briefly after being hurt).
            if (hurtCooldown == 0 || (hurtCooldown / 4) % 2 == 0) {
                quad(*renderer, hx, hy, heroSize, heroSize,
                     render::Color{0.95f, 0.82f, 0.3f, 1.0f});
                quad(*renderer, hx + 6.0f, hy + 6.0f, heroSize - 12.0f, heroSize - 12.0f,
                     render::Color{0.2f, 0.15f, 0.05f, 1.0f});
            }

            // HUD pass: fixed to the screen (pixel space).
            render::Camera2D hud;
            hud.usePixelSpace = true;
            renderer->setCamera2D(hud);
            const render::Color kInk{0.92f, 0.95f, 1.0f, 1.0f};
            char line[64];
            std::snprintf(line, sizeof(line), "GEMS  %d / %d", gemsGot,
                          gemsGot >= 0 ? gemsTotal : 0);
            font.drawText(*renderer, 20.0f, 18.0f, line, kInk, 0.7f);
            std::snprintf(line, sizeof(line), "HEARTS  %d", hearts);
            font.drawText(*renderer, 20.0f, 58.0f, line, render::Color{1.0f, 0.5f, 0.55f, 1.0f},
                          0.6f);
            font.drawText(*renderer, 20.0f, viewH - 40.0f,
                          autopilot ? "GROTTO  -  AUTOPILOT"
                                    : "GROTTO  -  WASD / ARROWS   ESC QUIT",
                          render::Color{0.55f, 0.62f, 0.72f, 1.0f}, 0.45f);
            font.drawText(*renderer, viewW - 340.0f, 18.0f,
                          open ? "EXIT RUNE OPEN" : "FIND ALL GEMS",
                          open ? render::Color{0.45f, 0.95f, 0.55f, 1.0f}
                               : render::Color{0.6f, 0.6f, 0.5f, 1.0f},
                          0.5f);
            if (escaped) {
                font.drawText(*renderer, viewW * 0.5f - 130.0f, viewH * 0.4f, "ESCAPED!",
                              render::Color{0.45f, 0.95f, 0.55f, 1.0f}, 1.2f);
            }

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ADVENTURE shutting down gems=%d/%d hearts=%d escaped=%d after %d frames (%s)",
                 gemsGot, gemsTotal, hearts, escaped, rendered,
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
