// Malformed-input robustness fuzz for the binary resource-pack archive (io::ResourcePack) — the .pck a
// shipping game (including the mobile bundle) loads every asset from. Its directory carries a header entry
// count and per-entry (path, offset, size) fields straight out of untrusted bytes, the classic OOB/OOM
// surface. The existing test only round-trips a valid archive; this feeds truncations and mutations of a
// valid pack plus random buffers, and a header declaring an absurd entry count. ResourcePack::load may
// reject bad input (return false) — it just must never read out of bounds, over-allocate, or invoke UB.
// Built under ASan+UBSan (the sanitizer CI job runs it). Found and now guards a real bug: an unbounded
// dir.reserve(count) that OOM-crashed on a hostile header.
#include "maz/io/ResourcePack.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feed(const std::uint8_t* d, std::size_t n) {
    io::ResourcePack p;
    if (p.load(d, n)) g_sink += p.count();
}

static void truncateAndMutate(std::vector<std::uint8_t> seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) feed(seed.data(), k);
    const std::uint8_t patches[3] = {0x00, 0xFF, 0x01};
    for (std::size_t i = 0; i < seed.size(); ++i) {
        const std::uint8_t orig = seed[i];
        for (std::uint8_t patch : patches) {
            seed[i] = (patch == 0x01) ? static_cast<std::uint8_t>(orig + 1) : patch;
            feed(seed.data(), seed.size());
        }
        seed[i] = orig;
    }
}

int main() {
    // Valid seed via the real packer, so truncations exercise the header + directory + data code paths.
    std::vector<io::PackEntry> entries = {
        {"res://a.txt", {1, 2, 3, 4, 5}},
        {"res://dir/b.bin", {9, 8, 7}},
        {"c", {}},
    };
    truncateAndMutate(io::packResources(entries));

    // The specific stressor: a well-formed header whose entry count is absurd (would reserve ~170 GB).
    {
        std::vector<std::uint8_t> pack = io::packResources(entries);
        if (pack.size() > 12) {
            pack[8] = pack[9] = pack[10] = pack[11] = 0xFF; // count field right after the 8-byte header
            feed(pack.data(), pack.size());
        }
    }

    // Random buffers, half of them carrying the pack magic so the parser gets past the header check.
    std::mt19937 rng(0xACE1u);
    std::vector<std::uint8_t> buf;
    for (int iter = 0; iter < 40000; ++iter) {
        const std::size_t n = rng() % 513u;
        buf.resize(n);
        for (std::size_t i = 0; i < n; ++i) buf[i] = static_cast<std::uint8_t>(rng());
        if (n >= 4 && (iter & 1) == 0) { // little-endian magic 0x31505A4D
            buf[0] = 0x4D; buf[1] = 0x5A; buf[2] = 0x50; buf[3] = 0x31;
        }
        feed(buf.data(), n);
    }

    std::printf("resourcepack fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
