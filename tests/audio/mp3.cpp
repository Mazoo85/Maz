// tests/audio/mp3.cpp — verifies the MPEG-audio (MP3) frame parser + seek index (audio::parseMp3FrameHeader,
// audio::scanMp3). Ground truths, all exact per the MPEG-1/2/2.5 Layer I/II/III spec, deterministic:
//   * a canonical 128 kbps/44.1 kHz MPEG-1 Layer III header (FF FB 90 00) parses to the right version,
//     layer, bitrate, sample rate, channels, samples-per-frame (1152), and frame length (417 bytes);
//   * an MPEG-2 Layer III header reports 576 samples-per-frame and its (shorter) frame length;
//   * an invalid sync word is rejected;
//   * scanMp3 walks a multi-frame buffer, builds the frame index, and totals samples -> duration;
//   * a leading ID3v2 tag is skipped before framing begins;
//   * pure garbage yields zero frames.
// Frame headers are hand-constructed from the spec (no external files), so correctness is fully checked.
#include "maz/audio/Mp3.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::audio::Mp3FrameHeader;
using maz::audio::Mp3Info;
using maz::audio::MpegLayer;
using maz::audio::MpegVersion;
using maz::audio::parseMp3FrameHeader;
using maz::audio::scanMp3;

// Build one MPEG-1 Layer III frame: header FF FB 90 00 (128 kbps, 44100 Hz, stereo, no pad) + zeros.
static std::vector<uint8_t> mpeg1L3Frame() {
    std::vector<uint8_t> f(417, 0);
    f[0] = 0xFF; f[1] = 0xFB; f[2] = 0x90; f[3] = 0x00;
    return f;
}

int main() {
    // --- 1. Canonical MPEG-1 Layer III header. ---
    {
        const uint8_t hdr[4] = {0xFF, 0xFB, 0x90, 0x00};
        const Mp3FrameHeader h = parseMp3FrameHeader(hdr, 4);
        CHECK(h.valid, "FF FB 90 00 is a valid frame");
        CHECK(h.version == MpegVersion::Mpeg1, "MPEG-1");
        CHECK(h.layer == MpegLayer::LayerIII, "Layer III");
        CHECK(h.bitrateKbps == 128, "128 kbps");
        CHECK(h.sampleRate == 44100, "44100 Hz");
        CHECK(h.channels == 2, "stereo -> 2 channels");
        CHECK(h.samplesPerFrame == 1152, "MPEG-1 L3 = 1152 samples/frame");
        CHECK(h.frameLength == 417, "frame length 417 bytes");
    }

    // --- 2. MPEG-2 Layer III header: FF F3 80 00 (64 kbps, 22050 Hz, stereo). ---
    {
        const uint8_t hdr[4] = {0xFF, 0xF3, 0x80, 0x00};
        const Mp3FrameHeader h = parseMp3FrameHeader(hdr, 4);
        CHECK(h.valid, "FF F3 80 00 is valid");
        CHECK(h.version == MpegVersion::Mpeg2, "MPEG-2");
        CHECK(h.layer == MpegLayer::LayerIII, "Layer III");
        CHECK(h.bitrateKbps == 64 && h.sampleRate == 22050, "64 kbps / 22050 Hz");
        CHECK(h.samplesPerFrame == 576, "MPEG-2 L3 = 576 samples/frame");
        // frameLength = 72 * 64000 / 22050 = 208 bytes.
        CHECK(h.frameLength == 208, "MPEG-2 L3 frame length 208 bytes");
    }

    // --- 3. Invalid sync is rejected. ---
    {
        const uint8_t bad[4] = {0xFF, 0x1F, 0x90, 0x00}; // sync bits not all set
        CHECK(!parseMp3FrameHeader(bad, 4).valid, "bad sync rejected");
        const uint8_t freeFmt[4] = {0xFF, 0xFB, 0x00, 0x00}; // bitrate index 0 = free
        CHECK(!parseMp3FrameHeader(freeFmt, 4).valid, "free-format bitrate not framed");
    }

    // --- 4. scanMp3 over three concatenated frames. ---
    {
        std::vector<uint8_t> buf;
        for (int k = 0; k < 3; ++k) {
            const auto f = mpeg1L3Frame();
            buf.insert(buf.end(), f.begin(), f.end());
        }
        const Mp3Info info = scanMp3(buf);
        CHECK(info.frames.size() == 3, "three frames indexed");
        CHECK(info.sampleRate == 44100 && info.channels == 2, "stream sample rate/channels");
        CHECK(info.totalSamples == 3u * 1152u, "total samples summed");
        CHECK(info.frames[1].offset == 417, "second frame offset follows the first");
        const double expected = (3.0 * 1152.0) / 44100.0;
        CHECK(std::fabs(info.durationSeconds - expected) < 1e-9, "duration = samples / rate");
        CHECK(info.audioStart == 0, "no ID3 tag -> audio starts at 0");
    }

    // --- 5. Leading ID3v2 tag is skipped. ---
    {
        std::vector<uint8_t> buf;
        // ID3v2 header: "ID3", version 4.0, flags 0, syncsafe size = 10 bytes of tag body.
        const uint8_t id3[10] = {'I', 'D', '3', 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0A};
        buf.insert(buf.end(), id3, id3 + 10);
        buf.insert(buf.end(), 10, 0x00); // 10-byte tag body
        for (int k = 0; k < 2; ++k) {
            const auto f = mpeg1L3Frame();
            buf.insert(buf.end(), f.begin(), f.end());
        }
        const Mp3Info info = scanMp3(buf);
        CHECK(info.audioStart == 20, "ID3v2 tag (10 header + 10 body) skipped");
        CHECK(info.frames.size() == 2, "frames after the tag indexed");
        CHECK(info.totalSamples == 2u * 1152u, "samples counted past the tag");
    }

    // --- 6. Garbage yields no frames. ---
    {
        std::vector<uint8_t> junk(200, 0x37);
        const Mp3Info info = scanMp3(junk);
        CHECK(info.frames.empty(), "no frames in garbage");
        CHECK(info.durationSeconds == 0.0, "zero duration");
    }

    if (g_fail == 0) {
        std::printf("mp3: OK — header parse (MPEG-1/2 L3), invalid rejection, frame index, duration, "
                    "ID3v2 skip, garbage.\n");
        return 0;
    }
    std::printf("mp3: %d failure(s).\n", g_fail);
    return 1;
}
