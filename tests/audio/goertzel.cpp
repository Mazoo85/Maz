// tests/audio/goertzel.cpp — verifies the Goertzel single-frequency detector (audio Goertzel.hpp).
// Ground truths, deterministic (synthesized tones, no <random>, no clock):
//   * MATCHES THE DFT BIN (airtight): the Goertzel magnitude equals a direct DFT sum's magnitude at the same
//     bin, for integer and fractional k — two independent formulas agreeing (Goertzel's phase is
//     block-relative, so only the magnitude is compared);
//   * PURE TONE: a sine of amplitude A on an integer bin gives magnitude A*N/2; other bins read ~0;
//   * SUPERPOSITION: with two tones summed, each target bin recovers its own amplitude;
//   * LINEARITY: scaling the signal scales the magnitude; a Hz helper maps to the right bin.
#include "maz/audio/Goertzel.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::audio::GoertzelBin;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

// Independent forward DFT bin (e^{-i...} convention).
static GoertzelBin directDftBin(const std::vector<float>& x, float k) {
    const double N = static_cast<double>(x.size());
    double re = 0.0, im = 0.0;
    for (std::size_t n = 0; n < x.size(); ++n) {
        const double a = 2.0 * 3.14159265358979324 * static_cast<double>(k) * static_cast<double>(n) / N;
        re += static_cast<double>(x[n]) * std::cos(a);
        im -= static_cast<double>(x[n]) * std::sin(a);
    }
    return GoertzelBin{static_cast<float>(re), static_cast<float>(im)};
}

int main() {
    using namespace maz::audio;
    const std::size_t N = 256;
    const float TAU = 6.28318530717958648f;

    // --- 1. Goertzel equals the direct DFT bin for integer and fractional k. ---
    {
        std::vector<float> x(N);
        for (std::size_t n = 0; n < N; ++n) {
            const float t = static_cast<float>(n);
            x[n] = 0.7f * std::sin(TAU * 5.0f * t / static_cast<float>(N) + 0.3f) +
                   0.4f * std::cos(TAU * 11.0f * t / static_cast<float>(N)) + 0.1f;
        }
        float worst = 0.0f;
        for (float k : {0.0f, 1.0f, 5.0f, 11.0f, 17.5f, 40.3f, 100.0f}) {
            const float g = goertzelBin(x, k).magnitude();
            const float d = directDftBin(x, k).magnitude();
            worst = std::max(worst, std::fabs(g - d));
        }
        CHECK(worst < 1e-2f, "Goertzel magnitude matches a direct DFT sum (integer and fractional k)");
    }

    // --- 2. Pure tone amplitude: A*N/2 on-bin, ~0 off-bin. ---
    {
        const float A = 0.8f;
        const int k0 = 13;
        std::vector<float> x(N);
        for (std::size_t n = 0; n < N; ++n) {
            x[n] = A * std::sin(TAU * static_cast<float>(k0) * static_cast<float>(n) / static_cast<float>(N));
        }
        const float onBin = goertzelMagnitude(x, static_cast<float>(k0));
        CHECK(std::fabs(onBin - A * static_cast<float>(N) / 2.0f) < 0.5f, "on-bin magnitude is A*N/2");
        CHECK(goertzelMagnitude(x, static_cast<float>(k0) + 3.0f) < onBin * 0.05f, "a distant bin reads ~0");
        CHECK(goertzelMagnitude(x, 2.0f) < onBin * 0.05f, "an unrelated low bin reads ~0");
    }

    // --- 3. Superposition: two tones, each bin recovers its own amplitude. ---
    {
        const float A1 = 0.6f, A2 = 0.9f;
        const int k1 = 7, k2 = 20;
        std::vector<float> x(N);
        for (std::size_t n = 0; n < N; ++n) {
            const float t = static_cast<float>(n);
            x[n] = A1 * std::sin(TAU * static_cast<float>(k1) * t / static_cast<float>(N)) +
                   A2 * std::sin(TAU * static_cast<float>(k2) * t / static_cast<float>(N));
        }
        CHECK(std::fabs(goertzelMagnitude(x, static_cast<float>(k1)) - A1 * static_cast<float>(N) / 2.0f) < 0.5f,
              "first tone amplitude recovered at its bin");
        CHECK(std::fabs(goertzelMagnitude(x, static_cast<float>(k2)) - A2 * static_cast<float>(N) / 2.0f) < 0.5f,
              "second tone amplitude recovered at its bin");
    }

    // --- 4. Linearity + Hz helper. ---
    {
        std::vector<float> x(N), x2(N);
        const int k0 = 9;
        for (std::size_t n = 0; n < N; ++n) {
            x[n] = std::sin(TAU * static_cast<float>(k0) * static_cast<float>(n) / static_cast<float>(N));
            x2[n] = 3.0f * x[n];
        }
        CHECK(std::fabs(goertzelMagnitude(x2, static_cast<float>(k0)) - 3.0f * goertzelMagnitude(x, static_cast<float>(k0))) < 1e-2f,
              "magnitude scales linearly with the signal");
        // Hz helper: sample rate 256 Hz, N=256 => bin k == frequency in Hz. Target 9 Hz -> bin 9.
        const float viaHz = goertzelMagnitudeHz(x, 9.0f, 256.0f);
        CHECK(std::fabs(viaHz - goertzelMagnitude(x, 9.0f)) < 1e-3f, "the Hz helper maps to the right bin");
    }

    if (g_fail == 0) {
        std::printf("goertzel: OK — DFT match, pure tone, superposition, linearity, Hz helper.\n");
        return 0;
    }
    std::printf("goertzel: %d failure(s).\n", g_fail);
    return 1;
}
