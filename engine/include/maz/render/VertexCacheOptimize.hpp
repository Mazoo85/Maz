#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render vertex-cache optimization (Forsyth's linear-speed algorithm) — reorders a mesh's triangle
// INDICES so the GPU's post-transform vertex cache hits far more often, a standard load-time mesh
// optimization every serious engine (and Godot's importer) runs. The GPU caches the last handful of
// transformed vertices; if consecutive triangles reuse those vertices the vertex shader runs far fewer
// times. A mesh straight out of an exporter is often ordered badly for this (e.g. a grid emitted
// row-by-row thrashes the cache). This rewrites the index order — the SAME triangles, just sequenced so
// neighbours share vertices — measured by ACMR (Average Cache Miss Ratio: cache misses per triangle; lower
// is better, ~0.5 is the floor for large closed meshes). Vertex positions are untouched, so it is a pure,
// lossless index permutation. Pure integer work — no GPU — so it unit-tests headlessly: the triangle SET is
// preserved and the simulated-cache ACMR drops.
//
// Reference: Tom Forsyth, "Linear-Speed Vertex Cache Optimisation" (2006).
namespace maz::render {

// Simulate an LRU vertex cache of `cacheSize` and return the ACMR (cache misses / triangle count) for an
// index list — the measure of how cache-friendly the ordering is, and what the test checks.
inline float simulateAcmr(const std::vector<std::uint32_t>& indices, int cacheSize = 32) {
    if (indices.size() < 3) return 0.0f;
    std::vector<std::uint32_t> cache;
    cache.reserve(static_cast<std::size_t>(cacheSize));
    std::size_t misses = 0;
    for (std::uint32_t v : indices) {
        bool hit = false;
        for (std::uint32_t c : cache) {
            if (c == v) { hit = true; break; }
        }
        if (!hit) {
            ++misses;
            cache.insert(cache.begin(), v);
            if (static_cast<int>(cache.size()) > cacheSize) cache.pop_back();
        }
    }
    const std::size_t tris = indices.size() / 3;
    return tris ? static_cast<float>(misses) / static_cast<float>(tris) : 0.0f;
}

namespace detail {

constexpr int kCacheSize = 32;
constexpr float kLastTriScore = 0.75f;
constexpr float kCacheDecayPower = 1.5f;
constexpr float kValenceBoostScale = 2.0f;
constexpr float kValenceBoostPower = 0.5f;

// Forsyth's per-vertex score: recently-cached vertices score high (LRU), and low-valence vertices (few
// remaining triangles) get a boost so they are not stranded. cachePos < 0 means "not in cache".
inline float vertexScore(int cachePos, int remainingTris) {
    if (remainingTris <= 0) return -1.0f;
    float score = 0.0f;
    if (cachePos >= 0) {
        if (cachePos < 3) {
            score = kLastTriScore; // the 3 verts of the most-recent triangle
        } else {
            const float scaler = 1.0f / static_cast<float>(kCacheSize - 3);
            const float linear = 1.0f - static_cast<float>(cachePos - 3) * scaler;
            score = std::pow(linear > 0.0f ? linear : 0.0f, kCacheDecayPower);
        }
    }
    score += kValenceBoostScale * std::pow(static_cast<float>(remainingTris), -kValenceBoostPower);
    return score;
}

} // namespace detail

// Reorder `indices` (triangle list, 3 per tri) to improve post-transform vertex-cache locality. Returns the
// new index order; the set of triangles is identical (a permutation of triangles + their vertex refs).
inline std::vector<std::uint32_t> optimizeVertexCache(const std::vector<std::uint32_t>& indices,
                                                      std::size_t vertexCount) {
    const std::size_t triCount = indices.size() / 3;
    if (triCount == 0 || vertexCount == 0) return indices;

    // Per-vertex: triangles that use it, and a live count.
    std::vector<std::vector<std::uint32_t>> vertTris(vertexCount);
    for (std::size_t t = 0; t < triCount; ++t) {
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t v = indices[t * 3 + static_cast<std::size_t>(k)];
            if (v < vertexCount) vertTris[v].push_back(static_cast<std::uint32_t>(t));
        }
    }

    std::vector<int> remaining(vertexCount);
    for (std::size_t v = 0; v < vertexCount; ++v) remaining[v] = static_cast<int>(vertTris[v].size());
    std::vector<int> cachePos(vertexCount, -1);
    std::vector<float> vscore(vertexCount, 0.0f);
    for (std::size_t v = 0; v < vertexCount; ++v) vscore[v] = detail::vertexScore(-1, remaining[v]);

    std::vector<char> triDone(triCount, 0);
    std::vector<float> triScore(triCount, 0.0f);
    auto computeTriScore = [&](std::size_t t) {
        return vscore[indices[t * 3 + 0]] + vscore[indices[t * 3 + 1]] + vscore[indices[t * 3 + 2]];
    };
    for (std::size_t t = 0; t < triCount; ++t) triScore[t] = computeTriScore(t);

    std::vector<std::uint32_t> out;
    out.reserve(indices.size());
    std::vector<std::uint32_t> cache; // most-recent at front

    int emitted = 0;
    int best = -1;
    while (emitted < static_cast<int>(triCount)) {
        if (best < 0) { // full scan fallback (start, or after a cache flush found nothing)
            float bestScore = -1.0f;
            for (std::size_t t = 0; t < triCount; ++t) {
                if (!triDone[t] && triScore[t] > bestScore) {
                    bestScore = triScore[t];
                    best = static_cast<int>(t);
                }
            }
        }
        if (best < 0) break;

        const std::size_t bt = static_cast<std::size_t>(best);
        triDone[bt] = 1;
        ++emitted;
        for (int k = 0; k < 3; ++k) {
            const std::uint32_t v = indices[bt * 3 + static_cast<std::size_t>(k)];
            out.push_back(v);
            --remaining[v];
            for (std::size_t i = 0; i < cache.size(); ++i) {
                if (cache[i] == v) { cache.erase(cache.begin() + static_cast<std::ptrdiff_t>(i)); break; }
            }
            cache.insert(cache.begin(), v);
        }
        if (cache.size() > static_cast<std::size_t>(detail::kCacheSize))
            cache.resize(static_cast<std::size_t>(detail::kCacheSize));

        for (std::size_t i = 0; i < cache.size(); ++i) cachePos[cache[i]] = static_cast<int>(i);

        // Re-score cached vertices, then pick the next-best triangle among those touching the cache.
        for (std::uint32_t v : cache) vscore[v] = detail::vertexScore(cachePos[v], remaining[v]);

        best = -1;
        float bestScore = -1.0f;
        for (std::uint32_t v : cache) {
            for (std::uint32_t t : vertTris[v]) {
                if (triDone[t]) continue;
                triScore[t] = computeTriScore(t);
                if (triScore[t] > bestScore) { bestScore = triScore[t]; best = static_cast<int>(t); }
            }
        }
    }
    return out;
}

} // namespace maz::render
