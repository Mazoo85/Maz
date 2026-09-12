// tests/audio/imaadpcm.cpp — verifies the IMA ADPCM codec (audio ImaAdpcm.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * 4:1 compression — the encoded stream is ~1/4 the PCM byte size (4 bits/sample + a tiny header);
//   * the first sample is reproduced EXACTLY (stored verbatim);
//   * decode(encode(x)) reconstructs a SMOOTH audio-like signal (sines) to within a small error — ADPCM is a
//     smooth-signal codec, so the reconstruction RMS error stays a low fraction of the amplitude;
//   * a linear ramp (maximally smooth) reconstructs very tightly;
//   * silence stays silence; empty in -> empty out; the codec is deterministic.
#include "maz/audio/ImaAdpcm.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

using maz::audio::decodeImaAdpcm;
using maz::audio::encodeImaAdpcm;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

static double rmsError(const std::vector<std::int16_t>& a, const std::vector<std::int16_t>& b) {
    double acc = 0.0;
    const std::size_t n = a.size();
    for (std::size_t i = 0; i < n; ++i) {
        const double d = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        acc += d * d;
    }
    return std::sqrt(acc / static_cast<double>(n));
}

int main() {
    // --- 1. Smooth audio-like signal: 4:1 size, exact first sample, bounded error. ---
    {
        std::vector<std::int16_t> pcm;
        const int N = 8000;
        for (int i = 0; i < N; ++i) {
            const double t = static_cast<double>(i);
            const double s = 9000.0 * std::sin(t * 0.05) + 4000.0 * std::sin(t * 0.017 + 1.0) +
                             2000.0 * std::sin(t * 0.003);
            pcm.push_back(static_cast<std::int16_t>(s));
        }
        const auto enc = encodeImaAdpcm(pcm);
        // ~0.5 byte/sample + 4-byte header, versus 2 bytes/sample of PCM -> ~4:1.
        CHECK(enc.size() <= pcm.size() / 2 + 8, "encoded stream is ~4:1 of 16-bit PCM");
        CHECK(enc.size() * 3 < pcm.size() * 2, "encoded is well under half the PCM byte size");

        const auto dec = decodeImaAdpcm(enc, pcm.size());
        CHECK(dec.size() == pcm.size(), "decode returns the requested sample count");
        CHECK(dec[0] == pcm[0], "first sample reproduced exactly");
        const double err = rmsError(pcm, dec);
        // For this smooth signal the RMS error is a small fraction of the ~15k amplitude.
        CHECK(err < 400.0, "RMS reconstruction error is small for a smooth signal");
    }

    // --- 2. Linear ramp reconstructs very tightly. ---
    {
        std::vector<std::int16_t> pcm;
        for (int i = 0; i < 2000; ++i) pcm.push_back(static_cast<std::int16_t>(-10000 + i * 10));
        const auto dec = decodeImaAdpcm(encodeImaAdpcm(pcm), pcm.size());
        double worst = 0.0;
        for (std::size_t i = 0; i < pcm.size(); ++i)
            worst = std::max(worst, std::fabs(static_cast<double>(pcm[i]) - static_cast<double>(dec[i])));
        CHECK(worst < 200.0, "a linear ramp reconstructs within a tight bound");
    }

    // --- 3. Silence, empty, determinism. ---
    {
        std::vector<std::int16_t> zeros(1000, 0);
        const auto dec = decodeImaAdpcm(encodeImaAdpcm(zeros), zeros.size());
        bool allZero = true;
        for (std::int16_t s : dec) if (s != 0) allZero = false;
        CHECK(allZero && dec.size() == 1000, "silence stays silence");

        CHECK(encodeImaAdpcm({}).empty(), "empty input -> empty output");
        CHECK(decodeImaAdpcm({}, 0).empty(), "decode of nothing -> empty");

        std::vector<std::int16_t> sig;
        for (int i = 0; i < 500; ++i) sig.push_back(static_cast<std::int16_t>(3000.0 * std::sin(i * 0.1)));
        CHECK(encodeImaAdpcm(sig) == encodeImaAdpcm(sig), "encoding is deterministic");
    }

    if (g_fail == 0) {
        std::printf("imaadpcm: OK — 4:1 size, exact first sample, smooth-signal error bound, ramp, silence.\n");
        return 0;
    }
    std::printf("imaadpcm: %d failure(s).\n", g_fail);
    return 1;
}
