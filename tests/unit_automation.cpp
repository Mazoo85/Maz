// Unit tests for the A4 automation system — the LFO shapes/rate and the Automation bank driving a
// live AudioEngine parameter over time. Pure DSP/logic, no audio device.

#include "maz/audio/AudioEngine.hpp"
#include "maz/audio/Automation.hpp"
#include "maz/audio/LFO.hpp"

#include <cmath>
#include <cstdio>

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
    // --- LFO ------------------------------------------------------------------
    audio::LFO sine;
    sine.shape = audio::Waveform::Sine;
    sine.rateHz = 1.0f;
    check(std::fabs(sine.valueBipolar(0.0)) < 1e-4, "sine LFO starts at 0");
    check(sine.valueBipolar(0.25) > 0.99f, "sine LFO peaks at a quarter period");
    check(std::fabs(sine.valueBipolar(1.0)) < 1e-4, "sine LFO completes one cycle in 1 s at 1 Hz");

    audio::LFO saw;
    saw.shape = audio::Waveform::Saw;
    saw.rateHz = 2.0f;
    // At 2 Hz the phase wraps every 0.5 s; a saw ramps -1→1 across each period.
    check(saw.valueBipolar(0.0) < -0.99f, "saw LFO starts low");
    check(saw.valueUnipolar(0.0) >= 0.0f && saw.valueUnipolar(0.0) <= 0.01f,
          "unipolar maps the low point to ~0");

    // --- Automation bank ------------------------------------------------------
    audio::Automation automation;
    check(!automation.anyEnabled(), "no lanes enabled by default");

    audio::AutoLane& lane = automation.lane(audio::AutoTarget::FilterCutoff);
    lane.enabled = true;
    lane.lfo.shape = audio::Waveform::Sine;
    lane.lfo.rateHz = 1.0f;
    lane.lo = 500.0f;
    lane.hi = 5000.0f;
    check(automation.anyEnabled(), "enabling a lane is observable");

    // Applying at different times must move the target between its bounds.
    audio::AudioEngine engine;
    engine.initOffline();
    automation.apply(engine, 0.0); // sine=0 → unipolar 0.5 → midpoint
    const float cutoffMid = engine.mixer().eq().cutoff();
    automation.apply(engine, 0.25); // sine peak → unipolar 1 → hi bound
    const float cutoffHi = engine.mixer().eq().cutoff();
    automation.apply(engine, 0.75); // sine trough → unipolar 0 → lo bound
    const float cutoffLo = engine.mixer().eq().cutoff();

    check(std::fabs(cutoffMid - 2750.0f) < 50.0f, "midpoint sweep lands between the bounds");
    check(cutoffHi > 4900.0f, "peak sweep reaches the high bound");
    check(cutoffLo < 600.0f, "trough sweep reaches the low bound");
    check(engine.mixer().eq().enabled(), "an active cutoff lane switches the EQ on");

    // --- Automation clips (breakpoint envelopes) ------------------------------
    {
        // A clip ramps 0 → 1 over 0 → 2 s, then holds. It must interpolate linearly and take
        // priority over the LFO on the same lane.
        audio::AutoLane clipLane;
        clipLane.lfo.shape = audio::Waveform::Sine; // would give 0.5 at t=0 if it were used
        clipLane.clip = {{0.0, 0.0f}, {2.0, 1.0f}};
        clipLane.clipLength = 0.0; // hold past the last point

        check(std::fabs(clipLane.sourceUnipolar(0.0) - 0.0f) < 1e-4f, "clip holds first value at t=0");
        check(std::fabs(clipLane.sourceUnipolar(1.0) - 0.5f) < 1e-4f, "clip interpolates to midpoint");
        check(std::fabs(clipLane.sourceUnipolar(2.0) - 1.0f) < 1e-4f, "clip reaches last value");
        check(std::fabs(clipLane.sourceUnipolar(5.0) - 1.0f) < 1e-4f,
              "clip holds the last value past its end");

        // With a loop length, time wraps: at t = clipLength it is back to the start value.
        audio::AutoLane looped;
        looped.clip = {{0.0, 0.2f}, {1.0, 0.8f}};
        looped.clipLength = 2.0;
        check(std::fabs(looped.sourceUnipolar(0.0) - 0.2f) < 1e-4f, "looped clip starts at 0.2");
        check(std::fabs(looped.sourceUnipolar(2.0) - 0.2f) < 1e-4f,
              "looped clip wraps back to the start after clipLength");
        check(std::fabs(looped.sourceUnipolar(0.5) - 0.5f) < 1e-4f, "looped clip midpoint interpolates");
    }

    // --- Extended targets: delay mix + distortion drive ----------------------
    {
        audio::Automation autom;
        audio::AutoLane& dl = autom.lane(audio::AutoTarget::DelayMix);
        dl.enabled = true;
        dl.lfo.shape = audio::Waveform::Sine;
        dl.lfo.rateHz = 1.0f;
        dl.lo = 0.0f;
        dl.hi = 0.5f;
        audio::AutoLane& dd = autom.lane(audio::AutoTarget::DistDrive);
        dd.enabled = true;
        dd.lfo.shape = audio::Waveform::Sine;
        dd.lfo.rateHz = 1.0f;
        dd.lo = 2.0f;
        dd.hi = 8.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bounds
        check(eng.mixer().delay().enabled() && eng.mixer().delay().mix() > 0.45f,
              "automating delay mix drives (and enables) the delay");
        check(eng.mixer().distortion().enabled() && eng.mixer().distortion().drive() > 7.5f,
              "automating distortion drive drives (and enables) the distortion");
        autom.apply(eng, 0.75); // trough → lo bounds
        check(eng.mixer().delay().mix() < 0.05f, "delay-mix automation reaches its low bound");
    }

    // --- Bitcrusher-mix target (lo-fi drops/risers) --------------------------
    {
        audio::Automation autom;
        audio::AutoLane& bc = autom.lane(audio::AutoTarget::BitcrusherMix);
        bc.enabled = true;
        bc.lfo.shape = audio::Waveform::Sine;
        bc.lfo.rateHz = 1.0f;
        bc.lo = 0.0f;
        bc.hi = 1.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().bitcrusher().enabled() && eng.mixer().bitcrusher().mix() > 0.95f,
              "automating bitcrusher mix drives (and enables) the crusher");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().bitcrusher().mix() < 0.05f, "bitcrusher-mix automation reaches its low bound");
    }

    // --- Pitch-shift target (pitch dives/risers) -----------------------------
    {
        audio::Automation autom;
        audio::AutoLane& ps = autom.lane(audio::AutoTarget::PitchShift);
        ps.enabled = true;
        ps.lfo.shape = audio::Waveform::Sine;
        ps.lfo.rateHz = 1.0f;
        ps.lo = -12.0f;
        ps.hi = 12.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().pitchShifter().enabled() && eng.mixer().pitchShifter().semitones() > 11.0f,
              "automating pitch shift drives (and enables) the shifter up");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().pitchShifter().semitones() < -11.0f,
              "pitch-shift automation reaches its low (dive) bound");
    }

    // --- Vibrato-depth target (pitch-wobble swells) --------------------------
    {
        audio::Automation autom;
        audio::AutoLane& vd = autom.lane(audio::AutoTarget::VibratoDepth);
        vd.enabled = true;
        vd.lfo.shape = audio::Waveform::Sine;
        vd.lfo.rateHz = 1.0f;
        vd.lo = 0.0f;
        vd.hi = 10.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().vibrato().enabled() && eng.mixer().vibrato().depth() > 9.0f,
              "automating vibrato depth swells (and enables) the vibrato");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().vibrato().depth() < 1.0f,
              "vibrato-depth automation settles back to its low bound");
    }

    // --- Stereo-width target -------------------------------------------------
    {
        audio::Automation autom;
        audio::AutoLane& sw = autom.lane(audio::AutoTarget::StereoWidth);
        sw.enabled = true;
        sw.lfo.shape = audio::Waveform::Sine;
        sw.lfo.rateHz = 1.0f;
        sw.lo = 0.0f;
        sw.hi = 2.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound (2.0)
        check(eng.mixer().widener().enabled() && eng.mixer().widener().width() > 1.9f,
              "automating stereo width drives (and enables) the widener");
        autom.apply(eng, 0.75); // trough → lo bound (0 = mono)
        check(eng.mixer().widener().width() < 0.1f, "stereo-width automation reaches its low bound");
    }

    // --- Synth-cutoff target: sweeps the lead synth's own resonant filter ----
    {
        audio::Automation autom;
        audio::AutoLane& sc = autom.lane(audio::AutoTarget::SynthCutoff);
        sc.enabled = true;
        sc.lfo.shape = audio::Waveform::Sine;
        sc.lfo.rateHz = 1.0f;
        sc.lo = 300.0f;
        sc.hi = 7000.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        eng.sequencer().synth().setFilter(1000.0f, 5.0f, 0.0f); // known resonance to preserve
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.sequencer().synth().filterCutoff() > 6800.0f,
              "automating synth cutoff sweeps the lead filter to the high bound");
        check(std::fabs(eng.sequencer().synth().filterResonance() - 5.0f) < 1e-3f,
              "synth-cutoff automation preserves the filter resonance");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.sequencer().synth().filterCutoff() < 400.0f,
              "synth-cutoff automation reaches its low bound");
    }

    // --- Bass-synth filter: cutoff + resonance on synth2 --------------------
    {
        audio::Automation autom;
        audio::AutoLane& bc = autom.lane(audio::AutoTarget::BassCutoff);
        bc.enabled = true;
        bc.lfo.shape = audio::Waveform::Sine;
        bc.lfo.rateHz = 1.0f;
        bc.lo = 200.0f;
        bc.hi = 6000.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        eng.sequencer().synth2().setFilter(800.0f, 4.0f, 0.0f); // known resonance to preserve
        autom.apply(eng, 0.25); // peak → hi
        check(eng.sequencer().synth2().filterCutoff() > 5800.0f,
              "automating bass cutoff sweeps the bass filter to the high bound");
        check(std::fabs(eng.sequencer().synth2().filterResonance() - 4.0f) < 1e-3f,
              "bass-cutoff automation preserves the resonance");
        autom.apply(eng, 0.75); // trough → lo
        check(eng.sequencer().synth2().filterCutoff() < 400.0f,
              "bass-cutoff automation reaches its low bound");

        // Resonance sweep on the bass filter.
        audio::Automation ares;
        audio::AutoLane& br = ares.lane(audio::AutoTarget::BassResonance);
        br.enabled = true;
        br.lfo.shape = audio::Waveform::Sine;
        br.lfo.rateHz = 1.0f;
        br.lo = 0.7f;
        br.hi = 12.0f;
        audio::AudioEngine eng2;
        eng2.initOffline();
        eng2.sequencer().synth2().setFilter(1200.0f, 1.0f, 0.0f);
        ares.apply(eng2, 0.25); // peak → hi
        check(eng2.sequencer().synth2().filterResonance() > 11.0f,
              "automating bass resonance drives the Q to its high bound");
        check(std::fabs(eng2.sequencer().synth2().filterCutoff() - 1200.0f) < 1.0f,
              "bass-resonance automation preserves the cutoff");
    }

    // --- Tempo-synced automation LFO ----------------------------------------
    {
        audio::Automation autom;
        audio::AutoLane& sc = autom.lane(audio::AutoTarget::SynthCutoff);
        sc.enabled = true;
        sc.sync = true;
        sc.syncDiv = 4; // 1/4 → 1 cycle per beat
        sc.lo = 200.0f;
        sc.hi = 2000.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.0, 120.0); // 120 BPM → 1/4 = 2 Hz
        check(std::fabs(autom.lane(audio::AutoTarget::SynthCutoff).lfo.rateHz - 2.0f) < 0.01f,
              "a synced lane locks its LFO rate to the tempo (1/4 @120 = 2 Hz)");
        autom.lane(audio::AutoTarget::SynthCutoff).syncDiv = 5; // 1/8 → 2 cycles/beat
        autom.apply(eng, 0.0, 120.0);
        check(std::fabs(autom.lane(audio::AutoTarget::SynthCutoff).lfo.rateHz - 4.0f) < 0.01f,
              "a finer division doubles the synced LFO rate");

        // Off → the manual rate is left alone.
        audio::Automation manual;
        audio::AutoLane& ml = manual.lane(audio::AutoTarget::ReverbMix);
        ml.enabled = true;
        ml.sync = false;
        ml.lfo.rateHz = 1.5f;
        audio::AudioEngine eng2;
        eng2.initOffline();
        manual.apply(eng2, 0.0, 120.0);
        check(std::fabs(manual.lane(audio::AutoTarget::ReverbMix).lfo.rateHz - 1.5f) < 1e-3f,
              "an unsynced lane keeps its manual LFO rate");

        check(std::fabs(audio::Automation::syncRateHz(2, 120.0) - 0.5f) < 0.01f,
              "1 bar at 120 BPM is 0.5 Hz");
    }

    // --- Filter-resonance target: sweeps the lead synth's filter Q ----------
    {
        audio::Automation autom;
        audio::AutoLane& fr = autom.lane(audio::AutoTarget::FilterResonance);
        fr.enabled = true;
        fr.lfo.shape = audio::Waveform::Sine;
        fr.lfo.rateHz = 1.0f;
        fr.lo = 1.0f;
        fr.hi = 12.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        eng.sequencer().synth().setFilter(1200.0f, 2.0f, 0.0f); // known cutoff to preserve
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.sequencer().synth().filterResonance() > 11.0f,
              "automating filter resonance sweeps the Q to the high bound");
        check(std::fabs(eng.sequencer().synth().filterCutoff() - 1200.0f) < 1.0f,
              "filter-resonance automation preserves the cutoff");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.sequencer().synth().filterResonance() < 1.5f,
              "filter-resonance automation reaches its low bound");
    }

    // --- Lead-bus targets: volume + pan on the lead mixer strip -------------
    {
        audio::Automation autom;
        audio::AutoLane& lv = autom.lane(audio::AutoTarget::LeadVolume);
        lv.enabled = true;
        lv.lfo.shape = audio::Waveform::Sine;
        lv.lfo.rateHz = 1.0f;
        lv.lo = 0.0f;
        lv.hi = 1.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.0); // sine=0 → unipolar 0.5 → midpoint (non-unity gain)
        check(std::fabs(eng.mixer().track(audio::MixerBus::Lead).gain() - 0.5f) < 0.02f,
              "automating lead volume lands the lead strip gain at its midpoint");
        check(eng.mixer().track(audio::MixerBus::Lead).active(),
              "a non-unity lead-volume lane makes the lead strip active (engages the stem path)");
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().track(audio::MixerBus::Lead).gain() > 0.95f,
              "automating lead volume drives the lead strip gain to its high bound");
        autom.apply(eng, 0.75); // trough → lo bound (silence the lead)
        check(eng.mixer().track(audio::MixerBus::Lead).gain() < 0.05f,
              "lead-volume automation reaches its low bound");

        // Pan sweeps the lead strip's stereo balance from hard-left to hard-right.
        audio::Automation apan;
        audio::AutoLane& lp = apan.lane(audio::AutoTarget::LeadPan);
        lp.enabled = true;
        lp.lfo.shape = audio::Waveform::Sine;
        lp.lfo.rateHz = 1.0f;
        lp.lo = -1.0f;
        lp.hi = 1.0f;
        audio::AudioEngine eng3;
        eng3.initOffline();
        apan.apply(eng3, 0.25); // peak → hi bound (+1 = hard right)
        check(eng3.mixer().track(audio::MixerBus::Lead).pan() > 0.95f,
              "automating lead pan drives the balance hard right at the high bound");
        apan.apply(eng3, 0.75); // trough → lo bound (-1 = hard left)
        check(eng3.mixer().track(audio::MixerBus::Lead).pan() < -0.95f,
              "lead-pan automation reaches hard left at the low bound");
    }

    // --- Drum-bus targets: volume + pan on the drum mixer strip -------------
    {
        audio::Automation autom;
        audio::AutoLane& dv = autom.lane(audio::AutoTarget::DrumVolume);
        dv.enabled = true;
        dv.lfo.shape = audio::Waveform::Sine;
        dv.lfo.rateHz = 1.0f;
        dv.lo = 0.0f;
        dv.hi = 1.0f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().track(audio::MixerBus::Drums).gain() > 0.95f,
              "automating drum volume drives the drum strip gain to its high bound");
        autom.apply(eng, 0.75); // trough → lo bound (drum drop / silence)
        check(eng.mixer().track(audio::MixerBus::Drums).gain() < 0.05f,
              "drum-volume automation reaches its low bound (the drop)");

        // Pan sweeps the drum strip's stereo balance.
        audio::Automation apan;
        audio::AutoLane& dp = apan.lane(audio::AutoTarget::DrumPan);
        dp.enabled = true;
        dp.lfo.shape = audio::Waveform::Sine;
        dp.lfo.rateHz = 1.0f;
        dp.lo = -1.0f;
        dp.hi = 1.0f;
        audio::AudioEngine eng2;
        eng2.initOffline();
        apan.apply(eng2, 0.25); // peak → +1 hard right
        check(eng2.mixer().track(audio::MixerBus::Drums).pan() > 0.95f,
              "automating drum pan drives the balance hard right at the high bound");
        apan.apply(eng2, 0.75); // trough → -1 hard left
        check(eng2.mixer().track(audio::MixerBus::Drums).pan() < -0.95f,
              "drum-pan automation reaches hard left at the low bound");
    }

    // --- Time-fx targets: delay feedback + reverb size ----------------------
    {
        audio::Automation autom;
        audio::AutoLane& df = autom.lane(audio::AutoTarget::DelayFeedback);
        df.enabled = true;
        df.lfo.shape = audio::Waveform::Sine;
        df.lfo.rateHz = 1.0f;
        df.lo = 0.1f;
        df.hi = 0.8f;
        audio::AutoLane& rz = autom.lane(audio::AutoTarget::ReverbSize);
        rz.enabled = true;
        rz.lfo.shape = audio::Waveform::Sine;
        rz.lfo.rateHz = 1.0f;
        rz.lo = 0.3f;
        rz.hi = 0.95f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // peak → hi bounds
        check(eng.mixer().delay().enabled() && eng.mixer().delay().feedback() > 0.78f,
              "automating delay feedback drives it to the high bound (and enables the delay)");
        check(eng.mixer().reverb().enabled() && eng.mixer().reverb().roomSize() > 0.93f,
              "automating reverb size drives it to the high bound (and enables the reverb)");
        autom.apply(eng, 0.75); // trough → lo bounds
        check(eng.mixer().delay().feedback() < 0.12f, "delay-feedback automation reaches its low bound");
        check(eng.mixer().reverb().roomSize() < 0.32f, "reverb-size automation reaches its low bound");
    }

    // --- Aux-send targets: parallel reverb + delay send levels --------------
    {
        audio::Automation autom;
        audio::AutoLane& rs = autom.lane(audio::AutoTarget::ReverbSend);
        rs.enabled = true;
        rs.lfo.shape = audio::Waveform::Sine;
        rs.lfo.rateHz = 1.0f;
        rs.lo = 0.0f;
        rs.hi = 0.8f;
        audio::AutoLane& ds = autom.lane(audio::AutoTarget::DelaySend);
        ds.enabled = true;
        ds.lfo.shape = audio::Waveform::Sine;
        ds.lfo.rateHz = 1.0f;
        ds.lo = 0.0f;
        ds.hi = 0.7f;

        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bounds
        check(eng.mixer().reverbSend() > 0.78f,
              "automating reverb send drives the send to its high bound");
        check(eng.mixer().delaySend() > 0.68f,
              "automating delay send drives the send to its high bound");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().reverbSend() < 0.02f && eng.mixer().delaySend() < 0.02f,
              "aux-send automation reaches its low bound");
    }

    // --- Master-pan target: whole-mix auto-pan ------------------------------
    {
        audio::Automation autom;
        audio::AutoLane& mp = autom.lane(audio::AutoTarget::MasterPan);
        mp.enabled = true;
        mp.lfo.shape = audio::Waveform::Sine;
        mp.lfo.rateHz = 1.0f;
        mp.lo = -1.0f;
        mp.hi = 1.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // peak → +1 (hard right)
        check(eng.mixer().masterBalance() > 0.95f,
              "automating master pan drives the balance hard right at the high bound");
        autom.apply(eng, 0.75); // trough → -1 (hard left)
        check(eng.mixer().masterBalance() < -0.95f,
              "master-pan automation reaches hard left at the low bound");
    }

    // --- Bass-bus targets: volume + pan on the bass mixer strip -------------
    {
        audio::Automation autom;
        audio::AutoLane& bv = autom.lane(audio::AutoTarget::BassVolume);
        bv.enabled = true;
        bv.lfo.shape = audio::Waveform::Sine;
        bv.lfo.rateHz = 1.0f;
        bv.lo = 0.0f;
        bv.hi = 1.0f;
        audio::AutoLane& bp = autom.lane(audio::AutoTarget::BassPan);
        bp.enabled = true;
        bp.lfo.shape = audio::Waveform::Sine;
        bp.lfo.rateHz = 1.0f;
        bp.lo = -1.0f;
        bp.hi = 1.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // peak → hi bounds
        check(eng.mixer().track(audio::MixerBus::Bass).gain() > 0.95f,
              "automating bass volume drives the bass strip gain to its high bound");
        check(eng.mixer().track(audio::MixerBus::Bass).pan() > 0.95f,
              "automating bass pan drives the balance hard right at the high bound");
        autom.apply(eng, 0.75); // trough → lo bounds
        check(eng.mixer().track(audio::MixerBus::Bass).gain() < 0.05f &&
                  eng.mixer().track(audio::MixerBus::Bass).pan() < -0.95f,
              "bass volume/pan automation reaches its low bounds");
    }

    // A disabled lane leaves its target untouched.
    audio::Automation idle;
    audio::AudioEngine engine2;
    engine2.initOffline();
    const float before = engine2.mixer().masterGain();
    idle.apply(engine2, 0.3);
    check(std::fabs(engine2.mixer().masterGain() - before) < 1e-6f,
          "disabled lanes do not touch their targets");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
