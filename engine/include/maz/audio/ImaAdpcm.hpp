#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::audio IMA ADPCM — the classic 4-bit Adaptive Differential PCM codec (Interactive Multimedia
// Association / DVI), the one behind WAV format tag 0x11 and the sound banks of countless games. It squeezes
// 16-bit PCM down to 4 bits per sample — a flat 4:1 compression — by storing, per sample, only a 4-bit code
// for the DIFFERENCE from a running prediction, with an adaptive step size that grows on loud passages and
// shrinks on quiet ones. Decode is a handful of adds and shifts per sample (no tables of multiplies, no
// floating point), so it is cheap enough to decode hundreds of voices on any CPU. This complements the
// engine's other audio codecs: QOA (higher quality, ~3.2 bits/sample), G.711 (telephony 8-bit companding),
// and raw WAV — ADPCM is the tiny-and-fast option for short SFX where 4:1 with graceful quality is exactly
// right. The stream stores the first sample verbatim (so it is reproduced EXACTLY) plus the initial step
// index, then two 4-bit codes per byte. Header-only, std-only, deterministic. Godot has no ADPCM codec.
namespace maz::audio {

namespace detail {

// The 89-entry IMA step-size table and the 16-entry index adjustment table (the IMA/DVI standard).
inline const int* imaStepTable() {
    static const int kStep[89] = {
        7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45,
        50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173, 190, 209, 230, 253,
        279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796, 876, 963, 1060, 1166,
        1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
        4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289,
        16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
    return kStep;
}
inline const int* imaIndexTable() {
    static const int kIndex[16] = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
    return kIndex;
}

inline int clampSample(int v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return v;
}
inline int clampIndex(int i) {
    if (i < 0) return 0;
    if (i > 88) return 88;
    return i;
}

} // namespace detail

// Encoder/decoder state (running prediction + step index). Encode and decode must start from the same state.
struct AdpcmState {
    int predictor = 0; // last reconstructed sample
    int index = 0;     // index into the step table
};

// Encode one 16-bit sample to a 4-bit code, advancing the state exactly as the decoder will.
inline std::uint8_t adpcmEncodeSample(AdpcmState& st, std::int16_t sample) {
    const int* step = detail::imaStepTable();
    int stepSize = step[st.index];
    int diff = static_cast<int>(sample) - st.predictor;
    int code = 0;
    if (diff < 0) {
        code = 8;
        diff = -diff;
    }
    int vpdiff = stepSize >> 3;
    if (diff >= stepSize) { code |= 4; diff -= stepSize; vpdiff += stepSize; }
    stepSize >>= 1;
    if (diff >= stepSize) { code |= 2; diff -= stepSize; vpdiff += stepSize; }
    stepSize >>= 1;
    if (diff >= stepSize) { code |= 1; vpdiff += stepSize; }
    if (code & 8) {
        st.predictor = detail::clampSample(st.predictor - vpdiff);
    } else {
        st.predictor = detail::clampSample(st.predictor + vpdiff);
    }
    st.index = detail::clampIndex(st.index + detail::imaIndexTable()[code]);
    return static_cast<std::uint8_t>(code & 0x0F);
}

// Decode one 4-bit code back to a 16-bit sample, advancing the state.
inline std::int16_t adpcmDecodeSample(AdpcmState& st, std::uint8_t code) {
    const int stepSize = detail::imaStepTable()[st.index];
    int vpdiff = stepSize >> 3;
    if (code & 4) vpdiff += stepSize;
    if (code & 2) vpdiff += stepSize >> 1;
    if (code & 1) vpdiff += stepSize >> 2;
    if (code & 8) {
        st.predictor = detail::clampSample(st.predictor - vpdiff);
    } else {
        st.predictor = detail::clampSample(st.predictor + vpdiff);
    }
    st.index = detail::clampIndex(st.index + detail::imaIndexTable()[code & 0x0F]);
    return static_cast<std::int16_t>(st.predictor);
}

// Encode a whole mono PCM stream. Layout: int16 first-sample (little-endian), uint8 initial index (0),
// uint8 padding, then two 4-bit codes per byte (low nibble first) for samples[1..]. Empty in -> empty out.
inline std::vector<std::uint8_t> encodeImaAdpcm(const std::vector<std::int16_t>& pcm) {
    std::vector<std::uint8_t> out;
    if (pcm.empty()) {
        return out;
    }
    AdpcmState st;
    st.predictor = pcm[0];
    st.index = 0;
    const std::uint16_t p0 = static_cast<std::uint16_t>(pcm[0]);
    out.push_back(static_cast<std::uint8_t>(p0 & 0xFF));
    out.push_back(static_cast<std::uint8_t>((p0 >> 8) & 0xFF));
    out.push_back(0); // initial index
    out.push_back(0); // padding / reserved

    bool haveLow = false;
    std::uint8_t cur = 0;
    for (std::size_t i = 1; i < pcm.size(); ++i) {
        const std::uint8_t code = adpcmEncodeSample(st, pcm[i]);
        if (!haveLow) {
            cur = code;
            haveLow = true;
        } else {
            out.push_back(static_cast<std::uint8_t>(cur | (code << 4)));
            haveLow = false;
        }
    }
    if (haveLow) {
        out.push_back(cur);
    }
    return out;
}

// Decode a stream produced by encodeImaAdpcm back to `sampleCount` PCM samples.
inline std::vector<std::int16_t> decodeImaAdpcm(const std::vector<std::uint8_t>& data, std::size_t sampleCount) {
    std::vector<std::int16_t> out;
    if (sampleCount == 0 || data.size() < 4) {
        return out;
    }
    out.reserve(sampleCount);
    AdpcmState st;
    const std::uint16_t p0 = static_cast<std::uint16_t>(data[0] | (static_cast<std::uint16_t>(data[1]) << 8));
    st.predictor = static_cast<std::int16_t>(p0);
    st.index = detail::clampIndex(data[2]);
    out.push_back(static_cast<std::int16_t>(st.predictor)); // first sample verbatim

    std::size_t byte = 4;
    bool useHigh = false;
    while (out.size() < sampleCount && byte < data.size()) {
        std::uint8_t code;
        if (!useHigh) {
            code = static_cast<std::uint8_t>(data[byte] & 0x0F);
            useHigh = true;
        } else {
            code = static_cast<std::uint8_t>((data[byte] >> 4) & 0x0F);
            useHigh = false;
            ++byte;
        }
        out.push_back(adpcmDecodeSample(st, code));
    }
    return out;
}

} // namespace maz::audio
