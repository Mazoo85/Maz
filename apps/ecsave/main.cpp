// Maz Engine — "ECSAVE" (ECS scene save / load round-trip)
// Everything on screen is proof that a live entity world survives a JSON round-trip. At startup the
// app builds a source ecs::World (entities with Transform / Look / Spin components), serializes it to
// JSON with io::SceneSerializer, then deserializes that JSON into a SECOND, fresh world — and renders
// the second world. If the picture is right, save+load works. The document is also written to the save
// directory. Spin is a pure function of the fixed-step clock, so the render is golden-stable.
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

// --- Components ----------------------------------------------------------------------------------
struct Transform2D {
    float x, y; // fractional screen position (0..1)
};
struct Look {
    float r, g, b;
    float size;
    int shape; // 0 = disc, 1 = box
};
struct Spin {
    float rate; // radians / second
};

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.9f ? (1.0f - d) / 0.1f : 1.0f);
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

// Register the three component types with the serializer (name + to/from JSON).
void registerComponents(io::SceneSerializer& s) {
    s.component<Transform2D>(
        "Transform",
        [](const Transform2D& t) {
            io::JsonValue j;
            j.set("x", t.x);
            j.set("y", t.y);
            return j;
        },
        [](const io::JsonValue& j) {
            return Transform2D{j["x"].asFloat(0.5f), j["y"].asFloat(0.5f)};
        });
    s.component<Look>(
        "Look",
        [](const Look& l) {
            io::JsonValue j;
            j.set("r", l.r);
            j.set("g", l.g);
            j.set("b", l.b);
            j.set("size", l.size);
            j.set("shape", l.shape);
            return j;
        },
        [](const io::JsonValue& j) {
            return Look{j["r"].asFloat(1), j["g"].asFloat(1), j["b"].asFloat(1),
                        j["size"].asFloat(40), j["shape"].asInt(0)};
        });
    s.component<Spin>(
        "Spin",
        [](const Spin& sp) {
            io::JsonValue j;
            j.set("rate", sp.rate);
            return j;
        },
        [](const io::JsonValue& j) { return Spin{j["rate"].asFloat(0)}; });
}

// Build the source scene programmatically.
void buildScene(ecs::World& w) {
    struct Def {
        float x, y, r, g, b, size;
        int shape;
        float spin;
    };
    const Def defs[] = {
        {0.30f, 0.40f, 1.00f, 0.45f, 0.35f, 120, 0, 0.0f},
        {0.45f, 0.40f, 1.00f, 0.80f, 0.35f, 96, 0, 0.0f},
        {0.60f, 0.40f, 0.45f, 0.90f, 0.55f, 110, 0, 0.0f},
        {0.75f, 0.40f, 0.45f, 0.70f, 1.00f, 84, 0, 0.0f},
        {0.37f, 0.68f, 0.95f, 0.85f, 0.40f, 60, 1, 0.9f},
        {0.53f, 0.68f, 0.85f, 0.50f, 1.00f, 60, 1, -1.2f},
        {0.68f, 0.68f, 0.55f, 0.95f, 0.95f, 60, 1, 0.6f},
    };
    for (const Def& d : defs) {
        ecs::Entity e = w.create();
        w.add<Transform2D>(e, {d.x, d.y});
        w.add<Look>(e, {d.r, d.g, d.b, d.size, d.shape});
        if (d.spin != 0.0f) w.add<Spin>(e, {d.spin});
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("ECSAVE (ECS scene save/load) starting");

    io::SceneSerializer serializer;
    registerComponents(serializer);

    // 1) Build a source world. 2) Serialize it to JSON. 3) Load that JSON into a fresh world.
    ecs::World source;
    buildScene(source);
    io::JsonValue doc = serializer.saveWorld(source);
    const size_t savedCount = doc["entities"].size();

    ecs::World world; // the world we actually render — populated ONLY from the serialized JSON
    const int loaded = serializer.loadWorld(doc, world);
    MAZ_LOG_INFO("saved %zu entities to JSON, reloaded %d into a fresh world",
                 savedCount, loaded);

    // Also write the scene to the save directory, to show file save works.
    const std::string savePath = platform::prefPath("MazEngine", "ecsave", "scene.json");
    if (serializer.saveWorldFile(source, savePath))
        MAZ_LOG_INFO("wrote scene to %s", savePath.c_str());

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — ECS Save/Load";
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

    render::TextureHandle white = whiteTex(*renderer);
    render::TextureHandle disc = discTex(*renderer, 64);

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

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            // Render the RELOADED world: every entity with Transform + Look.
            world.view<Transform2D, Look>([&](ecs::Entity e, Transform2D& tr, Look& look) {
                render::SpriteDesc d;
                d.x = tr.x * sw - look.size * 0.5f;
                d.y = tr.y * sh - look.size * 0.5f;
                d.width = look.size;
                d.height = look.size;
                d.color = render::Color{look.r, look.g, look.b, 1.0f};
                if (Spin* sp = world.get<Spin>(e)) d.rotation = t * sp->rate;
                renderer->drawSprite(look.shape == 0 ? disc : white, d);
            });

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  ECS SAVE / LOAD (JSON)",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[128];
            std::snprintf(buf, sizeof(buf),
                          "built %zu entities -> serialized to JSON -> reloaded %d (rendering the reload)",
                          savedCount, loaded);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.85f, 1.0f, 1}, 0.46f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("ECSAVE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
