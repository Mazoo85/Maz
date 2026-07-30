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

    // Random-buffer fuzz for every decoder, including the ones without a matching encoder (PNM/DDS/PNG).
    // Deterministic PRNG so a finding reproduces. Sizes 0..1024 cover short-header and body paths.
    DecodeFn all[] = {bmpFn, tgaFn, gifFn, pnmFn, ddsFn, pngFn};
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
        for (DecodeFn fn : all) feed(fn, buf.data(), n);
    }

    std::printf("imagecodec fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
