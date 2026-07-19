#include "maz/audio/MidiWriter.hpp"

#include "maz/audio/Sequencer.hpp"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <vector>

namespace maz::audio {

namespace {

// General-MIDI percussion note numbers for the default kit (Kick/Snare/ClosedHat/OpenHat/Clap).
constexpr int kGmDrum[] = {36, 38, 42, 46, 39};

struct MidiEvent {
    int tick;
    int order;   // 0 = note-off, 1 = note-on (offs sort first at the same tick)
    uint8_t status;
    uint8_t data1;
    uint8_t data2;
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

bool writeMidi(const std::string& path, Sequencer& seq, int ppq, std::string* err) {
    if (ppq < 24) {
        ppq = 96;
    }
    const int ticksPerStep = ppq / std::max(seq.stepsPerBeat(), 1);
    const int drumCount = static_cast<int>(sizeof(kGmDrum) / sizeof(kGmDrum[0]));

    std::vector<MidiEvent> events;

    // Melody (lead): piano-roll notes on channel 0.
    for (const Note& n : seq.roll().notes()) {
        const int onTick = n.startStep * ticksPerStep;
        const int offTick = (n.startStep + n.lengthSteps) * ticksPerStep;
        const uint8_t v = vel7(n.velocity);
        events.push_back({onTick, 1, 0x90, static_cast<uint8_t>(n.pitch & 0x7F), v});
        events.push_back({offTick, 0, 0x80, static_cast<uint8_t>(n.pitch & 0x7F), 0});
    }

    // Bass: the second piano-roll's notes on channel 1 (previously omitted from the export).
    for (const Note& n : seq.roll2().notes()) {
        const int onTick = n.startStep * ticksPerStep;
        const int offTick = (n.startStep + n.lengthSteps) * ticksPerStep;
        const uint8_t v = vel7(n.velocity);
        events.push_back({onTick, 1, 0x91, static_cast<uint8_t>(n.pitch & 0x7F), v});
        events.push_back({offTick, 0, 0x81, static_cast<uint8_t>(n.pitch & 0x7F), 0});
    }

    // Drums: the grid as GM percussion on channel 9 (MIDI channel 10).
    for (int c = 0; c < seq.numChannels() && c < drumCount; ++c) {
        for (int s = 0; s < seq.numSteps(); ++s) {
            if (seq.step(c, s)) {
                const int onTick = s * ticksPerStep;
                const int offTick = onTick + ticksPerStep / 2;
                const uint8_t note = static_cast<uint8_t>(kGmDrum[c] & 0x7F);
                events.push_back({onTick, 1, 0x99, note, vel7(seq.stepVelocity(c, s))});
                events.push_back({offTick, 0, 0x89, note, 0});
            }
        }
    }

    std::sort(events.begin(), events.end(), [](const MidiEvent& a, const MidiEvent& b) {
        return a.tick != b.tick ? a.tick < b.tick : a.order < b.order;
    });

    // Encode the track.
    std::vector<uint8_t> track;
    int prevTick = 0;
    for (const MidiEvent& e : events) {
        putVLQ(track, static_cast<uint32_t>(e.tick - prevTick));
        prevTick = e.tick;
        track.push_back(e.status);
        track.push_back(e.data1);
        track.push_back(e.data2);
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
