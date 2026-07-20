// A minimal, self-contained VST3 *instrument* plugin: a monophonic sine synth driven by note events.
//
// It is the instrument counterpart to plugins/example_vst3 (the tremolo effect): where the effect
// declares an audio input bus and modulates incoming audio, this instrument declares an *event*
// (note) input bus plus an audio *output* bus and synthesises sound from note-on / note-off events.
// It exists so maz::audio::Vst3Host can be exercised end-to-end as an instrument host — first the
// hasEventInput() detection (this file makes it report true), and next the note-event delivery path.
//
// Like the tremolo it is hand-written against the MIT-licensed VST3 `pluginterfaces` headers only
// (no GPL `public.sdk`), implementing just enough of the COM-style API (FUnknown + IPluginBase +
// IComponent + IAudioProcessor + an IPluginFactory) to be instantiated and to process 32-bit float.

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivstevents.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <cmath>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

// This plugin's unique class id (arbitrary but fixed). "MazSynthVST3" packed into a 128-bit UID.
static const TUID kMazSynthCID = INLINE_UID(0x4D617A53, 0x796E7468, 0x56535433, 0x496E7374);

constexpr double kTwoPi = 6.283185307179586;

// The instrument. It is both the processing component and the audio processor.
class MazSynth : public IComponent, public IAudioProcessor {
public:
    MazSynth() = default;
    virtual ~MazSynth() = default;

    //--- FUnknown -----------------------------------------------------------
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) SMTG_OVERRIDE {
        if (FUnknownPrivate::iidEqual(_iid, FUnknown_iid) ||
            FUnknownPrivate::iidEqual(_iid, IPluginBase_iid) ||
            FUnknownPrivate::iidEqual(_iid, IComponent_iid)) {
            addRef();
            *obj = static_cast<IComponent*>(this);
            return kResultOk;
        }
        if (FUnknownPrivate::iidEqual(_iid, IAudioProcessor_iid)) {
            addRef();
            *obj = static_cast<IAudioProcessor*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() SMTG_OVERRIDE { return static_cast<uint32>(++refCount_); }
    uint32 PLUGIN_API release() SMTG_OVERRIDE {
        if (--refCount_ == 0) {
            delete this;
            return 0;
        }
        return static_cast<uint32>(refCount_);
    }

    //--- IPluginBase --------------------------------------------------------
    tresult PLUGIN_API initialize(FUnknown* /*context*/) SMTG_OVERRIDE { return kResultOk; }
    tresult PLUGIN_API terminate() SMTG_OVERRIDE { return kResultOk; }

    //--- IComponent ---------------------------------------------------------
    tresult PLUGIN_API getControllerClassId(TUID /*classId*/) SMTG_OVERRIDE { return kNotImplemented; }
    tresult PLUGIN_API setIoMode(IoMode /*mode*/) SMTG_OVERRIDE { return kResultOk; }
    int32 PLUGIN_API getBusCount(MediaType type, BusDirection dir) SMTG_OVERRIDE {
        // One event (note) input bus, one stereo audio output bus, no audio input — this is what makes
        // it an instrument rather than an effect.
        if (type == kEvent) {
            return dir == kInput ? 1 : 0;
        }
        if (type == kAudio) {
            return dir == kOutput ? 1 : 0;
        }
        return 0;
    }
    tresult PLUGIN_API getBusInfo(MediaType type, BusDirection dir, int32 index,
                                  BusInfo& bus) SMTG_OVERRIDE {
        if (type == kEvent && dir == kInput && index == 0) {
            bus.mediaType = kEvent;
            bus.direction = kInput;
            bus.channelCount = 1;
            bus.busType = kMain;
            bus.flags = BusInfo::kDefaultActive;
            bus.name[0] = 0;
            return kResultOk;
        }
        if (type == kAudio && dir == kOutput && index == 0) {
            bus.mediaType = kAudio;
            bus.direction = kOutput;
            bus.channelCount = 2;
            bus.busType = kMain;
            bus.flags = BusInfo::kDefaultActive;
            bus.name[0] = 0;
            return kResultOk;
        }
        return kInvalidArgument;
    }
    tresult PLUGIN_API getRoutingInfo(RoutingInfo& /*in*/, RoutingInfo& /*out*/) SMTG_OVERRIDE {
        return kNotImplemented;
    }
    tresult PLUGIN_API activateBus(MediaType /*type*/, BusDirection /*dir*/, int32 /*index*/,
                                   TBool /*state*/) SMTG_OVERRIDE {
        return kResultOk;
    }
    tresult PLUGIN_API setActive(TBool /*state*/) SMTG_OVERRIDE { return kResultOk; }
    tresult PLUGIN_API setState(IBStream* /*state*/) SMTG_OVERRIDE { return kResultOk; }
    tresult PLUGIN_API getState(IBStream* /*state*/) SMTG_OVERRIDE { return kResultOk; }

    //--- IAudioProcessor ----------------------------------------------------
    tresult PLUGIN_API setBusArrangements(SpeakerArrangement* /*inputs*/, int32 /*numIns*/,
                                          SpeakerArrangement* /*outputs*/,
                                          int32 /*numOuts*/) SMTG_OVERRIDE {
        return kResultOk;
    }
    tresult PLUGIN_API getBusArrangement(BusDirection /*dir*/, int32 /*index*/,
                                         SpeakerArrangement& arr) SMTG_OVERRIDE {
        arr = 0x3; // kStereo (L | R)
        return kResultOk;
    }
    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE {
        return symbolicSampleSize == kSample32 ? kResultTrue : kResultFalse;
    }
    uint32 PLUGIN_API getLatencySamples() SMTG_OVERRIDE { return 0; }
    tresult PLUGIN_API setupProcessing(ProcessSetup& setup) SMTG_OVERRIDE {
        sampleRate_ = setup.sampleRate > 0 ? setup.sampleRate : 48000.0;
        return kResultOk;
    }
    tresult PLUGIN_API setProcessing(TBool /*state*/) SMTG_OVERRIDE { return kResultOk; }
    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE {
        if (data.numOutputs < 1 || data.symbolicSampleSize != kSample32) {
            return kResultOk;
        }

        // Apply incoming note events (monophonic: the latest note-on wins, note-off gates it off).
        if (data.inputEvents != nullptr) {
            const int32 nev = data.inputEvents->getEventCount();
            for (int32 e = 0; e < nev; ++e) {
                Event ev{};
                if (data.inputEvents->getEvent(e, ev) != kResultOk) {
                    continue;
                }
                if (ev.type == Event::kNoteOnEvent && ev.noteOn.velocity > 0.0f) {
                    freq_ = 440.0 * std::pow(2.0, (static_cast<double>(ev.noteOn.pitch) - 69.0) / 12.0);
                    gain_ = static_cast<double>(ev.noteOn.velocity);
                    gate_ = true;
                } else if (ev.type == Event::kNoteOffEvent) {
                    gate_ = false;
                }
            }
        }

        const int32 frames = data.numSamples;
        AudioBusBuffers& out = data.outputs[0];
        const int32 chans = out.numChannels;
        for (int32 c = 0; c < chans; ++c) {
            float* dst = out.channelBuffers32[c];
            if (dst == nullptr) {
                continue;
            }
            double phase = phase_;
            for (int32 i = 0; i < frames; ++i) {
                const float s = gate_ ? static_cast<float>(std::sin(phase * kTwoPi) * gain_) : 0.0f;
                dst[i] = s;
                phase += freq_ / sampleRate_;
                if (phase >= 1.0) {
                    phase -= 1.0;
                }
            }
        }
        // Advance the shared phase once for the block (channels stay in sync).
        if (gate_) {
            phase_ += (freq_ / sampleRate_) * static_cast<double>(frames);
            phase_ -= std::floor(phase_);
        }
        return kResultOk;
    }
    uint32 PLUGIN_API getTailSamples() SMTG_OVERRIDE { return 0; }

private:
    int refCount_ = 1;
    double sampleRate_ = 48000.0;
    double phase_ = 0.0;
    double freq_ = 440.0;
    double gain_ = 0.0;
    bool gate_ = false;
};

// The plugin factory: exposes the single MazSynth class.
class MazFactory : public IPluginFactory {
public:
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) SMTG_OVERRIDE {
        if (FUnknownPrivate::iidEqual(_iid, FUnknown_iid) ||
            FUnknownPrivate::iidEqual(_iid, IPluginFactory_iid)) {
            addRef();
            *obj = static_cast<IPluginFactory*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() SMTG_OVERRIDE { return 1000; } // static singleton
    uint32 PLUGIN_API release() SMTG_OVERRIDE { return 1000; }

    tresult PLUGIN_API getFactoryInfo(PFactoryInfo* info) SMTG_OVERRIDE {
        if (info == nullptr) {
            return kInvalidArgument;
        }
        std::memset(info, 0, sizeof(PFactoryInfo));
        std::strncpy(info->vendor, "Maz", sizeof(info->vendor) - 1);
        std::strncpy(info->url, "https://example.invalid", sizeof(info->url) - 1);
        info->flags = 0;
        return kResultOk;
    }
    int32 PLUGIN_API countClasses() SMTG_OVERRIDE { return 1; }
    tresult PLUGIN_API getClassInfo(int32 index, PClassInfo* info) SMTG_OVERRIDE {
        if (index != 0 || info == nullptr) {
            return kInvalidArgument;
        }
        std::memset(info, 0, sizeof(PClassInfo));
        std::memcpy(info->cid, kMazSynthCID, sizeof(TUID));
        info->cardinality = PClassInfo::kManyInstances;
        // VST3 instruments still register under the audio-module class category; the event bus (and,
        // in a full plugin, the "Instrument" subcategory) is what marks it as a synth.
        std::strncpy(info->category, kVstAudioEffectClass, sizeof(info->category) - 1);
        std::strncpy(info->name, "Maz Synth", sizeof(info->name) - 1);
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(FIDString cid, FIDString _iid, void** obj) SMTG_OVERRIDE {
        if (obj == nullptr) {
            return kInvalidArgument;
        }
        if (FUnknownPrivate::iidEqual(cid, kMazSynthCID)) {
            MazSynth* p = new MazSynth();
            const tresult r = p->queryInterface(_iid, obj);
            p->release();
            return r;
        }
        *obj = nullptr;
        return kNoInterface;
    }
};

MazFactory g_factory;

} // namespace

extern "C" {

// The VST3 module entry points (Linux uses ModuleEntry/ModuleExit; hosts also accept their absence).
SMTG_EXPORT_SYMBOL bool ModuleEntry(void* /*handle*/) { return true; }
SMTG_EXPORT_SYMBOL bool ModuleExit(void) { return true; }

// The required factory entry point every VST3 module exports.
SMTG_EXPORT_SYMBOL IPluginFactory* PLUGIN_API GetPluginFactory() {
    g_factory.addRef();
    return &g_factory;
}

} // extern "C"
