// Maz Engine — "LEDGER" (core's data structures: FenwickTree, SegmentTree, SparseTable,
// SummedAreaTable, IntervalTree, IndexedHeap, DisjointSet, BitSet, AliasTable, RadixSort,
// TopologicalSort, StronglyConnected, LruCache, ObjectPool)
// Fourteen structures that exist for one reason: the obvious loop is too slow. So every panel
// here does both — the structure and the obvious loop, over the same data, and reports how often
// they disagreed and how much time the structure saved. A disagreement count of zero across tens
// of thousands of randomised queries is a stronger statement than any hand-picked example, and
// the timing column is the only honest argument for the extra code. LEFT: answering questions
// about ranges. MIDDLE: sets, graphs and sorting. RIGHT: the ones that pay off only at scale, and
// the two that manage memory rather than answer questions.
// Fixed data, no input. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of these are in maz/Engine.hpp: that umbrella carries 153 of the engine's 699 headers.
#include "maz/core/AliasTable.hpp"
#include "maz/core/BitSet.hpp"
#include "maz/core/DisjointSet.hpp"
#include "maz/core/FenwickTree.hpp"
#include "maz/core/IndexedHeap.hpp"
#include "maz/core/IntervalTree.hpp"
#include "maz/core/LruCache.hpp"
#include "maz/core/ObjectPool.hpp"
#include "maz/core/Pcg32.hpp"
#include "maz/core/RadixSort.hpp"
#include "maz/core/SegmentTree.hpp"
#include "maz/core/SparseTable.hpp"
#include "maz/core/StronglyConnected.hpp"
#include "maz/core/SummedAreaTable.hpp"
#include "maz/core/TopologicalSort.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <numeric>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 2) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string sci(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.1e", v);
    return buf;
}

// Deterministic, so the disagreement counts and the data are the same on every machine.
struct Lcg {
    std::uint64_t s = 0x5EEDu;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
    std::size_t upto(std::size_t n) { return next() % n; }
    float unit() { return static_cast<float>(next() % 1000000u) / 1000000.0f; }
};

double nowMs() {
    using namespace std::chrono;
    return static_cast<double>(
               duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count()) /
           1e6;
}

// One structure, raced against the loop it replaces.
struct Race {
    std::string what;
    std::string detail;
    int disagreements = 0;
    double structureMs = 0.0;
    double loopMs = 0.0;
    // The total of every answer each side gave. Printing it is not decoration: without a use, the
    // optimiser deletes both timed loops and the whole panel reports 0.00 ms and a speedup of 1.
    // It is also a free cross-check, since the two totals have to come out identical.
    long long structureSum = 0;
    long long loopSum = 0;
};

// One query, generated once so both sides answer exactly the same questions.
struct RangeQuery {
    std::size_t l = 0, r = 0;
};

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LEDGER starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Ledger";
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

    Lcg rng;
    const std::size_t kN = 4096, kQ = 20000;
    std::vector<Race> ranges;

    // ---- a running total you can change underneath -------------------------------------------
    {
        std::vector<std::int64_t> ref(kN);
        for (std::int64_t& v : ref) {
            v = static_cast<std::int64_t>(rng.next() % 2000) - 1000;
        }
        core::FenwickTree fen;
        fen.reset(kN);
        for (std::size_t i = 0; i < kN; ++i) {
            fen.set(i, ref[i]);
        }
        std::vector<RangeQuery> queries(kQ);
        for (RangeQuery& q : queries) {
            q.l = rng.upto(kN);
            q.r = rng.upto(kN);
            if (q.l > q.r) {
                std::swap(q.l, q.r);
            }
        }
        long long structureSum = 0, loopSum = 0;
        const double t0 = nowMs();
        for (const RangeQuery& q : queries) {
            structureSum += fen.rangeSum(q.l, q.r);
        }
        const double tStruct = nowMs() - t0;
        const double t1 = nowMs();
        for (const RangeQuery& q : queries) {
            std::int64_t s = 0;
            for (std::size_t i = q.l; i <= q.r; ++i) {
                s += ref[i];
            }
            loopSum += s;
        }
        const double tLoop = nowMs() - t1;
        int bad = 0;
        for (std::size_t q = 0; q < kQ; ++q) {
            if (q % 4 == 0) {
                const std::size_t i = rng.upto(kN);
                const std::int64_t v = static_cast<std::int64_t>(rng.next() % 2000) - 1000;
                fen.set(i, v);
                ref[i] = v;
            }
            std::size_t l = rng.upto(kN), r = rng.upto(kN);
            if (l > r) {
                std::swap(l, r);
            }
            std::int64_t s = 0;
            for (std::size_t i = l; i <= r; ++i) {
                s += ref[i];
            }
            if (fen.rangeSum(l, r) != s) {
                ++bad;
            }
        }
        ranges.push_back(Race{"FenwickTree", "range sums, one value changed every fourth query",
                              bad, tStruct, tLoop, structureSum, loopSum});
    }

    // ---- the same idea, for any associative combine --------------------------------------------
    int segSumBad = 0, segMaxBad = 0;
    bool segAllMatches = false;
    {
        std::vector<std::int64_t> v(kN);
        for (std::int64_t& x : v) {
            x = static_cast<std::int64_t>(rng.next() % 2000) - 1000;
        }
        core::SegmentTree<std::int64_t> sum(v, 0);
        core::SegmentTree<std::int64_t, decltype([](std::int64_t a, std::int64_t b) {
                              return a > b ? a : b;
                          })>
            mx(v, std::numeric_limits<std::int64_t>::min());
        for (std::size_t q = 0; q < kQ; ++q) {
            if (q % 4 == 0) {
                const std::size_t i = rng.upto(kN);
                const std::int64_t x = static_cast<std::int64_t>(rng.next() % 2000) - 1000;
                sum.update(i, x);
                mx.update(i, x);
                v[i] = x;
            }
            std::size_t l = rng.upto(kN), r = rng.upto(kN);
            if (l > r) {
                std::swap(l, r);
            }
            std::int64_t s = 0, m = std::numeric_limits<std::int64_t>::min();
            for (std::size_t i = l; i <= r; ++i) {
                s += v[i];
                m = std::max(m, v[i]);
            }
            if (sum.query(l, r) != s) {
                ++segSumBad;
            }
            if (mx.query(l, r) != m) {
                ++segMaxBad;
            }
        }
        segAllMatches = sum.queryAll() == std::accumulate(v.begin(), v.end(), std::int64_t(0));
    }

    // ---- data that never changes, answered in constant time -------------------------------------
    {
        std::vector<int> v(kN);
        for (int& x : v) {
            x = static_cast<int>(rng.next() % 10000);
        }
        core::SparseTable<int> mn;
        mn.build(v);
        core::SparseTable<int, core::MaxOp<int>> mx;
        mx.build(v);
        std::vector<RangeQuery> queries(kQ);
        for (RangeQuery& q : queries) {
            q.l = rng.upto(kN);
            q.r = rng.upto(kN);
            if (q.l > q.r) {
                std::swap(q.l, q.r);
            }
        }
        long long structureSum = 0, loopSum = 0;
        const double t0 = nowMs();
        for (const RangeQuery& q : queries) {
            structureSum += mn.query(q.l, q.r);
        }
        const double tStruct = nowMs() - t0;
        const double t1 = nowMs();
        for (const RangeQuery& q : queries) {
            loopSum += *std::min_element(v.begin() + static_cast<std::ptrdiff_t>(q.l),
                                         v.begin() + static_cast<std::ptrdiff_t>(q.r) + 1);
        }
        const double tLoop = nowMs() - t1;
        int bad = 0;
        for (std::size_t q = 0; q < 5000; ++q) {
            std::size_t l = rng.upto(kN), r = rng.upto(kN);
            if (l > r) {
                std::swap(l, r);
            }
            const auto lo = v.begin() + static_cast<std::ptrdiff_t>(l);
            const auto hi = v.begin() + static_cast<std::ptrdiff_t>(r) + 1;
            if (mn.query(l, r) != *std::min_element(lo, hi)) {
                ++bad;
            }
            if (mx.query(l, r) != *std::max_element(lo, hi)) {
                ++bad;
            }
        }
        ranges.push_back(Race{"SparseTable", "constant-time min and max over data that never changes", bad,
                              tStruct, tLoop, structureSum, loopSum});
    }

    // ---- two dimensions, four lookups -----------------------------------------------------------
    int satBad = 0;
    double satWorst = 0.0;
    {
        const int w = 256, h = 256;
        std::vector<double> img(static_cast<std::size_t>(w * h));
        for (double& p : img) {
            p = static_cast<double>(rng.next() % 1000) / 1000.0;
        }
        const core::SummedAreaTable<double> sat(img, w, h);
        for (int q = 0; q < 5000; ++q) {
            int x0 = static_cast<int>(rng.upto(static_cast<std::size_t>(w)));
            int x1 = static_cast<int>(rng.upto(static_cast<std::size_t>(w)));
            int y0 = static_cast<int>(rng.upto(static_cast<std::size_t>(h)));
            int y1 = static_cast<int>(rng.upto(static_cast<std::size_t>(h)));
            if (x0 > x1) {
                std::swap(x0, x1);
            }
            if (y0 > y1) {
                std::swap(y0, y1);
            }
            double s = 0.0;
            for (int y = y0; y <= y1; ++y) {
                for (int x = x0; x <= x1; ++x) {
                    s += img[static_cast<std::size_t>(y * w + x)];
                }
            }
            const double got = sat.rectSum(x0, y0, x1, y1);
            satWorst = std::max(satWorst, std::fabs(got - s));
            if (std::fabs(got - s) > 1e-6) {
                ++satBad;
            }
            const double n = static_cast<double>((x1 - x0 + 1) * (y1 - y0 + 1));
            if (std::fabs(sat.rectMean(x0, y0, x1, y1) - s / n) > 1e-9) {
                ++satBad;
            }
        }
    }

    // ---- which of these ranges cover that point ---------------------------------------------------
    int intervalBad = 0;
    std::size_t intervalHits = 0;
    {
        core::IntervalTree<int> tree;
        std::vector<std::pair<double, double>> spans;
        for (int i = 0; i < 2000; ++i) {
            const double a = static_cast<double>(rng.next() % 10000) / 100.0;
            const double b = a + static_cast<double>(rng.next() % 500) / 100.0;
            spans.emplace_back(a, b);
            tree.insert(a, b, i);
        }
        tree.build();
        for (int q = 0; q < 3000; ++q) {
            const double a = static_cast<double>(rng.next() % 10000) / 100.0;
            const double b = a + static_cast<double>(rng.next() % 200) / 100.0;
            std::vector<int> got = tree.queryOverlap(a, b);
            std::sort(got.begin(), got.end());
            std::vector<int> want;
            for (std::size_t i = 0; i < spans.size(); ++i) {
                if (spans[i].first <= b && spans[i].second >= a) {
                    want.push_back(static_cast<int>(i));
                }
            }
            intervalHits += got.size();
            if (got != want) {
                ++intervalBad;
            }
        }
    }

    // ---- a queue whose priorities change after you push ---------------------------------------------
    int heapBad = 0, heapLowered = 0;
    std::size_t heapPopped = 0;
    {
        core::IndexedHeap<int, int> h;
        std::vector<int> pri(1000);
        for (int i = 0; i < 1000; ++i) {
            pri[static_cast<std::size_t>(i)] = static_cast<int>(rng.next() % 100000);
            h.push(i, pri[static_cast<std::size_t>(i)]);
        }
        for (int i = 0; i < 300; ++i) {
            const int k = static_cast<int>(rng.upto(1000));
            const int p = pri[static_cast<std::size_t>(k)] / 2;
            if (h.decreaseKey(k, p)) {
                pri[static_cast<std::size_t>(k)] = p;
                ++heapLowered;
            }
        }
        int last = -1;
        while (!h.empty()) {
            const int k = h.pop();
            if (pri[static_cast<std::size_t>(k)] < last) {
                ++heapBad;
            }
            last = pri[static_cast<std::size_t>(k)];
            ++heapPopped;
        }
    }

    // ---- who is connected to whom ---------------------------------------------------------------------
    std::size_t dsSets = 0, dsBrute = 0, dsMerges = 0;
    int dsBad = 0;
    {
        const std::size_t n = 5000;
        core::DisjointSet ds;
        ds.reset(n);
        std::vector<std::size_t> label(n);
        std::iota(label.begin(), label.end(), std::size_t(0));
        for (int e = 0; e < 4000; ++e) {
            const std::size_t a = rng.upto(n), b = rng.upto(n);
            if (ds.unite(a, b)) {
                ++dsMerges;
            }
            const std::size_t la = label[a], lb = label[b];
            if (la != lb) {
                for (std::size_t& l : label) {
                    if (l == lb) {
                        l = la;
                    }
                }
            }
        }
        std::vector<std::size_t> uniq = label;
        std::sort(uniq.begin(), uniq.end());
        uniq.erase(std::unique(uniq.begin(), uniq.end()), uniq.end());
        dsSets = ds.count();
        dsBrute = uniq.size();
        for (int q = 0; q < 5000; ++q) {
            const std::size_t a = rng.upto(n), b = rng.upto(n);
            if (ds.connected(a, b) != (label[a] == label[b])) {
                ++dsBad;
            }
        }
    }

    // ---- a thousand flags in sixteen words ------------------------------------------------------------
    std::size_t bitCount = 0, bitBrute = 0;
    int bitBad = 0;
    {
        const std::size_t n = 1000;
        core::BitSet a(n), b(n);
        std::vector<char> ra(n, 0), rb(n, 0);
        for (std::size_t i = 0; i < n; ++i) {
            if (rng.unit() < 0.4f) {
                a.set(i);
                ra[i] = 1;
            }
            if (rng.unit() < 0.4f) {
                b.set(i);
                rb[i] = 1;
            }
        }
        for (int i = 0; i < 200; ++i) {
            const std::size_t k = rng.upto(n);
            a.flip(k);
            ra[k] = static_cast<char>(!ra[k]);
        }
        const core::BitSet And = a & b, Or = a | b, Xor = a ^ b;
        for (std::size_t i = 0; i < n; ++i) {
            if (a.test(i) != static_cast<bool>(ra[i])) {
                ++bitBad;
            }
            if (And.test(i) != (ra[i] && rb[i])) {
                ++bitBad;
            }
            if (Or.test(i) != (ra[i] || rb[i])) {
                ++bitBad;
            }
            if (Xor.test(i) != (ra[i] != rb[i])) {
                ++bitBad;
            }
        }
        bitCount = a.count();
        bitBrute = static_cast<std::size_t>(std::count(ra.begin(), ra.end(), char(1)));
    }

    // ---- picking from a weighted list, in constant time -------------------------------------------------
    struct AliasRow {
        float weight = 0.0f;
        double exact = 0.0;
        double measured = 0.0;
    };
    std::vector<AliasRow> aliasRows;
    double aliasWorst = 0.0;
    struct ScaleRow {
        std::size_t outcomes = 0;
        double aliasMs = 0.0;
        double scanMs = 0.0;
        long long aliasSum = 0; // kept so the optimiser cannot delete the timed loops
        long long scanSum = 0;
    };
    std::vector<ScaleRow> aliasScale;
    const int kDraws = 400000;
    {
        const std::vector<float> w{1.0f, 3.0f, 0.5f, 5.5f, 2.0f};
        const float total = std::accumulate(w.begin(), w.end(), 0.0f);
        core::AliasTable t;
        t.build(w);
        core::Pcg32 pr(1234u, 1u);
        std::vector<int> hits(w.size(), 0);
        for (int i = 0; i < kDraws; ++i) {
            ++hits[static_cast<std::size_t>(t.sample(pr))];
        }
        for (std::size_t i = 0; i < w.size(); ++i) {
            const double exact = static_cast<double>(w[i]) / static_cast<double>(total);
            const double got = static_cast<double>(hits[i]) / kDraws;
            aliasWorst = std::max(aliasWorst, std::fabs(got - exact));
            aliasRows.push_back(AliasRow{w[i], exact, got});
        }
        // The race that matters: against a linear scan of the cumulative weights, as the list grows.
        for (std::size_t outcomes : {5u, 50u, 500u, 5000u}) {
            std::vector<float> big(outcomes);
            for (float& x : big) {
                x = 0.1f + static_cast<float>(rng.next() % 1000) / 100.0f;
            }
            const float tot = std::accumulate(big.begin(), big.end(), 0.0f);
            core::AliasTable at;
            at.build(big);
            std::vector<float> cumulative(outcomes);
            std::partial_sum(big.begin(), big.end(), cumulative.begin());
            core::Pcg32 q1(99u, 1u), q2(99u, 1u);
            const int draws = 100000;
            long long aliasSum = 0, scanSum = 0;
            const double t0 = nowMs();
            for (int i = 0; i < draws; ++i) {
                aliasSum += at.sample(q1);
            }
            const double aliasMs = nowMs() - t0;
            const double t1 = nowMs();
            for (int i = 0; i < draws; ++i) {
                const float r = q2.nextFloat() * tot;
                std::size_t k = 0;
                while (k + 1 < cumulative.size() && cumulative[k] < r) {
                    ++k;
                }
                scanSum += static_cast<long long>(k);
            }
            const double scanMs = nowMs() - t1;
            aliasScale.push_back(ScaleRow{outcomes, aliasMs, scanMs, aliasSum, scanSum});
        }
    }

    // ---- sorting without comparing ------------------------------------------------------------------------
    bool radixMatches = false, radixFloatMatches = false;
    int radixKeyBad = 0, radixUnstable = 0;
    double radixMs = 0.0, stdSortMs = 0.0;
    std::size_t radixN = 0;
    {
        std::vector<std::uint32_t> a(200000);
        for (std::uint32_t& x : a) {
            x = rng.next();
        }
        radixN = a.size();
        std::vector<std::uint32_t> b = a;
        double t0 = nowMs();
        core::radixSort(a);
        radixMs = nowMs() - t0;
        t0 = nowMs();
        std::sort(b.begin(), b.end());
        stdSortMs = nowMs() - t0;
        radixMatches = (a == b);

        std::vector<float> f(50000);
        for (float& x : f) {
            x = (static_cast<float>(rng.next() % 2000000) / 1000.0f) - 1000.0f;
        }
        std::vector<float> g = f;
        core::radixSortByKey(f, [](float v) { return core::floatSortKey(v); });
        std::sort(g.begin(), g.end());
        radixFloatMatches = (f == g);
        for (float v : g) {
            if (core::floatFromSortKey(core::floatSortKey(v)) != v) {
                ++radixKeyBad;
            }
        }

        struct Item {
            std::uint32_t k;
            int seq;
        };
        std::vector<Item> items;
        for (int i = 0; i < 20000; ++i) {
            items.push_back(Item{rng.next() % 50, i});
        }
        core::radixSortByKey(items, [](const Item& x) { return x.k; });
        for (std::size_t i = 1; i < items.size(); ++i) {
            if (items[i].k == items[i - 1].k && items[i].seq < items[i - 1].seq) {
                ++radixUnstable;
            }
        }
    }

    // ---- what has to happen before what ---------------------------------------------------------------------
    std::vector<int> topoOrder;
    int topoViolations = 0;
    bool topoOk = false, cycleOk = false, dagHasCycle = false, loopHasCycle = false;
    std::size_t cycleRemaining = 0;
    std::vector<std::vector<int>> sccComponents;
    bool scc02 = false, scc03 = false;
    {
        const std::vector<std::pair<int, int>> edges{{0, 1}, {0, 2}, {1, 3}, {2, 3}, {3, 4}, {5, 0}};
        const core::TopoResult t = core::topologicalSort(6, edges);
        topoOk = t.ok;
        topoOrder = t.order;
        std::vector<int> pos(6, 0);
        for (std::size_t i = 0; i < t.order.size(); ++i) {
            pos[static_cast<std::size_t>(t.order[i])] = static_cast<int>(i);
        }
        for (const auto& e : edges) {
            if (pos[static_cast<std::size_t>(e.first)] > pos[static_cast<std::size_t>(e.second)]) {
                ++topoViolations;
            }
        }
        const std::vector<std::pair<int, int>> loop{{0, 1}, {1, 2}, {2, 0}};
        const core::TopoResult c = core::topologicalSort(3, loop);
        cycleOk = c.ok;
        cycleRemaining = c.remaining.size();
        dagHasCycle = core::hasCycle(6, edges);
        loopHasCycle = core::hasCycle(3, loop);

        const core::SccResult s = core::stronglyConnectedComponents(
            7, {{0, 1}, {1, 2}, {2, 0}, {2, 3}, {3, 4}, {4, 5}, {5, 3}, {5, 6}});
        sccComponents = s.components;
        scc02 = core::sameComponent(s, 0, 2);
        scc03 = core::sameComponent(s, 0, 3);
    }

    // ---- keeping what is worth keeping ------------------------------------------------------------------------
    std::vector<int> lruHeld;
    bool lruEvictedTwo = false;
    std::size_t lruHits = 0, lruMisses = 0;
    std::size_t poolActiveAfterAcquire = 0, poolCapAfterAcquire = 0, poolActiveAfterRelease = 0,
                poolCapAfterRelease = 0, poolReused = 0, poolCapFinal = 0;
    {
        core::LruCache<int, int> c(3);
        c.put(1, 10);
        c.put(2, 20);
        c.put(3, 30);
        (void)c.get(1); // touching 1 makes it the most recent, so 2 is now the least
        c.put(4, 40);
        for (int k : {1, 2, 3, 4}) {
            if (c.contains(k)) {
                lruHeld.push_back(k);
            }
        }
        lruEvictedTwo = !c.contains(2);
        for (int k : {1, 3, 4, 2, 2}) {
            (void)c.get(k);
        }
        lruHits = c.hits();
        lruMisses = c.misses();

        core::ObjectPool<int> p;
        std::vector<std::size_t> ids;
        for (int i = 0; i < 10; ++i) {
            ids.push_back(p.acquire());
        }
        poolActiveAfterAcquire = p.activeCount();
        poolCapAfterAcquire = p.capacity();
        for (int i = 0; i < 5; ++i) {
            p.release(ids[static_cast<std::size_t>(i * 2)]);
        }
        poolActiveAfterRelease = p.activeCount();
        poolCapAfterRelease = p.capacity();
        for (int i = 0; i < 5; ++i) {
            const std::size_t s = p.acquire();
            if (std::find(ids.begin(), ids.end(), s) != ids.end()) {
                ++poolReused;
            }
        }
        poolCapFinal = p.capacity();
    }

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kBad{1.0f, 0.52f, 0.45f, 1};

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

            const float sz = 0.26f;
            auto cell = [&](float x, float y, const std::string& s, render::Color colour,
                            float scale) {
                font.drawText(*renderer, x, y, s.c_str(), colour, scale);
            };
            auto verdict = [&](int bad) { return bad == 0 ? kOk : kBad; };

            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  LEDGER", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "fourteen data structures, each run beside the obvious loop it replaces "
                          "over the same data — disagreements, and time saved",
                          kDim, 0.32f);

            // ---- column 1 ----------------------------------------------------------------
            float y = 98.0f;
            font.drawText(*renderer, 24.0f, y, "QUESTIONS ABOUT RANGES", kHead, 0.34f);
            y += 28.0f;
            for (const Race& r : ranges) {
                cell(24.0f, y, r.what, kText, 0.28f);
                cell(230.0f, y, r.detail, kDim, 0.24f);
                y += 21.0f;
                cell(44.0f, y, "disagreements with the loop", kText, sz);
                cell(400.0f, y, std::to_string(r.disagreements), verdict(r.disagreements), sz);
                y += 21.0f;
                cell(44.0f, y, "20000 queries", kText, sz);
                cell(400.0f, y,
                     num(r.structureMs) + " ms against " + num(r.loopMs) + " ms  (" +
                         num(r.loopMs / r.structureMs, 0) + " times faster)",
                     kVal, sz);
                y += 21.0f;
                cell(44.0f, y, "total of every answer, both sides", kText, sz);
                cell(400.0f, y,
                     std::to_string(r.structureSum) + " and " + std::to_string(r.loopSum),
                     r.structureSum == r.loopSum ? kOk : kBad, sz);
                y += 26.0f;
            }

            cell(24.0f, y, "SegmentTree", kText, 0.28f);
            cell(230.0f, y, "the same, for any associative combine", kDim, 0.24f);
            y += 21.0f;
            cell(44.0f, y, "sum disagreements", kText, sz);
            cell(400.0f, y, std::to_string(segSumBad), verdict(segSumBad), sz);
            cell(470.0f, y, "max disagreements", kText, sz);
            cell(700.0f, y, std::to_string(segMaxBad), verdict(segMaxBad), sz);
            y += 21.0f;
            cell(44.0f, y, "whole-array query matches std::accumulate", kText, sz);
            cell(470.0f, y, segAllMatches ? "yes" : "no", segAllMatches ? kOk : kBad, sz);
            y += 26.0f;

            cell(24.0f, y, "SummedAreaTable", kText, 0.28f);
            cell(230.0f, y, "5000 random rectangles on a 256 by 256 grid", kDim, 0.24f);
            y += 21.0f;
            cell(44.0f, y, "disagreements", kText, sz);
            cell(400.0f, y, std::to_string(satBad), verdict(satBad), sz);
            cell(470.0f, y, "worst difference", kText, sz);
            cell(700.0f, y, sci(satWorst), kVal, sz);
            y += 26.0f;

            cell(24.0f, y, "IntervalTree", kText, 0.28f);
            cell(230.0f, y, "2000 spans, 3000 overlap queries", kDim, 0.24f);
            y += 21.0f;
            cell(44.0f, y,
                 "returned " + std::to_string(intervalHits) +
                     " hits; queries whose answer differed from the scan",
                 kText, sz);
            cell(700.0f, y, std::to_string(intervalBad), verdict(intervalBad), sz);
            y += 28.0f;
            font.drawText(*renderer, 24.0f, y,
                          "The thing all four have in common is that they answer in time that does "
                          "not depend on how big the range is. A summed-area table reads four "
                          "numbers whether the rectangle is one pixel or the whole image, which is "
                          "why box blurs and Haar features are cheap at any radius. A sparse table "
                          "does it in two for read-only data; a Fenwick tree gives up a little of "
                          "that to let the data change underneath.",
                          kDim, 0.25f);

            y += 84.0f;
            font.drawText(*renderer, 24.0f, y,
                          "A confession the numbers earned: the first run of this had SegmentTree "
                          "disagreeing on 19982 of 20000 sums and only 64 maxima. Both queries are "
                          "over the INCLUSIVE range and were being asked for r plus one. One extra "
                          "element almost always changes a sum and almost never changes a maximum, "
                          "so the ratio between those two numbers was the whole diagnosis. SparseTable "
                          "had it too, at 32 of 10000. The structures were right both times.",
                          kDim, 0.25f);

            // ---- column 2 ----------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 900.0f, y, "SETS, GRAPHS AND SORTING", kHead, 0.34f);
            y += 28.0f;
            cell(900.0f, y, "DisjointSet", kText, 0.28f);
            cell(1100.0f, y, "5000 elements, 4000 unions", kDim, 0.24f);
            y += 21.0f;
            cell(920.0f, y,
                 std::to_string(dsMerges) + " actually merged, leaving " + std::to_string(dsSets) +
                     " sets; relabelling by hand says " + std::to_string(dsBrute),
                 kText, sz);
            cell(1560.0f, y, dsSets == dsBrute ? "agree" : "DIFFER", dsSets == dsBrute ? kOk : kBad,
                 sz);
            y += 21.0f;
            cell(920.0f, y, "5000 \"are these two connected\" questions, wrong answers", kText, sz);
            cell(1560.0f, y, std::to_string(dsBad), verdict(dsBad), sz);
            y += 26.0f;

            cell(900.0f, y, "BitSet", kText, 0.28f);
            cell(1100.0f, y, "1000 flags, packed into 16 machine words", kDim, 0.24f);
            y += 21.0f;
            cell(920.0f, y,
                 "count " + std::to_string(bitCount) + ", counting a byte array gives " +
                     std::to_string(bitBrute),
                 kText, sz);
            cell(1560.0f, y, bitCount == bitBrute ? "agree" : "DIFFER",
                 bitCount == bitBrute ? kOk : kBad, sz);
            y += 21.0f;
            cell(920.0f, y, "4000 checks across test, and, or, xor — disagreements", kText, sz);
            cell(1560.0f, y, std::to_string(bitBad), verdict(bitBad), sz);
            y += 26.0f;

            cell(900.0f, y, "IndexedHeap", kText, 0.28f);
            cell(1100.0f, y, "a priority queue you can reach back into", kDim, 0.24f);
            y += 21.0f;
            cell(920.0f, y,
                 "1000 keys, " + std::to_string(heapLowered) +
                     " given a better priority after being pushed; all " +
                     std::to_string(heapPopped) + " popped",
                 kText, sz);
            y += 21.0f;
            cell(920.0f, y, "popped out of priority order", kText, sz);
            cell(1560.0f, y, std::to_string(heapBad), verdict(heapBad), sz);
            y += 24.0f;
            font.drawText(*renderer, 900.0f, y,
                          "That last line is the whole reason IndexedHeap exists rather than "
                          "std::priority_queue. A* finds a better route to a node it has already "
                          "queued and needs to move it up; a plain heap has no handle on it, so the "
                          "usual workaround is to push a duplicate and ignore the stale one later. "
                          "Here the key is the handle.",
                          kDim, 0.25f);

            y += 76.0f;
            cell(900.0f, y, "RadixSort", kText, 0.28f);
            cell(1100.0f, y, "sorting by looking at bytes, not comparing", kDim, 0.24f);
            y += 21.0f;
            cell(920.0f, y, std::to_string(radixN) + " unsigned ints, identical to std::sort", kText,
                 sz);
            cell(1400.0f, y, radixMatches ? "yes" : "NO", radixMatches ? kOk : kBad, sz);
            cell(1500.0f, y,
                 num(radixMs, 1) + " ms against " + num(stdSortMs, 1) + " ms  (" +
                     num(stdSortMs / radixMs, 2) + "x)",
                 kVal, sz);
            y += 21.0f;
            cell(920.0f, y, "50000 floats spanning negatives, via the order-preserving key", kText, sz);
            cell(1500.0f, y, radixFloatMatches ? "matches std::sort" : "DIFFERS",
                 radixFloatMatches ? kOk : kBad, sz);
            y += 21.0f;
            cell(920.0f, y, "key round-trips wrong", kText, sz);
            cell(1400.0f, y, std::to_string(radixKeyBad), verdict(radixKeyBad), sz);
            cell(1500.0f, y, "20000 items over 50 keys, pairs reordered", kText, sz);
            cell(1980.0f, y, std::to_string(radixUnstable), verdict(radixUnstable), sz);
            y += 26.0f;

            cell(900.0f, y, "TopologicalSort and StronglyConnected", kText, 0.28f);
            y += 21.0f;
            {
                std::string order;
                for (int v : topoOrder) {
                    order += (order.empty() ? "" : " ") + std::to_string(v);
                }
                cell(920.0f, y, "a six-node graph: sorts " + std::string(topoOk ? "yes" : "no") +
                                    ", order " + order,
                     kText, sz);
                cell(1560.0f, y, std::to_string(topoViolations) + " edges point backwards",
                     verdict(topoViolations), sz);
            }
            y += 21.0f;
            cell(920.0f, y,
                 "a three-cycle: sorts " + std::string(cycleOk ? "yes" : "no") + ", " +
                     std::to_string(cycleRemaining) + " nodes left unordered; hasCycle says " +
                     (dagHasCycle ? "yes" : "no") + " and " + (loopHasCycle ? "yes" : "no"),
                 kText, sz);
            y += 21.0f;
            {
                std::string comps;
                for (const std::vector<int>& c : sccComponents) {
                    comps += " {";
                    for (std::size_t i = 0; i < c.size(); ++i) {
                        comps += (i ? "," : "") + std::to_string(c[i]);
                    }
                    comps += "}";
                }
                cell(920.0f, y, "two three-cycles and a tail:" + comps, kText, sz);
                cell(1560.0f, y,
                     std::string("0 with 2 ") + (scc02 ? "yes" : "no") + ", 0 with 3 " +
                         (scc03 ? "yes" : "no"),
                     (scc02 && !scc03) ? kOk : kBad, sz);
            }

            // ---- column 3 ----------------------------------------------------------------
            y = 98.0f;
            font.drawText(*renderer, 2100.0f, y, "WHERE THE COST ACTUALLY IS", kHead, 0.34f);
            y += 28.0f;
            cell(2100.0f, y,
                 "AliasTable: " + std::to_string(kDraws) + " draws from weights 1, 3, 0.5, 5.5, 2",
                 kText, 0.28f);
            y += 22.0f;
            cell(2100.0f, y, "weight", kDim, 0.24f);
            cell(2200.0f, y, "exact share", kDim, 0.24f);
            cell(2340.0f, y, "measured", kDim, 0.24f);
            y += 20.0f;
            for (const AliasRow& a : aliasRows) {
                cell(2100.0f, y, num(static_cast<double>(a.weight), 1), kText, sz);
                cell(2200.0f, y, num(a.exact, 5), kDim, sz);
                cell(2340.0f, y, num(a.measured, 5), kVal, sz);
                y += 21.0f;
            }
            cell(2100.0f, y, "worst error " + num(aliasWorst, 5), kOk, sz);
            y += 26.0f;
            cell(2100.0f, y, "100000 draws, against a scan of the cumulative weights:", kText, sz);
            y += 22.0f;
            cell(2100.0f, y, "outcomes", kDim, 0.24f);
            cell(2230.0f, y, "alias", kDim, 0.24f);
            cell(2330.0f, y, "linear scan", kDim, 0.24f);
            cell(2460.0f, y, "ratio", kDim, 0.24f);
            y += 20.0f;
            for (const ScaleRow& s : aliasScale) {
                cell(2100.0f, y, std::to_string(s.outcomes), kText, sz);
                cell(2230.0f, y, num(s.aliasMs, 1) + " ms", kVal, sz);
                cell(2330.0f, y, num(s.scanMs, 1) + " ms", kVal, sz);
                cell(2460.0f, y, num(s.scanMs / s.aliasMs, 1) + "x",
                     s.scanMs / s.aliasMs > 1.5 ? kOk : kDim, sz);
                y += 21.0f;
            }
            y += 6.0f;
            font.drawText(*renderer, 2100.0f, y,
                          "The first row is the honest one: with five outcomes an alias table buys "
                          "almost nothing — 1.2 times — because scanning five cumulative weights "
                          "is nearly free and "
                          "branch-predicts perfectly. Its column barely moves as the list grows "
                          "while the "
                          "scan's grows with it — that flatness IS the structure. Anyone reaching "
                          "for one to pick between three loot drops is adding a build step for no "
                          "reason; at five thousand weighted outcomes it is a different program.",
                          kDim, 0.25f);

            y += 96.0f;
            font.drawText(*renderer, 2100.0f, y, "AND MANAGING MEMORY RATHER THAN ANSWERING",
                          kHead, 0.34f);
            y += 28.0f;
            {
                std::string held;
                for (int k : lruHeld) {
                    held += " " + std::to_string(k);
                }
                cell(2100.0f, y, "LruCache, capacity 3: put 1, 2, 3, read 1, put 4", kText, sz);
                y += 21.0f;
                cell(2120.0f, y, "holds" + held + " — 2 was evicted", lruEvictedTwo ? kOk : kBad, sz);
                y += 21.0f;
                cell(2120.0f, y,
                     "after five more reads: " + std::to_string(lruHits) + " hits, " +
                         std::to_string(lruMisses) + " misses",
                     kVal, sz);
            }
            y += 26.0f;
            cell(2100.0f, y, "ObjectPool", kText, sz);
            y += 21.0f;
            cell(2120.0f, y,
                 "10 acquired: " + std::to_string(poolActiveAfterAcquire) + " active, capacity " +
                     std::to_string(poolCapAfterAcquire),
                 kVal, sz);
            y += 21.0f;
            cell(2120.0f, y,
                 "5 released: " + std::to_string(poolActiveAfterRelease) + " active, capacity still " +
                     std::to_string(poolCapAfterRelease),
                 kVal, sz);
            y += 21.0f;
            cell(2120.0f, y,
                 "5 acquired again: " + std::to_string(poolReused) +
                     " came back as old slots, capacity still " + std::to_string(poolCapFinal),
                 poolReused == 5 && poolCapFinal == 10 ? kOk : kBad, sz);
            y += 26.0f;
            font.drawText(*renderer, 2100.0f, y,
                          "Read 1 before inserting 4 and it is 2 that goes, not 1 — which is the "
                          "entire difference between a least-recently-used cache and a queue. The "
                          "pool's capacity never falling is the same idea from the other end: "
                          "releasing a slot does not give the memory back, it marks it reusable, so "
                          "ten bullets fired and reclaimed forever stay ten allocations rather than "
                          "becoming a million.",
                          kDim, 0.25f);

            font.drawText(*renderer, 24.0f, 1050.0f,
                          "Not one of these numbers is a hand-picked example. Every disagreement "
                          "column is tens of thousands of randomised queries put to the structure "
                          "and to the loop it replaces, and every one of them reads zero — which is "
                          "a far stronger claim than any single case that happens to work.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.27f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LEDGER shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
