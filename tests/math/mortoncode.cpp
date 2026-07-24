// tests/math/mortoncode.cpp — verifies Morton (Z-order) codes (math MortonCode.hpp).
// Ground truths, deterministic (exhaustive + sampled coordinates, no <random>, no clock):
//   * ROUND TRIP (airtight): decode(encode(x,y)) == (x,y) for every 2D coordinate in a full 256x256 grid,
//     and for a sampled 3D grid — the defining property of a bijective code;
//   * KNOWN VALUES: encode(0,0)=0, (1,0)=1, (0,1)=2, (1,1)=3, (2,0)=4, (0,2)=8, (3,3)=15;
//   * BIT LAYOUT: encode(x,0) places x only in the even bits; encode(0,y) only in the odd bits;
//   * MONOTONE ON AN AXIS: along a single axis (other coords 0) the code increases with the coordinate;
//   * LOCALITY: two coordinates one cell apart never differ in code by more than a small bound.
#include "maz/math/MortonCode.hpp"

#include <cstdint>
#include <cstdio>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

int main() {
    using namespace maz::math;

    // --- 1. 2D round trip over a full 256x256 grid. ---
    {
        bool ok = true;
        for (std::uint32_t y = 0; y < 256 && ok; ++y) {
            for (std::uint32_t x = 0; x < 256; ++x) {
                const std::uint32_t code = mortonEncode2D(static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y));
                std::uint16_t dx = 0, dy = 0;
                mortonDecode2D(code, dx, dy);
                if (dx != x || dy != y) {
                    ok = false;
                    break;
                }
            }
        }
        CHECK(ok, "2D decode(encode(x,y)) recovers every coordinate in a 256x256 grid");
    }

    // --- 2. Known values. ---
    {
        CHECK(mortonEncode2D(0, 0) == 0u, "encode(0,0) = 0");
        CHECK(mortonEncode2D(1, 0) == 1u, "encode(1,0) = 1");
        CHECK(mortonEncode2D(0, 1) == 2u, "encode(0,1) = 2");
        CHECK(mortonEncode2D(1, 1) == 3u, "encode(1,1) = 3");
        CHECK(mortonEncode2D(2, 0) == 4u, "encode(2,0) = 4");
        CHECK(mortonEncode2D(0, 2) == 8u, "encode(0,2) = 8");
        CHECK(mortonEncode2D(3, 3) == 15u, "encode(3,3) = 15");
    }

    // --- 3. Bit layout: x in even bits, y in odd bits. ---
    {
        bool ok = true;
        for (std::uint32_t v = 0; v < 65536; ++v) {
            const std::uint32_t cx = mortonEncode2D(static_cast<std::uint16_t>(v), 0);
            const std::uint32_t cy = mortonEncode2D(0, static_cast<std::uint16_t>(v));
            if ((cx & 0xAAAAAAAAu) != 0u || (cy & 0x55555555u) != 0u) { // cx must have no odd bits; cy no even bits
                ok = false;
                break;
            }
        }
        CHECK(ok, "encode(x,0) lives in even bits and encode(0,y) in odd bits");
    }

    // --- 4. Monotone along a single axis + locality bound. ---
    {
        bool mono = true;
        std::uint32_t prev = mortonEncode2D(0, 0);
        for (std::uint32_t x = 1; x < 1024; ++x) {
            const std::uint32_t c = mortonEncode2D(static_cast<std::uint16_t>(x), 0);
            if (c <= prev) {
                mono = false;
                break;
            }
            prev = c;
        }
        CHECK(mono, "encode(x,0) strictly increases with x");
        // Locality: adjacent cells in x differ by at most a bounded code gap on a small grid.
        std::uint32_t worstGap = 0;
        for (std::uint32_t y = 0; y < 32; ++y) {
            for (std::uint32_t x = 0; x + 1 < 32; ++x) {
                const std::uint32_t a = mortonEncode2D(static_cast<std::uint16_t>(x), static_cast<std::uint16_t>(y));
                const std::uint32_t b = mortonEncode2D(static_cast<std::uint16_t>(x + 1), static_cast<std::uint16_t>(y));
                const std::uint32_t gap = a > b ? a - b : b - a;
                worstGap = gap > worstGap ? gap : worstGap;
            }
        }
        CHECK(worstGap < 2048u, "one-cell steps stay reasonably local in code space");
    }

    // --- 5. 3D round trip over a sampled grid. ---
    {
        bool ok = true;
        for (std::uint32_t z = 0; z < 64 && ok; z += 3) {
            for (std::uint32_t y = 0; y < 64 && ok; y += 3) {
                for (std::uint32_t x = 0; x < 64; x += 3) {
                    const std::uint32_t code = mortonEncode3D(static_cast<std::uint16_t>(x),
                                                              static_cast<std::uint16_t>(y),
                                                              static_cast<std::uint16_t>(z));
                    std::uint16_t dx = 0, dy = 0, dz = 0;
                    mortonDecode3D(code, dx, dy, dz);
                    if (dx != x || dy != y || dz != z) {
                        ok = false;
                        break;
                    }
                }
            }
        }
        CHECK(ok, "3D decode(encode(x,y,z)) recovers the coordinates");
        CHECK(mortonEncode3D(1, 0, 0) == 1u && mortonEncode3D(0, 1, 0) == 2u && mortonEncode3D(0, 0, 1) == 4u,
              "3D unit axes encode to 1, 2, 4");
    }

    if (g_fail == 0) {
        std::printf("mortoncode: OK — round trip, known values, bit layout, monotone/locality, 3D.\n");
        return 0;
    }
    std::printf("mortoncode: %d failure(s).\n", g_fail);
    return 1;
}
