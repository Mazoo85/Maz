// Unit tests for maz::audio — pure DSP, no audio device required. Verifies the oscillator produces
// a sane signal (correct length, bounded amplitude, non-zero energy, expected frequency) and that
// the offline engine render matches the requested duration.

#include "maz/audio/AudioEngine.hpp"
#include "maz/audio/Oscillator.hpp"

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

// Count rising zero-crossings and convert to an estimated fundamental frequency.
double estimateHz(const std::vector<float>& mono, int sampleRate) {
    if (mono.size() < 2) {
        return 0.0;
    }
    int crossings = 0;
    float prev = mono[0];
    for (size_t i = 1; i < mono.size(); ++i) {
        if (prev <= 0.0f && mono[i] > 0.0f) {
            ++crossings;
        }
        prev = mono[i];
    }
    return static_cast<double>(crossings) * static_cast<double>(sampleRate) /
           static_cast<double>(mono.size());
}

} // namespace

int main() {
    const int sampleRate = 48000;
    const int frames = sampleRate; // one second

    // --- Oscillator: pure DSP ------------------------------------------------
    audio::Oscillator osc;
    osc.setWaveform(audio::Waveform::Sine);
    osc.setAmplitude(0.5f);
    osc.noteOn(440.0f);

    std::vector<float> mono(static_cast<size_t>(frames), 0.0f);
    osc.render(mono.data(), frames, sampleRate);

    check(mono.size() == static_cast<size_t>(frames), "render fills the requested sample count");

    float peak = 0.0f;
    double energy = 0.0;
    bool inRange = true;
    for (float s : mono) {
        peak = std::max(peak, std::fabs(s));
        energy += static_cast<double>(s) * static_cast<double>(s);
        if (s < -1.0f || s > 1.0f) {
            inRange = false;
        }
    }
    check(inRange, "all samples stay within [-1, 1]");
    check(energy > 0.0, "signal carries non-zero energy");
    check(peak > 0.4f && peak <= 0.5f + 1e-4f, "peak tracks the 0.5 amplitude");

    const double hz = estimateHz(mono, sampleRate);
    check(std::fabs(hz - 440.0) < 2.0, "estimated frequency is ~440 Hz");

    // noteOff() then a full ramp should decay to silence.
    osc.noteOff();
    std::vector<float> tail(static_cast<size_t>(frames), 0.0f);
    osc.render(tail.data(), frames, sampleRate);
    check(!osc.active(), "voice goes inactive after release");
    check(std::fabs(tail.back()) < 1e-6f, "signal decays to silence after noteOff");

    // Frequency should be settable and reflected in the output.
    audio::Oscillator osc2;
    osc2.noteOn(880.0f);
    std::vector<float> mono2(static_cast<size_t>(frames), 0.0f);
    osc2.render(mono2.data(), frames, sampleRate);
    check(std::fabs(estimateHz(mono2, sampleRate) - 880.0) < 3.0, "880 Hz note renders at ~880 Hz");

    // --- Trapezoid waveform: a clipped triangle (flat tops, DC-free) ---------
    {
        const int N = 2048;
        auto hfEnergy = [](audio::Waveform w, int n) {
            double e = 0.0;
            float prev = audio::waveSample(w, 0.0);
            for (int i = 1; i <= n; ++i) {
                const float cur = audio::waveSample(w, static_cast<double>(i % n) / n);
                const float d = cur - prev;
                e += static_cast<double>(d) * d;
                prev = cur;
            }
            return e;
        };
        // DC-free: one cycle integrates to ~0 (symmetric, no offset — safe as an audio oscillator).
        double dc = 0.0;
        for (int i = 0; i < N; ++i) {
            dc += audio::waveSample(audio::Waveform::Trapezoid, static_cast<double>(i) / N);
        }
        check(std::fabs(dc / N) < 1e-3, "trapezoid is DC-free over a cycle");
        // Flat tops: samples across the peak plateau all clamp to +1, unlike a triangle which only
        // touches +1 at a single instant.
        check(std::fabs(audio::waveSample(audio::Waveform::Trapezoid, 0.02) - 1.0f) < 1e-4f &&
                  std::fabs(audio::waveSample(audio::Waveform::Trapezoid, 0.08) - 1.0f) < 1e-4f,
              "trapezoid has a flat plateau at its peak");
        check(audio::waveSample(audio::Waveform::Triangle, 0.08) < 0.9f,
              "a triangle does not (control: it is still ramping at the same phase)");
        // Harmonic brightness sits between triangle (smoothest) and square (hardest edges).
        const double tri = hfEnergy(audio::Waveform::Triangle, N);
        const double trap = hfEnergy(audio::Waveform::Trapezoid, N);
        const double sqr = hfEnergy(audio::Waveform::Square, N);
        check(trap > tri && trap < sqr, "trapezoid brightness sits between triangle and square");
    }

    // --- AudioEngine: offline render ----------------------------------------
    audio::AudioEngine engine;
    engine.initOffline();
    engine.noteOn(440.0f);
    const std::vector<float> stereo = engine.renderOffline(1.0);
    const int channels = engine.config().channels;
    check(stereo.size() == static_cast<size_t>(frames) * static_cast<size_t>(channels),
          "offline render length matches seconds * sampleRate * channels");
    check(engine.framesRendered() == static_cast<uint64_t>(frames),
          "sample clock advanced by exactly one second of frames");
    // Left and right channels should be identical (mono voice fanned out).
    check(stereo.size() >= 2 && std::fabs(stereo[0] - stereo[1]) < 1e-6f,
          "interleaved channels carry the same mono signal");

    // --- AudioEngine: stem export (per-bus offline bounce) -------------------
    {
        auto energyOf = [](const std::vector<float>& b) {
            double e = 0.0;
            for (float v : b) {
                e += static_cast<double>(v) * static_cast<double>(v);
            }
            return e;
        };
        audio::AudioEngine eng;
        eng.initOffline();
        audio::Sequencer& seq = eng.sequencer();
        seq.setStep(0, 0, true);                          // a kick on the drums bus
        seq.roll().addNote(audio::Note{0, 8, 60, 1.0f});  // a lead note on the lead bus
        // (no notes on roll2 → the bass bus stays silent)
        seq.play();
        const audio::AudioEngine::Stems st = eng.renderStemsOffline(0.3);
        const int stChannels = eng.config().channels;
        const size_t want = static_cast<size_t>(0.3 * 48000.0) * static_cast<size_t>(stChannels);
        check(st.drums.size() == want && st.lead.size() == want && st.bass.size() == want,
              "each stem buffer matches seconds * sampleRate * channels");
        check(energyOf(st.drums) > 0.0, "the drums stem carries the kick");
        check(energyOf(st.lead) > 0.0, "the lead stem carries the synth note");
        check(energyOf(st.bass) < 1e-6, "the bass stem is silent (no bass notes)");
        // The stems are genuinely separated, not copies of the same mix.
        check(energyOf(st.drums) != energyOf(st.lead), "drums and lead stems differ (real separation)");

        // A per-track insert (muting the drums strip) is reflected in that stem alone.
        audio::AudioEngine eng2;
        eng2.initOffline();
        audio::Sequencer& seq2 = eng2.sequencer();
        seq2.setStep(0, 0, true);
        seq2.roll().addNote(audio::Note{0, 8, 60, 1.0f});
        eng2.mixer().track(audio::MixerBus::Drums).setMuted(true);
        seq2.play();
        const audio::AudioEngine::Stems st2 = eng2.renderStemsOffline(0.3);
        check(energyOf(st2.drums) < 1e-6, "muting the drums track silences the drums stem");
        check(energyOf(st2.lead) > 0.0, "muting the drums track leaves the lead stem intact");
    }

    // --- Per-bus solo: only soloed buses are heard in the master render -----
    {
        auto masterEnergy = [](audio::AudioEngine& e, double sec) {
            const std::vector<float> b = e.renderOffline(sec);
            double en = 0.0;
            for (float v : b) {
                en += static_cast<double>(v) * static_cast<double>(v);
            }
            return en;
        };
        // A kick on the drums bus only. Soloing the (empty) lead bus silences the master.
        audio::AudioEngine soloLead;
        soloLead.initOffline();
        soloLead.sequencer().setStep(0, 0, true);
        soloLead.mixer().track(audio::MixerBus::Lead).setSoloed(true);
        soloLead.sequencer().play();
        check(masterEnergy(soloLead, 0.3) < 1e-6,
              "soloing an empty bus silences the other (non-soloed) buses");

        // Soloing the drums bus keeps the kick audible.
        audio::AudioEngine soloDrums;
        soloDrums.initOffline();
        soloDrums.sequencer().setStep(0, 0, true);
        soloDrums.mixer().track(audio::MixerBus::Drums).setSoloed(true);
        soloDrums.sequencer().play();
        check(masterEnergy(soloDrums, 0.3) > 0.0, "soloing the active bus keeps it audible");
    }

    // --- Per-bus aux send: a bus's reverb send feeds the shared reverb tail --
    {
        // Energy in the late tail (well after the dry kick has decayed) reveals the reverb.
        auto tailEnergy = [](audio::AudioEngine& e) {
            const std::vector<float> b = e.renderOffline(0.6);
            const int ch = e.config().channels;
            const size_t start = static_cast<size_t>(0.35 * 48000.0) * static_cast<size_t>(ch);
            double en = 0.0;
            for (size_t i = start; i < b.size(); ++i) {
                en += static_cast<double>(b[i]) * static_cast<double>(b[i]);
            }
            return en;
        };
        // Dry: a single kick, no sends → the tail is near silent once the kick decays.
        audio::AudioEngine dry;
        dry.initOffline();
        dry.sequencer().setStep(0, 0, true);
        dry.sequencer().play();
        const double dryTail = tailEnergy(dry);

        // Wet: the same kick with the drum bus's reverb send up → a lingering reverb tail.
        audio::AudioEngine wet;
        wet.initOffline();
        wet.sequencer().setStep(0, 0, true);
        wet.mixer().track(audio::MixerBus::Drums).setReverbSend(1.0f);
        wet.sequencer().play();
        const double wetTail = tailEnergy(wet);
        check(wetTail > dryTail * 4.0, "a per-bus reverb send feeds the shared reverb tail");
    }

    // --- Master output metering: peak + RMS track the rendered level ---------
    {
        audio::AudioEngine eng;
        eng.initOffline();
        check(eng.masterPeak() == 0.0f && eng.masterRms() == 0.0f,
              "a fresh engine reports zero output level");
        eng.sequencer().setStep(0, 0, true); // a loud kick on the drums
        eng.sequencer().play();
        (void)eng.renderOffline(0.05); // render a block containing the kick onset
        check(eng.masterPeak() > 0.01f && eng.masterPeak() <= 1.0f,
              "the master peak meter reflects the rendered kick (0 < peak <= 1)");
        check(eng.masterRms() > 0.0f && eng.masterRms() <= eng.masterPeak() + 1e-6f,
              "the master RMS is positive and never exceeds the peak");
    }

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
