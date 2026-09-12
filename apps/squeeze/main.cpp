// Maz Engine — "SQUEEZE" (io::Huffman, io::Lzw, io::Bwt, io::MoveToFront, io::Varint,
// io::BinaryDiff — the engine's byte-squeezing shelf, toward Godot's FileAccess compression modes)
// Six codecs that a save file, a resource pack or a network packet reaches for, run over the same two
// inputs so the numbers can be compared honestly. LEFT: the same 912-byte block through Huffman and LZW,
// with the ratio each achieves and a round-trip check, plus what BWT + move-to-front does to it first —
// which compresses nothing on its own but turns runs into zeros for the coder after it. MIDDLE: varints,
// where a small number costs one byte and a large one ten, and zig-zag makes that true of negatives too.
// RIGHT: a binary patch between two nearly-identical builds, the case an update downloader cares about.
// Every input is fixed, so the ratios are the same every run and the picture is the test.
// --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the six are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/io/BinaryDiff.hpp"
#include "maz/io/Bwt.hpp"
#include "maz/io/Huffman.hpp"
#include "maz/io/Lzw.hpp"
#include "maz/io/MoveToFront.hpp"
#include "maz/io/Varint.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::size_t countZeros(const std::vector<std::uint8_t>& v) {
    std::size_t n = 0;
    for (std::uint8_t b : v) {
        if (b == 0) n++;
    }
    return n;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("SQUEEZE starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Squeeze";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- The sample: text with the redundancy a real save file has ------------------------------------
    // Repeated keys, repeated words, a narrow alphabet. Exactly what these codecs are for, and exactly
    // what a random block is not — a random block is why "compression" with no sample is a meaningless
    // number.
    std::string sampleText;
    for (int i = 0; i < 16; ++i) {
        sampleText += "{\"name\":\"player\",\"hp\":100,\"pos\":[12,34],\"room\":\"cellar\"},";
    }
    const std::vector<std::uint8_t> sample(sampleText.begin(), sampleText.end());
    const double originalSize = static_cast<double>(sample.size());

    const std::vector<std::uint8_t> huff = io::huffmanCompress(sample);
    const std::vector<std::uint8_t> lzw = io::lzwCompress(sample);
    const bool huffOk = io::huffmanDecompress(huff) == sample;
    const bool lzwOk = io::lzwDecompress(lzw) == sample;
    const double huffRatio = 100.0 * static_cast<double>(huff.size()) / originalSize;
    const double lzwRatio = 100.0 * static_cast<double>(lzw.size()) / originalSize;

    // BWT + move-to-front: neither shrinks anything. Together they rewrite the block so the coder that
    // comes next has far less to say — which shows up as a pile of zeros where runs used to be.
    const io::BwtResult bwt = io::bwtEncode(sample);
    const std::vector<std::uint8_t> mtf = io::mtfEncode(bwt.data);
    const std::size_t zerosBefore = countZeros(sample);
    const std::size_t zerosAfter = countZeros(mtf);
    const std::vector<std::uint8_t> bwtHuff = io::huffmanCompress(mtf);
    const double pipelineRatio = 100.0 * static_cast<double>(bwtHuff.size()) / originalSize;
    const bool bwtOk = io::bwtDecode(io::mtfDecode(mtf), bwt.primaryIndex) == sample;

    // ---- Varints: a length prefix that costs what the number is worth ---------------------------------
    struct VarintRow {
        const char* label;
        std::uint64_t value;
        std::size_t bytes;
    };
    const VarintRow varints[] = {
        {"0", 0, io::varintSize(0)},
        {"127", 127, io::varintSize(127)},
        {"128", 128, io::varintSize(128)},
        {"65535", 65535, io::varintSize(65535)},
        {"2^32", 4294967296ULL, io::varintSize(4294967296ULL)},
        {"2^63", 9223372036854775808ULL, io::varintSize(9223372036854775808ULL)},
    };

    // Zig-zag: without it, -1 is a 64-bit number with every high bit set and costs ten bytes.
    const std::size_t plainMinusOne = io::varintSize(static_cast<std::uint64_t>(-1));
    const std::size_t zigzagMinusOne = io::varintSize(io::zigzagEncode(-1));
    const bool zigzagOk = io::zigzagDecode(io::zigzagEncode(-12345)) == -12345;

    // A round trip through a packed buffer, since varints are only useful in sequence.
    std::vector<std::uint8_t> packed;
    const std::uint64_t toPack[] = {1, 300, 70000, 5, 4294967296ULL};
    for (std::uint64_t v : toPack) {
        io::appendVarint(packed, v);
    }
    std::size_t offset = 0;
    bool packedOk = true;
    for (std::uint64_t expected : toPack) {
        std::uint64_t got = 0;
        if (!io::readVarint(packed, offset, got) || got != expected) {
            packedOk = false;
        }
    }
    const std::size_t packedFixed = sizeof(toPack);   // the same five as fixed 64-bit fields

    // ---- A binary patch between two builds -------------------------------------------------------------
    // The update-downloader case: a large file, a small change, and the question of how much has to
    // travel over the wire.
    std::vector<std::uint8_t> v1(4096);
    for (std::size_t i = 0; i < v1.size(); ++i) {
        v1[i] = static_cast<std::uint8_t>((i * 37 + (i / 16)) & 0xFF);
    }
    std::vector<std::uint8_t> v2 = v1;
    for (std::size_t i = 1000; i < 1064; ++i) {
        v2[i] = static_cast<std::uint8_t>(i & 0x1F);     // 64 bytes changed in the middle
    }
    const std::vector<std::uint8_t> patch = io::binaryDiff(v1, v2);
    std::vector<std::uint8_t> patched;
    const bool patchOk = io::binaryPatch(v1, patch, patched) && patched == v2;
    const double patchRatio = 100.0 * static_cast<double>(patch.size()) / static_cast<double>(v2.size());

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kNo{1.0f, 0.48f, 0.42f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.30f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  SQUEEZE", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "Huffman + LZW + BWT + move-to-front + varints + binary diff, same inputs",
                          kDim, 0.34f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 240.0f, y, value.c_str(), colour, sz);
            };
            auto ok = [&](bool b) { return b ? kOk : kNo; };

            // ---- column 1: the codecs ----
            float y = 106.0f;
            font.drawText(*renderer, 24.0f, y, "ONE BLOCK, EVERY WAY", kHead, 0.36f);
            y += 36.0f;
            row(24.0f, y, "original", num(originalSize, 0) + " bytes", kText); y += 26.0f;
            row(24.0f, y, "io::huffmanCompress", std::to_string(huff.size()) + "  (" +
                              num(huffRatio) + "%)", kVal); y += 26.0f;
            row(24.0f, y, "io::lzwCompress", std::to_string(lzw.size()) + "  (" +
                              num(lzwRatio) + "%)", kVal); y += 26.0f;
            row(24.0f, y, "BWT + MTF + Huffman", std::to_string(bwtHuff.size()) + "  (" +
                              num(pipelineRatio) + "%)", kVal); y += 32.0f;
            row(24.0f, y, "round trips", std::string(huffOk ? "huffman " : "HUFFMAN? ") +
                              (lzwOk ? "lzw " : "LZW? ") + (bwtOk ? "bwt" : "BWT?"),
                ok(huffOk && lzwOk && bwtOk)); y += 34.0f;
            font.drawText(*renderer, 24.0f, y, "the sample is repetitive JSON — what a save file is",
                          kDim, 0.26f);
            y += 34.0f;
            font.drawText(*renderer, 24.0f, y, "WHAT BWT + MTF ACTUALLY DO", kHead, 0.32f);
            y += 30.0f;
            row(24.0f, y, "zero bytes before", std::to_string(zerosBefore), kVal); y += 26.0f;
            row(24.0f, y, "zero bytes after", std::to_string(zerosAfter), kOk); y += 26.0f;
            font.drawText(*renderer, 24.0f, y,
                          "neither shrinks anything — they make runs into zeros for the next coder",
                          kDim, 0.26f);

            // ---- column 2: varints ----
            y = 106.0f;
            font.drawText(*renderer, 560.0f, y, "io::Varint  -  PAY FOR WHAT YOU SEND", kHead, 0.36f);
            y += 36.0f;
            for (const VarintRow& v : varints) {
                row(560.0f, y, v.label, std::to_string(v.bytes) + (v.bytes == 1 ? " byte" : " bytes"),
                    kVal);
                y += 26.0f;
            }
            y += 10.0f;
            row(560.0f, y, "-1 as a raw uint64", std::to_string(plainMinusOne) + " bytes", kNo);
            y += 26.0f;
            row(560.0f, y, "-1 zig-zagged", std::to_string(zigzagMinusOne) + " bytes", kOk);
            y += 26.0f;
            row(560.0f, y, "zig-zag round trip", zigzagOk ? "exact" : "BROKEN", ok(zigzagOk));
            y += 32.0f;
            row(560.0f, y, "five values packed", std::to_string(packed.size()) + " bytes", kOk);
            y += 26.0f;
            row(560.0f, y, "same five, fixed 64-bit", std::to_string(packedFixed) + " bytes", kNo);
            y += 26.0f;
            row(560.0f, y, "read back", packedOk ? "all five, in order" : "MISMATCH", ok(packedOk));
            y += 32.0f;
            font.drawText(*renderer, 560.0f, y,
                          "small numbers are common; this is why a packet is not a struct", kDim, 0.26f);

            // ---- column 3: the patch ----
            y = 106.0f;
            font.drawText(*renderer, 1060.0f, y, "io::BinaryDiff", kHead, 0.36f);
            y += 36.0f;
            row(1060.0f, y, "build v1", std::to_string(v1.size()) + " bytes", kText); y += 26.0f;
            row(1060.0f, y, "build v2", std::to_string(v2.size()) + " bytes", kText); y += 26.0f;
            row(1060.0f, y, "bytes changed", "64", kText); y += 26.0f;
            row(1060.0f, y, "patch", std::to_string(patch.size()) + "  (" + num(patchRatio) + "%)",
                kOk); y += 26.0f;
            row(1060.0f, y, "patch applies", patchOk ? "v1 + patch == v2" : "BROKEN", ok(patchOk));
            y += 34.0f;
            font.drawText(*renderer, 1060.0f, y, "what an update downloads", kDim, 0.26f);
            y += 24.0f;
            font.drawText(*renderer, 1060.0f, y, "instead of the whole build", kDim, 0.26f);

            font.drawText(*renderer, 24.0f, 664.0f,
                          "Every size above is measured, not quoted. A codec's ratio is a fact about "
                          "its input, so the inputs are fixed and shown.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("SQUEEZE shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
