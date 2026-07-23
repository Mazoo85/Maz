// Unit test for VST3 plugin hosting — load a real .vst3 module (built from plugins/example_vst3),
// instantiate it through the Vst3Host, and confirm it processes / modulates audio. No audio device.

#include "maz/audio/Automation.hpp"
#include "maz/audio/Mixer.hpp"
#include "maz/audio/ProjectIO.hpp"
#include "maz/audio/Sequencer.hpp"
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

#ifdef MAZ_TEST_VST3_INSTRUMENT
    // The example instrument declares an event/note input bus, so the host detects it as hostable as
    // an instrument (the true side of the hasEventInput() routing signal — the tremolo above is false).
    audio::Vst3Host inst;
    const bool iok = inst.load(MAZ_TEST_VST3_INSTRUMENT, sr, 512, &err);
    check(iok, "loads the example VST3 instrument");
    if (iok) {
        check(inst.pluginName() == std::string("Maz Synth"), "reads the instrument class name");
        check(inst.hasEventInput(), "the instrument exposes a VST3 event input bus");

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
        check(energy(on) > 0.0, "hosted VST3 instrument produces sound from a note-on event");

        // Note-off then render again: the synth gates off, so the block goes silent.
        inst.noteOff(69);
        std::vector<float> off(static_cast<size_t>(block) * 2, 0.0f);
        inst.process(off.data(), block, sr);
        check(energy(off) == 0.0, "a note-off silences the hosted VST3 instrument");

        // Panic: hold notes, then allNotesOff() must release them so nothing hangs.
        inst.noteOn(60, 1.0f);
        inst.noteOn(64, 1.0f);
        inst.allNotesOff();
        std::vector<float> panic(static_cast<size_t>(block) * 2, 0.0f);
        inst.process(panic.data(), block, sr);
        check(energy(panic) == 0.0, "allNotesOff releases held VST3 notes (no hang)");

        // Parameter automation: the synth applies a "Gain" parameter (id 0) delivered via VST3
        // inputParameterChanges. Setting it to 0 mutes the output; 1 restores it — proving host-queued
        // parameter changes reach the plugin's processor.
        inst.setParam(0, 0.0);
        inst.noteOn(69, 1.0f);
        std::vector<float> vquiet(static_cast<size_t>(block) * 2, 0.0f);
        inst.process(vquiet.data(), block, sr);
        check(energy(vquiet) == 0.0, "a Gain=0 parameter change mutes the hosted VST3 instrument");
        inst.setParam(0, 1.0);
        std::vector<float> vloud(static_cast<size_t>(block) * 2, 0.0f);
        inst.process(vloud.data(), block, sr);
        check(energy(vloud) > 0.0, "a Gain=1 parameter change restores the hosted VST3 instrument");
        inst.allNotesOff();

        inst.unload();
    } else {
        std::printf("  instrument load error: %s\n", err.c_str());
    }

    // Route a hosted VST3 instrument onto a sequencer channel: the lead lane drives the plugin and its
    // audio shows up in the lead stem. Mute the built-in lead synth so the energy is the plugin's. The
    // Sequencer picks the VST3 host from the ".vst3" path extension (CLAP for anything else).
    {
        audio::Sequencer seq;
        seq.synth().setGain(0.0f); // built-in lead silent → lead stem energy is the plugin's alone
        seq.roll().addNote(audio::Note{0, 4, 69, 1.0f});
        const bool lok = seq.loadLeadPlugin(MAZ_TEST_VST3_INSTRUMENT, sr);
        check(lok && seq.leadPluginLoaded(), "sequencer loads a VST3 instrument on the lead lane");
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
        check(le > 0.0, "the hosted VST3 lead instrument is audible in the lead stem (built-in muted)");

        // The lead plugin's path round-trips through the project (reloaded on load, VST3 host again).
        audio::Mixer mx;
        audio::Automation autom;
        const std::string proj = audio::saveProjectToString(seq, mx, autom);
        audio::Sequencer seq2;
        audio::Mixer mx2;
        audio::Automation autom2;
        audio::loadProjectFromString(proj, seq2, mx2, autom2);
        check(seq2.leadPluginLoaded(), "the VST3 lead plugin path round-trips through the project");
    }
#endif

    audio::Vst3Host bad;
    check(!bad.load("/nonexistent/missing.vst3", sr, 512, &err), "loading a missing .vst3 fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
#endif
}
