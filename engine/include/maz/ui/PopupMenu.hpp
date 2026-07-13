#pragma once

#include "maz/math/Math.hpp"
#include "maz/ui/Rect.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace maz::ui {

// PopupMenu — Godot's PopupMenu: the vertical list of items behind right-click context menus, OptionButton
// dropdowns, and menu bars. Each item is a label with a caller id and optional check state (a checkbox or a
// radio button), a disabled flag, an accelerator/shortcut hint, or a submenu arrow; a `separator` item draws a
// thin divider and is never selectable. The menu stacks items from its `position` at a fixed row height, so it
// exposes the geometry a view + input need: `rect()` / `itemRect(i)` for drawing, `itemAtPoint()` to hit-test
// the cursor (rejecting separators and outside points), `hoverNext`/`hoverPrev` for keyboard navigation
// (skipping separators + disabled rows), and `activate()` to fire the hovered item — toggling a checkbox,
// switching a radio group, and returning the item id. Pure logic, header-only, deterministic — it unit-tests
// exactly and drives a golden (an open menu).

enum class MenuCheck { None, CheckBox, Radio };

struct MenuItem {
    std::string text;
    int id = -1;
    MenuCheck check = MenuCheck::None;
    bool checked = false;
    bool disabled = false;
    bool separator = false;
    bool submenu = false;    // draws a right-pointing arrow
    std::string shortcut;    // accelerator hint, right-aligned (e.g. "Ctrl+S")
};

class PopupMenu {
public:
    math::vec2 position{0.0f, 0.0f};
    float width = 220.0f;
    float itemHeight = 28.0f;
    float separatorHeight = 9.0f;

    // --- building ---
    std::size_t addItem(std::string text, int id = -1) { return push(makeItem(std::move(text), id)); }
    std::size_t addCheckItem(std::string text, int id = -1) {
        MenuItem it = makeItem(std::move(text), id);
        it.check = MenuCheck::CheckBox;
        return push(it);
    }
    std::size_t addRadioItem(std::string text, int id = -1) {
        MenuItem it = makeItem(std::move(text), id);
        it.check = MenuCheck::Radio;
        return push(it);
    }
    std::size_t addSubmenuItem(std::string text, int id = -1) {
        MenuItem it = makeItem(std::move(text), id);
        it.submenu = true;
        return push(it);
    }
    void addSeparator() {
        MenuItem it;
        it.separator = true;
        push(it);
    }

    std::size_t count() const { return m_items.size(); }
    const MenuItem& item(std::size_t i) const { return m_items[i]; }
    MenuItem& item(std::size_t i) { return m_items[i]; }

    void setChecked(std::size_t i, bool c) {
        if (i < m_items.size()) {
            m_items[i].checked = c;
        }
    }
    bool isChecked(std::size_t i) const { return i < m_items.size() && m_items[i].checked; }
    void setDisabled(std::size_t i, bool d) {
        if (i < m_items.size()) {
            m_items[i].disabled = d;
        }
    }
    void setShortcut(std::size_t i, std::string s) {
        if (i < m_items.size()) {
            m_items[i].shortcut = std::move(s);
        }
    }

    // Check radio item i and uncheck every other radio item (single-choice group).
    void checkRadio(std::size_t i) {
        if (i >= m_items.size() || m_items[i].check != MenuCheck::Radio) {
            return;
        }
        for (std::size_t j = 0; j < m_items.size(); ++j) {
            if (m_items[j].check == MenuCheck::Radio) {
                m_items[j].checked = (j == i);
            }
        }
    }

    // --- geometry ---
    float rowHeight(std::size_t i) const {
        return (i < m_items.size() && m_items[i].separator) ? separatorHeight : itemHeight;
    }

    float totalHeight() const {
        float h = 0.0f;
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            h += rowHeight(i);
        }
        return h;
    }

    Rect rect() const { return Rect{position.x, position.y, width, totalHeight()}; }

    Rect itemRect(std::size_t i) const {
        float y = position.y;
        for (std::size_t k = 0; k < i && k < m_items.size(); ++k) {
            y += rowHeight(k);
        }
        return Rect{position.x, y, width, rowHeight(i)};
    }

    // Row under a screen point, or -1 if outside the menu, on a separator, or past the last row.
    long itemAtPoint(float px, float py) const {
        if (!rect().contains(px, py)) {
            return -1;
        }
        float y = position.y;
        for (std::size_t i = 0; i < m_items.size(); ++i) {
            const float h = rowHeight(i);
            if (py >= y && py < y + h) {
                return m_items[i].separator ? -1 : static_cast<long>(i);
            }
            y += h;
        }
        return -1;
    }

    // --- hover / activation ---
    long hovered() const { return m_hovered; }
    void setHovered(long i) { m_hovered = i; }
    void clearHover() { m_hovered = -1; }

    long hoverNext() { return step(1); }
    long hoverPrev() { return step(-1); }

    // Fire the hovered item: toggles a checkbox, switches a radio group, and returns its id (-1 if nothing
    // actionable is hovered).
    int activate() {
        if (m_hovered < 0 || static_cast<std::size_t>(m_hovered) >= m_items.size()) {
            return -1;
        }
        MenuItem& it = m_items[static_cast<std::size_t>(m_hovered)];
        if (it.separator || it.disabled) {
            return -1;
        }
        if (it.check == MenuCheck::CheckBox) {
            it.checked = !it.checked;
        } else if (it.check == MenuCheck::Radio) {
            checkRadio(static_cast<std::size_t>(m_hovered));
        }
        return it.id;
    }

private:
    static MenuItem makeItem(std::string text, int id) {
        MenuItem it;
        it.text = std::move(text);
        it.id = id;
        return it;
    }
    std::size_t push(const MenuItem& it) {
        m_items.push_back(it);
        return m_items.size() - 1;
    }

    bool selectable(std::size_t i) const {
        return i < m_items.size() && !m_items[i].separator && !m_items[i].disabled;
    }

    // Move hover by `dir`, skipping separators + disabled, wrapping around. Returns the new hovered index (or
    // the old one if nothing is selectable).
    long step(int dir) {
        if (m_items.empty()) {
            return m_hovered;
        }
        const long n = static_cast<long>(m_items.size());
        long i = m_hovered;
        for (long guard = 0; guard < n; ++guard) {
            i = (i + dir % n + n) % n; // wrap
            if (selectable(static_cast<std::size_t>(i))) {
                m_hovered = i;
                return m_hovered;
            }
        }
        return m_hovered;
    }

    std::vector<MenuItem> m_items;
    long m_hovered = -1;
};

} // namespace maz::ui
