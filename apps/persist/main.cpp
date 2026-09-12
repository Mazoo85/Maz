// Maz Engine — "PERSIST" (serialization / scene save-load demo)
// Builds a scene of colored props, serializes it to a binary file on disk with maz::io, clears the
// in-memory scene, then reads the file back and renders the reconstructed scene. Proves the engine
// can round-trip structured game state to disk (save games, level files). The HUD reports the byte
// count and that the render you see came from the reloaded data. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

// One serializable prop: a named, transformed, colored box. This is the app's on-disk schema; the
// engine's maz::io codec handles the byte-level details.
struct Prop {
    std::string name;
    float pos[3];
    float rotY;
    float scale;
    float color[3];
};

constexpr uint32_t kMagic = 0x4D415A53;  // 'MAZS'
constexpr uint32_t kVersion = 1;

void saveScene(io::ByteWriter& w, const std::vector<Prop>& props) {
    w.writeHeader(kMagic, kVersion);
    w.write<uint32_t>(static_cast<uint32_t>(props.size()));
    for (const Prop& p : props) {
        w.writeString(p.name);
        w.writeBytes(p.pos, sizeof(p.pos));
        w.write<float>(p.rotY);
        w.write<float>(p.scale);
        w.writeBytes(p.color, sizeof(p.color));
    }
}

bool loadScene(io::ByteReader& r, std::vector<Prop>& props) {
    props.clear();
    if (!r.readHeader(kMagic, kVersion)) {
        return false;
    }
    const uint32_t count = r.read<uint32_t>();
    if (!r.ok()) {
        return false;
    }
    for (uint32_t i = 0; i < count && r.ok(); ++i) {
        Prop p{};
        p.name = r.readString();
        r.readBytes(p.pos, sizeof(p.pos));
        p.rotY = r.read<float>();
        p.scale = r.read<float>();
        r.readBytes(p.color, sizeof(p.color));
        if (r.ok()) {
            props.push_back(std::move(p));
        }
    }
    return r.ok();
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("PERSIST (scene save/load demo) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Serialization";
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

    // --- Author a scene, save it, clear it, reload it -----------------------------------------
    std::vector<Prop> authored;
    const float palette[6][3] = {{0.85f, 0.4f, 0.4f}, {0.4f, 0.8f, 0.5f}, {0.4f, 0.6f, 0.9f},
                                 {0.9f, 0.8f, 0.4f},  {0.8f, 0.45f, 0.85f}, {0.4f, 0.85f, 0.85f}};
    for (int i = 0; i < 14; ++i) {
        Prop p{};
        p.name = "prop_" + std::to_string(i);
        const float ang = static_cast<float>(i) * 0.62f;
        const float rad = 3.0f + static_cast<float>(i % 4) * 1.6f;
        p.pos[0] = std::cos(ang) * rad;
        p.pos[1] = 0.5f + static_cast<float>(i % 3) * 0.8f;
        p.pos[2] = std::sin(ang) * rad;
        p.rotY = ang;
        p.scale = 0.7f + static_cast<float>(i % 3) * 0.4f;
        p.color[0] = palette[i % 6][0];
        p.color[1] = palette[i % 6][1];
        p.color[2] = palette[i % 6][2];
        authored.push_back(p);
    }

    io::ByteWriter writer;
    saveScene(writer, authored);
    const std::string path = platform::prefPath("MazEngine", "persist", "scene.bin");
    const bool wrote = io::writeFile(path, writer.data());
    const size_t byteCount = writer.size();

    // Wipe the authored copy so what we render can only come from disk.
    authored.clear();

    std::vector<Prop> scene;
    bool loaded = false;
    {
        std::vector<uint8_t> bytes;
        if (wrote && io::readFile(path, bytes)) {
            io::ByteReader reader(bytes);
            loaded = loadScene(reader, scene);
        }
    }
    MAZ_LOG_INFO("PERSIST wrote=%d loaded=%d (%zu props, %zu bytes) path=%s", wrote ? 1 : 0,
                 loaded ? 1 : 0, scene.size(), byteCount, path.c_str());

    // --- Meshes / textures --------------------------------------------------------------------
    auto boxData = render::shapes::makeBox(1.0f, render::Color{1, 1, 1, 1});
    render::MeshHandle box = renderer->createMesh(
        boxData.vertices.data(), static_cast<uint32_t>(boxData.vertices.size()),
        boxData.indices.data(), static_cast<uint32_t>(boxData.indices.size()));
    auto floorData = render::shapes::makePlane(16.0f, render::Color{0.3f, 0.32f, 0.36f, 1});
    render::MeshHandle floor = renderer->createMesh(
        floorData.vertices.data(), static_cast<uint32_t>(floorData.vertices.size()),
        floorData.indices.data(), static_cast<uint32_t>(floorData.indices.size()));
    const uint8_t whitePx[4] = {255, 255, 255, 255};
    render::TextureHandle white = renderer->createTexture(1, 1, whitePx);

    // One texture per reloaded prop's color (built from the deserialized data).
    std::vector<render::TextureHandle> propTex;
    for (const Prop& p : scene) {
        const uint8_t px[4] = {static_cast<uint8_t>(p.color[0] * 255),
                               static_cast<uint8_t>(p.color[1] * 255),
                               static_cast<uint8_t>(p.color[2] * 255), 255};
        propTex.push_back(renderer->createTexture(1, 1, px));
    }

    render::SceneLighting lighting;
    lighting.ambient[0] = lighting.ambient[1] = lighting.ambient[2] = 0.4f;
    renderer->setLighting(lighting);

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
        const float aspect =
            bh > 0 ? static_cast<float>(bw) / static_cast<float>(bh) : 16.0f / 9.0f;

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
            t += static_cast<float>(clock.fixedDelta());
        }

        const glm::vec3 eye(std::sin(t * 0.2f) * 13.0f, 8.0f, std::cos(t * 0.2f) * 13.0f);
        const glm::mat4 proj = math::perspective(glm::radians(52.0f), aspect, 0.1f, 100.0f);
        const glm::mat4 view = glm::lookAt(eye, glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0, 1, 0));
        const glm::mat4 viewProj = proj * view;

        renderer->setClearColor(render::Color{0.12f, 0.14f, 0.18f, 1.0f});
        if (renderer->beginFrame()) {
            renderer->setViewProjection3D(glm::value_ptr(viewProj));
            renderer->setCameraPosition(glm::value_ptr(eye));

            glm::mat4 fm = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -0.25f, 0.0f));
            fm = glm::scale(fm, glm::vec3(1.0f, 0.5f, 1.0f));
            renderer->drawMesh(floor, glm::value_ptr(fm), white);

            // Render the RELOADED scene (authored copy was cleared before this point).
            for (size_t i = 0; i < scene.size(); ++i) {
                const Prop& p = scene[i];
                glm::mat4 m = glm::translate(glm::mat4(1.0f), glm::vec3(p.pos[0], p.pos[1], p.pos[2]));
                m = glm::rotate(m, p.rotY, glm::vec3(0, 1, 0));
                m = glm::scale(m, glm::vec3(p.scale));
                renderer->drawMesh(box, glm::value_ptr(m), propTex[i]);
            }

            render::Camera2D ui;
            ui.usePixelSpace = true;
            renderer->setCamera2D(ui);
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SCENE SERIALIZATION",
                          render::Color{1, 1, 1, 1}, 0.7f);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%zu props saved to disk (%zu bytes) -> reloaded & drawn",
                          scene.size(), byteCount);
            font.drawText(*renderer, 16.0f, 46.0f, buf, render::Color{0.7f, 0.95f, 0.75f, 1}, 0.5f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("PERSIST shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
