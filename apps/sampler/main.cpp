// Maz Engine — "SAMPLER" (sample-playback mixer, toward Godot's AudioStreamPlayer over AudioStreamWAV)
// Two short clips are synthesized and round-tripped through the WAV codec (encode -> decode) to prove they
// are *decoded* audio, then registered as voices in an audio::SampleMixer: a decaying tone panned LEFT and a
// noise blip panned RIGHT, at different gains. The mixer sums them offline into one interleaved stereo
// buffer, which is drawn as two oscilloscopes (L and R) beneath the two source clips — you can SEE the tone
// living in the left trace and the blip in the right. All synthesis is a fixed deterministic LCG, so the
// mixed output is byte-stable → golden-safe. Run --headless / --frames N for CI.

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

// Downsample an interleaved buffer's channel `ch` (of `channels`) into a waveform polyline.
std::vector<math::vec2> waveform(const std::vector<float>& samples, std::uint16_t channels,
                                 std::uint16_t ch, float x0, float x1, float midY, float amp, int points) {
    std::vector<math::vec2> w;
    const std::size_t frames = channels ? samples.size() / channels : 0;
    for (int p = 0; p < points && frames > 0; ++p) {
        const float u = static_cast<float>(p) / static_cast<float>(points - 1);
        const std::size_t idx =
            static_cast<std::size_t>(u * static_cast<float>(frames - 1)) * channels + ch;
        const float v = idx < samples.size() ? samples[idx] : 0.0f;
        w.push_back(math::vec2(x0 + u * (x1 - x0), midY - v * amp));
    }
    return w;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SAMPLER (sample-playback mixer) starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Sample Mixer";
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

    const std::uint32_t kRate = 8000;

    // --- Clip A: a decaying two-tone. Round-trip through the WAV codec so it is genuinely decoded audio. ---
    audio::WavData toneSrc;
    toneSrc.sampleRate = kRate;
    toneSrc.channels = 1;
    {
        const int n = 2400; // 0.3 s
        toneSrc.samples.reserve(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(kRate);
            const float env = std::exp(-t * 7.0f);
            const float s = std::sin(t * 6.2831853f * 300.0f) * 0.6f +
                            std::sin(t * 6.2831853f * 450.0f) * 0.25f;
            toneSrc.samples.push_back(s * env);
        }
    }
    audio::WavData tone;
    audio::decodeWav(audio::encodeWav(toneSrc), tone);

    // --- Clip B: a short decaying noise blip (deterministic LCG so the golden is stable). ---
    audio::WavData noiseSrc;
    noiseSrc.sampleRate = kRate;
    noiseSrc.channels = 1;
    {
        const int n = 1400; // 0.175 s
        noiseSrc.samples.reserve(static_cast<std::size_t>(n));
        std::uint32_t seed = 0x1234567u;
        for (int i = 0; i < n; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const float r = static_cast<float>(seed >> 9) / static_cast<float>(1u << 23) * 2.0f - 1.0f;
            const float t = static_cast<float>(i) / static_cast<float>(kRate);
            const float env = std::exp(-t * 16.0f);
            noiseSrc.samples.push_back(r * env * 0.7f);
        }
    }
    audio::WavData noise;
    audio::decodeWav(audio::encodeWav(noiseSrc), noise);

    // --- Register both as mixer voices and render the whole scene offline into one stereo buffer. ---
    audio::SampleMixer mixer;
    mixer.play(tone, /*gain=*/0.85f, /*pan=*/-0.9f);  // tone → left
    mixer.play(noise, /*gain=*/0.9f, /*pan=*/0.85f);  // blip → right
    const int voicesStarted = static_cast<int>(mixer.activeVoices());
    const int outFrames = 2600;
    std::vector<float> stereo(static_cast<std::size_t>(outFrames) * 2, 0.0f);
    mixer.mix(stereo.data(), static_cast<std::size_t>(outFrames), kRate);

    // Precompute all polylines (static → deterministic golden).
    const float x0 = 56.0f, x1 = 1224.0f;
    const std::vector<math::vec2> waveTone =
        waveform(tone.samples, 1, 0, x0, x1, 236.0f, 78.0f, 640);
    const std::vector<math::vec2> waveNoise =
        waveform(noise.samples, 1, 0, x0, x1, 236.0f, 78.0f, 640);
    const std::vector<math::vec2> waveL = waveform(stereo, 2, 0, x0, x1, 470.0f, 90.0f, 640);
    const std::vector<math::vec2> waveR = waveform(stereo, 2, 1, x0, x1, 620.0f, 90.0f, 640);

    const render::Color kText{1, 1, 1, 1};
    const render::Color kDim{0.72f, 0.78f, 0.9f, 1};
    const render::Color kLine{0.28f, 0.32f, 0.4f, 1.0f};
    const render::Color kTone{0.55f, 0.8f, 1.0f, 1.0f};
    const render::Color kNoise{1.0f, 0.7f, 0.45f, 1.0f};
    const render::Color kLeft{0.5f, 0.9f, 0.65f, 1.0f};
    const render::Color kRight{0.95f, 0.6f, 0.85f, 1.0f};

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

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SAMPLE MIXER", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 46.0f,
                          "two decoded .wav clips played as voices -> summed into one stereo buffer "
                          "(audio::SampleMixer)",
                          kDim, 0.32f);

            // Source clips (small, top).
            fillRect(*renderer, white, x0, 236.0f, x1 - x0, 1.0f, kLine);
            font.drawText(*renderer, 16.0f, 150.0f, "source clips:", kDim, 0.3f);
            stroke(*renderer, waveTone, 2.0f, kTone);
            stroke(*renderer, waveNoise, 2.0f, kNoise);
            font.drawText(*renderer, x1 - 360.0f, 150.0f, "tone (left)  +  noise blip (right)", kDim, 0.28f);

            // Mixed output — L then R.
            fillRect(*renderer, white, x0, 470.0f, x1 - x0, 1.0f, kLine);
            fillRect(*renderer, white, x0, 620.0f, x1 - x0, 1.0f, kLine);
            font.drawText(*renderer, 16.0f, 360.0f, "mixed output:", kDim, 0.3f);
            font.drawText(*renderer, 20.0f, 448.0f, "L", kLeft, 0.34f);
            font.drawText(*renderer, 20.0f, 598.0f, "R", kRight, 0.34f);
            stroke(*renderer, waveL, 2.4f, kLeft);
            stroke(*renderer, waveR, 2.4f, kRight);

            char info[96];
            std::snprintf(info, sizeof(info), "%d voices played  ->  %d stereo frames @ %u Hz",
                          voicesStarted, outFrames, kRate);
            font.drawText(*renderer, 56.0f, 686.0f, info, render::Color{0.62f, 0.68f, 0.78f, 1}, 0.3f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SAMPLER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
