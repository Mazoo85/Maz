// Tests for the animated GIF encoder.
//
// A GIF is a container with a shape, and the only way to know an encoder wrote a real one is to WALK
// that shape rather than to look for bytes: 0x2C and 0x21 both occur constantly inside LZW data, so
// counting them in the file finds frames that are not there. The walker below is the test's own
// miniature parser, and it is what makes "24 frames" mean 24 frames.
#include "maz/render/ImageCodecGif.hpp"
#include "maz/render/ImageCodecGifAnim.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::render::Color;
using maz::render::Image;

// What a structural walk of the file found.
struct Walk {
    bool ok = false;         // the whole file parsed, ending at the trailer
    int frames = 0;          // image descriptors
    int loopBlocks = 0;      // NETSCAPE2.0 application extensions
    int loopCount = -1;      // the count inside the first one
    int firstDelay = -1;     // the delay on the first graphic control extension
    int width = 0;
    int height = 0;
};

// Skip a chain of length-prefixed sub-blocks, ending on the zero terminator.
bool skipSubBlocks(const std::vector<std::uint8_t>& d, std::size_t& p) {
    while (p < d.size() && d[p] != 0x00) {
        p += 1u + d[p];
    }
    if (p >= d.size()) {
        return false;
    }
    ++p; // the terminator
    return true;
}

Walk walk(const std::vector<std::uint8_t>& d) {
    Walk w;
    if (d.size() < 14 || std::string(d.begin(), d.begin() + 6) != "GIF89a") {
        return w;
    }
    w.width = d[6] | (d[7] << 8);
    w.height = d[8] | (d[9] << 8);
    const bool globalTable = (d[10] & 0x80) != 0;
    const int bits = (d[10] & 0x07) + 1;
    std::size_t p = 13;
    if (globalTable) {
        p += 3u * (1u << bits);
    }
    while (p < d.size()) {
        const std::uint8_t b = d[p++];
        if (b == 0x3B) {
            w.ok = (p == d.size());
            return w;
        }
        if (b == 0x21) { // extension
            if (p >= d.size()) return w;
            const std::uint8_t label = d[p++];
            if (label == 0xFF && p + 12 <= d.size() &&
                std::string(d.begin() + static_cast<std::ptrdiff_t>(p) + 1,
                            d.begin() + static_cast<std::ptrdiff_t>(p) + 12) == "NETSCAPE2.0") {
                ++w.loopBlocks;
                // 11-byte identifier block, then a 3-byte sub-block: 0x01, count lo, count hi.
                if (p + 16 <= d.size() && w.loopCount < 0) {
                    w.loopCount = d[p + 14] | (d[p + 15] << 8);
                }
            }
            if (label == 0xF9 && p + 5 <= d.size() && w.firstDelay < 0) {
                w.firstDelay = d[p + 2] | (d[p + 3] << 8);
            }
            if (!skipSubBlocks(d, p)) return w;
            continue;
        }
        if (b == 0x2C) { // image descriptor
            if (p + 9 > d.size()) return w;
            const std::uint8_t packed = d[p + 8];
            p += 9;
            if ((packed & 0x80) != 0) {
                p += 3u * (1u << ((packed & 0x07) + 1));
            }
            if (p >= d.size()) return w;
            ++p; // LZW minimum code size
            if (!skipSubBlocks(d, p)) return w;
            ++w.frames;
            continue;
        }
        return w; // a byte that is not a block is a malformed file
    }
    return w;
}

// A frame with a recognisable picture in it: a colour ramp, plus a block that moves with `step`.
Image madeFrame(int w, int h, int step) {
    Image img(w, h, Color{0.0f, 0.0f, 0.0f, 1.0f});
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const float u = static_cast<float>(x) / static_cast<float>(w - 1);
            const float v = static_cast<float>(y) / static_cast<float>(h - 1);
            img.setPixel(x, y, Color{u, v, 0.25f, 1.0f});
        }
    }
    for (int y = 4; y < 12 && y < h; ++y) {
        for (int x = step * 3; x < step * 3 + 8 && x < w; ++x) {
            img.setPixel(x, y, Color{1.0f, 1.0f, 1.0f, 1.0f});
        }
    }
    return img;
}

} // namespace

int main() {
    // --- 1. Bad input gives nothing, rather than a file no player can open. ---
    {
        CHECK(maz::render::encodeGifAnimation({}).empty(), "no frames encodes to nothing");
        CHECK(maz::render::encodeGifAnimation({Image()}).empty(), "an empty image encodes to nothing");
        const std::vector<Image> mixed{Image(8, 8, Color{0, 0, 0, 1}), Image(9, 8, Color{0, 0, 0, 1})};
        CHECK(maz::render::encodeGifAnimation(mixed).empty(),
              "frames of different sizes are refused, not silently cropped");
    }

    // --- 2. The file is a well-formed GIF89a with one image per frame. ---
    {
        std::vector<Image> frames;
        for (int i = 0; i < 6; ++i) {
            frames.push_back(madeFrame(40, 24, i));
        }
        maz::render::GifAnimOptions opts;
        opts.delayCentiseconds = 5;
        const auto bytes = maz::render::encodeGifAnimation(frames, opts);
        const Walk w = walk(bytes);
        CHECK(w.ok, "the whole file parses and ends at the trailer");
        CHECK(w.frames == 6, "six frames in, six image descriptors out");
        CHECK(w.width == 40 && w.height == 24, "the logical screen is the frame size");
        CHECK(w.firstDelay == 5, "the delay asked for is the delay written");
        CHECK(w.loopBlocks == 1, "exactly one NETSCAPE2.0 block — more and players disagree");
        CHECK(w.loopCount == 0, "loop count 0 means forever");
    }

    // --- 3. The first frame survives the round trip. ---
    //
    // 256 colours for a whole animation is lossy by construction, so this asks for CLOSE, not equal:
    // the point is that the pixels are the picture and not noise or a shifted row.
    {
        const std::vector<Image> frames{madeFrame(40, 24, 0), madeFrame(40, 24, 3)};
        const auto bytes = maz::render::encodeGifAnimation(frames);
        const Image back = maz::render::decodeGif(bytes);
        CHECK(back.width() == 40 && back.height() == 24, "the decoded frame is the right size");
        double worst = 0.0;
        double total = 0.0;
        if (!back.empty()) {
            for (int y = 0; y < 24; ++y) {
                for (int x = 0; x < 40; ++x) {
                    const Color a = frames[0].getPixel(x, y);
                    const Color b = back.getPixel(x, y);
                    const double e = std::fmax(std::fmax(std::fabs(a.r - b.r), std::fabs(a.g - b.g)),
                                               std::fabs(a.b - b.b)) * 255.0;
                    worst = std::fmax(worst, e);
                    total += e;
                }
            }
        }
        const double mean = total / (40.0 * 24.0);
        CHECK(mean < 12.0, ("the first frame comes back as the same picture (mean error " +
                            std::to_string(mean) + "/255)").c_str());
        CHECK(worst < 48.0, ("no pixel is wildly wrong (worst " + std::to_string(worst) +
                             "/255)").c_str());
    }

    // --- 4. Same frames, same bytes. A reel rebuilt must be the same file. ---
    {
        const std::vector<Image> frames{madeFrame(24, 16, 0), madeFrame(24, 16, 2)};
        CHECK(maz::render::encodeGifAnimation(frames) == maz::render::encodeGifAnimation(frames),
              "encoding is deterministic");
    }

    // --- 5. Options that would make an unplayable file are clamped, not obeyed. ---
    {
        const std::vector<Image> frames{madeFrame(16, 16, 0)};
        maz::render::GifAnimOptions opts;
        opts.delayCentiseconds = 0; // a zero delay means "as fast as possible" to some players
        opts.loopCount = -1;
        opts.maxColors = 1000;
        const Walk w = walk(maz::render::encodeGifAnimation(frames, opts));
        CHECK(w.ok, "clamped options still give a well-formed file");
        CHECK(w.firstDelay == 1, "a zero delay is raised to one centisecond");
        CHECK(w.loopCount == 0, "a negative loop count becomes forever");
    }

    // --- 6. Dithering is a choice, and both choices produce a real file. ---
    {
        std::vector<Image> frames;
        for (int i = 0; i < 3; ++i) {
            frames.push_back(madeFrame(32, 20, i));
        }
        maz::render::GifAnimOptions plain;
        plain.dither = false;
        const auto dithered = maz::render::encodeGifAnimation(frames);
        const auto flat = maz::render::encodeGifAnimation(frames, plain);
        CHECK(walk(dithered).frames == 3 && walk(flat).frames == 3, "both encode every frame");
        CHECK(dithered != flat, "the dither flag actually changes the pixels");
        // An ordered dither breaks up runs, so it costs bytes. Worth knowing if that ever inverts.
        CHECK(flat.size() < dithered.size(), "the undithered file is the smaller one");
    }

    if (g_fail == 0) {
        std::printf("gif animation: all checks passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
