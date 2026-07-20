#pragma once

#include "maz/audio/DrumVoice.hpp"
#include "maz/audio/PianoRoll.hpp"
#include "maz/audio/Sampler.hpp"
#include "maz/audio/SynthInstrument.hpp"

#include <string>
#include <vector>

namespace maz::audio {

// One pattern's musical content: the drum step grid plus the piano-roll notes. Instruments (the
// drum kit, synth, sampler) are shared across patterns; only this content changes per pattern.
struct Pattern {
    std::string name;          // human-readable label for the arrangement/playlist
    std::vector<uint8_t> grid; // channel-major: grid[channel * numSteps + step]
    std::vector<uint8_t> prob; // per-step trigger probability, 0..255 (255 = always). Parallel to grid.
    std::vector<uint8_t> ratchet; // per-step retrigger count 1..4 (0/1 = single hit). Parallel to grid.
    std::vector<int8_t> tune;     // per-step pitch offset in semitones (0 = channel pitch). Parallel to grid.
    std::vector<int8_t> nudge;    // per-step timing push, % of the step's slot (0 = on-grid, later). Parallel to grid.
    std::vector<uint8_t> stride;  // per-step trig condition: fire only every Nth pattern loop (1 = always). Parallel to grid.
    PianoRoll roll;            // lead instrument
    PianoRoll roll2;           // second (bass) instrument
    float swing = 0.0f;        // per-pattern swing amount (0..0.9); each pattern grooves on its own
    int transpose = 0;         // per-pattern transpose in semitones (added to the global transpose)
    float tempoMul = 1.0f;     // per-pattern tempo multiplier on the song BPM (0.25..4; 1 = song tempo)
                               // — a section can run half-time or double-time in a chained song
};

// An FL-style step sequencer (a "channel rack"): a grid of channels × steps, a transport
// (play/stop + BPM), and a set of built-in drum voices — one per channel. When playing, it walks
// the step grid in sample-accurate time and strikes each channel whose step is switched on.
//
// It holds multiple patterns and a playlist: in song mode the transport chains the playlist's
// patterns back-to-back into a full arrangement; otherwise it loops the selected pattern.
//
// render() ADDS the mixed channel output into the caller's buffer and internally splits the block
// at step boundaries so triggers land on the sample, not the block edge. Voices keep ringing after
// stop() so tails are not cut off; only the transport freezes.
//
// This is the seam the rest of the DAW grows from: patterns, the piano roll, and automation all
// schedule against this same clock.
class Sequencer {
public:
    Sequencer();

    // --- Transport -----------------------------------------------------------
    void play();  // start from step 0 and strike step 0 immediately
    void stop();  // freeze the transport (ringing voices still decay)

    // Release every held melodic voice (lead synth, bass synth, sampler) at once — a "panic" /
    // all-notes-off for stuck notes, and the clean way to switch patterns live without leaving the
    // outgoing pattern's notes ringing. Voices enter their release stage (they still fade out).
    void releaseAllNotes();
    bool playing() const { return playing_; }

    void setBpm(double bpm) { bpm_ = bpm > 1.0 ? bpm : 1.0; }
    double bpm() const { return bpm_; }

    // Swing/groove: 0 = straight; higher values push the off-beat (odd) steps later for a shuffled
    // feel, while keeping each pair of steps the same total length (tempo preserved). Range 0..0.9.
    // Per-pattern: each pattern carries its own swing, so different patterns can groove differently.
    void setSwing(float s);
    float swing() const { return patterns_[static_cast<size_t>(current_)].swing; }

    // Groove templates: stamp a named per-16th micro-timing feel onto every channel's per-step nudge
    // (a recognisable MPC/Akai-style groove that varies step to step, richer than the single swing
    // knob's uniform odd-step delay). Presets: 0 = Straight (clears all nudge), 1 = Swing 16th,
    // 2 = Swing 8th, 3 = Laid-back (whole pattern pushed slightly late), 4 = Hard swing. Applies to
    // the current pattern's drum grid. kGrooveCount presets; grooveName(i) for the UI labels.
    static constexpr int kGrooveCount = 5;
    static const char* grooveName(int preset);
    void applyGroove(int preset);

    // Humanize drum micro-timing: set a random per-step nudge in [0, maxNudge] % of a step on every
    // channel, for a loose "drummer feel" (the timing counterpart to the velocity humanize, and the
    // drum-grid counterpart to the piano roll's humanizeTiming). Deterministic per `seed`.
    void humanizeStepTiming(int maxNudge, uint32_t seed);

    // Per-pattern transpose (semitones, clamped ±48): the current pattern's melodic notes (lead + bass
    // + arp) are shifted by this on top of the global transpose, so a song can change key between
    // patterns (e.g. a chorus a step up). Drums are unaffected. Persisted with the pattern.
    void setPatternTranspose(int semis);
    int patternTranspose() const { return patterns_[static_cast<size_t>(current_)].transpose; }

    // Per-pattern tempo multiplier on the song BPM (0.25..4; 1 = the song tempo). In song mode each
    // chained pattern plays at its own speed, so a section can drop to half-time or push to double-time.
    void setPatternTempoMul(float mul);
    float patternTempoMul() const { return patterns_[static_cast<size_t>(current_)].tempoMul; }

    // Humanize: randomize each drum hit's velocity slightly (0 = off, 1 = max) for a less
    // machine-like feel. Deterministic, so renders are reproducible.
    void setHumanize(float amount);
    float humanize() const { return humanize_; }

    // Sidechain ducking: when the kick (channel 0) fires, the melodic bus is ducked and recovers
    // over `releaseMs` — the classic pumping effect. `amount` 0..1 is the depth. `attackMs` is how
    // fast the duck engages: 0 (default) snaps instantly (the classic hard pump); higher values ramp
    // the gain down over that many ms for a softer, rounded duck (like a real sidechain compressor's
    // attack). Old projects (and callers that omit it) keep the instant snap.
    void setSidechain(bool on, float amount, float releaseMs, float attackMs = 0.0f);
    bool sidechainOn() const { return sidechainOn_; }
    float sidechainAmount() const { return scAmount_; }
    float sidechainReleaseMs() const { return scReleaseMs_; }
    float sidechainAttackMs() const { return scAttackMs_; }
    // Which drum channel's hits duck the melodic bus (default 0 = kick). Lets any channel — a snare,
    // a clap — be the sidechain trigger, FL-style.
    void setSidechainSource(int channel) { sidechainSource_ = channel < 0 ? 0 : channel; }
    int sidechainSource() const { return sidechainSource_; }

    // Number of grid steps that make up one loop of the pattern (default 16 = one 4/4 bar of 16ths).
    int numSteps() const { return numSteps_; }
    // Resize the pattern length (1–64 steps). Every pattern's grids are re-laid-out, preserving the
    // steps that still fit. Lets patterns run shorter/longer than a bar (odd meters, 2-bar loops…).
    void setNumSteps(int steps);
    int stepsPerBeat() const { return stepsPerBeat_; }
    // Steps per beat (grid subdivision): 4 = 16th notes (default), 3 = 8th triplets, 6 = 16th
    // triplets, 8 = 32nds. A beat's duration stays tied to the BPM; this changes how finely it is
    // divided into steps. Clamped to 1–8.
    void setStepsPerBeat(int steps) { stepsPerBeat_ = steps < 1 ? 1 : (steps > 8 ? 8 : steps); }
    int numChannels() const { return static_cast<int>(channels_.size()); }
    const std::string& channelName(int channel) const { return names_[static_cast<size_t>(channel)]; }
    DrumVoice& channelVoice(int channel) { return channels_[static_cast<size_t>(channel)]; }

    // Per-channel mixer strip: volume (linear), mute, and solo. When any channel is soloed, only
    // soloed channels are heard.
    void setChannelVolume(int c, float v);
    void setChannelMute(int c, bool m);
    void setChannelSolo(int c, bool s);
    void setChannelPan(int c, float p); // -1 = hard left, 0 = center, +1 = hard right
    float channelVolume(int c) const;
    bool channelMute(int c) const;
    bool channelSolo(int c) const;
    float channelPan(int c) const;

    // Choke group (0 = none): when a channel triggers, it chokes every other channel in the same
    // group — the classic open-hat/closed-hat cutoff.
    void setChannelChokeGroup(int c, int group);
    int channelChokeGroup(int c) const;

    // Per-channel drum tuning in semitones (pitches the kick/snare up or down).
    void setChannelTune(int c, float semitones);
    float channelTune(int c) const;

    // Per-channel drum decay-length multiplier (0.25–4).
    void setChannelDecay(int c, float mul);
    float channelDecay(int c) const;

    // Per-channel drum drive/saturation (0..1): tanh soft-clip for punchier, grittier hits.
    void setChannelDrive(int c, float drive);
    float channelDrive(int c) const;

    // Per-channel pitch-envelope depth / "punch" (0..2): scales the tonal drums' (kick/tom) initial
    // pitch sweep. 1 = the natural sweep; 0 = flat sub (no click); 2 = a deeper, snappier attack.
    void setChannelPitchEnv(int c, float amount);
    float channelPitchEnv(int c) const;

    // Per-channel pitch-envelope time (0.25..4): scales how long the kick/tom pitch sweep takes.
    void setChannelPitchEnvTime(int c, float mul);
    float channelPitchEnvTime(int c) const;

    // Per-channel tone (low-pass cutoff in Hz, 20000 = open): darkens an individual drum, FL-style.
    void setChannelTone(int c, float hz);
    float channelTone(int c) const;

    // Per-channel high-pass / low-cut (cutoff in Hz, 0 = off): thins an individual drum — tighten a
    // boomy kick, shave rumble off hats/claps.
    void setChannelHighpass(int c, float hz);
    float channelHighpass(int c) const;

    // Per-channel snap (0..1): the snare's noise-vs-tone balance (body ↔ wires); 0.5 = classic.
    void setChannelSnap(int c, float s);
    float channelSnap(int c) const;

    // Per-channel flam (0..50 ms): plays a quiet grace hit immediately, then the full hit this many
    // ms later — the classic flam/drag humanization. 0 = off (a single hit).
    void setChannelFlam(int c, float ms);
    float channelFlam(int c) const;

    // Per-channel drum sound: which synthesized voice (kick/snare/hat/…/tom) the channel plays, so
    // any row can be reassigned (e.g. two kicks, or a tom).
    void setChannelType(int c, Drum type);
    Drum channelType(int c) const;

    // The step currently sounding (0..numSteps-1); useful for a playhead in the UI.
    int currentStep() const { return currentStep_; }

    // The melodic side: a pitched synth (or the sampler) playing the piano-roll pattern.
    SynthInstrument& synth() { return synth_; }
    Sampler& sampler() { return sampler_; }
    PianoRoll& roll() { return patterns_[static_cast<size_t>(current_)].roll; }

    // The second (bass) instrument: its own synth and piano-roll lane.
    SynthInstrument& synth2() { return synth2_; }
    PianoRoll& roll2() { return patterns_[static_cast<size_t>(current_)].roll2; }

    // --- Patterns & arrangement ---------------------------------------------
    int patternCount() const { return static_cast<int>(patterns_.size()); }
    int currentPattern() const { return current_; }
    void selectPattern(int i);
    int addPattern(); // append an empty pattern; returns its index
    int clonePattern(int src); // append a full copy of pattern `src` (name + " copy"); returns index
    void clearArrangement(); // reset to a single empty pattern, empty playlist, pattern mode

    // Human-readable pattern name (defaults to "Pattern N"), shown in the arrangement UI.
    void setPatternName(int i, const std::string& name);
    const std::string& patternName(int i) const;

    void setSongMode(bool on) { songMode_ = on; }
    bool songMode() const { return songMode_; }
    // Whether the playlist loops (default) or plays once and stops at the end — the latter for
    // bouncing a finite arrangement without a trailing repeat.
    void setSongLoop(bool on) { songLoop_ = on; }
    bool songLoop() const { return songLoop_; }
    const std::vector<int>& playlist() const { return playlist_; }
    void setPlaylist(std::vector<int> seq) { playlist_ = std::move(seq); }
    void clearPlaylist() { playlist_.clear(); }
    void appendToPlaylist(int patternIndex) { playlist_.push_back(patternIndex); }

    // Song loop region: restrict song-mode playback to the playlist index half-open range
    // [start, end) — playback starts at `start` and, when looping, wraps `end`→`start` instead of
    // cycling the whole playlist. A degenerate/empty range (end <= start) clears it (loop the whole
    // playlist, the default). Great for looping a section (a drop, a chorus) while working on it.
    void setSongLoopRange(int start, int end) {
        songLoopStart_ = start < 0 ? 0 : start;
        songLoopEnd_ = end;
    }
    int songLoopStart() const { return songLoopStart_; }
    int songLoopEnd() const { return songLoopEnd_; }

    // Route the piano roll to the sampler instead of the synth (when a sample is loaded).
    void setUseSampler(bool on) { useSampler_ = on; }
    bool useSampler() const { return useSampler_; }

    // Arpeggiator: when on, held piano-roll chords are played one note per step, cycling through the
    // held pitches. Mode 0 = up, 1 = down, 2 = up-down, 3 = random (deterministic), 4 = as-played
    // (the notes' entry order), 5 = chord (strike every held pitch together — rhythmic stabs).
    // `arpCurrentPitch` reflects the last note it played (-1 if silent) — useful for the UI and tests.
    void setArp(bool on, int mode);
    bool arpOn() const { return arpOn_; }
    int arpMode() const { return arpMode_; }
    int arpCurrentPitch() const { return arpCurrentPitch_; }
    // Octave range (1..4): the arp cycles through the held pitches replicated across this many
    // octaves, so it climbs/descends over a wider range.
    void setArpOctaves(int octaves) { arpOctaves_ = octaves < 1 ? 1 : (octaves > 4 ? 4 : octaves); }
    int arpOctaves() const { return arpOctaves_; }
    // Gate length (0.05..1): the fraction of each step an arp note is held before it is released. 1
    // (default) = legato (the note rings until the next step); lower = staccato (a gap before the next
    // note), the classic tight arp bounce.
    void setArpGate(float g) { arpGate_ = g < 0.05f ? 0.05f : (g > 1.0f ? 1.0f : g); }
    float arpGate() const { return arpGate_; }
    // Rate (1..8 steps per arp note): the arp advances/retriggers every `steps` grid steps and holds
    // the note in between, so it can run at 1/8 or 1/4 over a 1/16 grid. 1 (default) = one note per
    // step (the classic behaviour).
    void setArpRate(int steps) { arpRate_ = steps < 1 ? 1 : (steps > 8 ? 8 : steps); }
    int arpRate() const { return arpRate_; }

    // Bus levels: relative gain of the drum kit, the lead synth, and the bass synth before the
    // soft-limited sum. (synthGain is the lead level; bassGain the second instrument.)
    void setDrumGain(float g) { drumGain_ = g; }
    void setSynthGain(float g) { synthGain_ = g; }
    void setBassGain(float g) { bassGain_ = g; }

    // Global transpose in semitones (±48): shifts every melodic note (lead, bass, arp, sampler) at
    // playback without editing the notes — change key on the fly.
    void setTranspose(int semis) { transpose_ = semis < -48 ? -48 : (semis > 48 ? 48 : semis); }
    int transpose() const { return transpose_; }
    float drumGain() const { return drumGain_; }
    float synthGain() const { return synthGain_; }
    float bassGain() const { return bassGain_; }

    // Stereo pan for the melodic buses (-1 = hard left, 0 = center, +1 = hard right), equal-power.
    void setLeadPan(float p) { leadPan_ = p < -1.0f ? -1.0f : (p > 1.0f ? 1.0f : p); }
    void setBassPan(float p) { bassPan_ = p < -1.0f ? -1.0f : (p > 1.0f ? 1.0f : p); }
    float leadPan() const { return leadPan_; }
    float bassPan() const { return bassPan_; }

    // Metronome: an accented click on each beat while playing (a brighter click on the downbeat,
    // step 0 of the bar). A monitoring aid, mixed into the output.
    void setMetronome(bool on) { metronome_ = on; }
    bool metronome() const { return metronome_; }
    // Metronome click level (0..1): scales the click loudness. 0.5 (default) matches the original.
    void setMetronomeLevel(float v) { metroLevel_ = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
    float metronomeLevel() const { return metroLevel_; }

    // Count-in: play this many bars of clicks before the pattern starts (0 = none). While counting
    // in, only the metronome sounds; the pattern begins when the count-in finishes.
    void setCountInBars(int bars) { countInBars_ = bars < 0 ? 0 : bars; }
    int countInBars() const { return countInBars_; }
    bool countingIn() const { return countingIn_; }

    // --- Pattern grid --------------------------------------------------------
    bool step(int channel, int step) const;
    void setStep(int channel, int step, bool on);
    void toggle(int channel, int step);
    void clear(); // switch every step off

    // Per-step velocity/accent in [0, 1] (0 = off). setStep uses full velocity.
    float stepVelocity(int channel, int step) const;
    void setStepVelocity(int channel, int step, float velocity);

    // Per-step trigger probability in [0, 1] (1 = always fire). A step below 1.0 fires only some of
    // the time, driven by a deterministic per-transport RNG — humanizing/generative grooves.
    float stepProbability(int channel, int step) const;
    void setStepProbability(int channel, int step, float probability);

    // Per-step ratchet count (1–4): a step > 1 retriggers that many evenly-spaced hits within its
    // slot — drum rolls, stutters, hi-hat rushes.
    int stepRatchet(int channel, int step) const;
    void setStepRatchet(int channel, int step, int count);

    // Per-step pitch offset in semitones (±24, 0 = the channel's own tuning): the channel-rack graph
    // editor's pitch row, so individual hits can be tuned up/down (melodic toms, pitched hats).
    int stepTune(int channel, int step) const;
    void setStepTune(int channel, int step, int semitones);

    // Per-step micro-timing nudge (0..95): push a single step's hit later by this percent of the
    // step's own slot, so it lands off the grid for a laid-back/behind-the-beat feel — a per-step,
    // finer-grained cousin of swing. 0 = dead on the grid. Delivered through the deferred-hit queue,
    // so the strike stays sample-accurate. Drum grid only (the piano roll keeps its own timing).
    int stepNudge(int channel, int step) const;
    void setStepNudge(int channel, int step, int percent);

    // Per-step trig condition (1..8): the step fires only on transport loops where (loopIndex %
    // stride) == 0, i.e. stride 1 = every loop (default), 2 = every other loop, 4 = one loop in four —
    // the classic Elektron/FL conditional trig for fills and long-form variation. The loop index
    // counts bars from the start of playback (so in a single looping pattern it is that pattern's loop
    // count; in song mode it counts global bars across the playlist — song-wide fills every N bars).
    // Deterministic: the same transport always plays the same loops. Distinct from per-step
    // probability (random). Drum grid.
    int stepStride(int channel, int step) const;
    void setStepStride(int channel, int step, int stride);

    // Rotate a channel's whole step row by `offset` steps with wraparound (positive = later, negative
    // = earlier), carrying each step's velocity, probability, and ratchet along with it. A quick way
    // to shift a groove around the bar.
    void rotateChannel(int channel, int offset);

    // Euclidean fill: replace a channel's row with `pulses` hits distributed as evenly as possible
    // across the pattern length (the classic Euclidean rhythm). pulses is clamped to [0, numSteps];
    // 0 clears the row. Returns the number of hits placed.
    int euclidFill(int channel, int pulses);

    // Render `frames` of interleaved STEREO samples, ADDING the panned channel mix into out
    // (out has 2*frames floats). Advances the transport when playing. `sampleRate` is in Hz.
    void render(float* out, int frames, int sampleRate);

    // Render the three mixer buses separately into their own interleaved-stereo buffers (each
    // 2*frames floats, ADDED into). This is the per-track path: a caller can run each bus through
    // its own insert chain before summing. Advances the transport exactly like render(). Bus order
    // matches MixerBus: 0 = drums, 1 = lead, 2 = bass.
    void renderStems(float* drums, float* lead, float* bass, int frames, int sampleRate);

private:
    // Frames for a given step at the current tempo. stepsPerBeat_ 16th-notes → 4 steps per beat.
    // Swing lengthens even steps and shortens odd ones, so `step`'s parity matters.
    double samplesPerStep(int sampleRate, int step) const;
    void triggerStep(int step);

    std::vector<DrumVoice> channels_;
    std::vector<std::string> names_;
    std::vector<float> chanVolume_; // per-channel linear level
    std::vector<uint8_t> chanMute_;
    std::vector<uint8_t> chanSolo_;
    std::vector<float> chanPan_; // per-channel pan (-1..1)
    std::vector<int> chanChoke_; // per-channel choke group (0 = none)
    std::vector<float> chanFlam_; // per-channel flam offset in ms (0 = off)
    std::vector<Pattern> patterns_; // at least one; patterns_[current_] is edited/played
    int current_ = 0;
    std::vector<int> playlist_;     // ordered pattern indices for song mode
    int songLoopStart_ = 0;         // song loop region start (playlist index)
    int songLoopEnd_ = 0;           // song loop region end (exclusive); <= start = whole playlist
    bool songMode_ = false;
    bool songLoop_ = true;
    int playlistPos_ = 0;

    std::vector<float> mixScratch_;   // per-block, per-channel drum render
    std::vector<float> synthScratch_; // per-block lead (synth + sampler) sum
    std::vector<float> bassScratch_;  // per-block bass (synth2) sum
    std::vector<float> lBuf_;         // per-block stereo accumulators (pre-limit)
    std::vector<float> rBuf_;
    std::vector<float> stemDrums_;    // render()'s temporaries: the three buses before summing
    std::vector<float> stemLead_;
    std::vector<float> stemBass_;

    SynthInstrument synth_{};  // lead instrument playing roll
    SynthInstrument synth2_{}; // bass instrument playing roll2
    Sampler sampler_{};        // alternative lead instrument (sample playback)
    bool useSampler_ = false;
    bool arpOn_ = false;
    int arpMode_ = 0;
    int arpOctaves_ = 1;
    int arpCounter_ = 0;
    int arpCurrentPitch_ = -1;
    std::vector<int> arpHeld_; // all arp pitches currently sounding (multiple only in chord mode)
    uint32_t arpRng_ = 0x1234567u; // deterministic RNG for the random arp mode
    float arpGate_ = 1.0f;         // arp note length as a fraction of a step (1 = legato)
    int arpRate_ = 1;              // grid steps per arp note (1 = one note per step)
    int arpGateFramesLeft_ = -1;   // frames until the current arp note is released (-1 = none pending)
    float drumGain_ = 1.0f;
    float synthGain_ = 1.0f;
    float bassGain_ = 1.0f;
    float leadPan_ = 0.0f;
    float bassPan_ = 0.0f;
    int transpose_ = 0;

    int numSteps_ = 16;
    int stepsPerBeat_ = 4;
    double bpm_ = 120.0;
    float humanize_ = 0.0f;
    uint32_t humanizeCounter_ = 0;
    bool sidechainOn_ = false;
    float scAmount_ = 0.7f;
    float scReleaseMs_ = 200.0f;
    float scAttackMs_ = 0.0f; // duck engage time (0 = instant snap)
    int sidechainSource_ = 0; // drum channel that triggers the duck (0 = kick)
    float scEnv_ = 1.0f;      // current ducking gain (1 = open)
    float scTarget_ = 1.0f;   // floor the duck is heading toward while attacking (1 - amount)
    bool scAttacking_ = false; // true while ramping down to scTarget_ (attack phase)

    bool metronome_ = false;
    float metroLevel_ = 0.5f;  // metronome click level (0..1)
    int metroLastStep_ = -1;  // last step a click fired on (avoids double-triggering)
    double metroPhase_ = 0.0; // click oscillator phase
    float metroEnv_ = 0.0f;   // click amplitude envelope
    float metroFreq_ = 0.0f;  // current click pitch (accented on the downbeat)
    int countInBars_ = 0;     // bars of count-in before the pattern starts
    bool countingIn_ = false; // currently playing the count-in
    int countInStepsRemaining_ = 0;
    uint32_t probRng_ = 0x9E3779B9u; // deterministic RNG for per-step probability
    uint32_t loopCounter_ = 0;       // pattern-loop index (increments each wrap), for per-step trig conditions

    int sampleRate_ = 48000; // last render rate, used to schedule ratchet sub-hits
    struct RatchetHit {
        int channel;
        float velocity;
        int framesUntil;
        float tune = 0.0f; // per-step pitch offset carried to the sub-hit
    };
    std::vector<RatchetHit> ratchets_; // pending ratchet retriggers

    // Pending melodic roll retriggers (per-note ratchet): a note re-struck within its first step.
    struct MelodicHit {
        int pitch;         // already transposed
        float velocity;
        float fineTune;
        int framesUntil;
        bool bass;         // true = synth2 (bass lane); false = lead
        bool toSampler;    // lead lane routed to the sampler
        float cutoff;      // per-note filter-cutoff offset in octaves (FL "Mod X")
    };
    std::vector<MelodicHit> melodicHits_;

    bool playing_ = false;
    int currentStep_ = 0;
    double samplesIntoStep_ = 0.0;
};

} // namespace maz::audio
