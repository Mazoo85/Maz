#pragma once

#include "maz/ui/Rect.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace maz::ui {

// ItemList — Godot's ItemList control: a scrollable column of selectable text rows. It backs Godot's
// FileDialog file list, the animation/audio-bus pickers, inventory and dialogue lists, level-select
// menus — anywhere a game shows a bounded box of choosable entries. Each row carries text, a caller id,
// and `selectable`/`disabled` flags. Selection is either Single (picking one clears the rest, like a
// radio group) or Multi (rows toggle independently). The list has a fixed row height + separation and a
// vertical `scroll` offset, so it exposes the geometry a renderer needs: itemRect(i) for a row's pixel
// box, itemAtPoint() to hit-test a click, ensureVisible() / visibleRange() for scrolling. It is pure
// logic — no GPU, no windowing — so it unit-tests deterministically and a view just draws the rows it
// reports.

enum class ItemSelectMode { Single, Multi };

struct ListItem {
    std::string text;
    int id = -1;           // caller payload
    bool selectable = true;
    bool disabled = false; // drawn dimmed; never selectable
    bool selected = false;
};

class ItemList {
public:
    Rect rect{0.0f, 0.0f, 220.0f, 320.0f}; // widget box in screen pixels (origin top-left)
    float itemHeight = 26.0f;              // fixed row height
    float separation = 2.0f;               // gap between rows
    ItemSelectMode selectMode = ItemSelectMode::Single;

    // Append a row; returns its index.
    std::size_t addItem(std::string text, int id = -1) {
        ListItem it;
        it.text = std::move(text);
        it.id = id;
        m_items.push_back(std::move(it));
        return m_items.size() - 1;
    }

    std::size_t count() const { return m_items.size(); }
    const ListItem& item(std::size_t i) const { return m_items[i]; }
    ListItem& item(std::size_t i) { return m_items[i]; }

    void setDisabled(std::size_t i, bool d) {
        if (i < m_items.size()) {
            m_items[i].disabled = d;
        }
    }
    void setSelectable(std::size_t i, bool s) {
        if (i < m_items.size()) {
            m_items[i].selectable = s;
        }
    }

    // --- selection ---

    // Select row i. In Single mode this first clears every other row (radio behaviour); in Multi mode it
    // adds to the set. Disabled or non-selectable rows are ignored. Updates the keyboard `current`.
    void select(std::size_t i) {
        if (i >= m_items.size() || !m_items[i].selectable || m_items[i].disabled) {
            return;
        }
        if (selectMode == ItemSelectMode::Single) {
            for (auto& it : m_items) {
                it.selected = false;
            }
        }
        m_items[i].selected = true;
        m_current = static_cast<long>(i);
    }

    void deselect(std::size_t i) {
        if (i < m_items.size()) {
            m_items[i].selected = false;
        }
    }
    void deselectAll() {
        for (auto& it : m_items) {
            it.selected = false;
        }
    }
    bool isSelected(std::size_t i) const { return i < m_items.size() && m_items[i].selected; }

    // Toggle a row's selection (Multi-mode convenience; honours selectable/disabled).
    void toggle(std::size_t i) {
        if (i >= m_items.size() || !m_items[i].selectable || m_items[i].disabled) {
            return;
        }
        if (m_items[i].selected) {
            m_items[i].selected = false;
        } else {
            select(i);
        }
    }

    std::vector<std::size_t> selectedItems() const {
        std::vector<std::size_t> out;
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i].selected) {
                out.push_back(i);
            }
        }
        return out;
    }

    // First selected index, or -1 when nothing is selected.
    long firstSelected() const {
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i].selected) {
                return static_cast<long>(i);
            }
        }
        return -1;
    }

    long current() const { return m_current; }

    // Keyboard navigation: move `current` to the next / previous selectable, non-disabled row, select it
    // (respecting the mode) and scroll it into view. Stops at the ends (no wrap). Returns the new current.
    long selectNext() { return moveCurrent(1); }
    long selectPrevious() { return moveCurrent(-1); }

    // --- geometry / scrolling ---

    float rowStride() const { return itemHeight + separation; }

    // Total pixel height of all rows (n rows + n-1 separators).
    float contentHeight() const {
        if (m_items.empty()) {
            return 0.0f;
        }
        const float n = static_cast<float>(m_items.size());
        return n * itemHeight + (n - 1.0f) * separation;
    }

    float maxScroll() const { return std::max(0.0f, contentHeight() - rect.h); }
    float scroll() const { return m_scroll; }
    void setScroll(float s) { m_scroll = std::clamp(s, 0.0f, maxScroll()); }

    // Absolute pixel rect of row i (accounts for scroll). Not clipped to the widget box — a renderer
    // clips or skips rows outside `rect`.
    Rect itemRect(std::size_t i) const {
        const float top = rect.y - m_scroll + static_cast<float>(i) * rowStride();
        return Rect{rect.x, top, rect.w, itemHeight};
    }

    // Row index under a screen point, or -1 if the point is outside the widget box, in a separator gap,
    // or past the last row.
    long itemAtPoint(float px, float py) const {
        if (!rect.contains(px, py)) {
            return -1;
        }
        const float local = py - rect.y + m_scroll;
        if (local < 0.0f) {
            return -1;
        }
        const long idx = static_cast<long>(local / rowStride());
        if (idx < 0 || static_cast<std::size_t>(idx) >= m_items.size()) {
            return -1;
        }
        const float within = local - static_cast<float>(idx) * rowStride();
        if (within > itemHeight) {
            return -1; // in the separator gap between rows
        }
        return idx;
    }

    // Adjust scroll minimally so row i is fully visible within the widget box.
    void ensureVisible(std::size_t i) {
        if (i >= m_items.size()) {
            return;
        }
        const float top = static_cast<float>(i) * rowStride();
        const float bottom = top + itemHeight;
        if (top < m_scroll) {
            setScroll(top);
        } else if (bottom > m_scroll + rect.h) {
            setScroll(bottom - rect.h);
        }
    }

    // Inclusive [first, last] range of rows that intersect the widget box, clamped to valid indices. An
    // empty list reports {0, -1} (first > last), so `for (i = first; i <= last; ++i)` is a no-op.
    std::pair<long, long> visibleRange() const {
        if (m_items.empty()) {
            return {0, -1};
        }
        long first = static_cast<long>(std::floor(m_scroll / rowStride()));
        long last = static_cast<long>(std::floor((m_scroll + rect.h) / rowStride()));
        const long lastIdx = static_cast<long>(m_items.size()) - 1;
        first = std::clamp<long>(first, 0, lastIdx);
        last = std::clamp<long>(last, 0, lastIdx);
        return {first, last};
    }

private:
    std::vector<ListItem> m_items;
    float m_scroll = 0.0f;
    long m_current = -1;

    long moveCurrent(int dir) {
        if (m_items.empty()) {
            return m_current;
        }
        long i = m_current;
        for (std::size_t guard = 0; guard < m_items.size(); ++guard) {
            i += dir;
            if (i < 0 || i >= static_cast<long>(m_items.size())) {
                return m_current; // clamp at the ends
            }
            const std::size_t u = static_cast<std::size_t>(i);
            if (m_items[u].selectable && !m_items[u].disabled) {
                select(u);
                ensureVisible(u);
                return m_current;
            }
        }
        return m_current;
    }
};

} // namespace maz::ui
