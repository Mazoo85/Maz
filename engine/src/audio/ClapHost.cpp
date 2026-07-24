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

// A concrete clap_input_events backed by a vector of note events (instrument hosting). The vector is
// reached through the event list's ctx pointer.
struct NoteEventFeed {
    std::vector<clap_event_note_t> notes;
    std::vector<clap_event_param_value_t> params; // param changes delivered alongside the notes
};
uint32_t feedSize(const clap_input_events* e) {
    const auto* f = static_cast<const NoteEventFeed*>(e->ctx);
    return static_cast<uint32_t>(f->notes.size() + f->params.size());
}
const clap_event_header_t* feedGet(const clap_input_events* e, uint32_t index) {
    const auto* f = static_cast<const NoteEventFeed*>(e->ctx);
    if (index < f->notes.size()) {
        return &f->notes[static_cast<size_t>(index)].header;
    }
    const uint32_t pidx = index - static_cast<uint32_t>(f->notes.size());
    if (pidx >= f->params.size()) {
        return nullptr;
    }
    return &f->params[static_cast<size_t>(pidx)].header;
}

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
    // Reject an incompatible ABI before calling through the entry: a 0.x or future-major CLAP has no
    // guaranteed struct layout, so reading init/get_factory (done above) and calling them would be UB.
    if (!clap_version_is_compatible(entry->clap_version)) {
        if (err != nullptr) {
            *err = "'" + path + "' has an incompatible CLAP ABI version";
        }
        unload();
        return false;
    }
    entry_ = entry;
    if (!entry->init(path.c_str())) {
        if (err != nullptr) {
            *err = "clap_entry->init failed";
        }
        // init() failed → the CLAP contract forbids deinit(); drop entry_ so unload() skips it (it
        // still dlcloses the module). Later failures reach unload() only after init() succeeded.
        entry_ = nullptr;
        unload();
        return false;
    }
    entryInited_ = true;

    const auto* factory =
        static_cast<const clap_plugin_factory_t*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    // Guard the mandatory factory vtable members before calling through them (a malformed plugin can
    // expose a non-null factory with null function pointers).
    if (factory == nullptr || factory->get_plugin_count == nullptr ||
        factory->get_plugin_descriptor == nullptr || factory->create_plugin == nullptr ||
        factory->get_plugin_count(factory) == 0) {
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
    if (plugin == nullptr || plugin->init == nullptr || plugin->activate == nullptr) {
        if (err != nullptr) {
            *err = "create_plugin failed / incomplete plugin";
        }
        if (plugin != nullptr && plugin->destroy != nullptr) {
            plugin->destroy(plugin); // release a partially-created plugin we won't adopt
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
    if (entry != nullptr && entryInited_ && entry->deinit != nullptr) {
        entry->deinit(); // only legal once init() has succeeded
    }
    if (handle_ != nullptr) {
        dlclose(handle_);
    }
    plugin_ = nullptr;
    entry_ = nullptr;
    entryInited_ = false;
    handle_ = nullptr;
    activated_ = false;
    processing_ = false;
    name_.clear();
}

bool ClapHost::hasNotePorts() const {
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    if (plugin == nullptr || plugin->get_extension == nullptr) {
        return false;
    }
    const auto* np = static_cast<const clap_plugin_note_ports_t*>(
        plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS));
    if (np == nullptr || np->count == nullptr) {
        return false;
    }
    return np->count(plugin, true) > 0; // true = input note ports → an instrument
}

void ClapHost::noteOn(int key, float velocity) {
    pendingNotes_.push_back({key, velocity < 0.0f ? 0.0f : (velocity > 1.0f ? 1.0f : velocity), true});
    heldKeys_.push_back(key);
}

void ClapHost::noteOff(int key) {
    pendingNotes_.push_back({key, 0.0f, false});
    for (size_t i = 0; i < heldKeys_.size(); ++i) {
        if (heldKeys_[i] == key) {
            heldKeys_.erase(heldKeys_.begin() + static_cast<long>(i));
            break;
        }
    }
}

void ClapHost::allNotesOff() {
    for (int key : heldKeys_) {
        pendingNotes_.push_back({key, 0.0f, false});
    }
    heldKeys_.clear();
}

const void* ClapHost::paramsExt() const {
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    if (plugin == nullptr || plugin->get_extension == nullptr) {
        return nullptr;
    }
    return plugin->get_extension(plugin, CLAP_EXT_PARAMS);
}

int ClapHost::paramCount() const {
    const auto* pr = static_cast<const clap_plugin_params_t*>(paramsExt());
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    if (pr == nullptr || pr->count == nullptr || plugin == nullptr) {
        return 0;
    }
    return static_cast<int>(pr->count(plugin));
}

// Fetch the param info for a display index (0..count-1). Returns false if unavailable.
static bool clapParamInfoAt(const void* paramsExt, const void* plugin_, int index,
                            clap_param_info_t* out) {
    const auto* pr = static_cast<const clap_plugin_params_t*>(paramsExt);
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    if (pr == nullptr || pr->get_info == nullptr || plugin == nullptr || index < 0) {
        return false;
    }
    return pr->get_info(plugin, static_cast<uint32_t>(index), out);
}

void ClapHost::setParam(int index, double value) {
    clap_param_info_t info;
    if (!clapParamInfoAt(paramsExt(), plugin_, index, &info)) {
        return;
    }
    const double v = value < info.min_value ? info.min_value
                                            : (value > info.max_value ? info.max_value : value);
    pendingParams_.push_back({info.id, v});
}

double ClapHost::paramValue(int index) const {
    clap_param_info_t info;
    if (!clapParamInfoAt(paramsExt(), plugin_, index, &info)) {
        return 0.0;
    }
    const auto* pr = static_cast<const clap_plugin_params_t*>(paramsExt());
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    double out = info.default_value;
    if (pr->get_value != nullptr) {
        pr->get_value(plugin, info.id, &out);
    }
    return out;
}

double ClapHost::paramMin(int index) const {
    clap_param_info_t info;
    return clapParamInfoAt(paramsExt(), plugin_, index, &info) ? info.min_value : 0.0;
}

double ClapHost::paramMax(int index) const {
    clap_param_info_t info;
    return clapParamInfoAt(paramsExt(), plugin_, index, &info) ? info.max_value : 1.0;
}

std::string ClapHost::paramName(int index) const {
    clap_param_info_t info;
    if (!clapParamInfoAt(paramsExt(), plugin_, index, &info)) {
        return "";
    }
    return std::string(info.name);
}

void ClapHost::process(float* stereo, int frames, int sampleRate) {
    (void)sampleRate;
    const auto* plugin = static_cast<const clap_plugin_t*>(plugin_);
    if (!enabled_ || plugin == nullptr || plugin->process == nullptr || frames <= 0) {
        return;
    }

    // Build the note events queued since the last call; deliver them at the top of the first sub-block.
    NoteEventFeed feed;
    for (const PendingNote& pn : pendingNotes_) {
        clap_event_note_t ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.header.size = sizeof(clap_event_note_t);
        ev.header.time = 0;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.type = static_cast<uint16_t>(pn.on ? CLAP_EVENT_NOTE_ON : CLAP_EVENT_NOTE_OFF);
        ev.header.flags = 0;
        ev.note_id = -1;
        ev.port_index = 0;
        ev.channel = 0;
        ev.key = static_cast<int16_t>(pn.key);
        ev.velocity = static_cast<double>(pn.velocity);
        feed.notes.push_back(ev);
    }
    pendingNotes_.clear();
    // Queued parameter changes ride the same event list (delivered on the first sub-block).
    for (const PendingParam& pp : pendingParams_) {
        clap_event_param_value_t ev;
        std::memset(&ev, 0, sizeof(ev));
        ev.header.size = sizeof(clap_event_param_value_t);
        ev.header.time = 0;
        ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        ev.header.type = static_cast<uint16_t>(CLAP_EVENT_PARAM_VALUE);
        ev.header.flags = 0;
        ev.param_id = pp.id;
        ev.cookie = nullptr;
        ev.note_id = -1;
        ev.port_index = -1;
        ev.channel = -1;
        ev.key = -1;
        ev.value = pp.value;
        feed.params.push_back(ev);
    }
    pendingParams_.clear();
    const clap_input_events_t noteEvents = {&feed, feedSize, feedGet};

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
        // Deliver the queued note events on the first sub-block only; later sub-blocks get none.
        p.in_events = (offset == 0) ? &noteEvents : &g_inEvents;
        p.out_events = &g_outEvents;

        const clap_process_status status = plugin->process(plugin, &p);

        // Only copy the plugin's output back when it reported success; on CLAP_PROCESS_ERROR it may
        // have left its output buffers untouched, so we keep the dry input rather than emit stale data.
        if (status != CLAP_PROCESS_ERROR) {
            for (int i = 0; i < n; ++i) {
                stereo[2 * (offset + i)] = outL_[static_cast<size_t>(i)];
                stereo[2 * (offset + i) + 1] = outR_[static_cast<size_t>(i)];
            }
        }
        offset += n;
    }
}

} // namespace maz::audio
