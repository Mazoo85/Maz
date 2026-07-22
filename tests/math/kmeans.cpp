// tests/math/kmeans.cpp — verifies k-means clustering (math KMeans.hpp).
// Ground truths, deterministic (no <random>, no clock — seeded splitmix inside):
//   * on well-separated blobs, every point of a blob shares one cluster and exactly k clusters are used;
//   * at convergence the result is a Lloyd FIXPOINT: each point's assigned centroid is its nearest, and each
//     centroid equals the mean of its assigned points (the defining optimality conditions);
//   * reported inertia matches a recomputed sum of squared distances;
//   * k>=n gives inertia ~0 (each point its own cluster); determinism holds; empty input is handled.
#include "maz/math/KMeans.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <vector>

using maz::math::kMeans;
using maz::math::KMeansResult;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    double jitter() { return (static_cast<double>(next()) / 4294967296.0 - 0.5) * 2.0; } // [-1,1]
};

static double distSq(const std::vector<double>& a, const std::vector<double>& b) {
    double d = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) { const double e = a[i] - b[i]; d += e * e; }
    return d;
}

int main() {
    // --- 1. Well-separated blobs recovered; Lloyd fixpoint. ---
    {
        Lcg rng{0xC1057Eu};
        // 4 tight blobs centered far apart; jitter is tiny relative to spacing.
        const double ctr[4][2] = {{0, 0}, {100, 0}, {0, 100}, {100, 100}};
        std::vector<std::vector<double>> pts;
        std::vector<int> blobOf;
        for (int b = 0; b < 4; ++b) {
            for (int i = 0; i < 50; ++i) {
                pts.push_back({ctr[b][0] + rng.jitter() * 2.0, ctr[b][1] + rng.jitter() * 2.0});
                blobOf.push_back(b);
            }
        }
        const KMeansResult r = kMeans(pts, 4, 12345u);

        // Each blob maps to a single cluster label, and all four labels are distinct.
        int blobLabel[4] = {-1, -1, -1, -1};
        bool consistent = true;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const int b = blobOf[i];
            if (blobLabel[b] == -1) blobLabel[b] = r.assignment[i];
            else if (blobLabel[b] != r.assignment[i]) consistent = false;
        }
        std::set<int> labels(blobLabel, blobLabel + 4);
        CHECK(consistent, "each separated blob is one cluster");
        CHECK(labels.size() == 4, "the four blobs land in four distinct clusters");

        // Lloyd fixpoint: every point assigned to its nearest centroid.
        bool nearest = true;
        for (std::size_t i = 0; i < pts.size(); ++i) {
            double bestD = 1e300; int bestC = 0;
            for (std::size_t c = 0; c < r.centroids.size(); ++c) {
                const double d = distSq(pts[i], r.centroids[c]);
                if (d < bestD) { bestD = d; bestC = static_cast<int>(c); }
            }
            if (bestC != r.assignment[i]) nearest = false;
        }
        CHECK(nearest, "each point is assigned to its nearest centroid (fixpoint)");

        // Each centroid equals the mean of its assigned points.
        const std::size_t k = r.centroids.size();
        std::vector<std::vector<double>> sum(k, std::vector<double>(2, 0.0));
        std::vector<int> cnt(k, 0);
        for (std::size_t i = 0; i < pts.size(); ++i) {
            const std::size_t c = static_cast<std::size_t>(r.assignment[i]);
            sum[c][0] += pts[i][0]; sum[c][1] += pts[i][1]; ++cnt[c];
        }
        bool meanOk = true;
        for (std::size_t c = 0; c < k; ++c) {
            if (cnt[c] == 0) continue;
            const double mx = sum[c][0] / cnt[c], my = sum[c][1] / cnt[c];
            if (std::fabs(mx - r.centroids[c][0]) > 1e-6 || std::fabs(my - r.centroids[c][1]) > 1e-6)
                meanOk = false;
        }
        CHECK(meanOk, "each centroid is the mean of its members");

        // Reported inertia matches a recomputation.
        double inertia = 0.0;
        for (std::size_t i = 0; i < pts.size(); ++i)
            inertia += distSq(pts[i], r.centroids[static_cast<std::size_t>(r.assignment[i])]);
        CHECK(std::fabs(inertia - r.inertia) < 1e-6, "reported inertia matches recomputation");
    }

    // --- 2. k >= n -> inertia ~0 and determinism. ---
    {
        std::vector<std::vector<double>> pts;
        for (int i = 0; i < 10; ++i) pts.push_back({static_cast<double>(i), static_cast<double>(i * i)});
        const KMeansResult r = kMeans(pts, 10, 7u);
        CHECK(r.inertia < 1e-9, "k==n gives each point its own cluster (inertia ~0)");

        const KMeansResult a = kMeans(pts, 3, 99u);
        const KMeansResult b = kMeans(pts, 3, 99u);
        CHECK(a.assignment == b.assignment && a.centroids == b.centroids, "same seed -> identical result");
    }

    // --- 3. Empty input. ---
    {
        const KMeansResult r = kMeans({}, 3, 1u);
        CHECK(r.centroids.empty() && r.assignment.empty(), "empty input -> empty result");
    }

    if (g_fail == 0) {
        std::printf("kmeans: OK — blob recovery, Lloyd fixpoint, inertia, k>=n, determinism, empty.\n");
        return 0;
    }
    std::printf("kmeans: %d failure(s).\n", g_fail);
    return 1;
}
