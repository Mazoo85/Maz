// tests/video/ivf.cpp — verifies the IVF video container demuxer (video::parseIvfHeader, video::demuxIvf).
// Ground truths, all exact per the IVF spec, deterministic:
//   * the 32-byte file header parses (DKIF signature, codec FourCC, dimensions, frame rate, count);
//   * fps and duration derive from the rate numerator/denominator and frame count;
//   * demuxIvf walks the 12-byte frame headers, extracting each frame's timestamp, payload offset and
//     size, and the payload bytes at those offsets are exactly the ones written;
//   * a bad signature is rejected;
//   * a truncated final frame (claiming more bytes than remain) is dropped cleanly.
// The bitstream is hand-built from the spec (no external files), so every field is checked.
#include "maz/video/Ivf.hpp"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::video::IvfVideo;
using maz::video::demuxIvf;
using maz::video::parseIvfHeader;

static void putU16(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xFF));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
static void putU32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) {
        b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
}
static void putU64(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        b.push_back(static_cast<uint8_t>((v >> (8 * i)) & 0xFF));
    }
}
static void putStr(std::vector<uint8_t>& b, const char* s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        b.push_back(static_cast<uint8_t>(s[i]));
    }
}

// Build a valid IVF file: header + two frames (payloads {AA BB CC DD} @ts0, {11 22 33} @ts1).
static std::vector<uint8_t> buildIvf() {
    std::vector<uint8_t> b;
    putStr(b, "DKIF", 4);   // signature
    putU16(b, 0);           // version
    putU16(b, 32);          // header length
    putStr(b, "VP80", 4);   // codec FourCC
    putU16(b, 320);         // width
    putU16(b, 240);         // height
    putU32(b, 30);          // rate numerator
    putU32(b, 1);           // rate denominator -> 30 fps
    putU32(b, 2);           // frame count
    putU32(b, 0);           // reserved

    // Frame 0: size 4, ts 0.
    putU32(b, 4);
    putU64(b, 0);
    b.push_back(0xAA); b.push_back(0xBB); b.push_back(0xCC); b.push_back(0xDD);
    // Frame 1: size 3, ts 1.
    putU32(b, 3);
    putU64(b, 1);
    b.push_back(0x11); b.push_back(0x22); b.push_back(0x33);
    return b;
}

int main() {
    const std::vector<uint8_t> file = buildIvf();

    // --- 1. Header. ---
    {
        const auto h = parseIvfHeader(file.data(), file.size());
        CHECK(h.valid, "DKIF header parses");
        CHECK(h.codec() == "VP80", "codec FourCC VP80");
        CHECK(h.width == 320 && h.height == 240, "dimensions 320x240");
        CHECK(h.rateNum == 30 && h.rateDen == 1, "rate 30/1");
        CHECK(h.frameCount == 2, "frame count 2");
        CHECK(std::fabs(h.fps() - 30.0) < 1e-9, "fps = 30");
    }

    // --- 2. Demux frames. ---
    {
        const IvfVideo v = demuxIvf(file);
        CHECK(v.header.valid, "demux header valid");
        CHECK(v.frames.size() == 2, "two frames demuxed");
        CHECK(v.frames[0].timestamp == 0 && v.frames[0].size == 4, "frame0 ts/size");
        CHECK(v.frames[1].timestamp == 1 && v.frames[1].size == 3, "frame1 ts/size");
        // frame0 payload begins at 32 (header) + 12 (frame header) = 44.
        CHECK(v.frames[0].offset == 44, "frame0 payload offset");
        CHECK(file[v.frames[0].offset] == 0xAA && file[v.frames[0].offset + 3] == 0xDD,
              "frame0 payload bytes match");
        // frame1 payload begins at 44 + 4 + 12 = 60.
        CHECK(v.frames[1].offset == 60, "frame1 payload offset");
        CHECK(file[v.frames[1].offset] == 0x11 && file[v.frames[1].offset + 2] == 0x33,
              "frame1 payload bytes match");
        CHECK(std::fabs(v.durationSeconds() - 2.0 / 30.0) < 1e-9, "duration = frames / fps");
    }

    // --- 3. Bad signature rejected. ---
    {
        std::vector<uint8_t> bad = file;
        bad[0] = 'X';
        const IvfVideo v = demuxIvf(bad);
        CHECK(!v.header.valid, "non-DKIF rejected");
        CHECK(v.frames.empty(), "no frames from invalid header");
    }

    // --- 4. Truncated final frame is dropped. ---
    {
        std::vector<uint8_t> trunc = file;
        trunc.pop_back(); // drop the last payload byte of frame1
        const IvfVideo v = demuxIvf(trunc);
        CHECK(v.frames.size() == 1, "truncated final frame dropped, first frame kept");
    }

    if (g_fail == 0) {
        std::printf("ivf: OK — header, fps/duration, frame demux + payload offsets, bad-sig, truncation.\n");
        return 0;
    }
    std::printf("ivf: %d failure(s).\n", g_fail);
    return 1;
}
