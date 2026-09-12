#pragma once

#include <algorithm>
#include <cstddef>
#include <vector>

namespace maz::render {

// Rectangle bin packer for texture atlases — the layout step behind Godot's atlas/sprite-sheet importer
// and dynamic font glyph caches. Give it a bin of fixed width×height and a set of rectangle sizes; it
// places each without overlap and reports where (or that it did not fit). It uses the **Skyline
// Bottom-Left** heuristic (track the upper contour of what's placed; drop each rect at the position whose
// resulting top is lowest, ties broken to the left), which packs tightly and, crucially, is fully
// deterministic — same inputs always give the same layout — so it unit-tests exactly and renders a
// golden-stable atlas. `pack()` height-sorts a batch first (the standard heuristic) while returning
// placements in the caller's original order.
//
// Honest scope: single-bin, axis-aligned, no rotation and no inter-rect padding (add spacing to your
// sizes if you need a gutter). It does NOT auto-grow the bin, pack across multiple pages, or do MaxRects
// / guillotine variants — those remain follow-ups.

struct PackSize {
    int w = 0;
    int h = 0;
};

struct Placement {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    bool placed = false;
};

class AtlasPacker {
public:
    AtlasPacker(int width, int height) : m_width(width), m_height(height) { reset(); }

    int width() const { return m_width; }
    int height() const { return m_height; }

    void reset() {
        m_skyline.clear();
        m_skyline.push_back(Node{0, 0, m_width});
        m_usedArea = 0;
    }

    // Fraction of the bin covered by placed rectangles, in [0,1].
    float occupancy() const {
        const long total = static_cast<long>(m_width) * static_cast<long>(m_height);
        return total > 0 ? static_cast<float>(static_cast<double>(m_usedArea) / static_cast<double>(total))
                         : 0.0f;
    }

    // Place one rectangle. Returns placed=false (and leaves the bin unchanged) if it does not fit.
    Placement insert(int w, int h) {
        Placement result;
        result.w = w;
        result.h = h;
        if (w <= 0 || h <= 0 || w > m_width || h > m_height) {
            return result;
        }

        int bestTop = kIntMax;
        int bestX = 0;
        int bestY = 0;
        int bestIndex = -1;
        for (std::size_t i = 0; i < m_skyline.size(); ++i) {
            int y = 0;
            if (fits(i, w, h, y)) {
                const int top = y + h;
                const int x = m_skyline[i].x;
                if (top < bestTop || (top == bestTop && x < bestX)) {
                    bestTop = top;
                    bestY = y;
                    bestX = x;
                    bestIndex = static_cast<int>(i);
                }
            }
        }
        if (bestIndex < 0) {
            return result; // does not fit
        }

        addLevel(static_cast<std::size_t>(bestIndex), bestX, bestY, w, h);
        m_usedArea += static_cast<long>(w) * static_cast<long>(h);
        result.x = bestX;
        result.y = bestY;
        result.placed = true;
        return result;
    }

    // Pack a batch: insert tallest-first (the standard skyline heuristic) but return placements in the
    // caller's original order so indices line up with the input.
    std::vector<Placement> pack(const std::vector<PackSize>& sizes) {
        std::vector<std::size_t> order(sizes.size());
        for (std::size_t i = 0; i < sizes.size(); ++i) {
            order[i] = i;
        }
        std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
            if (sizes[a].h != sizes[b].h) {
                return sizes[a].h > sizes[b].h; // taller first
            }
            return sizes[a].w > sizes[b].w; // then wider
        });

        std::vector<Placement> out(sizes.size());
        for (std::size_t idx : order) {
            out[idx] = insert(sizes[idx].w, sizes[idx].h);
        }
        return out;
    }

private:
    static constexpr int kIntMax = 0x7FFFFFFF;

    struct Node {
        int x = 0;
        int y = 0;
        int width = 0;
    };

    // Can a w×h rect rest with its left edge at m_skyline[i].x? If so, outY = the base y it would sit at.
    bool fits(std::size_t i, int w, int h, int& outY) const {
        const int x = m_skyline[i].x;
        if (x + w > m_width) {
            return false;
        }
        int widthLeft = w;
        int y = m_skyline[i].y;
        std::size_t j = i;
        while (widthLeft > 0) {
            if (j >= m_skyline.size()) {
                return false;
            }
            y = std::max(y, m_skyline[j].y);
            if (y + h > m_height) {
                return false;
            }
            widthLeft -= m_skyline[j].width;
            ++j;
        }
        outY = y;
        return true;
    }

    // Raise the skyline: a new segment of height y+h spans [x, x+w); trim/merge what it covers.
    void addLevel(std::size_t index, int x, int y, int w, int h) {
        const Node newNode{x, y + h, w};
        m_skyline.insert(m_skyline.begin() + static_cast<std::ptrdiff_t>(index), newNode);

        for (std::size_t i = index + 1; i < m_skyline.size();) {
            const int prevRight = m_skyline[i - 1].x + m_skyline[i - 1].width;
            if (m_skyline[i].x < prevRight) {
                const int shrink = prevRight - m_skyline[i].x;
                m_skyline[i].x += shrink;
                m_skyline[i].width -= shrink;
                if (m_skyline[i].width <= 0) {
                    m_skyline.erase(m_skyline.begin() + static_cast<std::ptrdiff_t>(i));
                    continue; // re-check the new node now at index i
                }
                break;
            }
            break;
        }
        merge();
    }

    // Collapse adjacent segments of equal height.
    void merge() {
        for (std::size_t i = 0; i + 1 < m_skyline.size();) {
            if (m_skyline[i].y == m_skyline[i + 1].y) {
                m_skyline[i].width += m_skyline[i + 1].width;
                m_skyline.erase(m_skyline.begin() + static_cast<std::ptrdiff_t>(i + 1));
            } else {
                ++i;
            }
        }
    }

    int m_width;
    int m_height;
    std::vector<Node> m_skyline;
    long m_usedArea = 0;
};

} // namespace maz::render
