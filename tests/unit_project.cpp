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
    seq.setMetronome(true);
    seq.setMetronomeLevel(0.7f);
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
    seq.setChannelDrive(0, 0.65f);
    seq.setChannelPitchEnv(0, 1.7f); // kick punch
    seq.setChannelTone(0, 2500.0f);  // darken the kick
    seq.setChannelPitchEnvTime(0, 2.5f); // longer/boomier kick sweep
    seq.setChannelFlam(1, 18.0f);
    seq.setChannelType(1, audio::Drum::Tom);
    seq.setChannelType(4, audio::Drum::Ride);
    seq.setChannelType(2, audio::Drum::Clave);
    seq.setChannelType(3, audio::Drum::Shaker);
    seq.setChannelType(0, audio::Drum::Woodblock);
    // (channel 0's per-channel tone/punch below still apply to whatever voice it plays)
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
    seq.synth().setGlideLegato(true);
    seq.synth().setUnison(5, 18.0f);
    seq.synth().setSubWaveform(audio::Waveform::Trapezoid);
    seq.synth().setSubOctave(2);
    seq.synth().setVibrato(6.0f, 25.0f);
    seq.synth().setVibratoDelay(0.35f);
    seq.synth().setNoiseColor(0.7f);
    seq.synth().setHardSync(true);
    seq.synth().setSyncRatio(3.25f);
    seq.synth().setPulseWidth(0.3f);
    seq.synth().setPwmLfo(2.0f, 0.3f);
    seq.synth().setOsc2Semitones(7.0f);
    seq.synth().setOsc2Waveform(audio::Waveform::Square); // unlinks osc2 from the primary
    seq.synth().setOsc3Level(0.45f);
    seq.synth().setOsc3Semitones(-5.0f);
    seq.synth().setVelToCutoff(4200.0f);
    seq.synth().setFmFeedback(0.55f);
    seq.synth().setVelToFmIndex(3.0f);
    seq.synth().setRingMod(0.6f);
    seq.synth().setWavetableLfo(2.5f, 0.7f);
    seq.synth().setVelSensitivity(0.4f);
    seq.synth().setFilterKeyTrack(0.75f);
    seq.synth().setOctave(-1);
    seq.synth().setMono(true);
    seq.synth().setFilterLfo(3.5f, 1.5f);
    seq.synth().setFilterLfoSync(true);
    seq.synth().setFilterLfoSyncDivision(4); // 1/8T
    seq.synth().setFilterLfoShape(audio::Waveform::Square);
    seq.synth().setAmpLfoSync(true);
    seq.synth().setAmpLfoSyncDivision(5); // 1/16
    seq.synth().setVibratoSync(true);
    seq.synth().setVibratoSyncDivision(2); // 1/4
    seq.synth().setAmpLfo(4.5f, 0.6f);
    seq.synth().setDrift(18.0f);
    seq.synth().setFilterMode(audio::StateVariableFilter::Mode::Notch);
    seq.synth().setFilterEnvelope(0.02f, 0.15f, 0.3f, 0.25f);
    seq.synth().setFilterEnvDepth(4200.0f);
    seq.synth().setFilterDrive(0.65f);
    seq.synth().setStartPhaseRandom(0.55f);
    seq.synth().setPitchEnv(-7.0f, 0.08f);
    seq.setArp(true, 5); // chord mode
    seq.setArpOctaves(3);
    seq.setArpGate(0.4f);
    seq.setArpRate(3);
    seq.setSidechain(true, 0.8f, 180.0f, 25.0f);
    seq.setSidechainSource(2);
    seq.sampler().setBasePitch(48);
    seq.sampler().setStartOffset(0.25f);
    seq.sampler().setAmpEnv(0.02f, 0.3f);
    seq.sampler().setLoop(true);
    seq.sampler().setPingPong(true);
    seq.sampler().setDetuneCents(-25.0f);
    seq.sampler().setLoopRegion(0.3f, 0.75f);
    seq.sampler().setSlices(8);
    seq.sampler().setFilter(3200.0f, 4.5f);
    seq.sampler().setFilterEnvelope(0.02f, 0.12f, 0.4f, 0.2f);
    seq.sampler().setFilterEnvDepth(-3500.0f);
    seq.sampler().setPitchEnv(9.0f, 0.12f); // pitch envelope
    seq.sampler().setFilterVelo(5000.0f);   // velocity → cutoff
    seq.sampler().setMono(true);            // monophonic mode
    seq.sampler().setAmpDecay(0.08f);       // amp ADSR decay
    seq.sampler().setAmpSustain(0.45f);     // amp ADSR sustain
    seq.setUseSampler(true);
    // Second instrument: a bass note on roll2 + a distinct synth2 patch.
    seq.roll2().addNote(audio::Note{2, 6, 40, 0.85f});
    seq.synth2().setWaveform(audio::Waveform::Square);
    seq.synth2().setFilter(700.0f, 4.0f, 900.0f);
    seq.synth2().setMode(audio::SynthMode::Wavetable);
    seq.synth2().setWavetablePosition(0.65f);
    seq.synth2().setWavetableMorph(0.4f);
    seq.synth2().setVelToWavePosition(0.5f);
    seq.synth2().setWavetableFrames(audio::Waveform::Saw, audio::Waveform::Square,
                                    audio::Waveform::Triangle, audio::Waveform::Sine);
    // A second pattern + a playlist arrangement.
    const int p1 = seq.addPattern();
    seq.selectPattern(p1);
    seq.setStep(2, 5, true);
    seq.setStepTune(2, 5, -5); // per-step pitch offset
    seq.setStepNudge(2, 5, 40); // per-step timing nudge
    seq.roll().addNote(audio::Note{4, 1, 72, 0.5f, 0.6f, 35.0f, 3}); // roll = 3
    seq.setPatternName(p1, "Chorus Fill");
    seq.setSwing(0.35f); // pattern 1's own groove
    seq.selectPattern(0);
    seq.setSwing(0.15f); // pattern 0's own groove
    seq.setPlaylist({0, 1, 0});
    seq.setSongMode(true);
    seq.setSongLoop(false);
    seq.setSongLoopRange(1, 3);

    mixer.setMasterGain(0.75f);
    mixer.setLimiterCeiling(0.9f);
    mixer.setMasterBalance(-0.4f);
    mixer.eq().setEnabled(true);
    mixer.eq().setCutoff(3200.0f);
    mixer.compressor().setEnabled(true);
    mixer.compressor().setThresholdDb(-20.0f);
    mixer.compressor().setRatio(6.0f);
    mixer.compressor().setMakeupDb(4.0f);
    mixer.compressor().setKneeDb(6.0f);
    mixer.compressor().setMix(0.6f);
    mixer.compressor().setSidechainHpf(90.0f);
    mixer.multiband().setEnabled(true);
    mixer.multiband().setCrossoverLow(180.0f);
    mixer.multiband().setCrossoverHigh(3200.0f);
    mixer.multiband().setBandThreshold(0, -24.0f);
    mixer.multiband().setBandRatio(0, 4.0f);
    mixer.multiband().setBandThreshold(2, -12.0f);
    mixer.multiband().setBandRatio(2, 2.5f);
    mixer.multiband().setAttackMs(15.0f);
    mixer.multiband().setReleaseMs(180.0f);
    mixer.delay().setEnabled(true);
    mixer.delay().setTime(250.0f);
    mixer.delay().setMix(0.4f);
    mixer.delay().setPingPong(true);
    mixer.delay().setDamping(0.4f);
    mixer.delay().setSync(true);
    mixer.delay().setSyncDivision(6);
    mixer.delay().setFeedbackLowCut(220.0f);
    mixer.delay().setModDepth(4.5f);
    mixer.delay().setModRate(1.2f);
    mixer.reverb().setEnabled(true);
    mixer.reverb().setRoomSize(0.85f);
    mixer.reverb().setMix(0.33f);
    mixer.reverb().setPreDelayMs(35.0f);
    mixer.reverb().setWidth(1.5f);
    mixer.reverb().setFreeze(true);
    mixer.reverb().setDuck(0.7f);
    mixer.reverb().setWetLowCut(120.0f);
    mixer.reverb().setWetHighCut(8000.0f);
    mixer.reverb().setGateMs(180.0f);
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
    mixer.autopan().setSync(true);
    mixer.autopan().setSyncDivision(3);
    mixer.autopan().setShape(audio::AutoPan::Shape::Square);
    mixer.monobass().setEnabled(true);
    mixer.monobass().setCrossover(90.0f);
    mixer.subbass().setEnabled(true);
    mixer.subbass().setAmount(0.6f);
    mixer.subbass().setCutoff(100.0f);
    mixer.subbass().setTone(180.0f);
    mixer.autowah().setEnabled(true);
    mixer.autowah().setBaseHz(250.0f);
    mixer.autowah().setRangeHz(2500.0f);
    mixer.autowah().setSensitivity(0.65f);
    mixer.autowah().setResonance(6.0f);
    mixer.autowah().setAttackMs(3.0f);
    mixer.autowah().setReleaseMs(120.0f);
    mixer.autowah().setDownward(true);
    mixer.comb().setEnabled(true);
    mixer.comb().setFrequency(330.0f);
    mixer.comb().setFeedback(0.72f);
    mixer.comb().setMix(0.45f);
    mixer.tremolo().setEnabled(true);
    mixer.tremolo().setRate(7.5f);
    mixer.tremolo().setDepth(0.85f);
    mixer.tremolo().setShape(audio::Tremolo::Shape::Square);
    mixer.tremolo().setSync(true);
    mixer.tremolo().setSyncDivision(5);
    mixer.stereoDelay().setEnabled(true);
    mixer.stereoDelay().setLeftMs(180.0f);
    mixer.stereoDelay().setRightMs(270.0f);
    mixer.stereoDelay().setFeedback(0.55f);
    mixer.stereoDelay().setMix(0.35f);
    mixer.stereoDelay().setSync(true);
    mixer.stereoDelay().setLeftDivision(3);
    mixer.stereoDelay().setRightDivision(6);
    mixer.stereoDelay().setDamping(0.5f);
    mixer.stereoDelay().setFeedbackLowCut(180.0f);
    mixer.stereoDelay().setPingPong(true);
    mixer.formant().setEnabled(true);
    mixer.formant().setVowel(audio::FormantFilter::Vowel::E);
    mixer.formant().setMix(0.6f);
    mixer.utility().setEnabled(true);
    mixer.utility().setGainDb(-3.0f);
    mixer.utility().setInvertR(true);
    mixer.utility().setMono(true);
    mixer.limiter().setEnabled(true);
    mixer.limiter().setInputGainDb(6.0f);
    mixer.limiter().setCeilingDb(-1.5f);
    mixer.limiter().setReleaseMs(200.0f);
    mixer.limiter().setLookaheadMs(3.0f);
    mixer.clipper().setEnabled(true);
    mixer.clipper().setDriveDb(4.5f);
    mixer.clipper().setCeiling(0.75f);
    mixer.clipper().setHardness(0.4f);
    mixer.deEsser().setEnabled(true);
    mixer.deEsser().setThresholdDb(-28.0f);
    mixer.deEsser().setFrequency(7000.0f);
    mixer.deEsser().setAmount(0.65f);
    mixer.deEsser().setReleaseMs(45.0f);
    mixer.stereoEnhancer().setEnabled(true);
    mixer.stereoEnhancer().setDelayMs(18.0f);
    mixer.stereoEnhancer().setAmount(0.55f);
    mixer.peq().setEnabled(true);
    mixer.peq().setMid(1200.0f, 1.5f, 5.0f);
    mixer.peq().setMid2(4200.0f, 2.5f, -6.0f);
    mixer.bitcrusher().setEnabled(true);
    mixer.bitcrusher().setBits(6.0f);
    mixer.bitcrusher().setTone(3200.0f);
    mixer.distortion().setEnabled(true);
    mixer.distortion().setCurve(audio::Distortion::Curve::Tube);
    mixer.distortion().setTone(4800.0f);
    mixer.distortion().setOutputDb(-4.5f);
    mixer.ringmod().setEnabled(true);
    mixer.ringmod().setFreq(440.0f);
    mixer.ringmod().setMix(0.7f);
    mixer.flanger().setEnabled(true);
    mixer.flanger().setRate(0.4f);
    mixer.flanger().setFeedback(0.6f);
    mixer.flanger().setSync(true);
    mixer.flanger().setSyncDivision(3);
    mixer.chorus().setEnabled(true);
    mixer.chorus().setSync(true);
    mixer.chorus().setSyncDivision(2);
    mixer.chorus().setFeedback(0.45f);
    mixer.phaser().setEnabled(true);
    mixer.phaser().setSync(true);
    mixer.phaser().setSyncDivision(4);
    mixer.phaser().setStages(8);
    mixer.gate().setEnabled(true);
    mixer.gate().setThresholdDb(-38.0f);
    mixer.gate().setRatio(5.0f);
    mixer.gate().setRangeDb(-55.0f);
    mixer.gate().setHoldMs(50.0f);
    mixer.widener().setEnabled(true);
    mixer.widener().setWidth(1.6f);
    mixer.widener().setBassMonoHz(120.0f);
    mixer.tape().setEnabled(true);
    mixer.tape().setDrive(4.5f);
    mixer.tape().setWarmth(0.6f);
    mixer.tape().setMix(0.9f);
    mixer.tape().setWowFlutter(0.4f);
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
    mixer.track(audio::MixerBus::Lead).highpass().setEnabled(true);
    mixer.track(audio::MixerBus::Lead).highpass().setCutoff(120.0f);
    mixer.track(audio::MixerBus::Bass).setPan(-0.4f);
    mixer.track(audio::MixerBus::Drums).transientShaper().setEnabled(true);
    mixer.track(audio::MixerBus::Drums).transientShaper().setAttack(0.7f);
    mixer.track(audio::MixerBus::Drums).transientShaper().setSustain(-0.3f);
    mixer.track(audio::MixerBus::Lead).gate().setEnabled(true);
    mixer.track(audio::MixerBus::Lead).gate().setThresholdDb(-30.0f);
    mixer.track(audio::MixerBus::Lead).gate().setRatio(6.0f);
    mixer.track(audio::MixerBus::Lead).gate().setAttackMs(3.0f);
    mixer.track(audio::MixerBus::Lead).gate().setReleaseMs(120.0f);
    mixer.track(audio::MixerBus::Bass).setSoloed(true);
    mixer.track(audio::MixerBus::Drums).setReverbSend(0.55f);
    mixer.track(audio::MixerBus::Drums).setDelaySend(0.35f);

    audio::AutoLane& lane = automation.lane(audio::AutoTarget::FilterCutoff);
    lane.enabled = true;
    lane.lfo.shape = audio::Waveform::Saw;
    lane.lfo.rateHz = 1.75f;
    lane.lo = 300.0f;
    lane.hi = 5500.0f;
    lane.sync = true;
    lane.syncDiv = 3;
    // A breakpoint automation clip on a second lane.
    audio::AutoLane& reverbLane = automation.lane(audio::AutoTarget::ReverbMix);
    reverbLane.enabled = true;
    reverbLane.clip = {{0.0, 0.1f}, {1.5, 0.9f}, {3.0, 0.3f}};
    reverbLane.clipLength = 4.0;
    // A lead-bus volume lane (a later-appended target) — checks new lanes persist by index.
    audio::AutoLane& leadVolLane = automation.lane(audio::AutoTarget::LeadVolume);
    leadVolLane.enabled = true;
    leadVolLane.lfo.shape = audio::Waveform::Triangle;
    leadVolLane.lfo.rateHz = 0.8f;
    leadVolLane.lo = 0.2f;
    leadVolLane.hi = 0.95f;
    // An aux-send lane (reverb send) — another later-appended target.
    audio::AutoLane& sendLane = automation.lane(audio::AutoTarget::ReverbSend);
    sendLane.enabled = true;
    sendLane.lfo.shape = audio::Waveform::Square;
    sendLane.lfo.rateHz = 0.5f;
    sendLane.lo = 0.1f;
    sendLane.hi = 0.75f;
    // A drum-bus lane (drum volume) — one of the newest appended targets.
    audio::AutoLane& drumVolLane = automation.lane(audio::AutoTarget::DrumVolume);
    drumVolLane.enabled = true;
    drumVolLane.lfo.shape = audio::Waveform::Saw;
    drumVolLane.lfo.rateHz = 0.6f;
    drumVolLane.lo = 0.15f;
    drumVolLane.hi = 0.9f;
    // A bass-filter lane (bass cutoff) — the newest appended target.
    audio::AutoLane& bassCutLane = automation.lane(audio::AutoTarget::BassCutoff);
    bassCutLane.enabled = true;
    bassCutLane.lfo.shape = audio::Waveform::Triangle;
    bassCutLane.lfo.rateHz = 0.45f;
    bassCutLane.lo = 200.0f;
    bassCutLane.hi = 4800.0f;
    // A time-fx lane (delay feedback) — one of the newest appended targets.
    audio::AutoLane& dfbLane = automation.lane(audio::AutoTarget::DelayFeedback);
    dfbLane.enabled = true;
    dfbLane.lfo.shape = audio::Waveform::Sine;
    dfbLane.lfo.rateHz = 0.25f;
    dfbLane.lo = 0.2f;
    dfbLane.hi = 0.8f;

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
    check(seq2.metronome() && near(seq2.metronomeLevel(), 0.7f), "metronome + level round-trip");

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
    check(near(seq2.channelDrive(0), 0.65f), "channel drive round-trips");
    check(near(seq2.channelPitchEnv(0), 1.7f), "channel pitch-env (punch) round-trips");
    check(near(seq2.channelTone(0), 2500.0f), "channel tone (low-pass) round-trips");
    check(near(seq2.channelPitchEnvTime(0), 2.5f), "channel pitch-env time round-trips");
    check(near(seq2.channelFlam(1), 18.0f), "channel flam round-trips");
    check(seq2.channelType(1) == audio::Drum::Tom &&
              seq2.channelType(4) == audio::Drum::Ride &&
              seq2.channelType(2) == audio::Drum::Clave &&
              seq2.channelType(3) == audio::Drum::Shaker &&
              seq2.channelType(0) == audio::Drum::Woodblock,
          "per-channel drum type round-trips");
    check(!seq2.step(0, 1) && !seq2.step(3, 0), "inactive steps stay off");

    // Arrangement: patterns, per-pattern content, playlist, song mode.
    check(seq2.patternCount() == 2, "pattern count round-trips");
    check(seq2.patternName(1) == "Chorus Fill", "pattern name round-trips");
    check(seq2.patternName(0) == "Pattern 1", "default pattern name is preserved");
    check(seq2.songMode() && !seq2.songLoop(), "song mode + play-once flag round-trip");
    check(seq2.songLoopStart() == 1 && seq2.songLoopEnd() == 3, "song loop region round-trips");
    check(seq2.playlist().size() == 3 && seq2.playlist()[0] == 0 && seq2.playlist()[1] == 1 &&
              seq2.playlist()[2] == 0,
          "playlist round-trips");
    seq2.selectPattern(1);
    check(seq2.step(2, 5) && seq2.roll().notes().size() == 1, "second pattern content round-trips");
    check(seq2.stepTune(2, 5) == -5, "per-step pitch round-trips");
    check(seq2.stepNudge(2, 5) == 40, "per-step timing nudge round-trips");
    check(near(seq2.roll().notes()[0].probability, 0.6f), "per-note probability round-trips");
    check(near(seq2.roll().notes()[0].fineTune, 35.0f), "per-note fine tune round-trips");
    check(seq2.roll().notes()[0].roll == 3, "per-note roll/ratchet round-trips");
    check(near(seq2.swing(), 0.35f), "pattern 1 per-pattern swing round-trips");
    seq2.selectPattern(0);
    check(near(seq2.swing(), 0.15f), "pattern 0 per-pattern swing round-trips");

    // Second instrument round-trips (roll2 note + synth2 patch).
    check(seq2.roll2().notes().size() == 1 && seq2.roll2().notes()[0].pitch == 40,
          "second-instrument (bass) notes round-trip");
    check(seq2.synth2().waveform() == audio::Waveform::Square, "synth2 patch round-trips");
    check(seq2.synth2().mode() == audio::SynthMode::Wavetable &&
              near(seq2.synth2().wavetablePosition(), 0.65f) &&
              near(seq2.synth2().wavetableMorph(), 0.4f) &&
              near(seq2.synth2().velToWavePosition(), 0.5f),
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
    check(seq2.synth().glideLegato(), "glide legato mode round-trips");
    check(near(seq2.synth().filterLfoRate(), 3.5f), "filter LFO rate round-trips");
    check(near(seq2.synth().filterLfoDepth(), 1.5f), "filter LFO depth round-trips");
    check(seq2.synth().filterLfoSync() && seq2.synth().filterLfoSyncDivision() == 4,
          "filter LFO tempo sync round-trips");
    check(seq2.synth().filterLfoShape() == audio::Waveform::Square,
          "filter LFO shape round-trips");
    check(seq2.synth().ampLfoSync() && seq2.synth().ampLfoSyncDivision() == 5,
          "tremolo LFO tempo sync round-trips");
    check(seq2.synth().vibratoSync() && seq2.synth().vibratoSyncDivision() == 2,
          "vibrato tempo sync round-trips");
    check(near(seq2.synth().ampLfoRate(), 4.5f) && near(seq2.synth().ampLfoDepth(), 0.6f),
          "amp LFO (tremolo) round-trips");
    check(near(seq2.synth().drift(), 18.0f), "analog drift round-trips");
    check(seq2.synth().filterMode() == audio::StateVariableFilter::Mode::Notch,
          "synth filter type round-trips");
    check(near(seq2.synth().filterEnvDepth(), 4200.0f) &&
              near(seq2.synth().filterEnvSustain(), 0.3f) &&
              near(seq2.synth().filterEnvDecay(), 0.15f),
          "synth filter envelope round-trips");
    check(near(seq2.synth().filterDrive(), 0.65f), "synth filter drive round-trips");
    check(near(seq2.synth().startPhaseRandom(), 0.55f), "synth start-phase randomization round-trips");
    check(seq2.synth().unisonVoices() == 5 && near(seq2.synth().unisonDetune(), 18.0f),
          "unison round-trips");
    check(seq2.synth().subWaveform() == audio::Waveform::Trapezoid,
          "sub waveform round-trips (incl. the trapezoid index)");
    check(seq2.synth().subOctave() == 2, "sub octave round-trips");
    check(near(seq2.synth().vibratoDelay(), 0.35f), "vibrato delay round-trips");
    check(near(seq2.synth().vibratoRate(), 6.0f) && near(seq2.synth().vibratoDepth(), 25.0f),
          "vibrato round-trips");
    check(near(seq2.synth().noiseColor(), 0.7f), "noise color round-trips");
    check(seq2.synth().hardSync() && near(seq2.synth().syncRatio(), 3.25f),
          "hard sync round-trips");
    check(near(seq2.synth().pulseWidth(), 0.3f), "pulse width round-trips");
    check(near(seq2.synth().pwmLfoRate(), 2.0f) && near(seq2.synth().pwmLfoDepth(), 0.3f),
          "PWM LFO round-trips");
    check(near(seq2.synth().osc2Semitones(), 7.0f), "osc2 coarse tune round-trips");
    check(!seq2.synth().osc2WaveformLinked() &&
              seq2.synth().osc2Waveform() == audio::Waveform::Square,
          "osc2 waveform (unlinked) round-trips");
    check(near(seq2.synth().osc3Level(), 0.45f) && near(seq2.synth().osc3Semitones(), -5.0f),
          "osc3 round-trips");
    check(near(seq2.synth().velToCutoff(), 4200.0f), "velocity→cutoff round-trips");
    check(near(seq2.synth().fmFeedback(), 0.55f), "FM feedback round-trips");
    check(near(seq2.synth().velToFmIndex(), 3.0f), "velocity → FM index round-trips");
    check(near(seq2.synth().ringMod(), 0.6f), "ring mod round-trips");
    check(near(seq2.synth().wavetableLfoRate(), 2.5f) &&
              near(seq2.synth().wavetableLfoDepth(), 0.7f),
          "wavetable scan LFO round-trips");
    check(near(seq2.synth().velSensitivity(), 0.4f), "velocity→amp sensitivity round-trips");
    check(near(seq2.synth().filterKeyTrack(), 0.75f), "filter key tracking round-trips");
    check(seq2.synth().octave() == -1, "per-instrument octave round-trips");
    check(seq2.synth().mono(), "monophonic mode round-trips");
    check(near(seq2.synth().pitchEnvAmount(), -7.0f) && near(seq2.synth().pitchEnvTime(), 0.08f),
          "pitch envelope round-trips");
    check(seq2.arpOn() && seq2.arpMode() == 5 && seq2.arpOctaves() == 3 &&
              near(seq2.arpGate(), 0.4f) && seq2.arpRate() == 3,
          "arp settings round-trip");
    check(seq2.sidechainOn() && near(seq2.sidechainAmount(), 0.8f) &&
              seq2.sidechainSource() == 2 && near(seq2.sidechainAttackMs(), 25.0f),
          "sidechain (incl. routable source + attack) round-trips");
    check(seq2.useSampler() && seq2.synth().gain() >= 0.0f && seq2.sampler().basePitch() == 48 &&
              near(seq2.sampler().startOffset(), 0.25f) && near(seq2.sampler().attack(), 0.02f) &&
              near(seq2.sampler().release(), 0.3f) && seq2.sampler().loop() &&
              seq2.sampler().pingPong() && near(seq2.sampler().detuneCents(), -25.0f) &&
              near(seq2.sampler().loopStart(), 0.3f) && near(seq2.sampler().loopEnd(), 0.75f) &&
              seq2.sampler().slices() == 8 && near(seq2.sampler().filterCutoff(), 3200.0f) &&
              near(seq2.sampler().filterResonance(), 4.5f) &&
              near(seq2.sampler().filterEnvDepth(), -3500.0f) &&
              near(seq2.sampler().filterEnvSustain(), 0.4f) &&
              near(seq2.sampler().filterEnvDecay(), 0.12f) &&
              near(seq2.sampler().pitchEnvDepth(), 9.0f) &&
              near(seq2.sampler().pitchEnvTime(), 0.12f) &&
              near(seq2.sampler().filterVelo(), 5000.0f) && seq2.sampler().mono() &&
              near(seq2.sampler().ampDecay(), 0.08f) && near(seq2.sampler().ampSustain(), 0.45f),
          "sampler settings round-trip");

    // Mixer + effects.
    check(near(mixer2.masterGain(), 0.75f), "master gain round-trips");
    check(near(mixer2.limiterCeiling(), 0.9f), "limiter ceiling round-trips");
    check(near(mixer2.masterBalance(), -0.4f), "master balance round-trips");
    check(mixer2.eq().enabled() && near(mixer2.eq().cutoff(), 3200.0f), "EQ round-trips");
    check(near(mixer2.compressor().kneeDb(), 6.0f), "compressor knee round-trips");
    check(near(mixer2.compressor().mix(), 0.6f), "compressor mix round-trips");
    check(near(mixer2.compressor().sidechainHpf(), 90.0f), "compressor sidechain HPF round-trips");
    check(mixer2.compressor().enabled() && near(mixer2.compressor().thresholdDb(), -20.0f) &&
              near(mixer2.compressor().ratio(), 6.0f) && near(mixer2.compressor().makeupDb(), 4.0f),
          "compressor round-trips");
    check(mixer2.multiband().enabled() && near(mixer2.multiband().crossoverLow(), 180.0f) &&
              near(mixer2.multiband().crossoverHigh(), 3200.0f) &&
              near(mixer2.multiband().bandThreshold(0), -24.0f) &&
              near(mixer2.multiband().bandRatio(0), 4.0f) &&
              near(mixer2.multiband().bandRatio(2), 2.5f) &&
              near(mixer2.multiband().attackMs(), 15.0f) && near(mixer2.multiband().releaseMs(), 180.0f),
          "multiband compressor round-trips");
    check(mixer2.delay().enabled() && near(mixer2.delay().time(), 250.0f) &&
              near(mixer2.delay().mix(), 0.4f) && mixer2.delay().pingPong() &&
              near(mixer2.delay().damping(), 0.4f) && mixer2.delay().sync() &&
              mixer2.delay().syncDivision() == 6 && near(mixer2.delay().feedbackLowCut(), 220.0f) &&
              near(mixer2.delay().modDepth(), 4.5f) && near(mixer2.delay().modRate(), 1.2f),
          "delay round-trips (incl. ping-pong + damping + feedback low-cut)");
    check(near(mixer2.reverbSend(), 0.45f) && near(mixer2.reverbReturn().roomSize(), 0.6f) &&
              near(mixer2.delaySend(), 0.3f) && near(mixer2.delayReturn().time(), 180.0f),
          "aux send/return buses round-trip");
    check(near(mixer2.track(audio::MixerBus::Drums).gain(), 0.8f) &&
              mixer2.track(audio::MixerBus::Drums).distortion().enabled() &&
              near(mixer2.track(audio::MixerBus::Drums).distortion().drive(), 6.0f) &&
              mixer2.track(audio::MixerBus::Lead).muted() &&
              mixer2.track(audio::MixerBus::Bass).eq().enabled() &&
              near(mixer2.track(audio::MixerBus::Bass).eq().lowGain(), 4.5f) &&
              mixer2.track(audio::MixerBus::Lead).highpass().enabled() &&
              near(mixer2.track(audio::MixerBus::Lead).highpass().cutoff(), 120.0f) &&
              near(mixer2.track(audio::MixerBus::Bass).pan(), -0.4f) &&
              mixer2.track(audio::MixerBus::Drums).transientShaper().enabled() &&
              near(mixer2.track(audio::MixerBus::Drums).transientShaper().attack(), 0.7f) &&
              near(mixer2.track(audio::MixerBus::Drums).transientShaper().sustain(), -0.3f) &&
              mixer2.track(audio::MixerBus::Lead).gate().enabled() &&
              near(mixer2.track(audio::MixerBus::Lead).gate().thresholdDb(), -30.0f) &&
              near(mixer2.track(audio::MixerBus::Lead).gate().ratio(), 6.0f) &&
              near(mixer2.track(audio::MixerBus::Lead).gate().releaseMs(), 120.0f) &&
              mixer2.track(audio::MixerBus::Bass).soloed() &&
              near(mixer2.track(audio::MixerBus::Drums).reverbSend(), 0.55f) &&
              near(mixer2.track(audio::MixerBus::Drums).delaySend(), 0.35f),
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
              near(mixer2.autopan().depth(), 0.8f) && mixer2.autopan().sync() &&
              mixer2.autopan().syncDivision() == 3 &&
              mixer2.autopan().shape() == audio::AutoPan::Shape::Square,
          "auto-pan round-trips");
    check(mixer2.monobass().enabled() && near(mixer2.monobass().crossover(), 90.0f),
          "mono-bass round-trips");
    check(mixer2.subbass().enabled() && near(mixer2.subbass().amount(), 0.6f) &&
              near(mixer2.subbass().cutoff(), 100.0f) && near(mixer2.subbass().tone(), 180.0f),
          "sub-bass round-trips");
    check(mixer2.autowah().enabled() && near(mixer2.autowah().baseHz(), 250.0f) &&
              near(mixer2.autowah().rangeHz(), 2500.0f) &&
              near(mixer2.autowah().sensitivity(), 0.65f) &&
              near(mixer2.autowah().resonance(), 6.0f) && near(mixer2.autowah().attackMs(), 3.0f) &&
              near(mixer2.autowah().releaseMs(), 120.0f) && mixer2.autowah().downward(),
          "auto-wah round-trips");
    check(mixer2.comb().enabled() && near(mixer2.comb().frequency(), 330.0f) &&
              near(mixer2.comb().feedback(), 0.72f) && near(mixer2.comb().mix(), 0.45f),
          "comb resonator round-trips");
    check(mixer2.tremolo().enabled() && near(mixer2.tremolo().rate(), 7.5f) &&
              near(mixer2.tremolo().depth(), 0.85f) &&
              mixer2.tremolo().shape() == audio::Tremolo::Shape::Square &&
              mixer2.tremolo().sync() && mixer2.tremolo().syncDivision() == 5,
          "tremolo round-trips");
    check(mixer2.formant().enabled() &&
              mixer2.formant().vowel() == audio::FormantFilter::Vowel::E &&
              near(mixer2.formant().mix(), 0.6f),
          "formant filter round-trips");
    check(mixer2.utility().enabled() && near(mixer2.utility().gainDb(), -3.0f) &&
              !mixer2.utility().invertL() && mixer2.utility().invertR() && mixer2.utility().mono(),
          "utility round-trips");
    check(mixer2.limiter().enabled() && near(mixer2.limiter().inputGainDb(), 6.0f) &&
              near(mixer2.limiter().ceilingDb(), -1.5f) && near(mixer2.limiter().releaseMs(), 200.0f) &&
              near(mixer2.limiter().lookaheadMs(), 3.0f),
          "limiter round-trips");
    check(mixer2.clipper().enabled() && near(mixer2.clipper().driveDb(), 4.5f) &&
              near(mixer2.clipper().ceiling(), 0.75f) && near(mixer2.clipper().hardness(), 0.4f),
          "clipper round-trips");
    check(mixer2.deEsser().enabled() && near(mixer2.deEsser().thresholdDb(), -28.0f) &&
              near(mixer2.deEsser().frequency(), 7000.0f) && near(mixer2.deEsser().amount(), 0.65f) &&
              near(mixer2.deEsser().releaseMs(), 45.0f),
          "de-esser round-trips");
    check(mixer2.stereoEnhancer().enabled() && near(mixer2.stereoEnhancer().delayMs(), 18.0f) &&
              near(mixer2.stereoEnhancer().amount(), 0.55f),
          "stereo enhancer round-trips");
    check(mixer2.stereoDelay().enabled() && near(mixer2.stereoDelay().leftMs(), 180.0f) &&
              near(mixer2.stereoDelay().rightMs(), 270.0f) &&
              near(mixer2.stereoDelay().feedback(), 0.55f) &&
              near(mixer2.stereoDelay().mix(), 0.35f) && mixer2.stereoDelay().sync() &&
              mixer2.stereoDelay().leftDivision() == 3 && mixer2.stereoDelay().rightDivision() == 6 &&
              near(mixer2.stereoDelay().damping(), 0.5f) &&
              near(mixer2.stereoDelay().feedbackLowCut(), 180.0f) &&
              mixer2.stereoDelay().pingPong(),
          "stereo delay round-trips");
    check(mixer2.distortion().curve() == audio::Distortion::Curve::Tube &&
              near(mixer2.distortion().tone(), 4800.0f) &&
              near(mixer2.distortion().outputDb(), -4.5f),
          "distortion curve round-trips");
    check(near(mixer2.peq().midGain(), 5.0f) && near(mixer2.peq().mid2Freq(), 4200.0f) &&
              near(mixer2.peq().mid2Q(), 2.5f) && near(mixer2.peq().mid2Gain(), -6.0f),
          "parametric EQ 2nd mid band round-trips");
    check(mixer2.bitcrusher().enabled() && near(mixer2.bitcrusher().bits(), 6.0f) &&
              near(mixer2.bitcrusher().tone(), 3200.0f),
          "bitcrusher (incl. post tone) round-trips");
    check(mixer2.ringmod().enabled() && near(mixer2.ringmod().freq(), 440.0f) &&
              near(mixer2.ringmod().mix(), 0.7f),
          "ring-mod round-trips");
    check(mixer2.flanger().enabled() && near(mixer2.flanger().rate(), 0.4f) &&
              near(mixer2.flanger().feedback(), 0.6f) && mixer2.flanger().sync() &&
              mixer2.flanger().syncDivision() == 3,
          "flanger round-trips");
    check(mixer2.chorus().enabled() && mixer2.chorus().sync() && mixer2.chorus().syncDivision() == 2 &&
              near(mixer2.chorus().feedback(), 0.45f),
          "chorus tempo sync round-trips");
    check(mixer2.phaser().enabled() && mixer2.phaser().sync() && mixer2.phaser().syncDivision() == 4 &&
              mixer2.phaser().stages() == 8,
          "phaser tempo sync round-trips");
    check(mixer2.gate().enabled() && near(mixer2.gate().thresholdDb(), -38.0f) &&
              near(mixer2.gate().ratio(), 5.0f) && near(mixer2.gate().rangeDb(), -55.0f) &&
              near(mixer2.gate().holdMs(), 50.0f),
          "gate round-trips (incl. hold)");
    check(mixer2.widener().enabled() && near(mixer2.widener().width(), 1.6f) &&
              near(mixer2.widener().bassMonoHz(), 120.0f),
          "stereo widener round-trips");
    check(mixer2.tape().enabled() && near(mixer2.tape().drive(), 4.5f) &&
              near(mixer2.tape().warmth(), 0.6f) && near(mixer2.tape().mix(), 0.9f) &&
              near(mixer2.tape().wowFlutter(), 0.4f),
          "tape saturation round-trips");
    check(near(mixer2.reverb().preDelayMs(), 35.0f), "reverb pre-delay round-trips");
    check(near(mixer2.reverb().width(), 1.5f), "reverb width round-trips");
    check(mixer2.reverb().freeze(), "reverb freeze round-trips");
    check(near(mixer2.reverb().duck(), 0.7f), "reverb ducking round-trips");
    check(near(mixer2.reverb().gateMs(), 180.0f), "reverb gate time round-trips");
    check(near(mixer2.reverb().wetLowCut(), 120.0f) && near(mixer2.reverb().wetHighCut(), 8000.0f),
          "reverb wet tone (low/high cut) round-trips");
    check(mixer2.reverb().enabled() && near(mixer2.reverb().roomSize(), 0.85f) &&
              near(mixer2.reverb().mix(), 0.33f),
          "reverb round-trips");

    // Automation lane.
    const audio::AutoLane& lane2 = automation2.lane(audio::AutoTarget::FilterCutoff);
    check(lane2.enabled && lane2.lfo.shape == audio::Waveform::Saw &&
              near(lane2.lfo.rateHz, 1.75f) && near(lane2.lo, 300.0f) && near(lane2.hi, 5500.0f) &&
              lane2.sync && lane2.syncDiv == 3,
          "automation lane round-trips");
    const audio::AutoLane& clipLane2 = automation2.lane(audio::AutoTarget::ReverbMix);
    check(clipLane2.clip.size() == 3 && near(static_cast<float>(clipLane2.clipLength), 4.0f) &&
              near(static_cast<float>(clipLane2.clip[1].time), 1.5f) &&
              near(clipLane2.clip[1].value, 0.9f),
          "automation clip (breakpoints) round-trips");
    const audio::AutoLane& leadVol2 = automation2.lane(audio::AutoTarget::LeadVolume);
    check(leadVol2.enabled && leadVol2.lfo.shape == audio::Waveform::Triangle &&
              near(leadVol2.lfo.rateHz, 0.8f) && near(leadVol2.lo, 0.2f) && near(leadVol2.hi, 0.95f),
          "lead-volume automation lane round-trips");
    const audio::AutoLane& send2 = automation2.lane(audio::AutoTarget::ReverbSend);
    check(send2.enabled && send2.lfo.shape == audio::Waveform::Square &&
              near(send2.lo, 0.1f) && near(send2.hi, 0.75f),
          "reverb-send automation lane round-trips");
    const audio::AutoLane& drumVol2 = automation2.lane(audio::AutoTarget::DrumVolume);
    check(drumVol2.enabled && drumVol2.lfo.shape == audio::Waveform::Saw &&
              near(drumVol2.lfo.rateHz, 0.6f) && near(drumVol2.lo, 0.15f) && near(drumVol2.hi, 0.9f),
          "drum-volume automation lane round-trips");
    const audio::AutoLane& bassCut2 = automation2.lane(audio::AutoTarget::BassCutoff);
    check(bassCut2.enabled && bassCut2.lfo.shape == audio::Waveform::Triangle &&
              near(bassCut2.lfo.rateHz, 0.45f) && near(bassCut2.lo, 200.0f) &&
              near(bassCut2.hi, 4800.0f),
          "bass-cutoff automation lane round-trips");
    const audio::AutoLane& dfb2 = automation2.lane(audio::AutoTarget::DelayFeedback);
    check(dfb2.enabled && near(dfb2.lfo.rateHz, 0.25f) && near(dfb2.lo, 0.2f) && near(dfb2.hi, 0.8f),
          "delay-feedback automation lane round-trips");

    // A non-.cjc file is rejected.
    audio::Sequencer seq3;
    audio::Mixer mixer3;
    audio::Automation automation3;
    check(!audio::loadProject("/nonexistent/definitely_missing.cjc", seq3, mixer3, automation3, &err),
          "loading a missing file fails cleanly");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
