// Maz Engine — "DATA" (data-driven scene from JSON)
// Nothing on screen is hard-coded: the entire scene — clear color, title, and every sprite (its
// shape, position, size, tint, and bob animation) — is described by an embedded JSON document and
// parsed at runtime with maz::io::parseJson. This proves the engine can be driven by human-editable
// data files (configs, levels, tuning) rather than code. The document is deterministic and the bob
// motion is a pure function of the fixed-step clock, so the render is golden-stable.
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

// A soft-edged disc (solid=false gives a filled circle) used for round sprites.
render::TextureHandle discTex(render::Renderer& r, int size) {
    std::vector<uint8_t> px(static_cast<size_t>(size) * static_cast<size_t>(size) * 4, 0);
    const float c = (static_cast<float>(size) - 1.0f) * 0.5f;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const float dx = (static_cast<float>(x) - c) / c, dy = (static_cast<float>(y) - c) / c;
            const float d = std::sqrt(dx * dx + dy * dy);
            const float a = d >= 1.0f ? 0.0f : (d > 0.92f ? (1.0f - d) / 0.08f : 1.0f);
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

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

// Read an RGB(A) color from a JSON array like [0.8, 0.4, 0.2] (alpha optional, defaults to 1).
render::Color colorFrom(const io::JsonValue& arr, render::Color def) {
    if (!arr.isArray() || arr.size() < 3) return def;
    return render::Color{arr[0].asFloat(def.r), arr[1].asFloat(def.g), arr[2].asFloat(def.b),
                         arr.size() >= 4 ? arr[3].asFloat(1.0f) : 1.0f};
}

// The scene document. In a shipping game this would be a file on disk; embedding it keeps the demo
// self-contained (and the golden reproducible) while exercising the exact same parse path.
const char* kSceneJson = R"JSON(
{
  "title": "DATA-DRIVEN SCENE (parsed from JSON)",
  "clearColor": [0.07, 0.08, 0.12, 1.0],
  "sprites": [
    { "shape": "disc", "x": 0.20, "y": 0.42, "size": 120, "color": [1.00, 0.45, 0.35],
      "bob": { "amp": 46, "speed": 1.3, "phase": 0.0 } },
    { "shape": "disc", "x": 0.38, "y": 0.42, "size": 96,  "color": [1.00, 0.80, 0.35],
      "bob": { "amp": 40, "speed": 1.7, "phase": 0.9 } },
    { "shape": "disc", "x": 0.54, "y": 0.42, "size": 110, "color": [0.45, 0.90, 0.55],
      "bob": { "amp": 52, "speed": 1.1, "phase": 1.8 } },
    { "shape": "disc", "x": 0.70, "y": 0.42, "size": 84,  "color": [0.45, 0.70, 1.00],
      "bob": { "amp": 36, "speed": 2.1, "phase": 2.7 } },
    { "shape": "disc", "x": 0.84, "y": 0.42, "size": 128, "color": [0.80, 0.50, 1.00],
      "bob": { "amp": 58, "speed": 0.9, "phase": 3.6 } },
    { "shape": "box", "x": 0.50, "y": 0.80, "size": 620, "height": 26, "color": [0.20, 0.24, 0.34] },
    { "shape": "box", "x": 0.30, "y": 0.20, "size": 40, "color": [0.95, 0.95, 0.55], "spin": 0.6 },
    { "shape": "box", "x": 0.72, "y": 0.20, "size": 40, "color": [0.55, 0.95, 0.95], "spin": -0.8 }
  ]
}
)JSON";

struct SpriteSpec {
    bool isDisc = false;
    float fx = 0.5f, fy = 0.5f;  // fractional position (0..1 of screen)
    float w = 32.0f, h = 32.0f;
    render::Color color{1, 1, 1, 1};
    float bobAmp = 0.0f, bobSpeed = 0.0f, bobPhase = 0.0f;
    float spin = 0.0f;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("DATA (data-driven scene) starting");

    // --- Parse the scene document up front; fail loudly but survive gracefully -------------------
    auto parsed = io::parseJson(kSceneJson);
    if (!parsed.ok) {
        MAZ_LOG_ERROR("scene JSON parse error at %d:%d: %s", parsed.line, parsed.column,
                      parsed.error.c_str());
    }
    const io::JsonValue& scene = parsed.value;
    const std::string title = scene["title"].asString("(untitled scene)");
    const render::Color clear = colorFrom(scene["clearColor"], render::Color{0.07f, 0.08f, 0.12f, 1});

    std::vector<SpriteSpec> specs;
    for (const io::JsonValue& s : scene["sprites"].items()) {
        SpriteSpec sp;
        sp.isDisc = s["shape"].asString("box") == "disc";
        sp.fx = s["x"].asFloat(0.5f);
        sp.fy = s["y"].asFloat(0.5f);
        sp.w = s["size"].asFloat(32.0f);
        sp.h = s.fields().contains("height") ? s["height"].asFloat(sp.w) : sp.w;
        sp.color = colorFrom(s["color"], render::Color{1, 1, 1, 1});
        sp.bobAmp = s["bob"]["amp"].asFloat(0.0f);
        sp.bobSpeed = s["bob"]["speed"].asFloat(0.0f);
        sp.bobPhase = s["bob"]["phase"].asFloat(0.0f);
        sp.spin = s["spin"].asFloat(0.0f);
        specs.push_back(sp);
    }
    MAZ_LOG_INFO("loaded %zu sprites from JSON scene", specs.size());

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Data-Driven Scene";
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
    render::TextureHandle white = whiteTex(*renderer);

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

        renderer->setClearColor(clear);
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            for (const SpriteSpec& sp : specs) {
                const float cx = sp.fx * sw;
                const float bob = sp.bobAmp * std::sin(t * sp.bobSpeed + sp.bobPhase);
                const float cy = sp.fy * sh + bob;
                render::SpriteDesc d;
                d.x = cx - sp.w * 0.5f;
                d.y = cy - sp.h * 0.5f;
                d.width = sp.w;
                d.height = sp.h;
                d.color = sp.color;
                if (sp.spin != 0.0f) {
                    d.rotation = t * sp.spin;  // radians about the sprite center
                }
                renderer->drawSprite(sp.isDisc ? disc : white, d);
            }

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  DATA-DRIVEN SCENE",
                          render::Color{1, 1, 1, 1}, 0.7f);
            font.drawText(*renderer, 16.0f, 46.0f, title.c_str(),
                          render::Color{0.7f, 0.85f, 1.0f, 1}, 0.5f);
            char buf[96];
            std::snprintf(buf, sizeof(buf), "%zu sprites parsed from JSON at runtime", specs.size());
            font.drawTextCentered(*renderer, sw * 0.5f, sh - 40.0f, buf,
                                  render::Color{0.6f, 0.7f, 0.85f, 1}, 0.45f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("DATA shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
