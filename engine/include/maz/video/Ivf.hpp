#pragma once

#include <cstdint>
#include <string>
#include <vector>

// maz::video IVF container demuxer — the container/framing half of Godot's video playback. IVF is the
// simplest, fully-specified video container (a 32-byte file header + a 12-byte header before each
// compressed frame), used to wrap VP8/VP9/AV1 bitstreams. Demuxing means: read the file header (codec
// FourCC, dimensions, frame rate, frame count), then walk the file pulling out each compressed frame's
// bytes and presentation timestamp. That is the step a player runs BEFORE handing frame payloads to the
// video codec — you cannot decode or seek video without first demuxing it, and dimensions/fps/duration
// are what a UI needs first.
//
// Honest scope: this is the container demuxer + metadata, not the VP8/VP9/AV1 codec. Turning a frame's
// compressed bytes into pixels is a separate, very large codec (and its display is GPU-side). Everything
// here is exact per the IVF spec and unit-tested against a hand-constructed bitstream, so it is fully
// verifiable headlessly.
namespace maz::video {

inline uint16_t readU16le(const uint8_t* p) {
    return static_cast<uint16_t>(static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8));
}
inline uint32_t readU32le(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
}
inline uint64_t readU64le(const uint8_t* p) {
    return static_cast<uint64_t>(readU32le(p)) | (static_cast<uint64_t>(readU32le(p + 4)) << 32);
}

// The 32-byte IVF file header.
struct IvfHeader {
    bool valid = false;
    char fourcc[4] = {0, 0, 0, 0}; // codec, e.g. "VP80", "VP90", "AV01"
    uint16_t version = 0;
    uint16_t headerLength = 32;
    uint16_t width = 0;
    uint16_t height = 0;
    uint32_t rateNum = 0; // frame-rate numerator (time base)
    uint32_t rateDen = 0; // frame-rate denominator
    uint32_t frameCount = 0;

    std::string codec() const { return std::string(fourcc, 4); }
    double fps() const {
        return rateDen != 0 ? static_cast<double>(rateNum) / static_cast<double>(rateDen) : 0.0;
    }
};

// One demuxed frame: its presentation timestamp and where its compressed payload sits in the buffer.
struct IvfFrame {
    uint64_t timestamp = 0; // presentation time, in time-base units
    size_t offset = 0;      // byte offset of the compressed payload within the buffer
    uint32_t size = 0;      // payload size in bytes
};

struct IvfVideo {
    IvfHeader header;
    std::vector<IvfFrame> frames;

    // Duration from the header's frame count and frame rate (frameCount / fps).
    double durationSeconds() const {
        const double f = header.fps();
        return f > 0.0 ? static_cast<double>(header.frameCount) / f : 0.0;
    }
};

// Parse just the 32-byte file header. valid=false unless the "DKIF" signature is present.
inline IvfHeader parseIvfHeader(const uint8_t* data, size_t n) {
    IvfHeader h;
    if (n < 32 || data[0] != 'D' || data[1] != 'K' || data[2] != 'I' || data[3] != 'F') {
        return h;
    }
    h.version = readU16le(data + 4);
    h.headerLength = readU16le(data + 6);
    h.fourcc[0] = static_cast<char>(data[8]);
    h.fourcc[1] = static_cast<char>(data[9]);
    h.fourcc[2] = static_cast<char>(data[10]);
    h.fourcc[3] = static_cast<char>(data[11]);
    h.width = readU16le(data + 12);
    h.height = readU16le(data + 14);
    h.rateNum = readU32le(data + 16);
    h.rateDen = readU32le(data + 20);
    h.frameCount = readU32le(data + 24);
    h.valid = true;
    return h;
}

// Demux a whole IVF buffer: parse the header, then walk each 12-byte frame header + payload into an
// index. Stops cleanly if a frame claims more bytes than remain (truncated file).
inline IvfVideo demuxIvf(const uint8_t* data, size_t n) {
    IvfVideo v;
    v.header = parseIvfHeader(data, n);
    if (!v.header.valid) {
        return v;
    }
    size_t i = v.header.headerLength >= 32 ? v.header.headerLength : 32;
    while (i + 12 <= n) {
        const uint32_t size = readU32le(data + i);
        const uint64_t ts = readU64le(data + i + 4);
        const size_t payload = i + 12;
        if (payload + static_cast<size_t>(size) > n) {
            break; // truncated frame
        }
        v.frames.push_back(IvfFrame{ts, payload, size});
        i = payload + static_cast<size_t>(size);
    }
    return v;
}

inline IvfVideo demuxIvf(const std::vector<uint8_t>& bytes) {
    return demuxIvf(bytes.data(), bytes.size());
}

} // namespace maz::video
