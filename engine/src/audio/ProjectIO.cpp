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
                          : (mode == 2 ? SynthMode::Wavetable : SynthMode::Subtractive));
    syn.setWaveform(static_cast<Waveform>(wave < 0 || wave > 3 ? 0 : wave));
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
}
// Parse a `synthosc`/`synthosc2` line.
void parseOscLine(std::istringstream& ls, SynthInstrument& syn) {
    float detune = 0.0f, osc2 = 0.0f, sub = 0.0f, noise = 0.0f;
    ls >> detune >> osc2 >> sub >> noise;
    syn.setOscillators(detune, osc2, sub, noise);
}
} // namespace

// File format (line-based text, ".cjc"):
//   cjc 1
//   bpm <double>
//   busgain <drum> <synth>
//   step <channel> <step>                 (repeated, one per active drum step)
//   note <start> <len> <pitch> <velocity> (repeated, one per piano-roll note)
//   master <gain>
//   fx eq <enabled> <cutoff>
//   fx comp <enabled> <thrDb> <ratio> <atkMs> <relMs> <makeupDb>
//   fx delay <enabled> <timeMs> <feedback> <mix>
//   fx reverb <enabled> <roomSize> <damping> <mix>

bool saveProject(const std::string& path, Sequencer& seq, Mixer& mixer, Automation& automation,
                 std::string* err) {
    std::ofstream f(path);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for writing";
        }
        return false;
    }

    f << "cjc 1\n";
    f << "bpm " << seq.bpm() << "\n";
    f << "swing " << seq.swing() << "\n";
    f << "sidechain " << (seq.sidechainOn() ? 1 : 0) << " " << seq.sidechainAmount() << " "
      << seq.sidechainReleaseMs() << "\n";
    f << "arp " << (seq.arpOn() ? 1 : 0) << " " << seq.arpMode() << "\n";
    f << "humanize " << seq.humanize() << "\n";
    f << "metronome " << (seq.metronome() ? 1 : 0) << "\n";
    f << "countin " << seq.countInBars() << "\n";
    f << "busgain " << seq.drumGain() << " " << seq.synthGain() << " " << seq.bassGain() << "\n";

    auto writeSynth = [&](const char* tag, const char* oscTag, const SynthInstrument& s) {
        f << tag << " " << static_cast<int>(s.mode()) << " " << static_cast<int>(s.waveform()) << " "
          << s.attack() << " " << s.decay() << " " << s.sustain() << " " << s.release() << " "
          << s.fmRatio() << " " << s.fmIndex() << " " << s.gain() << " " << s.filterCutoff() << " "
          << s.filterResonance() << " " << s.filterEnvAmount() << " " << s.wavetablePosition() << " "
          << s.wavetableMorph() << " " << s.glide() << "\n";
        f << oscTag << " " << s.detuneCents() << " " << s.osc2Level() << " " << s.subLevel() << " "
          << s.noiseLevel() << "\n";
    };
    writeSynth("synth", "synthosc", seq.synth());
    writeSynth("synth2", "synthosc2", seq.synth2());

    f << "samplercfg " << (seq.sampler().reverse() ? 1 : 0) << " " << (seq.sampler().loop() ? 1 : 0)
      << "\n";
    f << "sampler " << (seq.useSampler() ? 1 : 0) << " " << seq.sampler().basePitch() << " "
      << seq.sampler().gain() << " " << seq.sampler().path() << "\n";

    for (int c = 0; c < seq.numChannels(); ++c) {
        f << "chan " << c << " " << seq.channelVolume(c) << " " << (seq.channelMute(c) ? 1 : 0)
          << " " << (seq.channelSolo(c) ? 1 : 0) << " " << seq.channelPan(c) << "\n";
    }

    // Arrangement: every pattern's grid + notes, the playlist, and the song-mode flag.
    const int savedCurrent = seq.currentPattern();
    f << "patterns " << seq.patternCount() << "\n";
    f << "songmode " << (seq.songMode() ? 1 : 0) << "\n";
    f << "playlist " << seq.playlist().size();
    for (int idx : seq.playlist()) {
        f << " " << idx;
    }
    f << "\n";
    for (int p = 0; p < seq.patternCount(); ++p) {
        seq.selectPattern(p);
        for (int c = 0; c < seq.numChannels(); ++c) {
            for (int s = 0; s < seq.numSteps(); ++s) {
                if (seq.step(c, s)) {
                    const int vel = static_cast<int>(seq.stepVelocity(c, s) * 255.0f + 0.5f);
                    f << "step " << p << " " << c << " " << s << " " << vel << "\n";
                }
            }
        }
        for (const Note& n : seq.roll2().notes()) {
            f << "note2 " << p << " " << n.startStep << " " << n.lengthSteps << " " << n.pitch << " "
              << n.velocity << "\n";
        }
        for (const Note& n : seq.roll().notes()) {
            f << "note " << p << " " << n.startStep << " " << n.lengthSteps << " " << n.pitch << " "
              << n.velocity << "\n";
        }
    }
    seq.selectPattern(savedCurrent);

    f << "master " << mixer.masterGain() << "\n";
    f << "fx eq " << (mixer.eq().enabled() ? 1 : 0) << " " << mixer.eq().cutoff() << "\n";
    f << "fx hp " << (mixer.highpass().enabled() ? 1 : 0) << " " << mixer.highpass().cutoff() << "\n";
    f << "fx comp " << (mixer.compressor().enabled() ? 1 : 0) << " "
      << mixer.compressor().thresholdDb() << " " << mixer.compressor().ratio() << " "
      << mixer.compressor().attackMs() << " " << mixer.compressor().releaseMs() << " "
      << mixer.compressor().makeupDb() << "\n";
    f << "fx delay " << (mixer.delay().enabled() ? 1 : 0) << " " << mixer.delay().time() << " "
      << mixer.delay().feedback() << " " << mixer.delay().mix() << " "
      << (mixer.delay().pingPong() ? 1 : 0) << "\n";
    f << "fx reverb " << (mixer.reverb().enabled() ? 1 : 0) << " " << mixer.reverb().roomSize()
      << " " << mixer.reverb().damping() << " " << mixer.reverb().mix() << " "
      << mixer.reverb().preDelayMs() << "\n";
    f << "fx peq " << (mixer.peq().enabled() ? 1 : 0) << " " << mixer.peq().lowGain() << " "
      << mixer.peq().midFreq() << " " << mixer.peq().midQ() << " " << mixer.peq().midGain() << " "
      << mixer.peq().highGain() << "\n";
    f << "fx dist " << (mixer.distortion().enabled() ? 1 : 0) << " " << mixer.distortion().drive()
      << " " << mixer.distortion().mix() << "\n";
    f << "fx chorus " << (mixer.chorus().enabled() ? 1 : 0) << " " << mixer.chorus().rate() << " "
      << mixer.chorus().depth() << " " << mixer.chorus().mix() << "\n";
    f << "fx phaser " << (mixer.phaser().enabled() ? 1 : 0) << " " << mixer.phaser().rate() << " "
      << mixer.phaser().depth() << " " << mixer.phaser().feedback() << " " << mixer.phaser().mix()
      << "\n";
    f << "fx crush " << (mixer.bitcrusher().enabled() ? 1 : 0) << " " << mixer.bitcrusher().bits()
      << " " << mixer.bitcrusher().downsample() << " " << mixer.bitcrusher().mix() << "\n";
    f << "fx gate " << (mixer.gate().enabled() ? 1 : 0) << " " << mixer.gate().thresholdDb() << " "
      << mixer.gate().ratio() << " " << mixer.gate().rangeDb() << " " << mixer.gate().attackMs()
      << " " << mixer.gate().releaseMs() << "\n";
    f << "fx width " << (mixer.widener().enabled() ? 1 : 0) << " " << mixer.widener().width()
      << "\n";
    f << "fx tape " << (mixer.tape().enabled() ? 1 : 0) << " " << mixer.tape().drive() << " "
      << mixer.tape().warmth() << " " << mixer.tape().mix() << "\n";

    // Aux send/return buses: send level + the return effect's params.
    f << "send reverb " << mixer.reverbSend() << " " << mixer.reverbReturn().roomSize() << " "
      << mixer.reverbReturn().damping() << "\n";
    f << "send delay " << mixer.delaySend() << " " << mixer.delayReturn().time() << " "
      << mixer.delayReturn().feedback() << "\n";

    // Per-bus mixer-track insert strips (0 = drums, 1 = lead, 2 = bass).
    for (int t = 0; t < Mixer::trackCount(); ++t) {
        MixerTrack& tr = mixer.track(t);
        f << "track " << t << " " << tr.gain() << " " << (tr.muted() ? 1 : 0) << " "
          << (tr.eq().enabled() ? 1 : 0) << " " << tr.eq().lowGain() << " " << tr.eq().midFreq()
          << " " << tr.eq().midQ() << " " << tr.eq().midGain() << " " << tr.eq().highGain() << " "
          << (tr.distortion().enabled() ? 1 : 0) << " " << tr.distortion().drive() << " "
          << (tr.compressor().enabled() ? 1 : 0) << " " << tr.compressor().thresholdDb() << " "
          << tr.compressor().ratio() << " " << tr.compressor().makeupDb() << "\n";
    }

    f << "plugin " << (mixer.plugin().enabled() ? 1 : 0) << " " << mixer.plugin().path() << "\n";

    for (int i = 0; i < Automation::count(); ++i) {
        const AutoLane& lane = automation.lane(i);
        f << "auto " << i << " " << (lane.enabled ? 1 : 0) << " "
          << static_cast<int>(lane.lfo.shape) << " " << lane.lfo.rateHz << " " << lane.lo << " "
          << lane.hi << "\n";
        // Automation clip (breakpoints): only written when the lane has one.
        if (!lane.clip.empty()) {
            f << "autoclip " << i << " " << lane.clipLength << " " << lane.clip.size();
            for (const AutoPoint& p : lane.clip) {
                f << " " << p.time << " " << p.value;
            }
            f << "\n";
        }
    }

    if (!f) {
        if (err != nullptr) {
            *err = "write to '" + path + "' failed";
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

    // Reset the destination to a clean slate so the file fully defines the project.
    seq.clearArrangement();

    std::string line;
    bool sawHeader = false;
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
        } else if (tag == "swing") {
            float sw = 0.0f;
            ls >> sw;
            seq.setSwing(sw);
        } else if (tag == "sidechain") {
            int on = 0;
            float amount = 0.7f, rel = 200.0f;
            ls >> on >> amount >> rel;
            seq.setSidechain(on != 0, amount, rel);
        } else if (tag == "arp") {
            int on = 0, mode = 0;
            ls >> on >> mode;
            seq.setArp(on != 0, mode);
        } else if (tag == "humanize") {
            float h = 0.0f;
            ls >> h;
            seq.setHumanize(h);
        } else if (tag == "metronome") {
            int m = 0;
            ls >> m;
            seq.setMetronome(m != 0);
        } else if (tag == "countin") {
            int b = 0;
            ls >> b;
            seq.setCountInBars(b);
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
        } else if (tag == "synth") {
            parseSynthLine(ls, seq.synth());
        } else if (tag == "synth2") {
            parseSynthLine(ls, seq.synth2());
        } else if (tag == "synthosc") {
            parseOscLine(ls, seq.synth());
        } else if (tag == "synthosc2") {
            parseOscLine(ls, seq.synth2());
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
        } else if (tag == "patterns") {
            int count = 1;
            ls >> count;
            while (seq.patternCount() < count) {
                seq.addPattern();
            }
        } else if (tag == "songmode") {
            int on = 0;
            ls >> on;
            seq.setSongMode(on != 0);
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
        } else if (tag == "note") {
            int p = 0;
            Note n;
            ls >> p >> n.startStep >> n.lengthSteps >> n.pitch >> n.velocity;
            seq.selectPattern(p);
            seq.roll().addNote(n);
        } else if (tag == "note2") {
            int p = 0;
            Note n;
            ls >> p >> n.startStep >> n.lengthSteps >> n.pitch >> n.velocity;
            seq.selectPattern(p);
            seq.roll2().addNote(n);
        } else if (tag == "master") {
            float g = 0.9f;
            ls >> g;
            mixer.setMasterGain(g);
        } else if (tag == "fx") {
            std::string which;
            int en = 0;
            ls >> which >> en;
            if (which == "eq") {
                float cutoff = 8000.0f;
                ls >> cutoff;
                mixer.eq().setEnabled(en != 0);
                mixer.eq().setCutoff(cutoff);
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
            } else if (which == "gate") {
                float thr = -40.0f, ratio = 4.0f, range = -60.0f, atk = 2.0f, rel = 80.0f;
                ls >> thr >> ratio >> range >> atk >> rel;
                mixer.gate().setEnabled(en != 0);
                mixer.gate().setThresholdDb(thr);
                mixer.gate().setRatio(ratio);
                mixer.gate().setRangeDb(range);
                mixer.gate().setAttackMs(atk);
                mixer.gate().setReleaseMs(rel);
            } else if (which == "width") {
                float w = 1.0f;
                ls >> w;
                mixer.widener().setEnabled(en != 0);
                mixer.widener().setWidth(w);
            } else if (which == "tape") {
                float drive = 2.0f, warmth = 0.3f, mix = 1.0f;
                ls >> drive >> warmth >> mix;
                mixer.tape().setEnabled(en != 0);
                mixer.tape().setDrive(drive);
                mixer.tape().setWarmth(warmth);
                mixer.tape().setMix(mix);
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
            } else if (which == "dist") {
                float drive = 2.0f, mix = 0.5f;
                ls >> drive >> mix;
                mixer.distortion().setEnabled(en != 0);
                mixer.distortion().setDrive(drive);
                mixer.distortion().setMix(mix);
            } else if (which == "chorus") {
                float rate = 0.8f, depth = 3.0f, mix = 0.4f;
                ls >> rate >> depth >> mix;
                mixer.chorus().setEnabled(en != 0);
                mixer.chorus().setRate(rate);
                mixer.chorus().setDepth(depth);
                mixer.chorus().setMix(mix);
            } else if (which == "crush") {
                float bits = 8.0f, ds = 4.0f, mix = 0.5f;
                ls >> bits >> ds >> mix;
                mixer.bitcrusher().setEnabled(en != 0);
                mixer.bitcrusher().setBits(bits);
                mixer.bitcrusher().setDownsample(ds);
                mixer.bitcrusher().setMix(mix);
            } else if (which == "phaser") {
                float rate = 0.5f, depth = 0.7f, fb = 0.3f, mix = 0.5f;
                ls >> rate >> depth >> fb >> mix;
                mixer.phaser().setEnabled(en != 0);
                mixer.phaser().setRate(rate);
                mixer.phaser().setDepth(depth);
                mixer.phaser().setFeedback(fb);
                mixer.phaser().setMix(mix);
            } else if (which == "peq") {
                float lowDb = 0.0f, midF = 1000.0f, midQ = 1.0f, midDb = 0.0f, highDb = 0.0f;
                ls >> lowDb >> midF >> midQ >> midDb >> highDb;
                mixer.peq().setEnabled(en != 0);
                mixer.peq().setLowGain(lowDb);
                mixer.peq().setMid(midF, midQ, midDb);
                mixer.peq().setHighGain(highDb);
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
            int t = -1, muted = 0, eqEn = 0, distEn = 0, compEn = 0;
            float gain = 1.0f, low = 0.0f, midF = 1000.0f, midQ = 1.0f, midDb = 0.0f, high = 0.0f;
            float drive = 1.0f, thr = -18.0f, ratio = 4.0f, mk = 0.0f;
            ls >> t >> gain >> muted >> eqEn >> low >> midF >> midQ >> midDb >> high >> distEn >>
                drive >> compEn >> thr >> ratio >> mk;
            if (t >= 0 && t < Mixer::trackCount()) {
                MixerTrack& tr = mixer.track(t);
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
        } else if (tag == "auto") {
            int idx = -1, en = 0, shape = 0;
            float rate = 0.5f, lo = 0.0f, hi = 1.0f;
            ls >> idx >> en >> shape >> rate >> lo >> hi;
            if (idx >= 0 && idx < Automation::count()) {
                AutoLane& lane = automation.lane(idx);
                lane.enabled = en != 0;
                lane.lfo.shape = static_cast<Waveform>(shape < 0 || shape > 3 ? 0 : shape);
                lane.lfo.rateHz = rate;
                lane.lo = lo;
                lane.hi = hi;
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
            }
        }
        // Unknown tags are ignored for forward compatibility.
    }

    seq.selectPattern(0); // leave the first pattern selected after a load

    if (!sawHeader) {
        if (err != nullptr) {
            *err = "'" + path + "' is not a .cjc project (missing header)";
        }
        return false;
    }
    return true;
}

} // namespace maz::audio
