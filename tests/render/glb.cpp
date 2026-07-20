// tests/render/glb.cpp — verifies the in-memory GLB (binary glTF) container parser (render::parseGlb /
// buildGlb) against hand-built byte buffers: header magic/version/length validation, JSON + BIN chunk
// extraction, 4-byte chunk alignment, a build->parse round-trip, JSON-only files, unknown-chunk skipping,
// and rejection of malformed input. Pure bytes, no file device.
#include "maz/render/GlbContainer.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz::render;

int main() {
    const std::string json = R"({"asset":{"version":"2.0"}})"; // 27 bytes -> needs 1 byte of pad
    const std::vector<std::uint8_t> binAligned = {1, 2, 3, 4}; // 4 bytes -> no padding
    const std::vector<std::uint8_t> binOdd = {1, 2, 3, 4, 5};  // 5 bytes -> 3 bytes of zero pad per spec

    // --- 1. build -> parse round-trip: aligned BIN comes back exactly; JSON content intact. ---
    {
        const std::vector<std::uint8_t> glb = buildGlb(json, binAligned);
        CHECK(glb.size() % 4 == 0, "GLB total length is 4-byte aligned");
        GlbChunks chunks;
        const bool ok = parseGlb(glb, chunks);
        CHECK(ok, "valid GLB parses");
        CHECK(chunks.version == 2, "glTF 2.0 version read");
        // JSON may carry trailing space padding; the document content must be intact at the front.
        CHECK(chunks.json.rfind(json, 0) == 0, "JSON chunk content preserved");
        CHECK(chunks.bin == binAligned, "aligned BIN chunk bytes preserved exactly");
    }

    // --- 1b. An unaligned BIN is zero-padded to 4 bytes per the spec; the prefix is intact. ---
    {
        const std::vector<std::uint8_t> glb = buildGlb(json, binOdd);
        GlbChunks chunks;
        CHECK(parseGlb(glb, chunks), "GLB with unaligned BIN parses");
        CHECK(chunks.bin.size() == 8, "BIN chunk is padded to a 4-byte multiple (5 -> 8)");
        CHECK(chunks.bin[0] == 1 && chunks.bin[4] == 5, "BIN payload prefix intact");
        CHECK(chunks.bin[5] == 0 && chunks.bin[6] == 0 && chunks.bin[7] == 0, "BIN pad is zero-filled");
    }

    // --- 2. JSON-only GLB (no BIN chunk) is valid; bin comes back empty. ---
    {
        const std::vector<std::uint8_t> glb = buildGlb(json);
        GlbChunks chunks;
        CHECK(parseGlb(glb, chunks), "JSON-only GLB parses");
        CHECK(chunks.bin.empty(), "no BIN chunk -> empty bin");
        CHECK(chunks.json.rfind(json, 0) == 0, "JSON preserved in JSON-only file");
    }

    // --- 3. Header validation: bad magic, wrong version, and over-long declared length are rejected. ---
    {
        std::vector<std::uint8_t> glb = buildGlb(json, binAligned);
        GlbChunks chunks;

        std::vector<std::uint8_t> badMagic = glb;
        badMagic[0] ^= 0xFF;
        CHECK(!parseGlb(badMagic, chunks), "bad magic rejected");

        std::vector<std::uint8_t> badVer = glb;
        badVer[4] = 1; // version 1
        CHECK(!parseGlb(badVer, chunks), "wrong version rejected");

        std::vector<std::uint8_t> tooShort(glb.begin(), glb.begin() + 8);
        CHECK(!parseGlb(tooShort, chunks), "truncated header rejected");

        std::vector<std::uint8_t> overrun = glb;
        overrun[8] = 0xFF; overrun[9] = 0xFF; // declared length far bigger than the buffer
        CHECK(!parseGlb(overrun, chunks), "declared length overrunning the buffer rejected");
    }

    // --- 4. Unknown chunk types are skipped, JSON still found (forward-compat). ---
    {
        // Manually assemble: header + an unknown chunk + a JSON chunk.
        auto pushU32 = [](std::vector<std::uint8_t>& v, std::uint32_t x) {
            v.push_back(static_cast<std::uint8_t>(x & 0xff));
            v.push_back(static_cast<std::uint8_t>((x >> 8) & 0xff));
            v.push_back(static_cast<std::uint8_t>((x >> 16) & 0xff));
            v.push_back(static_cast<std::uint8_t>((x >> 24) & 0xff));
        };
        std::vector<std::uint8_t> body;
        // unknown chunk: 4 bytes, type 0x12345678
        pushU32(body, 4);
        pushU32(body, 0x12345678u);
        body.insert(body.end(), {0xAA, 0xBB, 0xCC, 0xDD});
        // JSON chunk
        const std::uint32_t jlen = static_cast<std::uint32_t>(json.size() + ((4 - json.size() % 4) % 4));
        pushU32(body, jlen);
        pushU32(body, kGlbChunkJson);
        body.insert(body.end(), json.begin(), json.end());
        for (std::size_t i = json.size(); i < jlen; ++i) body.push_back(0x20);

        std::vector<std::uint8_t> glb;
        pushU32(glb, kGlbMagic);
        pushU32(glb, 2);
        pushU32(glb, static_cast<std::uint32_t>(12 + body.size()));
        glb.insert(glb.end(), body.begin(), body.end());

        GlbChunks chunks;
        CHECK(parseGlb(glb, chunks), "GLB with an unknown chunk still parses");
        CHECK(chunks.json.rfind(json, 0) == 0, "JSON found after skipping the unknown chunk");
    }

    if (g_fail == 0) {
        std::printf("glb: OK — round-trip, JSON-only, header validation, unknown-chunk skip.\n");
        return 0;
    }
    std::printf("glb: %d failure(s).\n", g_fail);
    return 1;
}
