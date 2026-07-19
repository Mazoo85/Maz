#include "maz/audio/Automation.hpp"

#include "maz/audio/AudioEngine.hpp"

#include <algorithm>

namespace maz::audio {

float AutoLane::sourceUnipolar(double t) const {
    if (clip.empty()) {
        return lfo.valueUnipolar(t);
    }
    // Loop the clip time if a positive length is set, otherwise clamp/hold past the last point.
    double ct = t;
    if (clipLength > 0.0) {
        ct = t - clipLength * std::floor(t / clipLength);
    }
    // Before the first point → hold the first value; after the last → hold the last value.
    if (ct <= clip.front().time) {
        return clip.front().value;
    }
    if (ct >= clip.back().time) {
        return clip.back().value;
    }
    // Find the segment [a, b) containing ct and interpolate linearly.
    for (size_t i = 1; i < clip.size(); ++i) {
        if (ct < clip[i].time) {
            const AutoPoint& a = clip[i - 1];
            const AutoPoint& b = clip[i];
            const double span = b.time - a.time;
            const float frac = span > 0.0 ? static_cast<float>((ct - a.time) / span) : 0.0f;
            return a.value + (b.value - a.value) * frac;
        }
    }
    return clip.back().value;
}

Automation::Automation() {
    // Sensible default sweep ranges per target (used once a lane is enabled).
    lane(AutoTarget::FilterCutoff).lo = 400.0f;
    lane(AutoTarget::FilterCutoff).hi = 6000.0f;
    lane(AutoTarget::FmIndex).lo = 0.0f;
    lane(AutoTarget::FmIndex).hi = 8.0f;
    lane(AutoTarget::ReverbMix).lo = 0.0f;
    lane(AutoTarget::ReverbMix).hi = 0.5f;
    lane(AutoTarget::MasterGain).lo = 0.4f;
    lane(AutoTarget::MasterGain).hi = 1.0f;
    lane(AutoTarget::DelayMix).lo = 0.0f;
    lane(AutoTarget::DelayMix).hi = 0.6f;
    lane(AutoTarget::DistDrive).lo = 1.0f;
    lane(AutoTarget::DistDrive).hi = 10.0f;
    lane(AutoTarget::StereoWidth).lo = 0.0f;
    lane(AutoTarget::StereoWidth).hi = 2.0f;
    lane(AutoTarget::SynthCutoff).lo = 200.0f;
    lane(AutoTarget::SynthCutoff).hi = 8000.0f;
    lane(AutoTarget::FilterResonance).lo = 0.7f;
    lane(AutoTarget::FilterResonance).hi = 12.0f;
    lane(AutoTarget::LeadVolume).lo = 0.0f;
    lane(AutoTarget::LeadVolume).hi = 1.0f;
    lane(AutoTarget::LeadPan).lo = -1.0f;
    lane(AutoTarget::LeadPan).hi = 1.0f;
    lane(AutoTarget::ReverbSend).lo = 0.0f;
    lane(AutoTarget::ReverbSend).hi = 0.8f;
    lane(AutoTarget::DelaySend).lo = 0.0f;
    lane(AutoTarget::DelaySend).hi = 0.7f;
    lane(AutoTarget::MasterPan).lo = -1.0f;
    lane(AutoTarget::MasterPan).hi = 1.0f;
    lane(AutoTarget::BassVolume).lo = 0.0f;
    lane(AutoTarget::BassVolume).hi = 1.0f;
    lane(AutoTarget::BassPan).lo = -1.0f;
    lane(AutoTarget::BassPan).hi = 1.0f;
    // A gentle default rate on each.
    for (int i = 0; i < count(); ++i) {
        lane(i).lfo.rateHz = 0.5f;
    }
}

const char* Automation::targetName(AutoTarget t) {
    switch (t) {
    case AutoTarget::FilterCutoff:
        return "Filter Cutoff";
    case AutoTarget::FmIndex:
        return "FM Index";
    case AutoTarget::ReverbMix:
        return "Reverb Mix";
    case AutoTarget::MasterGain:
        return "Master Gain";
    case AutoTarget::DelayMix:
        return "Delay Mix";
    case AutoTarget::DistDrive:
        return "Distortion Drive";
    case AutoTarget::StereoWidth:
        return "Stereo Width";
    case AutoTarget::SynthCutoff:
        return "Synth Cutoff";
    case AutoTarget::FilterResonance:
        return "Filter Reso";
    case AutoTarget::LeadVolume:
        return "Lead Volume";
    case AutoTarget::LeadPan:
        return "Lead Pan";
    case AutoTarget::ReverbSend:
        return "Reverb Send";
    case AutoTarget::DelaySend:
        return "Delay Send";
    case AutoTarget::MasterPan:
        return "Master Pan";
    case AutoTarget::BassVolume:
        return "Bass Volume";
    case AutoTarget::BassPan:
        return "Bass Pan";
    case AutoTarget::Count:
        break;
    }
    return "?";
}

namespace {
// Each automation sync division as LFO cycles per beat (a quarter note = 1 cycle per beat).
constexpr float kAutoCyclesPerBeat[Automation::kSyncDivisions] = {
    1.0f / 16.0f, // 4 bars
    1.0f / 8.0f,  // 2 bars
    1.0f / 4.0f,  // 1 bar
    0.5f,         // 1/2
    1.0f,         // 1/4
    2.0f,         // 1/8
};
constexpr const char* kAutoDivName[Automation::kSyncDivisions] = {
    "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/8",
};
} // namespace

const char* Automation::syncDivisionName(int div) {
    if (div < 0 || div >= kSyncDivisions) {
        return "?";
    }
    return kAutoDivName[div];
}

float Automation::syncRateHz(int div, double bpm) {
    if (div < 0 || div >= kSyncDivisions || bpm <= 0.0) {
        return 1.0f;
    }
    return static_cast<float>(bpm / 60.0 * static_cast<double>(kAutoCyclesPerBeat[div]));
}

bool Automation::anyEnabled() const {
    for (const AutoLane& l : lanes_) {
        if (l.enabled) {
            return true;
        }
    }
    return false;
}

void Automation::apply(AudioEngine& engine, double timeSeconds, double bpm) {
    for (int i = 0; i < count(); ++i) {
        AutoLane& l = lane(i);
        if (!l.enabled) {
            continue;
        }
        // Tempo sync: lock the LFO rate to the transport (only when the lane uses its LFO, i.e. no
        // drawn clip is overriding it).
        if (l.sync && bpm > 0.0 && l.clip.empty()) {
            l.lfo.rateHz = syncRateHz(l.syncDiv, bpm);
        }
        const float u = l.sourceUnipolar(timeSeconds);
        const float v = l.lo + u * (l.hi - l.lo);
        switch (static_cast<AutoTarget>(i)) {
        case AutoTarget::FilterCutoff:
            engine.mixer().eq().setEnabled(true);
            engine.mixer().eq().setCutoff(v);
            break;
        case AutoTarget::FmIndex:
            engine.sequencer().synth().setFmIndex(v);
            break;
        case AutoTarget::ReverbMix:
            engine.mixer().reverb().setEnabled(true);
            engine.mixer().reverb().setMix(v);
            break;
        case AutoTarget::MasterGain:
            engine.mixer().setMasterGain(v);
            break;
        case AutoTarget::DelayMix:
            engine.mixer().delay().setEnabled(true);
            engine.mixer().delay().setMix(v);
            break;
        case AutoTarget::DistDrive:
            engine.mixer().distortion().setEnabled(true);
            engine.mixer().distortion().setDrive(v);
            break;
        case AutoTarget::StereoWidth:
            engine.mixer().widener().setEnabled(true);
            engine.mixer().widener().setWidth(v);
            break;
        case AutoTarget::SynthCutoff: {
            // Sweep the lead synth's own resonant filter cutoff (the classic lead filter automation),
            // preserving its resonance and envelope amount.
            SynthInstrument& syn = engine.sequencer().synth();
            syn.setFilter(v, syn.filterResonance(), syn.filterEnvAmount());
            break;
        }
        case AutoTarget::FilterResonance: {
            // Sweep the lead synth's filter resonance (Q) — screaming acid builds — preserving the
            // cutoff and envelope amount.
            SynthInstrument& syn = engine.sequencer().synth();
            syn.setFilter(syn.filterCutoff(), v, syn.filterEnvAmount());
            break;
        }
        case AutoTarget::LeadVolume:
            // Sweep the lead mixer strip's gain (bus-level tremolo / volume rides). Touching the
            // track makes it active, so the per-bus stem path engages automatically.
            engine.mixer().track(MixerBus::Lead).setGain(v);
            break;
        case AutoTarget::LeadPan:
            // Sweep the lead mixer strip's stereo balance (auto-pan on the lead bus only).
            engine.mixer().track(MixerBus::Lead).setPan(v);
            break;
        case AutoTarget::ReverbSend:
            // Sweep the parallel reverb send level (risers/build-ups) — the return bus is always
            // wet, so the send alone gates how much signal is fed into the reverb tail.
            engine.mixer().setReverbSend(v);
            break;
        case AutoTarget::DelaySend:
            // Sweep the parallel delay send level (throw-style dub delays on the fly).
            engine.mixer().setDelaySend(v);
            break;
        case AutoTarget::MasterPan:
            // Sweep the master output balance (whole-mix auto-pan).
            engine.mixer().setMasterBalance(v);
            break;
        case AutoTarget::BassVolume:
            // Sweep the bass mixer strip's gain (bus volume rides on the bass).
            engine.mixer().track(MixerBus::Bass).setGain(v);
            break;
        case AutoTarget::BassPan:
            // Sweep the bass mixer strip's stereo balance (auto-pan on the bass bus only).
            engine.mixer().track(MixerBus::Bass).setPan(v);
            break;
        case AutoTarget::Count:
            break;
        }
    }
}

} // namespace maz::audio
