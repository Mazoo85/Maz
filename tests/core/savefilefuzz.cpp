// Malformed-input robustness fuzz for the binary save-file deserializers a game loads from untrusted bytes:
// the input-replay reader (core::Replay<T>::load) and the checkpoint save-file reader
// (core::Checkpoints::loadFile), plus the move-to-front inverse transform (io::mtfDecode). A player (or a
// cloud-save sync, or a shared "ghost" replay) supplies these, so a corrupt or hostile blob must fail cleanly
// (return false / leave state empty) — never crash, read out of bounds, or OOM. The existing save tests feed
// only well-formed round-trips; this feeds absurd count/length headers, truncations and byte mutations of
// valid seeds, and random buffers. Built under ASan+UBSan (the sanitizer CI job runs it). This harness found
// and now guards a real OOM DoS: Replay::load reserve()d frameCount frames from a hostile 4-byte header (and
// expanded runs unboundedly), so a tiny file could force a multi-gigabyte allocation.
#include "maz/core/Replay.hpp"
#include "maz/core/Checkpoints.hpp"
#include "maz/io/MoveToFront.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

// A representative fixed-step input snapshot (buttons bitmask + analog stick), the kind of POD Replay<T> holds.
struct Input {
    std::uint16_t buttons = 0;
    std::int16_t stickX = 0;
    std::int16_t stickY = 0;
};

static void feedReplay(const std::vector<std::uint8_t>& b) {
    core::Replay<Input> r;
    if (r.load(b)) g_sink += r.size();
}
static void feedCkpt(const std::vector<std::uint8_t>& b) {
    core::Checkpoints c;
    if (c.loadFile(b)) g_sink += 1;
}
static void feedMtf(const std::vector<std::uint8_t>& b) { g_sink += io::mtfDecode(b).size(); }

static void truncate(void (*fn)(const std::vector<std::uint8_t>&), const std::vector<std::uint8_t>& seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) fn(std::vector<std::uint8_t>(seed.begin(), seed.begin() + k));
    std::vector<std::uint8_t> s = seed;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const std::uint8_t orig = s[i];
        for (std::uint8_t p : {std::uint8_t(0), std::uint8_t(0xff), std::uint8_t(0x80), std::uint8_t(0x01)}) {
            s[i] = p;
            fn(s);
        }
        s[i] = orig;
    }
}

int main() {
    auto u32 = [](std::vector<std::uint8_t>& b, std::uint32_t v) {
        b.push_back(static_cast<std::uint8_t>(v & 0xff));
        b.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
        b.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
        b.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
    };

    // --- structural stressors: count/length headers that claim absurd sizes (the OOM shapes) ---
    { std::vector<std::uint8_t> b = {'M','Z','R','P'};
      u32(b, 1); u32(b, sizeof(Input)); u32(b, 0xFFFFFFFFu); u32(b, 0xFFFFFFFFu);   // frameCount/runCount huge
      feedReplay(b); }
    { std::vector<std::uint8_t> b = {'M','Z','R','P'};
      u32(b, 1); u32(b, sizeof(Input)); u32(b, 1000000u); u32(b, 1);                 // one run, huge count
      u32(b, 0xFFFFFFFFu); for (std::size_t i = 0; i < sizeof(Input); ++i) b.push_back(0);
      feedReplay(b); }
    { std::vector<std::uint8_t> b = {'M','Z','S','V'};
      u32(b, 1); u32(b, 0xFFFFFFFFu);                                                // slotCount huge
      u32(b, 0xFFFFFFFFu);                                                           // nameLen huge
      feedCkpt(b); }

    // --- round-trip sanity: a real replay must still load after the guard ---
    { core::Replay<Input> r;
      for (int i = 0; i < 5000; ++i) r.record(Input{static_cast<std::uint16_t>(i % 3), 0, 0});
      const auto bytes = r.serialize();
      core::Replay<Input> r2;
      if (!r2.load(bytes) || r2.size() != 5000) { std::printf("FAIL: replay round-trip broken\n"); return 1; }
    }
    { core::Checkpoints c;
      c.save("slot1", {1, 2, 3}, 10);
      c.save("slot2", {4, 5}, 20);
      const auto bytes = c.serialize();
      core::Checkpoints c2;
      if (!c2.loadFile(bytes) || !c2.has("slot1") || !c2.has("slot2")) {
          std::printf("FAIL: checkpoints round-trip broken\n"); return 1;
      }
    }

    // --- truncations + byte mutations of small valid seeds ---
    { core::Replay<Input> r; for (int i = 0; i < 6; ++i) r.record(Input{static_cast<std::uint16_t>(i), 1, 2});
      truncate(feedReplay, r.serialize()); }
    { core::Checkpoints c; c.save("a", {9, 8, 7}, 1); c.save("bb", {6}, 2);
      truncate(feedCkpt, c.serialize()); }

    // --- random buffers ---
    std::mt19937 rng(0x5A7Eu);
    for (int iter = 0; iter < 60000; ++iter) {
        const std::size_t n = rng() % 64u;
        std::vector<std::uint8_t> b(n);
        for (auto& x : b) x = static_cast<std::uint8_t>(rng() & 0xff);
        feedReplay(b);
        feedCkpt(b);
        feedMtf(b);
    }

    std::printf("savefile fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
