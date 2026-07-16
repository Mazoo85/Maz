#include "maz/audio/WavWriter.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
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
                int sampleRate, std::string* err) {
    if (interleaved == nullptr || frames < 0 || channels <= 0 || sampleRate <= 0) {
        if (err != nullptr) {
            *err = "invalid arguments";
        }
        return false;
    }

    const uint32_t sampleCount = static_cast<uint32_t>(frames) * static_cast<uint32_t>(channels);
    const uint16_t bitsPerSample = 16;
    const uint16_t blockAlign = static_cast<uint16_t>(channels * (bitsPerSample / 8));
    const uint32_t byteRate = static_cast<uint32_t>(sampleRate) * blockAlign;
    const uint32_t dataBytes = sampleCount * (bitsPerSample / 8);

    std::vector<uint8_t> buf;
    buf.reserve(44 + dataBytes);

    // RIFF header.
    buf.insert(buf.end(), {'R', 'I', 'F', 'F'});
    putLE(buf, 36u + dataBytes, 4); // file size minus the first 8 bytes
    buf.insert(buf.end(), {'W', 'A', 'V', 'E'});

    // fmt chunk (PCM).
    buf.insert(buf.end(), {'f', 'm', 't', ' '});
    putLE(buf, 16u, 4);                                   // fmt chunk size
    putLE(buf, 1u, 2);                                    // audio format: 1 = PCM
    putLE(buf, static_cast<uint32_t>(channels), 2);
    putLE(buf, static_cast<uint32_t>(sampleRate), 4);
    putLE(buf, byteRate, 4);
    putLE(buf, blockAlign, 2);
    putLE(buf, bitsPerSample, 2);

    // data chunk.
    buf.insert(buf.end(), {'d', 'a', 't', 'a'});
    putLE(buf, dataBytes, 4);
    for (uint32_t i = 0; i < sampleCount; ++i) {
        const float clamped = std::clamp(interleaved[i], -1.0f, 1.0f);
        const int32_t v = static_cast<int32_t>(std::lround(clamped * 32767.0f));
        putLE(buf, static_cast<uint32_t>(static_cast<uint16_t>(static_cast<int16_t>(v))), 2);
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
