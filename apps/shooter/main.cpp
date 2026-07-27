// DEAD SECTOR — a top-down zombie shooter built on the Maz Engine.
//
// This is the app/presentation layer: it owns the window, the Vulkan renderer, and input, and it
// translates keyboard/mouse into a shooter::Input each fixed step, then draws the shooter::Sim
// state with the engine's 2D sprite path (Renderer::uploadTexture + drawSprite). All gameplay
// lives in Sim.cpp (engine-independent, unit-tested); all glyphs live in Font.hpp.
//
// Controls (desktop — the engine has no touch input):
//   WASD / arrows : move        mouse : aim
//   hold LMB (or SPACE) : fire  ESC : quit        when dead, SPACE / ENTER : retry
//
// Runs headless (--headless --frames N) for CI: with no GPU the renderer no-ops its draws but the
// simulation still advances, so the whole wiring is exercised without a display.

#include "Font.hpp"
#include "Sim.hpp"

#include "maz/Engine.hpp"
#include "maz/render/Renderer.hpp"

#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;
namespace font = shooter::font;

namespace {

render::Color rgb(int r, int g, int b, float a = 1.0f) {
    return {static_cast<float>(r) / 255.0f, static_cast<float>(g) / 255.0f,
            static_cast<float>(b) / 255.0f, a};
}

// A soft-edged white disc (RGBA). Tinted per-draw to become the player, zombies, particles, etc.
// White core with an alpha falloff at the rim so scaled-up circles stay smooth.
std::vector<uint8_t> makeDisc(uint32_t size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * size * 4, 0);
    const float c = static_cast<float>(size - 1) * 0.5f;
    const float rad = static_cast<float>(size) * 0.5f;
    for (uint32_t y = 0; y < size; ++y) {
        for (uint32_t x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / rad;
            const float dy = (static_cast<float>(y) - c) / rad;
            const float d = std::sqrt(dx * dx + dy * dy);
            float a = 1.0f;
            if (d > 1.0f) a = 0.0f;
            else if (d > 0.80f) a = 1.0f - (d - 0.80f) / 0.20f; // smooth rim
            const size_t i = (static_cast<size_t>(y) * size + x) * 4;
            px[i + 0] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return px;
}

// Bake the 5x7 font into a single-row RGBA atlas: N glyphs, each 5px wide, 7px tall.
// '#' cells are opaque white (tinted at draw time), everything else fully transparent.
std::vector<uint8_t> makeFontAtlas(uint32_t& atlasW, uint32_t& atlasH, size_t& glyphCount) {
    std::size_t n = 0;
    const font::Glyph* g = font::glyphs(n);
    glyphCount = n;
    atlasW = static_cast<uint32_t>(n) * static_cast<uint32_t>(font::kGlyphW);
    atlasH = static_cast<uint32_t>(font::kGlyphH);
    std::vector<uint8_t> px(static_cast<size_t>(atlasW) * atlasH * 4, 0);
    for (std::size_t gi = 0; gi < n; ++gi) {
        for (int ry = 0; ry < font::kGlyphH; ++ry) {
            for (int rx = 0; rx < font::kGlyphW; ++rx) {
                if (g[gi].rows[ry][rx] != '#') continue;
                const size_t ax = gi * static_cast<size_t>(font::kGlyphW) + static_cast<size_t>(rx);
                const size_t i = (static_cast<size_t>(ry) * atlasW + ax) * 4;
                px[i + 0] = 255; px[i + 1] = 255; px[i + 2] = 255; px[i + 3] = 255;
            }
        }
    }
    return px;
}

// Convert a zombie type into its body tint.
render::Color zombieTint(shooter::ZombieType t) {
    switch (t) {
        case shooter::ZombieType::Runner: return rgb(200, 209, 90);
        case shooter::ZombieType::Brute:  return rgb(138, 90, 74);
        case shooter::ZombieType::Walker:
        default:                          return rgb(111, 155, 90);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    cfg.title = "DEAD SECTOR";
    MAZ_LOG_INFO("DEAD SECTOR starting (headless=%d, frames=%d)", cfg.headless, cfg.frames);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = cfg.title;
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        MAZ_LOG_ERROR("window init failed");
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
#if defined(MAZ_DEBUG)
    rc.enableValidation = !cfg.headless;
#endif
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        MAZ_LOG_ERROR("renderer init failed");
        return 1;
    }

    // --- Textures (no-ops / -1 when headless; draws below just skip) ---
    const std::vector<uint8_t> disc = makeDisc(64);
    const int discTex = renderer->uploadTexture(disc.data(), 64, 64);
    const uint8_t solidPx[4] = {255, 255, 255, 255};
    const int solidTex = renderer->uploadTexture(solidPx, 1, 1);
    uint32_t atlasW = 0, atlasH = 0;
    size_t glyphCount = 0;
    const std::vector<uint8_t> atlas = makeFontAtlas(atlasW, atlasH, glyphCount);
    const int fontTex = renderer->uploadTexture(atlas.data(), atlasW, atlasH);

    shooter::Sim sim(0xC0FFEEu);
    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    // Presentation-only state (kept out of the deterministic sim).
    float camX = 0.0f, camY = 0.0f;
    float shake = 0.0f;
    float prevInvuln = 0.0f;

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (window.consumeResized()) {
            uint32_t rw = 0, rh = 0;
            window.drawableSize(rw, rh);
            renderer->onResize(rw, rh);
        }
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) window.requestClose();

        uint32_t dw = 0, dh = 0;
        window.drawableSize(dw, dh);
        const float fw = static_cast<float>(dw);
        const float fh = static_cast<float>(dh);

        // --- Build the sim input from keyboard/mouse ---
        shooter::Input in;
        float mx = 0.0f, my = 0.0f;
        if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP)) my -= 1.0f;
        if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN)) my += 1.0f;
        if (input.keyDown(SDL_SCANCODE_A) || input.keyDown(SDL_SCANCODE_LEFT)) mx -= 1.0f;
        if (input.keyDown(SDL_SCANCODE_D) || input.keyDown(SDL_SCANCODE_RIGHT)) mx += 1.0f;
        in.move = {mx, my};

        // Aim toward the mouse, measured from screen centre (where the player is drawn). Mouse
        // coords are logical window space, so use logical size here.
        const float lw = static_cast<float>(window.width());
        const float lh = static_cast<float>(window.height());
        in.aim = {input.mouseX() - lw * 0.5f, input.mouseY() - lh * 0.5f};
        in.aiming = true;
        in.firing = input.mouseDown(1) || input.keyDown(SDL_SCANCODE_SPACE);
        in.restart = input.keyPressed(SDL_SCANCODE_SPACE) || input.keyPressed(SDL_SCANCODE_RETURN);

        // --- Fixed-step simulation ---
        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            sim.update(dt, in);
            // Kick a little screen shake the moment the player takes a hit.
            if (sim.playerInvuln() > prevInvuln + 0.001f) shake = 12.0f;
            prevInvuln = sim.playerInvuln();
            if (shake > 0.0f) shake = std::fmax(0.0f, shake - dt * 40.0f);
        }

        // Camera eases toward the player; shake jitters the whole view.
        camX += (sim.playerPos().x - camX) * 0.12f;
        camY += (sim.playerPos().y - camY) * 0.12f;
        float shx = 0.0f, shy = 0.0f;
        if (shake > 0.0f) {
            shx = std::sin(static_cast<float>(rendered) * 12.9898f) * shake;
            shy = std::cos(static_cast<float>(rendered) * 7.233f) * shake;
        }
        const float originX = fw * 0.5f - camX + shx;
        const float originY = fh * 0.5f - camY + shy;

        // --- Draw helpers (capture the renderer + transform) ---
        auto worldSprite = [&](int tex, float wx, float wy, float w, float h, render::Color tint,
                               float rot) {
            render::Sprite s;
            s.w = w; s.h = h;
            s.x = originX + wx - w * 0.5f;
            s.y = originY + wy - h * 0.5f;
            s.rotation = rot;
            s.tint = tint;
            renderer->drawSprite(tex, s);
        };
        auto screenRect = [&](float x, float y, float w, float h, render::Color tint) {
            render::Sprite s;
            s.x = x; s.y = y; s.w = w; s.h = h; s.tint = tint;
            renderer->drawSprite(solidTex, s);
        };
        auto text = [&](const std::string& str, float x, float y, float scale, render::Color tint) {
            float cx = x;
            const float aw = static_cast<float>(atlasW);
            for (char ch : str) {
                const std::size_t gi = font::indexOf(ch);
                render::Sprite s;
                s.w = static_cast<float>(font::kGlyphW) * scale;
                s.h = static_cast<float>(font::kGlyphH) * scale;
                s.x = cx; s.y = y;
                s.u0 = static_cast<float>(gi * font::kGlyphW) / aw;
                s.u1 = static_cast<float>(gi * font::kGlyphW + font::kGlyphW) / aw;
                s.tint = tint;
                renderer->drawSprite(fontTex, s);
                cx += static_cast<float>(font::kGlyphW + 1) * scale;
            }
        };
        auto textWidth = [&](const std::string& str, float scale) {
            return static_cast<float>(str.size()) * static_cast<float>(font::kGlyphW + 1) * scale;
        };

        renderer->setClearColor(rgb(12, 17, 12));
        if (renderer->beginFrame()) {
            // Floor grid (screen-space vertical + horizontal lines aligned to the world).
            const float grid = 80.0f;
            const render::Color gridCol = rgb(70, 90, 70, 0.16f);
            const float sx0 = std::fmod(originX, grid);
            for (float gx = sx0; gx < fw; gx += grid) screenRect(gx, 0.0f, 1.5f, fh, gridCol);
            const float sy0 = std::fmod(originY, grid);
            for (float gy = sy0; gy < fh; gy += grid) screenRect(0.0f, gy, fw, 1.5f, gridCol);

            // World border.
            const float B = shooter::Sim::kWorld;
            const render::Color borderCol = rgb(120, 60, 60, 0.5f);
            worldSprite(solidTex, 0.0f, -B, B * 2.0f, 6.0f, borderCol, 0.0f);
            worldSprite(solidTex, 0.0f, B, B * 2.0f, 6.0f, borderCol, 0.0f);
            worldSprite(solidTex, -B, 0.0f, 6.0f, B * 2.0f, borderCol, 0.0f);
            worldSprite(solidTex, B, 0.0f, 6.0f, B * 2.0f, borderCol, 0.0f);

            // Pickups (green box + white cross).
            for (const shooter::Pickup& p : sim.pickups()) {
                const float bob = std::sin(p.bob) * 3.0f;
                const float alpha = p.life < 3.0f ? 0.45f + 0.55f * std::fabs(std::sin(p.life * 6.0f)) : 1.0f;
                worldSprite(solidTex, p.pos.x, p.pos.y + bob, 26.0f, 26.0f, rgb(14, 42, 14, alpha), 0.0f);
                worldSprite(solidTex, p.pos.x, p.pos.y + bob, 16.0f, 5.0f, rgb(89, 217, 90, alpha), 0.0f);
                worldSprite(solidTex, p.pos.x, p.pos.y + bob, 5.0f, 16.0f, rgb(89, 217, 90, alpha), 0.0f);
            }

            // Bullets (short streaks along their velocity).
            for (const shooter::Bullet& b : sim.bullets()) {
                const float rot = std::atan2(b.vel.y, b.vel.x);
                worldSprite(solidTex, b.pos.x, b.pos.y, 14.0f, 3.4f, rgb(255, 230, 128), rot);
            }

            // Zombies (disc body + tiny hp bar when hurt).
            for (const shooter::Zombie& z : sim.zombies()) {
                const render::Color tint = z.hitFlash > 0.0f ? rgb(255, 255, 255) : zombieTint(z.type);
                worldSprite(discTex, z.pos.x, z.pos.y, z.radius * 2.2f, z.radius * 2.2f, tint, 0.0f);
                if (z.hp < z.maxHp) {
                    const float w = z.radius * 2.0f;
                    const float frac = z.hp / z.maxHp;
                    worldSprite(solidTex, z.pos.x, z.pos.y - z.radius - 8.0f, w, 4.0f, rgb(0, 0, 0, 0.6f), 0.0f);
                    worldSprite(solidTex, z.pos.x - w * (1.0f - frac) * 0.5f, z.pos.y - z.radius - 8.0f,
                                w * frac, 4.0f, rgb(255, 85, 85), 0.0f);
                }
            }

            // Player: shadow, gun barrel, body, facing dot.
            const shooter::Vec2 pp = sim.playerPos();
            const float pr = sim.playerRadius();
            const float ang = sim.playerAngle();
            worldSprite(discTex, pp.x, pp.y + 4.0f, pr * 2.2f, pr * 1.5f, rgb(0, 0, 0, 0.35f), 0.0f);
            const float barrel = 22.0f;
            worldSprite(solidTex, pp.x + std::cos(ang) * (pr + barrel * 0.5f),
                        pp.y + std::sin(ang) * (pr + barrel * 0.5f), barrel, 7.0f, rgb(40, 40, 40), ang);
            const bool hurtBlink = sim.playerInvuln() > 0.0f &&
                                   (static_cast<int>(sim.playerInvuln() * 20.0f) % 2 == 0);
            worldSprite(discTex, pp.x, pp.y, pr * 2.0f, pr * 2.0f,
                        hurtBlink ? rgb(255, 128, 128) : rgb(74, 163, 214), 0.0f);
            worldSprite(discTex, pp.x + std::cos(ang) * pr * 0.45f, pp.y + std::sin(ang) * pr * 0.45f,
                        7.0f, 7.0f, rgb(234, 255, 255), 0.0f);

            // Particles (blood, muzzle sparks, pickup pops).
            for (const shooter::Particle& p : sim.particles()) {
                const float a = p.life / p.maxLife;
                worldSprite(discTex, p.pos.x, p.pos.y, p.radius * 2.0f, p.radius * 2.0f,
                            rgb(p.r, p.g, p.b, a < 0.0f ? 0.0f : (a > 1.0f ? 1.0f : a)), 0.0f);
            }

            // --- HUD ---
            // Health bar (top-left).
            const float hpFrac = sim.playerHp() / sim.playerMaxHp();
            screenRect(16.0f, 16.0f, 224.0f, 24.0f, rgb(0, 0, 0, 0.45f));
            screenRect(20.0f, 20.0f, 216.0f * (hpFrac < 0.0f ? 0.0f : hpFrac), 16.0f,
                       hpFrac > 0.4f ? rgb(89, 217, 90) : rgb(255, 85, 85));
            text("HP " + std::to_string(static_cast<int>(sim.playerHp())), 22.0f, 46.0f, 2.0f,
                 rgb(207, 238, 238));
            // Wave / enemies (top-left, below health).
            text("WAVE " + std::to_string(sim.wave()), 16.0f, 74.0f, 3.0f, rgb(89, 217, 90));
            text("ENEMIES " + std::to_string(sim.enemiesRemaining()), 16.0f, 102.0f, 2.0f,
                 rgb(207, 238, 238));
            // Score / kills (top-right).
            const std::string scoreStr = "SCORE " + std::to_string(sim.score());
            const std::string killStr = "KILLS " + std::to_string(sim.kills());
            text(scoreStr, fw - textWidth(scoreStr, 3.0f) - 16.0f, 16.0f, 3.0f, rgb(255, 255, 255));
            text(killStr, fw - textWidth(killStr, 2.0f) - 16.0f, 46.0f, 2.0f, rgb(207, 238, 238));

            // Hurt vignette (approximated with a translucent red overlay).
            if (sim.playerInvuln() > 0.0f) {
                const float a = sim.playerInvuln();
                screenRect(0.0f, 0.0f, fw, fh, rgb(160, 0, 0, (a > 0.6f ? 0.6f : a) * 0.5f));
            }

            // Death screen.
            if (sim.state() == shooter::GameState::Dead) {
                screenRect(0.0f, 0.0f, fw, fh, rgb(2, 4, 4, 0.72f));
                const std::string t1 = "YOU DIED";
                const std::string t2 = "WAVE " + std::to_string(sim.wave()) + "  SCORE " +
                                       std::to_string(sim.score());
                const std::string t3 = "PRESS SPACE TO RETRY";
                text(t1, (fw - textWidth(t1, 6.0f)) * 0.5f, fh * 0.34f, 6.0f, rgb(255, 85, 85));
                text(t2, (fw - textWidth(t2, 3.0f)) * 0.5f, fh * 0.50f, 3.0f, rgb(230, 240, 230));
                text(t3, (fw - textWidth(t3, 2.5f)) * 0.5f, fh * 0.60f, 2.5f, rgb(150, 210, 150));
            }

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            MAZ_LOG_INFO("reached frame cap (%d); exiting", cfg.frames);
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("DEAD SECTOR shutting down after %d frames (wave %d, score %d)", rendered,
                 sim.wave(), sim.score());
    renderer->shutdown();
    window.shutdown();
    return 0;
}
