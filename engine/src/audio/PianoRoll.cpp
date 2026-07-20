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

namespace {
// Semitone offsets from the root for each chord quality (shared by addChord and harmonize).
std::vector<int> chordOffsets(Chord chord) {
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
    case Chord::Dim7:
        offsets = {0, 3, 6, 9};
        break;
    case Chord::HalfDim7:
        offsets = {0, 3, 6, 10};
        break;
    case Chord::Dom11:
        offsets = {0, 4, 7, 10, 14, 17};
        break;
    case Chord::Dom13:
        offsets = {0, 4, 7, 10, 14, 21};
        break;
    case Chord::Power:
        offsets = {0, 7};
        break;
    case Chord::MinMaj7:
        offsets = {0, 3, 7, 11};
        break;
    case Chord::Aug7:
        offsets = {0, 4, 8, 10};
        break;
    case Chord::SixNine:
        offsets = {0, 4, 7, 9, 14};
        break;
    }
    return offsets;
}
} // namespace

int PianoRoll::addChord(int startStep, int lengthSteps, int rootPitch, Chord chord, float velocity) {
    const std::vector<int> offsets = chordOffsets(chord);
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

int PianoRoll::harmonize(Chord chord) {
    // Thicken every existing note into a chord: for each note, add a copy at each of the chord's
    // non-root semitone offsets (same start/length/velocity/probability/fine-tune/roll), treating the
    // note's own pitch as the chord root. Turns a melody line into moving chords. New copies are added
    // only for the original notes, and any pushed out of the MIDI range [0,127] are skipped. Returns
    // the number of notes added.
    const std::vector<int> offsets = chordOffsets(chord);
    const size_t original = notes_.size();
    int added = 0;
    for (size_t i = 0; i < original; ++i) {
        const Note base = notes_[i];
        for (int off : offsets) {
            if (off == 0) {
                continue; // the root already exists (the original note)
            }
            const int p = base.pitch + off;
            if (p < 0 || p > 127) {
                continue;
            }
            Note n = base;
            n.pitch = p;
            notes_.push_back(n);
            ++added;
        }
    }
    return added;
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

float PianoRoll::setNoteFineTune(int pitch, int step, float cents) {
    const float c = cents < -200.0f ? -200.0f : (cents > 200.0f ? 200.0f : cents);
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.fineTune = c;
            return c;
        }
    }
    return 0.0f;
}

float PianoRoll::noteFineTune(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.fineTune;
        }
    }
    return 0.0f;
}

int PianoRoll::setNoteRoll(int pitch, int step, int count) {
    const int c = count < 1 ? 1 : (count > 8 ? 8 : count);
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.roll = c;
            return c;
        }
    }
    return 1;
}

int PianoRoll::noteRoll(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.roll;
        }
    }
    return 1;
}

bool PianoRoll::setNoteSlide(int pitch, int step, bool on) {
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.slide = on;
            return on;
        }
    }
    return false;
}

bool PianoRoll::noteSlide(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.slide;
        }
    }
    return false;
}

int PianoRoll::setNoteStride(int pitch, int step, int stride) {
    const int s = stride < 1 ? 1 : (stride > 8 ? 8 : stride);
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.stride = s;
            return s;
        }
    }
    return 1;
}

int PianoRoll::noteStride(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.stride;
        }
    }
    return 1;
}

int PianoRoll::setNoteNudge(int pitch, int step, int percent) {
    const int v = percent < 0 ? 0 : (percent > 95 ? 95 : percent);
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.nudge = v;
            return v;
        }
    }
    return 0;
}

int PianoRoll::noteNudge(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.nudge;
        }
    }
    return 0;
}

float PianoRoll::setNoteCutoff(int pitch, int step, float octaves) {
    const float v = octaves < -8.0f ? -8.0f : (octaves > 8.0f ? 8.0f : octaves);
    for (Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            n.cutoff = v;
            return v;
        }
    }
    return 0.0f;
}

float PianoRoll::noteCutoff(int pitch, int step) const {
    for (const Note& n : notes_) {
        if (n.pitch == pitch && n.startStep == step) {
            return n.cutoff;
        }
    }
    return 0.0f;
}

int PianoRoll::humanizeTiming(int maxNudge, uint32_t seed) {
    const int cap = maxNudge < 0 ? 0 : (maxNudge > 95 ? 95 : maxNudge);
    if (cap == 0) {
        return 0;
    }
    uint32_t rng = seed != 0u ? seed : 1u;
    int changed = 0;
    for (Note& n : notes_) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        n.nudge = static_cast<int>(rng % static_cast<uint32_t>(cap + 1)); // [0, cap]
        ++changed;
    }
    return changed;
}

int PianoRoll::setNudgeForStep(int step, int percent) {
    const int v = percent < 0 ? 0 : (percent > 95 ? 95 : percent);
    int count = 0;
    for (Note& n : notes_) {
        if (n.startStep == step) {
            n.nudge = v;
            ++count;
        }
    }
    return count;
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

int PianoRoll::quantizeLengths(int division) {
    if (division < 2) {
        return 0; // 1 (or less) → already on the grid
    }
    int changed = 0;
    for (Note& n : notes_) {
        int snapped = ((n.lengthSteps + division / 2) / division) * division;
        if (snapped < division) {
            snapped = division; // never collapse a note to zero length
        }
        if (snapped != n.lengthSteps) {
            n.lengthSteps = snapped;
            ++changed;
        }
    }
    return changed;
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

std::vector<int> PianoRoll::scaleDegrees(Scale scale) {
    // Semitone degrees (0..11) each scale allows above the root pitch class.
    switch (scale) {
    case Scale::Major:
        return {0, 2, 4, 5, 7, 9, 11};
    case Scale::Minor:
        return {0, 2, 3, 5, 7, 8, 10};
    case Scale::Dorian:
        return {0, 2, 3, 5, 7, 9, 10};
    case Scale::Phrygian:
        return {0, 1, 3, 5, 7, 8, 10};
    case Scale::Lydian:
        return {0, 2, 4, 6, 7, 9, 11};
    case Scale::Mixolydian:
        return {0, 2, 4, 5, 7, 9, 10};
    case Scale::Locrian:
        return {0, 1, 3, 5, 6, 8, 10};
    case Scale::HarmonicMinor:
        return {0, 2, 3, 5, 7, 8, 11};
    case Scale::MelodicMinor:
        return {0, 2, 3, 5, 7, 9, 11};
    case Scale::PentatonicMajor:
        return {0, 2, 4, 7, 9};
    case Scale::PentatonicMinor:
        return {0, 3, 5, 7, 10};
    case Scale::Blues:
        return {0, 3, 5, 6, 7, 10};
    case Scale::WholeTone:
        return {0, 2, 4, 6, 8, 10};
    case Scale::Chromatic:
        return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    case Scale::PhrygianDominant:
        return {0, 1, 4, 5, 7, 8, 10};
    case Scale::HungarianMinor:
        return {0, 2, 3, 6, 7, 8, 11};
    case Scale::DoubleHarmonic:
        return {0, 1, 4, 5, 7, 8, 11};
    case Scale::NeapolitanMinor:
        return {0, 1, 3, 5, 7, 8, 11};
    case Scale::Hirajoshi:
        return {0, 2, 3, 7, 8};
    case Scale::InSen:
        return {0, 1, 5, 7, 10};
    case Scale::Egyptian:
        return {0, 2, 5, 7, 10};
    }
    return {0, 2, 4, 5, 7, 9, 11};
}

int PianoRoll::snapToScale(int rootPitch, Scale scale) {
    const std::vector<int> degrees = scaleDegrees(scale);
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

int PianoRoll::transposeDiatonic(int degrees, int rootPitch, Scale scale) {
    if (degrees == 0) {
        return 0;
    }
    const std::vector<int> deg = scaleDegrees(scale);
    const int n = static_cast<int>(deg.size());
    if (n == 0) {
        return 0;
    }
    // Floor division helpers (C++ integer division truncates toward zero).
    auto floorDiv = [](int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
    int moved = 0;
    for (Note& note : notes_) {
        const int rel = note.pitch - rootPitch;
        const int oct = floorDiv(rel, 12);
        const int within = rel - oct * 12; // 0..11
        // Scale-degree index at or below `within` (off-scale notes snap down to the nearest degree).
        int idx = 0;
        for (int i = 0; i < n; ++i) {
            if (deg[i] <= within) {
                idx = i;
            }
        }
        const int newDegAbs = idx + degrees;
        const int degOct = floorDiv(newDegAbs, n);
        const int newIdx = newDegAbs - degOct * n; // 0..n-1
        const int newPitch = rootPitch + (oct + degOct) * 12 + deg[static_cast<size_t>(newIdx)];
        if (newPitch != note.pitch) {
            ++moved;
        }
        note.pitch = newPitch;
    }
    return moved;
}

int PianoRoll::mutate(float amount, int rootPitch, Scale scale, int maxDegrees, uint32_t seed) {
    if (amount <= 0.0f || maxDegrees < 1) {
        return 0;
    }
    const std::vector<int> deg = scaleDegrees(scale);
    const int n = static_cast<int>(deg.size());
    if (n == 0) {
        return 0;
    }
    auto floorDiv = [](int a, int b) { return a >= 0 ? a / b : -((-a + b - 1) / b); };
    uint32_t rng = seed ? seed : 0x1234567u; // deterministic xorshift; avoid a 0 state
    auto next = [&]() {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return rng;
    };
    int moved = 0;
    for (Note& note : notes_) {
        // Roll for whether this note mutates (deterministic across the whole pass).
        const float roll = static_cast<float>(next() >> 8) / 16777216.0f; // [0,1)
        if (roll >= amount) {
            continue;
        }
        // A random non-zero degree offset in [-maxDegrees, +maxDegrees].
        const int span = 2 * maxDegrees + 1;
        int offset = static_cast<int>(next() % static_cast<uint32_t>(span)) - maxDegrees;
        if (offset == 0) {
            offset = (next() & 1u) ? maxDegrees : -maxDegrees; // nudge off zero (no no-op mutation)
        }
        // Same diatonic-shift math as transposeDiatonic: snap the note to a degree index, then step.
        const int rel = note.pitch - rootPitch;
        const int oct = floorDiv(rel, 12);
        const int within = rel - oct * 12;
        int idx = 0;
        for (int i = 0; i < n; ++i) {
            if (deg[i] <= within) {
                idx = i;
            }
        }
        const int newDegAbs = idx + offset;
        const int degOct = floorDiv(newDegAbs, n);
        const int newIdx = newDegAbs - degOct * n;
        int newPitch = rootPitch + (oct + degOct) * 12 + deg[static_cast<size_t>(newIdx)];
        newPitch = newPitch < 0 ? 0 : (newPitch > 127 ? 127 : newPitch);
        if (newPitch != note.pitch) {
            ++moved;
        }
        note.pitch = newPitch;
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

int PianoRoll::scaleVelocities(float factor) {
    if (notes_.empty() || factor == 1.0f) {
        return 0;
    }
    if (factor < 0.0f) {
        factor = 0.0f;
    } else if (factor > 4.0f) {
        factor = 4.0f;
    }
    // Pivot on the phrase's own average velocity so the dynamics scale around the line's center.
    double sum = 0.0;
    for (const Note& n : notes_) {
        sum += static_cast<double>(n.velocity);
    }
    const float mean = static_cast<float>(sum / static_cast<double>(notes_.size()));
    int changed = 0;
    for (Note& n : notes_) {
        float v = mean + (n.velocity - mean) * factor;
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

int PianoRoll::randomizeLengths(float amount, uint32_t seed) {
    if (amount <= 0.0f) {
        return 0;
    }
    if (amount > 1.0f) {
        amount = 1.0f;
    }
    uint32_t rng = seed != 0u ? seed : 1u;
    int changed = 0;
    for (Note& n : notes_) {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        const float u = static_cast<float>(rng & 0xFFFFu) / 32768.0f - 1.0f; // [-1, 1)
        const float scaled = static_cast<float>(n.lengthSteps) * (1.0f + amount * u);
        int len = static_cast<int>(scaled + 0.5f); // round (lengths are positive)
        if (len < 1) {
            len = 1;
        }
        if (len != n.lengthSteps) {
            n.lengthSteps = len;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::transpose(int semitones) {
    if (semitones == 0) {
        return 0;
    }
    int changed = 0;
    for (Note& n : notes_) {
        int p = n.pitch + semitones;
        p = p < 0 ? 0 : (p > 127 ? 127 : p);
        if (p != n.pitch) {
            n.pitch = p;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::limitToRange(int lowPitch, int highPitch) {
    if (highPitch < lowPitch) {
        std::swap(lowPitch, highPitch);
    }
    int moved = 0;
    for (Note& n : notes_) {
        int p = n.pitch;
        while (p < lowPitch) {
            p += 12; // fold up an octave (preserves pitch class)
        }
        while (p > highPitch) {
            p -= 12; // fold down an octave
        }
        // Range narrower than an octave and folding overshot the low bound → clamp to the nearer edge.
        if (p < lowPitch) {
            p = lowPitch;
        }
        if (p != n.pitch) {
            n.pitch = p;
            ++moved;
        }
    }
    return moved;
}

int PianoRoll::shift(int steps) {
    if (notes_.empty() || numSteps_ <= 0 || steps % numSteps_ == 0) {
        return 0;
    }
    const int off = ((steps % numSteps_) + numSteps_) % numSteps_; // normalize to [0, numSteps_)
    for (Note& n : notes_) {
        n.startStep = (n.startStep + off) % numSteps_;
    }
    return static_cast<int>(notes_.size());
}

int PianoRoll::stretch(float factor) {
    if (factor <= 0.0f || factor == 1.0f) {
        return 0;
    }
    int changed = 0;
    for (Note& n : notes_) {
        const int newStart = static_cast<int>(std::lround(static_cast<double>(n.startStep) * factor));
        int newLen = static_cast<int>(std::lround(static_cast<double>(n.lengthSteps) * factor));
        if (newLen < 1) {
            newLen = 1;
        }
        if (newStart != n.startStep || newLen != n.lengthSteps) {
            n.startStep = newStart;
            n.lengthSteps = newLen;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::scaleLengths(float factor) {
    if (factor <= 0.0f || factor == 1.0f) {
        return 0;
    }
    int changed = 0;
    for (Note& n : notes_) {
        int newLen = static_cast<int>(std::lround(static_cast<double>(n.lengthSteps) * factor));
        if (newLen < 1) {
            newLen = 1;
        }
        if (newLen != n.lengthSteps) {
            n.lengthSteps = newLen;
            ++changed;
        }
    }
    return changed;
}

int PianoRoll::arpeggiate(int noteLenSteps, int mode) {
    if (noteLenSteps < 1 || notes_.empty()) {
        return 0;
    }
    // Collect the distinct chord start steps in ascending order.
    std::vector<int> starts;
    for (const Note& n : notes_) {
        if (std::find(starts.begin(), starts.end(), n.startStep) == starts.end()) {
            starts.push_back(n.startStep);
        }
    }
    std::sort(starts.begin(), starts.end());

    std::vector<Note> out;
    int created = 0;
    for (int start : starts) {
        // Gather this chord's notes (sharing the start step).
        std::vector<Note> group;
        for (const Note& n : notes_) {
            if (n.startStep == start) {
                group.push_back(n);
            }
        }
        if (group.size() < 2) {
            out.push_back(group.front()); // a single note is not a chord — leave it be
            continue;
        }
        // Sort the chord's pitches low→high; the chord's duration is its longest note.
        std::sort(group.begin(), group.end(),
                  [](const Note& a, const Note& b) { return a.pitch < b.pitch; });
        int dur = 0;
        for (const Note& n : group) {
            dur = std::max(dur, n.lengthSteps);
        }
        const int n = static_cast<int>(group.size());
        const int steps = std::max(1, dur / noteLenSteps);
        for (int k = 0; k < steps; ++k) {
            int idx;
            if (mode == 1) { // down
                idx = (n - 1) - (k % n);
            } else if (mode == 2 && n > 1) { // up-down
                const int period = 2 * n - 2;
                const int pos = k % period;
                idx = pos < n ? pos : period - pos;
            } else { // up
                idx = k % n;
            }
            Note nt = group[static_cast<size_t>(idx)];
            nt.startStep = start + k * noteLenSteps;
            nt.lengthSteps = noteLenSteps;
            out.push_back(nt);
            ++created;
        }
    }
    notes_ = std::move(out);
    return created;
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

int PianoRoll::flam(int stepGap, float velScale) {
    if (stepGap < 1) {
        return 0;
    }
    const float vs = velScale < 0.0f ? 0.0f : (velScale > 1.0f ? 1.0f : velScale);
    const std::vector<Note> src = notes_; // snapshot so grace notes aren't themselves graced
    int created = 0;
    for (const Note& n : src) {
        if (n.startStep < stepGap) {
            continue; // no room before this note for a grace
        }
        Note g = n;
        g.startStep = n.startStep - stepGap;
        g.lengthSteps = 1;
        g.velocity = n.velocity * vs;
        notes_.push_back(g);
        ++created;
    }
    return created;
}

int PianoRoll::echo(int repeats, int stepGap, float decay) {
    if (repeats < 1 || stepGap < 1) {
        return 0;
    }
    const float d = decay < 0.0f ? 0.0f : (decay > 1.0f ? 1.0f : decay);
    const std::vector<Note> src = notes_; // snapshot so the echoes are not themselves echoed
    int created = 0;
    for (const Note& n : src) {
        float vel = n.velocity;
        for (int r = 1; r <= repeats; ++r) {
            vel *= d;
            Note e = n;
            e.startStep = n.startStep + r * stepGap;
            e.velocity = vel < 0.0f ? 0.0f : (vel > 1.0f ? 1.0f : vel);
            notes_.push_back(e);
            ++created;
        }
    }
    return created;
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
