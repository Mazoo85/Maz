// Maz Engine — "SCENES" (scene-stack / game-state demo)
// A tiny app shell built on maz::core::SceneStack: a Menu scene replaces itself with a Game scene,
// which pushes a transparent Pause overlay (the game freezes and still shows through), then pops it
// to resume. The stack drives the whole flow — push/replace/pop with enter/pause/resume/exit — and
// the HUD prints the live stack. Transitions run on timers so the golden is deterministic. This is
// the application-framework layer that turns single-screen demos into an actual game shell.
// Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace maz;

namespace {

struct Ctx {
    render::Renderer* r = nullptr;
    ui::Font* font = nullptr;
    render::TextureHandle white = render::kInvalidTexture;
    render::TextureHandle ball = render::kInvalidTexture;
    core::SceneStack* stack = nullptr;
    float sw = 1280.0f, sh = 720.0f;
    const char* topName = "";
    int stackDepth = 0;
};

void quad(Ctx& c, float x, float y, float w, float h, render::Color col) {
    render::SpriteDesc s;
    s.x = x;
    s.y = y;
    s.width = w;
    s.height = h;
    s.color = col;
    c.r->drawSprite(c.white, s);
}

// --- Pause overlay: transparent (game shows through) and modal (game update frozen). ------------
class PauseScene : public core::Scene {
public:
    explicit PauseScene(Ctx& c) : m_c(c) {}
    void onEnter() override { m_c.topName = "Pause"; }
    void onResume() override { m_c.topName = "Pause"; }
    bool blocksUpdate() const override { return true; }  // freeze the game beneath
    bool blocksRender() const override { return false; } // draw the game beneath us

    void update(float dt) override {
        m_t += dt;
        if (m_t > 2.0f) {
            m_c.stack->pop(); // resume the game (deferred, applied after update)
        }
    }
    void render() override {
        quad(m_c, 0, 0, m_c.sw, m_c.sh, render::Color{0.05f, 0.06f, 0.10f, 0.6f}); // dim veil
        const float pw = 360.0f, ph = 160.0f;
        const float px = m_c.sw * 0.5f - pw * 0.5f, py = m_c.sh * 0.5f - ph * 0.5f;
        quad(m_c, px, py, pw, ph, render::Color{0.16f, 0.18f, 0.26f, 0.95f});
        m_c.font->drawTextCentered(*m_c.r, m_c.sw * 0.5f, py + 34.0f, "PAUSED",
                                   render::Color{1, 1, 1, 1}, 0.9f);
        m_c.font->drawTextCentered(*m_c.r, m_c.sw * 0.5f, py + 96.0f, "resuming...",
                                   render::Color{0.7f, 0.8f, 0.95f, 1}, 0.5f);
    }

private:
    Ctx& m_c;
    float m_t = 0.0f;
};

// --- Game: a few bouncing balls + a HUD; pushes the pause overlay after a while. ----------------
class GameScene : public core::Scene {
public:
    explicit GameScene(Ctx& c) : m_c(c) {
        m_world.gravity = math::vec2(0.0f, 1400.0f);
        m_world.bounds = game::Bounds2D{40.0f, 120.0f, m_c.sw - 40.0f, m_c.sh - 40.0f};
        m_world.hasBounds = true;
        const render::Color pal[5] = {{0.9f, 0.5f, 0.45f, 1}, {0.5f, 0.85f, 0.55f, 1},
                                      {0.5f, 0.65f, 0.9f, 1},  {0.9f, 0.8f, 0.45f, 1},
                                      {0.8f, 0.55f, 0.9f, 1}};
        for (int i = 0; i < 10; ++i) {
            game::Body2D b;
            b.radius = 24.0f + static_cast<float>(i % 3) * 8.0f;
            b.pos = math::vec2(140.0f + static_cast<float>(i) * (m_c.sw - 280.0f) / 9.0f, 180.0f);
            b.vel = math::vec2(static_cast<float>((i % 3) - 1) * 60.0f, 0.0f);
            b.restitution = 0.55f;
            b.invMass = 1.0f / (b.radius * b.radius * 0.01f);
            m_world.add(b);
            m_colors.push_back(pal[i % 5]);
        }
    }
    void onEnter() override { m_c.topName = "Game"; }
    void onResume() override { m_c.topName = "Game"; }

    void update(float dt) override {
        m_world.step(dt, 6);
        m_t += dt;
        if (!m_paused && m_t > 3.0f) { // one auto-pause to show the overlay
            m_paused = true;
            m_c.stack->push(std::make_unique<PauseScene>(m_c));
        }
    }
    void render() override {
        for (size_t i = 0; i < m_world.bodies.size(); ++i) {
            const game::Body2D& b = m_world.bodies[i];
            render::SpriteDesc s;
            s.x = b.pos.x - b.radius;
            s.y = b.pos.y - b.radius;
            s.width = b.radius * 2.0f;
            s.height = b.radius * 2.0f;
            s.color = m_colors[i];
            m_c.r->drawSprite(m_c.ball, s);
        }
        m_c.font->drawText(*m_c.r, 16.0f, 46.0f, "PLAYING  -  bouncing physics scene",
                           render::Color{0.8f, 0.95f, 0.85f, 1}, 0.5f);
    }

private:
    Ctx& m_c;
    game::PhysicsWorld2D m_world;
    std::vector<render::Color> m_colors;
    float m_t = 0.0f;
    bool m_paused = false;
};

// --- Menu: title + a highlighted PLAY button; replaces itself with the game after a beat. -------
class MenuScene : public core::Scene {
public:
    explicit MenuScene(Ctx& c) : m_c(c) {}
    void onEnter() override { m_c.topName = "Menu"; }

    void update(float dt) override {
        m_t += dt;
        if (m_t > 2.0f) {
            m_c.stack->replace(std::make_unique<GameScene>(m_c));
        }
    }
    void render() override {
        quad(m_c, 0, 0, m_c.sw, m_c.sh, render::Color{0.10f, 0.12f, 0.18f, 1.0f});
        m_c.font->drawTextCentered(*m_c.r, m_c.sw * 0.5f, m_c.sh * 0.34f, "MAZ ENGINE",
                                   render::Color{1, 1, 1, 1}, 1.3f);
        m_c.font->drawTextCentered(*m_c.r, m_c.sw * 0.5f, m_c.sh * 0.34f + 60.0f,
                                   "SCENE-STACK APP SHELL", render::Color{0.6f, 0.75f, 1.0f, 1}, 0.55f);
        // A PLAY button, highlighted (as if hovered) — the timer starts the game.
        const float bw = 260.0f, bh = 56.0f;
        const float bx = m_c.sw * 0.5f - bw * 0.5f, by = m_c.sh * 0.58f;
        const float pulse = 0.5f + 0.5f * std::sin(m_t * 4.0f);
        quad(m_c, bx, by, bw, bh, render::Color{0.30f + 0.15f * pulse, 0.45f, 0.7f, 1.0f});
        m_c.font->drawTextCentered(*m_c.r, m_c.sw * 0.5f, by + 12.0f, "PLAY",
                                   render::Color{1, 1, 1, 1}, 0.7f);
    }

private:
    Ctx& m_c;
    float m_t = 0.0f;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SCENES (scene-stack demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Scene Stack";
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
    // Soft ball texture.
    render::TextureHandle ballTex;
    {
        const int sz = 64;
        std::vector<uint8_t> px(static_cast<size_t>(sz) * sz * 4, 0);
        const float c = (sz - 1) * 0.5f;
        for (int y = 0; y < sz; ++y)
            for (int x = 0; x < sz; ++x) {
                const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
                const float d = std::sqrt(dx * dx + dy * dy);
                const float a = d >= 1.0f ? 0.0f : (d > 0.9f ? (1.0f - (d - 0.9f) / 0.1f) : 1.0f);
                const float shade = 1.0f - 0.3f * d;
                const size_t i = (static_cast<size_t>(y) * sz + static_cast<size_t>(x)) * 4;
                px[i] = px[i + 1] = px[i + 2] = static_cast<uint8_t>(255.0f * shade);
                px[i + 3] = static_cast<uint8_t>(255.0f * a);
            }
        ballTex = renderer->createTexture(sz, sz, px.data());
    }

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    uint32_t bw = 0, bh = 0;
    window.drawableSize(bw, bh);

    Ctx ctx;
    ctx.r = renderer.get();
    ctx.font = &font;
    ctx.white = white;
    ctx.ball = ballTex;
    ctx.sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
    ctx.sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

    core::SceneStack stack;
    ctx.stack = &stack;
    stack.push(std::make_unique<MenuScene>(ctx));

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            stack.update(static_cast<float>(clock.fixedDelta()));
        }
        ctx.stackDepth = static_cast<int>(stack.size());

        renderer->setClearColor(render::Color{0.06f, 0.07f, 0.10f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            stack.render();

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SCENE STACK",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "stack depth %d   top: %s", ctx.stackDepth, ctx.topName);
            font.drawText(*renderer, ctx.sw - 340.0f, 12.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1},
                          0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SCENES shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
