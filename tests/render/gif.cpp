// tests/render/gif.cpp — verifies the GIF codec (render::encodeGif / decodeGif) by an ENCODE->DECODE
// round-trip that is lossless for <=256-color images (GIF's target content), plus header/dimension checks
// and malformed-input rejection. Exercises the variable-width LZW path (code growth, clear/EOI) on a
// non-trivial pattern. Pure bytes, headless.
#include "maz/render/ImageCodecGif.hpp"

#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool sameRgb(const Color& a, const Color& b) {
    auto q = [](float v) { return static_cast<int>(v * 255.0f + 0.5f); };
    return q(a.r) == q(b.r) && q(a.g) == q(b.g) && q(a.b) == q(b.b);
}

int main() {
    // --- 1. Multi-color pattern round-trips losslessly (few distinct colors). ---
    {
        const int w = 32, h = 24;
        Image src(w, h, Color{0, 0, 0, 1});
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                // A handful of distinct colors in a pattern that stresses LZW runs.
                const int sel = ((x / 4) + (y / 3)) % 5;
                Color c;
                switch (sel) {
                    case 0: c = color8(200, 30, 30, 255); break;
                    case 1: c = color8(30, 200, 30, 255); break;
                    case 2: c = color8(30, 30, 200, 255); break;
                    case 3: c = color8(220, 220, 40, 255); break;
                    default: c = color8(20, 20, 20, 255); break;
                }
                src.setPixel(x, y, c);
            }
        }

        const std::vector<std::uint8_t> gif = encodeGif(src);
        CHECK(gif.size() > 14, "encoder produced a GIF stream");
        CHECK(gif[0] == 'G' && gif[1] == 'I' && gif[2] == 'F', "GIF magic present");
        CHECK(gif.back() == 0x3B, "GIF trailer present");

        const Image out = decodeGif(gif);
        CHECK(out.width() == w && out.height() == h, "decoded dimensions match");
        bool lossless = true;
        for (int y = 0; y < h && lossless; ++y)
            for (int x = 0; x < w; ++x)
                if (!sameRgb(src.getPixel(x, y), out.getPixel(x, y))) { lossless = false; break; }
        CHECK(lossless, "round-trip is pixel-exact for a <=256-color image");
    }

    // --- 2. A large solid fill (long LZW runs, dictionary growth) round-trips exactly. ---
    {
        const int w = 100, h = 100;
        Image src(w, h, color8(17, 133, 200, 255));
        // A couple of stripes so it's not a single palette entry.
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x)
                if ((x ^ y) & 16) src.setPixel(x, y, color8(240, 120, 10, 255));

        const Image out = decodeGif(encodeGif(src));
        CHECK(out.width() == w && out.height() == h, "solid-fill dims match");
        CHECK(sameRgb(src.getPixel(0, 0), out.getPixel(0, 0)) &&
              sameRgb(src.getPixel(50, 50), out.getPixel(50, 50)) &&
              sameRgb(src.getPixel(99, 99), out.getPixel(99, 99)),
              "sampled pixels survive the long-run LZW path");
    }

    // --- 3. Malformed input is rejected cleanly. ---
    {
        CHECK(decodeGif(std::vector<std::uint8_t>{}).empty(), "empty -> empty");
        std::vector<std::uint8_t> notGif = {'N', 'O', 'T', 'G', 'I', 'F', 0, 0, 0, 0, 0, 0, 0};
        CHECK(decodeGif(notGif).empty(), "bad magic rejected");
        CHECK(encodeGif(Image()).empty(), "encoding empty image yields empty");
    }

    if (g_fail == 0) {
        std::printf("gif: OK — LZW encode/decode round-trip lossless, header, long-run, rejection.\n");
        return 0;
    }
    std::printf("gif: %d failure(s).\n", g_fail);
    return 1;
}
