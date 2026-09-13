// tests/render/gifanim.cpp — verifies the GIF encoder after refactor: the single-frame path still
// round-trips losslessly (regression), and the new encodeGifAnimation produces a GIF89a whose first
// frame decodes back to the first image, carries a NETSCAPE loop block, and tolerates odd-sized frames.
#include "maz/render/ImageCodecGif.hpp"

#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

using namespace maz::render;

static int g_fail = 0;
#define CHECK(c, m)                                                                                  \
    do {                                                                                             \
        if (!(c)) {                                                                                  \
            std::printf("FAIL: %s\n", (m));                                                          \
            ++g_fail;                                                                                \
        }                                                                                            \
    } while (0)

// A small image painted with a handful of distinct colours (a diagonal split), so the palette stays
// well under 256 and the encode is lossless.
static Image patterned(int w, int h, std::uint8_t base) {
    Image img(w, h, Color{0, 0, 0, 1});
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const std::uint8_t r = static_cast<std::uint8_t>(base + x * 10);
            const std::uint8_t g = static_cast<std::uint8_t>(base + y * 10);
            const std::uint8_t b = (x + y) % 2 ? 200 : 40;
            img.setPixel(x, y, color8(r, g, b, 255));
        }
    }
    return img;
}

static bool samePixels(const Image& a, const Image& b) {
    if (a.width() != b.width() || a.height() != b.height()) {
        return false;
    }
    return a.data() == b.data();
}

static bool contains(const std::vector<std::uint8_t>& hay, const char* needle) {
    const std::string n = needle;
    const std::string h(hay.begin(), hay.end());
    return h.find(n) != std::string::npos;
}

int main() {
    // --- single-frame regression: encode -> decode is lossless for <=256 colours ---
    {
        const Image img = patterned(6, 5, 30);
        const std::vector<std::uint8_t> gif = encodeGif(img);
        CHECK(gif.size() > 13 && gif[0] == 'G' && gif[1] == 'I' && gif[2] == 'F', "single-frame GIF header");
        const Image back = decodeGif(gif);
        CHECK(samePixels(img, back), "single-frame GIF round-trips losslessly");
    }

    // --- animated GIF: structure + first-frame decode ---
    {
        const Image f0 = patterned(6, 5, 20);
        const Image f1 = patterned(6, 5, 120);
        const std::vector<std::uint8_t> anim = encodeGifAnimation({f0, f1}, 5, 0);
        CHECK(anim.size() > 13 && anim[0] == 'G' && anim[1] == 'I' && anim[2] == 'F', "animated GIF header");
        CHECK(contains(anim, "NETSCAPE2.0"), "animated GIF has the loop extension");
        // decodeGif reads the FIRST frame, skipping the NETSCAPE + graphic-control extensions.
        const Image back0 = decodeGif(anim);
        CHECK(samePixels(f0, back0), "animated GIF first frame decodes to frame 0");
        // Two frames should be larger than one.
        CHECK(anim.size() > encodeGif(f0).size(), "two-frame animation larger than one frame");
    }

    // --- robustness: empty input and mismatched-size frames ---
    {
        CHECK(encodeGifAnimation({}, 5).empty(), "no frames -> empty");
        const Image f0 = patterned(6, 5, 10);
        const Image odd = patterned(4, 4, 10); // different size -> skipped
        const std::vector<std::uint8_t> anim = encodeGifAnimation({f0, odd}, 5, 0);
        const Image back0 = decodeGif(anim);
        CHECK(samePixels(f0, back0), "mismatched-size frame skipped; frame 0 still valid");
    }

    if (g_fail == 0) {
        std::printf("gifanim: all checks passed\n");
    }
    return g_fail ? 1 : 0;
}
