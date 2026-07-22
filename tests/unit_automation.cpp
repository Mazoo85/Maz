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

    // --- Sample & hold LFO ----------------------------------------------------
    {
        audio::LFO sh;
        sh.shape = audio::Waveform::Sine; // ignored while sampleHold is on
        sh.rateHz = 1.0f;                 // one new random value per second
        sh.sampleHold = true;
        // Holds constant within a cycle: t=0.1 and t=0.9 fall in the same step → identical value.
        check(std::fabs(sh.valueBipolar(0.1) - sh.valueBipolar(0.9)) < 1e-6f,
              "sample & hold holds one value for the whole cycle");
        // Deterministic: the same t always gives the same value.
        check(sh.valueBipolar(3.4) == sh.valueBipolar(3.4),
              "sample & hold is deterministic (pure function of t)");
        // Stepped: different cycles almost always give different values, and values span the range.
        int distinct = 0;
        float prev = sh.valueBipolar(0.5);
        float minV = prev, maxV = prev;
        for (int k = 1; k < 20; ++k) {
            const float v = sh.valueBipolar(static_cast<double>(k) + 0.5);
            if (std::fabs(v - prev) > 1e-6f) ++distinct;
            minV = std::min(minV, v);
            maxV = std::max(maxV, v);
            prev = v;
            check(v >= -1.0f && v <= 1.0f, "sample & hold stays in [-1, 1]");
        }
        check(distinct >= 15, "sample & hold jumps to a new value most cycles (stepped/random)");
        check(maxV > 0.4f && minV < -0.4f, "sample & hold spans a wide range");
        // Off by default → the periodic shape is used (a sine at t=0.25 peaks).
        audio::LFO def;
        def.rateHz = 1.0f;
        check(!def.sampleHold && def.valueBipolar(0.25) > 0.99f,
              "sample & hold defaults off (periodic shape)");
    }

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

    // --- Newer automation targets drive their effects -----------------------
    {
        audio::AudioEngine eng;
        eng.initOffline();
        audio::Automation au;
        // lo == hi → the swept value is constant, so the check is time-independent.
        auto fixLane = [&](audio::AutoTarget t, float val) {
            audio::AutoLane& L = au.lane(t);
            L.enabled = true;
            L.lo = val;
            L.hi = val;
        };
        fixLane(audio::AutoTarget::ReverbShimmer, 0.7f);
        fixLane(audio::AutoTarget::PhaserRate, 3.0f);
        fixLane(audio::AutoTarget::FlangerRate, 2.0f);
        fixLane(audio::AutoTarget::AmpCabDrive, 0.8f);
        fixLane(audio::AutoTarget::AutoPanRate, 5.0f);
        fixLane(audio::AutoTarget::CombFrequency, 440.0f);
        fixLane(audio::AutoTarget::OctaverAmount, 0.6f);
        fixLane(audio::AutoTarget::ConvolverMix, 0.5f);
        fixLane(audio::AutoTarget::DistortionBias, 0.4f);
        fixLane(audio::AutoTarget::BeatRepeatMix, 0.9f);
        fixLane(audio::AutoTarget::FormantVowel, 2.5f);
        fixLane(audio::AutoTarget::CompThreshold, -20.0f);
        fixLane(audio::AutoTarget::MasterTune, -50.0f);
        au.apply(eng, 0.0);
        check(std::fabs(eng.sequencer().masterTune() - (-50.0f)) < 1e-3f,
              "master-tune automation drives the project concert pitch");
        check(std::fabs(eng.mixer().reverb().shimmer() - 0.7f) < 1e-3f &&
                  eng.mixer().reverb().enabled(),
              "reverb-shimmer automation drives the shimmer and enables the reverb");
        check(std::fabs(eng.mixer().phaser().rate() - 3.0f) < 1e-3f,
              "phaser-rate automation drives the phaser rate");
        check(std::fabs(eng.mixer().flanger().rate() - 2.0f) < 1e-3f,
              "flanger-rate automation drives the flanger rate");
        check(std::fabs(eng.mixer().ampCab().drive() - 0.8f) < 1e-3f &&
                  eng.mixer().ampCab().enabled(),
              "amp-drive automation drives the amp/cab and enables it");
        check(std::fabs(eng.mixer().autopan().rate() - 5.0f) < 1e-3f,
              "auto-pan-rate automation drives the auto-pan rate");
        check(std::fabs(eng.mixer().comb().frequency() - 440.0f) < 1e-3f,
              "comb-frequency automation drives the comb resonator pitch");
        check(std::fabs(eng.mixer().octaver().amount() - 0.6f) < 1e-3f &&
                  eng.mixer().octaver().enabled(),
              "octaver-amount automation drives the octaver and enables it");
        check(std::fabs(eng.mixer().convolver().mix() - 0.5f) < 1e-3f &&
                  eng.mixer().convolver().enabled(),
              "convolver-mix automation drives the convolver and enables it");
        check(std::fabs(eng.mixer().distortion().bias() - 0.4f) < 1e-3f &&
                  eng.mixer().distortion().enabled(),
              "distortion-bias automation drives the bias and enables the distortion");
        check(std::fabs(eng.mixer().beatRepeat().mix() - 0.9f) < 1e-3f &&
                  eng.mixer().beatRepeat().enabled(),
              "beat-repeat-mix automation drives the stutter wet and enables it");
        check(std::fabs(eng.mixer().formant().morph() - 2.5f) < 1e-3f &&
                  eng.mixer().formant().enabled() && eng.mixer().formant().morphEnabled(),
              "formant-vowel automation drives the morph and enables the formant filter");
        check(std::fabs(eng.mixer().compressor().thresholdDb() - (-20.0f)) < 1e-3f &&
                  eng.mixer().compressor().enabled(),
              "comp-threshold automation drives the master compressor threshold and enables it");
    }

    // --- Reusable helpers: applyTargetValue + evalPoints --------------------
    {
        // applyTargetValue writes a value straight onto a target (the same mapping apply() uses),
        // including auto-enabling the effect. This is the path timeline automation clips will use.
        audio::AudioEngine eng;
        eng.initOffline();
        audio::Automation::applyTargetValue(eng, audio::AutoTarget::FilterCutoff, 3000.0f);
        check(std::fabs(eng.mixer().eq().cutoff() - 3000.0f) < 1e-3f && eng.mixer().eq().enabled(),
              "applyTargetValue drives a target directly and auto-enables its effect");
        audio::Automation::applyTargetValue(eng, audio::AutoTarget::MasterTune, -40.0f);
        check(std::fabs(eng.sequencer().masterTune() - (-40.0f)) < 1e-3f,
              "applyTargetValue reaches a sequencer target too");

        // evalPoints reproduces the drawn-clip interpolation used by AutoLane::sourceUnipolar: a two
        // point 0→1 ramp reads its midpoint at 0.5, holds the endpoints, and (with a loop length) wraps.
        const std::vector<audio::AutoPoint> ramp = {{0.0, 0.0f, 0.0f}, {1.0, 1.0f, 0.0f}};
        check(std::fabs(audio::Automation::evalPoints(ramp, 0.0, 0.0) - 0.0f) < 1e-6f,
              "evalPoints holds the first point at t=0");
        check(std::fabs(audio::Automation::evalPoints(ramp, 0.5, 0.0) - 0.5f) < 1e-6f,
              "evalPoints interpolates the ramp midpoint");
        check(std::fabs(audio::Automation::evalPoints(ramp, 2.0, 0.0) - 1.0f) < 1e-6f,
              "evalPoints holds the last point past the end when not looping");
        check(std::fabs(audio::Automation::evalPoints(ramp, 1.5, 1.0) - 0.5f) < 1e-6f,
              "evalPoints loops with a positive loop length (1.5 → 0.5)");
        check(audio::Automation::evalPoints({}, 0.5, 0.0) == 0.0f, "evalPoints returns 0 for no points");
        // Parity with the lane path: a lane holding the same clip evaluates identically.
        audio::AutoLane clipLane;
        clipLane.clip = ramp;
        clipLane.clipLength = 0.0;
        check(std::fabs(clipLane.sourceUnipolar(0.5) - audio::Automation::evalPoints(ramp, 0.5, 0.0)) <
                  1e-6f,
              "sourceUnipolar and evalPoints agree on a drawn clip");
    }

    // --- LFO phase offset ----------------------------------------------------
    {
        // A quarter-cycle phase offset shifts a sine LFO to its peak at t=0 (sin of a quarter turn),
        // whereas no offset starts it at the zero crossing. This lets lanes run out of phase.
        audio::LFO base;
        base.shape = audio::Waveform::Sine;
        base.rateHz = 1.0f;
        audio::LFO shifted = base;
        shifted.phase = 0.25f;
        check(std::fabs(base.valueBipolar(0.0)) < 1e-4f, "a sine LFO starts at zero with no phase offset");
        check(shifted.valueBipolar(0.0) > 0.99f, "a 0.25 phase offset starts the sine LFO at its peak");
        // A full-cycle offset is a no-op (phase wraps modulo 1).
        audio::LFO whole = base;
        whole.phase = 1.0f;
        check(std::fabs(whole.valueBipolar(0.3) - base.valueBipolar(0.3)) < 1e-4f,
              "a whole-cycle phase offset is equivalent to none (wraps mod 1)");
    }

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

        // Curve tension warps the segment while preserving both endpoints. A 0→1 ramp with positive
        // tension on the starting point rises above the linear midpoint (fast start / ease-out);
        // negative tension dips below it (slow start / ease-in); 0 stays exactly linear.
        audio::AutoLane curved;
        curved.clip = {{0.0, 0.0f}, {2.0, 1.0f}};
        curved.clipLength = 0.0;
        curved.clip[0].tension = 0.6f;
        check(std::fabs(curved.sourceUnipolar(0.0) - 0.0f) < 1e-4f &&
                  std::fabs(curved.sourceUnipolar(2.0) - 1.0f) < 1e-4f,
              "a tensioned segment still hits both breakpoints exactly");
        check(curved.sourceUnipolar(1.0) > 0.55f, "positive tension rises above the linear midpoint");
        curved.clip[0].tension = -0.6f;
        check(curved.sourceUnipolar(1.0) < 0.45f, "negative tension dips below the linear midpoint");
        curved.clip[0].tension = 0.0f;
        check(std::fabs(curved.sourceUnipolar(1.0) - 0.5f) < 1e-4f,
              "zero tension is exactly linear (bit-identical)");

        // Edge cases: a single-point clip holds its value at every time (before, at, and after the
        // point) — it must never fall through to the interpolation loop.
        audio::AutoLane single;
        single.clip = {{1.0, 0.42f}};
        check(std::fabs(single.sourceUnipolar(0.0) - 0.42f) < 1e-4f &&
                  std::fabs(single.sourceUnipolar(1.0) - 0.42f) < 1e-4f &&
                  std::fabs(single.sourceUnipolar(9.0) - 0.42f) < 1e-4f,
              "a single-point clip holds its value everywhere");

        // Two points at the SAME time must not divide by zero — the span==0 guard returns the left
        // value rather than producing NaN/Inf.
        audio::AutoLane coincident;
        coincident.clip = {{0.0, 0.1f}, {1.0, 0.3f}, {1.0, 0.9f}, {2.0, 0.5f}};
        const float atDup = coincident.sourceUnipolar(1.0);
        check(std::isfinite(atDup), "coincident-time clip points do not divide by zero (finite output)");
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

    // --- Ring-mod frequency target (metallic sweeps) -------------------------
    {
        audio::Automation autom;
        audio::AutoLane& rm = autom.lane(audio::AutoTarget::RingModFreq);
        rm.enabled = true;
        rm.lfo.shape = audio::Waveform::Sine;
        rm.lfo.rateHz = 1.0f;
        rm.lo = 30.0f;
        rm.hi = 1500.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().ringmod().enabled() && eng.mixer().ringmod().freq() > 1400.0f,
              "automating ring-mod frequency sweeps (and enables) the carrier up");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().ringmod().freq() < 100.0f,
              "ring-mod-frequency automation reaches its low bound");
    }

    // --- Chorus-mix target (wet swells) --------------------------------------
    {
        audio::Automation autom;
        audio::AutoLane& cm = autom.lane(audio::AutoTarget::ChorusMix);
        cm.enabled = true;
        cm.lfo.shape = audio::Waveform::Sine;
        cm.lfo.rateHz = 1.0f;
        cm.lo = 0.0f;
        cm.hi = 1.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().chorus().enabled() && eng.mixer().chorus().mix() > 0.9f,
              "automating chorus mix swells (and enables) the wet signal");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().chorus().mix() < 0.1f,
              "chorus-mix automation fades back to dry");
    }

    // --- Reverb-damping target (evolving space) ------------------------------
    {
        audio::Automation autom;
        audio::AutoLane& rd = autom.lane(audio::AutoTarget::ReverbDamping);
        rd.enabled = true;
        rd.lfo.shape = audio::Waveform::Sine;
        rd.lfo.rateHz = 1.0f;
        rd.lo = 0.0f;
        rd.hi = 1.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().reverb().enabled() && eng.mixer().reverb().damping() > 0.9f,
              "automating reverb damping darkens (and enables) the tail");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().reverb().damping() < 0.1f,
              "reverb-damping automation opens the tail back up");
    }

    // --- Frequency-shift target (evolving metallic textures) -----------------
    {
        audio::Automation autom;
        audio::AutoLane& fsl = autom.lane(audio::AutoTarget::FreqShift);
        fsl.enabled = true;
        fsl.lfo.shape = audio::Waveform::Sine;
        fsl.lfo.rateHz = 1.0f;
        fsl.lo = -500.0f;
        fsl.hi = 500.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().freqShifter().enabled() && eng.mixer().freqShifter().shiftHz() > 450.0f,
              "automating frequency shift sweeps (and enables) the shifter up");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().freqShifter().shiftHz() < -450.0f,
              "frequency-shift automation reaches its low (downward) bound");
    }

    // --- Rotary-rate target (Leslie slow/fast ramp) --------------------------
    {
        audio::Automation autom;
        audio::AutoLane& rr = autom.lane(audio::AutoTarget::RotaryRate);
        rr.enabled = true;
        rr.lfo.shape = audio::Waveform::Sine;
        rr.lfo.rateHz = 1.0f;
        rr.lo = 0.8f;
        rr.hi = 7.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound (fast/tremolo)
        check(eng.mixer().rotary().enabled() && eng.mixer().rotary().rate() > 6.5f,
              "automating rotary rate ramps (and enables) it up to tremolo speed");
        autom.apply(eng, 0.75); // trough → lo bound (slow/chorale)
        check(eng.mixer().rotary().rate() < 1.2f,
              "rotary-rate automation drops back to chorale speed");
    }

    // --- Delay-time target (tape-warp echoes) --------------------------------
    {
        audio::Automation autom;
        audio::AutoLane& dt = autom.lane(audio::AutoTarget::DelayTime);
        dt.enabled = true;
        dt.lfo.shape = audio::Waveform::Sine;
        dt.lfo.rateHz = 1.0f;
        dt.lo = 40.0f;
        dt.hi = 400.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().delay().enabled() && eng.mixer().delay().time() > 380.0f,
              "automating delay time sweeps (and enables) the echo time up");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().delay().time() < 60.0f,
              "delay-time automation reaches its short bound");
    }

    // --- Tremolo-depth target (gate-in over a build) -------------------------
    {
        audio::Automation autom;
        audio::AutoLane& td = autom.lane(audio::AutoTarget::TremoloDepth);
        td.enabled = true;
        td.lfo.shape = audio::Waveform::Sine;
        td.lfo.rateHz = 1.0f;
        td.lo = 0.0f;
        td.hi = 1.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.mixer().tremolo().enabled() && eng.mixer().tremolo().depth() > 0.9f,
              "automating tremolo depth brings the gating in (and enables it)");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.mixer().tremolo().depth() < 0.1f,
              "tremolo-depth automation fades the gating back out");
    }

    // --- Master filter sweep + resonance targets -----------------------------
    {
        audio::Automation autom;
        audio::AutoLane& fc = autom.lane(audio::AutoTarget::MasterFilterCutoff);
        fc.enabled = true;
        fc.lfo.shape = audio::Waveform::Sine;
        fc.lfo.rateHz = 1.0f;
        fc.lo = 200.0f;
        fc.hi = 12000.0f;
        audio::AutoLane& fq = autom.lane(audio::AutoTarget::MasterFilterReso);
        fq.enabled = true;
        fq.lfo.shape = audio::Waveform::Sine;
        fq.lfo.rateHz = 1.0f;
        fq.lo = 0.7f;
        fq.hi = 15.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // peak → hi bounds
        check(eng.mixer().filter().enabled() && eng.mixer().filter().cutoff() > 11000.0f &&
                  eng.mixer().filter().resonance() > 14.0f,
              "automating the master filter opens the cutoff and peaks the resonance");
        autom.apply(eng, 0.75); // trough → lo bounds
        check(eng.mixer().filter().cutoff() < 400.0f && eng.mixer().filter().resonance() < 1.0f,
              "master-filter automation sweeps back down");
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

    // --- Wavetable-position target (Serum/FL-style timbre morph sweeps) -------
    {
        audio::Automation autom;
        audio::AutoLane& wp = autom.lane(audio::AutoTarget::WavetablePosition);
        wp.enabled = true;
        wp.lfo.shape = audio::Waveform::Sine;
        wp.lfo.rateHz = 1.0f;
        wp.lo = 0.0f;
        wp.hi = 1.0f;
        audio::AudioEngine eng;
        eng.initOffline();
        eng.sequencer().synth().setMode(audio::SynthMode::Wavetable);
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.sequencer().synth().wavetablePosition() > 0.95f,
              "automating wavetable position scans the lead synth to the top of the table");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.sequencer().synth().wavetablePosition() < 0.05f,
              "wavetable-position automation reaches its low bound (back to table start)");
        // The lane is the last real target — proves the enum widened correctly and the apply switch,
        // name table and default bounds all cover it (a missing case would fail to compile under
        // -Werror, and a missing default bound would leave hi at the fallback 1.0).
        check(std::string(audio::Automation::targetName(audio::AutoTarget::WavetablePosition)) ==
                  "Wavetable Pos",
              "the wavetable-position target has a UI label");
    }

    // --- Synth pulse-width target (manual PWM sweeps on the Square wave) ------
    {
        audio::Automation autom;
        audio::AutoLane& pw = autom.lane(audio::AutoTarget::SynthPulseWidth);
        pw.enabled = true;
        pw.lfo.shape = audio::Waveform::Sine;
        pw.lfo.rateHz = 1.0f;
        pw.lo = 0.5f;
        pw.hi = 0.95f;
        audio::AudioEngine eng;
        eng.initOffline();
        eng.sequencer().synth().setWaveform(audio::Waveform::Square);
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(eng.sequencer().synth().pulseWidth() > 0.94f,
              "automating synth PWM widens the square duty cycle toward the thin-pulse bound");
        autom.apply(eng, 0.75); // trough → lo bound
        check(std::fabs(eng.sequencer().synth().pulseWidth() - 0.5f) < 0.01f,
              "synth-PWM automation returns the duty cycle to a plain square at the low bound");
        check(std::string(audio::Automation::targetName(audio::AutoTarget::SynthPulseWidth)) ==
                  "Synth PWM",
              "the synth-PWM target has a UI label");
    }

    // --- Sampler-start target (glitch/stutter sample-start modulation) --------
    {
        audio::Automation autom;
        audio::AutoLane& ss = autom.lane(audio::AutoTarget::SamplerStart);
        ss.enabled = true;
        ss.lfo.shape = audio::Waveform::Sine;
        ss.lfo.rateHz = 1.0f;
        ss.lo = 0.0f;
        ss.hi = 0.5f;
        audio::AudioEngine eng;
        eng.initOffline();
        autom.apply(eng, 0.25); // sine peak → unipolar 1 → hi bound
        check(std::fabs(eng.sequencer().sampler().startOffset() - 0.5f) < 0.01f,
              "automating sampler start pushes the note start halfway into the sample");
        autom.apply(eng, 0.75); // trough → lo bound
        check(eng.sequencer().sampler().startOffset() < 0.01f,
              "sampler-start automation returns the start to the head of the sample");
        check(std::string(audio::Automation::targetName(audio::AutoTarget::SamplerStart)) ==
                  "Sampler Start",
              "the sampler-start target has a UI label");
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
