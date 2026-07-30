// Robustness fuzz harness for the hand-written image decoders — feeds truncated, mutated, and random
// byte buffers to each decode* entry point under ASan+UBSan. Existing tests only feed valid input; this
// checks the untrusted-input path a game hits when loading a corrupt or malicious asset. Any out-of-bounds
// read, signed/unsigned overflow, or other UB trips the sanitizer and fails the run. A decoder is allowed
// to reject bad input (return an empty Image) — it just must never read out of bounds or invoke UB.
#include "maz/render/ImageCodecBmp.hpp"
#include "maz/render/ImageCodecTga.hpp"
#include "maz/render/ImageCodecGif.hpp"
#include "maz/render/ImageCodecPnm.hpp"
#include "maz/render/ImageCodecDds.hpp"
#include "maz/render/ImageCodecPng.hpp"
#include "maz/render/ImageCodecQoi.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using namespace maz::render;

using DecodeFn = Image (*)(const std::uint8_t*, std::size_t);

static Image bmpFn(const std::uint8_t* d, std::size_t n) { return decodeBmp(d, n); }
static Image tgaFn(const std::uint8_t* d, std::size_t n) { return decodeTga(d, n); }
static Image gifFn(const std::uint8_t* d, std::size_t n) { return decodeGif(d, n); }
static Image pnmFn(const std::uint8_t* d, std::size_t n) { return decodePnm(d, n); }
static Image ddsFn(const std::uint8_t* d, std::size_t n) { return decodeDds(d, n); }
static Image pngFn(const std::uint8_t* d, std::size_t n) { return decodePng(d, n); }
static Image qoiFn(const std::uint8_t* d, std::size_t n) { return decodeQoi(d, n); }

static volatile std::size_t g_sink = 0; // keep the decode result observable so nothing is optimized away
static void feed(DecodeFn fn, const std::uint8_t* d, std::size_t n) {
    Image img = fn(d, n);
    g_sink += static_cast<std::size_t>(img.width()) + static_cast<std::size_t>(img.height());
}

// Every truncation of `seed`, then every single-byte mutation (to 0x00, 0xFF, and +1) of the full seed.
static void truncateAndMutate(DecodeFn fn, std::vector<std::uint8_t> seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) {
        feed(fn, seed.data(), k);
    }
    const std::uint8_t patches[3] = {0x00, 0xFF, 0x01};
    for (std::size_t i = 0; i < seed.size(); ++i) {
        const std::uint8_t orig = seed[i];
        for (std::uint8_t p : patches) {
            seed[i] = (p == 0x01) ? static_cast<std::uint8_t>(orig + 1) : p;
            feed(fn, seed.data(), seed.size());
        }
        seed[i] = orig;
    }
}

int main() {
    // Valid seeds via the matching encoders (BMP/TGA/GIF), so truncations exercise real header/body paths.
    Image src(5, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 5; ++x)
            src.setPixel(x, y, color8(static_cast<int>(x * 40), static_cast<int>(y * 60), 128, 255));

    truncateAndMutate(bmpFn, encodeBmp(src));
    truncateAndMutate(tgaFn, encodeTga(src));
    truncateAndMutate(gifFn, encodeGif(src));
    truncateAndMutate(qoiFn, encodeQoi(src));

    // Compressed-format dimension bomb: a tiny header declaring an enormous image. The uncompressed codecs
    // reject this via their "pixel data must fit the file" check, but the COMPRESSED decoders (QOI, PNG)
    // allocated Image(w,h) straight from the header — a ~40 GB OOM on a few-byte file. Both now cap it.
    {
        // QOI: 'qoif' + u32 w + u32 h + channels + colorspace, w=h=100000 (0x000186A0, big-endian).
        std::vector<std::uint8_t> qoi = {'q', 'o', 'i', 'f', 0, 1, 0x86, 0xA0, 0, 1, 0x86, 0xA0, 4, 0};
        feed(qoiFn, qoi.data(), qoi.size());
        // PNG: signature + IHDR(w=h=0x40000000, 8-bit RGBA) + IDAT(empty zlib) + IEND. decodePng does not
        // verify the CRC, so the huge dimensions reach the (now guarded) Image allocation.
        auto be32 = [](std::vector<std::uint8_t>& v, std::uint32_t x) {
            v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xff));
            v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xff));
            v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xff));
            v.push_back(static_cast<std::uint8_t>(x & 0xff));
        };
        std::vector<std::uint8_t> png = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
        be32(png, 13);
        png.insert(png.end(), {'I', 'H', 'D', 'R'});
        be32(png, 0x40000000u);
        be32(png, 0x40000000u);
        png.insert(png.end(), {8, 6, 0, 0, 0});
        be32(png, 0);
        const std::vector<std::uint8_t> z = {0x78, 0x9c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01};
        be32(png, static_cast<std::uint32_t>(z.size()));
        png.insert(png.end(), {'I', 'D', 'A', 'T'});
        png.insert(png.end(), z.begin(), z.end());
        be32(png, 0);
        be32(png, 0);
        png.insert(png.end(), {'I', 'E', 'N', 'D'});
        be32(png, 0);
        feed(pngFn, png.data(), png.size());
    }

    // Random-buffer fuzz for every decoder, including the ones without a matching encoder (PNM/DDS/PNG).
    // Deterministic PRNG so a finding reproduces. Sizes 0..1024 cover short-header and body paths.
    DecodeFn all[] = {bmpFn, tgaFn, gifFn, pnmFn, ddsFn, pngFn, qoiFn};
    std::mt19937 rng(0xC0FFEEu);
    std::vector<std::uint8_t> buf;
    for (int iter = 0; iter < 40000; ++iter) {
        const std::size_t n = rng() % 1025u;
        buf.resize(n);
        for (std::size_t i = 0; i < n; ++i) buf[i] = static_cast<std::uint8_t>(rng());
        // Bias some buffers toward each format's magic so the parser gets past the signature check.
        if (n >= 2 && (iter & 3) == 0) { buf[0] = 'B'; buf[1] = 'M'; }        // BMP
        if (n >= 3 && (iter & 3) == 1) { buf[0] = 'P'; buf[1] = static_cast<std::uint8_t>('1' + (iter % 6)); buf[2] = '\n'; } // PNM
        if (n >= 4 && (iter & 3) == 2) { buf[0] = 'D'; buf[1] = 'D'; buf[2] = 'S'; buf[3] = ' '; } // DDS
        if (n >= 6 && (iter & 3) == 3) { buf[0] = 'G'; buf[1] = 'I'; buf[2] = 'F'; buf[3] = '8'; buf[4] = '9'; buf[5] = 'a'; } // GIF
        if (n >= 14 && (iter % 5) == 4) { buf[0] = 'q'; buf[1] = 'o'; buf[2] = 'i'; buf[3] = 'f'; buf[12] = static_cast<std::uint8_t>(3 + (iter & 1)); } // QOI (3/4 channels)
        for (DecodeFn fn : all) feed(fn, buf.data(), n);
    }

    std::printf("imagecodec fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
