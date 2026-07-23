#include "maz/audio/Vst3Host.hpp"

#include "maz/core/Log.hpp"

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wshadow"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstcomponent.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cstring>
#include <dlfcn.h>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace maz::audio {

namespace {
using GetFactoryProc = IPluginFactory* (PLUGIN_API*)();
using ModuleEntryProc = bool (PLUGIN_API*)(void*);
using ModuleExitProc = bool (PLUGIN_API*)();

IPluginFactory* factoryOf(void* p) { return static_cast<IPluginFactory*>(p); }
IComponent* componentOf(void* p) { return static_cast<IComponent*>(p); }
IAudioProcessor* processorOf(void* p) { return static_cast<IAudioProcessor*>(p); }

// A minimal host-side IEventList: a plain buffer of VST3 events handed to the plugin's process() as
// data.inputEvents. Ref-counting is a no-op — the list lives on the stack for the duration of one
// process() call (a static singleton lifetime, exactly like the plugin factories in the examples).
class HostEventList : public IEventList {
public:
    std::vector<Event> events;
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) SMTG_OVERRIDE {
        if (FUnknownPrivate::iidEqual(_iid, FUnknown_iid) ||
            FUnknownPrivate::iidEqual(_iid, IEventList_iid)) {
            *obj = static_cast<IEventList*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() SMTG_OVERRIDE { return 1000; }
    uint32 PLUGIN_API release() SMTG_OVERRIDE { return 1000; }
    int32 PLUGIN_API getEventCount() SMTG_OVERRIDE { return static_cast<int32>(events.size()); }
    tresult PLUGIN_API getEvent(int32 index, Event& e) SMTG_OVERRIDE {
        if (index < 0 || index >= static_cast<int32>(events.size())) {
            return kInvalidArgument;
        }
        e = events[static_cast<size_t>(index)];
        return kResultOk;
    }
    tresult PLUGIN_API addEvent(Event& e) SMTG_OVERRIDE {
        events.push_back(e);
        return kResultOk;
    }
};

// A one-parameter, one-point value queue: carries a single automation value for a ParamID.
class HostParamValueQueue : public IParamValueQueue {
public:
    ParamID id = 0;
    ParamValue value = 0.0;
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) SMTG_OVERRIDE {
        if (FUnknownPrivate::iidEqual(_iid, FUnknown_iid) ||
            FUnknownPrivate::iidEqual(_iid, IParamValueQueue_iid)) {
            *obj = static_cast<IParamValueQueue*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() SMTG_OVERRIDE { return 1000; }
    uint32 PLUGIN_API release() SMTG_OVERRIDE { return 1000; }
    ParamID PLUGIN_API getParameterId() SMTG_OVERRIDE { return id; }
    int32 PLUGIN_API getPointCount() SMTG_OVERRIDE { return 1; }
    tresult PLUGIN_API getPoint(int32 index, int32& sampleOffset, ParamValue& val) SMTG_OVERRIDE {
        if (index != 0) {
            return kInvalidArgument;
        }
        sampleOffset = 0;
        val = value;
        return kResultOk;
    }
    tresult PLUGIN_API addPoint(int32, ParamValue, int32&) SMTG_OVERRIDE { return kNotImplemented; }
};

// Host-side IParameterChanges: one queue per queued parameter change, handed to process().
class HostParameterChanges : public IParameterChanges {
public:
    std::vector<HostParamValueQueue> queues;
    tresult PLUGIN_API queryInterface(const TUID _iid, void** obj) SMTG_OVERRIDE {
        if (FUnknownPrivate::iidEqual(_iid, FUnknown_iid) ||
            FUnknownPrivate::iidEqual(_iid, IParameterChanges_iid)) {
            *obj = static_cast<IParameterChanges*>(this);
            return kResultOk;
        }
        *obj = nullptr;
        return kNoInterface;
    }
    uint32 PLUGIN_API addRef() SMTG_OVERRIDE { return 1000; }
    uint32 PLUGIN_API release() SMTG_OVERRIDE { return 1000; }
    int32 PLUGIN_API getParameterCount() SMTG_OVERRIDE { return static_cast<int32>(queues.size()); }
    IParamValueQueue* PLUGIN_API getParameterData(int32 index) SMTG_OVERRIDE {
        if (index < 0 || index >= static_cast<int32>(queues.size())) {
            return nullptr;
        }
        return &queues[static_cast<size_t>(index)];
    }
    IParamValueQueue* PLUGIN_API addParameterData(const ParamID&, int32&) SMTG_OVERRIDE {
        return nullptr;
    }
};
} // namespace

Vst3Host::~Vst3Host() { unload(); }

bool Vst3Host::hasEventInput() const {
    if (component_ == nullptr) {
        return false;
    }
    // An instrument declares one or more event (note/MIDI) input buses; a pure audio effect declares
    // none. Query the component's kEvent/kInput bus count.
    return componentOf(component_)->getBusCount(kEvent, kInput) > 0;
}

void Vst3Host::noteOn(int key, float velocity) {
    pendingNotes_.push_back({key, velocity, true});
    if (std::find(heldKeys_.begin(), heldKeys_.end(), key) == heldKeys_.end()) {
        heldKeys_.push_back(key);
    }
}

void Vst3Host::noteOff(int key) {
    pendingNotes_.push_back({key, 0.0f, false});
    heldKeys_.erase(std::remove(heldKeys_.begin(), heldKeys_.end(), key), heldKeys_.end());
}

void Vst3Host::allNotesOff() {
    for (int key : heldKeys_) {
        pendingNotes_.push_back({key, 0.0f, false});
    }
    heldKeys_.clear();
}

namespace {
IEditController* controllerOf(void* p) { return static_cast<IEditController*>(p); }
} // namespace

int Vst3Host::paramCount() const {
    return controller_ != nullptr ? static_cast<int>(controllerOf(controller_)->getParameterCount()) : 0;
}

void Vst3Host::setParam(int index, double value) {
    // Map the display index to a ParamID via the controller when present; otherwise assume id == index
    // (true for the simple example synth). VST3 parameter values are normalized [0,1].
    unsigned int id = static_cast<unsigned int>(index < 0 ? 0 : index);
    if (controller_ != nullptr) {
        ParameterInfo info{};
        if (controllerOf(controller_)->getParameterInfo(index, info) == kResultOk) {
            id = info.id;
        }
    }
    const double v = value < 0.0 ? 0.0 : (value > 1.0 ? 1.0 : value);
    pendingParams_.push_back({id, v});
}

double Vst3Host::paramValue(int index) const {
    if (controller_ == nullptr) {
        return 0.0;
    }
    ParameterInfo info{};
    if (controllerOf(controller_)->getParameterInfo(index, info) != kResultOk) {
        return 0.0;
    }
    return controllerOf(controller_)->getParamNormalized(info.id);
}

double Vst3Host::paramMin(int) const { return 0.0; } // VST3 params are normalized [0,1]
double Vst3Host::paramMax(int) const { return 1.0; }

std::string Vst3Host::paramName(int index) const {
    if (controller_ == nullptr) {
        return "";
    }
    ParameterInfo info{};
    if (controllerOf(controller_)->getParameterInfo(index, info) != kResultOk) {
        return "";
    }
    // title is a UTF-16 String128; convert the ASCII subset for the UI.
    std::string out;
    for (int i = 0; i < 128 && info.title[i] != 0; ++i) {
        out.push_back(static_cast<char>(info.title[i] & 0x7F));
    }
    return out;
}

bool Vst3Host::load(const std::string& path, int sampleRate, int maxBlock, std::string* err) {
    unload();
    const auto fail = [&](const std::string& m) {
        if (err != nullptr) {
            *err = m;
        }
        unload();
        return false;
    };

    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_ == nullptr) {
        const char* e = dlerror(); // read once — a second dlerror() would return null
        return fail(std::string("dlopen failed: ") + (e != nullptr ? e : "?"));
    }

    // Optional module init (Linux VST3 modules expose ModuleEntry/ModuleExit). ModuleEntry returns
    // false when the module's global init failed — bail out instead of using an uninitialized module,
    // and only record moduleEntered_ on success so unload() doesn't call ModuleExit against a module
    // that never entered (an unbalanced init/exit).
    if (auto entry = reinterpret_cast<ModuleEntryProc>(dlsym(handle_, "ModuleEntry"))) {
        if (!entry(handle_)) {
            return fail("VST3 ModuleEntry failed (module global init error)");
        }
        moduleEntered_ = true;
    }

    auto getFactory = reinterpret_cast<GetFactoryProc>(dlsym(handle_, "GetPluginFactory"));
    if (getFactory == nullptr) {
        return fail("GetPluginFactory not found (not a VST3 module?)");
    }
    IPluginFactory* factory = getFactory();
    if (factory == nullptr) {
        return fail("GetPluginFactory returned null");
    }
    factory_ = factory;

    // Find the first audio-effect class and instantiate its IComponent.
    IComponent* component = nullptr;
    const int32 count = factory->countClasses();
    for (int32 i = 0; i < count; ++i) {
        PClassInfo ci;
        if (factory->getClassInfo(i, &ci) != kResultOk) {
            continue;
        }
        if (std::strcmp(ci.category, kVstAudioEffectClass) != 0) {
            continue;
        }
        void* obj = nullptr;
        if (factory->createInstance(ci.cid, IComponent_iid, &obj) == kResultOk && obj != nullptr) {
            component = static_cast<IComponent*>(obj);
            name_ = ci.name;
            break;
        }
    }
    if (component == nullptr) {
        return fail("no instantiable audio-effect class in the module");
    }
    component_ = component;

    if (component->initialize(nullptr) != kResultOk) {
        return fail("IComponent::initialize failed");
    }

    // Query the audio processor interface.
    void* proc = nullptr;
    if (component->queryInterface(IAudioProcessor_iid, &proc) != kResultOk || proc == nullptr) {
        return fail("plugin exposes no IAudioProcessor");
    }
    processor_ = proc;
    IAudioProcessor* processor = processorOf(processor_);

    // Optional: an IEditController for parameter enumeration. Single-component plugins expose it on the
    // component itself; a null controller just means no parameter names (delivery still works by index).
    void* ctrl = nullptr;
    if (component->queryInterface(IEditController_iid, &ctrl) == kResultOk && ctrl != nullptr) {
        controller_ = ctrl;
    }

    if (processor->canProcessSampleSize(kSample32) != kResultTrue) {
        return fail("plugin cannot process 32-bit float");
    }

    maxBlock_ = maxBlock > 0 ? maxBlock : 4096;
    ProcessSetup setup{};
    setup.processMode = kRealtime;
    setup.symbolicSampleSize = kSample32;
    setup.maxSamplesPerBlock = maxBlock_;
    setup.sampleRate = sampleRate > 0 ? static_cast<double>(sampleRate) : 48000.0;
    if (processor->setupProcessing(setup) != kResultOk) {
        return fail("setupProcessing failed");
    }

    component->setActive(true);
    processor->setProcessing(true);
    active_ = true;

    inL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    inR_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    outL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    outR_.assign(static_cast<size_t>(maxBlock_), 0.0f);

    MAZ_LOG_INFO("vst3: loaded '%s' from %s", name_.c_str(), path.c_str());
    return true;
}

void Vst3Host::unload() {
    if (processor_ != nullptr && active_) {
        processorOf(processor_)->setProcessing(false);
    }
    if (component_ != nullptr && active_) {
        componentOf(component_)->setActive(false);
    }
    active_ = false;
    if (controller_ != nullptr) {
        controllerOf(controller_)->release();
        controller_ = nullptr;
    }
    if (processor_ != nullptr) {
        processorOf(processor_)->release();
        processor_ = nullptr;
    }
    if (component_ != nullptr) {
        componentOf(component_)->terminate();
        componentOf(component_)->release();
        component_ = nullptr;
    }
    if (factory_ != nullptr) {
        factoryOf(factory_)->release();
        factory_ = nullptr;
    }
    if (handle_ != nullptr) {
        if (moduleEntered_) {
            if (auto exit = reinterpret_cast<ModuleExitProc>(dlsym(handle_, "ModuleExit"))) {
                exit();
            }
            moduleEntered_ = false;
        }
        dlclose(handle_);
        handle_ = nullptr;
    }
    name_.clear();
}

void Vst3Host::process(float* stereo, int frames, int sampleRate) {
    if (!enabled() || processor_ == nullptr || frames <= 0) {
        return;
    }
    (void)sampleRate;
    IAudioProcessor* processor = processorOf(processor_);

    // Drain queued note events into a VST3 event list (instrument hosting). It is attached to the
    // first sub-block only, so each note fires exactly once. With no queued notes the list stays empty
    // and inputEvents is left null — the audio-effect path is then bit-identical to before.
    HostEventList events;
    for (const auto& pn : pendingNotes_) {
        Event ev{};
        ev.busIndex = 0;
        ev.sampleOffset = 0;
        ev.ppqPosition = 0.0;
        ev.flags = 0;
        if (pn.on) {
            ev.type = Event::kNoteOnEvent;
            ev.noteOn.channel = 0;
            ev.noteOn.pitch = static_cast<int16>(pn.key);
            ev.noteOn.tuning = 0.0f;
            ev.noteOn.velocity = pn.velocity;
            ev.noteOn.length = 0;
            ev.noteOn.noteId = -1;
        } else {
            ev.type = Event::kNoteOffEvent;
            ev.noteOff.channel = 0;
            ev.noteOff.pitch = static_cast<int16>(pn.key);
            ev.noteOff.velocity = 0.0f;
            ev.noteOff.noteId = -1;
            ev.noteOff.tuning = 0.0f;
        }
        events.events.push_back(ev);
    }
    pendingNotes_.clear();
    // Build the queued parameter changes (one queue per change) for the first sub-block.
    HostParameterChanges paramChanges;
    for (const auto& pp : pendingParams_) {
        HostParamValueQueue q;
        q.id = pp.id;
        q.value = pp.value;
        paramChanges.queues.push_back(q);
    }
    pendingParams_.clear();
    bool eventsFed = false;

    int done = 0;
    while (done < frames) {
        const int n = std::min(frames - done, maxBlock_);
        // Deinterleave into the plugin's L/R input buffers.
        for (int i = 0; i < n; ++i) {
            inL_[static_cast<size_t>(i)] = stereo[2 * (done + i)];
            inR_[static_cast<size_t>(i)] = stereo[2 * (done + i) + 1];
        }

        float* inChans[2] = {inL_.data(), inR_.data()};
        float* outChans[2] = {outL_.data(), outR_.data()};
        AudioBusBuffers inBus{};
        inBus.numChannels = 2;
        inBus.silenceFlags = 0;
        inBus.channelBuffers32 = inChans;
        AudioBusBuffers outBus{};
        outBus.numChannels = 2;
        outBus.silenceFlags = 0;
        outBus.channelBuffers32 = outChans;

        ProcessData data{};
        data.processMode = kRealtime;
        data.symbolicSampleSize = kSample32;
        data.numSamples = n;
        data.numInputs = 1;
        data.numOutputs = 1;
        data.inputs = &inBus;
        data.outputs = &outBus;
        // Deliver queued note events + parameter changes on the first sub-block only.
        if (!eventsFed) {
            if (!events.events.empty()) {
                data.inputEvents = &events;
            }
            if (!paramChanges.queues.empty()) {
                data.inputParameterChanges = &paramChanges;
            }
        }
        eventsFed = true;

        if (processor->process(data) == kResultOk) {
            for (int i = 0; i < n; ++i) {
                stereo[2 * (done + i)] = outL_[static_cast<size_t>(i)];
                stereo[2 * (done + i) + 1] = outR_[static_cast<size_t>(i)];
            }
        }
        done += n;
    }
}

} // namespace maz::audio
