// Maz Engine — "BASE64" (base64 encode/decode, toward Godot's Marshalls raw<->base64)
// Base64 carries BINARY data through TEXT channels — embedding a blob inside JSON, a .tres resource, a URL,
// or a config value. This demo is a static readout: a few inputs (text and a raw byte buffer) shown with
// their io::base64Encode output, plus a decode round-trip confirmation — every string computed live from
// the codec, so the picture IS the test. Fixed inputs -> deterministic, golden-stable. --headless / --frames.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string toHex(const std::vector<std::uint8_t>& b) {
    std::string s;
    char buf[4];
    for (std::uint8_t v : b) {
        std::snprintf(buf, sizeof(buf), "%02X ", v);
        s += buf;
    }
    return s;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("BASE64 starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Base64";
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
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- Compute everything up front from the codec. -------------------------------------------------
    const std::string t1 = "hello";
    const std::string t2 = "Maz Engine \xE2\x9C\x93"; // includes a multi-byte UTF-8 check mark
    std::vector<std::uint8_t> blob(12);
    for (int i = 0; i < 12; ++i) {
        blob[static_cast<std::size_t>(i)] = static_cast<std::uint8_t>(i * 21 + 3); // 3,24,45,...
    }

    const std::string e1 = io::base64Encode(t1);
    const std::string e2 = io::base64Encode(t2);
    const std::string e3 = io::base64Encode(blob);

    // Round-trip: decode e2 and confirm it matches the original bytes.
    const std::vector<std::uint8_t> d2 = io::base64Decode(e2);
    const std::string d2str(d2.begin(), d2.end());
    const bool roundTrip = (d2str == t2);

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.62f, 0.68f, 0.8f, 1};
    const render::Color kIn{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOut{1.0f, 0.8f, 0.45f, 1};
    const render::Color kOk{0.5f, 0.95f, 0.6f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.08f, 0.09f, 0.12f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  BASE64", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "binary <-> text via io::base64Encode / base64Decode (Godot Marshalls)", kDim,
                          0.34f);

            float y = 130.0f;
            const float sz = 0.36f;
            auto pair = [&](const std::string& label, const std::string& in, render::Color inCol,
                            const std::string& b64) {
                font.drawText(*renderer, 40.0f, y, label.c_str(), kDim, sz);
                font.drawText(*renderer, 220.0f, y, in.c_str(), inCol, sz);
                y += 34.0f;
                font.drawText(*renderer, 40.0f, y, "  base64", kDim, sz);
                font.drawText(*renderer, 220.0f, y, b64.c_str(), kOut, sz);
                y += 54.0f;
            };

            pair("text", "\"" + t1 + "\"", kIn, e1);
            pair("text (utf-8)", "\"" + t2 + "\"", kIn, e2);
            pair("bytes (hex)", toHex(blob), kIn, e3);

            font.drawText(*renderer, 40.0f, y, "decode round-trip", kDim, sz);
            font.drawText(*renderer, 300.0f, y,
                          roundTrip ? "decode(encode(x)) == x   OK" : "MISMATCH",
                          roundTrip ? kOk : render::Color{1, 0.4f, 0.4f, 1}, sz);

            font.drawText(*renderer, 40.0f, 664.0f,
                          "3 bytes -> 4 chars (A-Z a-z 0-9 + /), '=' pads the tail; the decoder skips "
                          "embedded newlines",
                          render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("BASE64 shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
