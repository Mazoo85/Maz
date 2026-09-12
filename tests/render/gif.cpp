// tests/render/gif.cpp — verifies the GIF codec (render::encodeGif / decodeGif).
//
// An encode->decode round-trip is most of this file, and a round-trip against yourself is NOT ENOUGH:
// the variable-width LZW code size has to widen at exactly the moment a conformant decoder expects,
// and an encoder and decoder that are both off by one agree with each other perfectly while producing
// files no other program on earth can read. That is a bug this repository shipped. So check 5 works
// from tests/render/gif-reference.gif -- a file THIS CODE DID NOT WRITE, produced by a reference
// encoder and verified pixel for pixel by an unrelated decoder -- and demands that we read it back
// exactly and that our own encoder reproduces it byte for byte.
#include "maz/render/ImageCodecGif.hpp"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

static bool sameRgb(const Color& a, const Color& b) {
    auto q = [](float v) { return static_cast<int>(v * 255.0f + 0.5f); };
    return q(a.r) == q(b.r) && q(a.g) == q(b.g) && q(a.b) == q(b.b);
}

// The reference fixture's pixels, by the rule that generated them: a 32x32 field of 16 colours from a
// plain LCG. Noise, on purpose -- it fills the LZW dictionary quickly and so crosses the code-width
// boundary several times in 1024 pixels, which is the thing being tested.
static void referencePixels(std::vector<int>& raw, std::vector<Color>& palette) {
    std::uint32_t s = 2463534242u;
    raw.resize(32u * 32u);
    for (std::size_t i = 0; i < raw.size(); ++i) {
        s = s * 1103515245u + 12345u;
        raw[i] = static_cast<int>((s >> 16) & 15u);
    }
    // The palette in first-seen order, which is the order encodeGif assigns.
    std::vector<int> order;
    for (int v : raw) {
        bool seen = false;
        for (int o : order) seen = seen || o == v;
        if (!seen) order.push_back(v);
    }
    palette.clear();
    for (int v : order) {
        palette.push_back(color8(static_cast<std::uint8_t>((v * 16) & 0xFF),
                                 static_cast<std::uint8_t>((255 - v * 16) & 0xFF),
                                 static_cast<std::uint8_t>((v * 37) & 0xFF), 255));
    }
    // Rewrite raw as palette indices.
    for (int& v : raw) {
        for (std::size_t i = 0; i < order.size(); ++i) {
            if (order[i] == v) { v = static_cast<int>(i); break; }
        }
    }
}

int main(int argc, char** argv) {
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

    // --- 4. A large, varied image fills the dictionary and forces a mid-stream reset. ---
    //
    // 4096 entries is the ceiling; past it the encoder must emit a clear code and start the dictionary
    // again. 200x200 of noise over 200 colours gets there several times.
    {
        const int w = 200, h = 200;
        Image src(w, h, Color{0, 0, 0, 1});
        std::uint32_t s = 777u;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                s = s * 1103515245u + 12345u;
                const std::uint8_t v = static_cast<std::uint8_t>((s >> 16) % 200u);
                src.setPixel(x, y, color8(v, static_cast<std::uint8_t>(255 - v),
                                          static_cast<std::uint8_t>((v * 7) & 0xFF), 255));
            }
        }
        const std::vector<std::uint8_t> bytes = encodeGif(src);
        const Image back = decodeGif(bytes);
        bool exact = back.width() == w && back.height() == h;
        for (int y = 0; y < h && exact; ++y) {
            for (int x = 0; x < w && exact; ++x) {
                exact = sameRgb(src.getPixel(x, y), back.getPixel(x, y));
            }
        }
        CHECK(exact, "40000 pixels over 200 colours survive the dictionary reset losslessly");
    }

    // --- 5. Agreement with a GIF this code did not write. ---
    {
        const std::string path = argc > 1 ? argv[1] : "tests/render/gif-reference.gif";
        std::ifstream f(path, std::ios::binary);
        std::vector<std::uint8_t> ref((std::istreambuf_iterator<char>(f)),
                                      std::istreambuf_iterator<char>());
        CHECK(!ref.empty(), ("the reference fixture loads (" + path + ")").c_str());
        if (!ref.empty()) {
            std::vector<int> idx;
            std::vector<Color> palette;
            referencePixels(idx, palette);

            // (a) We can READ a conformant file.
            const Image back = decodeGif(ref);
            bool readOk = back.width() == 32 && back.height() == 32;
            int firstBad = -1;
            for (int y = 0; y < 32 && readOk; ++y) {
                for (int x = 0; x < 32; ++x) {
                    const Color want = palette[static_cast<std::size_t>(idx[static_cast<std::size_t>(y) * 32u + static_cast<std::size_t>(x)])];
                    if (!sameRgb(want, back.getPixel(x, y))) {
                        readOk = false;
                        firstBad = y * 32 + x;
                        break;
                    }
                }
            }
            CHECK(readOk, ("every pixel of the reference GIF decodes correctly (first wrong pixel: " +
                           std::to_string(firstBad) + ")").c_str());

            // (b) We WRITE the same bytes a reference encoder wrote for the same picture. This is the
            // check that pins the code-width rule to the format instead of to ourselves.
            Image src(32, 32, Color{0, 0, 0, 1});
            for (int y = 0; y < 32; ++y) {
                for (int x = 0; x < 32; ++x) {
                    src.setPixel(x, y, palette[static_cast<std::size_t>(idx[static_cast<std::size_t>(y) * 32u + static_cast<std::size_t>(x)])]);
                }
            }
            const std::vector<std::uint8_t> mine = encodeGif(src);
            std::size_t at = 0;
            while (at < mine.size() && at < ref.size() && mine[at] == ref[at]) ++at;
            CHECK(mine == ref, ("our bytes are the reference bytes (" + std::to_string(mine.size()) +
                                " vs " + std::to_string(ref.size()) + ", first difference at " +
                                std::to_string(at) + ")").c_str());
        }
    }

    // --- 6. An image with more colours than GIF has gets quantised, not flattened. ---
    //
    // This is the path a photograph or a rendered frame takes. The failure it guards against is not
    // subtle banding: it is the whole picture collapsing to one palette entry, which is what sending
    // every colour past the 256th to index 0 does.
    {
        const int w = 96, h = 64;
        Image src(w, h, Color{0, 0, 0, 1});
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                src.setPixel(x, y,
                             Color{static_cast<float>(x) / static_cast<float>(w - 1),
                                   static_cast<float>(y) / static_cast<float>(h - 1),
                                   static_cast<float>((x + y) % 37) / 36.0f, 1.0f});
            }
        }
        const Image back = decodeGif(encodeGif(src));
        double total = 0.0;
        double worst = 0.0;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const Color a = src.getPixel(x, y);
                const Color b = back.getPixel(x, y);
                const double e = (std::fabs(static_cast<double>(a.r) - b.r) +
                                  std::fabs(static_cast<double>(a.g) - b.g) +
                                  std::fabs(static_cast<double>(a.b) - b.b)) / 3.0 * 255.0;
                total += e;
                worst = e > worst ? e : worst;
            }
        }
        const double mean = total / (static_cast<double>(w) * static_cast<double>(h));
        CHECK(mean < 10.0, ("a many-coloured image comes back as the same picture (mean error " +
                            std::to_string(mean) + "/255)").c_str());
        CHECK(worst < 60.0, ("no pixel is thrown away (worst " + std::to_string(worst) +
                             "/255)").c_str());
    }

    if (g_fail == 0) {
        std::printf("gif: OK — round-trip lossless, dictionary reset, quantised path, header, rejection, "
                    "and byte-for-byte agreement with a reference GIF.\n");
        return 0;
    }
    std::printf("gif: %d failure(s).\n", g_fail);
    return 1;
}
