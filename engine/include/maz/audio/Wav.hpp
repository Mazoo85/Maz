#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace maz::audio {

// WAV load/save — Godot's AudioStreamWAV / the "load a .wav" half of the asset pipeline. Every prior Maz
// sound was PROCEDURALLY synthesized (audio::SoundDesc); there was no way to read an actual audio file, or
// to export one. This is a self-contained RIFF/WAVE PCM codec: `decodeWav` parses the exact bytes of a
// .wav file into float samples, and `encodeWav` writes them back out. It handles the two ubiquitous PCM
// formats — 8-bit unsigned and 16-bit signed, mono or interleaved multi-channel — which covers the vast
// majority of game sound assets. Byte-in / byte-out (no file device), so it unit-tests headlessly and the
// app owns any real disk read/write.

struct WavData {
    std::uint32_t sampleRate = 44100;
    std::uint16_t channels = 1;
    std::vector<float> samples; // interleaved, normalized to [-1, 1]

    std::size_t frameCount() const { return channels ? samples.size() / channels : 0; }
};

namespace detail {

inline std::uint16_t rd16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
inline std::uint32_t rd32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}
inline void wr16(std::vector<std::uint8_t>& b, std::uint16_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}
inline void wr32(std::vector<std::uint8_t>& b, std::uint32_t v) {
    b.push_back(static_cast<std::uint8_t>(v & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
    b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
}
inline void wrTag(std::vector<std::uint8_t>& b, const char* t) {
    b.push_back(static_cast<std::uint8_t>(t[0]));
    b.push_back(static_cast<std::uint8_t>(t[1]));
    b.push_back(static_cast<std::uint8_t>(t[2]));
    b.push_back(static_cast<std::uint8_t>(t[3]));
}
inline bool tagEq(const std::uint8_t* p, const char* t) {
    return p[0] == static_cast<std::uint8_t>(t[0]) && p[1] == static_cast<std::uint8_t>(t[1]) &&
           p[2] == static_cast<std::uint8_t>(t[2]) && p[3] == static_cast<std::uint8_t>(t[3]);
}

} // namespace detail

// Parse a RIFF/WAVE PCM byte stream into float samples. Returns false on a malformed / unsupported stream
// (only PCM 8-bit unsigned and 16-bit signed are supported).
inline bool decodeWav(const std::uint8_t* data, std::size_t size, WavData& out) {
    out = WavData{};
    if (!data || size < 44) {
        return false;
    }
    if (!detail::tagEq(data, "RIFF") || !detail::tagEq(data + 8, "WAVE")) {
        return false;
    }

    std::uint16_t bits = 0;
    bool haveFmt = false;
    std::size_t i = 12;
    while (i + 8 <= size) {
        const std::uint8_t* chunk = data + i;
        const std::uint32_t chunkSize = detail::rd32(chunk + 4);
        const std::size_t body = i + 8;
        if (body > size) {
            break;
        }
        if (detail::tagEq(chunk, "fmt ") && chunkSize >= 16 && body + 16 <= size) {
            const std::uint16_t format = detail::rd16(data + body);
            out.channels = detail::rd16(data + body + 2);
            out.sampleRate = detail::rd32(data + body + 4);
            bits = detail::rd16(data + body + 14);
            haveFmt = true;
            if (format != 1) { // 1 = integer PCM
                return false;
            }
        } else if (detail::tagEq(chunk, "data")) {
            if (!haveFmt || out.channels == 0) {
                return false;
            }
            const std::size_t avail = size - body;
            const std::size_t dataLen = static_cast<std::size_t>(chunkSize) <= avail ? chunkSize : avail;
            const std::uint8_t* pcm = data + body;
            if (bits == 16) {
                const std::size_t n = dataLen / 2;
                out.samples.reserve(n);
                for (std::size_t s = 0; s < n; ++s) {
                    const std::int16_t v = static_cast<std::int16_t>(detail::rd16(pcm + s * 2));
                    out.samples.push_back(static_cast<float>(v) / 32768.0f);
                }
            } else if (bits == 8) {
                out.samples.reserve(dataLen);
                for (std::size_t s = 0; s < dataLen; ++s) {
                    out.samples.push_back((static_cast<float>(pcm[s]) - 128.0f) / 128.0f);
                }
            } else {
                return false;
            }
            return true; // data is the payload; stop once decoded
        }
        // Chunks are word-aligned (padded to even length).
        i = body + chunkSize + (chunkSize & 1u);
    }
    return false; // no data chunk
}

inline bool decodeWav(const std::vector<std::uint8_t>& bytes, WavData& out) {
    return decodeWav(bytes.data(), bytes.size(), out);
}

// Write float samples out as a 16-bit PCM RIFF/WAVE byte stream (clamped to [-1, 1]).
inline std::vector<std::uint8_t> encodeWav(const WavData& wav) {
    std::vector<std::uint8_t> b;
    const std::uint16_t channels = wav.channels ? wav.channels : 1;
    const std::uint16_t bits = 16;
    const std::uint16_t blockAlign = static_cast<std::uint16_t>(channels * (bits / 8));
    const std::uint32_t byteRate = wav.sampleRate * blockAlign;
    const std::uint32_t dataBytes = static_cast<std::uint32_t>(wav.samples.size() * 2);

    detail::wrTag(b, "RIFF");
    detail::wr32(b, 36u + dataBytes);
    detail::wrTag(b, "WAVE");
    detail::wrTag(b, "fmt ");
    detail::wr32(b, 16u);
    detail::wr16(b, 1u); // PCM
    detail::wr16(b, channels);
    detail::wr32(b, wav.sampleRate);
    detail::wr32(b, byteRate);
    detail::wr16(b, blockAlign);
    detail::wr16(b, bits);
    detail::wrTag(b, "data");
    detail::wr32(b, dataBytes);
    for (float f : wav.samples) {
        const float c = f < -1.0f ? -1.0f : (f > 1.0f ? 1.0f : f);
        const std::int32_t v = static_cast<std::int32_t>(c * 32767.0f);
        detail::wr16(b, static_cast<std::uint16_t>(static_cast<std::int16_t>(v)));
    }
    return b;
}

} // namespace maz::audio
