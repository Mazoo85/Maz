// Maz Engine — "CATCHER" (a complete little game, integrating many engine systems)
// Move the paddle to catch gold coins (+score) and dodge red hazards (-life). It's a full game loop
// on the scene stack — Menu -> Play -> Game Over with a persistent high score — wiring together:
//   core::SceneStack  (menu / play / game-over screens)
//   core::EventBus     (catch/hit events fan out to score, particles, shake)
//   fx::ParticleSystem (bursts on catch/hit)     game::Shake (screen jolt on a hit)
//   core::KeyValueStore(high score saved to disk) ui::Font (HUD + menus)
// Runs in a deterministic ATTRACT mode (a seeded RNG spawns items, an AI plays the paddle) so CI /
// golden capture is stable; press an arrow key to take over. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace maz;

namespace {

// A tiny deterministic RNG (no time/global state) so attract mode is reproducible for the golden.
struct Rng {
    uint64_t s = 0x243F6A8885A308D3ull;
    uint32_t next() {
        s ^= s << 13;
        s ^= s >> 7;
        s ^= s << 17;
        return static_cast<uint32_t>(s >> 32);
    }
    float unit() { return static_cast<float>(next()) / 4294967296.0f; }
    float range(float a, float b) { return a + (b - a) * unit(); }
};

struct Game {
    render::Renderer* r = nullptr;
    ui::Font* font = nullptr;
    platform::Input* input = nullptr;
    core::SceneStack* stack = nullptr;
    core::KeyValueStore* store = nullptr;
    render::TextureHandle white = 0, disc = 0, dot = 0;
    float sw = 1280.0f, sh = 720.0f;
    int highScore = 0;
    int lastScore = 0;
    bool newHigh = false;
    bool attract = true; // AI plays until the human presses a key
};

void quad(Game& g, float x, float y, float w, float h, render::Color c) {
    render::SpriteDesc s;
    s.x = x;
    s.y = y;
    s.width = w;
    s.height = h;
    s.color = c;
    g.r->drawSprite(g.white, s);
}
void disc(Game& g, render::TextureHandle tex, float cx, float cy, float rad, render::Color c) {
    render::SpriteDesc s;
    s.x = cx - rad;
    s.y = cy - rad;
    s.width = rad * 2.0f;
    s.height = rad * 2.0f;
    s.color = c;
    g.r->drawSprite(tex, s);
}

// ---- Events fanned out through the bus -------------------------------------------------------
struct CatchEvent {
    float x, y;
};
struct HitEvent {
    float x, y;
};

// ---- Game-over screen ------------------------------------------------------------------------
class GameOverScene : public core::Scene {
public:
    explicit GameOverScene(Game& g) : m_g(g) {}
    void update(float dt) override {
        m_t += dt;
        const bool key = m_g.input->keyPressed(SDL_SCANCODE_SPACE) ||
                         m_g.input->keyPressed(SDL_SCANCODE_RETURN);
        if (m_t > 3.5f || key) {
            m_g.stack->clear();
            // Rebuilt by main via a fresh Menu (see main loop's empty-stack guard).
        }
    }
    void render() override {
        quad(m_g, 0, 0, m_g.sw, m_g.sh, render::Color{0.09f, 0.07f, 0.10f, 1.0f});
        m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.28f, "GAME OVER",
                                   render::Color{1.0f, 0.5f, 0.5f, 1}, 1.4f);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "score  %d", m_g.lastScore);
        m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.46f, buf,
                                   render::Color{1, 1, 1, 1}, 0.8f);
        std::snprintf(buf, sizeof(buf), "best   %d", m_g.highScore);
        m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.54f, buf,
                                   render::Color{0.9f, 0.85f, 0.5f, 1}, 0.7f);
        if (m_g.newHigh) {
            m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.63f, "NEW HIGH SCORE!",
                                       render::Color{0.5f, 1.0f, 0.6f, 1}, 0.7f);
        }
    }

private:
    Game& m_g;
    float m_t = 0.0f;
};

// ---- The actual gameplay ---------------------------------------------------------------------
class PlayScene : public core::Scene {
public:
    explicit PlayScene(Game& g) : m_g(g), m_particles(1024) {
        m_paddleX = m_g.sw * 0.5f;
        // Wire the bus: one catch/hit event fans out to score, particles, and shake independently.
        m_bus.subscribe<CatchEvent>([this](const CatchEvent& e) {
            m_score += 10;
            m_shake.addTrauma(0.12f);
            fx::BurstDesc b;
            b.count = 24;
            b.x = e.x;
            b.y = e.y;
            b.speedMin = 60;
            b.speedMax = 220;
            b.lifeMin = 0.3f;
            b.lifeMax = 0.7f;
            b.sizeStart = 10;
            b.sizeEnd = 1;
            b.colorStart = render::Color{1.0f, 0.85f, 0.35f, 1};
            b.colorEnd = render::Color{1.0f, 0.6f, 0.2f, 0};
            b.gravity = 300;
            m_particles.emit(b);
        });
        m_bus.subscribe<HitEvent>([this](const HitEvent& e) {
            if (m_lives > 0) --m_lives;
            m_shake.addTrauma(0.7f);
            fx::BurstDesc b;
            b.count = 30;
            b.x = e.x;
            b.y = e.y;
            b.speedMin = 80;
            b.speedMax = 300;
            b.lifeMin = 0.3f;
            b.lifeMax = 0.8f;
            b.sizeStart = 12;
            b.sizeEnd = 1;
            b.colorStart = render::Color{1.0f, 0.4f, 0.35f, 1};
            b.colorEnd = render::Color{0.8f, 0.2f, 0.2f, 0};
            b.gravity = 120;
            m_particles.emit(b);
        });
    }

    void update(float dt) override {
        m_time += dt;
        // Spawn items from the top on a timer.
        m_spawnT += dt;
        if (m_spawnT > 0.55f) {
            m_spawnT = 0.0f;
            Item it;
            it.x = m_rng.range(60.0f, m_g.sw - 60.0f);
            it.y = 130.0f;
            it.vy = m_rng.range(190.0f, 300.0f);
            it.hazard = m_rng.unit() < 0.28f;
            m_items.push_back(it);
        }

        // Paddle: human input, or attract-mode AI tracking the lowest catchable coin / dodging.
        const float speed = 620.0f;
        bool human = false;
        if (m_g.input->keyDown(SDL_SCANCODE_LEFT)) {
            m_paddleX -= speed * dt;
            human = true;
        }
        if (m_g.input->keyDown(SDL_SCANCODE_RIGHT)) {
            m_paddleX += speed * dt;
            human = true;
        }
        if (human) {
            m_g.attract = false;
        }
        if (m_g.attract) {
            float targetX = m_paddleX;
            float bestY = -1.0f;
            for (const Item& it : m_items) {
                if (!it.hazard && it.y > bestY && it.y < m_paddleY) {
                    bestY = it.y;
                    targetX = it.x;
                }
            }
            // Nudge away if a hazard is right above the paddle.
            for (const Item& it : m_items) {
                if (it.hazard && std::fabs(it.x - m_paddleX) < 70.0f && it.y < m_paddleY) {
                    targetX = m_paddleX + (it.x < m_paddleX ? 90.0f : -90.0f);
                }
            }
            const float d = targetX - m_paddleX;
            m_paddleX += (d < 0 ? -1 : 1) * std::fmin(std::fabs(d), speed * dt);
        }
        m_paddleX = std::fmax(m_halfW, std::fmin(m_g.sw - m_halfW, m_paddleX));

        // Move items; catch or miss at the paddle line.
        for (Item& it : m_items) {
            it.y += it.vy * dt;
            if (!it.dead && it.y > m_paddleY - 18.0f && it.y < m_paddleY + 26.0f &&
                std::fabs(it.x - m_paddleX) < m_halfW + 18.0f) {
                it.dead = true;
                if (it.hazard) {
                    m_bus.emit(HitEvent{it.x, it.y});
                } else {
                    m_bus.emit(CatchEvent{it.x, it.y});
                }
            }
            if (it.y > m_g.sh + 40.0f) {
                it.dead = true;
            }
        }
        m_items.erase(std::remove_if(m_items.begin(), m_items.end(),
                                     [](const Item& it) { return it.dead; }),
                      m_items.end());

        m_particles.update(dt);
        m_shake.update(dt);

        if (m_lives <= 0) {
            m_g.lastScore = m_score;
            m_g.newHigh = m_score > m_g.highScore;
            if (m_g.newHigh) {
                m_g.highScore = m_score;
                m_g.store->set("highscore", m_g.highScore);
                m_g.store->save();
            }
            m_g.stack->replace(std::make_unique<GameOverScene>(m_g));
        }
    }

    void render() override {
        const math::vec3 sh = m_shake.offset(m_time, 16.0f);
        const float ox = sh.x, oy = sh.y;
        quad(m_g, 0, 0, m_g.sw, m_g.sh, render::Color{0.10f, 0.12f, 0.16f, 1.0f});

        for (const Item& it : m_items) {
            const render::Color c = it.hazard ? render::Color{0.95f, 0.4f, 0.4f, 1}
                                              : render::Color{1.0f, 0.82f, 0.35f, 1};
            disc(m_g, m_g.disc, it.x + ox, it.y + oy, it.hazard ? 16.0f : 14.0f, c);
        }
        m_particles.draw(*m_g.r, m_g.dot);

        // Paddle.
        quad(m_g, m_paddleX - m_halfW + ox, m_paddleY - 10.0f + oy, m_halfW * 2.0f, 20.0f,
             render::Color{0.5f, 0.8f, 1.0f, 1});

        // HUD.
        char buf[64];
        std::snprintf(buf, sizeof(buf), "SCORE  %d", m_score);
        m_g.font->drawText(*m_g.r, 16.0f, 46.0f, buf, render::Color{1, 1, 1, 1}, 0.6f);
        std::snprintf(buf, sizeof(buf), "LIVES  %d", m_lives);
        m_g.font->drawText(*m_g.r, 16.0f, 80.0f, buf, render::Color{1.0f, 0.6f, 0.6f, 1}, 0.55f);
        std::snprintf(buf, sizeof(buf), "BEST  %d", m_g.highScore);
        m_g.font->drawText(*m_g.r, m_g.sw - 200.0f, 46.0f, buf, render::Color{0.9f, 0.85f, 0.5f, 1},
                           0.55f);
        if (m_g.attract) {
            m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh - 44.0f,
                                       "ATTRACT MODE  -  arrow keys to play",
                                       render::Color{0.6f, 0.7f, 0.85f, 1}, 0.45f);
        }
    }

private:
    struct Item {
        float x = 0, y = 0, vy = 0;
        bool hazard = false;
        bool dead = false;
    };

    Game& m_g;
    core::EventBus m_bus;
    fx::ParticleSystem m_particles;
    game::Shake m_shake;
    Rng m_rng;
    std::vector<Item> m_items;
    float m_paddleX = 0.0f;
    const float m_paddleY = 620.0f;
    const float m_halfW = 62.0f;
    int m_score = 0;
    int m_lives = 3;
    float m_spawnT = 0.0f;
    float m_time = 0.0f;
};

// ---- Title screen ----------------------------------------------------------------------------
class MenuScene : public core::Scene {
public:
    explicit MenuScene(Game& g) : m_g(g) {}
    void update(float dt) override {
        m_t += dt;
        const bool key = m_g.input->keyPressed(SDL_SCANCODE_SPACE) ||
                         m_g.input->keyPressed(SDL_SCANCODE_RETURN) ||
                         m_g.input->keyPressed(SDL_SCANCODE_LEFT) ||
                         m_g.input->keyPressed(SDL_SCANCODE_RIGHT);
        if (m_t > 1.5f || key) {
            m_g.stack->replace(std::make_unique<PlayScene>(m_g));
        }
    }
    void render() override {
        quad(m_g, 0, 0, m_g.sw, m_g.sh, render::Color{0.10f, 0.12f, 0.18f, 1.0f});
        m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.30f, "CATCHER",
                                   render::Color{1.0f, 0.85f, 0.4f, 1}, 1.6f);
        m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.30f + 74.0f,
                                   "catch the gold, dodge the red",
                                   render::Color{0.7f, 0.8f, 1.0f, 1}, 0.55f);
        const float pulse = 0.5f + 0.5f * std::sin(m_t * 4.0f);
        m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.6f, "PRESS SPACE",
                                   render::Color{1, 1, 1, 0.4f + 0.6f * pulse}, 0.7f);
        char buf[48];
        std::snprintf(buf, sizeof(buf), "best  %d", m_g.highScore);
        m_g.font->drawTextCentered(*m_g.r, m_g.sw * 0.5f, m_g.sh * 0.72f, buf,
                                   render::Color{0.9f, 0.85f, 0.5f, 1}, 0.5f);
    }

private:
    Game& m_g;
    float m_t = 0.0f;
};

render::TextureHandle softDisc(render::Renderer& r, int size, bool solid) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            float a;
            if (solid) {
                a = d >= 1.0f ? 0.0f : (d > 0.88f ? (1.0f - (d - 0.88f) / 0.12f) : 1.0f);
            } else {
                a = d >= 1.0f ? 0.0f : (1.0f - d) * (1.0f - d);
            }
            const float shade = solid ? 1.0f - 0.25f * d : 1.0f;
            const size_t i = (static_cast<size_t>(y) * static_cast<size_t>(size) +
                              static_cast<size_t>(x)) * 4;
            px[i] = px[i + 1] = px[i + 2] = static_cast<uint8_t>(255.0f * shade);
            px[i + 3] = static_cast<uint8_t>(255.0f * a);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("CATCHER starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Catcher";
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

    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    core::KeyValueStore store;
    store.load(platform::prefPath("MazEngine", "catcher", "save.ini"));

    uint32_t bw = 0, bh = 0;
    window.drawableSize(bw, bh);

    Game g;
    g.r = renderer.get();
    g.font = &font;
    g.input = &input;
    g.white = white;
    g.disc = softDisc(*renderer, 48, true);
    g.dot = softDisc(*renderer, 32, false);
    g.store = &store;
    g.sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
    g.sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);
    g.highScore = store.getInt("highscore", 0);

    core::SceneStack stack;
    g.stack = &stack;
    stack.push(std::make_unique<MenuScene>(g));

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            stack.update(static_cast<float>(clock.fixedDelta()));
            if (stack.empty()) { // game-over cleared the stack -> back to the title
                g.attract = true;
                stack.push(std::make_unique<MenuScene>(g));
            }
        }

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);
            stack.render();
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  CATCHER",
                          render::Color{1, 1, 1, 1}, 0.65f);
            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("CATCHER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
