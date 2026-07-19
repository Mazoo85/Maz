#include "maz/audio/MidiReader.hpp"

#include "maz/audio/PianoRoll.hpp"
#include "maz/audio/Sequencer.hpp"

#include <array>
#include <cstdint>
#include <fstream>
#include <vector>

namespace maz::audio {

namespace {

// General-MIDI percussion note numbers for the default kit (Kick/Snare/ClosedHat/OpenHat/Clap) —
// the same mapping writeMidi uses, so a write→read round-trip restores the grid.
constexpr int kGmDrum[] = {36, 38, 42, 46, 39};

// A cursor over a byte buffer with bounds-checked reads.
struct Reader {
    const uint8_t* p;
    size_t n;
    size_t i = 0;
    bool ok = true;

    uint8_t u8() {
        if (i >= n) {
            ok = false;
            return 0;
        }
        return p[i++];
    }
    uint32_t be(int bytes) {
        uint32_t v = 0;
        for (int k = 0; k < bytes; ++k) {
            v = (v << 8) | u8();
        }
        return v;
    }
    // MIDI variable-length quantity.
    uint32_t vlq() {
        uint32_t v = 0;
        for (int k = 0; k < 4; ++k) {
            const uint8_t b = u8();
            v = (v << 7) | (b & 0x7Fu);
            if ((b & 0x80u) == 0) {
                break;
            }
        }
        return v;
    }
    void skip(size_t k) {
        if (i + k > n) {
            i = n;
            ok = false;
        } else {
            i += k;
        }
    }
};

struct RawNote {
    int channel;
    int pitch;
    int onTick;
    int offTick;
    float velocity;
};

} // namespace

bool readMidi(const std::string& path, Sequencer& seq, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for reading";
        }
        return false;
    }
    std::vector<uint8_t> buf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (buf.size() < 14 || buf[0] != 'M' || buf[1] != 'T' || buf[2] != 'h' || buf[3] != 'd') {
        if (err != nullptr) {
            *err = "'" + path + "' is not a Standard MIDI File";
        }
        return false;
    }

    Reader r{buf.data(), buf.size()};
    r.skip(4);                       // "MThd"
    const uint32_t headerLen = r.be(4);
    r.skip(2);                       // format
    const uint32_t ntrks = r.be(2);
    const uint32_t division = r.be(2);
    r.skip(headerLen - 6);           // any extra header bytes
    if ((division & 0x8000u) != 0 || division == 0) {
        if (err != nullptr) {
            *err = "SMPTE-timed MIDI is not supported";
        }
        return false;
    }
    const int ticksPerStep = std::max(1, static_cast<int>(division) / std::max(seq.stepsPerBeat(), 1));

    std::vector<RawNote> notes;
    std::array<std::array<int, 128>, 16> onTick{};   // active note start tick (-1 = none)
    std::array<std::array<float, 128>, 16> onVel{};
    for (auto& ch : onTick) {
        ch.fill(-1);
    }

    for (uint32_t t = 0; t < ntrks && r.ok; ++t) {
        // Find the next MTrk chunk.
        while (r.i + 8 <= r.n && !(r.p[r.i] == 'M' && r.p[r.i + 1] == 'T' && r.p[r.i + 2] == 'r' &&
                                   r.p[r.i + 3] == 'k')) {
            ++r.i;
        }
        r.skip(4); // "MTrk"
        const uint32_t trackLen = r.be(4);
        const size_t trackEnd = r.i + trackLen;
        int tick = 0;
        uint8_t running = 0;
        while (r.i < trackEnd && r.ok) {
            tick += static_cast<int>(r.vlq());
            uint8_t status = r.p[r.i];
            if (status & 0x80u) {
                r.i++;
                running = status;
            } else {
                status = running; // running status: reuse the previous status byte
            }
            const uint8_t hi = status & 0xF0u;
            const int ch = status & 0x0F;
            if (hi == 0x80 || hi == 0x90) {
                const uint8_t pitch = r.u8() & 0x7Fu;
                const uint8_t vel = r.u8() & 0x7Fu;
                const bool noteOn = (hi == 0x90) && vel > 0;
                if (noteOn) {
                    onTick[static_cast<size_t>(ch)][pitch] = tick;
                    onVel[static_cast<size_t>(ch)][pitch] = static_cast<float>(vel) / 127.0f;
                } else if (onTick[static_cast<size_t>(ch)][pitch] >= 0) {
                    notes.push_back({ch, pitch, onTick[static_cast<size_t>(ch)][pitch], tick,
                                     onVel[static_cast<size_t>(ch)][pitch]});
                    onTick[static_cast<size_t>(ch)][pitch] = -1;
                }
            } else if (hi == 0xA0 || hi == 0xB0 || hi == 0xE0) {
                r.skip(2); // two data bytes
            } else if (hi == 0xC0 || hi == 0xD0) {
                r.skip(1); // one data byte
            } else if (status == 0xFF) {
                r.u8();                       // meta type
                const uint32_t len = r.vlq();
                r.skip(len);
            } else if (status == 0xF0 || status == 0xF7) {
                const uint32_t len = r.vlq();
                r.skip(len);
            } else {
                break; // unknown byte — bail out of this track
            }
        }
        r.i = trackEnd; // jump to the next chunk regardless of how this track parsed
    }

    // Apply: channel-0 melodic notes → the lead roll, channel-1 → the bass roll, channel-10
    // percussion → the grid. Clear all three first.
    seq.roll().clear();
    seq.roll2().clear();
    seq.clear();
    const int drumCount = static_cast<int>(sizeof(kGmDrum) / sizeof(kGmDrum[0]));
    for (const RawNote& rn : notes) {
        const int startStep = (rn.onTick + ticksPerStep / 2) / ticksPerStep;
        int lenSteps = (rn.offTick - rn.onTick + ticksPerStep / 2) / ticksPerStep;
        if (lenSteps < 1) {
            lenSteps = 1;
        }
        if (rn.channel == 9) {
            // Percussion: map the GM note back to a kit channel and set the step.
            for (int c = 0; c < drumCount && c < seq.numChannels(); ++c) {
                if (kGmDrum[c] == rn.pitch && startStep >= 0 && startStep < seq.numSteps()) {
                    seq.setStepVelocity(c, startStep, rn.velocity);
                    break;
                }
            }
        } else {
            Note note;
            note.startStep = startStep;
            note.lengthSteps = lenSteps;
            note.pitch = rn.pitch;
            note.velocity = rn.velocity;
            // Channel 1 → bass roll (symmetric with the writer); every other melodic channel → lead.
            if (rn.channel == 1) {
                seq.roll2().addNote(note);
            } else {
                seq.roll().addNote(note);
            }
        }
    }
    return true;
}

} // namespace maz::audio
