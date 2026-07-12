// Maz Engine — "WAV" (WAV audio load/save, toward Godot's AudioStreamWAV)
// Until now every Maz sound was synthesized on the fly — there was no way to read (or write) an actual
// audio file. This synthesizes a short decaying tone, ENCODES it to the exact bytes of a .wav file
// (audio::encodeWav), then DECODES those bytes back (audio::decodeWav) and draws the reconstructed
// waveform as an oscilloscope, alongside the parsed RIFF/WAVE header fields. Deterministic (fixed
// synthesis) → golden-stable. Run --headless / --frames N for CI.

#include "maz/Engine.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

render::TextureHandle whiteTex(render::Renderer& r) {
    const uint8_t px[4] = {255, 255, 255, 255};
    return r.createTexture(1, 1, px);
}

void fillRect(render::Renderer& r, render::TextureHandle white, float x, float y, float w, float h,
              render::Color c) {
    render::SpriteDesc d;
    d.x = x;
    d.y = y;
    d.width = w;
    d.height = h;
    d.color = c;
    r.drawSprite(white, d);
}

void stroke(render::Renderer& r, const std::vector<math::vec2>& pts, float width, render::Color col) {
    render::PolylineStyle s;
    s.width = width;
    s.joint = render::JointMode::Round;
    s.cap = render::CapMode::Round;
    const std::vector<math::vec2> tris = render::buildPolyline(pts, s);
    for (std::size_t i = 0; i + 2 < tris.size(); i += 3) {
        const render::Point2 t[3] = {{tris[i].x, tris[i].y},
                                     {tris[i + 1].x, tris[i + 1].y},
                                     {tris[i + 2].x, tris[i + 2].y}};
        r.drawConvexPolygon(t, 3, col);
    }
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("WAV (audio load/save) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — WAV Load/Save";
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

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // --- Synthesize a short tone, encode to WAV bytes, decode it back. ---
    audio::WavData src;
    src.sampleRate = 8000;
    src.channels = 1;
    const int frames = 4000; // 0.5 s
    src.samples.reserve(static_cast<std::size_t>(frames));
    for (int i = 0; i < frames; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(src.sampleRate);
        const float env = std::exp(-t * 6.0f);
        const float tone = std::sin(t * 6.2831853f * 220.0f) * 0.55f + std::sin(t * 6.2831853f * 440.0f) * 0.2f;
        src.samples.push_back(tone * env);
    }

    const std::vector<std::uint8_t> bytes = audio::encodeWav(src);
    audio::WavData wav;
    const bool ok = audio::decodeWav(bytes, wav);

    // Downsample the decoded samples into a waveform polyline across the width.
    std::vector<math::vec2> wave;
    {
        const float x0 = 56.0f, x1 = 1224.0f, midY = 430.0f, amp = 150.0f;
        const int points = 640;
        const std::size_t fc = wav.frameCount();
        for (int p = 0; p < points && fc > 0; ++p) {
            const float u = static_cast<float>(p) / static_cast<float>(points - 1);
            const std::size_t idx = static_cast<std::size_t>(u * static_cast<float>(fc - 1)) * wav.channels;
            const float v = idx < wav.samples.size() ? wav.samples[idx] : 0.0f;
            wave.push_back(math::vec2(x0 + u * (x1 - x0), midY - v * amp));
        }
    }

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  WAV LOAD / SAVE",
                          render::Color{1, 1, 1, 1}, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "synthesize -> encodeWav (.wav bytes) -> decodeWav -> draw the reconstructed "
                          "waveform (audio::Wav)",
                          render::Color{0.78f, 0.83f, 0.95f, 1}, 0.32f);

            // Parsed header panel.
            fillRect(*renderer, white, 40.0f, 92.0f, 1200.0f, 120.0f, render::Color{0.11f, 0.13f, 0.17f, 1.0f});
            font.drawText(*renderer, 56.0f, 104.0f, "decoded .wav header:", render::Color{0.8f, 0.86f, 0.95f, 1}, 0.32f);
            char l1[96], l2[96];
            std::snprintf(l1, sizeof(l1), "RIFF/WAVE PCM 16-bit    %u Hz    %u channel(s)", wav.sampleRate,
                          static_cast<unsigned>(wav.channels));
            std::snprintf(l2, sizeof(l2), "%zu frames    %.2f s    encoded %zu bytes    decode: %s",
                          wav.frameCount(),
                          static_cast<double>(wav.frameCount()) /
                              static_cast<double>(wav.sampleRate ? wav.sampleRate : 1),
                          bytes.size(), ok ? "OK" : "FAILED");
            font.drawText(*renderer, 56.0f, 140.0f, l1, render::Color{0.82f, 0.88f, 0.8f, 1}, 0.3f);
            font.drawText(*renderer, 56.0f, 172.0f, l2, render::Color{0.82f, 0.88f, 0.8f, 1}, 0.3f);

            // Oscilloscope: zero line + amplitude guides + the waveform.
            fillRect(*renderer, white, 56.0f, 430.0f, 1168.0f, 1.5f, render::Color{0.3f, 0.34f, 0.4f, 1.0f});
            fillRect(*renderer, white, 56.0f, 280.0f, 1168.0f, 1.0f, render::Color{0.2f, 0.23f, 0.28f, 1.0f});
            fillRect(*renderer, white, 56.0f, 580.0f, 1168.0f, 1.0f, render::Color{0.2f, 0.23f, 0.28f, 1.0f});
            font.drawText(*renderer, 20.0f, 272.0f, "+1", render::Color{0.5f, 0.55f, 0.6f, 1}, 0.28f);
            font.drawText(*renderer, 24.0f, 422.0f, "0", render::Color{0.5f, 0.55f, 0.6f, 1}, 0.28f);
            font.drawText(*renderer, 20.0f, 572.0f, "-1", render::Color{0.5f, 0.55f, 0.6f, 1}, 0.28f);
            stroke(*renderer, wave, 2.5f, render::Color{0.45f, 0.85f, 0.6f, 1.0f});

            font.drawText(*renderer, 56.0f, 636.0f,
                          "the same codec reads external .wav assets and exports recorded audio",
                          render::Color{0.6f, 0.64f, 0.72f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("WAV shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
