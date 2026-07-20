// Unit test for native plugin hosting — load the example tremolo plugin (a real .so via dlopen) and
// confirm the host runs it and it modulates the signal. No audio device.

#include "maz/audio/PluginHost.hpp"

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
#ifndef MAZ_TEST_PLUGIN
    std::printf("[FAIL] MAZ_TEST_PLUGIN not defined\n");
    return 1;
#else
    const int sr = 48000;
    audio::PluginHost host;
    std::string err;

    check(!host.loaded(), "host starts empty");
    const bool ok = host.load(MAZ_TEST_PLUGIN, sr, &err);
    check(ok, "loads the example plugin .so");
    if (!ok) {
        std::printf("  load error: %s\n", err.c_str());
        std::printf("FAILURES: 1 failure(s)\n");
        return 1;
    }
    check(host.loaded(), "reports loaded");
    check(host.pluginName() == std::string("Example Tremolo"), "reads the plugin's name");
    check(host.paramCount() == 2, "reads the parameter count");

    // A steady full-scale signal; the tremolo should modulate its amplitude (max depth) so the
    // processed signal dips below the input at some points.
    host.setEnabled(true);
    host.setParam(0, 1.0f); // full depth
    host.setParam(1, 8.0f); // 8 Hz
    std::vector<float> buf(static_cast<size_t>(sr) * 2, 0.5f); // 1 s, both channels at 0.5
    host.process(buf.data(), sr, sr);

    float minv = 1.0f, maxv = 0.0f;
    for (int i = 0; i < sr; ++i) {
        minv = std::min(minv, buf[static_cast<size_t>(i) * 2]);
        maxv = std::max(maxv, buf[static_cast<size_t>(i) * 2]);
    }
    check(minv < 0.2f, "tremolo dips the amplitude (modulation is applied)");
    check(maxv > 0.45f, "tremolo leaves peaks near the input level");

    // Out-of-range parameter indices are ignored (never reach the plugin's fixed param array).
    host.setParam(-1, 1.0f);
    host.setParam(999, 1.0f);
    host.setParam(host.paramCount(), 1.0f); // one past the last valid index
    std::vector<float> buf2(static_cast<size_t>(sr) * 2, 0.5f);
    host.process(buf2.data(), sr, sr); // must not crash / read out of bounds
    bool finite = true;
    for (float v : buf2) {
        if (!std::isfinite(v)) {
            finite = false;
        }
    }
    check(finite, "out-of-range setParam indices are ignored (no crash / OOB)");

    // A missing file fails cleanly.
    audio::PluginHost bad;
    check(!bad.load("/nonexistent/missing_plugin.so", sr, &err), "loading a missing plugin fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
#endif
}
