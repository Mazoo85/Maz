#include "maz/audio/Automation.hpp"

#include "maz/audio/AudioEngine.hpp"

namespace maz::audio {

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
    case AutoTarget::Count:
        break;
    }
    return "?";
}

bool Automation::anyEnabled() const {
    for (const AutoLane& l : lanes_) {
        if (l.enabled) {
            return true;
        }
    }
    return false;
}

void Automation::apply(AudioEngine& engine, double timeSeconds) {
    for (int i = 0; i < count(); ++i) {
        const AutoLane& l = lane(i);
        if (!l.enabled) {
            continue;
        }
        const float u = l.lfo.valueUnipolar(timeSeconds);
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
        case AutoTarget::Count:
            break;
        }
    }
}

} // namespace maz::audio
