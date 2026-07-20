// Unit test for CLAP plugin hosting — load a real .clap plugin (built from plugins/example_clap),
// activate it through the ClapHost, and confirm it processes/ modulates audio. No audio device.

#include "maz/audio/ClapHost.hpp"

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
#ifndef MAZ_TEST_CLAP
    std::printf("[FAIL] MAZ_TEST_CLAP not defined\n");
    return 1;
#else
    const int sr = 48000;
    audio::ClapHost host;
    std::string err;

    check(!host.loaded(), "host starts empty");
    const bool ok = host.load(MAZ_TEST_CLAP, sr, 512, &err);
    check(ok, "loads the example .clap via the CLAP ABI");
    if (!ok) {
        std::printf("  load error: %s\n", err.c_str());
        std::printf("FAILURES: 1 failure(s)\n");
        return 1;
    }
    check(host.loaded(), "reports loaded");
    check(host.pluginName() == std::string("CJC CLAP Tremolo"), "reads the CLAP plugin name");

    // A steady signal through the tremolo should have its amplitude modulated (dip below input).
    host.setEnabled(true);
    std::vector<float> buf(static_cast<size_t>(sr) * 2, 0.5f); // 1 s, both channels 0.5
    host.process(buf.data(), sr, sr);

    float minv = 1.0f, maxv = 0.0f;
    for (int i = 0; i < sr; ++i) {
        minv = std::min(minv, buf[static_cast<size_t>(i) * 2]);
        maxv = std::max(maxv, buf[static_cast<size_t>(i) * 2]);
    }
    check(minv < 0.2f, "CLAP tremolo modulates the amplitude down");
    check(maxv > 0.45f, "CLAP tremolo leaves peaks near the input");

    // The tremolo is an audio effect — no note input ports.
    check(!host.hasNotePorts(), "the tremolo effect exposes no note ports");

    host.unload();
    check(!host.loaded(), "unloads cleanly");

#ifdef MAZ_TEST_CLAP_INSTRUMENT
    // The example instrument declares a note input port and a synth name — the host detects it as an
    // instrument (the routing signal for hosting a plugin as a channel synth).
    audio::ClapHost inst;
    const bool iok = inst.load(MAZ_TEST_CLAP_INSTRUMENT, sr, 512, &err);
    check(iok, "loads the example CLAP instrument");
    if (iok) {
        check(inst.pluginName() == std::string("CJC CLAP Synth"), "reads the instrument name");
        check(inst.hasNotePorts(), "the instrument exposes an input note port");

        // Send a note and pull audio: the hosted instrument must synthesise sound from the note event.
        inst.setEnabled(true);
        auto energy = [](const std::vector<float>& b) {
            double e = 0.0;
            for (float v : b) {
                e += static_cast<double>(v) * static_cast<double>(v);
            }
            return e;
        };
        const int block = sr / 10; // 0.1 s
        inst.noteOn(69, 1.0f);     // A4
        std::vector<float> on(static_cast<size_t>(block) * 2, 0.0f);
        inst.process(on.data(), block, sr);
        check(energy(on) > 0.0, "hosted instrument produces sound from a note-on event");

        // Note-off then render again: the synth gates off, so the block goes silent.
        inst.noteOff(69);
        std::vector<float> off(static_cast<size_t>(block) * 2, 0.0f);
        inst.process(off.data(), block, sr);
        check(energy(off) == 0.0, "a note-off silences the hosted instrument");
        inst.unload();
    } else {
        std::printf("  instrument load error: %s\n", err.c_str());
    }
#endif

    audio::ClapHost bad;
    check(!bad.load("/nonexistent/missing.clap", sr, 512, &err), "loading a missing .clap fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
#endif
}
