#pragma once

#include "maz/ui/Range.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// maz::ui small stateful controls — the selection/value logic behind Godot's SpinBox, OptionButton, and
// TabBar. These are pure state models (no rendering): a SpinBox is a Range with step buttons + text
// format/parse (prefix/suffix), an OptionButton is a drop-down list with a selected item (text + id +
// disabled), and a TabBar is an ordered tab strip with a current tab and disabled-skipping navigation.
// Header-only + deterministic, so the behaviour unit-tests exactly; a widget layer draws them.
namespace maz::ui {

// SpinBox — a numeric field with up/down step buttons (Godot SpinBox, a Range + LineEdit).
class SpinBox {
  public:
    Range range;              // value/min/max/step live here
    std::string prefix;       // shown before the number (e.g. "$")
    std::string suffix;       // shown after (e.g. " px")

    double value() const { return range.value(); }
    void setValue(double v) { range.setValue(v); }

    // One step up/down (uses range.step, or 1 when continuous).
    void increment() { nudge(+1.0); }
    void decrement() { nudge(-1.0); }

    // The displayed string: prefix + number + suffix.
    std::string text() const { return prefix + formatNumber(range.value()) + suffix; }

    // Parse a user-typed string into the value (prefix/suffix stripped if present, then clamped/snapped).
    // Returns false if no number could be read (value left unchanged).
    bool setText(const std::string& s) {
        std::string t = s;
        if (!prefix.empty() && t.compare(0, prefix.size(), prefix) == 0) {
            t = t.substr(prefix.size());
        }
        if (!suffix.empty() && t.size() >= suffix.size() &&
            t.compare(t.size() - suffix.size(), suffix.size(), suffix) == 0) {
            t = t.substr(0, t.size() - suffix.size());
        }
        // trim spaces
        std::size_t a = 0, b = t.size();
        while (a < b && (t[a] == ' ' || t[a] == '\t')) ++a;
        while (b > a && (t[b - 1] == ' ' || t[b - 1] == '\t')) --b;
        t = t.substr(a, b - a);
        if (t.empty()) {
            return false;
        }
        char* end = nullptr;
        const double v = std::strtod(t.c_str(), &end);
        if (end == t.c_str()) {
            return false; // not a number
        }
        range.setValue(v);
        return true;
    }

  private:
    void nudge(double dir) {
        const double d = range.step > 0.0 ? range.step : 1.0;
        range.setValue(range.value() + d * dir);
    }
    static std::string formatNumber(double v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%g", v);
        return std::string(buf);
    }
};

// OptionButton — a drop-down that shows the selected item (Godot OptionButton).
class OptionButton {
  public:
    struct Item {
        std::string text;
        int id = -1;
        bool disabled = false;
    };

    // Add an item; if id < 0 it defaults to the item's index. Selecting the first item happens
    // automatically (Godot selects index 0 on the first add).
    int addItem(const std::string& text, int id = -1) {
        Item it;
        it.text = text;
        it.id = id >= 0 ? id : static_cast<int>(m_items.size());
        m_items.push_back(it);
        if (m_selected < 0) {
            m_selected = 0;
        }
        return static_cast<int>(m_items.size()) - 1;
    }

    std::size_t itemCount() const { return m_items.size(); }
    const Item* item(int idx) const {
        return (idx >= 0 && idx < static_cast<int>(m_items.size()))
                   ? &m_items[static_cast<std::size_t>(idx)]
                   : nullptr;
    }
    void setItemDisabled(int idx, bool disabled) {
        if (idx >= 0 && idx < static_cast<int>(m_items.size())) {
            m_items[static_cast<std::size_t>(idx)].disabled = disabled;
        }
    }

    // Select by index. A disabled or out-of-range index is ignored (returns false).
    bool select(int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_items.size()) ||
            m_items[static_cast<std::size_t>(idx)].disabled) {
            return false;
        }
        m_selected = idx;
        return true;
    }
    // Select the item whose id matches. Returns false if not found or disabled.
    bool selectById(int id) {
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            if (m_items[i].id == id) {
                return select(static_cast<int>(i));
            }
        }
        return false;
    }

    int selected() const { return m_selected; }
    int selectedId() const {
        const Item* it = item(m_selected);
        return it ? it->id : -1;
    }
    std::string selectedText() const {
        const Item* it = item(m_selected);
        return it ? it->text : std::string();
    }
    void clear() {
        m_items.clear();
        m_selected = -1;
    }

  private:
    std::vector<Item> m_items;
    int m_selected = -1;
};

// TabBar — an ordered strip of tabs with a current selection (Godot TabBar).
class TabBar {
  public:
    struct Tab {
        std::string title;
        bool disabled = false;
    };

    int addTab(const std::string& title) {
        m_tabs.push_back(Tab{title, false});
        if (m_current < 0) {
            m_current = 0;
        }
        return static_cast<int>(m_tabs.size()) - 1;
    }
    std::size_t tabCount() const { return m_tabs.size(); }
    const Tab* tab(int idx) const {
        return (idx >= 0 && idx < static_cast<int>(m_tabs.size()))
                   ? &m_tabs[static_cast<std::size_t>(idx)]
                   : nullptr;
    }
    void setTabDisabled(int idx, bool disabled) {
        if (idx >= 0 && idx < static_cast<int>(m_tabs.size())) {
            m_tabs[static_cast<std::size_t>(idx)].disabled = disabled;
        }
    }

    // Remove a tab; keeps `current` pointing at a valid tab (clamped), or -1 when none remain.
    bool removeTab(int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_tabs.size())) {
            return false;
        }
        m_tabs.erase(m_tabs.begin() + static_cast<std::ptrdiff_t>(idx));
        if (m_tabs.empty()) {
            m_current = -1;
        } else if (m_current >= static_cast<int>(m_tabs.size())) {
            m_current = static_cast<int>(m_tabs.size()) - 1;
        }
        return true;
    }

    int current() const { return m_current; }
    bool setCurrent(int idx) {
        if (idx < 0 || idx >= static_cast<int>(m_tabs.size()) ||
            m_tabs[static_cast<std::size_t>(idx)].disabled) {
            return false;
        }
        m_current = idx;
        return true;
    }
    // Move to the next/previous ENABLED tab (no wrap). Returns false if none is available.
    bool selectNext() { return stepTo(+1); }
    bool selectPrevious() { return stepTo(-1); }

  private:
    bool stepTo(int dir) {
        for (int i = m_current + dir; i >= 0 && i < static_cast<int>(m_tabs.size()); i += dir) {
            if (!m_tabs[static_cast<std::size_t>(i)].disabled) {
                m_current = i;
                return true;
            }
        }
        return false;
    }

    std::vector<Tab> m_tabs;
    int m_current = -1;
};

} // namespace maz::ui
