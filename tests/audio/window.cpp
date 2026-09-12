// tests/audio/window.cpp — verifies the DSP window functions (audio Window.hpp).
// Ground truths, deterministic (closed-form window values, no <random>, no clock):
//   * ENDPOINTS: Hann/Bartlett taper to 0 at the ends; Hamming ends at 0.08; Blackman ends at ~0;
//   * CENTRE: every taper peaks at 1.0 at the centre (odd length);
//   * SYMMETRY (airtight): w[n] == w[N-1-n] for all windows;
//   * RANGE: all values lie in [0,1];
//   * COHERENT GAIN: rectangular = 1, Hann ~= 0.5, Hamming ~= 0.54 (matches the analytic means);
//   * APPLY: applyWindow multiplies a buffer by the window pointwise.
#include "maz/audio/Window.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using maz::audio::WindowType;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    using namespace maz::audio;
    const std::size_t N = 65; // odd so there is an exact centre sample

    // --- 1. Endpoints. ---
    {
        CHECK(std::fabs(windowValue(WindowType::Hann, 0, N)) < 1e-6f, "Hann starts at 0");
        CHECK(std::fabs(windowValue(WindowType::Hann, N - 1, N)) < 1e-6f, "Hann ends at 0");
        CHECK(std::fabs(windowValue(WindowType::Bartlett, 0, N)) < 1e-6f, "Bartlett starts at 0");
        CHECK(std::fabs(windowValue(WindowType::Hamming, 0, N) - 0.08f) < 1e-5f, "Hamming ends at 0.08");
        CHECK(std::fabs(windowValue(WindowType::Blackman, 0, N)) < 1e-5f, "Blackman starts at ~0");
        CHECK(std::fabs(windowValue(WindowType::Rectangular, 0, N) - 1.0f) < 1e-6f, "rectangular is flat 1");
    }

    // --- 2. Centre peaks at 1. ---
    {
        const std::size_t c = (N - 1) / 2;
        for (WindowType t : {WindowType::Hann, WindowType::Hamming, WindowType::Blackman,
                             WindowType::BlackmanHarris, WindowType::Bartlett}) {
            CHECK(std::fabs(windowValue(t, c, N) - 1.0f) < 1e-4f, "window peaks at 1.0 at the centre");
        }
    }

    // --- 3. Symmetry + range over all windows and indices. ---
    {
        float worstSym = 0.0f, minV = 1e9f, maxV = -1e9f;
        for (WindowType t : {WindowType::Hann, WindowType::Hamming, WindowType::Blackman,
                             WindowType::BlackmanHarris, WindowType::Bartlett, WindowType::Rectangular}) {
            for (std::size_t n = 0; n < N; ++n) {
                const float v = windowValue(t, n, N);
                worstSym = std::max(worstSym, std::fabs(v - windowValue(t, N - 1 - n, N)));
                minV = std::min(minV, v);
                maxV = std::max(maxV, v);
            }
        }
        CHECK(worstSym < 1e-6f, "every window is symmetric w[n] == w[N-1-n]");
        CHECK(minV >= -1e-4f && maxV <= 1.0f + 1e-6f, "all window values lie in [0,1]");
    }

    // --- 4. Coherent gain matches the known means. ---
    {
        CHECK(std::fabs(coherentGain(WindowType::Rectangular, 1024) - 1.0f) < 1e-6f, "rectangular gain is 1");
        CHECK(std::fabs(coherentGain(WindowType::Hann, 1024) - 0.5f) < 2e-3f, "Hann coherent gain ~= 0.5");
        CHECK(std::fabs(coherentGain(WindowType::Hamming, 1024) - 0.54f) < 2e-3f, "Hamming coherent gain ~= 0.54");
        // Coherent gain equals the average window value by construction — cross-check by hand for a small N.
        double s = 0.0;
        for (std::size_t n = 0; n < 8; ++n) {
            s += static_cast<double>(windowValue(WindowType::Blackman, n, 8));
        }
        CHECK(std::fabs(coherentGain(WindowType::Blackman, 8) - static_cast<float>(s / 8.0)) < 1e-6f,
              "coherentGain is the mean of the window samples");
    }

    // --- 5. applyWindow multiplies pointwise. ---
    {
        std::vector<float> buf(N, 2.0f);
        applyWindow(buf, WindowType::Hann);
        float worst = 0.0f;
        for (std::size_t n = 0; n < N; ++n) {
            worst = std::max(worst, std::fabs(buf[n] - 2.0f * windowValue(WindowType::Hann, n, N)));
        }
        CHECK(worst < 1e-6f, "applyWindow multiplies the buffer by the window");
        CHECK(std::fabs(buf[0]) < 1e-6f && std::fabs(buf[N - 1]) < 1e-6f, "windowed buffer tapers to 0 at the ends");
    }

    if (g_fail == 0) {
        std::printf("window: OK — endpoints, centre, symmetry, range, coherent gain, apply.\n");
        return 0;
    }
    std::printf("window: %d failure(s).\n", g_fail);
    return 1;
}
