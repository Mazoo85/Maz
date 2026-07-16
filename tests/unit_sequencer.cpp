// Unit tests for maz::audio::Sequencer + DrumVoice — pure DSP/logic, no audio device.
// Verifies the pattern grid, transport timing (sample-accurate stepping), and that triggered
// drum voices produce sound while an empty pattern stays silent.

#include "maz/audio/DrumVoice.hpp"
#include "maz/audio/Sequencer.hpp"

#include <cmath>
#include <cstdio>
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

double rms(const std::vector<float>& buf) {
    if (buf.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (float s : buf) {
        sum += static_cast<double>(s) * static_cast<double>(s);
    }
    return std::sqrt(sum / static_cast<double>(buf.size()));
}

// Render `frames` of the sequencer into a fresh mono buffer.
std::vector<float> renderMono(audio::Sequencer& seq, int frames, int sampleRate) {
    std::vector<float> buf(static_cast<size_t>(frames), 0.0f);
    seq.render(buf.data(), frames, sampleRate);
    return buf;
}

} // namespace

int main() {
    const int sampleRate = 48000;

    // --- DrumVoice -----------------------------------------------------------
    audio::DrumVoice kick;
    kick.setType(audio::Drum::Kick);
    check(!kick.active(), "drum voice starts inactive");

    std::vector<float> quiet(4800, 0.0f);
    kick.render(quiet.data(), 4800, sampleRate);
    check(rms(quiet) == 0.0, "inactive voice renders silence");

    kick.trigger();
    check(kick.active(), "voice is active after trigger");
    std::vector<float> hit(4800, 0.0f); // 0.1 s
    kick.render(hit.data(), 4800, sampleRate);
    check(rms(hit) > 0.0, "triggered kick produces sound");

    // A closed hat is short — after ~0.3 s it should have decayed to inactive.
    audio::DrumVoice hat;
    hat.setType(audio::Drum::ClosedHat);
    hat.trigger();
    std::vector<float> tail(static_cast<size_t>(sampleRate) / 2, 0.0f);
    hat.render(tail.data(), sampleRate / 2, sampleRate);
    check(!hat.active(), "closed hat decays to inactive");

    // --- Sequencer grid ------------------------------------------------------
    audio::Sequencer seq;
    check(seq.numSteps() == 16, "default pattern is 16 steps");
    check(seq.numChannels() >= 4, "kit has at least four channels");

    seq.setStep(0, 0, true);
    check(seq.step(0, 0), "setStep switches a step on");
    seq.toggle(0, 0);
    check(!seq.step(0, 0), "toggle switches it back off");
    seq.setStep(0, 3, true);
    seq.clear();
    check(!seq.step(0, 3), "clear switches every step off");
    // Out-of-range access is safe and reads false.
    check(!seq.step(-1, 0) && !seq.step(0, 999), "out-of-range steps read false");

    // --- Transport timing (120 BPM, 16ths @ 48 kHz → exactly 6000 samples/step) -------------
    seq.setBpm(120.0);
    seq.play();
    check(seq.playing(), "play() starts the transport");
    check(seq.currentStep() == 0, "transport starts on step 0");

    (void)renderMono(seq, 6000, sampleRate); // one step
    check(seq.currentStep() == 1, "one step of frames advances to step 1");

    for (int i = 0; i < 15; ++i) {
        (void)renderMono(seq, 6000, sampleRate);
    }
    check(seq.currentStep() == 0, "playhead wraps back to step 0 after a full bar");

    // Block size must not change the timing: 6000 frames split as 1000×6 still advances one step.
    audio::Sequencer seq2;
    seq2.setBpm(120.0);
    seq2.play();
    for (int i = 0; i < 6; ++i) {
        (void)renderMono(seq2, 1000, sampleRate);
    }
    check(seq2.currentStep() == 1, "stepping is block-size independent");

    seq2.stop();
    check(!seq2.playing(), "stop() freezes the transport");

    // --- Sound vs silence ----------------------------------------------------
    audio::Sequencer beat;
    beat.setBpm(120.0);
    beat.setStep(0, 0, true); // kick on the downbeat
    beat.play();
    const std::vector<float> firstStep = renderMono(beat, 6000, sampleRate);
    check(rms(firstStep) > 0.0, "a step with a kick produces sound");

    audio::Sequencer silent;
    silent.play(); // empty pattern
    const std::vector<float> nothing = renderMono(silent, 6000, sampleRate);
    check(rms(nothing) == 0.0, "an empty pattern renders silence");

    // --- Arrangement: patterns + playlist -----------------------------------
    audio::Sequencer arr;
    check(arr.patternCount() == 1, "starts with one pattern");
    arr.setStep(0, 0, true); // kick on step 0 of pattern 0
    const int p1 = arr.addPattern();
    check(p1 == 1 && arr.patternCount() == 2, "addPattern appends");
    arr.selectPattern(p1);
    check(!arr.step(0, 0), "a fresh pattern is empty (independent grid)");
    arr.setStep(1, 0, true); // snare on step 0 of pattern 1
    arr.selectPattern(0);
    check(arr.step(0, 0) && !arr.step(1, 0), "patterns keep independent grids");

    // Song mode: playlist [0,1] should switch the current pattern at each bar boundary.
    arr.setBpm(120.0); // 6000 samples/step → 96000 samples/bar (16 steps)
    arr.setPlaylist({0, 1});
    arr.setSongMode(true);
    arr.play();
    check(arr.currentPattern() == 0, "song mode starts on the first playlist pattern");
    // Render one full bar (16 steps × 6000).
    (void)renderMono(arr, 16 * 6000, sampleRate);
    check(arr.currentPattern() == 1, "advances to the next playlist pattern after a bar");
    (void)renderMono(arr, 16 * 6000, sampleRate);
    check(arr.currentPattern() == 0, "playlist wraps back to the start");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
