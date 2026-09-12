// Maz Engine — "TWEEN" (easing showcase)
// One shared ping-pong Tween drives a column of markers, each rendered with a different easing curve
// (maz::anim). Because they share the same clock, the differences in motion — linear vs. accelerate,
// overshoot (Back), spring (Elastic), and bounce — are directly comparable as the markers slide
// left/right in lockstep. Also color-fades the floor tint with a second tween. Run --headless /
// --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz;

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("TWEEN (easing showcase) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Tweening";
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

    auto sphereData = render::shapes::makeSphere(0.6f, 16, 22, render::Color{1, 1, 1, 1});
    render::MeshHandle sphere = renderer->createMesh(
        sphereData.vertices.data(), static_cast<uint32_t>(sphereData.vertices.size()),
        sphereData.indices.data(), static_cast<uint32_t>(sphereData.indices.size()));
    auto railData = render::shapes::makeBox(1.0f, render::Color{0.25f, 0.27f, 0.32f, 1.0f});
    render::MeshHandle rail = renderer->createMesh(
        railData.vertices.data(), static_cast<uint32_t>(railData.vertices.size()),
        railData.indices.data(), static_cast<uint32_t>(railData.indices.size()));
    auto floorData = render::shapes::makePlane(20.0f, render::Color{1, 1, 1, 1});
    render::MeshHandle floor = renderer->createMesh(
        floorData.vertices.data(), static_cast<uint32_t>(floorData.vertices.size()),
        floorData.indices.data(), static_cast<uint32_t>(floorData.indices.size()));

    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);
    auto colorTex = [&](float r, float g, float b) {
        const uint8_t px[4] = {static_cast<uint8_t>(r * 255), static_cast<uint8_t>(g * 255),
                               static_cast<uint8_t>(b * 255), 255};
        return renderer->createTexture(1, 1, px);
    };

    // The curves on show, top to bottom, with a marker color each.
    struct Row {
        anim::Ease ease;
        const char* name;
        render::TextureHandle tex;
    };
    std::vector<Row> rows = {
        {anim::Ease::Linear, "linear", colorTex(0.7f, 0.7f, 0.7f)},
        {anim::Ease::QuadInOut, "quad", colorTex(0.4f, 0.7f, 1.0f)},
        {anim::Ease::CubicInOut, "cubic", colorTex(0.4f, 0.9f, 0.7f)},
        {anim::Ease::SineInOut, "sine", colorTex(0.9f, 0.85f, 0.3f)},
        {anim::Ease::ExpoOut, "expo", colorTex(1.0f, 0.6f, 0.3f)},
        {anim::Ease::BackOut, "back", colorTex(1.0f, 0.45f, 0.6f)},
        {anim::Ease::ElasticOut, "elastic", colorTex(0.8f, 0.4f, 1.0f)},
        {anim::Ease::BounceOut, "bounce", colorTex(0.5f, 1.0f, 0.5f)},
    };
    const float kMinX = -8.0f, kMaxX = 8.0f;
    const float rowGap = 1.7f;
    const float topZ = -static_cast<float>(rows.size() - 1) * rowGap * 0.5f;

    // Shared driver: a 2s ping-pong so every marker eases out to the right, then back.
    anim::Tween drive;
    drive.duration = 2.0f;
    drive.loop = anim::Loop::PingPong;
    // A slow color fade for the floor tint (independent tween).
    anim::Tween fade;
    fade.duration = 5.0f;
    fade.loop = anim::Loop::PingPong;
    fade.easing = anim::Ease::SineInOut;

    render::SceneLighting lighting;
    lighting.ambient[0] = lighting.ambient[1] = lighting.ambient[2] = 0.5f;
    renderer->setLighting(lighting);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            const float dt = static_cast<float>(clock.fixedDelta());
            drive.update(dt);
            fade.update(dt);
        }

        const glm::vec3 eye(0.0f, 12.0f, 15.0f);
        const glm::mat4 proj = math::perspective(glm::radians(52.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        const float rawProgress = drive.progress(); // shared raw cursor for all rows

        renderer->setClearColor(render::Color{0.10f, 0.11f, 0.15f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            // Floor whose tint slowly fades between two colors via the fade tween.
            const math::vec3 floorTint =
                fade.sample(math::vec3(0.28f, 0.30f, 0.36f), math::vec3(0.34f, 0.28f, 0.32f));
            glm::mat4 fm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.6f, 0.0f));
            render::Renderer::Material floorMat;
            floorMat.albedo = colorTex(floorTint.x, floorTint.y, floorTint.z);
            renderer->drawMeshMaterial(floor, glm::value_ptr(fm), floorMat);

            for (size_t i = 0; i < rows.size(); ++i) {
                const float z = topZ + static_cast<float>(i) * rowGap;
                // Rail spanning the travel range.
                glm::mat4 rm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.25f, z));
                rm = glm::scale(rm, glm::vec3((kMaxX - kMinX) + 1.2f, 0.12f, 0.12f));
                renderer->drawMesh(rail, glm::value_ptr(rm), white);

                // Marker position from this row's easing curve applied to the shared cursor.
                const float e = anim::ease(rows[i].ease, rawProgress);
                const float x = kMinX + (kMaxX - kMinX) * e;
                glm::mat4 mm = glm::translate(glm::mat4(1.0f), glm::vec3(x, 0.2f, z));
                renderer->drawMesh(sphere, glm::value_ptr(mm), rows[i].tex);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TWEENING / EASING",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f, "8 easing curves on one shared ping-pong tween",
                          render::Color{0.8f, 0.85f, 1.0f, 1}, 0.5f);
            // Label each row down the left edge.
            for (size_t i = 0; i < rows.size(); ++i) {
                const float y = 110.0f + static_cast<float>(i) * 30.0f;
                font.drawText(*renderer, 20.0f, y, rows[i].name, render::Color{0.75f, 0.8f, 0.9f, 1},
                              0.45f);
            }

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("TWEEN shutting down (renderer %s)",
                 renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
