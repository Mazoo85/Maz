// Maz Engine — "ORB RUN" (sample arcade game)
// An original, genre-neutral mini-game showing the engine can ship a real title: a game-state
// machine (title -> play -> win/lose -> restart), player movement, collectible orbs, roving
// hazards, a countdown timer, and a HUD. Run --headless / --frames N for CI; --demo autopilots
// (auto-start + steer toward orbs) for offscreen capture.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

namespace {

constexpr int kWinScore = 12;
constexpr float kRoundSeconds = 30.0f;
constexpr float kPlayerSize = 28.0f;
constexpr float kOrbSize = 22.0f;
constexpr float kHazardSize = 30.0f;
constexpr int kOrbCount = 6;
constexpr int kHazardCount = 5;
constexpr float kHudTop = 64.0f; // reserve the top strip for the HUD

void putPixel(std::vector<uint8_t>& px, uint32_t w, uint32_t x, uint32_t y, uint8_t r, uint8_t g,
              uint8_t b, uint8_t a) {
    const size_t i = (static_cast<size_t>(y) * w + x) * 4;
    px[i + 0] = r;
    px[i + 1] = g;
    px[i + 2] = b;
    px[i + 3] = a;
}

// Solid square with a darker border. eyes=true draws a little face (player).
std::vector<uint8_t> makeSquare(uint32_t s, uint8_t r, uint8_t g, uint8_t b, bool eyes) {
    std::vector<uint8_t> px(static_cast<size_t>(s) * s * 4, 0);
    for (uint32_t y = 0; y < s; ++y) {
        for (uint32_t x = 0; x < s; ++x) {
            const bool border = x == 0 || y == 0 || x == s - 1 || y == s - 1;
            if (border) {
                putPixel(px, s, x, y, 20, 20, 28, 255);
            } else {
                putPixel(px, s, x, y, r, g, b, 255);
            }
        }
    }
    if (eyes && s >= 16) {
        const uint32_t ey = s * 6 / 16;
        putPixel(px, s, s * 5 / 16, ey, 20, 20, 28, 255);
        putPixel(px, s, s * 6 / 16, ey, 20, 20, 28, 255);
        putPixel(px, s, s * 10 / 16, ey, 20, 20, 28, 255);
        putPixel(px, s, s * 11 / 16, ey, 20, 20, 28, 255);
    }
    return px;
}

// A glowing filled circle (collectible orb).
std::vector<uint8_t> makeOrb(uint32_t s) {
    std::vector<uint8_t> px(static_cast<size_t>(s) * s * 4, 0);
    const float c = (static_cast<float>(s) - 1.0f) * 0.5f;
    const float rad = c;
    for (uint32_t y = 0; y < s; ++y) {
        for (uint32_t x = 0; x < s; ++x) {
            const float dx = static_cast<float>(x) - c;
            const float dy = static_cast<float>(y) - c;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d <= rad) {
                const float t = d / rad;           // 0 center -> 1 edge
                const uint8_t core = static_cast<uint8_t>(255.0f * (1.0f - t * 0.5f));
                putPixel(px, s, x, y, core, 255, 230, 255);
            }
        }
    }
    return px;
}

struct Vec {
    float x, y;
};
struct Hazard {
    float x, y, vx, vy;
};

float dist(float ax, float ay, float bx, float by) {
    const float dx = ax - bx, dy = ay - by;
    return std::sqrt(dx * dx + dy * dy);
}

enum class State { Title, Play, Win, Lose };

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("ORB RUN starting (headless=%d frames=%d autopilot=%d)", cfg.headless, cfg.frames,
                 autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — ORB RUN";
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

    audio::Audio audio;
    audio.init(); // no-op if no device

    // Procedurally-synthesized sound effects (no asset files).
    const audio::SoundDesc sfxStart{audio::Wave::Square, 440.0f, 880.0f, 0.16f, 0.30f};
    const audio::SoundDesc sfxPickup{audio::Wave::Square, 720.0f, 1120.0f, 0.09f, 0.28f};
    const audio::SoundDesc sfxHit{audio::Wave::Square, 180.0f, 50.0f, 0.35f, 0.35f};
    const audio::SoundDesc sfxHitNoise{audio::Wave::Noise, 300.0f, 0.0f, 0.25f, 0.25f};
    const audio::SoundDesc sfxWin{audio::Wave::Square, 523.0f, 1047.0f, 0.5f, 0.30f};

    // Textures.
    const uint8_t white[4] = {255, 255, 255, 255};
    render::TextureHandle whiteTex = renderer->createTexture(1, 1, white);
    render::TextureHandle playerTex =
        renderer->createTexture(16, 16, makeSquare(16, 240, 200, 70, true).data());
    render::TextureHandle orbTex = renderer->createTexture(24, 24, makeOrb(24).data());
    render::TextureHandle hazardTex =
        renderer->createTexture(20, 20, makeSquare(20, 220, 60, 70, false).data());

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        if (!font.load(*renderer, fontPath.c_str(), 40.0f)) {
            MAZ_LOG_WARN("HUD font failed to load; continuing without text");
        }
    }

    std::mt19937 rng(20260711u);
    auto frand = [&](float lo, float hi) {
        std::uniform_real_distribution<float> d(lo, hi);
        return d(rng);
    };

    // Game state.
    State state = State::Title;
    Vec player{0.0f, 0.0f};
    std::vector<Vec> orbs(kOrbCount);
    std::vector<Hazard> hazards(kHazardCount);
    int score = 0;
    float timeLeft = kRoundSeconds;

    auto arenaBounds = [&](float& x0, float& y0, float& x1, float& y1) {
        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        x0 = 24.0f;
        y0 = kHudTop;
        x1 = static_cast<float>(bw > 0 ? bw : cfg.width) - 24.0f;
        y1 = static_cast<float>(bh > 0 ? bh : cfg.height) - 24.0f;
    };

    auto placeInArena = [&](float size) -> Vec {
        float x0, y0, x1, y1;
        arenaBounds(x0, y0, x1, y1);
        return Vec{frand(x0, x1 - size), frand(y0, y1 - size)};
    };

    auto startGame = [&]() {
        float x0, y0, x1, y1;
        arenaBounds(x0, y0, x1, y1);
        player = Vec{(x0 + x1) * 0.5f, (y0 + y1) * 0.5f};
        for (Vec& o : orbs) {
            o = placeInArena(kOrbSize);
        }
        for (Hazard& h : hazards) {
            const Vec p = placeInArena(kHazardSize);
            h = Hazard{p.x, p.y, frand(-140.0f, 140.0f), frand(-140.0f, 140.0f)};
            if (std::fabs(h.vx) < 40.0f) h.vx = 90.0f;
            if (std::fabs(h.vy) < 40.0f) h.vy = -90.0f;
        }
        score = 0;
        timeLeft = kRoundSeconds;
        state = State::Play;
        audio.play(sfxStart);
        audio.setMusic(true);
    };

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }
        const bool startPressed = input.keyPressed(SDL_SCANCODE_SPACE) ||
                                  input.keyPressed(SDL_SCANCODE_RETURN) ||
                                  (autopilot && state != State::Play);
        if (startPressed && state != State::Play) {
            startGame();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            if (state != State::Play) {
                continue;
            }
            const float dt = static_cast<float>(clock.fixedDelta());
            float x0, y0, x1, y1;
            arenaBounds(x0, y0, x1, y1);

            // Movement input (or autopilot toward the nearest orb).
            float mx = 0.0f, my = 0.0f;
            if (autopilot) {
                float best = 1e9f;
                for (const Vec& o : orbs) {
                    const float d = dist(player.x, player.y, o.x, o.y);
                    if (d < best) {
                        best = d;
                        mx = o.x - player.x;
                        my = o.y - player.y;
                    }
                }
            } else {
                if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT)) mx -= 1.0f;
                if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT)) mx += 1.0f;
                if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP)) my -= 1.0f;
                if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN)) my += 1.0f;
            }
            const float mlen = std::sqrt(mx * mx + my * my);
            if (mlen > 0.0001f) {
                const float speed = 260.0f;
                player.x += mx / mlen * speed * dt;
                player.y += my / mlen * speed * dt;
            }
            player.x = std::min(std::max(player.x, x0), x1 - kPlayerSize);
            player.y = std::min(std::max(player.y, y0), y1 - kPlayerSize);

            const float pcx = player.x + kPlayerSize * 0.5f;
            const float pcy = player.y + kPlayerSize * 0.5f;

            // Collect orbs.
            for (Vec& o : orbs) {
                const float ocx = o.x + kOrbSize * 0.5f;
                const float ocy = o.y + kOrbSize * 0.5f;
                if (dist(pcx, pcy, ocx, ocy) < (kPlayerSize + kOrbSize) * 0.4f) {
                    ++score;
                    o = placeInArena(kOrbSize);
                    audio.play(sfxPickup);
                }
            }

            // Move hazards (bounce), check collisions.
            for (Hazard& h : hazards) {
                h.x += h.vx * dt;
                h.y += h.vy * dt;
                if (h.x < x0) { h.x = x0; h.vx = -h.vx; }
                if (h.y < y0) { h.y = y0; h.vy = -h.vy; }
                if (h.x + kHazardSize > x1) { h.x = x1 - kHazardSize; h.vx = -h.vx; }
                if (h.y + kHazardSize > y1) { h.y = y1 - kHazardSize; h.vy = -h.vy; }
                const float hcx = h.x + kHazardSize * 0.5f;
                const float hcy = h.y + kHazardSize * 0.5f;
                if (state == State::Play &&
                    dist(pcx, pcy, hcx, hcy) < (kPlayerSize + kHazardSize) * 0.4f) {
                    state = State::Lose;
                    audio.play(sfxHit);
                    audio.play(sfxHitNoise);
                    audio.setMusic(false);
                }
            }

            timeLeft -= dt;
            if (state == State::Play && score >= kWinScore) {
                state = State::Win;
                audio.play(sfxWin);
                audio.setMusic(false);
            } else if (state == State::Play && timeLeft <= 0.0f) {
                timeLeft = 0.0f;
                state = State::Lose;
                audio.play(sfxHit);
                audio.setMusic(false);
            }
        }

        // ---- Render ----
        renderer->setClearColor(render::Color{0.05f, 0.06f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            uint32_t bw = 0, bh = 0;
            window.drawableSize(bw, bh);
            const float sw = static_cast<float>(bw);
            const float sh = static_cast<float>(bh);

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);

            auto rect = [&](float rx, float ry, float rw, float rh, render::Color col) {
                render::SpriteDesc s;
                s.x = rx;
                s.y = ry;
                s.width = rw;
                s.height = rh;
                s.color = col;
                renderer->drawSprite(whiteTex, s);
            };
            auto sprite = [&](render::TextureHandle t, float sx, float sy, float sz) {
                render::SpriteDesc s;
                s.x = sx;
                s.y = sy;
                s.width = sz;
                s.height = sz;
                renderer->drawSprite(t, s);
            };
            auto centerText = [&](float cx, float ty, const char* text, render::Color col,
                                  float scale) {
                font.drawText(*renderer, cx - font.textWidth(text, scale) * 0.5f, ty, text, col,
                              scale);
            };

            const render::Color kWhite{1, 1, 1, 1};
            const render::Color kDim{0.75f, 0.8f, 0.9f, 1};

            if (state == State::Play || state == State::Win || state == State::Lose) {
                // Play field entities.
                for (const Vec& o : orbs) {
                    sprite(orbTex, o.x, o.y, kOrbSize);
                }
                for (const Hazard& h : hazards) {
                    sprite(hazardTex, h.x, h.y, kHazardSize);
                }
                sprite(playerTex, player.x, player.y, kPlayerSize);

                // HUD.
                char buf[64];
                std::snprintf(buf, sizeof(buf), "SCORE  %d / %d", score, kWinScore);
                font.drawText(*renderer, 20.0f, 14.0f, buf, kWhite, 0.7f);
                std::snprintf(buf, sizeof(buf), "TIME  %d", static_cast<int>(timeLeft + 0.999f));
                font.drawText(*renderer, sw - 170.0f, 14.0f, buf, kWhite, 0.7f);
            }

            if (state == State::Title) {
                centerText(sw * 0.5f, sh * 0.32f, "ORB RUN", kWhite, 1.6f);
                centerText(sw * 0.5f, sh * 0.50f, "collect 12 orbs - dodge the red blocks", kDim,
                           0.6f);
                centerText(sw * 0.5f, sh * 0.60f, "WASD / ARROWS to move", kDim, 0.6f);
                centerText(sw * 0.5f, sh * 0.72f, "PRESS SPACE TO START", kWhite, 0.8f);
            } else if (state == State::Win || state == State::Lose) {
                rect(0.0f, 0.0f, sw, sh, render::Color{0.0f, 0.0f, 0.0f, 0.6f});
                const bool win = state == State::Win;
                centerText(sw * 0.5f, sh * 0.34f, win ? "YOU WIN!" : "GAME OVER",
                           win ? render::Color{0.4f, 1.0f, 0.5f, 1} : render::Color{1, 0.5f, 0.4f, 1},
                           1.5f);
                char buf[48];
                std::snprintf(buf, sizeof(buf), "SCORE  %d", score);
                centerText(sw * 0.5f, sh * 0.52f, buf, kWhite, 0.8f);
                centerText(sw * 0.5f, sh * 0.66f, "PRESS SPACE TO PLAY AGAIN", kDim, 0.7f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ORB RUN shutting down (score %d, renderer %s, audio %s)", score,
                 renderer->isActive() ? "active" : "inactive",
                 audio.active() ? "active" : "inactive");
    audio.shutdown();
    renderer->shutdown();
    window.shutdown();
    return 0;
}
