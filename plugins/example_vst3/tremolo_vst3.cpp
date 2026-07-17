// A minimal, self-contained VST3 audio-effect plugin: a stereo tremolo (amplitude LFO).
//
// It is hand-written directly against the MIT-licensed VST3 `pluginterfaces` headers — it does NOT
// use Steinberg's GPL `public.sdk` helper library — so both this plugin and the host that loads it
// stay free of GPL code. It implements just enough of the COM-style VST3 API (FUnknown +
// IPluginBase + IComponent + IAudioProcessor, plus an IPluginFactory) to be instantiated and to
// process 32-bit float audio, which is exactly what maz::audio::Vst3Host exercises.

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
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <cmath>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace {

// This plugin's unique class id (arbitrary but fixed). "MazTremoloVST3" packed into a 128-bit UID.
static const TUID kMazTremoloCID = INLINE_UID(0x4D617A54, 0x72656D6F, 0x6C6F5653, 0x54330001);

constexpr double kTwoPi = 6.283185307179586;

// The audio effect. It is both the processing component and the audio processor.
class MazTremolo : public IComponent, public IAudioProcessor {
public:
    MazTremolo() = default;
    virtual ~MazTremolo() = default;

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
    int32 PLUGIN_API getBusCount(MediaType type, BusDirection /*dir*/) SMTG_OVERRIDE {
        return type == kAudio ? 1 : 0; // one stereo audio in, one stereo audio out
    }
    tresult PLUGIN_API getBusInfo(MediaType type, BusDirection dir, int32 index,
                                  BusInfo& bus) SMTG_OVERRIDE {
        if (type != kAudio || index != 0) {
            return kInvalidArgument;
        }
        bus.mediaType = kAudio;
        bus.direction = dir;
        bus.channelCount = 2;
        bus.busType = kMain;
        bus.flags = BusInfo::kDefaultActive;
        bus.name[0] = 0;
        return kResultOk;
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
        if (data.numOutputs < 1 || data.numInputs < 1 || data.symbolicSampleSize != kSample32) {
            return kResultOk;
        }
        const int32 frames = data.numSamples;
        AudioBusBuffers& in = data.inputs[0];
        AudioBusBuffers& out = data.outputs[0];
        const int32 chans = out.numChannels < in.numChannels ? out.numChannels : in.numChannels;
        const double rateHz = 5.0; // 5 Hz tremolo
        for (int32 c = 0; c < chans; ++c) {
            const float* src = in.channelBuffers32[c];
            float* dst = out.channelBuffers32[c];
            double phase = phase_;
            for (int32 i = 0; i < frames; ++i) {
                // Amplitude modulation: gain swings between 0 and 1.
                const double g = 0.5 * (1.0 - std::cos(phase * kTwoPi));
                dst[i] = static_cast<float>(static_cast<double>(src[i]) * g);
                phase += rateHz / sampleRate_;
                if (phase >= 1.0) {
                    phase -= 1.0;
                }
            }
        }
        // Advance the shared phase once for the block (channels stay in sync).
        phase_ += (rateHz / sampleRate_) * static_cast<double>(frames);
        phase_ -= std::floor(phase_);
        return kResultOk;
    }
    uint32 PLUGIN_API getTailSamples() SMTG_OVERRIDE { return 0; }

private:
    int refCount_ = 1;
    double sampleRate_ = 48000.0;
    double phase_ = 0.0;
};

// The plugin factory: exposes the single MazTremolo class.
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
        std::memcpy(info->cid, kMazTremoloCID, sizeof(TUID));
        info->cardinality = PClassInfo::kManyInstances;
        std::strncpy(info->category, kVstAudioEffectClass, sizeof(info->category) - 1);
        std::strncpy(info->name, "Maz Tremolo", sizeof(info->name) - 1);
        return kResultOk;
    }
    tresult PLUGIN_API createInstance(FIDString cid, FIDString _iid, void** obj) SMTG_OVERRIDE {
        if (obj == nullptr) {
            return kInvalidArgument;
        }
        if (FUnknownPrivate::iidEqual(cid, kMazTremoloCID)) {
            MazTremolo* p = new MazTremolo();
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
