// Unit test for MIDI export — write a Standard MIDI File for a known pattern and verify the header,
// track chunk, and that note-on events are present. No audio device.

#include "maz/audio/MidiReader.hpp"
#include "maz/audio/MidiWriter.hpp"
#include "maz/audio/Sequencer.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
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

bool tagAt(const std::vector<uint8_t>& b, size_t off, const char* t) {
    return b.size() >= off + 4 && b[off] == static_cast<uint8_t>(t[0]) &&
           b[off + 1] == static_cast<uint8_t>(t[1]) && b[off + 2] == static_cast<uint8_t>(t[2]) &&
           b[off + 3] == static_cast<uint8_t>(t[3]);
}

} // namespace

int main() {
    audio::Sequencer seq;
    seq.setStep(0, 0, true);  // kick
    seq.setStep(1, 4, true);  // snare
    seq.roll().addNote(audio::Note{0, 4, 60, 0.9f});
    seq.roll().addNote(audio::Note{8, 4, 67, 0.8f});
    seq.roll2().addNote(audio::Note{0, 4, 36, 0.9f}); // a bass note — must reach the export too
    const int midiExtra = seq.addInstrumentChannel();  // an extra instrument channel
    seq.instrumentRoll(midiExtra).addNote(audio::Note{0, 4, 72, 0.8f}); // must export on its own channel
    seq.setBpm(140.0); // a non-default tempo — must survive the round-trip

    const std::string path = "unit_midi_out.mid";
    std::string err;
    check(audio::writeMidi(path, seq, 96, &err), "writeMidi succeeds");

    std::ifstream f(path, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    check(b.size() > 22, "file has header + track");
    check(tagAt(b, 0, "MThd"), "starts with MThd");
    // Header: format 0, ntracks 1, division 96.
    check(b.size() > 13 && b[8] == 0 && b[9] == 0, "format 0");
    check(b[10] == 0 && b[11] == 1, "one track");
    check(b[12] == 0 && b[13] == 96, "division = 96 ppq");
    check(tagAt(b, 14, "MTrk"), "track chunk follows");

    // At least one note-on (0x90 melody or 0x99 drums), a bass note-on on channel 1 (0x91), and an
    // end-of-track meta event.
    bool hasNoteOn = false, hasBass = false, hasEot = false, hasExtra = false;
    for (size_t i = 22; i + 2 < b.size(); ++i) {
        if (b[i] == 0x90 || b[i] == 0x99) {
            hasNoteOn = true;
        }
        if (b[i] == 0x91) {
            hasBass = true;
        }
        if (b[i] == 0x92 && b[i + 1] == 72) { // extra channel 0 → MIDI channel 2, note 72
            hasExtra = true;
        }
        if (b[i] == 0xFF && b[i + 1] == 0x2F && b[i + 2] == 0x00) {
            hasEot = true;
        }
    }
    check(hasNoteOn, "contains note-on events");
    check(hasBass, "the bass piano-roll is exported on MIDI channel 1");
    check(hasExtra, "an extra instrument channel is exported on its own MIDI channel (2)");
    check(hasEot, "ends with an end-of-track meta event");

    // --- Import round-trip: read the file we just wrote back into a fresh sequencer ---------------
    audio::Sequencer in;
    check(audio::readMidi(path, in, &err), "readMidi succeeds");
    // The two melodic notes come back onto the lead roll with their pitches, timing, and velocity.
    const auto& notes = in.roll().notes();
    check(notes.size() == 2, "both melodic notes import");
    bool gotC = false, gotG = false;
    for (const audio::Note& n : notes) {
        if (n.pitch == 60 && n.startStep == 0 && n.lengthSteps == 4) {
            gotC = true;
        }
        if (n.pitch == 67 && n.startStep == 8 && n.lengthSteps == 4) {
            gotG = true;
        }
    }
    check(gotC && gotG, "imported notes keep pitch, start, and length");
    // The bass note round-trips onto the bass roll (channel 1), not the lead.
    const auto& bassNotes = in.roll2().notes();
    check(bassNotes.size() == 1 && bassNotes[0].pitch == 36 && bassNotes[0].startStep == 0,
          "the bass note round-trips onto the bass roll");
    // The extra-channel note round-trips onto an imported extra instrument channel (symmetric routing).
    check(in.instrumentChannelCount() >= 1 && in.instrumentRoll(0).notes().size() == 1 &&
              in.instrumentRoll(0).notes()[0].pitch == 72,
          "the extra-channel note round-trips onto an imported instrument channel");
    // The tempo meta event carries the project BPM through the file.
    check(std::fabs(in.bpm() - 140.0) < 0.5, "the project tempo round-trips via the MIDI tempo meta");
    // The two drum hits come back on the grid.
    check(in.step(0, 0) && in.step(1, 4), "imported drum hits land on the grid");
    check(!in.step(0, 1), "unset drum steps stay off after import");

    // Drums export by TYPE, not channel position: a reassigned channel emits its drum's GM note and
    // round-trips when the target kit has that drum.
    {
        audio::Sequencer cb;
        cb.setChannelType(0, audio::Drum::Cowbell);
        cb.setStep(0, 2, true);
        const std::string cbPath = "unit_midi_cowbell.mid";
        check(audio::writeMidi(cbPath, cb, 96, &err), "writeMidi (cowbell kit) succeeds");
        std::ifstream cf(cbPath, std::ios::binary);
        std::vector<uint8_t> cbytes((std::istreambuf_iterator<char>(cf)),
                                    std::istreambuf_iterator<char>());
        bool gmCowbell = false;
        for (size_t i = 22; i + 1 < cbytes.size(); ++i) {
            if (cbytes[i] == 0x99 && cbytes[i + 1] == 56) { // GM cowbell = 56
                gmCowbell = true;
            }
        }
        check(gmCowbell, "a cowbell channel exports the GM cowbell note (56), not a position default");

        audio::Sequencer cbIn;
        cbIn.setChannelType(0, audio::Drum::Cowbell);
        check(audio::readMidi(cbPath, cbIn, &err), "readMidi (cowbell kit) succeeds");
        check(cbIn.step(0, 2), "the cowbell hit round-trips onto the matching-type channel");
    }

    // Arrangement markers export as MIDI text marker meta-events (FF 06 <len> <text>), and a file
    // containing them still parses.
    {
        audio::Sequencer mk;
        mk.addMarker(0, "Chorus");
        const std::string mkPath = "unit_midi_marker.mid";
        check(audio::writeMidi(mkPath, mk, 96, &err), "writeMidi (marker) succeeds");
        std::ifstream mf(mkPath, std::ios::binary);
        std::vector<uint8_t> mb((std::istreambuf_iterator<char>(mf)), std::istreambuf_iterator<char>());
        const std::string want = "Chorus";
        const uint8_t wantLen = static_cast<uint8_t>(want.size());
        bool found = false;
        for (size_t i = 0; i + 3 + want.size() <= mb.size(); ++i) {
            if (mb[i] == 0xFF && mb[i + 1] == 0x06 && mb[i + 2] == wantLen) {
                bool match = true;
                for (size_t k = 0; k < want.size(); ++k) {
                    if (mb[i + 3 + k] != static_cast<uint8_t>(want[k])) {
                        match = false;
                    }
                }
                found = found || match;
            }
        }
        check(found, "an arrangement marker exports as a MIDI marker meta-event (FF 06)");
        audio::Sequencer mkIn;
        check(audio::readMidi(mkPath, mkIn, &err), "readMidi tolerates the marker meta-event");
        check(mkIn.markerCount() == 1 && mkIn.marker(0).bar == 0 && mkIn.marker(0).name == "Chorus",
              "a marker imports back from the MIDI meta-event");

        // A marker at a later bar round-trips to the same bar (tick = bar * numSteps * ticksPerStep).
        audio::Sequencer mk2;
        mk2.addMarker(3, "Bridge");
        const std::string mk2Path = "unit_midi_marker2.mid";
        check(audio::writeMidi(mk2Path, mk2, 96, &err), "writeMidi (bar-3 marker) succeeds");
        audio::Sequencer mk2In;
        check(audio::readMidi(mk2Path, mk2In, &err), "readMidi (bar-3 marker) succeeds");
        check(mk2In.markerCount() == 1 && mk2In.marker(0).bar == 3 && mk2In.marker(0).name == "Bridge",
              "a marker at bar 3 round-trips to the same bar through MIDI");
    }

    // Time-signature meta-event (FF 58): a 3-beats-per-bar project (12 steps at 4 steps/beat) writes a
    // 3/4 signature; a default 16-step/4 project writes 4/4. Importing DAWs need this to align bars.
    {
        auto tsNumerator = [&](int steps, int spb) -> int {
            audio::Sequencer ts;
            ts.setNumSteps(steps);
            ts.setStepsPerBeat(spb);
            const std::string tp = "unit_midi_timesig.mid";
            std::string e2;
            if (!audio::writeMidi(tp, ts, 96, &e2)) {
                return -1;
            }
            std::ifstream tf(tp, std::ios::binary);
            std::vector<uint8_t> tb((std::istreambuf_iterator<char>(tf)),
                                    std::istreambuf_iterator<char>());
            for (size_t i = 0; i + 4 < tb.size(); ++i) {
                if (tb[i] == 0xFF && tb[i + 1] == 0x58 && tb[i + 2] == 0x04) {
                    return static_cast<int>(tb[i + 3]); // numerator (beats per bar)
                }
            }
            return -1;
        };
        check(tsNumerator(16, 4) == 4, "a 16-step/4 project exports a 4/4 time signature");
        check(tsNumerator(12, 4) == 3, "a 12-step/4 project exports a 3/4 time signature");
        audio::Sequencer tsIn;
        std::string e3;
        check(audio::readMidi("unit_midi_timesig.mid", tsIn, &e3),
              "readMidi tolerates the time-signature meta-event");
    }

    // Arrangement export: a 2-entry playlist writes both patterns back to back, each offset by one
    // pattern length.
    {
        audio::Sequencer song;
        const int n = song.numSteps();
        song.selectPattern(0);
        song.roll().addNote(audio::Note{0, 2, 60, 1.0f}); // pattern 0, step 0
        const int p1 = song.addPattern();
        song.selectPattern(p1);
        song.roll().addNote(audio::Note{2, 2, 64, 1.0f}); // pattern 1, step 2
        song.setPlaylist({0, p1});

        const std::string sp = "unit_midi_song.mid";
        check(audio::writeMidi(sp, song, 96, &err, true), "arrangement MIDI export succeeds");
        audio::Sequencer in2;
        check(audio::readMidi(sp, in2, &err), "arrangement MIDI reads back");
        const auto& ns = in2.roll().notes();
        bool got0 = false, gotOffset = false;
        for (const audio::Note& nn : ns) {
            if (nn.pitch == 60 && nn.startStep == 0) {
                got0 = true;
            }
            if (nn.pitch == 64 && nn.startStep == n + 2) {
                gotOffset = true;
            }
        }
        check(ns.size() == 2 && got0 && gotOffset,
              "arrangement export writes playlist patterns back to back (2nd offset by a pattern)");
    }

    // Clip-song arrangement export: the 2-D clip timeline exports each clip at its own bar offset
    // (a clip on bar 2 lands two pattern-lengths into the file), honouring clip-song mode.
    {
        audio::Sequencer song;
        const int n = song.numSteps();
        song.selectPattern(0);
        song.roll().addNote(audio::Note{0, 2, 60, 1.0f}); // pattern 0 lead note at step 0
        const int p1 = song.addPattern();
        song.selectPattern(p1);
        song.roll().addNote(audio::Note{3, 2, 67, 1.0f}); // pattern 1 lead note at step 3
        song.selectPattern(0);
        song.addClip(0, 0, 0);  // pattern 0 at bar 0
        song.addClip(p1, 2, 0); // pattern 1 at bar 2
        song.setSongUsesClips(true);

        const std::string cp = "unit_midi_clips.mid";
        check(audio::writeMidi(cp, song, 96, &err, true), "clip-arrangement MIDI export succeeds");
        audio::Sequencer cin;
        check(audio::readMidi(cp, cin, &err), "clip-arrangement MIDI reads back");
        const auto& cns = cin.roll().notes();
        bool at0 = false, at2 = false;
        for (const audio::Note& nn : cns) {
            if (nn.pitch == 60 && nn.startStep == 0) {
                at0 = true;
            }
            if (nn.pitch == 67 && nn.startStep == 2 * n + 3) {
                at2 = true;
            }
        }
        check(cns.size() == 2 && at0 && at2,
              "clip export places each clip's pattern at its bar offset (bar 2 → two pattern lengths in)");
    }

    // A note held to end-of-track (no note-off) is still imported, ended at the final tick.
    {
        auto putBE = [](std::vector<uint8_t>& v, uint32_t x, int nb) {
            for (int k = nb - 1; k >= 0; --k) {
                v.push_back(static_cast<uint8_t>((x >> (8 * k)) & 0xFFu));
            }
        };
        const std::vector<uint8_t> trk = {
            0x00, 0x90, 0x3C, 0x64,       // tick 0: note-on ch0, note 60, vel 100
            0x81, 0x70, 0xFF, 0x2F, 0x00, // +240 ticks: end-of-track (NO note-off)
        };
        std::vector<uint8_t> mid;
        mid.insert(mid.end(), {'M', 'T', 'h', 'd'});
        putBE(mid, 6, 4);
        putBE(mid, 0, 2); // format 0
        putBE(mid, 1, 2); // one track
        putBE(mid, 96, 2); // 96 ppq
        mid.insert(mid.end(), {'M', 'T', 'r', 'k'});
        putBE(mid, static_cast<uint32_t>(trk.size()), 4);
        mid.insert(mid.end(), trk.begin(), trk.end());
        std::ofstream of("unit_midi_hanging.mid", std::ios::binary);
        of.write(reinterpret_cast<const char*>(mid.data()), static_cast<std::streamsize>(mid.size()));
        of.close();

        audio::Sequencer hn;
        check(audio::readMidi("unit_midi_hanging.mid", hn, &err), "a MIDI with a hanging note reads");
        const auto& hns = hn.roll().notes();
        check(hns.size() == 1 && hns[0].pitch == 60 && hns[0].startStep == 0 &&
                  hns[0].lengthSteps > 1,
              "a note held to end-of-track is imported (ended at the track's end)");
    }

    // Transpose (global + per-pattern) is baked into the exported pitches, so the MIDI file matches
    // what playback sounds — not the raw stored note numbers.
    {
        audio::Sequencer ts;
        ts.setTranspose(5);          // global +5 st
        ts.setPatternTranspose(7);   // this pattern +7 st → effective +12 (an octave)
        ts.roll().addNote(audio::Note{0, 4, 60, 0.9f}); // C4 → should export as C5 (72)
        const std::string tp = "unit_midi_transpose.mid";
        check(audio::writeMidi(tp, ts, 96, &err), "writeMidi (transposed) succeeds");
        audio::Sequencer tin;
        check(audio::readMidi(tp, tin, &err), "readMidi (transposed) succeeds");
        bool found72 = false, found60 = false;
        for (const audio::Note& n : tin.roll().notes()) {
            if (n.pitch == 72) {
                found72 = true;
            }
            if (n.pitch == 60) {
                found60 = true;
            }
        }
        check(found72 && !found60,
              "MIDI export bakes in the global + per-pattern transpose (C4 +12 → C5)");
    }

    // A truncated file whose MTrk length claims far more bytes than are present must not over-read
    // past the buffer (regression: trackEnd was the unclamped claimed length, so the event loop and
    // the tempo-meta read walked off the end of a corrupt/truncated import).
    {
        auto putBE = [](std::vector<uint8_t>& v, uint32_t x, int nb) {
            for (int k = nb - 1; k >= 0; --k) {
                v.push_back(static_cast<uint8_t>((x >> (8 * k)) & 0xFFu));
            }
        };
        std::vector<uint8_t> mid;
        mid.insert(mid.end(), {'M', 'T', 'h', 'd'});
        putBE(mid, 6, 4);
        putBE(mid, 0, 2);  // format 0
        putBE(mid, 1, 2);  // one track
        putBE(mid, 96, 2); // 96 ppq
        mid.insert(mid.end(), {'M', 'T', 'r', 'k'});
        putBE(mid, 0xFFFFFFFFu, 4); // claims a 4 GB track...
        // ...but there is NO track data at all (the file ends here).
        std::ofstream of("unit_midi_truncated.mid", std::ios::binary);
        of.write(reinterpret_cast<const char*>(mid.data()), static_cast<std::streamsize>(mid.size()));
        of.close();

        audio::Sequencer tr;
        // The only requirement is that this returns without an out-of-bounds read / crash.
        (void)audio::readMidi("unit_midi_truncated.mid", tr, &err);
        check(tr.roll().notes().empty(), "a truncated MIDI track yields no notes and does not over-read");
    }

    // A non-MIDI file fails cleanly.
    audio::Sequencer bad;
    check(!audio::readMidi("/nonexistent/missing.mid", bad, &err), "reading a missing file fails");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
