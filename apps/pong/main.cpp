// Maz Engine — "PONG" — a complete, classic two-paddle game built from the engine's 2D primitives:
// the sprite/polygon renderer, pixel-space camera, font HUD, and the fixed-timestep loop. Left
// paddle is the player (W/S or Up/Down); the right paddle is a tracking AI; the ball bounces off
// the walls and paddles, speeds up on each hit, and scores when it passes a paddle. --demo makes
// BOTH paddles AI (deterministic) for offscreen capture; --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>

using namespace maz;

namespace {

void quad(render::Renderer& r, float x, float y, float w, float h, render::Color c) {
    const render::Point2 p[4] = {{x, y}, {x + w, y}, {x + w, y + h}, {x, y + h}};
    r.drawConvexPolygon(p, 4, c);
}

// Move a paddle's center toward a target y at a capped speed (the AI + smoothing).
float trackTo(float cur, float target, float maxStep) {
    const float d = target - cur;
    if (d > maxStep)
        return cur + maxStep;
    if (d < -maxStep)
        return cur - maxStep;
    return target;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    const bool autopilot = cfg.demo;
    MAZ_LOG_INFO("PONG headless=%d frames=%d autopilot=%d", cfg.headless, cfg.frames, autopilot);

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — PONG";
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
        font.load(*renderer, fontPath.c_str(), 64.0f);
    }

    // Court dimensions in pixels (logical; the pixel-space camera matches the window).
    const float courtW = static_cast<float>(cfg.width);
    const float courtH = static_cast<float>(cfg.height);
    const float padW = 16.0f, padH = 110.0f, padMargin = 40.0f, padSpeed = 7.0f;
    const float ballSize = 16.0f;

    float leftY = courtH * 0.5f; // paddle centers
    float rightY = courtH * 0.5f;
    float ballX = courtW * 0.5f, ballY = courtH * 0.5f;
    float ballVX = 5.5f, ballVY = 3.0f; // deterministic serve
    int scoreL = 0, scoreR = 0;

    auto resetBall = [&](float dir) {
        ballX = courtW * 0.5f;
        ballY = courtH * 0.5f;
        ballVX = 5.5f * dir;
        ballVY = 3.0f;
    };

    int rendered = 0;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            // Left paddle: player input, or AI in autopilot.
            if (autopilot) {
                leftY = trackTo(leftY, ballY, padSpeed * 0.9f);
            } else {
                if (input.keyDown(SDL_SCANCODE_W) || input.keyDown(SDL_SCANCODE_UP))
                    leftY -= padSpeed;
                if (input.keyDown(SDL_SCANCODE_S) || input.keyDown(SDL_SCANCODE_DOWN))
                    leftY += padSpeed;
            }
            // Right paddle: tracking AI (slightly slower so it's beatable).
            rightY = trackTo(rightY, ballY, padSpeed * 0.82f);

            // Clamp paddles to the court.
            const float half = padH * 0.5f;
            if (leftY < half)
                leftY = half;
            if (leftY > courtH - half)
                leftY = courtH - half;
            if (rightY < half)
                rightY = half;
            if (rightY > courtH - half)
                rightY = courtH - half;

            // Advance the ball.
            ballX += ballVX;
            ballY += ballVY;

            // Bounce off top/bottom walls.
            if (ballY < ballSize * 0.5f) {
                ballY = ballSize * 0.5f;
                ballVY = -ballVY;
            }
            if (ballY > courtH - ballSize * 0.5f) {
                ballY = courtH - ballSize * 0.5f;
                ballVY = -ballVY;
            }

            // Paddle collisions (AABB overlap on the ball's leading edge), speeding up on each hit.
            const float lpx = padMargin, rpx = courtW - padMargin - padW;
            const float b = ballSize * 0.5f;
            if (ballVX < 0 && ballX - b < lpx + padW && ballX - b > lpx && ballY > leftY - half &&
                ballY < leftY + half) {
                ballVX = -ballVX * 1.04f;
                ballVY += (ballY - leftY) * 0.03f; // english off the paddle
                ballX = lpx + padW + b;
            }
            if (ballVX > 0 && ballX + b > rpx && ballX + b < rpx + padW && ballY > rightY - half &&
                ballY < rightY + half) {
                ballVX = -ballVX * 1.04f;
                ballVY += (ballY - rightY) * 0.03f;
                ballX = rpx - b;
            }

            // Scoring: ball past a paddle.
            if (ballX < -ballSize) {
                ++scoreR;
                resetBall(1.0f);
            }
            if (ballX > courtW + ballSize) {
                ++scoreL;
                resetBall(-1.0f);
            }
        }

        renderer->setClearColor(render::Color{0.04f, 0.05f, 0.07f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const render::Color kWhite{0.92f, 0.94f, 1.0f, 1.0f};

            // Center net (dashed).
            for (float y = 8.0f; y < courtH; y += 34.0f) {
                quad(*renderer, courtW * 0.5f - 3.0f, y, 6.0f, 20.0f,
                     render::Color{0.25f, 0.28f, 0.34f, 1.0f});
            }

            // Paddles + ball.
            quad(*renderer, padMargin, leftY - padH * 0.5f, padW, padH, kWhite);
            quad(*renderer, courtW - padMargin - padW, rightY - padH * 0.5f, padW, padH, kWhite);
            quad(*renderer, ballX - ballSize * 0.5f, ballY - ballSize * 0.5f, ballSize, ballSize,
                 render::Color{1.0f, 0.85f, 0.35f, 1.0f});

            // Scores.
            char ls[16], rs[16];
            std::snprintf(ls, sizeof(ls), "%d", scoreL);
            std::snprintf(rs, sizeof(rs), "%d", scoreR);
            font.drawText(*renderer, courtW * 0.5f - 120.0f, 24.0f, ls, kWhite, 1.0f);
            font.drawText(*renderer, courtW * 0.5f + 80.0f, 24.0f, rs, kWhite, 1.0f);

            font.drawText(*renderer, 16.0f, courtH - 34.0f,
                          autopilot ? "PONG  -  AUTOPILOT" : "PONG  -  W/S OR ARROWS   ESC QUIT",
                          render::Color{0.55f, 0.6f, 0.7f, 1.0f}, 0.4f);

            renderer->endFrame();
        }

        ++rendered;
        if (cfg.frames >= 0 && rendered >= cfg.frames) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PONG shutting down %d:%d after %d frames (renderer %s)", scoreL, scoreR, rendered,
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
