// tests/audio/qoa.cpp — verifies the QOA audio codec (audio::encodeQoa / decodeQoa) by an ENCODE->DECODE
// round-trip. QOA is lossy but deterministic, so the check is: the stream carries the right header/rate/
// channels/sample-count, the decoded signal tracks the input within QOA's bounded per-sample error, and it
// actually compresses. No external reference file — the input is a synthesized sine (mono) and a two-tone
// stereo signal built in-memory.
#include "maz/audio/Qoa.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::audio;

static double rms_error(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 1e9;
    double acc = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        const double e = static_cast<double>(a[i]) - static_cast<double>(b[i]);
        acc += e * e;
    }
    return std::sqrt(acc / static_cast<double>(a.size()));
}

int main() {
    // --- 1. Mono sine round-trip: header, metadata, sample count, bounded error, compression. ---
    {
        WavData in;
        in.sampleRate = 44100;
        in.channels = 1;
        const int n = 4000; // spans multiple 20-sample slices
        in.samples.resize(static_cast<std::size_t>(n));
        for (int i = 0; i < n; ++i)
            in.samples[static_cast<std::size_t>(i)] =
                0.6f * std::sin(2.0f * 3.14159265f * 440.0f * static_cast<float>(i) / 44100.0f);

        const std::vector<std::uint8_t> enc = encodeQoa(in);
        CHECK(enc.size() > 8, "encoder produced a stream");
        CHECK(enc[0] == 'q' && enc[1] == 'o' && enc[2] == 'a' && enc[3] == 'f', "magic is qoaf");
        // header sample count (BE u32) == frames.
        const std::uint32_t hdrSamples = (static_cast<std::uint32_t>(enc[4]) << 24) |
                                         (static_cast<std::uint32_t>(enc[5]) << 16) |
                                         (static_cast<std::uint32_t>(enc[6]) << 8) | enc[7];
        CHECK(hdrSamples == static_cast<std::uint32_t>(n), "header sample-count matches input");

        const WavData out = decodeQoa(enc);
        CHECK(out.channels == 1 && out.sampleRate == 44100, "decoded metadata preserved");
        CHECK(out.samples.size() == in.samples.size(), "decoded sample count matches");
        const double err = rms_error(in.samples, out.samples);
        CHECK(err < 0.05, "round-trip RMS error is small (bounded QOA loss)");
        // QOA packs 20 samples into 8 bytes (+small headers) => ~2.5x smaller than 16-bit PCM.
        CHECK(enc.size() < in.samples.size() * 2, "QOA stream is smaller than 16-bit PCM");
    }

    // --- 2. Stereo round-trip: interleaved channels stay aligned. ---
    {
        WavData in;
        in.sampleRate = 22050;
        in.channels = 2;
        const int frames = 1500;
        in.samples.resize(static_cast<std::size_t>(frames) * 2);
        for (int i = 0; i < frames; ++i) {
            const float t = static_cast<float>(i) / 22050.0f;
            in.samples[static_cast<std::size_t>(i) * 2 + 0] = 0.5f * std::sin(2.0f * 3.14159265f * 220.0f * t);
            in.samples[static_cast<std::size_t>(i) * 2 + 1] = 0.5f * std::sin(2.0f * 3.14159265f * 660.0f * t);
        }
        const WavData out = decodeQoa(encodeQoa(in));
        CHECK(out.channels == 2 && out.sampleRate == 22050, "stereo metadata preserved");
        CHECK(out.samples.size() == in.samples.size(), "stereo sample count matches");
        CHECK(rms_error(in.samples, out.samples) < 0.06, "stereo round-trip error bounded");
    }

    // --- 3. Malformed / empty inputs are rejected cleanly. ---
    {
        CHECK(decodeQoa(std::vector<std::uint8_t>{}).samples.empty(), "empty input -> empty");
        std::vector<std::uint8_t> notQoa = {'n', 'o', 'p', 'e', 0, 0, 0, 1};
        CHECK(decodeQoa(notQoa).samples.empty(), "bad magic rejected");
        CHECK(encodeQoa(WavData()).empty(), "encoding empty audio yields empty");
    }

    if (g_fail == 0) {
        std::printf("qoa: OK — mono+stereo encode/decode round-trip, metadata, compression, rejection.\n");
        return 0;
    }
    std::printf("qoa: %d failure(s).\n", g_fail);
    return 1;
}
