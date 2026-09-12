#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

// maz::game::ChunkStreamer — chunked streaming for large tilemaps / open worlds. A world too big to
// hold in memory (or to draw) is cut into fixed-size square CHUNKS; only the chunks near the camera
// stay resident, and as the focus point moves the streamer reports which chunks to LOAD (newly in
// range) and which to UNLOAD (left range). That load/unload delta is exactly what a game feeds to
// its asset loader and renderer to page a giant map in and out without a hitch.
//
// This is the streaming layer Godot's TileMap doesn't provide (it loads a whole map at once); the
// streamer is generic — chunk coordinates are integer (cx, cy) cells of `chunkSize` world units,
// and a chunk is resident when it lies within `radius` chunks (Chebyshev / square window, or set
// `circular` for a disc) of the focus chunk. update() returns the delta AND updates the resident
// set; deterministic, allocation-light, header-only, no GPU.
namespace maz::game {

struct ChunkCoord {
    int x = 0;
    int y = 0;
    bool operator==(const ChunkCoord& o) const { return x == o.x && y == o.y; }
};

struct ChunkCoordHash {
    size_t operator()(const ChunkCoord& c) const {
        // Mix the two ints; good enough for a spatial set.
        return (static_cast<size_t>(static_cast<uint32_t>(c.x)) * 73856093u) ^
               (static_cast<size_t>(static_cast<uint32_t>(c.y)) * 19349663u);
    }
};

class ChunkStreamer {
  public:
    struct Delta {
        std::vector<ChunkCoord> toLoad;   // chunks that entered range this update
        std::vector<ChunkCoord> toUnload; // chunks that left range this update
    };

    ChunkStreamer() = default;
    ChunkStreamer(float chunkSize, int radius, bool circular = false)
        : m_chunkSize(chunkSize > 0 ? chunkSize : 1.0f), m_radius(radius < 0 ? 0 : radius),
          m_circular(circular) {}

    float chunkSize() const { return m_chunkSize; }
    int radius() const { return m_radius; }

    // The chunk containing a world point (floor division so negatives map correctly).
    ChunkCoord chunkAt(float worldX, float worldY) const {
        return {floorDiv(worldX, m_chunkSize), floorDiv(worldY, m_chunkSize)};
    }

    bool isResident(const ChunkCoord& c) const { return m_resident.count(c) != 0; }
    size_t residentCount() const { return m_resident.size(); }
    const std::unordered_set<ChunkCoord, ChunkCoordHash>& resident() const { return m_resident; }

    // The set of chunks that SHOULD be resident for a focus point (no state change).
    std::vector<ChunkCoord> desiredFor(float focusX, float focusY) const {
        const ChunkCoord center = chunkAt(focusX, focusY);
        std::vector<ChunkCoord> out;
        const int r = m_radius;
        for (int dy = -r; dy <= r; ++dy) {
            for (int dx = -r; dx <= r; ++dx) {
                if (m_circular && dx * dx + dy * dy > r * r) {
                    continue; // outside the disc
                }
                out.push_back({center.x + dx, center.y + dy});
            }
        }
        return out;
    }

    // Move the focus; returns the chunks to load/unload and updates the resident set to match.
    Delta update(float focusX, float focusY) {
        std::unordered_set<ChunkCoord, ChunkCoordHash> want;
        for (const ChunkCoord& c : desiredFor(focusX, focusY)) {
            want.insert(c);
        }
        Delta d;
        for (const ChunkCoord& c : want) {
            if (m_resident.count(c) == 0) {
                d.toLoad.push_back(c);
            }
        }
        for (const ChunkCoord& c : m_resident) {
            if (want.count(c) == 0) {
                d.toUnload.push_back(c);
            }
        }
        m_resident.swap(want);
        // Deterministic ordering of the reported deltas (sets don't guarantee iteration order).
        sortCoords(d.toLoad);
        sortCoords(d.toUnload);
        return d;
    }

    // Drop everything (e.g. on teleport/level change); the next update() reloads from scratch.
    void clear() { m_resident.clear(); }

  private:
    static int floorDiv(float v, float size) { return static_cast<int>(std::floor(v / size)); }
    static void sortCoords(std::vector<ChunkCoord>& v) {
        std::sort(v.begin(), v.end(), [](const ChunkCoord& a, const ChunkCoord& b) {
            return a.y != b.y ? a.y < b.y : a.x < b.x;
        });
    }

    float m_chunkSize = 1.0f;
    int m_radius = 1;
    bool m_circular = false;
    std::unordered_set<ChunkCoord, ChunkCoordHash> m_resident;
};

} // namespace maz::game
