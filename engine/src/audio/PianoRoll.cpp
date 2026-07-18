#include "maz/audio/PianoRoll.hpp"

#include <algorithm>
#include <cmath>

namespace maz::audio {

bool PianoRoll::hasNote(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return true;
        }
    }
    return false;
}

int PianoRoll::addChord(int startStep, int lengthSteps, int rootPitch, Chord chord, float velocity) {
    // Semitone offsets from the root for each chord quality.
    std::vector<int> offsets;
    switch (chord) {
    case Chord::Major:
        offsets = {0, 4, 7};
        break;
    case Chord::Minor:
        offsets = {0, 3, 7};
        break;
    case Chord::Dom7:
        offsets = {0, 4, 7, 10};
        break;
    case Chord::Maj7:
        offsets = {0, 4, 7, 11};
        break;
    case Chord::Min7:
        offsets = {0, 3, 7, 10};
        break;
    case Chord::Dim:
        offsets = {0, 3, 6};
        break;
    case Chord::Aug:
        offsets = {0, 4, 8};
        break;
    case Chord::Sus2:
        offsets = {0, 2, 7};
        break;
    case Chord::Sus4:
        offsets = {0, 5, 7};
        break;
    case Chord::Maj6:
        offsets = {0, 4, 7, 9};
        break;
    case Chord::Min6:
        offsets = {0, 3, 7, 9};
        break;
    case Chord::Maj9:
        offsets = {0, 4, 7, 11, 14};
        break;
    case Chord::Min9:
        offsets = {0, 3, 7, 10, 14};
        break;
    case Chord::Dom9:
        offsets = {0, 4, 7, 10, 14};
        break;
    case Chord::Add9:
        offsets = {0, 4, 7, 14};
        break;
    }
    for (int off : offsets) {
        Note n;
        n.startStep = startStep;
        n.lengthSteps = lengthSteps < 1 ? 1 : lengthSteps;
        n.pitch = rootPitch + off;
        n.velocity = velocity;
        notes_.push_back(n);
    }
    return static_cast<int>(offsets.size());
}

float PianoRoll::setNoteProbability(int pitch, int step, float probability) {
    const float p = probability < 0.0f ? 0.0f : (probability > 1.0f ? 1.0f : probability);
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.probability = p;
            return p;
        }
    }
    return 1.0f;
}

float PianoRoll::noteProbability(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.probability;
        }
    }
    return 1.0f;
}

int PianoRoll::quantize(int division) {
    if (division < 2) {
        return 0; // 1 (or less) → already on the grid
    }
    int moved = 0;
    for (Note& n : notes_) {
        const int snapped = ((n.startStep + division / 2) / division) * division;
        if (snapped != n.startStep) {
            n.startStep = snapped;
            ++moved;
        }
    }
    return moved;
}

int PianoRoll::quantizeStrength(int division, float strength) {
    if (division < 2) {
        return 0;
    }
    const float s = strength < 0.0f ? 0.0f : (strength > 1.0f ? 1.0f : strength);
    int moved = 0;
    for (Note& n : notes_) {
        const int snapped = ((n.startStep + division / 2) / division) * division;
        const int target =
            n.startStep + static_cast<int>(std::lround(s * static_cast<float>(snapped - n.startStep)));
        if (target != n.startStep) {
            n.startStep = target;
            ++moved;
        }
    }
    return moved;
}

int PianoRoll::snapToScale(int rootPitch, Scale scale) {
    // Semitone degrees (0..11) each scale allows above the root pitch class.
    std::vector<int> degrees;
    switch (scale) {
    case Scale::Major:
        degrees = {0, 2, 4, 5, 7, 9, 11};
        break;
    case Scale::Minor:
        degrees = {0, 2, 3, 5, 7, 8, 10};
        break;
    case Scale::Dorian:
        degrees = {0, 2, 3, 5, 7, 9, 10};
        break;
    case Scale::Phrygian:
        degrees = {0, 1, 3, 5, 7, 8, 10};
        break;
    case Scale::Lydian:
        degrees = {0, 2, 4, 6, 7, 9, 11};
        break;
    case Scale::Mixolydian:
        degrees = {0, 2, 4, 5, 7, 9, 10};
        break;
    case Scale::Locrian:
        degrees = {0, 1, 3, 5, 6, 8, 10};
        break;
    case Scale::HarmonicMinor:
        degrees = {0, 2, 3, 5, 7, 8, 11};
        break;
    case Scale::MelodicMinor:
        degrees = {0, 2, 3, 5, 7, 9, 11};
        break;
    case Scale::PentatonicMajor:
        degrees = {0, 2, 4, 7, 9};
        break;
    case Scale::PentatonicMinor:
        degrees = {0, 3, 5, 7, 10};
        break;
    case Scale::Blues:
        degrees = {0, 3, 5, 6, 7, 10};
        break;
    }
    // Membership test for a pitch class relative to the root.
    auto inScale = [&](int pitch) {
        const int pc = ((pitch - rootPitch) % 12 + 12) % 12;
        for (int d : degrees) {
            if (d == pc) {
                return true;
            }
        }
        return false;
    };
    int moved = 0;
    for (Note& n : notes_) {
        if (inScale(n.pitch)) {
            continue;
        }
        // Search outward for the nearest in-scale pitch: distance 1 down, 1 up, 2 down, 2 up, …
        // (down first, so exact ties resolve downward). A degree is always within 6 semitones.
        for (int k = 1; k <= 6; ++k) {
            if (inScale(n.pitch - k)) {
                n.pitch -= k;
                ++moved;
                break;
            }
            if (inScale(n.pitch + k)) {
                n.pitch += k;
                ++moved;
                break;
            }
        }
    }
    return moved;
}

int PianoRoll::strum(int stepOffset) {
    if (stepOffset == 0 || notes_.empty()) {
        return 0;
    }
    // Snapshot each note's original start so grouping is unaffected by the shifts we apply.
    std::vector<int> orig(notes_.size());
    for (size_t i = 0; i < notes_.size(); ++i) {
        orig[i] = notes_[i].startStep;
    }
    // The distinct original start steps (each stack is processed once).
    std::vector<int> starts;
    for (int s : orig) {
        if (std::find(starts.begin(), starts.end(), s) == starts.end()) {
            starts.push_back(s);
        }
    }
    int moved = 0;
    for (int s : starts) {
        std::vector<size_t> idx;
        for (size_t i = 0; i < notes_.size(); ++i) {
            if (orig[i] == s) {
                idx.push_back(i);
            }
        }
        if (idx.size() < 2) {
            continue; // a single note is not a chord to roll
        }
        // Order the stack low → high, then delay each successive note a little more.
        std::sort(idx.begin(), idx.end(),
                  [&](size_t a, size_t b) { return notes_[a].pitch < notes_[b].pitch; });
        for (size_t j = 1; j < idx.size(); ++j) {
            int ns = s + stepOffset * static_cast<int>(j);
            if (ns < 0) {
                ns = 0;
            }
            notes_[idx[j]].startStep = ns;
            ++moved;
        }
    }
    return moved;
}

int PianoRoll::legato() {
    int changed = 0;
    for (Note& n : notes_) {
        // The nearest start step strictly after this note's start.
        int nextStart = -1;
        for (const Note& other : notes_) {
            if (other.startStep > n.startStep && (nextStart < 0 || other.startStep < nextStart)) {
                nextStart = other.startStep;
            }
        }
        if (nextStart < 0) {
            continue; // nothing follows → leave the last note(s) as they are
        }
        const int newLen = nextStart - n.startStep; // ≥ 1 since nextStart > startStep
        if (newLen != n.lengthSteps) {
            n.lengthSteps = newLen;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::invert(int pivotPitch) {
    int changed = 0;
    for (Note& n : notes_) {
        int mirrored = 2 * pivotPitch - n.pitch;
        if (mirrored < 0) {
            mirrored = 0;
        } else if (mirrored > 127) {
            mirrored = 127;
        }
        if (mirrored != n.pitch) {
            n.pitch = mirrored;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::reverseTime() {
    int moved = 0;
    for (Note& n : notes_) {
        int newStart = numSteps_ - n.startStep - n.lengthSteps;
        if (newStart < 0) {
            newStart = 0; // a note running past the pattern end pins to the start
        }
        if (newStart != n.startStep) {
            n.startStep = newStart;
            ++moved;
        }
    }
    return moved;
}

int PianoRoll::duplicate(int offsetSteps) {
    if (offsetSteps <= 0) {
        return 0;
    }
    const size_t count = notes_.size(); // snapshot before appending
    for (size_t i = 0; i < count; ++i) {
        Note copy = notes_[i];
        copy.startStep += offsetSteps;
        notes_.push_back(copy);
    }
    return static_cast<int>(count);
}

int PianoRoll::randomizeVelocity(float amount, uint32_t seed) {
    if (amount <= 0.0f) {
        return 0;
    }
    uint32_t rng = seed != 0u ? seed : 1u; // xorshift needs a non-zero state
    int changed = 0;
    for (Note& n : notes_) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        const float r = static_cast<float>(rng) / 4294967295.0f;    // [0,1]
        const float factor = 1.0f + amount * (2.0f * r - 1.0f);     // [1-amount, 1+amount]
        float v = n.velocity * factor;
        if (v < 0.0f) {
            v = 0.0f;
        } else if (v > 1.0f) {
            v = 1.0f;
        }
        if (v != n.velocity) {
            n.velocity = v;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::randomizeTiming(int maxSteps, uint32_t seed) {
    if (maxSteps <= 0) {
        return 0;
    }
    uint32_t rng = seed != 0u ? seed : 1u;
    const int span = 2 * maxSteps + 1; // offsets in [-maxSteps, +maxSteps]
    int changed = 0;
    for (Note& n : notes_) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        const int offset = static_cast<int>(rng % static_cast<uint32_t>(span)) - maxSteps;
        int start = n.startStep + offset;
        if (start < 0) {
            start = 0;
        }
        if (start != n.startStep) {
            n.startStep = start;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::chop(int pieces) {
    if (pieces < 2) {
        return 0;
    }
    std::vector<Note> out;
    out.reserve(notes_.size());
    int chopped = 0;
    for (const Note& n : notes_) {
        if (n.lengthSteps >= pieces) {
            const int pieceLen = n.lengthSteps / pieces; // ≥ 1 since lengthSteps ≥ pieces
            for (int p = 0; p < pieces; ++p) {
                Note c = n;
                c.startStep = n.startStep + p * pieceLen;
                c.lengthSteps = pieceLen;
                out.push_back(c);
            }
            ++chopped;
        } else {
            out.push_back(n); // too short to split into whole-step pieces
        }
    }
    notes_ = std::move(out);
    return chopped;
}

int PianoRoll::velocityRamp(float fromVel, float toVel) {
    if (notes_.empty()) {
        return 0;
    }
    auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
    const float from = clamp01(fromVel);
    const float to = clamp01(toVel);
    // Span the ramp across the notes' start-step range so it follows musical time.
    int minStart = notes_[0].startStep;
    int maxStart = notes_[0].startStep;
    for (const Note& n : notes_) {
        if (n.startStep < minStart) {
            minStart = n.startStep;
        }
        if (n.startStep > maxStart) {
            maxStart = n.startStep;
        }
    }
    const int range = maxStart - minStart;
    int changed = 0;
    for (Note& n : notes_) {
        const float t = range > 0
                            ? static_cast<float>(n.startStep - minStart) / static_cast<float>(range)
                            : 0.0f;
        const float v = clamp01(from + (to - from) * t);
        if (v != n.velocity) {
            n.velocity = v;
            ++changed;
        }
    }
    return changed;
}

void PianoRoll::toggle(int pitch, int step, float velocity) {
    for (size_t i = 0; i < notes_.size(); ++i) {
        if (notes_[i].pitch == pitch && notes_[i].startStep == step) {
            notes_.erase(notes_.begin() + static_cast<std::ptrdiff_t>(i));
            return;
        }
    }
    Note n;
    n.startStep = step;
    n.lengthSteps = 1;
    n.pitch = pitch;
    n.velocity = velocity;
    notes_.push_back(n);
}

} // namespace maz::audio
