#include "maz/audio/ProjectIO.hpp"

#include "maz/audio/Automation.hpp"
#include "maz/audio/Mixer.hpp"
#include "maz/audio/Sequencer.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace maz::audio {

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
    f << "busgain " << seq.drumGain() << " " << seq.synthGain() << "\n";

    const SynthInstrument& syn = seq.synth();
    f << "synth " << static_cast<int>(syn.mode()) << " " << static_cast<int>(syn.waveform()) << " "
      << syn.attack() << " " << syn.decay() << " " << syn.sustain() << " " << syn.release() << " "
      << syn.fmRatio() << " " << syn.fmIndex() << " " << syn.gain() << "\n";

    f << "sampler " << (seq.useSampler() ? 1 : 0) << " " << seq.sampler().basePitch() << " "
      << seq.sampler().gain() << " " << seq.sampler().path() << "\n";

    for (int c = 0; c < seq.numChannels(); ++c) {
        f << "chan " << c << " " << seq.channelVolume(c) << " " << (seq.channelMute(c) ? 1 : 0)
          << " " << (seq.channelSolo(c) ? 1 : 0) << "\n";
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
                    f << "step " << p << " " << c << " " << s << "\n";
                }
            }
        }
        for (const Note& n : seq.roll().notes()) {
            f << "note " << p << " " << n.startStep << " " << n.lengthSteps << " " << n.pitch << " "
              << n.velocity << "\n";
        }
    }
    seq.selectPattern(savedCurrent);

    f << "master " << mixer.masterGain() << "\n";
    f << "fx eq " << (mixer.eq().enabled() ? 1 : 0) << " " << mixer.eq().cutoff() << "\n";
    f << "fx comp " << (mixer.compressor().enabled() ? 1 : 0) << " "
      << mixer.compressor().thresholdDb() << " " << mixer.compressor().ratio() << " "
      << mixer.compressor().attackMs() << " " << mixer.compressor().releaseMs() << " "
      << mixer.compressor().makeupDb() << "\n";
    f << "fx delay " << (mixer.delay().enabled() ? 1 : 0) << " " << mixer.delay().time() << " "
      << mixer.delay().feedback() << " " << mixer.delay().mix() << "\n";
    f << "fx reverb " << (mixer.reverb().enabled() ? 1 : 0) << " " << mixer.reverb().roomSize()
      << " " << mixer.reverb().damping() << " " << mixer.reverb().mix() << "\n";

    for (int i = 0; i < Automation::count(); ++i) {
        const AutoLane& lane = automation.lane(i);
        f << "auto " << i << " " << (lane.enabled ? 1 : 0) << " "
          << static_cast<int>(lane.lfo.shape) << " " << lane.lfo.rateHz << " " << lane.lo << " "
          << lane.hi << "\n";
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
        } else if (tag == "busgain") {
            float d = 1.0f;
            float s = 1.0f;
            ls >> d >> s;
            seq.setDrumGain(d);
            seq.setSynthGain(s);
        } else if (tag == "synth") {
            int mode = 0, wave = 0;
            float atk = 0.005f, dec = 0.08f, sus = 0.6f, rel = 0.12f, ratio = 2.0f, index = 3.0f,
                  gain = 0.28f;
            ls >> mode >> wave >> atk >> dec >> sus >> rel >> ratio >> index >> gain;
            SynthInstrument& syn = seq.synth();
            syn.setMode(mode == 1 ? SynthMode::FM : SynthMode::Subtractive);
            syn.setWaveform(static_cast<Waveform>(wave < 0 || wave > 3 ? 0 : wave));
            syn.setEnvelope(atk, dec, sus, rel);
            syn.setFmRatio(ratio);
            syn.setFmIndex(index);
            syn.setGain(gain);
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
        } else if (tag == "chan") {
            int c = -1;
            float vol = 1.0f;
            int mute = 0;
            int solo = 0;
            ls >> c >> vol >> mute >> solo;
            seq.setChannelVolume(c, vol);
            seq.setChannelMute(c, mute != 0);
            seq.setChannelSolo(c, solo != 0);
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
            seq.setStep(c, s, true);
        } else if (tag == "note") {
            int p = 0;
            Note n;
            ls >> p >> n.startStep >> n.lengthSteps >> n.pitch >> n.velocity;
            seq.selectPattern(p);
            seq.roll().addNote(n);
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
            } else if (which == "reverb") {
                float room = 0.7f, damp = 0.35f, mix = 0.25f;
                ls >> room >> damp >> mix;
                mixer.reverb().setEnabled(en != 0);
                mixer.reverb().setRoomSize(room);
                mixer.reverb().setDamping(damp);
                mixer.reverb().setMix(mix);
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
