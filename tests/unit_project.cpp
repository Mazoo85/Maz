// Unit test for .cjc project save/load — a full round-trip of the musical + mixing state through a
// file, with no audio device. Proves loadProject(saveProject(x)) == x for tempo, the drum grid, the
// piano-roll notes, bus gains, and every mixer effect parameter.

#include "maz/audio/Automation.hpp"
#include "maz/audio/Mixer.hpp"
#include "maz/audio/ProjectIO.hpp"
#include "maz/audio/Sequencer.hpp"

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

bool near(float a, float b) {
    return std::fabs(a - b) < 1e-3f;
}

} // namespace

int main() {
    // Build a project with distinctive, non-default state everywhere.
    audio::Sequencer seq;
    audio::Mixer mixer;
    audio::Automation automation;

    seq.setBpm(137.0);
    seq.setDrumGain(0.8f);
    seq.setSynthGain(1.2f);
    seq.setLeadPan(-0.5f);
    seq.setBassPan(0.3f);
    seq.setTranspose(-7);
    seq.setNumSteps(32);
    seq.setStepsPerBeat(3);
    seq.setStep(0, 0, true);
    seq.setStep(1, 4, true);
    seq.setStep(2, 7, true);
    seq.setStep(0, 24, true); // a step only present at the longer length
    seq.setStepProbability(1, 4, 0.5f);
    seq.setStepRatchet(2, 7, 3);
    seq.setChannelChokeGroup(0, 2);
    seq.setChannelTune(0, -5.0f);
    seq.setChannelDecay(0, 2.5f);
    audio::Note n1{0, 4, 60, 0.9f};
    audio::Note n2{8, 2, 67, 0.7f};
    seq.roll().addNote(n1);
    seq.roll().addNote(n2);
    seq.synth().setMode(audio::SynthMode::FM);
    seq.synth().setWaveform(audio::Waveform::Triangle);
    seq.synth().setEnvelope(0.01f, 0.2f, 0.4f, 0.5f);
    seq.synth().setFmRatio(3.5f);
    seq.synth().setFmIndex(6.0f);
    seq.synth().setGlide(0.15f);
    seq.synth().setUnison(5, 18.0f);
    seq.synth().setSubWaveform(audio::Waveform::Square);
    seq.synth().setVibrato(6.0f, 25.0f);
    seq.synth().setNoiseColor(0.7f);
    seq.synth().setHardSync(true);
    seq.synth().setSyncRatio(3.25f);
    seq.synth().setPitchEnv(-7.0f, 0.08f);
    seq.setArp(true, 2);
    seq.setArpOctaves(3);
    seq.sampler().setBasePitch(48);
    seq.sampler().setStartOffset(0.25f);
    seq.sampler().setAmpEnv(0.02f, 0.3f);
    seq.setUseSampler(true);
    // Second instrument: a bass note on roll2 + a distinct synth2 patch.
    seq.roll2().addNote(audio::Note{2, 6, 40, 0.85f});
    seq.synth2().setWaveform(audio::Waveform::Square);
    seq.synth2().setFilter(700.0f, 4.0f, 900.0f);
    seq.synth2().setMode(audio::SynthMode::Wavetable);
    seq.synth2().setWavetablePosition(0.65f);
    seq.synth2().setWavetableMorph(0.4f);
    seq.synth2().setWavetableFrames(audio::Waveform::Saw, audio::Waveform::Square,
                                    audio::Waveform::Triangle, audio::Waveform::Sine);
    // A second pattern + a playlist arrangement.
    const int p1 = seq.addPattern();
    seq.selectPattern(p1);
    seq.setStep(2, 5, true);
    seq.roll().addNote(audio::Note{4, 1, 72, 0.5f, 0.6f});
    seq.setPatternName(p1, "Chorus Fill");
    seq.selectPattern(0);
    seq.setPlaylist({0, 1, 0});
    seq.setSongMode(true);
    seq.setSongLoop(false);

    mixer.setMasterGain(0.75f);
    mixer.setLimiterCeiling(0.9f);
    mixer.eq().setEnabled(true);
    mixer.eq().setCutoff(3200.0f);
    mixer.compressor().setEnabled(true);
    mixer.compressor().setThresholdDb(-20.0f);
    mixer.compressor().setRatio(6.0f);
    mixer.compressor().setMakeupDb(4.0f);
    mixer.compressor().setKneeDb(6.0f);
    mixer.delay().setEnabled(true);
    mixer.delay().setTime(250.0f);
    mixer.delay().setMix(0.4f);
    mixer.delay().setPingPong(true);
    mixer.delay().setDamping(0.4f);
    mixer.reverb().setEnabled(true);
    mixer.reverb().setRoomSize(0.85f);
    mixer.reverb().setMix(0.33f);
    mixer.reverb().setPreDelayMs(35.0f);
    mixer.reverb().setWidth(1.5f);
    mixer.highpass().setEnabled(true);
    mixer.highpass().setCutoff(45.0f);
    mixer.tilt().setEnabled(true);
    mixer.tilt().setTilt(-6.0f);
    mixer.exciter().setEnabled(true);
    mixer.exciter().setCrossover(6500.0f);
    mixer.exciter().setAmount(0.42f);
    mixer.transient().setEnabled(true);
    mixer.transient().setAttack(0.6f);
    mixer.transient().setSustain(-0.3f);
    mixer.autopan().setEnabled(true);
    mixer.autopan().setRate(2.5f);
    mixer.autopan().setDepth(0.8f);
    mixer.monobass().setEnabled(true);
    mixer.monobass().setCrossover(90.0f);
    mixer.distortion().setEnabled(true);
    mixer.distortion().setCurve(audio::Distortion::Curve::Fold);
    mixer.ringmod().setEnabled(true);
    mixer.ringmod().setFreq(440.0f);
    mixer.ringmod().setMix(0.7f);
    mixer.flanger().setEnabled(true);
    mixer.flanger().setRate(0.4f);
    mixer.flanger().setFeedback(0.6f);
    mixer.gate().setEnabled(true);
    mixer.gate().setThresholdDb(-38.0f);
    mixer.gate().setRatio(5.0f);
    mixer.gate().setRangeDb(-55.0f);
    mixer.gate().setHoldMs(50.0f);
    mixer.widener().setEnabled(true);
    mixer.widener().setWidth(1.6f);
    mixer.tape().setEnabled(true);
    mixer.tape().setDrive(4.5f);
    mixer.tape().setWarmth(0.6f);
    mixer.tape().setMix(0.9f);
    // Aux send/return buses.
    mixer.setReverbSend(0.45f);
    mixer.reverbReturn().setRoomSize(0.6f);
    mixer.setDelaySend(0.3f);
    mixer.delayReturn().setTime(180.0f);
    // Per-bus mixer-track inserts.
    mixer.track(audio::MixerBus::Drums).setGain(0.8f);
    mixer.track(audio::MixerBus::Drums).distortion().setEnabled(true);
    mixer.track(audio::MixerBus::Drums).distortion().setDrive(6.0f);
    mixer.track(audio::MixerBus::Lead).setMuted(true);
    mixer.track(audio::MixerBus::Bass).eq().setEnabled(true);
    mixer.track(audio::MixerBus::Bass).eq().setLowGain(4.5f);

    audio::AutoLane& lane = automation.lane(audio::AutoTarget::FilterCutoff);
    lane.enabled = true;
    lane.lfo.shape = audio::Waveform::Saw;
    lane.lfo.rateHz = 1.75f;
    lane.lo = 300.0f;
    lane.hi = 5500.0f;
    // A breakpoint automation clip on a second lane.
    audio::AutoLane& reverbLane = automation.lane(audio::AutoTarget::ReverbMix);
    reverbLane.enabled = true;
    reverbLane.clip = {{0.0, 0.1f}, {1.5, 0.9f}, {3.0, 0.3f}};
    reverbLane.clipLength = 4.0;

    const std::string path = "unit_project_roundtrip.cjc";
    std::string err;
    check(audio::saveProject(path, seq, mixer, automation, &err), "saveProject succeeds");

    // Load into fresh, differently-initialised objects.
    audio::Sequencer seq2;
    audio::Mixer mixer2;
    audio::Automation automation2;
    seq2.setBpm(90.0); // will be overwritten
    check(audio::loadProject(path, seq2, mixer2, automation2, &err), "loadProject succeeds");

    // Transport + bus.
    check(near(static_cast<float>(seq2.bpm()), 137.0f), "bpm round-trips");
    check(near(seq2.drumGain(), 0.8f) && near(seq2.synthGain(), 1.2f), "bus gains round-trip");
    check(near(seq2.leadPan(), -0.5f) && near(seq2.bassPan(), 0.3f), "melodic bus pans round-trip");
    check(seq2.transpose() == -7, "global transpose round-trips");

    // Drum grid (pattern 0).
    check(seq2.numSteps() == 32, "pattern length round-trips");
    check(seq2.stepsPerBeat() == 3, "steps-per-beat round-trips");
    check(seq2.step(0, 0) && seq2.step(1, 4) && seq2.step(2, 7) && seq2.step(0, 24),
          "active steps round-trip (incl. the extended range)");
    check(std::fabs(seq2.stepProbability(1, 4) - 0.5f) < 0.01f &&
              seq2.stepProbability(0, 0) == 1.0f,
          "per-step probability round-trips");
    check(seq2.stepRatchet(2, 7) == 3 && seq2.stepRatchet(0, 0) == 1,
          "per-step ratchet round-trips");
    check(seq2.channelChokeGroup(0) == 2, "channel choke group round-trips");
    check(near(seq2.channelTune(0), -5.0f), "channel tune round-trips");
    check(near(seq2.channelDecay(0), 2.5f), "channel decay round-trips");
    check(!seq2.step(0, 1) && !seq2.step(3, 0), "inactive steps stay off");

    // Arrangement: patterns, per-pattern content, playlist, song mode.
    check(seq2.patternCount() == 2, "pattern count round-trips");
    check(seq2.patternName(1) == "Chorus Fill", "pattern name round-trips");
    check(seq2.patternName(0) == "Pattern 1", "default pattern name is preserved");
    check(seq2.songMode() && !seq2.songLoop(), "song mode + play-once flag round-trip");
    check(seq2.playlist().size() == 3 && seq2.playlist()[0] == 0 && seq2.playlist()[1] == 1 &&
              seq2.playlist()[2] == 0,
          "playlist round-trips");
    seq2.selectPattern(1);
    check(seq2.step(2, 5) && seq2.roll().notes().size() == 1, "second pattern content round-trips");
    check(near(seq2.roll().notes()[0].probability, 0.6f), "per-note probability round-trips");
    seq2.selectPattern(0);

    // Second instrument round-trips (roll2 note + synth2 patch).
    check(seq2.roll2().notes().size() == 1 && seq2.roll2().notes()[0].pitch == 40,
          "second-instrument (bass) notes round-trip");
    check(seq2.synth2().waveform() == audio::Waveform::Square, "synth2 patch round-trips");
    check(seq2.synth2().mode() == audio::SynthMode::Wavetable &&
              near(seq2.synth2().wavetablePosition(), 0.65f) &&
              near(seq2.synth2().wavetableMorph(), 0.4f),
          "synth2 wavetable mode + position/morph round-trip");
    check(seq2.synth2().wavetableFrame(0) == audio::Waveform::Saw &&
              seq2.synth2().wavetableFrame(3) == audio::Waveform::Sine,
          "custom wavetable frames round-trip");

    // Piano-roll notes.
    check(seq2.roll().notes().size() == 2, "note count round-trips");
    bool notesOk = false;
    if (seq2.roll().notes().size() == 2) {
        const audio::Note& a = seq2.roll().notes()[0];
        const audio::Note& b = seq2.roll().notes()[1];
        notesOk = a.startStep == 0 && a.lengthSteps == 4 && a.pitch == 60 && near(a.velocity, 0.9f) &&
                  b.startStep == 8 && b.lengthSteps == 2 && b.pitch == 67 && near(b.velocity, 0.7f);
    }
    check(notesOk, "note fields round-trip");

    // Synth engine settings.
    check(seq2.synth().mode() == audio::SynthMode::FM, "synth mode round-trips");
    check(seq2.synth().waveform() == audio::Waveform::Triangle, "synth waveform round-trips");
    check(near(seq2.synth().sustain(), 0.4f) && near(seq2.synth().release(), 0.5f),
          "synth envelope round-trips");
    check(near(seq2.synth().fmRatio(), 3.5f) && near(seq2.synth().fmIndex(), 6.0f),
          "FM params round-trip");
    check(near(seq2.synth().glide(), 0.15f), "glide time round-trips");
    check(seq2.synth().unisonVoices() == 5 && near(seq2.synth().unisonDetune(), 18.0f),
          "unison round-trips");
    check(seq2.synth().subWaveform() == audio::Waveform::Square, "sub waveform round-trips");
    check(near(seq2.synth().vibratoRate(), 6.0f) && near(seq2.synth().vibratoDepth(), 25.0f),
          "vibrato round-trips");
    check(near(seq2.synth().noiseColor(), 0.7f), "noise color round-trips");
    check(seq2.synth().hardSync() && near(seq2.synth().syncRatio(), 3.25f),
          "hard sync round-trips");
    check(near(seq2.synth().pitchEnvAmount(), -7.0f) && near(seq2.synth().pitchEnvTime(), 0.08f),
          "pitch envelope round-trips");
    check(seq2.arpOn() && seq2.arpMode() == 2 && seq2.arpOctaves() == 3, "arp settings round-trip");
    check(seq2.useSampler() && seq2.synth().gain() >= 0.0f && seq2.sampler().basePitch() == 48 &&
              near(seq2.sampler().startOffset(), 0.25f) && near(seq2.sampler().attack(), 0.02f) &&
              near(seq2.sampler().release(), 0.3f),
          "sampler settings round-trip");

    // Mixer + effects.
    check(near(mixer2.masterGain(), 0.75f), "master gain round-trips");
    check(near(mixer2.limiterCeiling(), 0.9f), "limiter ceiling round-trips");
    check(mixer2.eq().enabled() && near(mixer2.eq().cutoff(), 3200.0f), "EQ round-trips");
    check(near(mixer2.compressor().kneeDb(), 6.0f), "compressor knee round-trips");
    check(mixer2.compressor().enabled() && near(mixer2.compressor().thresholdDb(), -20.0f) &&
              near(mixer2.compressor().ratio(), 6.0f) && near(mixer2.compressor().makeupDb(), 4.0f),
          "compressor round-trips");
    check(mixer2.delay().enabled() && near(mixer2.delay().time(), 250.0f) &&
              near(mixer2.delay().mix(), 0.4f) && mixer2.delay().pingPong() &&
              near(mixer2.delay().damping(), 0.4f),
          "delay round-trips (incl. ping-pong + damping)");
    check(near(mixer2.reverbSend(), 0.45f) && near(mixer2.reverbReturn().roomSize(), 0.6f) &&
              near(mixer2.delaySend(), 0.3f) && near(mixer2.delayReturn().time(), 180.0f),
          "aux send/return buses round-trip");
    check(near(mixer2.track(audio::MixerBus::Drums).gain(), 0.8f) &&
              mixer2.track(audio::MixerBus::Drums).distortion().enabled() &&
              near(mixer2.track(audio::MixerBus::Drums).distortion().drive(), 6.0f) &&
              mixer2.track(audio::MixerBus::Lead).muted() &&
              mixer2.track(audio::MixerBus::Bass).eq().enabled() &&
              near(mixer2.track(audio::MixerBus::Bass).eq().lowGain(), 4.5f),
          "per-bus mixer-track insert strips round-trip");
    check(mixer2.highpass().enabled() && near(mixer2.highpass().cutoff(), 45.0f),
          "high-pass round-trips");
    check(mixer2.tilt().enabled() && near(mixer2.tilt().tilt(), -6.0f), "tilt EQ round-trips");
    check(mixer2.exciter().enabled() && near(mixer2.exciter().crossover(), 6500.0f) &&
              near(mixer2.exciter().amount(), 0.42f),
          "exciter round-trips");
    check(mixer2.transient().enabled() && near(mixer2.transient().attack(), 0.6f) &&
              near(mixer2.transient().sustain(), -0.3f),
          "transient shaper round-trips");
    check(mixer2.autopan().enabled() && near(mixer2.autopan().rate(), 2.5f) &&
              near(mixer2.autopan().depth(), 0.8f),
          "auto-pan round-trips");
    check(mixer2.monobass().enabled() && near(mixer2.monobass().crossover(), 90.0f),
          "mono-bass round-trips");
    check(mixer2.distortion().curve() == audio::Distortion::Curve::Fold,
          "distortion curve round-trips");
    check(mixer2.ringmod().enabled() && near(mixer2.ringmod().freq(), 440.0f) &&
              near(mixer2.ringmod().mix(), 0.7f),
          "ring-mod round-trips");
    check(mixer2.flanger().enabled() && near(mixer2.flanger().rate(), 0.4f) &&
              near(mixer2.flanger().feedback(), 0.6f),
          "flanger round-trips");
    check(mixer2.gate().enabled() && near(mixer2.gate().thresholdDb(), -38.0f) &&
              near(mixer2.gate().ratio(), 5.0f) && near(mixer2.gate().rangeDb(), -55.0f) &&
              near(mixer2.gate().holdMs(), 50.0f),
          "gate round-trips (incl. hold)");
    check(mixer2.widener().enabled() && near(mixer2.widener().width(), 1.6f),
          "stereo widener round-trips");
    check(mixer2.tape().enabled() && near(mixer2.tape().drive(), 4.5f) &&
              near(mixer2.tape().warmth(), 0.6f) && near(mixer2.tape().mix(), 0.9f),
          "tape saturation round-trips");
    check(near(mixer2.reverb().preDelayMs(), 35.0f), "reverb pre-delay round-trips");
    check(near(mixer2.reverb().width(), 1.5f), "reverb width round-trips");
    check(mixer2.reverb().enabled() && near(mixer2.reverb().roomSize(), 0.85f) &&
              near(mixer2.reverb().mix(), 0.33f),
          "reverb round-trips");

    // Automation lane.
    const audio::AutoLane& lane2 = automation2.lane(audio::AutoTarget::FilterCutoff);
    check(lane2.enabled && lane2.lfo.shape == audio::Waveform::Saw &&
              near(lane2.lfo.rateHz, 1.75f) && near(lane2.lo, 300.0f) && near(lane2.hi, 5500.0f),
          "automation lane round-trips");
    const audio::AutoLane& clipLane2 = automation2.lane(audio::AutoTarget::ReverbMix);
    check(clipLane2.clip.size() == 3 && near(static_cast<float>(clipLane2.clipLength), 4.0f) &&
              near(static_cast<float>(clipLane2.clip[1].time), 1.5f) &&
              near(clipLane2.clip[1].value, 0.9f),
          "automation clip (breakpoints) round-trips");

    // A non-.cjc file is rejected.
    audio::Sequencer seq3;
    audio::Mixer mixer3;
    audio::Automation automation3;
    check(!audio::loadProject("/nonexistent/definitely_missing.cjc", seq3, mixer3, automation3, &err),
          "loading a missing file fails cleanly");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
