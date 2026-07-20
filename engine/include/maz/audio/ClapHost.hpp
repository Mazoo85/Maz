#pragma once

#include "maz/audio/Effect.hpp"

#include <string>
#include <vector>

namespace maz::audio {

// Hosts a CLAP plugin (https://cleveraudio.org) — a real, open plugin format. The plugin's `.clap`
// shared library is dlopen'd, its `clap_entry` resolved, its factory queried, the first plugin
// instantiated, activated, and run as an Effect on the master bus. This is genuine CLAP format
// support (the CLAP SDK headers are vendored in engine/third_party/clap).
//
// This is a focused host: one stereo audio-in / stereo audio-out plugin, no parameter/GUI/event
// plumbing yet. The CLAP structs are kept out of this header (opaque pointers) so it stays clean.
class ClapHost : public Effect {
public:
    ClapHost() = default;
    ~ClapHost() override;
    ClapHost(const ClapHost&) = delete;
    ClapHost& operator=(const ClapHost&) = delete;

    bool load(const std::string& path, int sampleRate, int maxBlock = 4096,
              std::string* err = nullptr);
    void unload();
    bool loaded() const { return plugin_ != nullptr; }
    const std::string& pluginName() const { return name_; }
    // True if the loaded plugin exposes at least one INPUT note port — i.e. it is an instrument
    // (driven by note events) rather than a pure audio effect. Used to route it as a channel synth.
    bool hasNotePorts() const;

    const char* name() const override { return name_.empty() ? "CLAP" : name_.c_str(); }
    void process(float* stereo, int frames, int sampleRate) override;

private:
    void* handle_ = nullptr; // dlopen handle
    const void* entry_ = nullptr;
    const void* plugin_ = nullptr;
    bool activated_ = false;
    bool processing_ = false;
    int maxBlock_ = 4096;
    std::string name_;
    std::vector<float> inL_, inR_, outL_, outR_;
};

} // namespace maz::audio
