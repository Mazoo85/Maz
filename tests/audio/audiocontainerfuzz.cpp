// Malformed-input robustness fuzz for the binary audio + container decoders that read length/size fields
// straight out of untrusted bytes — audio::decodeWav / decodeQoa / decodeImaAdpcm and render::parseGlb.
// These are the classic out-of-bounds surface a game hits loading a corrupt or malicious asset, and the
// existing tests feed them only valid, self-encoded input. Here each decoder gets every truncation of a
// valid seed, single-byte mutations, and random buffers; decodeImaAdpcm additionally gets a large declared
// sample count against a short data buffer (the mismatch that drives a length-field overread). A decoder may
// reject bad input (return false / empty) — it just must never read out of bounds or invoke UB. Built under
// ASan+UBSan (the sanitizer CI job runs it); this main only needs to finish without a sanitizer trap.
#include "maz/audio/Wav.hpp"
#include "maz/audio/Qoa.hpp"
#include "maz/audio/ImaAdpcm.hpp"
#include "maz/render/GlbContainer.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feedWav(const std::uint8_t* d, std::size_t n) {
    audio::WavData out;
    if (audio::decodeWav(d, n, out)) g_sink += out.samples.size() + out.channels;
}
static void feedQoa(const std::uint8_t* d, std::size_t n) {
    audio::WavData out = audio::decodeQoa(d, n);
    g_sink += out.samples.size() + out.channels;
}
static void feedGlb(const std::uint8_t* d, std::size_t n) {
    render::GlbChunks out;
    if (render::parseGlb(d, n, out)) g_sink += out.json.size() + out.bin.size();
}

using ByteFn = void (*)(const std::uint8_t*, std::size_t);

// Every truncation of `seed`, then single-byte mutations (0x00, 0xFF, +1) of the full seed.
static void truncateAndMutate(ByteFn fn, std::vector<std::uint8_t> seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) fn(seed.data(), k);
    const std::uint8_t patches[3] = {0x00, 0xFF, 0x01};
    for (std::size_t i = 0; i < seed.size(); ++i) {
        const std::uint8_t orig = seed[i];
        for (std::uint8_t p : patches) {
            seed[i] = (p == 0x01) ? static_cast<std::uint8_t>(orig + 1) : p;
            fn(seed.data(), seed.size());
        }
        seed[i] = orig;
    }
}

int main() {
    // Valid seeds via the matching encoders so truncations exercise real header/body code paths.
    audio::WavData wav;
    wav.sampleRate = 22050;
    wav.channels = 2;
    for (int i = 0; i < 128; ++i) wav.samples.push_back(static_cast<float>((i % 33) - 16) / 16.0f);

    truncateAndMutate(feedWav, audio::encodeWav(wav));
    truncateAndMutate(feedQoa, audio::encodeQoa(wav));
    truncateAndMutate(feedGlb, render::buildGlb("{\"asset\":{\"version\":\"2.0\"}}",
                                                std::vector<std::uint8_t>{1, 2, 3, 4, 5, 6, 7, 8}));

    // Random-buffer fuzz, biased toward each format's magic so the parser gets past the signature check.
    ByteFn all[] = {feedWav, feedQoa, feedGlb};
    std::mt19937 rng(0x5EEDu);
    std::vector<std::uint8_t> buf;
    for (int iter = 0; iter < 30000; ++iter) {
        const std::size_t n = rng() % 513u;
        buf.resize(n);
        for (std::size_t i = 0; i < n; ++i) buf[i] = static_cast<std::uint8_t>(rng());
        if (n >= 4 && (iter & 1) == 0) { buf[0] = 'R'; buf[1] = 'I'; buf[2] = 'F'; buf[3] = 'F'; } // WAV
        if (n >= 4 && (iter & 1) == 1) { buf[0] = 'q'; buf[1] = 'o'; buf[2] = 'a'; buf[3] = 'f'; } // QOA
        if (n >= 4 && (iter % 3) == 0) { buf[0] = 'g'; buf[1] = 'l'; buf[2] = 'T'; buf[3] = 'F'; } // GLB magic
        for (ByteFn fn : all) fn(buf.data(), n);
    }

    // decodeImaAdpcm(data, sampleCount): a large declared sampleCount against a short data buffer is the
    // length-field mismatch that drives an overread. Sweep short random blocks against big sample counts.
    for (int iter = 0; iter < 20000; ++iter) {
        const std::size_t n = rng() % 65u;
        buf.resize(n);
        for (std::size_t i = 0; i < n; ++i) buf[i] = static_cast<std::uint8_t>(rng());
        const std::size_t sampleCount = rng() % 20000u; // often far exceeds what `buf` actually encodes
        std::vector<std::int16_t> pcm = audio::decodeImaAdpcm(buf, sampleCount);
        g_sink += pcm.size();
    }

    std::printf("audiocontainer fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
