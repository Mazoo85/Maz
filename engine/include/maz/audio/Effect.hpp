#pragma once

namespace maz::audio {

// A stereo audio effect processed in place on the master bus. Effects hold internal state (delay
// lines, filter memory) across render blocks, so one instance is reused for the life of the mixer.
// Pure DSP — no SDL — so each effect is unit-testable on its own.
class Effect {
public:
    virtual ~Effect() = default;

    // Human-readable name for the mixer UI.
    virtual const char* name() const = 0;

    // Process `frames` of interleaved stereo samples (L, R, L, R, …) in place. `sampleRate` in Hz.
    // Implementations must no-op when !enabled().
    virtual void process(float* stereo, int frames, int sampleRate) = 0;

    // Clear internal state (silence delay lines etc.).
    virtual void reset() {}

    void setEnabled(bool e) { enabled_ = e; }
    bool enabled() const { return enabled_; }

protected:
    bool enabled_ = true;
};

} // namespace maz::audio
