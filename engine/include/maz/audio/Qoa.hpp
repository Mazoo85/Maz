#pragma once

#include "maz/audio/Wav.hpp" // audio::WavData

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::audio QOA (Quite OK Audio) codec — a compact lossy audio format that closes the "only WAV" gap in the
// asset pipeline. WAV is uncompressed (huge on disk); QOA is a fixed, dependency-free format that compresses
// PCM roughly 3-4x at good quality using a tiny per-channel LMS predictor plus 4-bit-scaled 3-bit residuals,
// with none of the patent/complexity baggage of MP3/Vorbis. This is a full, spec-accurate encoder AND
// decoder (the reference algorithm by Dominic Szablewski, ported to header-only C++): `encodeQoa` turns an
// `audio::WavData` into a `.qoa` byte stream, `decodeQoa` turns those bytes back into `WavData`. Because it
// is byte-in/byte-out and lossy-but-deterministic, it unit-tests headlessly by an ENCODE→DECODE round-trip:
// the decoded signal matches the input within QOA's bounded per-sample error — no external reference file
// needed. Mono or interleaved multi-channel, any sample rate.
//
// Scope note (honest): QOA v1 (magic "qoaf", static frames of up to 5120 samples/channel). Streaming
// playback hookup is the app's job; this provides the encode/decode.
namespace maz::audio {

namespace detail {

inline constexpr int kQoaLmsLen = 4;
inline constexpr int kQoaSlicesPerFrame = 256;
inline constexpr int kQoaFrameLen = kQoaSlicesPerFrame * 20; // 5120 samples per channel per frame
inline constexpr std::uint32_t kQoaMagic = 0x716f6166;       // "qoaf"

struct QoaLms {
    int history[kQoaLmsLen] = {0, 0, 0, 0};
    int weights[kQoaLmsLen] = {0, 0, 0, 0};
};

inline const int kQoaQuantTab[17] = {
    7, 7, 7, 5, 5, 3, 3, 1, /* -8..-1 */
    0,                      /*  0     */
    0, 2, 2, 4, 4, 6, 6, 6  /*  1.. 8 */
};
inline const int kQoaScalefactorTab[16] = {1, 7, 21, 45, 84, 138, 211, 304,
                                           421, 562, 731, 928, 1157, 1419, 1715, 2048};
inline const int kQoaReciprocalTab[16] = {65536, 9363, 3121, 1457, 781, 475, 311, 216,
                                          156, 117, 90, 71, 57, 47, 39, 32};
inline const int kQoaDequantTab[16][8] = {
    {1, -1, 3, -3, 5, -5, 7, -7},
    {5, -5, 18, -18, 32, -32, 49, -49},
    {16, -16, 53, -53, 95, -95, 147, -147},
    {34, -34, 113, -113, 203, -203, 315, -315},
    {63, -63, 210, -210, 378, -378, 588, -588},
    {104, -104, 345, -345, 621, -621, 966, -966},
    {158, -158, 528, -528, 950, -950, 1477, -1477},
    {228, -228, 760, -760, 1368, -1368, 2128, -2128},
    {316, -316, 1053, -1053, 1895, -1895, 2947, -2947},
    {422, -422, 1405, -1405, 2529, -2529, 3934, -3934},
    {548, -548, 1828, -1828, 3290, -3290, 5117, -5117},
    {696, -696, 2320, -2320, 4176, -4176, 6496, -6496},
    {868, -868, 2893, -2893, 5207, -5207, 8099, -8099},
    {1064, -1064, 3548, -3548, 6386, -6386, 9933, -9933},
    {1286, -1286, 4288, -4288, 7718, -7718, 12005, -12005},
    {1536, -1536, 5120, -5120, 9216, -9216, 14336, -14336},
};

inline int qoaLmsPredict(const QoaLms& lms) {
    int p = 0;
    for (int i = 0; i < kQoaLmsLen; ++i) p += lms.weights[i] * lms.history[i];
    return p >> 13;
}
inline void qoaLmsUpdate(QoaLms& lms, int sample, int residual) {
    const int delta = residual >> 4;
    for (int i = 0; i < kQoaLmsLen; ++i) lms.weights[i] += lms.history[i] < 0 ? -delta : delta;
    for (int i = 0; i < kQoaLmsLen - 1; ++i) lms.history[i] = lms.history[i + 1];
    lms.history[kQoaLmsLen - 1] = sample;
}
inline int qoaClamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline int qoaClampS16(int v) { return qoaClamp(v, -32768, 32767); }
inline int qoaDiv(int v, int scalefactor) {
    const int reciprocal = kQoaReciprocalTab[scalefactor];
    // v (a residual up to ±65535) times reciprocal (up to 65536) overflows int — signed-overflow UB. The
    // quotient after the >>16 is small, so widen the product to 64 bits and narrow the result back.
    int n = static_cast<int>((static_cast<std::int64_t>(v) * reciprocal + (1 << 15)) >> 16);
    n = n + ((v > 0) - (v < 0)) - ((n > 0) - (n < 0)); // round away from zero
    return n;
}

inline void putU16Be(std::vector<std::uint8_t>& out, unsigned v) {
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
}

} // namespace detail

// Encode PCM (WavData, float [-1,1]) into a QOA byte stream.
inline std::vector<std::uint8_t> encodeQoa(const WavData& in) {
    using namespace detail;
    std::vector<std::uint8_t> out;
    const int channels = in.channels ? in.channels : 1;
    const std::size_t frames = in.channels ? in.samples.size() / static_cast<std::size_t>(in.channels) : 0;
    if (frames == 0) return out;

    // Convert to int16 interleaved.
    std::vector<short> pcm(in.samples.size());
    for (std::size_t i = 0; i < in.samples.size(); ++i) {
        const float s = in.samples[i];
        const int v = static_cast<int>(s * 32767.0f + (s < 0.0f ? -0.5f : 0.5f));
        pcm[i] = static_cast<short>(qoaClampS16(v));
    }

    // File header: "qoaf" + total samples-per-channel (BE u32).
    out.push_back('q'); out.push_back('o'); out.push_back('a'); out.push_back('f');
    const std::uint32_t total = static_cast<std::uint32_t>(frames);
    out.push_back(static_cast<std::uint8_t>((total >> 24) & 0xff));
    out.push_back(static_cast<std::uint8_t>((total >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((total >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>(total & 0xff));

    // Running LMS per channel (reference init).
    std::vector<QoaLms> lms(static_cast<std::size_t>(channels));
    for (auto& l : lms) {
        l.weights[0] = 0; l.weights[1] = 0; l.weights[2] = -(1 << 13); l.weights[3] = 1 << 14;
    }

    for (std::size_t frameStart = 0; frameStart < frames;
         frameStart += static_cast<std::size_t>(kQoaFrameLen)) {
        const std::size_t frameLen =
            (frames - frameStart) < static_cast<std::size_t>(kQoaFrameLen)
                ? (frames - frameStart)
                : static_cast<std::size_t>(kQoaFrameLen);
        const unsigned slices = static_cast<unsigned>((frameLen + 19) / 20);
        const unsigned frameSize =
            8u + static_cast<unsigned>(kQoaLmsLen) * 4u * static_cast<unsigned>(channels) +
            8u * slices * static_cast<unsigned>(channels);

        // Frame header.
        out.push_back(static_cast<std::uint8_t>(channels));
        out.push_back(static_cast<std::uint8_t>((in.sampleRate >> 16) & 0xff));
        out.push_back(static_cast<std::uint8_t>((in.sampleRate >> 8) & 0xff));
        out.push_back(static_cast<std::uint8_t>(in.sampleRate & 0xff));
        putU16Be(out, static_cast<unsigned>(frameLen));
        putU16Be(out, frameSize);

        // LMS state for each channel.
        for (int c = 0; c < channels; ++c) {
            for (int i = 0; i < kQoaLmsLen; ++i)
                putU16Be(out, static_cast<unsigned>(static_cast<std::uint16_t>(lms[static_cast<std::size_t>(c)].history[i])));
            for (int i = 0; i < kQoaLmsLen; ++i)
                putU16Be(out, static_cast<unsigned>(static_cast<std::uint16_t>(lms[static_cast<std::size_t>(c)].weights[i])));
        }

        for (std::size_t si = 0; si < frameLen; si += 20) {
            const std::size_t sliceLen = (frameLen - si) < 20 ? (frameLen - si) : 20;
            for (int c = 0; c < channels; ++c) {
                const std::size_t base = (frameStart + si) * static_cast<std::size_t>(channels) +
                                         static_cast<std::size_t>(c);

                long long bestError = -1;
                std::uint64_t bestSlice = 0;
                QoaLms bestLms = lms[static_cast<std::size_t>(c)];

                for (int sf = 0; sf < 16; ++sf) {
                    QoaLms cur = lms[static_cast<std::size_t>(c)];
                    std::uint64_t slice = static_cast<std::uint64_t>(sf);
                    long long curError = 0;
                    for (std::size_t k = 0; k < sliceLen; ++k) {
                        const int sample = pcm[base + k * static_cast<std::size_t>(channels)];
                        const int predicted = qoaLmsPredict(cur);
                        const int residual = sample - predicted;
                        const int scaled = qoaDiv(residual, sf);
                        const int clamped = qoaClamp(scaled, -8, 8);
                        const int quantized = kQoaQuantTab[clamped + 8];
                        const int dequantized = kQoaDequantTab[sf][quantized];
                        const int reconstructed = qoaClampS16(predicted + dequantized);
                        const long long e = sample - reconstructed;
                        curError += e * e;
                        if (bestError >= 0 && curError > bestError) break;
                        qoaLmsUpdate(cur, reconstructed, dequantized);
                        slice = (slice << 3) | static_cast<std::uint64_t>(quantized);
                    }
                    if (bestError < 0 || curError < bestError) {
                        bestError = curError;
                        bestSlice = slice;
                        bestLms = cur;
                    }
                }

                lms[static_cast<std::size_t>(c)] = bestLms;
                bestSlice <<= (20 - sliceLen) * 3; // left-align a partial final slice
                for (int b = 0; b < 8; ++b)
                    out.push_back(static_cast<std::uint8_t>((bestSlice >> (56 - b * 8)) & 0xff));
            }
        }
    }
    return out;
}

// Decode a QOA byte stream into PCM (WavData, float [-1,1]). Returns an empty WavData on malformed input.
inline WavData decodeQoa(const std::uint8_t* d, std::size_t n) {
    using namespace detail;
    WavData out;
    if (n < 8 || d[0] != 'q' || d[1] != 'o' || d[2] != 'a' || d[3] != 'f') return WavData();
    const std::uint32_t total = (static_cast<std::uint32_t>(d[4]) << 24) |
                                (static_cast<std::uint32_t>(d[5]) << 16) |
                                (static_cast<std::uint32_t>(d[6]) << 8) | static_cast<std::uint32_t>(d[7]);
    if (total == 0) return WavData();

    std::size_t p = 8;
    std::vector<short> pcm;
    std::uint16_t channels = 0;
    std::uint32_t sampleRate = 0;
    std::size_t decoded = 0;

    while (decoded < total) {
        if (p + 8 > n) return WavData();
        const int ch = d[p];
        const std::uint32_t sr = (static_cast<std::uint32_t>(d[p + 1]) << 16) |
                                 (static_cast<std::uint32_t>(d[p + 2]) << 8) |
                                 static_cast<std::uint32_t>(d[p + 3]);
        const unsigned fsamples = (static_cast<unsigned>(d[p + 4]) << 8) | d[p + 5];
        if (ch <= 0 || fsamples == 0) return WavData();
        if (channels == 0) {
            channels = static_cast<std::uint16_t>(ch);
            sampleRate = sr;
            // Guard against a corrupt/hostile header declaring an enormous sample count: QOA packs at most
            // 20 samples per channel into each 8-byte slice, so the decoded interleaved length can never
            // exceed ~2.5x the input byte count. Reject anything wildly larger rather than attempting a
            // multi-gigabyte allocation (which would OOM-crash the process on a malicious .qoa file).
            const std::uint64_t claimed =
                static_cast<std::uint64_t>(total) * static_cast<std::uint64_t>(ch);
            if (claimed > static_cast<std::uint64_t>(n) * 3u) return WavData();
            pcm.assign(static_cast<std::size_t>(claimed), 0);
        }
        p += 8;

        std::vector<QoaLms> lms(static_cast<std::size_t>(ch));
        for (int c = 0; c < ch; ++c) {
            for (int i = 0; i < kQoaLmsLen; ++i) {
                if (p + 2 > n) return WavData();
                lms[static_cast<std::size_t>(c)].history[i] =
                    static_cast<std::int16_t>((d[p] << 8) | d[p + 1]);
                p += 2;
            }
            for (int i = 0; i < kQoaLmsLen; ++i) {
                if (p + 2 > n) return WavData();
                lms[static_cast<std::size_t>(c)].weights[i] =
                    static_cast<std::int16_t>((d[p] << 8) | d[p + 1]);
                p += 2;
            }
        }

        for (unsigned si = 0; si < fsamples; si += 20) {
            for (int c = 0; c < ch; ++c) {
                if (p + 8 > n) return WavData();
                std::uint64_t slice = 0;
                for (int b = 0; b < 8; ++b) slice = (slice << 8) | d[p + static_cast<std::size_t>(b)];
                p += 8;
                const int sf = static_cast<int>((slice >> 60) & 0xf);
                const unsigned sliceLen = (fsamples - si) < 20 ? (fsamples - si) : 20;
                std::uint64_t bits = slice << 4; // drop the 4-bit scalefactor; residuals start at bit 60
                for (unsigned k = 0; k < sliceLen; ++k) {
                    const int predicted = qoaLmsPredict(lms[static_cast<std::size_t>(c)]);
                    const int quantized = static_cast<int>((bits >> 61) & 0x7);
                    bits <<= 3;
                    const int dequantized = kQoaDequantTab[sf][quantized];
                    const int reconstructed = qoaClampS16(predicted + dequantized);
                    const std::size_t idx =
                        (decoded + si + k) * static_cast<std::size_t>(ch) + static_cast<std::size_t>(c);
                    if (idx < pcm.size()) pcm[idx] = static_cast<short>(reconstructed);
                    qoaLmsUpdate(lms[static_cast<std::size_t>(c)], reconstructed, dequantized);
                }
            }
        }
        decoded += fsamples;
    }

    out.channels = channels;
    out.sampleRate = sampleRate;
    out.samples.resize(pcm.size());
    for (std::size_t i = 0; i < pcm.size(); ++i) out.samples[i] = static_cast<float>(pcm[i]) / 32768.0f;
    return out;
}

inline WavData decodeQoa(const std::vector<std::uint8_t>& bytes) {
    return decodeQoa(bytes.data(), bytes.size());
}

} // namespace maz::audio
