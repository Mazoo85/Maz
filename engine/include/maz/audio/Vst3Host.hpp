#pragma once

#include "maz/audio/Effect.hpp"

#include <string>
#include <vector>

namespace maz::audio {

// Hosts a VST3 audio-effect plugin. The plugin module (a `.vst3` / `.so`) is dlopen'd, its
// `GetPluginFactory` entry point resolved, the factory queried for the first "Audio Module Class",
// that class instantiated as an IComponent + IAudioProcessor, set up for 32-bit float stereo, and
// run as an Effect on the master bus.
//
// This host links only against the MIT-licensed VST3 `pluginterfaces` headers (vendored in
// engine/third_party/vst3) — not Steinberg's GPL `public.sdk` — so it carries no GPL obligation.
// It is a focused host: one stereo in / stereo out, no parameter/GUI/event plumbing yet. The VST3
// COM types are kept out of this header (opaque pointers) so it stays clean.
class Vst3Host : public Effect {
public:
    Vst3Host() = default;
    ~Vst3Host() override;
    Vst3Host(const Vst3Host&) = delete;
    Vst3Host& operator=(const Vst3Host&) = delete;

    bool load(const std::string& path, int sampleRate, int maxBlock = 4096,
              std::string* err = nullptr);
    void unload();
    bool loaded() const { return processor_ != nullptr; }
    const std::string& pluginName() const { return name_; }

    const char* name() const override { return name_.empty() ? "VST3" : name_.c_str(); }
    void process(float* stereo, int frames, int sampleRate) override;

private:
    void* handle_ = nullptr;      // dlopen handle
    void* factory_ = nullptr;     // IPluginFactory*
    void* component_ = nullptr;   // IComponent*
    void* processor_ = nullptr;   // IAudioProcessor*
    bool moduleEntered_ = false;  // whether ModuleExit must be called
    bool active_ = false;
    int maxBlock_ = 4096;
    std::string name_;
    std::vector<float> inL_, inR_, outL_, outR_;
};

} // namespace maz::audio
