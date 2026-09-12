// tests/core/kdtree3d.cpp — verifies the 3D k-d tree (core::KdTree3D) against brute force.
// Ground truths: nearest() matches the true argmin over a deterministic random cloud for many queries
// (including query points that coincide with a stored point and points far outside the cloud); kNearest()
// returns exactly the k closest indices in nondecreasing distance order and agrees with a brute-force sort;
// radius() returns exactly the set of points within r (as a set, unordered); empty-tree and k<=0 edge cases
// are safe; a single-point tree answers trivially. Pure CPU, deterministic (LCG, no <random>/clock).
#include "maz/core/KdTree3D.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::KdTree3D;
using maz::math::vec3;

// Deterministic LCG in [0,1).
struct Lcg {
    std::uint64_t s;
    float next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<float>((s >> 40) & 0xFFFFFF) / 16777216.0f;
    }
};

static float d2(vec3 a, vec3 b) {
    const float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

static int bruteNearest(const std::vector<vec3>& pts, vec3 q) {
    int best = -1;
    float bd = 0.0f;
    for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
        const float dd = d2(pts[static_cast<std::size_t>(i)], q);
        if (best < 0 || dd < bd) { best = i; bd = dd; }
    }
    return best;
}

int main() {
    Lcg rng{0x1234567u};
    std::vector<vec3> pts;
    for (int i = 0; i < 400; ++i) {
        pts.push_back(vec3(rng.next() * 20.0f - 10.0f, rng.next() * 20.0f - 10.0f,
                           rng.next() * 20.0f - 10.0f));
    }
    KdTree3D tree(pts);
    CHECK(tree.size() == pts.size(), "size matches");

    // --- 1. nearest() matches brute force for many queries. ---
    {
        int mism = 0;
        for (int t = 0; t < 300; ++t) {
            const vec3 q(rng.next() * 24.0f - 12.0f, rng.next() * 24.0f - 12.0f,
                         rng.next() * 24.0f - 12.0f);
            const int got = tree.nearest(q);
            const int exp = bruteNearest(pts, q);
            // Compare by distance (ties: different index but identical distance is acceptable).
            if (d2(pts[static_cast<std::size_t>(got)], q) != d2(pts[static_cast<std::size_t>(exp)], q)) {
                ++mism;
            }
        }
        CHECK(mism == 0, "nearest matches brute force over random queries");
    }

    // --- 2. Query exactly on a stored point returns that point (distance 0). ---
    {
        for (int t = 0; t < 20; ++t) {
            const int i = static_cast<int>(rng.next() * static_cast<float>(pts.size())) %
                          static_cast<int>(pts.size());
            const int got = tree.nearest(pts[static_cast<std::size_t>(i)]);
            CHECK(d2(pts[static_cast<std::size_t>(got)], pts[static_cast<std::size_t>(i)]) == 0.0f,
                  "nearest to a stored point is distance 0");
        }
    }

    // --- 3. A query far outside the cloud still finds the true nearest. ---
    {
        const vec3 q(1000.0f, -1000.0f, 500.0f);
        CHECK(d2(pts[static_cast<std::size_t>(tree.nearest(q))], q) ==
              d2(pts[static_cast<std::size_t>(bruteNearest(pts, q))], q),
              "far-away query still finds true nearest");
    }

    // --- 4. kNearest() agrees with a brute-force sort and is sorted nearest-first. ---
    {
        int mism = 0;
        for (int t = 0; t < 100; ++t) {
            const vec3 q(rng.next() * 24.0f - 12.0f, rng.next() * 24.0f - 12.0f,
                         rng.next() * 24.0f - 12.0f);
            const int k = 8;
            const std::vector<int> got = tree.kNearest(q, k);
            CHECK(static_cast<int>(got.size()) == k, "kNearest returns k results");
            // Sorted nondecreasing by distance.
            for (std::size_t i = 1; i < got.size(); ++i) {
                if (d2(pts[static_cast<std::size_t>(got[i - 1])], q) >
                    d2(pts[static_cast<std::size_t>(got[i])], q)) {
                    ++mism;
                }
            }
            // The k-th distance must equal the k-th smallest distance overall.
            std::vector<float> all;
            for (const vec3& p : pts) all.push_back(d2(p, q));
            std::sort(all.begin(), all.end());
            const float kthGot = d2(pts[static_cast<std::size_t>(got.back())], q);
            if (kthGot != all[static_cast<std::size_t>(k - 1)]) ++mism;
        }
        CHECK(mism == 0, "kNearest matches brute-force k-th distance and is sorted");
    }

    // --- 5. radius() returns exactly the in-radius set. ---
    {
        int mism = 0;
        for (int t = 0; t < 100; ++t) {
            const vec3 q(rng.next() * 24.0f - 12.0f, rng.next() * 24.0f - 12.0f,
                         rng.next() * 24.0f - 12.0f);
            const float r = 3.0f + rng.next() * 4.0f;
            const float r2 = r * r;
            const std::vector<int> gotVec = tree.radius(q, r);
            std::set<int> got(gotVec.begin(), gotVec.end());
            std::set<int> exp;
            for (int i = 0; i < static_cast<int>(pts.size()); ++i) {
                if (d2(pts[static_cast<std::size_t>(i)], q) <= r2) exp.insert(i);
            }
            if (got != exp) ++mism;
        }
        CHECK(mism == 0, "radius matches brute force as a set");
    }

    // --- 6. Edge cases: empty tree, k<=0, single point. ---
    {
        KdTree3D empty;
        CHECK(empty.size() == 0, "empty size 0");
        CHECK(empty.nearest(vec3(0, 0, 0)) == -1, "empty nearest -1");
        CHECK(empty.kNearest(vec3(0, 0, 0), 5).empty(), "empty kNearest empty");
        CHECK(empty.radius(vec3(0, 0, 0), 100.0f).empty(), "empty radius empty");

        CHECK(tree.kNearest(vec3(0, 0, 0), 0).empty(), "k=0 -> empty");
        CHECK(tree.kNearest(vec3(0, 0, 0), -3).empty(), "k<0 -> empty");

        std::vector<int> big = tree.kNearest(vec3(0, 0, 0), 100000);
        CHECK(big.size() == pts.size(), "k > n returns all points");

        KdTree3D one(std::vector<vec3>{vec3(2, 3, 4)});
        CHECK(one.nearest(vec3(0, 0, 0)) == 0, "single-point nearest is index 0");
        CHECK(one.kNearest(vec3(9, 9, 9), 5).size() == 1, "single-point kNearest size 1");
    }

    if (g_fail == 0) {
        std::printf("kdtree3d: all tests passed\n");
    }
    return g_fail == 0 ? 0 : 1;
}
