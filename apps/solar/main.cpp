// Maz Engine — "SOLAR" (2D transform hierarchy demo)
// A solar system built entirely from a scene::TransformGraph: the sun is the root, each planet hangs
// off an orbit pivot that spins around the sun, and each moon hangs off a pivot that spins around its
// planet. Nothing computes an orbit by hand — the app only sets each pivot's LOCAL rotation to
// speed*t; update() propagates those through the tree so planets sweep around the sun and moons sweep
// around planets automatically (a moon rides its planet's motion for free, which is the whole point of
// a hierarchy). Rotations are pure functions of the fixed-step clock, so the render is golden-stable.
// Run --headless / --frames N for CI.

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

render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.85f ? (1.0f - d) / 0.15f : 1.0f);
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

render::TextureHandle ringTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d > 1.0f ? 0.0f : std::exp(-((d - 0.97f) * (d - 0.97f)) * 900.0f);
            const size_t i =
                (static_cast<size_t>(y) * static_cast<size_t>(size) + static_cast<size_t>(x)) * 4;
            px[i] = 255;
            px[i + 1] = 255;
            px[i + 2] = 255;
            px[i + 3] = static_cast<uint8_t>(a * 255.0f);
        }
    }
    return r.createTexture(static_cast<uint32_t>(size), static_cast<uint32_t>(size), px.data());
}

struct Body {
    scene::TransformGraph::Node pivot; // orbit pivot (we set its rotation each frame)
    scene::TransformGraph::Node node;  // the body itself (child of pivot at (radius,0))
    float orbitSpeed;
    float radius;
    float drawSize;
    render::Color color;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SOLAR (transform hierarchy demo) starting");

    // --- Build the hierarchy once ----------------------------------------------------------------
    scene::TransformGraph graph;
    const scene::TransformGraph::Node sun =
        graph.create(); // root; positioned at screen center each frame

    struct PlanetDef {
        float radius, speed, size;
        render::Color color;
        float moonRadius, moonSpeed, moonSize;
    };
    const PlanetDef defs[] = {
        {120.0f, 0.90f, 26.0f, {0.55f, 0.75f, 1.00f, 1}, 40.0f, 3.0f, 10.0f},
        {200.0f, 0.60f, 34.0f, {0.60f, 0.95f, 0.65f, 1}, 52.0f, 2.2f, 12.0f},
        {285.0f, 0.42f, 30.0f, {1.00f, 0.70f, 0.45f, 1}, 46.0f, 2.6f, 11.0f},
        {360.0f, 0.30f, 40.0f, {0.85f, 0.55f, 1.00f, 1}, 66.0f, 1.8f, 14.0f},
    };

    std::vector<Body> planets, moons;
    for (const PlanetDef& d : defs) {
        Body p;
        p.pivot = graph.create(sun); // spins around the sun
        p.node = graph.create(p.pivot, scene::Transform2D{{d.radius, 0.0f}, 0.0f, {1, 1}});
        p.orbitSpeed = d.speed;
        p.radius = d.radius;
        p.drawSize = d.size;
        p.color = d.color;
        planets.push_back(p);

        Body m;
        m.pivot = graph.create(p.node); // spins around THIS planet (rides the planet's motion)
        m.node = graph.create(m.pivot, scene::Transform2D{{d.moonRadius, 0.0f}, 0.0f, {1, 1}});
        m.orbitSpeed = d.moonSpeed;
        m.radius = d.moonRadius;
        m.drawSize = d.moonSize;
        m.color = render::Color{0.8f, 0.82f, 0.88f, 1.0f};
        moons.push_back(m);
    }

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Transform Hierarchy";
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

    render::TextureHandle disc = discTex(*renderer, 64);
    render::TextureHandle ring = ringTex(*renderer, 256);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    float t = 0.0f;
    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        uint32_t bw = 0, bh = 0;
        window.drawableSize(bw, bh);
        const float sw = bw > 0 ? static_cast<float>(bw) : static_cast<float>(cfg.width);
        const float sh = bh > 0 ? static_cast<float>(bh) : static_cast<float>(cfg.height);

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            t += static_cast<float>(clock.fixedDelta());
        }

        // Anchor the sun at screen center; set each pivot's LOCAL rotation, then propagate.
        graph.local(sun).position = {sw * 0.5f, sh * 0.5f};
        for (const Body& p : planets) graph.local(p.pivot).rotation = t * p.orbitSpeed;
        for (const Body& m : moons) graph.local(m.pivot).rotation = t * m.orbitSpeed;
        graph.update();

        renderer->setClearColor(render::Color{0.04f, 0.05f, 0.09f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const math::vec2 center = graph.worldPosition(sun);

            // Faint orbit rings (one per planet radius).
            for (const Body& p : planets) {
                const float sz = p.radius * 2.0f;
                render::SpriteDesc d;
                d.x = center.x - sz * 0.5f;
                d.y = center.y - sz * 0.5f;
                d.width = sz;
                d.height = sz;
                d.color = render::Color{0.35f, 0.4f, 0.55f, 0.35f};
                renderer->drawSprite(ring, d);
            }

            // Sun.
            {
                const float sz = 64.0f;
                render::SpriteDesc d;
                d.x = center.x - sz * 0.5f;
                d.y = center.y - sz * 0.5f;
                d.width = sz;
                d.height = sz;
                d.color = render::Color{1.0f, 0.86f, 0.35f, 1.0f};
                renderer->drawSprite(disc, d);
            }

            auto drawBody = [&](const Body& b) {
                const math::vec2 wp = graph.worldPosition(b.node);
                render::SpriteDesc d;
                d.x = wp.x - b.drawSize * 0.5f;
                d.y = wp.y - b.drawSize * 0.5f;
                d.width = b.drawSize;
                d.height = b.drawSize;
                d.color = b.color;
                renderer->drawSprite(disc, d);
            };
            for (const Body& p : planets) drawBody(p);
            for (const Body& m : moons) drawBody(m);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  TRANSFORM HIERARCHY",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "sun -> %zu planets -> moons  (%zu nodes; parents spin, children follow)",
                          planets.size(), graph.size());
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.46f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SOLAR shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
