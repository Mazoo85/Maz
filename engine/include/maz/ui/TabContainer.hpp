#pragma once

#include <cstddef>
#include <string>
#include <vector>

// maz::ui TabContainer — Godot's TabContainer control: a container that holds several content panels
// but shows only one at a time, with an integrated strip of tabs to switch between them. Where TabBar
// (in Controls.hpp) is just the strip, a TabContainer also owns the *content* — each tab carries an
// opaque content id (the widget layer maps it to the panel/Control to display) and the container
// guarantees exactly one selectable tab is "current" (or none, when empty/all-unselectable). It adds
// hidden tabs (not shown in the strip, distinct from disabled tabs which are shown but greyed) and
// keeps `current` on a selectable tab across disable/hide/remove. Pure, header-only, deterministic.
namespace maz::ui {

class TabContainer {
  public:
    struct Tab {
        std::string title;
        int content = -1;      // opaque id of the panel this tab shows
        bool disabled = false; // shown in the strip but not selectable
        bool hidden = false;   // not shown in the strip at all
    };

    // Append a tab. The first *selectable* tab added becomes current.
    int addTab(const std::string& title, int content = -1) {
        m_tabs.push_back(Tab{title, content, false, false});
        if (m_current < 0) {
            m_current = reselect(static_cast<int>(m_tabs.size()) - 1);
        }
        return static_cast<int>(m_tabs.size()) - 1;
    }

    std::size_t tabCount() const { return m_tabs.size(); }
    const Tab* tab(int idx) const { return inRange(idx) ? &m_tabs[u(idx)] : nullptr; }

    const std::string& title(int idx) const {
        return inRange(idx) ? m_tabs[u(idx)].title : m_empty;
    }
    void setTitle(int idx, const std::string& t) {
        if (inRange(idx)) {
            m_tabs[u(idx)].title = t;
        }
    }

    int content(int idx) const { return inRange(idx) ? m_tabs[u(idx)].content : -1; }
    void setContent(int idx, int c) {
        if (inRange(idx)) {
            m_tabs[u(idx)].content = c;
        }
    }

    // The content id of the current tab, or -1 when there is no current tab.
    int currentContent() const { return m_current >= 0 ? m_tabs[u(m_current)].content : -1; }

    bool isSelectable(int idx) const {
        return inRange(idx) && !m_tabs[u(idx)].disabled && !m_tabs[u(idx)].hidden;
    }

    // --- disabled / hidden state (current is re-pointed to a selectable tab if it slips off) -----
    void setTabDisabled(int idx, bool disabled) {
        if (!inRange(idx)) {
            return;
        }
        m_tabs[u(idx)].disabled = disabled;
        if (idx == m_current && !isSelectable(idx)) {
            m_current = reselect(idx);
        } else if (m_current < 0) {
            m_current = reselect(idx);
        }
    }
    void setTabHidden(int idx, bool hidden) {
        if (!inRange(idx)) {
            return;
        }
        m_tabs[u(idx)].hidden = hidden;
        if (idx == m_current && !isSelectable(idx)) {
            m_current = reselect(idx);
        } else if (m_current < 0) {
            m_current = reselect(idx);
        }
    }

    // Remove a tab, keeping `current` on a valid selectable tab (or -1 when none remain).
    bool removeTab(int idx) {
        if (!inRange(idx)) {
            return false;
        }
        const int prev = m_current;
        m_tabs.erase(m_tabs.begin() + static_cast<std::ptrdiff_t>(idx));
        if (m_tabs.empty()) {
            m_current = -1;
            return true;
        }
        // Recompute current: shift index if a tab before it was removed, then ensure selectable.
        int want = prev;
        if (idx < prev) {
            want = prev - 1;
        } else if (idx == prev) {
            want = idx; // the tab that shifted into this slot
        }
        if (want >= static_cast<int>(m_tabs.size())) {
            want = static_cast<int>(m_tabs.size()) - 1;
        }
        m_current = reselect(want < 0 ? 0 : want);
        return true;
    }

    int currentTab() const { return m_current; }
    bool setCurrentTab(int idx) {
        if (!isSelectable(idx)) {
            return false;
        }
        m_current = idx;
        return true;
    }

    // Move to the next/previous SELECTABLE tab (skips disabled + hidden; no wrap).
    bool selectNext() { return step(+1); }
    bool selectPrevious() { return step(-1); }

    // Number of tabs visible in the strip (i.e. not hidden).
    std::size_t visibleTabCount() const {
        std::size_t n = 0;
        for (const Tab& t : m_tabs) {
            if (!t.hidden) {
                ++n;
            }
        }
        return n;
    }

  private:
    static std::size_t u(int i) { return static_cast<std::size_t>(i); }
    bool inRange(int idx) const { return idx >= 0 && idx < static_cast<int>(m_tabs.size()); }

    // Pick `preferred` if selectable, else the nearest selectable tab scanning outward (forward
    // first, then backward), else -1.
    int reselect(int preferred) const {
        if (isSelectable(preferred)) {
            return preferred;
        }
        const int n = static_cast<int>(m_tabs.size());
        for (int i = preferred + 1; i < n; ++i) {
            if (isSelectable(i)) {
                return i;
            }
        }
        for (int i = preferred - 1; i >= 0; --i) {
            if (isSelectable(i)) {
                return i;
            }
        }
        return -1;
    }

    bool step(int dir) {
        const int n = static_cast<int>(m_tabs.size());
        for (int i = m_current + dir; i >= 0 && i < n; i += dir) {
            if (isSelectable(i)) {
                m_current = i;
                return true;
            }
        }
        return false;
    }

    std::vector<Tab> m_tabs;
    int m_current = -1;
    std::string m_empty;
};

} // namespace maz::ui
