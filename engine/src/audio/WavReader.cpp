#include "maz/audio/WavReader.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace maz::audio {

std::vector<float> WavData::toMono() const {
    std::vector<float> mono;
    const int f = frames();
    if (channels <= 0 || f <= 0) {
        return mono;
    }
    mono.resize(static_cast<size_t>(f));
    for (int i = 0; i < f; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += samples[static_cast<size_t>(i) * static_cast<size_t>(channels) +
                           static_cast<size_t>(c)];
        }
        mono[static_cast<size_t>(i)] = sum / static_cast<float>(channels);
    }
    return mono;
}

namespace {
uint32_t readLE(const std::vector<uint8_t>& b, size_t off, int bytes) {
    uint32_t v = 0;
    for (int i = 0; i < bytes; ++i) {
        v |= static_cast<uint32_t>(b[off + static_cast<size_t>(i)]) << (8 * i);
    }
    return v;
}
bool tag(const std::vector<uint8_t>& b, size_t off, const char* t) {
    return b[off] == static_cast<uint8_t>(t[0]) && b[off + 1] == static_cast<uint8_t>(t[1]) &&
           b[off + 2] == static_cast<uint8_t>(t[2]) && b[off + 3] == static_cast<uint8_t>(t[3]);
}
} // namespace

bool readWav16(const std::string& path, WavData& out, std::string* err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        if (err != nullptr) {
            *err = "could not open '" + path + "'";
        }
        return false;
    }
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (b.size() < 44 || !tag(b, 0, "RIFF") || !tag(b, 8, "WAVE")) {
        if (err != nullptr) {
            *err = "'" + path + "' is not a RIFF/WAVE file";
        }
        return false;
    }

    // Walk the chunks looking for "fmt " and "data".
    int channels = 0;
    int sampleRate = 0;
    int bits = 0;
    int format = 0;
    size_t dataOff = 0;
    size_t dataLen = 0;
    size_t pos = 12;
    while (pos + 8 <= b.size()) {
        const uint32_t sz = readLE(b, pos + 4, 4);
        const size_t body = pos + 8;
        if (tag(b, pos, "fmt ") && body + 16 <= b.size()) {
            format = static_cast<int>(readLE(b, body, 2));
            channels = static_cast<int>(readLE(b, body + 2, 2));
            sampleRate = static_cast<int>(readLE(b, body + 4, 4));
            bits = static_cast<int>(readLE(b, body + 14, 2));
            // WAVE_FORMAT_EXTENSIBLE (0xFFFE): the real format code is the first 2 bytes of the
            // sub-format GUID in the fmt extension.
            if (format == 0xFFFE && sz >= 40 && body + 26 <= b.size()) {
                format = static_cast<int>(readLE(b, body + 24, 2));
            }
        } else if (tag(b, pos, "data")) {
            dataOff = body;
            dataLen = sz;
        }
        pos = body + sz + (sz & 1); // chunks are word-aligned
    }

    // Accept PCM (format 1) at 16/24/32-bit and IEEE float (format 3) at 32-bit — the common sample
    // formats. Anything else is rejected.
    const bool pcm = (format == 1) && (bits == 16 || bits == 24 || bits == 32);
    const bool isFloat = (format == 3) && (bits == 32);
    if ((!pcm && !isFloat) || channels <= 0 || sampleRate <= 0 || dataOff == 0) {
        if (err != nullptr) {
            *err = "'" + path + "' is not a supported WAV (need 16/24/32-bit PCM or 32-bit float)";
        }
        return false;
    }
    if (dataOff + dataLen > b.size()) {
        dataLen = b.size() - dataOff; // tolerate a truncated/oversized data length
    }

    const int bytesPer = bits / 8;
    const size_t sampleCount = dataLen / static_cast<size_t>(bytesPer);
    out.channels = channels;
    out.sampleRate = sampleRate;
    out.samples.resize(sampleCount);
    for (size_t i = 0; i < sampleCount; ++i) {
        const size_t off = dataOff + i * static_cast<size_t>(bytesPer);
        float sample = 0.0f;
        if (isFloat) {
            const uint32_t u = readLE(b, off, 4);
            std::memcpy(&sample, &u, sizeof(float));
        } else if (bits == 16) {
            sample = static_cast<float>(static_cast<int16_t>(readLE(b, off, 2))) / 32768.0f;
        } else if (bits == 24) {
            const uint32_t u = readLE(b, off, 3);
            // Sign-extend the 24-bit value into 32 bits, then normalize by 2^23.
            const int32_t s = (u & 0x800000u) ? static_cast<int32_t>(u | 0xFF000000u)
                                              : static_cast<int32_t>(u);
            sample = static_cast<float>(s) / 8388608.0f;
        } else { // 32-bit PCM
            sample = static_cast<float>(static_cast<int32_t>(readLE(b, off, 4))) / 2147483648.0f;
        }
        out.samples[i] = sample;
    }
    return true;
}

} // namespace maz::audio
