#include "maz/audio/ClapHost.hpp"

#include "maz/core/Log.hpp"

#include <clap/clap.h>

#include <algorithm>
#include <cstring>
#include <dlfcn.h>

namespace maz::audio {

namespace {

// Empty input/output event lists (no parameter/note events for this focused host).
uint32_t inEventsSize(const clap_input_events*) {
    return 0;
}
const clap_event_header_t* inEventsGet(const clap_input_events*, uint32_t) {
    return nullptr;
}
bool outEventsPush(const clap_output_events*, const clap_event_header_t*) {
    return false;
}

const clap_input_events_t g_inEvents = {nullptr, inEventsSize, inEventsGet};
const clap_output_events_t g_outEvents = {nullptr, outEventsPush};

// Minimal host callbacks.
const void* hostGetExtension(const clap_host*, const char*) {
    return nullptr;
}
void hostRequestRestart(const clap_host*) {}
void hostRequestProcess(const clap_host*) {}
void hostRequestCallback(const clap_host*) {}

const clap_host_t g_host = {
    CLAP_VERSION,
    nullptr, // host_data
    "CJC Music Station",
    "CJC",
    "https://example.invalid",
    "1.0",
    hostGetExtension,
    hostRequestRestart,
    hostRequestProcess,
    hostRequestCallback,
};

} // namespace

ClapHost::~ClapHost() {
    unload();
}

bool ClapHost::load(const std::string& path, int sampleRate, int maxBlock, std::string* err) {
    unload();
    maxBlock_ = std::max(maxBlock, 64);

    handle_ = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle_ == nullptr) {
        if (err != nullptr) {
            const char* e = dlerror();
            *err = "dlopen failed: " + std::string(e != nullptr ? e : "unknown");
        }
        return false;
    }

    const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(handle_, "clap_entry"));
    if (entry == nullptr || entry->init == nullptr || entry->get_factory == nullptr) {
        if (err != nullptr) {
            *err = "'" + path + "' has no clap_entry";
        }
        unload();
        return false;
    }
    entry_ = entry;
    if (!entry->init(path.c_str())) {
        if (err != nullptr) {
            *err = "clap_entry->init failed";
        }
        unload();
        return false;
    }

    const auto* factory =
        static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    if (factory == nullptr || factory->get_plugin_count(factory) == 0) {
        if (err != nullptr) {
            *err = "no CLAP plugin factory / empty factory";
        }
        unload();
        return false;
    }

    const clap_plugin_descriptor_t* desc = factory->get_plugin_descriptor(factory, 0);
    if (desc == nullptr) {
        if (err != nullptr) {
            *err = "null plugin descriptor";
        }
        unload();
        return false;
    }

    const clap_plugin_t* plugin = factory->create_plugin(factory, &g_host, desc->id);
    if (plugin == nullptr) {
        if (err != nullptr) {
            *err = "create_plugin failed";
        }
        unload();
        return false;
    }
    plugin_ = plugin;
    if (!plugin->init(plugin)) {
        if (err != nullptr) {
            *err = "plugin->init failed";
        }
        unload();
        return false;
    }
    name_ = desc->name != nullptr ? desc->name : "CLAP";

    if (!plugin->activate(plugin, static_cast<double>(sampleRate), 1,
                          static_cast<uint32_t>(maxBlock_))) {
        if (err != nullptr) {
            *err = "plugin->activate failed";
        }
        unload();
        return false;
    }
    activated_ = true;
    if (plugin->start_processing != nullptr) {
        processing_ = plugin->start_processing(plugin);
    }

    inL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    inR_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    outL_.assign(static_cast<size_t>(maxBlock_), 0.0f);
    outR_.assign(static_cast<size_t>(maxBlock_), 0.0f);

    MAZ_LOG_INFO("clap: loaded \"%s\" from %s", name_.c_str(), path.c_str());
    return true;
}

void ClapHost::unload() {
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    if (plugin != nullptr) {
        if (processing_ && plugin->stop_processing != nullptr) {
            plugin->stop_processing(plugin);
        }
        if (activated_ && plugin->deactivate != nullptr) {
            plugin->deactivate(plugin);
        }
        if (plugin->destroy != nullptr) {
            plugin->destroy(plugin);
        }
    }
    const auto* entry = static_cast<const clap_plugin_entry_t*>(entry_);
    if (entry != nullptr && entry->deinit != nullptr) {
        entry->deinit();
    }
    if (handle_ != nullptr) {
        dlclose(handle_);
    }
    plugin_ = nullptr;
    entry_ = nullptr;
    handle_ = nullptr;
    activated_ = false;
    processing_ = false;
    name_.clear();
}

void ClapHost::process(float* stereo, int frames, int sampleRate) {
    (void)sampleRate;
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    if (!enabled_ || plugin == nullptr || plugin->process == nullptr || frames <= 0) {
        return;
    }

    int offset = 0;
    while (offset < frames) {
        const int n = std::min(maxBlock_, frames - offset);
        for (int i = 0; i < n; ++i) {
            inL_[static_cast<size_t>(i)] = stereo[2 * (offset + i)];
            inR_[static_cast<size_t>(i)] = stereo[2 * (offset + i) + 1];
        }

        float* inData[2] = {inL_.data(), inR_.data()};
        float* outData[2] = {outL_.data(), outR_.data()};

        clap_audio_buffer_t inBuf;
        std::memset(&inBuf, 0, sizeof(inBuf));
        inBuf.data32 = inData;
        inBuf.channel_count = 2;

        clap_audio_buffer_t outBuf;
        std::memset(&outBuf, 0, sizeof(outBuf));
        outBuf.data32 = outData;
        outBuf.channel_count = 2;

        clap_process_t p;
        std::memset(&p, 0, sizeof(p));
        p.steady_time = -1;
        p.frames_count = static_cast<uint32_t>(n);
        p.transport = nullptr;
        p.audio_inputs = &inBuf;
        p.audio_outputs = &outBuf;
        p.audio_inputs_count = 1;
        p.audio_outputs_count = 1;
        p.in_events = &g_inEvents;
        p.out_events = &g_outEvents;

        plugin->process(plugin, &p);

        for (int i = 0; i < n; ++i) {
            stereo[2 * (offset + i)] = outL_[static_cast<size_t>(i)];
            stereo[2 * (offset + i) + 1] = outR_[static_cast<size_t>(i)];
        }
        offset += n;
    }
}

} // namespace maz::audio
