#include "maz/audio/PluginHost.hpp"

#include "maz/core/Log.hpp"

#include <dlfcn.h>

namespace maz::audio {

PluginHost::~PluginHost() {
    unload();
}

bool PluginHost::load(const std::string& path, int sampleRate, std::string* err) {
    unload();

    void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        if (err != nullptr) {
            const char* e = dlerror();
            *err = "dlopen failed: " + std::string(e != nullptr ? e : "unknown");
        }
        return false;
    }

    auto descriptor = reinterpret_cast<MazPluginDescriptorFn>(dlsym(handle, "maz_plugin_descriptor"));
    auto create = reinterpret_cast<MazPluginCreateFn>(dlsym(handle, "maz_plugin_create"));
    auto process = reinterpret_cast<MazPluginProcessFn>(dlsym(handle, "maz_plugin_process"));
    auto setParam = reinterpret_cast<MazPluginSetParamFn>(dlsym(handle, "maz_plugin_set_param"));
    auto destroy = reinterpret_cast<MazPluginDestroyFn>(dlsym(handle, "maz_plugin_destroy"));
    if (descriptor == nullptr || create == nullptr || process == nullptr || destroy == nullptr) {
        if (err != nullptr) {
            *err = "'" + path + "' is missing required plugin entry points";
        }
        dlclose(handle);
        return false;
    }

    const MazPluginDesc* desc = descriptor();
    if (desc == nullptr || desc->abi_version != MAZ_PLUGIN_ABI_VERSION) {
        if (err != nullptr) {
            *err = "'" + path + "' has an incompatible plugin ABI version";
        }
        dlclose(handle);
        return false;
    }

    MazPluginInstance* instance = create(sampleRate);
    if (instance == nullptr) {
        if (err != nullptr) {
            *err = "plugin '" + path + "' failed to instantiate";
        }
        dlclose(handle);
        return false;
    }

    handle_ = handle;
    instance_ = instance;
    process_ = process;
    setParam_ = setParam;
    destroy_ = destroy;
    name_ = desc->name != nullptr ? desc->name : "Plugin";
    path_ = path;
    paramCount_ = desc->param_count;
    MAZ_LOG_INFO("plugin: loaded \"%s\" from %s (%d params)", name_.c_str(), path.c_str(),
                 paramCount_);
    return true;
}

void PluginHost::unload() {
    if (instance_ != nullptr && destroy_ != nullptr) {
        destroy_(instance_);
    }
    instance_ = nullptr;
    process_ = nullptr;
    setParam_ = nullptr;
    destroy_ = nullptr;
    if (handle_ != nullptr) {
        dlclose(handle_);
        handle_ = nullptr;
    }
    name_.clear();
    path_.clear();
    paramCount_ = 0;
}

void PluginHost::setParam(int index, float value) {
    // Bounds-check against the plugin's declared parameter count so out-of-range automation/UI indices
    // never reach the hosted plugin (which may index a fixed array without checking).
    if (instance_ != nullptr && setParam_ != nullptr && index >= 0 && index < paramCount_) {
        setParam_(instance_, index, value);
    }
}

void PluginHost::process(float* stereo, int frames, int sampleRate) {
    (void)sampleRate;
    if (!enabled_ || instance_ == nullptr || process_ == nullptr || frames <= 0) {
        return;
    }
    process_(instance_, stereo, frames);
}

} // namespace maz::audio
