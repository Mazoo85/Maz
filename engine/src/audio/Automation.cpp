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
        case AutoTarget::Count:
            break;
        }
    }
}

} // namespace maz::audio
