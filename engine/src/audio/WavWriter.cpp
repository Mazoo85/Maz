#include "maz/audio/WavWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace maz::audio {

namespace {

// Append a little-endian integer of `bytes` width to a byte vector (WAV is little-endian).
void putLE(std::vector<uint8_t>& out, uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i) {
        out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xFFu));
    }
}

} // namespace

bool writeWav16(const std::string& path, const float* interleaved, int frames, int channels,
                int sampleRate, std::string* err, bool dither, int bits) {
    if (interleaved == nullptr || frames < 0 || channels <= 0 || sampleRate <= 0) {
        if (err != nullptr) {
            *err = "invalid arguments";
        }
        return false;
    }
    if (bits != 24 && bits != 32) {
        bits = 16; // 16/24-bit PCM and 32-bit float are supported; anything else falls back to 16
    }
    const bool isFloat = (bits == 32); // 32-bit export is IEEE float (format 3), not PCM

    const uint32_t sampleCount = static_cast<uint32_t>(frames) * static_cast<uint32_t>(channels);
    const uint16_t bitsPerSample = static_cast<uint16_t>(bits);
    const int bytesPerSample = bits / 8;
    const uint16_t blockAlign = static_cast<uint16_t>(channels * bytesPerSample);
    const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * blockAlign;
    const uint32_t dataBytes = sampleCount * static_cast<uint32_t>(bytesPerSample);
    // Full-scale integer for a PCM depth, e.g. 32767 (16-bit) or 8388607 (24-bit). Unused for float.
    const float maxVal = isFloat ? 0.0f : static_cast<float>((1 << (bits - 1)) - 1);
    const int32_t maxInt = isFloat ? 0 : (1 << (bits - 1)) - 1;
    const int32_t minInt = isFloat ? 0 : -(1 << (bits - 1));

    std::vector<uint8_t> buf;
    buf.reserve(44 + dataBytes);

    // RIFF header. Float files add a 12-byte "fact" chunk (required for non-PCM by the spec) and an
    // extended fmt chunk (18 bytes: the base 16 plus a 2-byte cbSize=0), which the WAVE spec requires
    // for any non-PCM format code — strict parsers reject a format-3 file with a 16-byte fmt.
    const uint32_t factBytes = isFloat ? 12u : 0u;
    const uint32_t fmtExtra = isFloat ? 2u : 0u; // the cbSize field on the extended fmt chunk
    buf.insert(buf.end(), {'R', 'I', 'F', 'F'});
    putLE(buf, 36u + fmtExtra + factBytes + dataBytes, 4); // file size minus the first 8 bytes
    buf.insert(buf.end(), {'W', 'A', 'V', 'E'});

    // fmt chunk: 1 = PCM, 3 = IEEE float.
    buf.insert(buf.end(), {'f', 'm', 't', ' '});
    putLE(buf, 16u + fmtExtra, 4);                       // fmt chunk size (18 for non-PCM/float)
    putLE(buf, isFloat ? 3u : 1u, 2);                    // audio format
    putLE(buf, static_cast<uint32_t>(channels), 2);
    putLE(buf, static_cast<uint32_t>(sampleRate), 4);
    putLE(buf, byteRate, 4);
    putLE(buf, blockAlign, 2);
    putLE(buf, bitsPerSample, 2);
    if (isFloat) {
        putLE(buf, 0u, 2); // cbSize: no extension bytes follow
    }

    // fact chunk (float only): the number of sample frames.
    if (isFloat) {
        buf.insert(buf.end(), {'f', 'a', 'c', 't'});
        putLE(buf, 4u, 4);
        putLE(buf, static_cast<uint32_t>(frames), 4);
    }

    // data chunk.
    buf.insert(buf.end(), {'d', 'a', 't', 'a'});
    putLE(buf, dataBytes, 4);
    // Fixed-seed xorshift32 PRNG for reproducible TPDF dither (only advanced when `dither` is set, so
    // the non-dithered path is bit-for-bit unchanged). Two independent uniforms summed with opposite
    // sign give a triangular distribution spanning (-1, +1) LSB — the standard TPDF dither.
    uint32_t rng = 0x2545F491u;
    auto nextUnit = [&rng]() -> float {
        rng ^= rng << 13;
        rng ^= rng >> 17;
        rng ^= rng << 5;
        return static_cast<float>(rng) * (1.0f / 4294967296.0f); // [0, 1)
    };
    for (uint32_t i = 0; i < sampleCount; ++i) {
        if (isFloat) {
            // 32-bit float: write the sample verbatim (no clamp — float export preserves headroom).
            uint32_t u;
            const float fv = interleaved[i];
            std::memcpy(&u, &fv, sizeof(float));
            putLE(buf, u, 4);
            continue;
        }
        const float clamped = std::clamp(interleaved[i], -1.0f, 1.0f);
        float scaled = clamped * maxVal;
        if (dither) {
            scaled += nextUnit() - nextUnit(); // TPDF noise in (-1, +1) LSB at the target depth
        }
        // Dither (or a full-scale sample) can push the rounded value past the range, so clamp.
        int32_t v = std::clamp(static_cast<int32_t>(std::lround(scaled)), minInt, maxInt);
        // Two's-complement low `bytesPerSample` bytes, little-endian.
        putLE(buf, static_cast<uint32_t>(v) & (bits == 24 ? 0xFFFFFFu : 0xFFFFu), bytesPerSample);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        if (err != nullptr) {
            *err = "could not open '" + path + "' for writing";
        }
        return false;
    }
    file.write(reinterpret_cast<const char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
    if (!file) {
        if (err != nullptr) {
            *err = "write to '" + path + "' failed";
        }
        return false;
    }
    return true;
}

} // namespace maz::audio
