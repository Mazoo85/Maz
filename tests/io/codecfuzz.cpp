// Malformed-input robustness fuzz for the compression / entropy / binary-serialization decoders that a game
// runs on untrusted bytes: DEFLATE/zlib inflate, gzip, LZW, Huffman, the LZ77 codec, BWT inverse, the range
// (arithmetic) coder, MessagePack, and varints. Assets, save games, and downloaded content arrive as these;
// a corrupt or hostile blob must fail cleanly (return false/empty), never crash, read out of bounds, spin
// forever, or OOM. The existing codec tests feed only well-formed round-trips; this feeds truncations and
// byte mutations of valid seeds plus large volumes of random buffers, and — crucially — drives each decoder's
// untrusted length/count headers. Built under ASan+UBSan (the sanitizer CI job runs it). This harness found
// and now guards a real OOM DoS: rangeDecode trusted a 4-byte symbol-count header and reserve()d ~17 GB on a
// few-byte input; it now rejects an implausible count.
#include "maz/io/Inflate.hpp"
#include "maz/io/Gzip.hpp"
#include "maz/io/Lzw.hpp"
#include "maz/io/Huffman.hpp"
#include "maz/io/Compression.hpp"
#include "maz/io/Bwt.hpp"
#include "maz/io/RangeCoder.hpp"
#include "maz/io/MessagePack.hpp"
#include "maz/io/Varint.hpp"

#include <cstdint>
#include <cstdio>
#include <random>
#include <vector>

using namespace maz;

static volatile std::size_t g_sink = 0;

static void feed(const std::vector<std::uint8_t>& b) {
    { std::vector<std::uint8_t> o; if (io::inflateRaw(b, o)) g_sink += o.size(); }
    { std::vector<std::uint8_t> o; if (io::zlibInflate(b, o)) g_sink += o.size(); }
    { std::vector<std::uint8_t> o; if (io::gunzip(b, o)) g_sink += o.size(); }
    { auto o = io::lzwDecompress(b); g_sink += o.size(); }
    { auto o = io::huffmanDecompress(b); g_sink += o.size(); }
    { auto o = io::lzDecompress(b.data(), b.size()); g_sink += o.size(); }
    // BWT inverse with a spread of untrusted primary indices (in-range, boundary, and wildly out-of-range).
    for (std::size_t pi : {std::size_t(0), b.size() / 2, b.size(), b.size() + 1, std::size_t(-1)}) {
        auto o = io::bwtDecode(b, pi);
        g_sink += o.size();
    }
    { io::MsgValue v; if (io::msgpackDecode(b, v)) g_sink += 1; }
    { std::size_t off = 0; std::uint64_t u = 0;
      while (off < b.size() && io::readVarint(b, off, u)) g_sink += static_cast<std::size_t>(u); }
    { std::size_t off = 0; std::int64_t sv = 0;
      while (off < b.size() && io::readVarintSigned(b, off, sv)) g_sink += static_cast<std::size_t>(sv); }
    // Range decode needs a frequency model; derive a small valid one (each weight >= 1, total <= 65536).
    { std::vector<std::uint32_t> freq(4, 1);
      for (std::size_t i = 0; i < b.size() && i < 4; ++i) freq[i] = 1u + (b[i] & 0x7f);
      auto o = io::rangeDecode(b, freq);
      g_sink += o.size(); }
}

static void truncate(const std::vector<std::uint8_t>& seed) {
    for (std::size_t k = 0; k <= seed.size(); ++k) feed(std::vector<std::uint8_t>(seed.begin(), seed.begin() + k));
    std::vector<std::uint8_t> s = seed;
    for (std::size_t i = 0; i < s.size(); ++i) {
        const std::uint8_t orig = s[i];
        for (std::uint8_t p : {std::uint8_t(0), std::uint8_t(0xff), std::uint8_t(0x80), std::uint8_t(0x01)}) {
            s[i] = p;
            feed(s);
        }
        s[i] = orig;
    }
}

int main() {
    // Structural stressors: length/count headers that claim absurd sizes (the shapes that OOM'd before).
    feed({0xff, 0xff, 0xff, 0xff});                                    // rangeDecode: ~4.29e9 symbol count
    feed({0xdd, 0xff, 0xff, 0xff, 0xff});                             // msgpack array32: ~4.29e9 elements
    feed({0xde, 0xff, 0xff});                                          // msgpack map16: 65535 pairs
    feed({0xdb, 0xff, 0xff, 0xff, 0xff});                             // msgpack str32: ~4.29e9 bytes

    // Truncations + byte mutations of small valid-ish seeds.
    truncate({0x78, 0x9c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01});        // empty zlib stream
    truncate({0x1f, 0x8b, 0x08, 0x00, 0, 0, 0, 0, 0, 0x03,
              0, 0, 0, 0, 0, 0, 0, 0});                                 // gzip header + empty deflate
    truncate({0x93, 0x01, 0x02, 0x03});                                // msgpack array [1,2,3]
    truncate({0x82, 0xa1, 0x61, 0x01, 0xa1, 0x62, 0x02});              // msgpack map {"a":1,"b":2}
    truncate({0x80, 0x80, 0x80, 0x80, 0x08});                          // 5-byte varint

    // Random + slightly larger buffers.
    std::mt19937 rng(0xC0DEu);
    for (int iter = 0; iter < 40000; ++iter) {
        const std::size_t n = rng() % 65u;
        std::vector<std::uint8_t> b(n);
        for (auto& x : b) x = static_cast<std::uint8_t>(rng() & 0xff);
        feed(b);
    }
    for (int iter = 0; iter < 2000; ++iter) {
        const std::size_t n = 64 + rng() % 256u;
        std::vector<std::uint8_t> b(n);
        for (auto& x : b) x = static_cast<std::uint8_t>(rng() & 0xff);
        feed(b);
    }

    std::printf("codec fuzz: completed, sink=%zu\n", static_cast<std::size_t>(g_sink));
    return 0;
}
