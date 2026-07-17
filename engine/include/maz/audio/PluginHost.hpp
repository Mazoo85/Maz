#pragma once

#include "maz/audio/Effect.hpp"
#include "maz/audio/PluginApi.h"

#include <string>

namespace maz::audio {

// Hosts a dynamically-loaded native audio plugin (a shared library exporting the maz plugin ABI,
// see PluginApi.h) and runs it as an Effect in the mixer chain. Real plugin hosting: the library is
// dlopen'd at runtime, its ABI is checked, and its process() is called on the master bus.
class PluginHost : public Effect {
public:
    PluginHost() = default;
    ~PluginHost() override;
    PluginHost(const PluginHost&) = delete;
    PluginHost& operator=(const PluginHost&) = delete;

    // Load a plugin shared library and instantiate it at `sampleRate`. Returns false + sets *err on
    // failure (missing file, missing symbols, ABI mismatch).
    bool load(const std::string& path, int sampleRate, std::string* err = nullptr);
    void unload();
    bool loaded() const { return instance_ != nullptr; }

    const std::string& pluginName() const { return name_; }
    const std::string& path() const { return path_; }
    int paramCount() const { return paramCount_; }
    void setParam(int index, float value);

    const char* name() const override { return name_.empty() ? "Plugin" : name_.c_str(); }
    void process(float* stereo, int frames, int sampleRate) override;

private:
    void* handle_ = nullptr;              // dlopen handle
    MazPluginInstance* instance_ = nullptr;
    MazPluginProcessFn process_ = nullptr;
    MazPluginSetParamFn setParam_ = nullptr;
    MazPluginDestroyFn destroy_ = nullptr;
    std::string name_;
    std::string path_;
    int paramCount_ = 0;
};

} // namespace maz::audio
