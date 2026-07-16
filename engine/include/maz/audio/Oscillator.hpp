#pragma once

namespace maz::audio {

// The available oscillator waveforms. Sine is the default; the rest are cheap analogue-style
// shapes (naive, not band-limited — good enough for the "hello sound" milestone).
enum class Waveform { Sine, Square, Saw, Triangle };

// A single monophonic oscillator voice with a short click-free amplitude envelope.
//
// Pure DSP: it touches no SDL, no device, and no global state, so it can be unit-tested on its own
// (see tests/unit_audio.cpp). noteOn()/noteOff() gate a linear attack/release ramp so starting and
// stopping a tone does not click. render() ADDS its output into the caller's buffer so a future
// mixer can sum many voices.
class Oscillator {
public:
    void setWaveform(Waveform w) { waveform_ = w; }
    Waveform waveform() const { return waveform_; }

    void setAmplitude(float a) { amplitude_ = a; }
    float amplitude() const { return amplitude_; }

    void setFrequency(float hz) { frequency_ = hz; }
    float frequency() const { return frequency_; }

    // Start sounding at freqHz (opens the gate → attack ramp).
    void noteOn(float freqHz);
    // Stop sounding (closes the gate → release ramp). The voice keeps emitting until the ramp
    // reaches zero, which is why active() can stay true briefly after noteOff().
    void noteOff();

    // True while the note is held or the release ramp has not yet reached silence.
    bool active() const { return gate_ || env_ > 0.0f; }

    // Render `frames` mono samples, ADDING into out[0..frames). `sampleRate` is in Hz.
    void render(float* out, int frames, int sampleRate);

private:
    Waveform waveform_ = Waveform::Sine;
    float frequency_ = 440.0f;
    float amplitude_ = 0.5f;
    double phase_ = 0.0; // cycle position in [0, 1)
    bool gate_ = false;  // is the note currently held?
    float env_ = 0.0f;   // current envelope level in [0, 1]
};

} // namespace maz::audio
