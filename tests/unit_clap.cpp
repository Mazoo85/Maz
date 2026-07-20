// Unit test for CLAP plugin hosting — load a real .clap plugin (built from plugins/example_clap),
// activate it through the ClapHost, and confirm it processes/ modulates audio. No audio device.

#include "maz/audio/Automation.hpp"
#include "maz/audio/ClapHost.hpp"
#include "maz/audio/Mixer.hpp"
#include "maz/audio/ProjectIO.hpp"
#include "maz/audio/Sequencer.hpp"

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

        // Panic: hold notes, then allNotesOff() must release them so nothing hangs.
        inst.noteOn(60, 1.0f);
        inst.noteOn(64, 1.0f);
        inst.allNotesOff();
        std::vector<float> panic(static_cast<size_t>(block) * 2, 0.0f);
        inst.process(panic.data(), block, sr);
        check(energy(panic) == 0.0, "allNotesOff releases held notes (no hang)");
        inst.unload();
    } else {
        std::printf("  instrument load error: %s\n", err.c_str());
    }

    // Route a hosted CLAP instrument onto a sequencer channel: the lead lane drives the plugin and its
    // audio shows up in the lead stem. Mute the built-in lead synth so the energy is the plugin's.
    {
        audio::Sequencer seq;
        seq.synth().setGain(0.0f); // built-in lead silent → lead stem energy is the plugin's alone
        seq.roll().addNote(audio::Note{0, 4, 69, 1.0f});
        const bool lok = seq.loadLeadPlugin(MAZ_TEST_CLAP_INSTRUMENT, sr);
        check(lok && seq.leadPluginLoaded(), "sequencer loads a CLAP instrument on the lead lane");
        seq.play();
        const int fr = sr / 4;
        std::vector<float> d(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> l(static_cast<size_t>(fr) * 2, 0.0f);
        std::vector<float> b(static_cast<size_t>(fr) * 2, 0.0f);
        seq.renderStems(d.data(), l.data(), b.data(), fr, sr);
        double le = 0.0;
        for (float v : l) {
            le += static_cast<double>(v) * static_cast<double>(v);
        }
        check(le > 0.0, "the hosted lead instrument is audible in the lead stem (built-in synth muted)");

        // The lead plugin's path round-trips through the project file (reloaded on load).
        audio::Mixer mx;
        audio::Automation autom;
        const std::string proj = audio::saveProjectToString(seq, mx, autom);
        audio::Sequencer seq2;
        audio::Mixer mx2;
        audio::Automation autom2;
        audio::loadProjectFromString(proj, seq2, mx2, autom2);
        check(seq2.leadPluginLoaded(), "the lead plugin path round-trips through the project");
    }
#endif

    audio::ClapHost bad;
    check(!bad.load("/nonexistent/missing.clap", sr, 512, &err), "loading a missing .clap fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
#endif
}
