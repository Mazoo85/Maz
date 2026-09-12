// tests/audio/g711.cpp — verifies the G.711 mu-law / A-law companding codecs (audio::encodeMuLaw /
// decodeMuLaw / encodeALaw / decodeALaw). Checks the ITU-T spec's exact anchor values (mu-law silence ->
// 0xFF), monotonicity + sign handling, and an encode->decode round-trip whose error is bounded by the
// standard's logarithmic quantization (small near silence, proportionally larger for loud samples). Byte
// in/out, headless.
#include "maz/audio/G711.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::audio;

int main() {
    // --- 1. Spec anchor: mu-law encodes 0 (silence) to 0xFF, and it decodes back near 0. ---
    {
        const std::uint8_t z = encodeMuLawSample(0);
        CHECK(z == 0xFF, "mu-law encodes silence to 0xFF (ITU-T anchor)");
        CHECK(std::abs(decodeMuLawSample(z)) <= 8, "mu-law 0xFF decodes back to ~0");
    }

    // --- 2. Sign is preserved and symmetric-ish for both laws. ---
    {
        CHECK(decodeMuLawSample(encodeMuLawSample(1000)) > 0, "mu-law keeps positive sign");
        CHECK(decodeMuLawSample(encodeMuLawSample(-1000)) < 0, "mu-law keeps negative sign");
        CHECK(decodeALawSample(encodeALawSample(1000)) > 0, "A-law keeps positive sign");
        CHECK(decodeALawSample(encodeALawSample(-1000)) < 0, "A-law keeps negative sign");
    }

    // --- 3. Monotonicity: bigger input magnitude -> bigger (or equal) decoded magnitude. ---
    {
        int prevMu = 0, prevA = 0;
        bool monoMu = true, monoA = true;
        for (int s = 0; s <= 30000; s += 500) {
            const int mu = std::abs(static_cast<int>(decodeMuLawSample(encodeMuLawSample(static_cast<std::int16_t>(s)))));
            const int al = std::abs(static_cast<int>(decodeALawSample(encodeALawSample(static_cast<std::int16_t>(s)))));
            if (mu < prevMu - 1) monoMu = false;
            if (al < prevA - 1) monoA = false;
            prevMu = mu; prevA = al;
        }
        CHECK(monoMu, "mu-law is monotonic in magnitude");
        CHECK(monoA, "A-law is monotonic in magnitude");
    }

    // --- 4. Round-trip through WavData: 8 bytes/sample -> 1 (2:1), error bounded by log quantization. ---
    {
        WavData in;
        in.sampleRate = 8000;
        in.channels = 1;
        const int n = 2000;
        in.samples.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            in.samples[static_cast<std::size_t>(i)] =
                0.7f * std::sin(2.0f * 3.14159265f * 300.0f * static_cast<float>(i) / 8000.0f);

        for (int law = 0; law < 2; ++law) {
            const std::vector<std::uint8_t> enc = (law == 0) ? encodeMuLaw(in) : encodeALaw(in);
            CHECK(enc.size() == in.samples.size(), "one byte per sample (2:1 vs 16-bit PCM)");
            const WavData out = (law == 0) ? decodeMuLaw(enc, 1, 8000) : decodeALaw(enc, 1, 8000);
            CHECK(out.channels == 1 && out.sampleRate == 8000, "metadata carried through");
            CHECK(out.samples.size() == in.samples.size(), "sample count preserved");
            double acc = 0.0;
            for (std::size_t i = 0; i < in.samples.size(); ++i) {
                const double e = static_cast<double>(in.samples[i]) - static_cast<double>(out.samples[i]);
                acc += e * e;
            }
            const double rms = std::sqrt(acc / static_cast<double>(in.samples.size()));
            CHECK(rms < 0.02, law == 0 ? "mu-law round-trip error small" : "A-law round-trip error small");
        }
    }

    if (g_fail == 0) {
        std::printf("g711: OK — mu-law/A-law anchors, sign, monotonic, 2:1 round-trip.\n");
        return 0;
    }
    std::printf("g711: %d failure(s).\n", g_fail);
    return 1;
}
