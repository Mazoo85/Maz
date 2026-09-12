// tests/io/binarydiff.cpp — verifies the binary diff/patch (io BinaryDiff.hpp).
// Ground truths, deterministic (no <random>, no clock):
//   * CORRECTNESS: for thousands of random (src, dst) pairs, binaryPatch(src, binaryDiff(src,dst)) == dst,
//     including the hard cases where src and dst share almost nothing;
//   * COMPRESSION: when dst is src with a few small edits, the patch is far smaller than dst;
//   * empty src and/or empty dst behave;
//   * a malformed / truncated / out-of-range patch is rejected, never overruns.
#include "maz/io/BinaryDiff.hpp"

#include <cstdint>
#include <cstdio>
#include <vector>

using maz::io::binaryDiff;
using maz::io::binaryPatch;
using Bytes = std::vector<std::uint8_t>;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    std::uint8_t byte(std::uint32_t alphabet) {
        return static_cast<std::uint8_t>(next() % alphabet);
    }
};

int main() {
    // --- 1. Correctness over random unrelated buffers (small alphabet -> forces some matches too). ---
    {
        Lcg rng{0xB1D1FFu};
        bool ok = true;
        for (int trial = 0; trial < 4000 && ok; ++trial) {
            const std::uint32_t alpha = 2u + rng.next() % 8u;
            Bytes src, dst;
            const std::uint32_t sn = rng.next() % 400u;
            const std::uint32_t dn = rng.next() % 400u;
            for (std::uint32_t i = 0; i < sn; ++i) src.push_back(rng.byte(alpha));
            for (std::uint32_t i = 0; i < dn; ++i) dst.push_back(rng.byte(alpha));
            const Bytes patch = binaryDiff(src, dst);
            Bytes out;
            if (!binaryPatch(src, patch, out) || out != dst) { ok = false; break; }
        }
        CHECK(ok, "patch(src, diff(src,dst)) reconstructs dst exactly (random buffers)");
    }

    // --- 2. Correctness for dst = edited copy of src, and the patch is small. ---
    {
        Lcg rng{0xED17u};
        bool ok = true;
        long patchTotal = 0, dstTotal = 0;
        for (int trial = 0; trial < 2000 && ok; ++trial) {
            Bytes src;
            const std::uint32_t sn = 500u + rng.next() % 500u;
            for (std::uint32_t i = 0; i < sn; ++i) src.push_back(static_cast<std::uint8_t>(rng.next() & 0xffu));
            // dst = src with a few byte flips + one small insertion + one small deletion.
            Bytes dst = src;
            for (int e = 0; e < 3; ++e) {
                if (!dst.empty()) dst[rng.next() % dst.size()] = static_cast<std::uint8_t>(rng.next() & 0xffu);
            }
            const std::size_t insAt = dst.empty() ? 0 : rng.next() % dst.size();
            dst.insert(dst.begin() + static_cast<std::ptrdiff_t>(insAt),
                       static_cast<std::uint8_t>(rng.next() & 0xffu));
            if (dst.size() > 10) dst.erase(dst.begin() + static_cast<std::ptrdiff_t>(rng.next() % dst.size()));

            const Bytes patch = binaryDiff(src, dst);
            Bytes out;
            if (!binaryPatch(src, patch, out) || out != dst) { ok = false; break; }
            patchTotal += static_cast<long>(patch.size());
            dstTotal += static_cast<long>(dst.size());
        }
        CHECK(ok, "edited-copy round-trips exactly");
        // A handful of edits on a ~750-byte buffer should compress massively (mostly COPY ops).
        CHECK(patchTotal * 3 < dstTotal, "patch of an edited copy is far smaller than the target");
    }

    // --- 3. Empty buffers. ---
    {
        Bytes empty, some{1, 2, 3, 4, 5};
        Bytes out;
        CHECK(binaryPatch(empty, binaryDiff(empty, empty), out) && out.empty(), "empty->empty");
        CHECK(binaryPatch(empty, binaryDiff(empty, some), out) && out == some, "empty->some (all literals)");
        CHECK(binaryPatch(some, binaryDiff(some, empty), out) && out.empty(), "some->empty");
        CHECK(binaryPatch(some, binaryDiff(some, some), out) && out == some, "identical->identical");
    }

    // --- 4. Malformed patches rejected (never overruns). ---
    {
        Bytes src{10, 20, 30, 40, 50};
        Bytes out;
        CHECK(!binaryPatch(src, Bytes{0x02}, out), "unknown op rejected");
        CHECK(!binaryPatch(src, Bytes{0x00}, out), "COPY missing operands rejected");
        // COPY with out-of-range source region (offset 0, len 250 > src size).
        Bytes badCopy{0x00, 0x00, 0xFA, 0x01}; // tag, varint 0, varint 250
        CHECK(!binaryPatch(src, badCopy, out), "out-of-range COPY rejected");
        // ADD claiming more literal bytes than remain.
        Bytes badAdd{0x01, 0x05, 0x01, 0x02}; // ADD len 5 but only 2 bytes follow
        CHECK(!binaryPatch(src, badAdd, out), "truncated ADD rejected");
    }

    if (g_fail == 0) {
        std::printf("binarydiff: OK — random correctness, edited-copy compression, empties, error handling.\n");
        return 0;
    }
    std::printf("binarydiff: %d failure(s).\n", g_fail);
    return 1;
}
