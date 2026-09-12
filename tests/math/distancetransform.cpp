// tests/math/distancetransform.cpp — verifies the exact Euclidean distance transform (DistanceTransform.hpp).
// Ground truths, deterministic:
//   * a single seed: every cell's distance equals the exact hypot to it, and the nearest is that seed;
//   * a seed cell reports distance 0 and points to itself;
//   * against a brute-force O(n*n*seeds) nearest-seed search on a small multi-seed grid, EVERY cell's
//     distance matches exactly and the reported nearest seed really is a nearest seed;
//   * an empty grid (no seeds) leaves distances at the large sentinel and nearest coords at -1.
#include "maz/math/DistanceTransform.hpp"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::math::distanceTransform;
using maz::math::DistanceField;

static bool near(float a, float b, float e = 1e-4f) { return std::fabs(a - b) < e; }

int main() {
    // --- 1. Single seed: exact hypot everywhere, nearest is the seed. ---
    {
        const int w = 9, h = 7;
        std::vector<uint8_t> seed(static_cast<size_t>(w * h), 0);
        const int sx = 3, sy = 2;
        seed[static_cast<size_t>(sx + sy * w)] = 1;
        const DistanceField df = distanceTransform(seed, w, h);
        bool ok = true, nn = true;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int i = x + y * w;
                const float exact = std::sqrt(static_cast<float>((x - sx) * (x - sx) + (y - sy) * (y - sy)));
                if (!near(df.distance[static_cast<size_t>(i)], exact)) ok = false;
                if (df.nearestX[static_cast<size_t>(i)] != sx || df.nearestY[static_cast<size_t>(i)] != sy)
                    nn = false;
            }
        }
        CHECK(ok, "single seed: distance equals exact hypot at every cell");
        CHECK(nn, "single seed: every cell points at the one seed");
        CHECK(near(df.distance[static_cast<size_t>(sx + sy * w)], 0.0f), "the seed cell has distance 0");
    }

    // --- 2. Brute-force exact match on a multi-seed grid. ---
    {
        const int w = 16, h = 12;
        std::vector<uint8_t> seed(static_cast<size_t>(w * h), 0);
        // A deterministic scatter of seeds.
        const int sxs[] = {1, 14, 7, 3, 11, 9};
        const int sys[] = {1, 2, 9, 6, 10, 4};
        const int ns = 6;
        for (int k = 0; k < ns; ++k) seed[static_cast<size_t>(sxs[k] + sys[k] * w)] = 1;
        const DistanceField df = distanceTransform(seed, w, h);

        bool distOk = true, nearestOk = true;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int i = x + y * w;
                float best = 1e30f;
                for (int k = 0; k < ns; ++k) {
                    const float d =
                        std::sqrt(static_cast<float>((x - sxs[k]) * (x - sxs[k]) + (y - sys[k]) * (y - sys[k])));
                    if (d < best) best = d;
                }
                if (!near(df.distance[static_cast<size_t>(i)], best, 1e-3f)) distOk = false;
                // The reported nearest must be an actual seed and at exactly the transform's distance.
                const int nx = df.nearestX[static_cast<size_t>(i)];
                const int ny = df.nearestY[static_cast<size_t>(i)];
                const bool isSeed = nx >= 0 && ny >= 0 && seed[static_cast<size_t>(nx + ny * w)] != 0;
                const float dToNearest =
                    std::sqrt(static_cast<float>((x - nx) * (x - nx) + (y - ny) * (y - ny)));
                if (!isSeed || !near(dToNearest, best, 1e-3f)) nearestOk = false;
            }
        }
        CHECK(distOk, "multi-seed: distances match brute-force nearest-seed search exactly");
        CHECK(nearestOk, "multi-seed: reported nearest seed is a real, genuinely-nearest seed");
    }

    // --- 3. Empty grid: no seeds -> large distance, nearest -1. ---
    {
        const int w = 5, h = 5;
        std::vector<uint8_t> seed(static_cast<size_t>(w * h), 0);
        const DistanceField df = distanceTransform(seed, w, h);
        CHECK(df.distance[0] > 1e10f, "no seeds -> effectively infinite distance");
        CHECK(df.nearestX[0] == -1 && df.nearestY[0] == -1, "no seeds -> nearest coords are -1");
    }

    if (g_fail == 0) {
        std::printf("distancetransform: OK — single-seed hypot, seed cell 0, brute-force exact match, "
                    "empty grid.\n");
        return 0;
    }
    std::printf("distancetransform: %d failure(s).\n", g_fail);
    return 1;
}
