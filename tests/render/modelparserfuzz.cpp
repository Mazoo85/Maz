// Malformed-input robustness fuzz for the 3D-asset container/model parsers that consume untrusted files:
// the ASCII FBX geometry importer (render::parseFbxAscii), the GLB binary container splitter
// (render::parseGlb), and the MP3 frame scanner (audio::scanMp3). A game imports models and audio a player
// (or a mod, or an asset store) supplies, so a corrupt or hostile file must fail cleanly (return false /
// empty) — never crash, read out of bounds, OOM, or invoke UB. The existing model tests feed only
// well-formed assets; this feeds truncations/mutations of valid seeds, absurd length/count fields, and
// random buffers, and drives the numeric-payload edge cases. Built under ASan+UBSan (the sanitizer CI job
// runs it, with float-cast-overflow enabled). This harness found and now guards a real UB: parseFbxAscii
// did static_cast<int> on a polygon index parsed from text, so a NaN or an out-of-int-range magnitude
// (e.g. 1e300) in PolygonVertexIndex was undefined behavior; it now rejects such values.
#include "maz/render/FbxLoader.hpp"
#include "maz/render/GlbContainer.hpp"
#include "maz/audio/Mp3.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feedFbx(const std::string& s) {
    render::shapes::MeshData m;
    if (render::parseFbxAscii(s, m)) g_sink += m.vertices.size() + m.indices.size();
}
static void feedGlb(const std::vector<std::uint8_t>& b) {
    render::GlbChunks c;
    if (render::parseGlb(b, c)) g_sink += c.json.size() + c.bin.size() + c.version;
}
static void feedMp3(const std::vector<std::uint8_t>& b) {
    const audio::Mp3Info info = audio::scanMp3(b);
    g_sink += info.frames.size() + static_cast<std::size_t>(info.totalSamples);
}

static void truncateFbx(const std::string& seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) feedFbx(seed.substr(0, k));
}
static void truncateBytes(void (*fn)(const std::vector<std::uint8_t>&), const std::vector<std::uint8_t>& seed) {
    // seed.begin() + k: k is size_t and an iterator's difference_type is signed, so this is a
    // signedness change. GCC's -Wconversion lets it pass; clang's implies -Wsign-conversion and
    // does not, which is why it only ever showed up on macOS. k <= seed.size(), so the cast is safe.
    for (std::size_t k = 0; k <= seed.size(); ++k) fn(std::vector<std::uint8_t>(seed.begin(), seed.begin() + static_cast<std::ptrdiff_t>(k)));
    std::vector<std::uint8_t> s = seed;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const std::uint8_t orig = s[i];
        for (std::uint8_t p : {std::uint8_t(0), std::uint8_t(0xff), std::uint8_t(0x7f)}) { s[i] = p; fn(s); }
        s[i] = orig;
    }
}

int main() {
    // --- FBX: hostile numeric payloads (the UB shapes) + truncations of a valid mesh ---
    feedFbx("Vertices: *9 { a: 0,0,0, 1,0,0, 0,1,0 }\nPolygonVertexIndex: *3 { a: 0,1,1e300 }");
    feedFbx("Vertices: *9 { a: 0,0,0, 1,0,0, 0,1,0 }\nPolygonVertexIndex: *3 { a: 0,1,-1e308 }");
    feedFbx("Vertices: *9 { a: 0,0,0, 1,0,0, 0,1,0 }\nPolygonVertexIndex: *3 { a: 0,1,-2147483648 }");
    feedFbx("Vertices: *9 { a: 0,0,0, 1,0,0, 0,1,0 }\nPolygonVertexIndex: *3 { a: 0,1,nan }");
    feedFbx("Vertices: *9 { a: 1e300,-1e300,0, 1,0,0, 0,1,0 }\nPolygonVertexIndex: *3 { a: 0,1,-3 }");
    truncateFbx("Vertices: *12 { a: 0,0,0, 1,0,0, 0,1,0, 1,1,0 }\n"
                "PolygonVertexIndex: *4 { a: 0,1,2,-4 }\n");

    // --- GLB: absurd chunk lengths + truncations/mutations of a valid container ---
    // Hand-build a minimal valid GLB (magic, version 2, total, JSON chunk).
    {
        auto glb = render::buildGlb("{\"asset\":{\"version\":\"2.0\"}}");
        truncateBytes(feedGlb, glb);
    }
    feedGlb({0x67, 0x6c, 0x54, 0x46, 0x02, 0, 0, 0, 0xff, 0xff, 0xff, 0xff, // header, total = ~4.29e9
             0xff, 0xff, 0xff, 0xff, 0x4a, 0x53, 0x4f, 0x4e});               // JSON chunk, len = ~4.29e9

    // --- MP3: ID3 tag with huge declared size + truncations of a frame ---
    feedMp3({'I', 'D', '3', 4, 0, 0, 0x7f, 0x7f, 0x7f, 0x7f});               // ID3v2 claiming ~256 MB
    truncateBytes(feedMp3, {0xff, 0xfb, 0x90, 0x00, 0, 0, 0, 0, 0, 0, 0, 0}); // MPEG1 LayerIII frame header

    // --- Random buffers ---
    std::mt19937 rng(0xB0A7u);
    const std::string fbxAlpha = "Vertices PolygonVertexIndex: *{}a,0123456789-+.eEnN \t\n";
    for (int iter = 0; iter < 30000; ++iter) {
        std::size_t fn = rng() % 96u;
        std::string s(fn, ' ');
        for (auto& ch : s) ch = fbxAlpha[rng() % fbxAlpha.size()];
        feedFbx(s);
        std::size_t bn = rng() % 48u;
        std::vector<std::uint8_t> b(bn);
        for (auto& x : b) x = static_cast<std::uint8_t>(rng() & 0xff);
        feedGlb(b);
        feedMp3(b);
    }

    std::printf("modelparser fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
