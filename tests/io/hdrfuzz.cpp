// Malformed-input robustness fuzz for the HDR image decoder (io::decodeHdr) and the .tres-style prefab text
// loader (io::loadPrefabText) — both consume untrusted asset files (an .hdr skybox / IBL environment, a
// prefab resource) that a game, a mod, or an asset store supplies. A corrupt or hostile file must fail
// cleanly (return an invalid image / false) — never crash, read out of bounds, OOM, or invoke UB. The
// existing tests feed only well-formed assets; this feeds absurd resolution headers, truncations/mutations of
// valid seeds, and random buffers. Built under ASan+UBSan (the sanitizer CI job runs it, with
// float-cast-overflow enabled). This harness found and now guards a real OOM DoS: decodeHdr allocated
// width*height*3 floats straight from the resolution line, so a tiny file declaring "-Y 100000 +X 100000"
// forced a ~120 GB allocation.
#include "maz/io/Hdr.hpp"
#include "maz/io/PrefabText.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void put(std::vector<std::uint8_t>& v, const std::string& s) {
    for (char c : s) v.push_back(static_cast<std::uint8_t>(c));
}
static void feedHdr(const std::vector<std::uint8_t>& b) {
    const io::HdrImage img = io::decodeHdr(b);
    if (img.valid()) g_sink += img.rgb.size();
}
static void feedPrefab(const std::string& s) {
    scene::Prefab p;
    if (io::loadPrefabText(s, p)) g_sink += 1;
}

static void truncateHdr(const std::vector<std::uint8_t>& seed) {
    // seed.begin() + k: k is size_t and an iterator's difference_type is signed, so this is a
    // signedness change. GCC's -Wconversion lets it pass; clang's implies -Wsign-conversion and
    // does not, which is why it only ever showed up on macOS. k <= seed.size(), so the cast is safe.
    for (std::size_t k = 0; k <= seed.size(); ++k) feedHdr(std::vector<std::uint8_t>(seed.begin(), seed.begin() + static_cast<std::ptrdiff_t>(k)));
    std::vector<std::uint8_t> s = seed;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const std::uint8_t orig = s[i];
        for (std::uint8_t p : {std::uint8_t(0), std::uint8_t(0xff), std::uint8_t(0x02), std::uint8_t('\n')}) {
            s[i] = p;
            feedHdr(s);
        }
        s[i] = orig;
    }
}
static void truncatePrefab(const std::string& seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) feedPrefab(seed.substr(0, k));
}

int main() {
    // --- HDR structural stressors: enormous declared dimensions on a tiny file (the OOM shapes) ---
    { std::vector<std::uint8_t> b; put(b, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 100000 +X 100000\n"); feedHdr(b); }
    { std::vector<std::uint8_t> b; put(b, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2000000000 +X 2000000000\n"); feedHdr(b); }
    { std::vector<std::uint8_t> b; put(b, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 30000 +X 30000\n"); feedHdr(b); }
    { std::vector<std::uint8_t> b; put(b, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n+Y 1 +X 40000\n"); feedHdr(b); } // raw path, big w

    // --- HDR: a valid 8x2 new-RLE image must still decode; also truncate/mutate it ---
    std::vector<std::uint8_t> valid;
    put(valid, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 8\n");
    for (int row = 0; row < 2; ++row) {
        valid.push_back(2); valid.push_back(2); valid.push_back(0); valid.push_back(8);
        const std::uint8_t chan[4] = {128, 64, 32, 128};
        for (int c = 0; c < 4; ++c) { valid.push_back(8); for (int i = 0; i < 8; ++i) valid.push_back(chan[c]); }
    }
    if (!io::decodeHdr(valid).valid()) { std::printf("FAIL: valid HDR no longer decodes\n"); return 1; }
    truncateHdr(valid);

    // --- Prefab text: truncations of a small valid-ish resource ---
    truncatePrefab("[prefab]\nname = \"root\"\n[node]\nname = \"child\"\nx = 1.0\ny = 2.0\n");

    // --- Random buffers (some prefixed with a plausible HDR header to reach the scanline decoder) ---
    std::mt19937 rng(0x4D0Cu);
    const std::string pAlpha = "[]=\"\n abcxyz0123456789._-prefabnodxy";
    for (int iter = 0; iter < 40000; ++iter) {
        std::vector<std::uint8_t> b;
        if (rng() & 1) put(b, "#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 8 +X 8\n");
        const std::size_t n = rng() % 80u;
        for (std::size_t i = 0; i < n; ++i) b.push_back(static_cast<std::uint8_t>(rng() & 0xff));
        feedHdr(b);

        std::string s(rng() % 96u, ' ');
        for (auto& ch : s) ch = pAlpha[rng() % pAlpha.size()];
        feedPrefab(s);
    }

    std::printf("hdr/prefab fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
