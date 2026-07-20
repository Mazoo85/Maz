#include "maz/audio/ProjectIO.hpp"

#include "maz/audio/Automation.hpp"
#include "maz/audio/Mixer.hpp"
#include "maz/audio/Sequencer.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace maz::audio {

namespace {
// Parse a `synth`/`synth2` line into a SynthInstrument (filter fields optional for old files).
void parseSynthLine(std::istringstream& ls, SynthInstrument& syn) {
    int mode = 0, wave = 0;
    float atk = 0.005f, dec = 0.08f, sus = 0.6f, rel = 0.12f, ratio = 2.0f, index = 3.0f,
          gain = 0.28f;
    ls >> mode >> wave >> atk >> dec >> sus >> rel >> ratio >> index >> gain;
    syn.setMode(mode == 1 ? SynthMode::FM
                          : (mode == 2 ? SynthMode::Wavetable
                                       : (mode == 3 ? SynthMode::Pluck
                                                    : (mode == 4 ? SynthMode::Organ
                                                                 : (mode == 5 ? SynthMode::PhaseDistortion
                                                                              : SynthMode::Subtractive)))));
    syn.setWaveform(static_cast<Waveform>(wave < 0 || wave > 6 ? 0 : wave));
    syn.setEnvelope(atk, dec, sus, rel);
    syn.setFmRatio(ratio);
    syn.setFmIndex(index);
    syn.setGain(gain);
    float cutoff = 20000.0f, reso = 0.7f, envAmt = 0.0f;
    if (ls >> cutoff >> reso >> envAmt) {
        syn.setFilter(cutoff, reso, envAmt);
    }
    float wtPos = 0.0f, wtMorph = 0.0f; // wavetable fields optional for old files
    if (ls >> wtPos >> wtMorph) {
        syn.setWavetablePosition(wtPos);
        syn.setWavetableMorph(wtMorph);
    }
    float glide = 0.0f; // glide optional for old files
    if (ls >> glide) {
        syn.setGlide(glide);
    }
    int f0 = 0, f1 = 1, f2 = 2, f3 = 3; // wavetable frames optional for old files
    if (ls >> f0 >> f1 >> f2 >> f3) {
        auto wf = [](int v) { return static_cast<Waveform>(v < 0 || v > 6 ? 0 : v); };
        syn.setWavetableFrames(wf(f0), wf(f1), wf(f2), wf(f3));
    }
    float vibRate = 5.0f, vibDepth = 0.0f; // vibrato optional for old files
    if (ls >> vibRate >> vibDepth) {
        syn.setVibrato(vibRate, vibDepth);
    }
    float peAmt = 0.0f, peTime = 0.05f; // pitch envelope optional for old files
    if (ls >> peAmt >> peTime) {
        syn.setPitchEnv(peAmt, peTime);
    }
    float velCut = 0.0f; // velocity→cutoff optional for old files
    if (ls >> velCut) {
        syn.setVelToCutoff(velCut);
    }
    float fmFb = 0.0f; // FM feedback optional for old files
    if (ls >> fmFb) {
        syn.setFmFeedback(fmFb);
    }
    float ring = 0.0f; // ring mod optional for old files
    if (ls >> ring) {
        syn.setRingMod(ring);
    }
    float wtLfoRate = 0.0f, wtLfoDepth = 0.0f; // wavetable scan LFO optional for old files
    if (ls >> wtLfoRate >> wtLfoDepth) {
        syn.setWavetableLfo(wtLfoRate, wtLfoDepth);
    }
    float velSens = 1.0f; // velocity→amp sensitivity optional for old files
    if (ls >> velSens) {
        syn.setVelSensitivity(velSens);
    }
    float keyTrack = 0.0f; // filter key tracking optional for old files
    if (ls >> keyTrack) {
        syn.setFilterKeyTrack(keyTrack);
    }
    int octave = 0; // per-instrument octave optional for old files
    if (ls >> octave) {
        syn.setOctave(octave);
    }
    int mono = 0; // monophonic mode optional for old files
    if (ls >> mono) {
        syn.setMono(mono != 0);
    }
    float filtLfoRate = 0.0f, filtLfoDepth = 0.0f; // filter cutoff LFO optional for old files
    if (ls >> filtLfoRate >> filtLfoDepth) {
        syn.setFilterLfo(filtLfoRate, filtLfoDepth);
    }
    float ampLfoRate = 0.0f, ampLfoDepth = 0.0f; // amplitude LFO optional for old files
    if (ls >> ampLfoRate >> ampLfoDepth) {
        syn.setAmpLfo(ampLfoRate, ampLfoDepth);
    }
    float drift = 0.0f; // analog drift optional for old files
    if (ls >> drift) {
        syn.setDrift(drift);
    }
    int fmode = 0; // filter mode optional for old files (0 = low-pass)
    if (ls >> fmode) {
        fmode = fmode < 0 || fmode > 3 ? 0 : fmode;
        syn.setFilterMode(static_cast<StateVariableFilter::Mode>(fmode));
    }
    float fa = 0.0f, fd = 0.0f, fs = 0.0f, fr = 0.0f, fdepth = 0.0f; // filter env optional for old files
    if (ls >> fa >> fd >> fs >> fr >> fdepth) {
        syn.setFilterEnvelope(fa, fd, fs, fr);
        syn.setFilterEnvDepth(fdepth);
    }
    float fdrive = 0.0f; // filter drive optional for old files (0 = clean)
    if (ls >> fdrive) {
        syn.setFilterDrive(fdrive);
    }
    float phaseRand = 0.0f; // start-phase randomization optional for old files (0 = off)
    if (ls >> phaseRand) {
        syn.setStartPhaseRandom(phaseRand);
    }
    float velFm = 0.0f; // velocity → FM index optional for old files (0 = off)
    if (ls >> velFm) {
        syn.setVelToFmIndex(velFm);
    }
    float vibDelay = 0.0f; // vibrato onset delay optional for old files (0 = immediate)
    if (ls >> vibDelay) {
        syn.setVibratoDelay(vibDelay);
    }
    float velWave = 0.0f; // velocity → wavetable position optional for old files (0 = off)
    if (ls >> velWave) {
        syn.setVelToWavePosition(velWave);
    }
    int glideLegato = 0; // legato-only glide optional for old files (0 = always)
    if (ls >> glideLegato) {
        syn.setGlideLegato(glideLegato != 0);
    }
    float pwmRate = 0.0f, pwmDepth = 0.0f; // PWM LFO optional for old files (0 = off)
    if (ls >> pwmRate >> pwmDepth) {
        syn.setPwmLfo(pwmRate, pwmDepth);
    }
    int flSync = 0, flDiv = 3; // filter-LFO tempo sync optional for old files (0 = free-running)
    if (ls >> flSync >> flDiv) {
        syn.setFilterLfoSync(flSync != 0);
        syn.setFilterLfoSyncDivision(flDiv);
    }
    int alSync = 0, alDiv = 3; // amp-LFO tempo sync optional for old files (0 = free-running)
    if (ls >> alSync >> alDiv) {
        syn.setAmpLfoSync(alSync != 0);
        syn.setAmpLfoSyncDivision(alDiv);
    }
    int viSync = 0, viDiv = 3; // vibrato tempo sync optional for old files (0 = free-running)
    if (ls >> viSync >> viDiv) {
        syn.setVibratoSync(viSync != 0);
        syn.setVibratoSyncDivision(viDiv);
    }
    int flShape = 0; // cutoff-LFO shape optional for old files (0 = sine)
    if (ls >> flShape) {
        syn.setFilterLfoShape(static_cast<Waveform>(flShape < 0 || flShape > 6 ? 0 : flShape));
    }
    int alShape = 0; // tremolo-LFO shape optional for old files (0 = sine)
    if (ls >> alShape) {
        syn.setAmpLfoShape(static_cast<Waveform>(alShape < 0 || alShape > 6 ? 0 : alShape));
    }
    int viShape = 0; // vibrato-LFO shape optional for old files (0 = sine)
    if (ls >> viShape) {
        syn.setVibratoShape(static_cast<Waveform>(viShape < 0 || viShape > 6 ? 0 : viShape));
    }
    float pdAmt = 0.0f; // phase-distortion amount optional for old files (0 = clean sine)
    if (ls >> pdAmt) {
        syn.setPdAmount(pdAmt);
    }
    int flSH = 0; // filter-LFO sample & hold optional for old files (0 = periodic shape)
    if (ls >> flSH) {
        syn.setFilterLfoSampleHold(flSH != 0);
    }
    int fslope = 12; // filter slope optional for old files (12 dB/oct)
    if (ls >> fslope) {
        syn.setFilterSlope(fslope);
    }
    float naAmt = 0.0f, naDecay = 15.0f; // noise attack optional for old files (0 = off)
    if (ls >> naAmt >> naDecay) {
        syn.setNoiseAttack(naAmt, naDecay);
    }
    int alSH = 0; // amp-LFO sample & hold optional for old files (0 = periodic shape)
    if (ls >> alSH) {
        syn.setAmpLfoSampleHold(alSH != 0);
    }
    int viSH = 0; // vibrato sample & hold optional for old files (0 = periodic shape)
    if (ls >> viSH) {
        syn.setVibratoSampleHold(viSH != 0);
    }
    float velAtk = 0.0f; // velocity → attack optional for old files (0 = off)
    if (ls >> velAtk) {
        syn.setVelToAttack(velAtk);
    }
    int fmModW = 0; // FM modulator waveform optional for old files (0 = Sine)
    if (ls >> fmModW) {
        syn.setFmModWaveform(static_cast<Waveform>(fmModW < 0 || fmModW > 6 ? 0 : fmModW));
    }
    float fold = 0.0f; // West-coast wavefolder amount optional for old files (0 = bypass)
    if (ls >> fold) {
        syn.setFold(fold);
    }
}
// Parse a `synthosc`/`synthosc2` line.
void parseOscLine(std::istringstream& ls, SynthInstrument& syn) {
    float detune = 0.0f, osc2 = 0.0f, sub = 0.0f, noise = 0.0f;
    ls >> detune >> osc2 >> sub >> noise;
    syn.setOscillators(detune, osc2, sub, noise);
    int uni = 1;
    float uniDet = 12.0f; // unison optional for old files
    if (ls >> uni >> uniDet) {
        syn.setUnison(uni, uniDet);
    }
    int subW = 0; // sub waveform optional for old files
    if (ls >> subW) {
        syn.setSubWaveform(static_cast<Waveform>(subW < 0 || subW > 6 ? 0 : subW));
    }
    float noiseCol = 0.0f; // noise color optional for old files
    if (ls >> noiseCol) {
        syn.setNoiseColor(noiseCol);
    }
    int hardSync = 0; // hard sync optional for old files
    float syncRatio = 1.5f;
    if (ls >> hardSync >> syncRatio) {
        syn.setHardSync(hardSync != 0);
        syn.setSyncRatio(syncRatio);
    }
    float pw = 0.5f; // pulse width optional for old files
    if (ls >> pw) {
        syn.setPulseWidth(pw);
    }
    float osc2semi = 0.0f; // osc2 coarse tune optional for old files
    if (ls >> osc2semi) {
        syn.setOsc2Semitones(osc2semi);
    }
    int subOct = 1; // sub octave optional for old files
    if (ls >> subOct) {
        syn.setSubOctave(subOct);
    }
    float osc3lvl = 0.0f, osc3semi = 0.0f; // 3rd oscillator optional for old files
    if (ls >> osc3lvl >> osc3semi) {
        syn.setOsc3Level(osc3lvl);
        syn.setOsc3Semitones(osc3semi);
    }
    int osc2Linked = 1, osc2w = 0; // osc2 waveform optional for old files (linked → follows primary)
    if (ls >> osc2Linked >> osc2w) {
        syn.setOsc2Waveform(static_cast<Waveform>(osc2w < 0 || osc2w > 6 ? 0 : osc2w));
        syn.setOsc2WaveformLinked(osc2Linked != 0);
    }
    int osc3Linked = 1, osc3w = 0; // osc3 waveform optional for old files (linked → follows primary)
    if (ls >> osc3Linked >> osc3w) {
        syn.setOsc3Waveform(static_cast<Waveform>(osc3w < 0 || osc3w > 6 ? 0 : osc3w));
        syn.setOsc3WaveformLinked(osc3Linked != 0);
    }
    float osc3fine = 0.0f; // osc3 fine tune optional for old files (0 = dead-on)
    if (ls >> osc3fine) {
        syn.setOsc3FineTune(osc3fine);
    }
    float pluckDamp = 0.0f; // pluck damping optional for old files (0 = natural KS decay)
    if (ls >> pluckDamp) {
        syn.setPluckDamping(pluckDamp);
    }
    float pluckPos = 0.0f; // pluck position optional for old files (0 = no excitation comb)
    if (ls >> pluckPos) {
        syn.setPluckPosition(pluckPos);
    }
    // Organ drawbars optional for old files (absent → the default fundamental-only tone).
    for (int b = 0; b < SynthInstrument::kOrganBars; ++b) {
        float lvl = 0.0f;
        if (ls >> lvl) {
            syn.setOrganBar(b, lvl);
        }
    }
    float organPerc = 0.0f; // organ percussion optional for old files (0 = off)
    int organPercThird = 0;
    if (ls >> organPerc >> organPercThird) {
        syn.setOrganPercussion(organPerc);
        syn.setOrganPercThird(organPercThird != 0);
    }
    // Absent → osc2 stays linked to the primary (the default), matching old files.
}
} // namespace

// File format (line-based text, ".cjc"):
//   cjc 1
//   bpm <double>
//   busgain <drum> <synth>
//   step <channel> <step>                 (repeated, one per active drum step)
//   note <start> <len> <pitch> <velocity> [prob] [fine] [roll] [slide] (one per piano-roll note)
//   master <gain>
//   fx eq <enabled> <cutoff>
//   fx comp <enabled> <thrDb> <ratio> <atkMs> <relMs> <makeupDb>
//   fx transient <enabled> <attack> <sustain>
//   fx delay <enabled> <timeMs> <feedback> <mix>
//   fx reverb <enabled> <roomSize> <damping> <mix>

// Serialize one synth engine (its main parameter line + oscillator line) to a stream. Shared by the
// whole-project saver and the standalone instrument-preset saver so both formats stay identical and a
// preset round-trips through the exact same parser (parseSynthLine/parseOscLine).
static void writeSynthBlock(std::ostream& f, const char* tag, const char* oscTag,
                            const SynthInstrument& s) {
    f << tag << " " << static_cast<int>(s.mode()) << " " << static_cast<int>(s.waveform()) << " "
      << s.attack() << " " << s.decay() << " " << s.sustain() << " " << s.release() << " "
      << s.fmRatio() << " " << s.fmIndex() << " " << s.gain() << " " << s.filterCutoff() << " "
      << s.filterResonance() << " " << s.filterEnvAmount() << " " << s.wavetablePosition() << " "
      << s.wavetableMorph() << " " << s.glide() << " " << static_cast<int>(s.wavetableFrame(0))
      << " " << static_cast<int>(s.wavetableFrame(1)) << " "
      << static_cast<int>(s.wavetableFrame(2)) << " " << static_cast<int>(s.wavetableFrame(3))
      << " " << s.vibratoRate() << " " << s.vibratoDepth() << " " << s.pitchEnvAmount() << " "
      << s.pitchEnvTime() << " " << s.velToCutoff() << " " << s.fmFeedback() << " "
      << s.ringMod() << " " << s.wavetableLfoRate() << " " << s.wavetableLfoDepth() << " "
      << s.velSensitivity() << " " << s.filterKeyTrack() << " " << s.octave() << " "
      << (s.mono() ? 1 : 0) << " " << s.filterLfoRate() << " " << s.filterLfoDepth() << " "
      << s.ampLfoRate() << " " << s.ampLfoDepth() << " " << s.drift() << " "
      << static_cast<int>(s.filterMode()) << " " << s.filterEnvAttack() << " "
      << s.filterEnvDecay() << " " << s.filterEnvSustain() << " " << s.filterEnvRelease() << " "
      << s.filterEnvDepth() << " " << s.filterDrive() << " " << s.startPhaseRandom() << " "
      << s.velToFmIndex() << " " << s.vibratoDelay() << " " << s.velToWavePosition() << " "
      << (s.glideLegato() ? 1 : 0) << " " << s.pwmLfoRate() << " " << s.pwmLfoDepth() << " "
      << (s.filterLfoSync() ? 1 : 0) << " " << s.filterLfoSyncDivision() << " "
      << (s.ampLfoSync() ? 1 : 0) << " " << s.ampLfoSyncDivision() << " "
      << (s.vibratoSync() ? 1 : 0) << " " << s.vibratoSyncDivision() << " "
      << static_cast<int>(s.filterLfoShape()) << " " << static_cast<int>(s.ampLfoShape()) << " "
      << static_cast<int>(s.vibratoShape()) << " " << s.pdAmount() << " "
      << (s.filterLfoSampleHold() ? 1 : 0) << " " << s.filterSlope() << " "
      << s.noiseAttackAmount() << " " << s.noiseAttackDecay() << " "
      << (s.ampLfoSampleHold() ? 1 : 0) << " " << (s.vibratoSampleHold() ? 1 : 0) << " "
      << s.velToAttack() << " " << static_cast<int>(s.fmModWaveform()) << " " << s.fold() << "\n";
    f << oscTag << " " << s.detuneCents() << " " << s.osc2Level() << " " << s.subLevel() << " "
      << s.noiseLevel() << " " << s.unisonVoices() << " " << s.unisonDetune() << " "
      << static_cast<int>(s.subWaveform()) << " " << s.noiseColor() << " "
      << (s.hardSync() ? 1 : 0) << " " << s.syncRatio() << " " << s.pulseWidth() << " "
      << s.osc2Semitones() << " " << s.subOctave() << " " << s.osc3Level() << " "
      << s.osc3Semitones() << " " << (s.osc2WaveformLinked() ? 1 : 0) << " "
      << static_cast<int>(s.osc2Waveform()) << " " << (s.osc3WaveformLinked() ? 1 : 0) << " "
      << static_cast<int>(s.osc3Waveform()) << " " << s.osc3FineTune() << " "
      << s.pluckDamping() << " " << s.pluckPosition();
    for (int b = 0; b < SynthInstrument::kOrganBars; ++b) {
        f << " " << s.organBar(b);
    }
    f << " " << s.organPercussion() << " " << (s.organPercThird() ? 1 : 0);
    f << "\n";
}

// Serialize a mixer track's insert-strip fields (everything after the tag+index) to a stream, with no
// leading space and no trailing newline. Shared by the fixed per-bus `track` lines and the `group`
// (submix) lines so both round-trip through the exact same format and reader.
static void writeMixerTrackFields(std::ostream& f, MixerTrack& tr) {
    f << tr.gain() << " " << (tr.muted() ? 1 : 0) << " " << (tr.eq().enabled() ? 1 : 0) << " "
      << tr.eq().lowGain() << " " << tr.eq().midFreq() << " " << tr.eq().midQ() << " "
      << tr.eq().midGain() << " " << tr.eq().highGain() << " " << (tr.distortion().enabled() ? 1 : 0)
      << " " << tr.distortion().drive() << " " << (tr.compressor().enabled() ? 1 : 0) << " "
      << tr.compressor().thresholdDb() << " " << tr.compressor().ratio() << " "
      << tr.compressor().makeupDb() << " " << (tr.highpass().enabled() ? 1 : 0) << " "
      << tr.highpass().cutoff() << " " << tr.pan() << " " << (tr.transientShaper().enabled() ? 1 : 0)
      << " " << tr.transientShaper().attack() << " " << tr.transientShaper().sustain() << " "
      << (tr.gate().enabled() ? 1 : 0) << " " << tr.gate().thresholdDb() << " " << tr.gate().ratio()
      << " " << tr.gate().attackMs() << " " << tr.gate().releaseMs() << " " << (tr.soloed() ? 1 : 0)
      << " " << tr.reverbSend() << " " << tr.delaySend() << " "
      << (tr.stereoEnhancer().enabled() ? 1 : 0) << " " << tr.stereoEnhancer().delayMs() << " "
      << tr.stereoEnhancer().amount();
}

// Read the insert-strip fields written by writeMixerTrackFields into a track. The later blocks are
// guarded so older/shorter lines (which omit them) still load — matching the pre-helper reader.
static void readMixerTrackFields(std::istringstream& ls, MixerTrack& tr) {
    int muted = 0, eqEn = 0, distEn = 0, compEn = 0;
    float gain = 1.0f, low = 0.0f, midF = 1000.0f, midQ = 1.0f, midDb = 0.0f, high = 0.0f;
    float drive = 1.0f, thr = -18.0f, ratio = 4.0f, mk = 0.0f;
    ls >> gain >> muted >> eqEn >> low >> midF >> midQ >> midDb >> high >> distEn >> drive >> compEn >>
        thr >> ratio >> mk;
    tr.setGain(gain);
    tr.setMuted(muted != 0);
    tr.eq().setEnabled(eqEn != 0);
    tr.eq().setLowGain(low);
    tr.eq().setMid(midF, midQ, midDb);
    tr.eq().setHighGain(high);
    tr.distortion().setEnabled(distEn != 0);
    tr.distortion().setDrive(drive);
    tr.compressor().setEnabled(compEn != 0);
    tr.compressor().setThresholdDb(thr);
    tr.compressor().setRatio(ratio);
    tr.compressor().setMakeupDb(mk);
    int hpEn = 0;
    float hpCut = 30.0f;
    if (ls >> hpEn >> hpCut) {
        tr.highpass().setEnabled(hpEn != 0);
        tr.highpass().setCutoff(hpCut);
    }
    float pan = 0.0f;
    if (ls >> pan) {
        tr.setPan(pan);
    }
    int trEn = 0;
    float trAtt = 0.0f, trSus = 0.0f;
    if (ls >> trEn >> trAtt >> trSus) {
        tr.transientShaper().setEnabled(trEn != 0);
        tr.transientShaper().setAttack(trAtt);
        tr.transientShaper().setSustain(trSus);
    }
    int gEn = 0;
    float gThr = -40.0f, gRatio = 4.0f, gAtk = 2.0f, gRel = 80.0f;
    if (ls >> gEn >> gThr >> gRatio >> gAtk >> gRel) {
        tr.gate().setEnabled(gEn != 0);
        tr.gate().setThresholdDb(gThr);
        tr.gate().setRatio(gRatio);
        tr.gate().setAttackMs(gAtk);
        tr.gate().setReleaseMs(gRel);
    }
    int solo = 0;
    if (ls >> solo) {
        tr.setSoloed(solo != 0);
    }
    float rsend = 0.0f, dsend = 0.0f;
    if (ls >> rsend >> dsend) {
        tr.setReverbSend(rsend);
        tr.setDelaySend(dsend);
    }
    int seEn = 0;
    float seMs = 12.0f, seAmt = 0.7f;
    if (ls >> seEn >> seMs >> seAmt) {
        tr.stereoEnhancer().setEnabled(seEn != 0);
        tr.stereoEnhancer().setDelayMs(seMs);
        tr.stereoEnhancer().setAmount(seAmt);
    }
}

// Serialize the whole project to any output stream (shared by the file and string savers).
static void writeProjectTo(std::ostream& f, Sequencer& seq, Mixer& mixer, Automation& automation) {
    f << "cjc 1\n";
    f << "bpm " << seq.bpm() << "\n";
    f << "steps " << seq.numSteps() << "\n";
    f << "spb " << seq.stepsPerBeat() << "\n";
    f << "swing " << seq.swing() << "\n";
    f << "sidechain " << (seq.sidechainOn() ? 1 : 0) << " " << seq.sidechainAmount() << " "
      << seq.sidechainReleaseMs() << " " << seq.sidechainSource() << " " << seq.sidechainAttackMs()
      << "\n";
    f << "arp " << (seq.arpOn() ? 1 : 0) << " " << seq.arpMode() << " " << seq.arpOctaves() << " "
      << seq.arpGate() << " " << seq.arpRate() << "\n";
    f << "humanize " << seq.humanize() << "\n";
    f << "metronome " << (seq.metronome() ? 1 : 0) << " " << seq.metronomeLevel() << "\n";
    f << "countin " << seq.countInBars() << "\n";
    // MIDI-learn CC bindings: one line per bound controller (controller number + target enum index).
    for (int cc = 0; cc < 128; ++cc) {
        const auto tgt = seq.midiCcTarget(cc);
        if (tgt != Sequencer::CcTarget::None) {
            f << "ccmap " << cc << " " << static_cast<int>(tgt) << "\n";
        }
    }
    f << "busgain " << seq.drumGain() << " " << seq.synthGain() << " " << seq.bassGain() << " "
      << seq.leadPan() << " " << seq.bassPan() << " " << seq.transpose() << "\n";

    writeSynthBlock(f, "synth", "synthosc", seq.synth());
    writeSynthBlock(f, "synth2", "synthosc2", seq.synth2());

    // Extra instrument channels: a count, then each channel's synth patch (reusing the shared synth
    // block format under an `instchannel` selector). Written before the pattern grid so the channels
    // exist when the per-pattern extra-lane notes (noteN) are read back.
    if (seq.instrumentChannelCount() > 0) {
        f << "instchannelcount " << seq.instrumentChannelCount() << "\n";
        for (int c = 0; c < seq.instrumentChannelCount(); ++c) {
            f << "instchannel " << c << " " << seq.instrumentGain(c) << " " << seq.instrumentPan(c)
              << " " << seq.instrumentBus(c) << "\n";
            writeSynthBlock(f, "synthI", "synthoscI", seq.instrumentSynth(c));
            if (!seq.instrumentPluginPath(c).empty()) {
                f << "instplugin " << c << " " << seq.instrumentPluginPath(c) << "\n";
            }
        }
    }

    f << "samplercfg " << (seq.sampler().reverse() ? 1 : 0) << " " << (seq.sampler().loop() ? 1 : 0)
      << " " << seq.sampler().startOffset() << " " << seq.sampler().attack() << " "
      << seq.sampler().release() << " " << (seq.sampler().pingPong() ? 1 : 0) << " "
      << seq.sampler().detuneCents() << " " << seq.sampler().loopStart() << " "
      << seq.sampler().loopEnd() << " " << seq.sampler().slices() << " "
      << seq.sampler().filterCutoff() << " " << seq.sampler().filterResonance() << " "
      << seq.sampler().filterEnvAttack() << " " << seq.sampler().filterEnvDecay() << " "
      << seq.sampler().filterEnvSustain() << " " << seq.sampler().filterEnvRelease() << " "
      << seq.sampler().filterEnvDepth() << " " << seq.sampler().pitchEnvDepth() << " "
      << seq.sampler().pitchEnvTime() << " " << seq.sampler().filterVelo() << " "
      << (seq.sampler().mono() ? 1 : 0) << " " << seq.sampler().ampDecay() << " "
      << seq.sampler().ampSustain() << " " << seq.sampler().velSensitivity() << " "
      << (seq.sampler().keyTrack() ? 1 : 0) << " " << seq.sampler().drive() << " "
      << seq.sampler().glide() << " " << (seq.sampler().glideLegato() ? 1 : 0) << " "
      << seq.sampler().velToAttack() << " " << seq.sampler().velToStart() << " "
      << seq.sampler().filterKeyTrack() << " "
      << static_cast<int>(seq.sampler().filterMode()) << " "
      << seq.sampler().filterLfoRate() << " " << seq.sampler().filterLfoDepth() << " "
      << (seq.sampler().filterLfoSync() ? 1 : 0) << " " << seq.sampler().filterLfoSyncDivision()
      << " " << static_cast<int>(seq.sampler().filterLfoShape()) << "\n";
    f << "sampler " << (seq.useSampler() ? 1 : 0) << " " << seq.sampler().basePitch() << " "
      << seq.sampler().gain() << " " << seq.sampler().path() << "\n";

    for (int c = 0; c < seq.numChannels(); ++c) {
        f << "chan " << c << " " << seq.channelVolume(c) << " " << (seq.channelMute(c) ? 1 : 0)
          << " " << (seq.channelSolo(c) ? 1 : 0) << " " << seq.channelPan(c) << " "
          << seq.channelChokeGroup(c) << " " << seq.channelTune(c) << " " << seq.channelDecay(c)
          << " " << seq.channelDrive(c) << " " << seq.channelFlam(c) << " "
          << static_cast<int>(seq.channelType(c)) << " " << seq.channelPitchEnv(c) << " "
          << seq.channelTone(c) << " " << seq.channelPitchEnvTime(c) << " " << seq.channelSnap(c)
          << " " << seq.channelHighpass(c) << "\n";
    }

    // Arrangement: every pattern's grid + notes, the playlist, and the song-mode flag.
    const int savedCurrent = seq.currentPattern();
    f << "patterns " << seq.patternCount() << "\n";
    f << "songmode " << (seq.songMode() ? 1 : 0) << " " << (seq.songLoop() ? 1 : 0) << " "
      << seq.songLoopStart() << " " << seq.songLoopEnd() << " " << (seq.songUsesClips() ? 1 : 0)
      << "\n";
    f << "playlist " << seq.playlist().size();
    for (int idx : seq.playlist()) {
        f << " " << idx;
    }
    f << "\n";
    // 2-D playlist clips (pattern @ bar @ track). Written after the legacy playlist; only when present.
    for (const PlaylistClip& cl : seq.clips()) {
        f << "clip " << cl.pattern << " " << cl.startBar << " " << cl.track << "\n";
    }
    for (int p = 0; p < seq.patternCount(); ++p) {
        seq.selectPattern(p);
        f << "patname " << p << " " << seq.patternName(p) << "\n";
        f << "patswing " << p << " " << seq.swing() << "\n"; // per-pattern groove
        f << "pattranspose " << p << " " << seq.patternTranspose() << "\n"; // per-pattern key change
        f << "pattempo " << p << " " << seq.patternTempoMul() << "\n"; // per-pattern tempo multiplier

        for (int c = 0; c < seq.numChannels(); ++c) {
            for (int s = 0; s < seq.numSteps(); ++s) {
                if (seq.step(c, s)) {
                    const int vel = static_cast<int>(seq.stepVelocity(c, s) * 255.0f + 0.5f);
                    const int prob = static_cast<int>(seq.stepProbability(c, s) * 255.0f + 0.5f);
                    f << "step " << p << " " << c << " " << s << " " << vel << " " << prob << " "
                      << seq.stepRatchet(c, s) << " " << seq.stepTune(c, s) << " "
                      << seq.stepNudge(c, s) << " " << seq.stepStride(c, s) << "\n";
                }
            }
        }
        for (const Note& n : seq.roll2().notes()) {
            f << "note2 " << p << " " << n.startStep << " " << n.lengthSteps << " " << n.pitch << " "
              << n.velocity << " " << n.probability << " " << n.fineTune << " " << n.roll << " "
              << (n.slide ? 1 : 0) << " " << n.stride << " " << n.nudge << " " << n.cutoff << "\n";
        }
        for (const Note& n : seq.roll().notes()) {
            f << "note " << p << " " << n.startStep << " " << n.lengthSteps << " " << n.pitch << " "
              << n.velocity << " " << n.probability << " " << n.fineTune << " " << n.roll << " "
              << (n.slide ? 1 : 0) << " " << n.stride << " " << n.nudge << " " << n.cutoff << "\n";
        }
        // Extra instrument-channel lanes (basic note fields: channel, pattern, start, len, pitch, vel,
        // fine). `noteN <channel> <pattern> ...`.
        for (int c = 0; c < seq.instrumentChannelCount(); ++c) {
            for (const Note& n : seq.instrumentRoll(c).notes()) {
                f << "noteN " << c << " " << p << " " << n.startStep << " " << n.lengthSteps << " "
                  << n.pitch << " " << n.velocity << " " << n.fineTune << " " << n.probability << " "
                  << n.roll << " " << (n.slide ? 1 : 0) << " " << n.stride << " " << n.nudge << " "
                  << n.cutoff << "\n";
            }
        }
    }
    seq.selectPattern(savedCurrent);

    f << "master " << mixer.masterGain() << " " << mixer.limiterCeiling() << " "
      << mixer.masterBalance() << "\n";
    f << "fx eq " << (mixer.eq().enabled() ? 1 : 0) << " " << mixer.eq().cutoff() << "\n";
    f << "fx filter " << (mixer.filter().enabled() ? 1 : 0) << " "
      << static_cast<int>(mixer.filter().mode()) << " " << mixer.filter().cutoff() << " "
      << mixer.filter().resonance() << " " << mixer.filter().drive() << "\n";
    f << "fx hp " << (mixer.highpass().enabled() ? 1 : 0) << " " << mixer.highpass().cutoff() << "\n";
    f << "fx comp " << (mixer.compressor().enabled() ? 1 : 0) << " "
      << mixer.compressor().thresholdDb() << " " << mixer.compressor().ratio() << " "
      << mixer.compressor().attackMs() << " " << mixer.compressor().releaseMs() << " "
      << mixer.compressor().makeupDb() << " " << mixer.compressor().kneeDb() << " "
      << mixer.compressor().mix() << " " << mixer.compressor().sidechainHpf() << " "
      << (mixer.compressor().autoMakeup() ? 1 : 0) << " " << mixer.compressor().lookaheadMs()
      << " " << (mixer.compressor().rmsDetection() ? 1 : 0) << " "
      << (mixer.compressor().stereoLink() ? 1 : 0) << "\n";
    f << "fx multiband " << (mixer.multiband().enabled() ? 1 : 0) << " "
      << mixer.multiband().crossoverLow() << " " << mixer.multiband().crossoverHigh() << " "
      << mixer.multiband().bandThreshold(0) << " " << mixer.multiband().bandRatio(0) << " "
      << mixer.multiband().bandThreshold(1) << " " << mixer.multiband().bandRatio(1) << " "
      << mixer.multiband().bandThreshold(2) << " " << mixer.multiband().bandRatio(2) << " "
      << mixer.multiband().attackMs() << " " << mixer.multiband().releaseMs() << "\n";
    f << "fx transient " << (mixer.transient().enabled() ? 1 : 0) << " "
      << mixer.transient().attack() << " " << mixer.transient().sustain() << "\n";
    f << "fx mbtransient " << (mixer.multibandTransient().enabled() ? 1 : 0) << " "
      << mixer.multibandTransient().crossoverLow() << " "
      << mixer.multibandTransient().crossoverHigh() << " " << mixer.multibandTransient().attack(0)
      << " " << mixer.multibandTransient().sustain(0) << " " << mixer.multibandTransient().attack(1)
      << " " << mixer.multibandTransient().sustain(1) << " " << mixer.multibandTransient().attack(2)
      << " " << mixer.multibandTransient().sustain(2) << "\n";
    f << "fx delay " << (mixer.delay().enabled() ? 1 : 0) << " " << mixer.delay().time() << " "
      << mixer.delay().feedback() << " " << mixer.delay().mix() << " "
      << (mixer.delay().pingPong() ? 1 : 0) << " " << mixer.delay().damping() << " "
      << (mixer.delay().sync() ? 1 : 0) << " " << mixer.delay().syncDivision() << " "
      << mixer.delay().feedbackLowCut() << " " << mixer.delay().modDepth() << " "
      << mixer.delay().modRate() << " " << mixer.delay().duck() << " "
      << mixer.delay().feedbackDrive() << "\n";
    f << "fx reverb " << (mixer.reverb().enabled() ? 1 : 0) << " " << mixer.reverb().roomSize()
      << " " << mixer.reverb().damping() << " " << mixer.reverb().mix() << " "
      << mixer.reverb().preDelayMs() << " " << mixer.reverb().width() << " "
      << (mixer.reverb().freeze() ? 1 : 0) << " " << mixer.reverb().duck() << " "
      << mixer.reverb().wetLowCut() << " " << mixer.reverb().wetHighCut() << " "
      << mixer.reverb().gateMs() << " " << mixer.reverb().shimmer() << " "
      << mixer.reverb().modDepth() << " " << mixer.reverb().modRate() << "\n";
    f << "fx convolver " << (mixer.convolver().enabled() ? 1 : 0) << " " << mixer.convolver().decay()
      << " " << mixer.convolver().tone() << " " << mixer.convolver().mix() << "\n";
    f << "fx peq " << (mixer.peq().enabled() ? 1 : 0) << " " << mixer.peq().lowGain() << " "
      << mixer.peq().midFreq() << " " << mixer.peq().midQ() << " " << mixer.peq().midGain() << " "
      << mixer.peq().highGain() << " " << mixer.peq().mid2Freq() << " " << mixer.peq().mid2Q() << " "
      << mixer.peq().mid2Gain() << " " << mixer.peq().mid3Freq() << " " << mixer.peq().mid3Q()
      << " " << mixer.peq().mid3Gain() << "\n";
    f << "fx tilt " << (mixer.tilt().enabled() ? 1 : 0) << " " << mixer.tilt().tilt() << " "
      << mixer.tilt().pivot() << "\n";
    f << "fx exciter " << (mixer.exciter().enabled() ? 1 : 0) << " " << mixer.exciter().crossover()
      << " " << mixer.exciter().amount() << " " << (mixer.exciter().evenHarmonics() ? 1 : 0) << "\n";
    f << "fx dist " << (mixer.distortion().enabled() ? 1 : 0) << " " << mixer.distortion().drive()
      << " " << mixer.distortion().mix() << " " << static_cast<int>(mixer.distortion().curve()) << " "
      << mixer.distortion().tone() << " " << mixer.distortion().outputDb() << " "
      << mixer.distortion().bias() << "\n";
    f << "fx ampcab " << (mixer.ampCab().enabled() ? 1 : 0) << " " << mixer.ampCab().drive() << " "
      << mixer.ampCab().presence() << " " << mixer.ampCab().tone() << " " << mixer.ampCab().mix()
      << "\n";
    f << "fx chorus " << (mixer.chorus().enabled() ? 1 : 0) << " " << mixer.chorus().rate() << " "
      << mixer.chorus().depth() << " " << mixer.chorus().mix() << " "
      << (mixer.chorus().sync() ? 1 : 0) << " " << mixer.chorus().syncDivision() << " "
      << mixer.chorus().feedback() << " " << mixer.chorus().width() << " " << mixer.chorus().voices()
      << "\n";
    f << "fx vibrato " << (mixer.vibrato().enabled() ? 1 : 0) << " " << mixer.vibrato().rate() << " "
      << mixer.vibrato().depth() << " " << (mixer.vibrato().sync() ? 1 : 0) << " "
      << mixer.vibrato().syncDivision() << " " << static_cast<int>(mixer.vibrato().shape()) << "\n";
    f << "fx rotary " << (mixer.rotary().enabled() ? 1 : 0) << " " << mixer.rotary().rate() << " "
      << mixer.rotary().depth() << " " << mixer.rotary().mix() << " " << mixer.rotary().drive()
      << "\n";
    f << "fx flanger " << (mixer.flanger().enabled() ? 1 : 0) << " " << mixer.flanger().rate() << " "
      << mixer.flanger().depth() << " " << mixer.flanger().feedback() << " " << mixer.flanger().mix()
      << " " << (mixer.flanger().sync() ? 1 : 0) << " " << mixer.flanger().syncDivision() << " "
      << (mixer.flanger().invert() ? 1 : 0) << "\n";
    f << "fx phaser " << (mixer.phaser().enabled() ? 1 : 0) << " " << mixer.phaser().rate() << " "
      << mixer.phaser().depth() << " " << mixer.phaser().feedback() << " " << mixer.phaser().mix()
      << " " << (mixer.phaser().sync() ? 1 : 0) << " " << mixer.phaser().syncDivision() << " "
      << mixer.phaser().stages() << " " << (mixer.phaser().stereo() ? 1 : 0) << "\n";
    f << "fx crush " << (mixer.bitcrusher().enabled() ? 1 : 0) << " " << mixer.bitcrusher().bits()
      << " " << mixer.bitcrusher().downsample() << " " << mixer.bitcrusher().mix() << " "
      << mixer.bitcrusher().tone() << "\n";
    f << "fx gate " << (mixer.gate().enabled() ? 1 : 0) << " " << mixer.gate().thresholdDb() << " "
      << mixer.gate().ratio() << " " << mixer.gate().rangeDb() << " " << mixer.gate().attackMs()
      << " " << mixer.gate().releaseMs() << " " << mixer.gate().holdMs() << " "
      << mixer.gate().sidechainHpf() << "\n";
    f << "fx width " << (mixer.widener().enabled() ? 1 : 0) << " " << mixer.widener().width() << " "
      << mixer.widener().bassMonoHz() << "\n";
    f << "fx imager " << (mixer.imager().enabled() ? 1 : 0) << " " << mixer.imager().crossoverLow()
      << " " << mixer.imager().crossoverHigh() << " " << mixer.imager().bandWidth(0) << " "
      << mixer.imager().bandWidth(1) << " " << mixer.imager().bandWidth(2) << "\n";
    f << "fx mbsat " << (mixer.multibandSaturator().enabled() ? 1 : 0) << " "
      << mixer.multibandSaturator().crossoverLow() << " " << mixer.multibandSaturator().crossoverHigh()
      << " " << mixer.multibandSaturator().drive(0) << " " << mixer.multibandSaturator().drive(1) << " "
      << mixer.multibandSaturator().drive(2) << "\n";
    f << "fx stereoenh " << (mixer.stereoEnhancer().enabled() ? 1 : 0) << " "
      << mixer.stereoEnhancer().delayMs() << " " << mixer.stereoEnhancer().amount() << "\n";
    f << "fx autopan " << (mixer.autopan().enabled() ? 1 : 0) << " " << mixer.autopan().rate() << " "
      << mixer.autopan().depth() << " " << (mixer.autopan().sync() ? 1 : 0) << " "
      << mixer.autopan().syncDivision() << " " << static_cast<int>(mixer.autopan().shape()) << "\n";
    f << "fx monobass " << (mixer.monobass().enabled() ? 1 : 0) << " " << mixer.monobass().crossover()
      << "\n";
    f << "fx subbass " << (mixer.subbass().enabled() ? 1 : 0) << " " << mixer.subbass().amount() << " "
      << mixer.subbass().cutoff() << " " << mixer.subbass().tone() << "\n";
    f << "fx octaver " << (mixer.octaver().enabled() ? 1 : 0) << " " << mixer.octaver().amount() << " "
      << mixer.octaver().tone() << "\n";
    f << "fx beatrepeat " << (mixer.beatRepeat().enabled() ? 1 : 0) << " " << mixer.beatRepeat().sliceMs()
      << " " << mixer.beatRepeat().repeats() << " " << mixer.beatRepeat().mix() << " "
      << (mixer.beatRepeat().sync() ? 1 : 0) << " " << mixer.beatRepeat().syncDivision() << "\n";
    f << "fx utility " << (mixer.utility().enabled() ? 1 : 0) << " " << mixer.utility().gainDb() << " "
      << (mixer.utility().invertL() ? 1 : 0) << " " << (mixer.utility().invertR() ? 1 : 0) << " "
      << (mixer.utility().mono() ? 1 : 0) << " " << mixer.utility().width() << "\n";
    f << "fx limiter " << (mixer.limiter().enabled() ? 1 : 0) << " " << mixer.limiter().inputGainDb()
      << " " << mixer.limiter().ceilingDb() << " " << mixer.limiter().releaseMs() << " "
      << mixer.limiter().lookaheadMs() << "\n";
    f << "fx leveler " << (mixer.leveler().enabled() ? 1 : 0) << " " << mixer.leveler().targetDb()
      << " " << mixer.leveler().responseMs() << " " << mixer.leveler().maxGainDb() << "\n";
    f << "fx clipper " << (mixer.clipper().enabled() ? 1 : 0) << " " << mixer.clipper().driveDb() << " "
      << mixer.clipper().ceiling() << " " << mixer.clipper().hardness() << " "
      << mixer.clipper().mix() << "\n";
    f << "fx deesser " << (mixer.deEsser().enabled() ? 1 : 0) << " " << mixer.deEsser().thresholdDb()
      << " " << mixer.deEsser().frequency() << " " << mixer.deEsser().amount() << " "
      << mixer.deEsser().releaseMs() << "\n";
    f << "fx dyneq " << (mixer.dynamicEq().enabled() ? 1 : 0) << " " << mixer.dynamicEq().frequency()
      << " " << mixer.dynamicEq().q() << " " << mixer.dynamicEq().thresholdDb() << " "
      << mixer.dynamicEq().rangeDb() << " " << mixer.dynamicEq().attackMs() << " "
      << mixer.dynamicEq().releaseMs() << "\n";
    f << "fx autowah " << (mixer.autowah().enabled() ? 1 : 0) << " " << mixer.autowah().baseHz() << " "
      << mixer.autowah().rangeHz() << " " << mixer.autowah().sensitivity() << " "
      << mixer.autowah().resonance() << " " << mixer.autowah().attackMs() << " "
      << mixer.autowah().releaseMs() << " " << (mixer.autowah().downward() ? 1 : 0) << " "
      << mixer.autowah().mix() << "\n";
    f << "fx comb " << (mixer.comb().enabled() ? 1 : 0) << " " << mixer.comb().frequency() << " "
      << mixer.comb().feedback() << " " << mixer.comb().mix() << " " << mixer.comb().damping() << "\n";
    f << "fx chordres " << (mixer.chordResonator().enabled() ? 1 : 0) << " "
      << mixer.chordResonator().rootNote() << " " << static_cast<int>(mixer.chordResonator().chord())
      << " " << mixer.chordResonator().feedback() << " " << mixer.chordResonator().damping() << " "
      << mixer.chordResonator().mix() << "\n";
    f << "fx tremolo " << (mixer.tremolo().enabled() ? 1 : 0) << " " << mixer.tremolo().rate() << " "
      << mixer.tremolo().depth() << " " << static_cast<int>(mixer.tremolo().shape()) << " "
      << (mixer.tremolo().sync() ? 1 : 0) << " " << mixer.tremolo().syncDivision() << "\n";
    f << "fx stepgate " << (mixer.stepGate().enabled() ? 1 : 0) << " " << mixer.stepGate().rate()
      << " " << (mixer.stepGate().sync() ? 1 : 0) << " " << mixer.stepGate().syncDivision() << " "
      << mixer.stepGate().mix();
    for (int s = 0; s < StepGate::kSteps; ++s) {
        f << " " << mixer.stepGate().step(s);
    }
    f << "\n";
    f << "fx stereodelay " << (mixer.stereoDelay().enabled() ? 1 : 0) << " "
      << mixer.stereoDelay().leftMs() << " " << mixer.stereoDelay().rightMs() << " "
      << mixer.stereoDelay().feedback() << " " << mixer.stereoDelay().mix() << " "
      << (mixer.stereoDelay().sync() ? 1 : 0) << " " << mixer.stereoDelay().leftDivision() << " "
      << mixer.stereoDelay().rightDivision() << " " << mixer.stereoDelay().damping() << " "
      << mixer.stereoDelay().feedbackLowCut() << " " << (mixer.stereoDelay().pingPong() ? 1 : 0)
      << "\n";
    f << "fx revdelay " << (mixer.reverseDelay().enabled() ? 1 : 0) << " "
      << mixer.reverseDelay().timeMs() << " " << mixer.reverseDelay().feedback() << " "
      << mixer.reverseDelay().mix() << "\n";
    f << "fx multitap " << (mixer.multiTapDelay().enabled() ? 1 : 0) << " "
      << mixer.multiTapDelay().timeMs() << " " << mixer.multiTapDelay().taps() << " "
      << mixer.multiTapDelay().decay() << " " << mixer.multiTapDelay().mix() << " "
      << mixer.multiTapDelay().spread() << " " << mixer.multiTapDelay().feedback() << "\n";
    f << "fx formant " << (mixer.formant().enabled() ? 1 : 0) << " "
      << static_cast<int>(mixer.formant().vowel()) << " " << mixer.formant().mix() << " "
      << (mixer.formant().morphEnabled() ? 1 : 0) << " " << mixer.formant().morph() << " "
      << mixer.formant().formantShift() << "\n";
    f << "fx vocoder " << (mixer.vocoder().enabled() ? 1 : 0) << " "
      << static_cast<int>(mixer.vocoder().carrier()) << " " << mixer.vocoder().carrierHz() << " "
      << mixer.vocoder().releaseMs() << " " << mixer.vocoder().mix() << "\n";
    f << "fx tape " << (mixer.tape().enabled() ? 1 : 0) << " " << mixer.tape().drive() << " "
      << mixer.tape().warmth() << " " << mixer.tape().mix() << " " << mixer.tape().wowFlutter()
      << "\n";
    f << "fx ringmod " << (mixer.ringmod().enabled() ? 1 : 0) << " " << mixer.ringmod().freq() << " "
      << mixer.ringmod().mix() << " " << static_cast<int>(mixer.ringmod().carrier()) << "\n";
    f << "fx pitch " << (mixer.pitchShifter().enabled() ? 1 : 0) << " "
      << mixer.pitchShifter().semitones() << " " << mixer.pitchShifter().mix() << " "
      << mixer.pitchShifter().feedback() << "\n";
    f << "fx freqshift " << (mixer.freqShifter().enabled() ? 1 : 0) << " "
      << mixer.freqShifter().shiftHz() << " " << mixer.freqShifter().mix() << "\n";

    // Aux send/return buses: send level + the return effect's params.
    f << "send reverb " << mixer.reverbSend() << " " << mixer.reverbReturn().roomSize() << " "
      << mixer.reverbReturn().damping() << "\n";
    f << "send delay " << mixer.delaySend() << " " << mixer.delayReturn().time() << " "
      << mixer.delayReturn().feedback() << "\n";

    // Per-bus mixer-track insert strips (0 = drums, 1 = lead, 2 = bass). The `output` routing index is
    // appended after the shared fields (guarded on read → old files load with output -1 = master).
    for (int t = 0; t < Mixer::trackCount(); ++t) {
        MixerTrack& tr = mixer.track(t);
        f << "track " << t << " ";
        writeMixerTrackFields(f, tr);
        f << " " << tr.output() << "\n";
    }
    // Mixer group (submix) tracks — a count line then one full insert strip per group (same format).
    if (mixer.groupCount() > 0) {
        f << "groupcount " << mixer.groupCount() << "\n";
        for (int g = 0; g < mixer.groupCount(); ++g) {
            f << "group " << g << " ";
            writeMixerTrackFields(f, mixer.group(g));
            f << " " << mixer.group(g).output() << "\n"; // group's own routing (master / higher group)
        }
    }

    f << "plugin " << (mixer.plugin().enabled() ? 1 : 0) << " " << mixer.plugin().path() << "\n";
    if (!seq.leadPluginPath().empty()) {
        f << "leadplugin " << seq.leadPluginPath() << "\n"; // hosted CLAP instrument on the lead lane
    }

    for (int i = 0; i < Automation::count(); ++i) {
        const AutoLane& lane = automation.lane(i);
        f << "auto " << i << " " << (lane.enabled ? 1 : 0) << " "
          << static_cast<int>(lane.lfo.shape) << " " << lane.lfo.rateHz << " " << lane.lo << " "
          << lane.hi << " " << (lane.sync ? 1 : 0) << " " << lane.syncDiv << " "
          << (lane.lfo.sampleHold ? 1 : 0) << " " << lane.lfo.phase << "\n";
        // Automation clip (breakpoints): only written when the lane has one.
        if (!lane.clip.empty()) {
            f << "autoclip " << i << " " << lane.clipLength << " " << lane.clip.size();
            for (const AutoPoint& p : lane.clip) {
                f << " " << p.time << " " << p.value;
            }
            // Per-point curve tension appended after all (time,value) pairs so old readers (which stop
            // after `size` pairs) ignore it and old files (which lack it) read back as tension 0.
            for (const AutoPoint& p : lane.clip) {
                f << " " << p.tension;
            }
            f << "\n";
        }
    }

}

bool saveProject(const std::string& path, Sequencer& seq, Mixer& mixer, Automation& automation,
                 std::string* err) {
    std::ofstream f(path);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for writing";
        }
        return false;
    }
    writeProjectTo(f, seq, mixer, automation);
    if (!f) {
        if (err != nullptr) {
            *err = "write to '" + path + "' failed";
        }
        return false;
    }
    return true;
}

std::string saveProjectToString(Sequencer& seq, Mixer& mixer, Automation& automation) {
    std::ostringstream ss;
    writeProjectTo(ss, seq, mixer, automation);
    return ss.str();
}

// Parse a whole project from any input stream (shared by the file and string loaders).
static bool readProjectFrom(std::istream& f, Sequencer& seq, Mixer& mixer, Automation& automation,
                            std::string* err) {

    // Reset the destination to a clean slate so the file fully defines the project.
    seq.clearArrangement();
    mixer.clearGroups(); // drop any stale submix groups so an older (group-less) snapshot restores cleanly

    std::string line;
    bool sawHeader = false;
    int curInst = -1; // currently-selected extra instrument channel (for synthI/synthoscI lines)
    while (std::getline(f, line)) {
        std::istringstream ls(line);
        std::string tag;
        if (!(ls >> tag) || tag.empty() || tag[0] == '#') {
            continue;
        }
        if (tag == "cjc") {
            sawHeader = true;
        } else if (tag == "bpm") {
            double bpm = 120.0;
            ls >> bpm;
            seq.setBpm(bpm);
        } else if (tag == "steps") {
            int st = 16;
            ls >> st;
            seq.setNumSteps(st); // resize before any step/pattern content is read
        } else if (tag == "spb") {
            int spb = 4;
            ls >> spb;
            seq.setStepsPerBeat(spb);
        } else if (tag == "swing") {
            float sw = 0.0f;
            ls >> sw;
            seq.setSwing(sw);
        } else if (tag == "sidechain") {
            int on = 0;
            float amount = 0.7f, rel = 200.0f;
            ls >> on >> amount >> rel;
            int src = 0;      // sidechain source channel optional (older files omit it)
            float atk = 0.0f; // attack ms optional (older files omit it → instant snap)
            ls >> src;        // fails on old files → src stays 0 (kick)
            ls >> atk;        // fails on old files → atk stays 0 (instant)
            seq.setSidechain(on != 0, amount, rel, atk);
            seq.setSidechainSource(src);
        } else if (tag == "arp") {
            int on = 0, mode = 0;
            ls >> on >> mode;
            seq.setArp(on != 0, mode);
            int oct = 1; // octave range optional (older files omit it)
            if (ls >> oct) {
                seq.setArpOctaves(oct);
            }
            float gate = 1.0f; // gate length optional (older files omit it)
            if (ls >> gate) {
                seq.setArpGate(gate);
            }
            int rate = 1; // arp rate optional (older files omit it → one note per step)
            if (ls >> rate) {
                seq.setArpRate(rate);
            }
        } else if (tag == "humanize") {
            float h = 0.0f;
            ls >> h;
            seq.setHumanize(h);
        } else if (tag == "metronome") {
            int m = 0;
            ls >> m;
            seq.setMetronome(m != 0);
            float mlvl = 0.5f; // metronome level optional (older files omit it → 0.5)
            if (ls >> mlvl) {
                seq.setMetronomeLevel(mlvl);
            }
        } else if (tag == "countin") {
            int b = 0;
            ls >> b;
            seq.setCountInBars(b);
        } else if (tag == "ccmap") {
            int cc = -1;
            int tgt = 0;
            if (ls >> cc >> tgt) {
                // Clamp the target index to the known enum range so a newer file's target can't index
                // out of range on an older build (widen this bound whenever CcTarget grows).
                if (tgt < 0 || tgt > 3) {
                    tgt = 0;
                }
                seq.mapMidiCc(cc, static_cast<Sequencer::CcTarget>(tgt));
            }
        } else if (tag == "busgain") {
            float d = 1.0f;
            float s = 1.0f;
            float bassG = 1.0f;
            ls >> d >> s;
            seq.setDrumGain(d);
            seq.setSynthGain(s);
            if (ls >> bassG) { // bass gain optional (older files omit it)
                seq.setBassGain(bassG);
            }
            float lp = 0.0f, bp = 0.0f; // lead/bass pan optional (older files omit them)
            if (ls >> lp >> bp) {
                seq.setLeadPan(lp);
                seq.setBassPan(bp);
            }
            int tr = 0; // transpose optional
            if (ls >> tr) {
                seq.setTranspose(tr);
            }
        } else if (tag == "synth") {
            parseSynthLine(ls, seq.synth());
        } else if (tag == "synth2") {
            parseSynthLine(ls, seq.synth2());
        } else if (tag == "synthosc") {
            parseOscLine(ls, seq.synth());
        } else if (tag == "synthosc2") {
            parseOscLine(ls, seq.synth2());
        } else if (tag == "instchannelcount") {
            int n = 0;
            ls >> n;
            while (seq.instrumentChannelCount() < n && seq.instrumentChannelCount() < 256) {
                seq.addInstrumentChannel();
            }
        } else if (tag == "instchannel") {
            ls >> curInst; // selector for the following synthI/synthoscI lines
            if (curInst >= 0 && curInst < seq.instrumentChannelCount()) {
                float g = 1.0f, p = 0.0f; // per-channel mix optional (older files omit → defaults)
                int b = 1;
                if (ls >> g) {
                    seq.setInstrumentGain(curInst, g);
                }
                if (ls >> p) {
                    seq.setInstrumentPan(curInst, p);
                }
                if (ls >> b) {
                    seq.setInstrumentBus(curInst, b);
                }
            }
        } else if (tag == "instplugin") {
            int ch = -1;
            ls >> ch;
            std::string pp;
            std::getline(ls, pp);
            const size_t nb = pp.find_first_not_of(' ');
            pp = (nb == std::string::npos) ? std::string() : pp.substr(nb);
            if (ch >= 0 && ch < seq.instrumentChannelCount() && !pp.empty()) {
                seq.loadInstrumentPlugin(ch, pp, 48000); // fails gracefully if missing
            }
        } else if (tag == "synthI") {
            if (curInst >= 0 && curInst < seq.instrumentChannelCount()) {
                parseSynthLine(ls, seq.instrumentSynth(curInst));
            }
        } else if (tag == "synthoscI") {
            if (curInst >= 0 && curInst < seq.instrumentChannelCount()) {
                parseOscLine(ls, seq.instrumentSynth(curInst));
            }
        } else if (tag == "noteN") {
            int c = -1, p = -1;
            Note n;
            ls >> c >> p >> n.startStep >> n.lengthSteps >> n.pitch >> n.velocity;
            float fine = 0.0f;
            if (ls >> fine) {
                n.fineTune = fine;
            }
            float prob = 1.0f; // per-note attributes optional (older/basic files omit them)
            if (ls >> prob) {
                n.probability = prob;
            }
            int roll = 1;
            if (ls >> roll) {
                n.roll = roll;
            }
            int slide = 0;
            if (ls >> slide) {
                n.slide = slide != 0;
            }
            int stride = 1;
            if (ls >> stride) {
                n.stride = stride < 1 ? 1 : (stride > 8 ? 8 : stride);
            }
            int nudge = 0;
            if (ls >> nudge) {
                n.nudge = nudge < 0 ? 0 : (nudge > 95 ? 95 : nudge);
            }
            float cutoff = 0.0f;
            if (ls >> cutoff) {
                n.cutoff = cutoff < -8.0f ? -8.0f : (cutoff > 8.0f ? 8.0f : cutoff);
            }
            if (c >= 0 && c < seq.instrumentChannelCount() && p >= 0 && p < seq.patternCount()) {
                seq.instrumentRoll(p, c).addNote(n);
            }
        } else if (tag == "sampler") {
            int use = 0;
            int base = 60;
            float g = 0.9f;
            ls >> use >> base >> g;
            std::string sp;
            std::getline(ls, sp);
            const size_t nb = sp.find_first_not_of(' ');
            sp = (nb == std::string::npos) ? std::string() : sp.substr(nb);
            seq.sampler().setBasePitch(base);
            seq.sampler().setGain(g);
            if (!sp.empty()) {
                std::string se;
                seq.sampler().load(sp, &se); // best-effort; a missing file just leaves it unloaded
            }
            seq.setUseSampler(use != 0);
        } else if (tag == "samplercfg") {
            int rev = 0, loop = 0;
            ls >> rev >> loop;
            seq.sampler().setReverse(rev != 0);
            seq.sampler().setLoop(loop != 0);
            float off = 0.0f; // start offset optional (older files omit it)
            if (ls >> off) {
                seq.sampler().setStartOffset(off);
            }
            float atk = 0.0f, rel = 0.0f; // amp env optional (older files omit it)
            if (ls >> atk >> rel) {
                seq.sampler().setAmpEnv(atk, rel);
            }
            int pingpong = 0; // ping-pong optional (older files omit it)
            if (ls >> pingpong) {
                seq.sampler().setPingPong(pingpong != 0);
            }
            float detune = 0.0f; // fine tune optional (older files omit it)
            if (ls >> detune) {
                seq.sampler().setDetuneCents(detune);
            }
            float loopS = 0.0f, loopE = 1.0f; // loop region optional (older files omit it)
            if (ls >> loopS >> loopE) {
                seq.sampler().setLoopRegion(loopS, loopE);
            }
            int slices = 1; // beat-slicer count optional (older files omit it)
            if (ls >> slices) {
                seq.sampler().setSlices(slices);
            }
            float fcut = 20000.0f, freso = 0.7f; // playback filter optional (older files omit it)
            if (ls >> fcut >> freso) {
                seq.sampler().setFilter(fcut, freso);
            }
            float fea = 0.005f, fed = 0.1f, fes = 0.0f, fer = 0.1f, fedep = 0.0f; // filter env optional
            if (ls >> fea >> fed >> fes >> fer >> fedep) {
                seq.sampler().setFilterEnvelope(fea, fed, fes, fer);
                seq.sampler().setFilterEnvDepth(fedep);
            }
            float pedep = 0.0f, petime = 0.05f; // pitch env optional (older files omit it → off)
            if (ls >> pedep >> petime) {
                seq.sampler().setPitchEnv(pedep, petime);
            }
            float fvelo = 0.0f; // velocity → cutoff optional (older files omit it → off)
            if (ls >> fvelo) {
                seq.sampler().setFilterVelo(fvelo);
            }
            int smono = 0; // sampler mono mode optional (older files omit it → polyphonic)
            if (ls >> smono) {
                seq.sampler().setMono(smono != 0);
            }
            float adec = 0.05f, asus = 1.0f; // amp decay/sustain optional (older files → sustain 1)
            if (ls >> adec >> asus) {
                seq.sampler().setAmpDecay(adec);
                seq.sampler().setAmpSustain(asus);
            }
            float vsens = 1.0f; // velocity→volume optional (older files → full sensitivity)
            if (ls >> vsens) {
                seq.sampler().setVelSensitivity(vsens);
            }
            int keytrack = 1; // key tracking optional (older files → on, a melodic sampler)
            if (ls >> keytrack) {
                seq.sampler().setKeyTrack(keytrack != 0);
            }
            float sdrive = 0.0f; // per-voice drive optional (older files → clean)
            if (ls >> sdrive) {
                seq.sampler().setDrive(sdrive);
            }
            float sglide = 0.0f; // portamento optional (older files → off)
            int sglideleg = 0;
            if (ls >> sglide >> sglideleg) {
                seq.sampler().setGlide(sglide);
                seq.sampler().setGlideLegato(sglideleg != 0);
            }
            float svelatk = 0.0f; // velocity → attack optional (older files → off)
            if (ls >> svelatk) {
                seq.sampler().setVelToAttack(svelatk);
            }
            float svelstart = 0.0f; // velocity → start optional (older files → off)
            if (ls >> svelstart) {
                seq.sampler().setVelToStart(svelstart);
            }
            float sfkt = 0.0f; // filter key-tracking optional (older files → off)
            if (ls >> sfkt) {
                seq.sampler().setFilterKeyTrack(sfkt);
            }
            int sfmode = 0; // filter mode optional (older files → LowPass)
            if (ls >> sfmode) {
                if (sfmode < 0 || sfmode > 3) {
                    sfmode = 0;
                }
                seq.sampler().setFilterMode(static_cast<StateVariableFilter::Mode>(sfmode));
            }
            float slfoRate = 5.0f, slfoDepth = 0.0f; // filter-LFO optional (older files → off)
            if (ls >> slfoRate >> slfoDepth) {
                seq.sampler().setFilterLfo(slfoRate, slfoDepth);
            }
            int slfoSync = 0, slfoDiv = 3; // filter-LFO tempo-sync optional (older files → off)
            if (ls >> slfoSync >> slfoDiv) {
                seq.sampler().setFilterLfoSync(slfoSync != 0);
                seq.sampler().setFilterLfoSyncDivision(slfoDiv);
            }
            int slfoShape = 0; // filter-LFO shape optional (older files → Sine)
            if (ls >> slfoShape) {
                if (slfoShape < 0 || slfoShape > 3) {
                    slfoShape = 0;
                }
                seq.sampler().setFilterLfoShape(static_cast<Sampler::LfoShape>(slfoShape));
            }
        } else if (tag == "chan") {
            int c = -1;
            float vol = 1.0f;
            int mute = 0;
            int solo = 0;
            float pan = 0.0f;
            ls >> c >> vol >> mute >> solo >> pan; // pan optional (older files omit it)
            seq.setChannelVolume(c, vol);
            seq.setChannelMute(c, mute != 0);
            seq.setChannelSolo(c, solo != 0);
            seq.setChannelPan(c, pan);
            int choke = 0; // choke group optional (older files omit it)
            if (ls >> choke) {
                seq.setChannelChokeGroup(c, choke);
            }
            float tune = 0.0f; // tuning optional (older files omit it)
            if (ls >> tune) {
                seq.setChannelTune(c, tune);
            }
            float decay = 1.0f; // decay optional (older files omit it)
            if (ls >> decay) {
                seq.setChannelDecay(c, decay);
            }
            float drive = 0.0f; // drive optional (older files omit it)
            if (ls >> drive) {
                seq.setChannelDrive(c, drive);
            }
            float flam = 0.0f; // flam optional (older files omit it)
            if (ls >> flam) {
                seq.setChannelFlam(c, flam);
            }
            int dtype = -1; // per-channel drum type optional (older files omit it)
            if (ls >> dtype && dtype >= 0 && dtype <= static_cast<int>(Drum::Guiro)) {
                seq.setChannelType(c, static_cast<Drum>(dtype));
            }
            float penv = 1.0f; // pitch-envelope depth optional (older files omit it → natural sweep)
            if (ls >> penv) {
                seq.setChannelPitchEnv(c, penv);
            }
            float tone = 20000.0f; // per-channel tone cutoff optional (older files omit it → open)
            if (ls >> tone) {
                seq.setChannelTone(c, tone);
            }
            float petime = 1.0f; // per-channel pitch-env time optional (older files omit it → natural)
            if (ls >> petime) {
                seq.setChannelPitchEnvTime(c, petime);
            }
            float snap = 0.5f; // per-channel snare snap optional (older files omit it → classic mix)
            if (ls >> snap) {
                seq.setChannelSnap(c, snap);
            }
            float hp = 0.0f; // per-channel high-pass optional (older files omit it → off)
            if (ls >> hp) {
                seq.setChannelHighpass(c, hp);
            }
        } else if (tag == "patterns") {
            int count = 1;
            ls >> count;
            while (seq.patternCount() < count) {
                seq.addPattern();
            }
        } else if (tag == "patname") {
            int p = 0;
            ls >> p;
            std::string nm;
            std::getline(ls, nm); // rest of the line is the name (may contain spaces)
            const size_t nb = nm.find_first_not_of(' ');
            nm = (nb == std::string::npos) ? std::string() : nm.substr(nb);
            if (!nm.empty()) {
                seq.setPatternName(p, nm);
            }
        } else if (tag == "patswing") {
            int p = 0;
            float sw = 0.0f;
            ls >> p >> sw;
            if (p >= 0 && p < seq.patternCount()) {
                seq.selectPattern(p);
                seq.setSwing(sw); // sets the selected pattern's swing
            }
        } else if (tag == "pattranspose") {
            int p = 0;
            int tr = 0;
            ls >> p >> tr;
            if (p >= 0 && p < seq.patternCount()) {
                seq.selectPattern(p);
                seq.setPatternTranspose(tr); // sets the selected pattern's transpose
            }
        } else if (tag == "pattempo") {
            int p = 0;
            float mul = 1.0f;
            ls >> p >> mul;
            if (p >= 0 && p < seq.patternCount()) {
                seq.selectPattern(p);
                seq.setPatternTempoMul(mul); // sets the selected pattern's tempo multiplier
            }
        } else if (tag == "songmode") {
            int on = 0;
            ls >> on;
            seq.setSongMode(on != 0);
            int loop = 1; // song loop optional (older files omit it → loop)
            if (ls >> loop) {
                seq.setSongLoop(loop != 0);
            }
            int lstart = 0, lend = 0; // loop region optional (older files omit it → whole playlist)
            if (ls >> lstart >> lend) {
                seq.setSongLoopRange(lstart, lend);
            }
            int useClips = 0; // clip-driven song mode optional (older files omit it → legacy playlist)
            if (ls >> useClips) {
                seq.setSongUsesClips(useClips != 0);
            }
        } else if (tag == "playlist") {
            int count = 0;
            ls >> count;
            std::vector<int> seqList;
            for (int k = 0; k < count; ++k) {
                int idx = 0;
                if (ls >> idx) {
                    seqList.push_back(idx);
                }
            }
            seq.setPlaylist(seqList);
        } else if (tag == "clip") {
            int pat = 0, bar = 0, trk = 0;
            ls >> pat >> bar >> trk;
            if (pat >= 0) {
                seq.addClip(pat, bar, trk);
            }
        } else if (tag == "step") {
            int p = 0;
            int c = 0;
            int s = 0;
            ls >> p >> c >> s;
            seq.selectPattern(p);
            int vel = 255;
            if (ls >> vel) {
                seq.setStepVelocity(c, s, static_cast<float>(vel) / 255.0f);
            } else {
                seq.setStep(c, s, true);
            }
            int prob = 255; // optional per-step probability (older files omit it → always)
            if (ls >> prob) {
                seq.setStepProbability(c, s, static_cast<float>(prob) / 255.0f);
            }
            int ratchet = 1; // optional per-step ratchet count (older files omit it → single hit)
            if (ls >> ratchet) {
                seq.setStepRatchet(c, s, ratchet);
            }
            int tune = 0; // optional per-step pitch offset (older files omit it → no offset)
            if (ls >> tune) {
                seq.setStepTune(c, s, tune);
            }
            int nudge = 0; // optional per-step timing nudge (older files omit it → on the grid)
            if (ls >> nudge) {
                seq.setStepNudge(c, s, nudge);
            }
            int stride = 1; // optional per-step trig condition (older files omit it → every loop)
            if (ls >> stride) {
                seq.setStepStride(c, s, stride);
            }
        } else if (tag == "note") {
            int p = 0;
            Note n;
            ls >> p >> n.startStep >> n.lengthSteps >> n.pitch >> n.velocity;
            float prob = 1.0f; // optional (older files omit it → keep the default 1.0)
            if (ls >> prob) {
                n.probability = prob;
            }
            float fine = 0.0f; // per-note fine tune optional (older files omit it)
            if (ls >> fine) {
                n.fineTune = fine;
            }
            int roll = 1; // per-note roll/ratchet optional (older files omit it → single hit)
            if (ls >> roll) {
                n.roll = roll;
            }
            int slide = 0; // per-note slide/portamento optional (older files omit it → off)
            if (ls >> slide) {
                n.slide = slide != 0;
            }
            int stride = 1; // per-note trig condition optional (older files omit it → every loop)
            if (ls >> stride) {
                n.stride = stride < 1 ? 1 : (stride > 8 ? 8 : stride);
            }
            int nudge = 0; // per-note micro-timing nudge optional (older files omit it → on the grid)
            if (ls >> nudge) {
                n.nudge = nudge < 0 ? 0 : (nudge > 95 ? 95 : nudge);
            }
            float cutoff = 0.0f; // per-note cutoff offset (FL "Mod X") optional (older files omit → 0)
            if (ls >> cutoff) {
                n.cutoff = cutoff < -8.0f ? -8.0f : (cutoff > 8.0f ? 8.0f : cutoff);
            }
            seq.selectPattern(p);
            seq.roll().addNote(n);
        } else if (tag == "note2") {
            int p = 0;
            Note n;
            ls >> p >> n.startStep >> n.lengthSteps >> n.pitch >> n.velocity;
            float prob = 1.0f;
            if (ls >> prob) {
                n.probability = prob;
            }
            float fine = 0.0f; // per-note fine tune optional (older files omit it)
            if (ls >> fine) {
                n.fineTune = fine;
            }
            int roll = 1; // per-note roll/ratchet optional (older files omit it → single hit)
            if (ls >> roll) {
                n.roll = roll;
            }
            int slide = 0; // per-note slide/portamento optional (older files omit it → off)
            if (ls >> slide) {
                n.slide = slide != 0;
            }
            int stride = 1; // per-note trig condition optional (older files omit it → every loop)
            if (ls >> stride) {
                n.stride = stride < 1 ? 1 : (stride > 8 ? 8 : stride);
            }
            int nudge = 0; // per-note micro-timing nudge optional (older files omit it → on the grid)
            if (ls >> nudge) {
                n.nudge = nudge < 0 ? 0 : (nudge > 95 ? 95 : nudge);
            }
            float cutoff = 0.0f; // per-note cutoff offset (FL "Mod X") optional (older files omit → 0)
            if (ls >> cutoff) {
                n.cutoff = cutoff < -8.0f ? -8.0f : (cutoff > 8.0f ? 8.0f : cutoff);
            }
            seq.selectPattern(p);
            seq.roll2().addNote(n);
        } else if (tag == "master") {
            float g = 0.9f;
            ls >> g;
            mixer.setMasterGain(g);
            float ceil = 1.0f; // limiter ceiling optional (older files omit it)
            if (ls >> ceil) {
                mixer.setLimiterCeiling(ceil);
            }
            float bal = 0.0f; // master balance optional (older files omit it)
            if (ls >> bal) {
                mixer.setMasterBalance(bal);
            }
        } else if (tag == "fx") {
            std::string which;
            int en = 0;
            ls >> which >> en;
            if (which == "eq") {
                float cutoff = 8000.0f;
                ls >> cutoff;
                mixer.eq().setEnabled(en != 0);
                mixer.eq().setCutoff(cutoff);
            } else if (which == "filter") {
                int mode = 0;
                float cutoff = 20000.0f, reso = 0.7f;
                ls >> mode >> cutoff >> reso;
                mixer.filter().setEnabled(en != 0);
                mixer.filter().setMode(static_cast<StateVariableFilter::Mode>(
                    mode < 0 || mode > 3 ? 0 : mode));
                mixer.filter().setCutoff(cutoff);
                mixer.filter().setResonance(reso);
                float fdrive = 0.0f; // filter drive optional (older files → clean)
                if (ls >> fdrive) {
                    mixer.filter().setDrive(fdrive);
                }
            } else if (which == "hp") {
                float cutoff = 30.0f;
                ls >> cutoff;
                mixer.highpass().setEnabled(en != 0);
                mixer.highpass().setCutoff(cutoff);
            } else if (which == "comp") {
                float thr = -18.0f, ratio = 4.0f, atk = 8.0f, rel = 120.0f, mk = 0.0f;
                ls >> thr >> ratio >> atk >> rel >> mk;
                mixer.compressor().setEnabled(en != 0);
                mixer.compressor().setThresholdDb(thr);
                mixer.compressor().setRatio(ratio);
                mixer.compressor().setAttackMs(atk);
                mixer.compressor().setReleaseMs(rel);
                mixer.compressor().setMakeupDb(mk);
                float knee = 0.0f; // knee optional (older files omit it)
                if (ls >> knee) {
                    mixer.compressor().setKneeDb(knee);
                }
                float mix = 1.0f; // parallel-compression mix optional (older files omit it)
                if (ls >> mix) {
                    mixer.compressor().setMix(mix);
                }
                float scHpf = 0.0f; // sidechain HPF optional (older files omit it → off)
                if (ls >> scHpf) {
                    mixer.compressor().setSidechainHpf(scHpf);
                }
                int autoMk = 0; // auto-makeup optional (older files omit it → off)
                if (ls >> autoMk) {
                    mixer.compressor().setAutoMakeup(autoMk != 0);
                }
                float lookahead = 0.0f; // lookahead optional (older files omit it → off)
                if (ls >> lookahead) {
                    mixer.compressor().setLookaheadMs(lookahead);
                }
                int rmsDet = 0; // RMS detection optional (older files omit it → peak)
                if (ls >> rmsDet) {
                    mixer.compressor().setRmsDetection(rmsDet != 0);
                }
                int slink = 1; // stereo link optional (older files omit it → linked)
                if (ls >> slink) {
                    mixer.compressor().setStereoLink(slink != 0);
                }
            } else if (which == "multiband") {
                float clo = 250.0f, chi = 2500.0f, t0 = -18.0f, r0 = 3.0f, t1 = -18.0f, r1 = 3.0f,
                      t2 = -18.0f, r2 = 3.0f, atk = 10.0f, rel = 120.0f;
                ls >> clo >> chi >> t0 >> r0 >> t1 >> r1 >> t2 >> r2 >> atk >> rel;
                mixer.multiband().setEnabled(en != 0);
                mixer.multiband().setCrossoverLow(clo);
                mixer.multiband().setCrossoverHigh(chi);
                mixer.multiband().setBandThreshold(0, t0);
                mixer.multiband().setBandRatio(0, r0);
                mixer.multiband().setBandThreshold(1, t1);
                mixer.multiband().setBandRatio(1, r1);
                mixer.multiband().setBandThreshold(2, t2);
                mixer.multiband().setBandRatio(2, r2);
                mixer.multiband().setAttackMs(atk);
                mixer.multiband().setReleaseMs(rel);
            } else if (which == "transient") {
                float atk = 0.0f, sus = 0.0f;
                ls >> atk >> sus;
                mixer.transient().setEnabled(en != 0);
                mixer.transient().setAttack(atk);
                mixer.transient().setSustain(sus);
            } else if (which == "mbtransient") {
                float clo = 200.0f, chi = 2000.0f, a0 = 0.0f, s0 = 0.0f, a1 = 0.0f, s1 = 0.0f,
                      a2 = 0.0f, s2 = 0.0f;
                ls >> clo >> chi >> a0 >> s0 >> a1 >> s1 >> a2 >> s2;
                auto& mt = mixer.multibandTransient();
                mt.setEnabled(en != 0);
                mt.setCrossoverLow(clo);
                mt.setCrossoverHigh(chi);
                mt.setAttack(0, a0);
                mt.setSustain(0, s0);
                mt.setAttack(1, a1);
                mt.setSustain(1, s1);
                mt.setAttack(2, a2);
                mt.setSustain(2, s2);
            } else if (which == "delay") {
                float t = 300.0f, fb = 0.35f, mix = 0.3f;
                ls >> t >> fb >> mix;
                mixer.delay().setEnabled(en != 0);
                mixer.delay().setTime(t);
                mixer.delay().setFeedback(fb);
                mixer.delay().setMix(mix);
                int pp = 0; // ping-pong flag optional for old files
                if (ls >> pp) {
                    mixer.delay().setPingPong(pp != 0);
                }
                float damp = 0.0f; // damping optional for old files
                if (ls >> damp) {
                    mixer.delay().setDamping(damp);
                }
                int sync = 0, div = 4; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    mixer.delay().setSync(sync != 0);
                    mixer.delay().setSyncDivision(div);
                }
                float fbLowCut = 0.0f; // feedback low-cut optional for old files
                if (ls >> fbLowCut) {
                    mixer.delay().setFeedbackLowCut(fbLowCut);
                }
                float modDepth = 0.0f, modRate = 0.3f; // delay modulation optional for old files
                if (ls >> modDepth >> modRate) {
                    mixer.delay().setModDepth(modDepth);
                    mixer.delay().setModRate(modRate);
                }
                float dduck = 0.0f; // delay ducking optional for old files (0 = off)
                if (ls >> dduck) {
                    mixer.delay().setDuck(dduck);
                }
                float dfbdrive = 0.0f; // feedback drive optional for old files (0 = clean)
                if (ls >> dfbdrive) {
                    mixer.delay().setFeedbackDrive(dfbdrive);
                }
            } else if (which == "gate") {
                float thr = -40.0f, ratio = 4.0f, range = -60.0f, atk = 2.0f, rel = 80.0f;
                ls >> thr >> ratio >> range >> atk >> rel;
                mixer.gate().setEnabled(en != 0);
                mixer.gate().setThresholdDb(thr);
                mixer.gate().setRatio(ratio);
                mixer.gate().setRangeDb(range);
                mixer.gate().setAttackMs(atk);
                mixer.gate().setReleaseMs(rel);
                float hold = 0.0f; // hold optional (older files omit it)
                if (ls >> hold) {
                    mixer.gate().setHoldMs(hold);
                }
                float scHpf = 0.0f; // sidechain HPF optional (older files omit it → off)
                if (ls >> scHpf) {
                    mixer.gate().setSidechainHpf(scHpf);
                }
            } else if (which == "width") {
                float w = 1.0f;
                ls >> w;
                mixer.widener().setEnabled(en != 0);
                mixer.widener().setWidth(w);
                float bm = 0.0f;
                if (ls >> bm) { mixer.widener().setBassMonoHz(bm); }
            } else if (which == "imager") {
                float clo = 250.0f, chi = 2500.0f, w0 = 0.5f, w1 = 1.0f, w2 = 1.5f;
                ls >> clo >> chi >> w0 >> w1 >> w2;
                mixer.imager().setEnabled(en != 0);
                mixer.imager().setCrossoverLow(clo);
                mixer.imager().setCrossoverHigh(chi);
                mixer.imager().setBandWidth(0, w0);
                mixer.imager().setBandWidth(1, w1);
                mixer.imager().setBandWidth(2, w2);
            } else if (which == "mbsat") {
                float clo = 200.0f, chi = 2000.0f, d0 = 0.0f, d1 = 0.0f, d2 = 0.0f;
                ls >> clo >> chi >> d0 >> d1 >> d2;
                mixer.multibandSaturator().setEnabled(en != 0);
                mixer.multibandSaturator().setCrossoverLow(clo);
                mixer.multibandSaturator().setCrossoverHigh(chi);
                mixer.multibandSaturator().setDrive(0, d0);
                mixer.multibandSaturator().setDrive(1, d1);
                mixer.multibandSaturator().setDrive(2, d2);
            } else if (which == "stereoenh") {
                float ms = 12.0f, amt = 0.7f;
                ls >> ms >> amt;
                mixer.stereoEnhancer().setEnabled(en != 0);
                mixer.stereoEnhancer().setDelayMs(ms);
                mixer.stereoEnhancer().setAmount(amt);
            } else if (which == "autopan") {
                float rate = 1.0f, depth = 0.5f;
                ls >> rate >> depth;
                mixer.autopan().setEnabled(en != 0);
                mixer.autopan().setRate(rate);
                mixer.autopan().setDepth(depth);
                int sync = 0, div = 2; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    mixer.autopan().setSync(sync != 0);
                    mixer.autopan().setSyncDivision(div);
                }
                int panShape = 0; // LFO shape optional (older files omit it → sine)
                if (ls >> panShape) {
                    mixer.autopan().setShape(static_cast<AutoPan::Shape>(
                        panShape < 0 || panShape > 3 ? 0 : panShape));
                }
            } else if (which == "monobass") {
                float x = 120.0f;
                ls >> x;
                mixer.monobass().setEnabled(en != 0);
                mixer.monobass().setCrossover(x);
            } else if (which == "subbass") {
                float amt = 0.0f, cut = 120.0f, tn = 220.0f;
                ls >> amt >> cut >> tn;
                mixer.subbass().setEnabled(en != 0);
                mixer.subbass().setAmount(amt);
                mixer.subbass().setCutoff(cut);
                mixer.subbass().setTone(tn);
            } else if (which == "octaver") {
                float amt = 0.0f, tn = 4000.0f;
                ls >> amt >> tn;
                mixer.octaver().setEnabled(en != 0);
                mixer.octaver().setAmount(amt);
                mixer.octaver().setTone(tn);
            } else if (which == "beatrepeat") {
                float slice = 125.0f, mix = 1.0f;
                int reps = 1;
                ls >> slice >> reps >> mix;
                mixer.beatRepeat().setEnabled(en != 0);
                mixer.beatRepeat().setSliceMs(slice);
                mixer.beatRepeat().setRepeats(reps);
                mixer.beatRepeat().setMix(mix);
                int brsync = 0, brdiv = 3; // tempo-sync fields optional (old files omit them)
                if (ls >> brsync) {
                    mixer.beatRepeat().setSync(brsync != 0);
                }
                if (ls >> brdiv) {
                    mixer.beatRepeat().setSyncDivision(brdiv);
                }
            } else if (which == "utility") {
                float gainDb = 0.0f;
                int invL = 0, invR = 0, mono = 0;
                ls >> gainDb >> invL >> invR >> mono;
                mixer.utility().setEnabled(en != 0);
                mixer.utility().setGainDb(gainDb);
                mixer.utility().setInvertL(invL != 0);
                mixer.utility().setInvertR(invR != 0);
                mixer.utility().setMono(mono != 0);
                float uwidth = 1.0f; // output width optional (older files omit it → unchanged)
                if (ls >> uwidth) {
                    mixer.utility().setWidth(uwidth);
                }
            } else if (which == "limiter") {
                float inGain = 0.0f, ceil = -0.3f, rel = 100.0f, look = 2.0f;
                ls >> inGain >> ceil >> rel >> look;
                mixer.limiter().setEnabled(en != 0);
                mixer.limiter().setInputGainDb(inGain);
                mixer.limiter().setCeilingDb(ceil);
                mixer.limiter().setReleaseMs(rel);
                mixer.limiter().setLookaheadMs(look);
            } else if (which == "leveler") {
                float tgt = -12.0f, resp = 800.0f, maxg = 12.0f;
                ls >> tgt >> resp >> maxg;
                mixer.leveler().setEnabled(en != 0);
                mixer.leveler().setTargetDb(tgt);
                mixer.leveler().setResponseMs(resp);
                mixer.leveler().setMaxGainDb(maxg);
            } else if (which == "clipper") {
                float drive = 0.0f, ceil = 0.9f, hard = 1.0f;
                ls >> drive >> ceil >> hard;
                mixer.clipper().setEnabled(en != 0);
                mixer.clipper().setDriveDb(drive);
                mixer.clipper().setCeiling(ceil);
                mixer.clipper().setHardness(hard);
                float cmix = 1.0f; // parallel mix optional (older files → fully clipped)
                if (ls >> cmix) {
                    mixer.clipper().setMix(cmix);
                }
            } else if (which == "deesser") {
                float thr = -24.0f, freq = 6000.0f, amt = 0.8f, rel = 60.0f;
                ls >> thr >> freq >> amt >> rel;
                mixer.deEsser().setEnabled(en != 0);
                mixer.deEsser().setThresholdDb(thr);
                mixer.deEsser().setFrequency(freq);
                mixer.deEsser().setAmount(amt);
                mixer.deEsser().setReleaseMs(rel);
            } else if (which == "dyneq") {
                float freq = 3000.0f, q = 2.0f, thr = -24.0f, range = -6.0f, atk = 5.0f, rel = 80.0f;
                ls >> freq >> q >> thr >> range >> atk >> rel;
                mixer.dynamicEq().setEnabled(en != 0);
                mixer.dynamicEq().setFrequency(freq);
                mixer.dynamicEq().setQ(q);
                mixer.dynamicEq().setThresholdDb(thr);
                mixer.dynamicEq().setRangeDb(range);
                mixer.dynamicEq().setAttackMs(atk);
                mixer.dynamicEq().setReleaseMs(rel);
            } else if (which == "comb") {
                float freq = 220.0f, fb = 0.8f, mix = 0.5f;
                ls >> freq >> fb >> mix;
                mixer.comb().setEnabled(en != 0);
                mixer.comb().setFrequency(freq);
                mixer.comb().setFeedback(fb);
                mixer.comb().setMix(mix);
                float cdamp = 0.0f; // damping optional for old files (0 = off/bright)
                if (ls >> cdamp) {
                    mixer.comb().setDamping(cdamp);
                }
            } else if (which == "chordres") {
                int root = 57, chord = 1;
                float fb = 0.9f, damp = 0.3f, mix = 0.5f;
                ls >> root >> chord >> fb >> damp >> mix;
                if (chord < 0 || chord > static_cast<int>(ChordResonator::Chord::Octaves)) {
                    chord = 0;
                }
                auto& cr = mixer.chordResonator();
                cr.setEnabled(en != 0);
                cr.setRootNote(root);
                cr.setChord(static_cast<ChordResonator::Chord>(chord));
                cr.setFeedback(fb);
                cr.setDamping(damp);
                cr.setMix(mix);
            } else if (which == "stepgate") {
                float rate = 2.0f, mix = 1.0f;
                int sync = 0, div = 2;
                ls >> rate >> sync >> div >> mix;
                mixer.stepGate().setEnabled(en != 0);
                mixer.stepGate().setRate(rate);
                mixer.stepGate().setSync(sync != 0);
                mixer.stepGate().setSyncDivision(div);
                mixer.stepGate().setMix(mix);
                for (int s = 0; s < StepGate::kSteps; ++s) {
                    float lvl = 1.0f;
                    if (ls >> lvl) {
                        mixer.stepGate().setStep(s, lvl);
                    }
                }
            } else if (which == "tremolo") {
                float rate = 5.0f, depth = 0.5f;
                int shape = 0;
                ls >> rate >> depth >> shape;
                mixer.tremolo().setEnabled(en != 0);
                mixer.tremolo().setRate(rate);
                mixer.tremolo().setDepth(depth);
                mixer.tremolo().setShape(
                    static_cast<Tremolo::Shape>(shape < 0 || shape > 3 ? 0 : shape));
                int sync = 0, div = 3; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    mixer.tremolo().setSync(sync != 0);
                    mixer.tremolo().setSyncDivision(div);
                }
            } else if (which == "stereodelay") {
                float lms = 250.0f, rms = 375.0f, fb = 0.4f, mix = 0.3f;
                ls >> lms >> rms >> fb >> mix;
                mixer.stereoDelay().setEnabled(en != 0);
                mixer.stereoDelay().setLeftMs(lms);
                mixer.stereoDelay().setRightMs(rms);
                mixer.stereoDelay().setFeedback(fb);
                mixer.stereoDelay().setMix(mix);
                int sync = 0, ldiv = 4, rdiv = 5; // tempo sync optional for old files
                if (ls >> sync >> ldiv >> rdiv) {
                    mixer.stereoDelay().setSync(sync != 0);
                    mixer.stereoDelay().setLeftDivision(ldiv);
                    mixer.stereoDelay().setRightDivision(rdiv);
                }
                float damp = 0.0f, fbLowCut = 0.0f; // feedback tone optional for old files
                if (ls >> damp) {
                    mixer.stereoDelay().setDamping(damp);
                }
                if (ls >> fbLowCut) {
                    mixer.stereoDelay().setFeedbackLowCut(fbLowCut);
                }
                int ping = 0; // ping-pong cross-feedback optional for old files
                if (ls >> ping) {
                    mixer.stereoDelay().setPingPong(ping != 0);
                }
            } else if (which == "revdelay") {
                float tms = 300.0f, fb = 0.3f, mix = 0.35f;
                ls >> tms >> fb >> mix;
                mixer.reverseDelay().setEnabled(en != 0);
                mixer.reverseDelay().setTimeMs(tms);
                mixer.reverseDelay().setFeedback(fb);
                mixer.reverseDelay().setMix(mix);
            } else if (which == "multitap") {
                float tms = 250.0f, decay = 0.6f, mix = 0.35f, spread = 0.8f;
                int taps = 3;
                ls >> tms >> taps >> decay >> mix >> spread;
                auto& mt = mixer.multiTapDelay();
                mt.setEnabled(en != 0);
                mt.setTimeMs(tms);
                mt.setTaps(taps);
                mt.setDecay(decay);
                mt.setMix(mix);
                mt.setSpread(spread);
                float mtfb = 0.0f; // multi-tap feedback optional for old files (0 = finite burst)
                if (ls >> mtfb) {
                    mt.setFeedback(mtfb);
                }
            } else if (which == "formant") {
                int vowel = 0;
                float mix = 0.5f;
                ls >> vowel >> mix;
                if (vowel < 0 || vowel > 4) {
                    vowel = 0;
                }
                mixer.formant().setEnabled(en != 0);
                mixer.formant().setVowel(static_cast<FormantFilter::Vowel>(vowel));
                mixer.formant().setMix(mix);
                int morphEn = 0;
                float morphPos = 0.0f; // vowel morph optional for old files (off)
                if (ls >> morphEn >> morphPos) {
                    mixer.formant().setMorphEnabled(morphEn != 0);
                    mixer.formant().setMorph(morphPos);
                }
                float fshift = 0.0f; // formant shift optional for old files (0 = natural)
                if (ls >> fshift) {
                    mixer.formant().setFormantShift(fshift);
                }
            } else if (which == "vocoder") {
                int carrier = 0;
                float chz = 110.0f, rel = 40.0f, mix = 1.0f;
                ls >> carrier >> chz >> rel >> mix;
                if (carrier < 0 || carrier > static_cast<int>(Vocoder::Carrier::Noise)) {
                    carrier = 0;
                }
                mixer.vocoder().setEnabled(en != 0);
                mixer.vocoder().setCarrier(static_cast<Vocoder::Carrier>(carrier));
                mixer.vocoder().setCarrierHz(chz);
                mixer.vocoder().setReleaseMs(rel);
                mixer.vocoder().setMix(mix);
            } else if (which == "autowah") {
                float base = 300.0f, range = 3000.0f, sens = 0.7f, reso = 4.0f, atk = 5.0f,
                      rel = 80.0f;
                ls >> base >> range >> sens >> reso >> atk >> rel;
                mixer.autowah().setEnabled(en != 0);
                mixer.autowah().setBaseHz(base);
                mixer.autowah().setRangeHz(range);
                mixer.autowah().setSensitivity(sens);
                mixer.autowah().setResonance(reso);
                mixer.autowah().setAttackMs(atk);
                mixer.autowah().setReleaseMs(rel);
                int down = 0; // direction optional for old files (0 = upward)
                if (ls >> down) {
                    mixer.autowah().setDownward(down != 0);
                }
                float wahMix = 1.0f; // dry/wet optional for old files (fully wet)
                if (ls >> wahMix) {
                    mixer.autowah().setMix(wahMix);
                }
            } else if (which == "tape") {
                float drive = 2.0f, warmth = 0.3f, mix = 1.0f;
                ls >> drive >> warmth >> mix;
                mixer.tape().setEnabled(en != 0);
                mixer.tape().setDrive(drive);
                mixer.tape().setWarmth(warmth);
                mixer.tape().setMix(mix);
                float wf = 0.0f; // wow/flutter optional for old files
                if (ls >> wf) {
                    mixer.tape().setWowFlutter(wf);
                }
            } else if (which == "ringmod") {
                float freq = 200.0f, mix = 1.0f;
                ls >> freq >> mix;
                mixer.ringmod().setEnabled(en != 0);
                mixer.ringmod().setFreq(freq);
                mixer.ringmod().setMix(mix);
                int carrier = 0; // carrier waveform optional for old files (0 = sine)
                if (ls >> carrier) {
                    mixer.ringmod().setCarrier(static_cast<RingMod::Carrier>(
                        carrier < 0 || carrier > 3 ? 0 : carrier));
                }
            } else if (which == "pitch") {
                float semis = 0.0f, mix = 1.0f;
                ls >> semis >> mix;
                mixer.pitchShifter().setEnabled(en != 0);
                mixer.pitchShifter().setSemitones(semis);
                mixer.pitchShifter().setMix(mix);
                float pfb = 0.0f; // pitch feedback optional (older files → off)
                if (ls >> pfb) {
                    mixer.pitchShifter().setFeedback(pfb);
                }
            } else if (which == "freqshift") {
                float hz = 0.0f, mix = 1.0f;
                ls >> hz >> mix;
                mixer.freqShifter().setEnabled(en != 0);
                mixer.freqShifter().setShiftHz(hz);
                mixer.freqShifter().setMix(mix);
            } else if (which == "reverb") {
                float room = 0.7f, damp = 0.35f, mix = 0.25f;
                ls >> room >> damp >> mix;
                mixer.reverb().setEnabled(en != 0);
                mixer.reverb().setRoomSize(room);
                mixer.reverb().setDamping(damp);
                mixer.reverb().setMix(mix);
                float pre = 0.0f; // pre-delay optional for old files
                if (ls >> pre) {
                    mixer.reverb().setPreDelayMs(pre);
                }
                float width = 1.0f; // width optional for old files
                if (ls >> width) {
                    mixer.reverb().setWidth(width);
                }
                int freeze = 0; // freeze optional for old files
                if (ls >> freeze) {
                    mixer.reverb().setFreeze(freeze != 0);
                }
                float duck = 0.0f; // ducking optional for old files
                if (ls >> duck) {
                    mixer.reverb().setDuck(duck);
                }
                float lowCut = 0.0f, highCut = 20000.0f; // wet tone optional for old files
                if (ls >> lowCut) {
                    mixer.reverb().setWetLowCut(lowCut);
                }
                if (ls >> highCut) {
                    mixer.reverb().setWetHighCut(highCut);
                }
                float gate = 0.0f; // gated-reverb time optional for old files (0 = off)
                if (ls >> gate) {
                    mixer.reverb().setGateMs(gate);
                }
                float shimmer = 0.0f; // octave-up shimmer optional for old files (0 = off)
                if (ls >> shimmer) {
                    mixer.reverb().setShimmer(shimmer);
                }
                float modDepth = 0.0f, modRate = 0.5f; // tail modulation optional for old files (0=off)
                if (ls >> modDepth >> modRate) {
                    mixer.reverb().setModDepth(modDepth);
                    mixer.reverb().setModRate(modRate);
                }
            } else if (which == "convolver") {
                float decay = 0.2f, tone = 6000.0f, mix = 0.3f;
                ls >> decay >> tone >> mix;
                mixer.convolver().setEnabled(en != 0);
                mixer.convolver().setDecay(decay);
                mixer.convolver().setTone(tone);
                mixer.convolver().setMix(mix);
            } else if (which == "dist") {
                float drive = 2.0f, mix = 0.5f;
                ls >> drive >> mix;
                mixer.distortion().setEnabled(en != 0);
                mixer.distortion().setDrive(drive);
                mixer.distortion().setMix(mix);
                int curve = 0; // curve type optional (older files omit it)
                if (ls >> curve) {
                    mixer.distortion().setCurve(static_cast<Distortion::Curve>(
                        curve < 0 || curve > 4 ? 0 : curve));
                }
                float tone = 20000.0f; // post tone optional (older files omit it → open)
                if (ls >> tone) {
                    mixer.distortion().setTone(tone);
                }
                float outDb = 0.0f; // output trim optional (older files omit it → unity)
                if (ls >> outDb) {
                    mixer.distortion().setOutputDb(outDb);
                }
                float bias = 0.0f; // asymmetry optional (older files omit it → symmetric)
                if (ls >> bias) {
                    mixer.distortion().setBias(bias);
                }
            } else if (which == "ampcab") {
                float drive = 0.5f, presence = 0.4f, tone = 5000.0f, mix = 1.0f;
                ls >> drive >> presence >> tone >> mix;
                mixer.ampCab().setEnabled(en != 0);
                mixer.ampCab().setDrive(drive);
                mixer.ampCab().setPresence(presence);
                mixer.ampCab().setTone(tone);
                mixer.ampCab().setMix(mix);
            } else if (which == "chorus") {
                float rate = 0.8f, depth = 3.0f, mix = 0.4f;
                ls >> rate >> depth >> mix;
                mixer.chorus().setEnabled(en != 0);
                mixer.chorus().setRate(rate);
                mixer.chorus().setDepth(depth);
                mixer.chorus().setMix(mix);
                int sync = 0, div = 0; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    mixer.chorus().setSync(sync != 0);
                    mixer.chorus().setSyncDivision(div);
                }
                float fb = 0.0f; // feedback optional for old files
                if (ls >> fb) {
                    mixer.chorus().setFeedback(fb);
                }
                float cwidth = 1.0f; // stereo width optional for old files (1 = natural)
                if (ls >> cwidth) {
                    mixer.chorus().setWidth(cwidth);
                }
                int cvoices = 1; // ensemble voices optional for old files (1 = single-voice)
                if (ls >> cvoices) {
                    mixer.chorus().setVoices(cvoices);
                }
            } else if (which == "vibrato") {
                float rate = 5.0f, depth = 4.0f;
                ls >> rate >> depth;
                mixer.vibrato().setEnabled(en != 0);
                mixer.vibrato().setRate(rate);
                mixer.vibrato().setDepth(depth);
                int sync = 0, div = 3; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    mixer.vibrato().setSync(sync != 0);
                    mixer.vibrato().setSyncDivision(div);
                }
                int vshape = 0; // pitch-LFO shape optional for old files (0 = Sine)
                if (ls >> vshape) {
                    mixer.vibrato().setShape(static_cast<Vibrato::Shape>(
                        vshape < 0 || vshape > 3 ? 0 : vshape));
                }
            } else if (which == "rotary") {
                float rate = 6.0f, depth = 0.5f, mix = 1.0f;
                ls >> rate >> depth >> mix;
                mixer.rotary().setEnabled(en != 0);
                mixer.rotary().setRate(rate);
                mixer.rotary().setDepth(depth);
                mixer.rotary().setMix(mix);
                float rdrive = 0.0f; // preamp drive optional (older files → clean)
                if (ls >> rdrive) {
                    mixer.rotary().setDrive(rdrive);
                }
            } else if (which == "flanger") {
                float rate = 0.3f, depth = 2.0f, fb = 0.5f, mix = 0.5f;
                ls >> rate >> depth >> fb >> mix;
                mixer.flanger().setEnabled(en != 0);
                mixer.flanger().setRate(rate);
                mixer.flanger().setDepth(depth);
                mixer.flanger().setFeedback(fb);
                mixer.flanger().setMix(mix);
                int sync = 0, div = 1; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    mixer.flanger().setSync(sync != 0);
                    mixer.flanger().setSyncDivision(div);
                }
                int inv = 0; // invert optional for old files (0 = normal)
                if (ls >> inv) {
                    mixer.flanger().setInvert(inv != 0);
                }
            } else if (which == "crush") {
                float bits = 8.0f, ds = 4.0f, mix = 0.5f;
                ls >> bits >> ds >> mix;
                mixer.bitcrusher().setEnabled(en != 0);
                mixer.bitcrusher().setBits(bits);
                mixer.bitcrusher().setDownsample(ds);
                mixer.bitcrusher().setMix(mix);
                float ctone = 20000.0f; // post tone optional (older files omit it → open)
                if (ls >> ctone) {
                    mixer.bitcrusher().setTone(ctone);
                }
            } else if (which == "phaser") {
                float rate = 0.5f, depth = 0.7f, fb = 0.3f, mix = 0.5f;
                ls >> rate >> depth >> fb >> mix;
                mixer.phaser().setEnabled(en != 0);
                mixer.phaser().setRate(rate);
                mixer.phaser().setDepth(depth);
                mixer.phaser().setFeedback(fb);
                mixer.phaser().setMix(mix);
                int sync = 0, div = 0; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    mixer.phaser().setSync(sync != 0);
                    mixer.phaser().setSyncDivision(div);
                }
                int stages = 4; // stage count optional for old files
                if (ls >> stages) {
                    mixer.phaser().setStages(stages);
                }
                int pstereo = 0; // stereo mode optional for old files (0 = mono)
                if (ls >> pstereo) {
                    mixer.phaser().setStereo(pstereo != 0);
                }
            } else if (which == "peq") {
                float lowDb = 0.0f, midF = 1000.0f, midQ = 1.0f, midDb = 0.0f, highDb = 0.0f;
                ls >> lowDb >> midF >> midQ >> midDb >> highDb;
                mixer.peq().setEnabled(en != 0);
                mixer.peq().setLowGain(lowDb);
                mixer.peq().setMid(midF, midQ, midDb);
                mixer.peq().setHighGain(highDb);
                float mid2F = 3500.0f, mid2Q = 1.0f, mid2Db = 0.0f; // 2nd mid optional (old files omit)
                if (ls >> mid2F >> mid2Q >> mid2Db) {
                    mixer.peq().setMid2(mid2F, mid2Q, mid2Db);
                }
                float mid3F = 7000.0f, mid3Q = 1.0f, mid3Db = 0.0f; // 3rd mid optional (old files omit)
                if (ls >> mid3F >> mid3Q >> mid3Db) {
                    mixer.peq().setMid3(mid3F, mid3Q, mid3Db);
                }
            } else if (which == "tilt") {
                float t = 0.0f;
                ls >> t;
                mixer.tilt().setEnabled(en != 0);
                mixer.tilt().setTilt(t);
                float pivot = 650.0f; // pivot optional for old files (default 650 Hz)
                if (ls >> pivot) {
                    mixer.tilt().setPivot(pivot);
                }
            } else if (which == "exciter") {
                float xover = 4000.0f, amt = 0.3f;
                ls >> xover >> amt;
                mixer.exciter().setEnabled(en != 0);
                mixer.exciter().setCrossover(xover);
                mixer.exciter().setAmount(amt);
                int even = 0; // even-harmonic mode optional (older files → odd/tanh)
                if (ls >> even) {
                    mixer.exciter().setEvenHarmonics(even != 0);
                }
            }
        } else if (tag == "send") {
            std::string which;
            ls >> which;
            if (which == "reverb") {
                float lvl = 0.0f, room = 0.7f, damp = 0.35f;
                ls >> lvl >> room >> damp;
                mixer.setReverbSend(lvl);
                mixer.reverbReturn().setRoomSize(room);
                mixer.reverbReturn().setDamping(damp);
            } else if (which == "delay") {
                float lvl = 0.0f, t = 300.0f, fb = 0.35f;
                ls >> lvl >> t >> fb;
                mixer.setDelaySend(lvl);
                mixer.delayReturn().setTime(t);
                mixer.delayReturn().setFeedback(fb);
            }
        } else if (tag == "track") {
            int t = -1;
            ls >> t;
            if (t >= 0 && t < Mixer::trackCount()) {
                MixerTrack& tr = mixer.track(t);
                readMixerTrackFields(ls, tr);
                int outIdx = -1; // output routing optional (older files omit it → master)
                if (ls >> outIdx) {
                    tr.setOutput(outIdx);
                }
            }
        } else if (tag == "groupcount") {
            int n = 0;
            ls >> n;
            mixer.clearGroups();
            for (int g = 0; g < n && g < 256; ++g) {
                mixer.addGroup();
            }
        } else if (tag == "group") {
            int g = -1;
            ls >> g;
            if (g >= 0 && g < mixer.groupCount()) {
                readMixerTrackFields(ls, mixer.group(g));
                int gOut = -1; // group routing optional (older files omit it → master)
                if (ls >> gOut) {
                    mixer.group(g).setOutput(gOut);
                }
            }
        } else if (tag == "plugin") {
            int en = 0;
            ls >> en;
            std::string pp;
            std::getline(ls, pp);
            const size_t nb = pp.find_first_not_of(' ');
            pp = (nb == std::string::npos) ? std::string() : pp.substr(nb);
            if (!pp.empty()) {
                std::string pe;
                if (mixer.plugin().load(pp, 48000, &pe)) {
                    mixer.plugin().setEnabled(en != 0);
                }
            }
        } else if (tag == "leadplugin") {
            std::string pp;
            std::getline(ls, pp);
            const size_t nb = pp.find_first_not_of(' ');
            pp = (nb == std::string::npos) ? std::string() : pp.substr(nb);
            if (!pp.empty()) {
                seq.loadLeadPlugin(pp, 48000); // fails gracefully if the plugin is missing
            }
        } else if (tag == "auto") {
            int idx = -1, en = 0, shape = 0;
            float rate = 0.5f, lo = 0.0f, hi = 1.0f;
            ls >> idx >> en >> shape >> rate >> lo >> hi;
            if (idx >= 0 && idx < Automation::count()) {
                AutoLane& lane = automation.lane(idx);
                lane.enabled = en != 0;
                lane.lfo.shape = static_cast<Waveform>(shape < 0 || shape > 6 ? 0 : shape);
                lane.lfo.rateHz = rate;
                lane.lo = lo;
                lane.hi = hi;
                int sync = 0, div = 2; // tempo sync optional for old files
                if (ls >> sync >> div) {
                    lane.sync = sync != 0;
                    lane.syncDiv = div;
                }
                int sh = 0; // sample & hold optional for old files (0 = periodic shape)
                if (ls >> sh) {
                    lane.lfo.sampleHold = sh != 0;
                }
                float ph = 0.0f; // LFO phase offset optional for old files (0 = no offset)
                if (ls >> ph) {
                    lane.lfo.phase = ph;
                }
            }
        } else if (tag == "autoclip") {
            int idx = -1, n = 0;
            double len = 0.0;
            ls >> idx >> len >> n;
            if (idx >= 0 && idx < Automation::count() && n >= 0) {
                AutoLane& lane = automation.lane(idx);
                lane.clipLength = len;
                lane.clip.clear();
                for (int k = 0; k < n; ++k) {
                    AutoPoint p;
                    if (ls >> p.time >> p.value) {
                        lane.clip.push_back(p);
                    }
                }
                // Optional trailing per-point tensions (older files omit them → linear segments).
                for (size_t k = 0; k < lane.clip.size(); ++k) {
                    float tension = 0.0f;
                    if (ls >> tension) {
                        lane.clip[k].tension =
                            tension < -1.0f ? -1.0f : (tension > 1.0f ? 1.0f : tension);
                    }
                }
            }
        }
        // Unknown tags are ignored for forward compatibility.
    }

    seq.selectPattern(0); // leave the first pattern selected after a load

    if (!sawHeader) {
        if (err != nullptr) {
            *err = "not a .cjc project (missing header)";
        }
        return false;
    }
    return true;
}

bool loadProject(const std::string& path, Sequencer& seq, Mixer& mixer, Automation& automation,
                 std::string* err) {
    std::ifstream f(path);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for reading";
        }
        return false;
    }
    return readProjectFrom(f, seq, mixer, automation, err);
}

bool loadProjectFromString(const std::string& text, Sequencer& seq, Mixer& mixer,
                           Automation& automation, std::string* err) {
    std::istringstream ss(text);
    return readProjectFrom(ss, seq, mixer, automation, err);
}

// Save a single synth engine as a standalone instrument preset (".cjcpatch"). The body is written by
// the very same writeSynthBlock() used for full projects, so a preset is just the two synth lines
// under a distinct header — and it loads back through the exact same parsers, guaranteeing an exact
// round-trip and forward/backward compatibility with the project format.
bool saveSynthPreset(const std::string& path, const SynthInstrument& s, std::string* err) {
    std::ofstream f(path);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for writing";
        }
        return false;
    }
    f << "cjcsynth 1\n";
    writeSynthBlock(f, "synth", "synthosc", s);
    if (!f) {
        if (err != nullptr) {
            *err = "write error on '" + path + "'";
        }
        return false;
    }
    return true;
}

bool loadSynthPreset(const std::string& path, SynthInstrument& s, std::string* err) {
    std::ifstream f(path);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for reading";
        }
        return false;
    }
    bool sawHeader = false, sawSynth = false;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ls(line);
        std::string tag;
        if (!(ls >> tag)) {
            continue;
        }
        if (tag == "cjcsynth") {
            sawHeader = true;
        } else if (tag == "synth") {
            parseSynthLine(ls, s);
            sawSynth = true;
        } else if (tag == "synthosc") {
            parseOscLine(ls, s);
        }
    }
    if (!sawHeader || !sawSynth) {
        if (err != nullptr) {
            *err = "'" + path + "' is not a valid CJC synth preset";
        }
        return false;
    }
    return true;
}

} // namespace maz::audio
