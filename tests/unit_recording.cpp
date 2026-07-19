// Unit test for session recording — arm recording, render, and confirm the captured buffer matches
// the rendered output and saves to a valid WAV. Pure offline, no device.

#include "maz/audio/AudioEngine.hpp"
#include "maz/audio/WavReader.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <vector>

using namespace maz;

namespace {

int g_failures = 0;

void check(bool cond, const char* msg) {
    std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", msg);
    if (!cond) {
        ++g_failures;
    }
}

double rms(const std::vector<float>& b) {
    if (b.empty()) {
        return 0.0;
    }
    double s = 0.0;
    for (float v : b) {
        s += static_cast<double>(v) * static_cast<double>(v);
    }
    return std::sqrt(s / static_cast<double>(b.size()));
}

} // namespace

int main() {
    audio::AudioEngine engine;
    engine.initOffline();
    engine.voice().setWaveform(audio::Waveform::Sine);
    engine.noteOn(440.0f);

    check(!engine.recording(), "recording is off by default");
    engine.armRecording();
    check(engine.recording(), "armRecording turns it on");

    const std::vector<float> out = engine.renderOffline(0.5);
    engine.stopRecording();
    check(!engine.recording(), "stopRecording turns it off");

    const std::vector<float>& rec = engine.recordedAudio();
    check(rec.size() == out.size(), "recorded length matches the rendered output");
    check(rms(rec) > 0.0, "recording captured sound");

    // The captured buffer should equal the rendered output sample-for-sample.
    bool identical = rec.size() == out.size();
    for (size_t i = 0; identical && i < rec.size(); ++i) {
        if (std::fabs(rec[i] - out[i]) > 1e-6f) {
            identical = false;
        }
    }
    check(identical, "recording is a faithful copy of the output");

    // Save + read back a valid WAV.
    const std::string path = "unit_recording_out.wav";
    std::string err;
    check(engine.saveRecording(path, &err), "saveRecording writes a WAV");
    audio::WavData wav;
    check(audio::readWav16(path, wav, &err) && wav.frames() > 0, "the recorded WAV reads back");

    // Recording honors the export bit depth (matching the bounce): save a 24-bit copy.
    const std::string path24 = "unit_recording_24.wav";
    check(engine.saveRecording(path24, &err, false, 24), "saveRecording writes a 24-bit WAV");
    audio::WavData wav24;
    check(audio::readWav16(path24, wav24, &err) && wav24.frames() == wav.frames(),
          "the 24-bit recording reads back with the same length");
    std::ifstream rf(path24, std::ios::binary);
    std::vector<uint8_t> rb((std::istreambuf_iterator<char>(rf)), std::istreambuf_iterator<char>());
    check(rb.size() > 35 && rb[34] == 24, "the recorded WAV header declares 24-bit");

    std::printf("%s: %d failure(s)\n", g_failures ? "FAILURES" : "ALL PASS", g_failures);
    return g_failures ? 1 : 0;
}
