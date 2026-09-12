#pragma once

#include <cstdint>
#include <string>
#include <vector>

// maz::audio MPEG audio (MP3) frame parsing + seek index — the demux/metadata half of MP3 support:
// locate every MPEG audio frame in a buffer, read its header (version, layer, bitrate, sample rate,
// channels), compute frame lengths, skip a leading ID3v2 tag, and total up samples/duration for a
// seek table. This is the layer a player runs BEFORE decoding: you cannot decode or seek an MP3
// without first framing it, and duration/seek metadata is what most apps need first.
//
// Honest scope: this is the container/framing + metadata layer, not the Layer III audio codec. The
// heavy DSP that turns frame payloads into PCM samples (Huffman decode, IMDCT, the synthesis
// filterbank) is a separate, large, patent-adjacent step — the engine already ships dependency-free
// compressed audio via QOA and uncompressed via WAV, so playback is not blocked on it. Everything
// here is exact per the MPEG-1/2/2.5 Layer I/II/III specification and unit-tested against
// hand-constructed frame headers, so it is fully verifiable headlessly.
namespace maz::audio {

enum class MpegVersion { Mpeg25, Reserved, Mpeg2, Mpeg1 };
enum class MpegLayer { Reserved, LayerIII, LayerII, LayerI };

// A parsed MPEG audio frame header.
struct Mp3FrameHeader {
    bool valid = false;
    MpegVersion version = MpegVersion::Reserved;
    MpegLayer layer = MpegLayer::Reserved;
    int bitrateKbps = 0;      // 0 = free/invalid
    int sampleRate = 0;       // Hz; 0 = invalid
    int channels = 0;         // 1 (mono) or 2
    bool padding = false;
    int samplesPerFrame = 0;  // 384 / 1152 / 576 depending on version+layer
    int frameLength = 0;      // total bytes of this frame (header + payload)
};

namespace detail {

// Bitrate tables in kbps, indexed [table][index]. Index 0 = free, 15 = invalid (0 here).
// tableId: 0=MPEG1 LayerI, 1=MPEG1 LayerII, 2=MPEG1 LayerIII, 3=MPEG2/2.5 LayerI,
//          4=MPEG2/2.5 LayerII&III.
inline int bitrateKbps(int tableId, int index) {
    static const int t[5][16] = {
        {0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 0},
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 0},
        {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0},
        {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256, 0},
        {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160, 0},
    };
    if (index < 0 || index > 15) {
        return 0;
    }
    return t[tableId][index];
}

inline int sampleRateHz(MpegVersion v, int index) {
    static const int t[4][4] = {
        {11025, 12000, 8000, 0}, // MPEG2.5
        {0, 0, 0, 0},            // reserved
        {22050, 24000, 16000, 0},// MPEG2
        {44100, 48000, 32000, 0},// MPEG1
    };
    if (index < 0 || index > 3) {
        return 0;
    }
    return t[static_cast<int>(v)][index];
}

} // namespace detail

// Parse a 4-byte MPEG audio frame header from `p` (with `avail` bytes readable). Returns a header with
// valid=false if the sync word, version, layer, bitrate, or sample rate are invalid.
inline Mp3FrameHeader parseMp3FrameHeader(const uint8_t* p, size_t avail) {
    Mp3FrameHeader h;
    if (avail < 4) {
        return h;
    }
    const uint8_t h0 = p[0];
    const uint8_t h1 = p[1];
    const uint8_t h2 = p[2];
    const uint8_t h3 = p[3];
    // Frame sync: 11 set bits.
    if (h0 != 0xFF || (h1 & 0xE0) != 0xE0) {
        return h;
    }
    h.version = static_cast<MpegVersion>((h1 >> 3) & 0x3);
    h.layer = static_cast<MpegLayer>((h1 >> 1) & 0x3);
    if (h.version == MpegVersion::Reserved || h.layer == MpegLayer::Reserved) {
        return h;
    }
    const int bitrateIndex = (h2 >> 4) & 0xF;
    const int sampleIndex = (h2 >> 2) & 0x3;
    h.padding = ((h2 >> 1) & 0x1) != 0;
    const int channelMode = (h3 >> 6) & 0x3;
    h.channels = channelMode == 3 ? 1 : 2;

    const bool mpeg1 = h.version == MpegVersion::Mpeg1;
    int tableId = 0;
    if (mpeg1) {
        tableId = h.layer == MpegLayer::LayerI ? 0 : (h.layer == MpegLayer::LayerII ? 1 : 2);
    } else {
        tableId = h.layer == MpegLayer::LayerI ? 3 : 4;
    }
    h.bitrateKbps = detail::bitrateKbps(tableId, bitrateIndex);
    h.sampleRate = detail::sampleRateHz(h.version, sampleIndex);
    if (bitrateIndex == 0 || bitrateIndex == 15 || h.bitrateKbps == 0 || h.sampleRate == 0) {
        return h; // free-format or invalid rate: not framed here
    }

    // Samples per frame: Layer I = 384; Layer II = 1152; Layer III = 1152 (MPEG1) / 576 (MPEG2/2.5).
    if (h.layer == MpegLayer::LayerI) {
        h.samplesPerFrame = 384;
    } else if (h.layer == MpegLayer::LayerII) {
        h.samplesPerFrame = 1152;
    } else {
        h.samplesPerFrame = mpeg1 ? 1152 : 576;
    }

    const int bitrate = h.bitrateKbps * 1000;
    const int pad = h.padding ? 1 : 0;
    if (h.layer == MpegLayer::LayerI) {
        // slot size 4 bytes.
        h.frameLength = (12 * bitrate / h.sampleRate + pad) * 4;
    } else {
        // slot size 1 byte; samplesPerFrame/8 = 144 (1152) or 72 (576).
        h.frameLength = (h.samplesPerFrame / 8) * bitrate / h.sampleRate + pad;
    }
    h.valid = h.frameLength > 4;
    return h;
}

// The result of scanning a whole MP3 buffer: a frame seek index plus totals.
struct Mp3Info {
    struct Frame {
        size_t offset = 0;   // byte offset of the frame in the buffer
        int length = 0;      // frame length in bytes
        int samples = 0;     // samples per channel in this frame
    };
    std::vector<Frame> frames;
    int sampleRate = 0;
    int channels = 0;
    uint64_t totalSamples = 0;   // per channel, summed across frames
    double durationSeconds = 0.0;
    size_t audioStart = 0;       // offset where audio framing began (after any ID3v2 tag)
};

// If the buffer begins with an ID3v2 tag, return the number of bytes to skip (10-byte header +
// syncsafe size), else 0.
inline size_t id3v2Size(const uint8_t* p, size_t n) {
    if (n < 10 || p[0] != 'I' || p[1] != 'D' || p[2] != '3') {
        return 0;
    }
    // Bytes 6..9 are a 28-bit syncsafe integer (7 bits per byte).
    const size_t size = (static_cast<size_t>(p[6] & 0x7F) << 21) |
                        (static_cast<size_t>(p[7] & 0x7F) << 14) |
                        (static_cast<size_t>(p[8] & 0x7F) << 7) |
                        (static_cast<size_t>(p[9] & 0x7F));
    return 10 + size;
}

// Scan an MP3 byte buffer into a frame index. Skips a leading ID3v2 tag, then walks frame-by-frame
// (resyncing one byte at a time past any junk). Reports sample rate/channels from the first valid
// frame and totals the samples for duration.
inline Mp3Info scanMp3(const uint8_t* data, size_t n) {
    Mp3Info info;
    size_t i = id3v2Size(data, n);
    info.audioStart = i;
    while (i + 4 <= n) {
        const Mp3FrameHeader h = parseMp3FrameHeader(data + i, n - i);
        if (!h.valid || i + static_cast<size_t>(h.frameLength) > n) {
            if (!h.valid) {
                ++i; // resync
                continue;
            }
            break; // valid header but frame runs past the buffer end
        }
        if (info.frames.empty()) {
            info.sampleRate = h.sampleRate;
            info.channels = h.channels;
        }
        info.frames.push_back(Mp3Info::Frame{i, h.frameLength, h.samplesPerFrame});
        info.totalSamples += static_cast<uint64_t>(h.samplesPerFrame);
        i += static_cast<size_t>(h.frameLength);
    }
    if (info.sampleRate > 0) {
        info.durationSeconds =
            static_cast<double>(info.totalSamples) / static_cast<double>(info.sampleRate);
    }
    return info;
}

inline Mp3Info scanMp3(const std::vector<uint8_t>& bytes) {
    return scanMp3(bytes.data(), bytes.size());
}

} // namespace maz::audio
