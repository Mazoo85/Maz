#include "maz/audio/MidiWriter.hpp"

#include "maz/audio/Sequencer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>

namespace maz::audio {

namespace {

struct MidiEvent {
    int tick;
    int order;   // -1 = meta (sorts first at a tick), 0 = note-off, 1 = note-on
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
    std::string meta = ""; // when non-empty, a text marker meta-event (FF 06); status/data ignored
};

void putBE(std::vector<uint8_t>& b, uint32_t v, int bytes) {
    for (int i = bytes - 1; i >= 0; --i) {
        b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFFu));
    }
}

// Variable-length quantity (MIDI delta-time encoding).
void putVLQ(std::vector<uint8_t>& b, uint32_t value) {
    uint8_t bytes[5];
    int n = 0;
    bytes[n++] = static_cast<uint8_t>(value & 0x7Fu);
    while ((value >>= 7) != 0) {
        bytes[n++] = static_cast<uint8_t>((value & 0x7Fu) | 0x80u);
    }
    for (int i = n - 1; i >= 0; --i) {
        b.push_back(bytes[i]);
    }
}

uint8_t vel7(float v) {
    int x = static_cast<int>(v * 127.0f + 0.5f);
    return static_cast<uint8_t>(std::clamp(x, 1, 127));
}

} // namespace

bool writeMidi(const std::string& path, Sequencer& seq, int ppq, std::string* err,
               bool arrangement) {
    if (ppq < 24) {
        ppq = 96;
    }
    const int ticksPerStep = ppq / std::max(seq.stepsPerBeat(), 1);

    std::vector<MidiEvent> events;

    // Emit the CURRENT pattern's lead (ch0), bass (ch1), and drums (ch10) shifted later by
    // `stepOffset` steps — one pattern's worth of steps when writing an arrangement back to back.
    auto emitPattern = [&](int stepOffset) {
        const int base = stepOffset * ticksPerStep;
        // Match playback: apply the global + current-pattern transpose to the melodic pitches (drums
        // are keyed by GM note, not transposed). Clamped to the valid MIDI range.
        const int tr = seq.transpose() + seq.patternTranspose();
        for (const Note& n : seq.roll().notes()) {
            const uint8_t v = vel7(n.velocity);
            const uint8_t p = static_cast<uint8_t>(std::clamp(n.pitch + tr, 0, 127));
            events.push_back({base + n.startStep * ticksPerStep, 1, 0x90, p, v});
            events.push_back({base + (n.startStep + n.lengthSteps) * ticksPerStep, 0, 0x80, p, 0});
        }
        for (const Note& n : seq.roll2().notes()) {
            const uint8_t v = vel7(n.velocity);
            const uint8_t p = static_cast<uint8_t>(std::clamp(n.pitch + tr, 0, 127));
            events.push_back({base + n.startStep * ticksPerStep, 1, 0x91, p, v});
            events.push_back({base + (n.startStep + n.lengthSteps) * ticksPerStep, 0, 0x81, p, 0});
        }
        // Extra instrument channels: each on its own MIDI channel, starting at 2 and skipping the GM
        // drum channel (9); channels past 15 clamp to 15. Transposed like the other melodic lanes.
        for (int c = 0; c < seq.instrumentChannelCount(); ++c) {
            int mc = 2 + c;
            if (mc >= 9) {
                mc += 1; // step over the reserved GM percussion channel
            }
            if (mc > 15) {
                mc = 15;
            }
            const uint8_t on = static_cast<uint8_t>(0x90 | mc);
            const uint8_t off = static_cast<uint8_t>(0x80 | mc);
            for (const Note& n : seq.instrumentRoll(c).notes()) {
                const uint8_t v = vel7(n.velocity);
                const uint8_t p = static_cast<uint8_t>(std::clamp(n.pitch + tr, 0, 127));
                events.push_back({base + n.startStep * ticksPerStep, 1, on, p, v});
                events.push_back({base + (n.startStep + n.lengthSteps) * ticksPerStep, 0, off, p, 0});
            }
        }
        // Drums map by each channel's drum TYPE, so every drum and channel exports correctly.
        for (int c = 0; c < seq.numChannels(); ++c) {
            const uint8_t note = static_cast<uint8_t>(gmNoteForDrum(seq.channelType(c)) & 0x7F);
            for (int s = 0; s < seq.numSteps(); ++s) {
                if (seq.step(c, s)) {
                    const int onTick = base + s * ticksPerStep;
                    events.push_back({onTick, 1, 0x99, note, vel7(seq.stepVelocity(c, s))});
                    events.push_back({onTick + ticksPerStep / 2, 0, 0x89, note, 0});
                }
            }
        }
    };

    if (arrangement && seq.songUsesClips() && seq.clipCount() > 0) {
        // Clip-song arrangement: walk the 2-D timeline bar by bar and emit every clip covering each bar
        // at that bar's step offset (honouring multi-bar spans). Clips sharing a bar overlay — their
        // events simply accumulate — matching the simultaneous multi-track clip playback.
        const int saved = seq.currentPattern();
        const int patLen = seq.numSteps();
        const int bars = seq.clipBarCount();
        for (int bar = 0; bar < bars; ++bar) {
            for (int i = 0; i < seq.clipCount(); ++i) {
                const PlaylistClip& c = seq.clip(i);
                const int span = c.bars < 1 ? 1 : c.bars;
                if (!c.muted && seq.trackAudible(c.track) && bar >= c.startBar &&
                    bar < c.startBar + span && c.pattern >= 0 && c.pattern < seq.patternCount()) {
                    seq.selectPattern(c.pattern);
                    emitPattern(bar * patLen);
                }
            }
        }
        seq.selectPattern(saved);
    } else if (arrangement && !seq.playlist().empty()) {
        // Write the whole playlist back to back; each entry is one pattern length (numSteps) later.
        const int saved = seq.currentPattern();
        const int patLen = seq.numSteps();
        const std::vector<int> pl = seq.playlist();
        for (size_t p = 0; p < pl.size(); ++p) {
            seq.selectPattern(pl[p]);
            emitPattern(static_cast<int>(p) * patLen);
        }
        seq.selectPattern(saved);
    } else {
        emitPattern(0);
    }

    // Arrangement markers → MIDI text marker meta-events at each marker's bar position (a bar is one
    // pattern length of steps). order -1 sorts them ahead of note events sharing the same tick.
    const int barTicks = seq.numSteps() * ticksPerStep;
    for (int i = 0; i < seq.markerCount(); ++i) {
        const ArrangementMarker& mk = seq.marker(i);
        events.push_back({mk.bar * barTicks, -1, 0, 0, 0, mk.name});
    }

    std::sort(events.begin(), events.end(), [](const MidiEvent& a, const MidiEvent& b) {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });

    // Encode the track.
    std::vector<uint8_t> track;
    // Tempo meta at tick 0, so the file plays back at the project's BPM instead of the 120 default.
    const double bpm = seq.bpm() > 0.0 ? seq.bpm() : 120.0;
    const uint32_t usPerQuarter = static_cast<uint32_t>(60000000.0 / bpm + 0.5);
    putVLQ(track, 0);
    track.push_back(0xFF);
    track.push_back(0x51);
    track.push_back(0x03);
    track.push_back(static_cast<uint8_t>((usPerQuarter >> 16) & 0xFFu));
    track.push_back(static_cast<uint8_t>((usPerQuarter >> 8) & 0xFFu));
    track.push_back(static_cast<uint8_t>(usPerQuarter & 0xFFu));
    // Time-signature meta at tick 0 so importing DAWs align bars correctly (without it they assume 4/4,
    // misplacing bar lines for a 3/4 or 6/8-style project). CJC's beat is a quarter note (BPM is
    // quarter-note tempo); beats per bar = numSteps / stepsPerBeat. FF 58 nn dd cc bb: dd = 2 (quarter),
    // cc = 24 MIDI clocks/click, bb = 8 thirty-seconds per quarter.
    const int beatsPerBar = std::max(1, seq.numSteps() / std::max(seq.stepsPerBeat(), 1));
    putVLQ(track, 0);
    track.push_back(0xFF);
    track.push_back(0x58);
    track.push_back(0x04);
    track.push_back(static_cast<uint8_t>(beatsPerBar & 0xFF));
    track.push_back(0x02); // denominator 2^2 = quarter note
    track.push_back(0x18); // 24 MIDI clocks per metronome click
    track.push_back(0x08); // 8 notated 32nd-notes per quarter
    int prevTick = 0;
    for (const MidiEvent& e : events) {
        putVLQ(track, static_cast<uint32_t>(e.tick - prevTick));
        prevTick = e.tick;
        if (!e.meta.empty()) {
            // Text marker meta-event: FF 06 <vlq length> <bytes>.
            track.push_back(0xFF);
            track.push_back(0x06);
            putVLQ(track, static_cast<uint32_t>(e.meta.size()));
            for (char ch : e.meta) {
                track.push_back(static_cast<uint8_t>(ch));
            }
        } else {
            track.push_back(e.status);
            track.push_back(e.data1);
            track.push_back(e.data2);
        }
    }
    putVLQ(track, 0);
    track.push_back(0xFF); // end of track
    track.push_back(0x2F);
    track.push_back(0x00);

    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'M', 'T', 'h', 'd'});
    putBE(buf, 6, 4);
    putBE(buf, 0, 2);                             // format 0
    putBE(buf, 1, 2);                             // one track
    putBE(buf, static_cast<uint32_t>(ppq), 2);    // division (ticks per quarter)
    buf.insert(buf.end(), {'M', 'T', 'r', 'k'});
    putBE(buf, static_cast<uint32_t>(track.size()), 4);
    buf.insert(buf.end(), track.begin(), track.end());

    std::ofstream f(path, std::ios::binary);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for writing";
        }
        return false;
    }
    f.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!f) {
        if (err != nullptr) {
            *err = "write to '" + path + "' failed";
        }
        return false;
    }
    return true;
}

} // namespace maz::audio
