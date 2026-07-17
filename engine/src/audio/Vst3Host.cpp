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
} // namespace

Vst3Host::~Vst3Host() { unload(); }

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

    // Optional module init (Linux VST3 modules expose ModuleEntry/ModuleExit).
    if (auto entry = reinterpret_cast<ModuleEntryProc>(dlsym(handle_, "ModuleEntry"))) {
        entry(handle_);
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
