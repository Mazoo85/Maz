// Malformed-input robustness fuzz for the binary patch applier (io::binaryPatch), which reconstructs a
// target from a base buffer plus a delta patch. A game applies these to update assets or save data
// incrementally, and the patch can arrive from a download / mod / cloud sync — so a corrupt or hostile patch
// must fail cleanly (return false), never read out of bounds or OOM. The existing test feeds only well-formed
// diffs; this feeds patches whose COPY/ADD length varints are near UINT64_MAX (to probe the bounds-check
// arithmetic), truncations and byte mutations of valid patches, and random buffers, plus a diff/patch
// round-trip sanity. Built under ASan+UBSan (the sanitizer CI job runs it). This harness guards a real OOB:
// `srcOff + len` / `off + len` overflowed, slipping a huge length past the bounds check into an out-of-range
// insert().
#include "maz/io/BinaryDiff.hpp"
#include "maz/io/Varint.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feed(const std::vector<std::uint8_t>& src, const std::vector<std::uint8_t>& patch) {
    std::vector<std::uint8_t> out;
    if (io::binaryPatch(src, patch, out)) g_sink += out.size();
}

static void truncateAndMutate(const std::vector<std::uint8_t>& src, const std::vector<std::uint8_t>& seed) {
    // seed.begin() + k: k is size_t and an iterator's difference_type is signed, so this is a
    // signedness change. GCC's -Wconversion lets it pass; clang's implies -Wsign-conversion and
    // does not, which is why it only ever showed up on macOS. k <= seed.size(), so the cast is safe.
    for (std::size_t k = 0; k <= seed.size(); ++k) feed(src, std::vector<std::uint8_t>(seed.begin(), seed.begin() + static_cast<std::ptrdiff_t>(k)));
    std::vector<std::uint8_t> s = seed;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const std::uint8_t orig = s[i];
        for (std::uint8_t p : {std::uint8_t(0), std::uint8_t(1), std::uint8_t(0x80), std::uint8_t(0xff)}) {
            s[i] = p;
            feed(src, s);
        }
        s[i] = orig;
    }
}

int main() {
    const std::vector<std::uint8_t> src(100, 'A');

    // --- structural stressors: COPY/ADD lengths that overflow the bounds arithmetic (the OOB shapes) ---
    { std::vector<std::uint8_t> p = {0x00}; io::appendVarint(p, 10); io::appendVarint(p, 0xFFFFFFFFFFFFFFFBull); feed(src, p); }
    { std::vector<std::uint8_t> p = {0x00}; io::appendVarint(p, 0xFFFFFFFFFFFFFFFFull); io::appendVarint(p, 1); feed(src, p); }
    { std::vector<std::uint8_t> p = {0x01}; io::appendVarint(p, 0xFFFFFFFFFFFFFFFDull); feed(src, p); }
    { std::vector<std::uint8_t> p = {0x00}; io::appendVarint(p, 50); io::appendVarint(p, 0xFFFFFFFF00000000ull); feed(src, p); }

    // --- round-trip sanity: a real diff must still apply and reconstruct exactly ---
    {
        const std::vector<std::uint8_t> base = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
        const std::vector<std::uint8_t> target = {'h', 'e', 'l', 'p', ' ', 'w', 'o', 'r', 'l', 'd', '!', '!'};
        const auto patch = io::binaryDiff(base, target);
        std::vector<std::uint8_t> out;
        if (!io::binaryPatch(base, patch, out) || out != target) { std::printf("FAIL: diff/patch round-trip broken\n"); return 1; }
    }

    // --- truncations + byte mutations of valid patches ---
    {
        const std::vector<std::uint8_t> base = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
        const std::vector<std::uint8_t> target = {'a', 'b', 'z', 'z', 'e', 'f', 'g', 'x'};
        truncateAndMutate(base, io::binaryDiff(base, target));
    }

    // --- random buffers as patches (deterministic PRNG) ---
    std::mt19937 rng(0xD1FFu);
    for (int iter = 0; iter < 60000; ++iter) {
        const std::size_t n = rng() % 48u;
        std::vector<std::uint8_t> patch(n);
        for (auto& x : patch) x = static_cast<std::uint8_t>(rng() & 0xff);
        feed(src, patch);
    }

    std::printf("binarydiff fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
