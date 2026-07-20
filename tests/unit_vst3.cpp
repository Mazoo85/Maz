// Unit test for VST3 plugin hosting — load a real .vst3 module (built from plugins/example_vst3),
// instantiate it through the Vst3Host, and confirm it processes / modulates audio. No audio device.

#include "maz/audio/Vst3Host.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

} // namespace

int main() {
#ifndef MAZ_TEST_VST3
    std::printf("[FAIL] MAZ_TEST_VST3 not defined\n");
    return 1;
#else
    const int sr = 48000;
    audio::Vst3Host host;
    std::string err;

    check(!host.loaded(), "host starts empty");
    const bool ok = host.load(MAZ_TEST_VST3, sr, 512, &err);
    check(ok, "loads the example .vst3 via the VST3 COM API");
    if (!ok) {
        std::printf("  load error: %s\n", err.c_str());
        std::printf("FAILURES: 1 failure(s)\n");
        return 1;
    }
    check(host.loaded(), "reports loaded");
    check(host.pluginName() == std::string("Maz Tremolo"), "reads the VST3 class name");

    // A steady signal through the tremolo should have its amplitude modulated (dip toward zero).
    host.setEnabled(true);
    std::vector<float> buf(static_cast<size_t>(sr) * 2, 0.5f); // 1 s, both channels 0.5
    host.process(buf.data(), sr, sr);

    float minv = 1.0f, maxv = 0.0f;
    for (int i = 0; i < sr; ++i) {
        minv = std::min(minv, buf[static_cast<size_t>(i) * 2]);
        maxv = std::max(maxv, buf[static_cast<size_t>(i) * 2]);
    }
    check(minv < 0.1f, "VST3 tremolo modulates the amplitude down");
    check(maxv > 0.45f, "VST3 tremolo leaves peaks near the input");

    // The tremolo is a pure audio effect — it declares no event/note input bus, so the host reports it
    // is not hostable as an instrument (the routing signal that separates effects from instruments).
    check(!host.hasEventInput(), "the tremolo effect exposes no VST3 event input");

    // A disabled host is transparent.
    audio::Vst3Host host2;
    check(host2.load(MAZ_TEST_VST3, sr, 512, &err), "reloads for the bypass check");
    host2.setEnabled(false);
    std::vector<float> dry(2000, 0.5f);
    const std::vector<float> ref = dry;
    host2.process(dry.data(), 1000, sr);
    bool unchanged = true;
    for (size_t i = 0; i < dry.size(); ++i) {
        if (std::fabs(dry[i] - ref[i]) > 1e-6f) {
            unchanged = false;
            break;
        }
    }
    check(unchanged, "a disabled VST3 host passes audio through untouched");

    host.unload();
    check(!host.loaded(), "unloads cleanly");

    audio::Vst3Host bad;
    check(!bad.load("/nonexistent/missing.vst3", sr, 512, &err), "loading a missing .vst3 fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
#endif
}
