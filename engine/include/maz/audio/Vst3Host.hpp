#pragma once

#include "maz/audio/InstrumentPlugin.hpp"

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
class Vst3Host : public InstrumentPlugin {
public:
    Vst3Host() = default;
    ~Vst3Host() override;
    Vst3Host(const Vst3Host&) = delete;
    Vst3Host& operator=(const Vst3Host&) = delete;

    bool load(const std::string& path, int sampleRate, int maxBlock = 4096,
              std::string* err = nullptr);
    void unload();
    bool loaded() const override { return processor_ != nullptr; }
    const std::string& pluginName() const { return name_; }

    // True if the plugin declares at least one event (MIDI/note) input bus — the routing signal that
    // it can be hosted as an instrument (driven by note events) rather than a pure audio effect.
    // Mirrors ClapHost::hasNotePorts(). Pure audio effects (e.g. the example tremolo) report false.
    bool hasEventInput() const;

    // Instrument hosting: queue note-on/off events for the next process() call. They are delivered to
    // the plugin as a VST3 IEventList at the top of that block; the plugin's synthesised audio then
    // replaces the buffer passed to process() (an instrument ignores audio input). Velocity is 0..1.
    // Mirrors ClapHost::noteOn/noteOff/allNotesOff.
    void noteOn(int key, float velocity) override;
    void noteOff(int key) override;
    // Release every currently-held note (panic / all-notes-off) so a hosted instrument doesn't hang.
    void allNotesOff() override;

    // Parameter automation: enumerate via the plugin's IEditController (if any) and queue changes
    // delivered to the processor as VST3 parameter changes at the top of the next process() block.
    int paramCount() const override;
    void setParam(int index, double value) override;
    double paramValue(int index) const override;
    double paramMin(int index) const override;
    double paramMax(int index) const override;
    std::string paramName(int index) const override;

    const char* name() const override { return name_.empty() ? "VST3" : name_.c_str(); }
    void process(float* stereo, int frames, int sampleRate) override;

private:
    void* handle_ = nullptr;      // dlopen handle
    void* factory_ = nullptr;     // IPluginFactory*
    void* component_ = nullptr;   // IComponent*
    void* processor_ = nullptr;   // IAudioProcessor*
    void* controller_ = nullptr;  // IEditController* (parameter enumeration), or null
    struct PendingParam {
        unsigned int id;
        double value;
    };
    std::vector<PendingParam> pendingParams_; // queued param changes for the next process()
    bool moduleEntered_ = false;  // whether ModuleExit must be called
    bool active_ = false;
    int maxBlock_ = 4096;
    std::string name_;
    std::vector<float> inL_, inR_, outL_, outR_;
    struct PendingNote {
        int key;
        float velocity;
        bool on; // true = note-on, false = note-off
    };
    std::vector<PendingNote> pendingNotes_; // queued for the next process() (instrument hosting)
    std::vector<int> heldKeys_;             // keys currently on (for allNotesOff / panic)
};

} // namespace maz::audio
