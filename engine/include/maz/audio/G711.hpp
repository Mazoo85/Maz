#pragma once

#include "maz/audio/Wav.hpp" // audio::WavData

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::audio G.711 (ITU-T) μ-law / A-law companding codec — the compression behind virtually all digital
// telephony and a great deal of voice/VoIP audio, and a common `.wav` payload format (WAVE format tags 7 and
// 6). G.711 "compands" a 16-bit PCM sample down to a single byte using a logarithmic curve, so quiet sounds
// keep their detail while loud ones lose the least-significant bits you can't hear anyway — a fixed 2:1
// compression at telephone quality, decode-anywhere with no tables to ship. This is the exact ITU-T
// reference companding: μ-law (used in North America/Japan) and A-law (used in Europe/international). Byte-in
// / byte-out around the engine's `audio::WavData` (float [-1,1]) via int16, so it unit-tests headlessly by an
// encode→decode round-trip whose error matches the standard's quantization, plus the spec's exact anchor
// values (μ-law encodes silence to 0xFF).
namespace maz::audio {

namespace detail {

// --- μ-law (ITU-T G.711) ---
inline std::uint8_t linearToMuLaw(int sample) {
    const int kBias = 0x84;   // 132
    const int kClip = 32635;
    int sign = (sample >> 8) & 0x80; // sign bit from the top
    if (sign != 0) sample = -sample;
    if (sample > kClip) sample = kClip;
    sample += kBias;
    int exponent = 7;
    for (int mask = 0x4000; (sample & mask) == 0 && exponent > 0; mask >>= 1) --exponent;
    const int mantissa = (sample >> (exponent + 3)) & 0x0F;
    const int mu = ~(sign | (exponent << 4) | mantissa);
    return static_cast<std::uint8_t>(mu & 0xFF);
}

inline int muLawToLinear(std::uint8_t mu) {
    mu = static_cast<std::uint8_t>(~mu);
    const int sign = mu & 0x80;
    const int exponent = (mu >> 4) & 0x07;
    const int mantissa = mu & 0x0F;
    int sample = ((mantissa << 3) + 0x84) << exponent;
    sample -= 0x84;
    return sign != 0 ? -sample : sample;
}

// --- A-law (ITU-T G.711) ---
inline std::uint8_t linearToALaw(int sample) {
    int sign = ((~sample) >> 8) & 0x80; // A-law: sign is inverted vs μ-law
    if (sign == 0) sample = -sample;    // work with the magnitude of the negative side
    if (sample > 32635) sample = 32635;
    int compressed;
    if (sample >= 256) {
        int exponent = 7;
        for (int mask = 0x4000; (sample & mask) == 0 && exponent > 0; mask >>= 1) --exponent;
        const int mantissa = (sample >> (exponent + 3)) & 0x0F;
        compressed = (exponent << 4) | mantissa;
    } else {
        compressed = sample >> 4;
    }
    compressed ^= (sign ^ 0x55); // toggle even bits per the standard
    return static_cast<std::uint8_t>(compressed & 0xFF);
}

inline int aLawToLinear(std::uint8_t a) {
    a ^= 0x55;
    const int sign = a & 0x80;
    const int exponent = (a >> 4) & 0x07;
    const int mantissa = a & 0x0F;
    int sample;
    if (exponent == 0) {
        sample = (mantissa << 4) + 8;
    } else {
        sample = ((mantissa << 4) + 0x108) << (exponent - 1);
    }
    return sign != 0 ? sample : -sample;
}

inline int floatToS16(float s) {
    const int v = static_cast<int>(s * 32767.0f + (s < 0.0f ? -0.5f : 0.5f));
    return v < -32768 ? -32768 : (v > 32767 ? 32767 : v);
}

} // namespace detail

// One-sample helpers (exposed for testing / direct use).
inline std::uint8_t encodeMuLawSample(std::int16_t pcm) { return detail::linearToMuLaw(pcm); }
inline std::int16_t decodeMuLawSample(std::uint8_t mu) { return static_cast<std::int16_t>(detail::muLawToLinear(mu)); }
inline std::uint8_t encodeALawSample(std::int16_t pcm) { return detail::linearToALaw(pcm); }
inline std::int16_t decodeALawSample(std::uint8_t a) { return static_cast<std::int16_t>(detail::aLawToLinear(a)); }

// Encode WavData (float [-1,1]) to one μ-law byte per sample (channels stay interleaved).
inline std::vector<std::uint8_t> encodeMuLaw(const WavData& in) {
    std::vector<std::uint8_t> out;
    out.reserve(in.samples.size());
    for (float s : in.samples) out.push_back(detail::linearToMuLaw(detail::floatToS16(s)));
    return out;
}

// Decode μ-law bytes back to WavData floats. `channels`/`sampleRate` come from the container (WAV header).
inline WavData decodeMuLaw(const std::uint8_t* d, std::size_t n, std::uint16_t channels = 1,
                           std::uint32_t sampleRate = 8000) {
    WavData out;
    out.channels = channels ? channels : 1;
    out.sampleRate = sampleRate;
    out.samples.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        out.samples[i] = static_cast<float>(detail::muLawToLinear(d[i])) / 32768.0f;
    return out;
}
inline WavData decodeMuLaw(const std::vector<std::uint8_t>& b, std::uint16_t channels = 1,
                           std::uint32_t sampleRate = 8000) {
    return decodeMuLaw(b.data(), b.size(), channels, sampleRate);
}

// Encode WavData to one A-law byte per sample.
inline std::vector<std::uint8_t> encodeALaw(const WavData& in) {
    std::vector<std::uint8_t> out;
    out.reserve(in.samples.size());
    for (float s : in.samples) out.push_back(detail::linearToALaw(detail::floatToS16(s)));
    return out;
}
inline WavData decodeALaw(const std::uint8_t* d, std::size_t n, std::uint16_t channels = 1,
                          std::uint32_t sampleRate = 8000) {
    WavData out;
    out.channels = channels ? channels : 1;
    out.sampleRate = sampleRate;
    out.samples.resize(n);
    for (std::size_t i = 0; i < n; ++i)
        out.samples[i] = static_cast<float>(detail::aLawToLinear(d[i])) / 32768.0f;
    return out;
}
inline WavData decodeALaw(const std::vector<std::uint8_t>& b, std::uint16_t channels = 1,
                          std::uint32_t sampleRate = 8000) {
    return decodeALaw(b.data(), b.size(), channels, sampleRate);
}

} // namespace maz::audio
