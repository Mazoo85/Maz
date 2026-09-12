#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace maz::ui {

// Single-line editable text — the model behind Godot's LineEdit. Pure logic: no rendering, no input
// polling, so it unit-tests headlessly. A UI widget draws text() and a caret at position caret();
// typed characters come in via insert() and the editing keys drive the caret/erase ops. ASCII/byte
// caret (one byte == one column), which is what the bundled font renders.
class TextField {
public:
    TextField() = default;
    explicit TextField(std::string initial) : m_text(std::move(initial)) { m_caret = m_text.size(); }

    const std::string& text() const { return m_text; }
    std::size_t caret() const { return m_caret; }

    void setText(std::string t) {
        m_text = std::move(t);
        if (m_caret > m_text.size()) {
            m_caret = m_text.size();
        }
    }
    // 0 = unlimited. Truncates the current text if it is already longer.
    void setMaxLength(std::size_t n) {
        m_maxLen = n;
        if (n != 0 && m_text.size() > n) {
            m_text.resize(n);
            if (m_caret > m_text.size()) m_caret = m_text.size();
        }
    }
    std::size_t maxLength() const { return m_maxLen; }

    // Insert a single printable character at the caret. Control chars and over-length inserts are ignored.
    void insert(char c) {
        const unsigned char uc = static_cast<unsigned char>(c);
        if (uc < 0x20 || uc == 0x7f) {
            return;
        }
        if (m_maxLen != 0 && m_text.size() >= m_maxLen) {
            return;
        }
        m_text.insert(m_text.begin() + static_cast<std::ptrdiff_t>(m_caret), c);
        ++m_caret;
    }
    // Insert a run of characters (each filtered/limited as by insert(char)).
    void insert(const char* s) {
        for (; s != nullptr && *s != '\0'; ++s) {
            insert(*s);
        }
    }

    void backspace() { // erase the char before the caret
        if (m_caret > 0) {
            m_text.erase(m_text.begin() + static_cast<std::ptrdiff_t>(m_caret - 1));
            --m_caret;
        }
    }
    void del() { // erase the char at the caret
        if (m_caret < m_text.size()) {
            m_text.erase(m_text.begin() + static_cast<std::ptrdiff_t>(m_caret));
        }
    }
    void moveLeft() {
        if (m_caret > 0) --m_caret;
    }
    void moveRight() {
        if (m_caret < m_text.size()) ++m_caret;
    }
    void home() { m_caret = 0; }
    void end() { m_caret = m_text.size(); }
    void clear() {
        m_text.clear();
        m_caret = 0;
    }

private:
    std::string m_text;
    std::size_t m_caret = 0;
    std::size_t m_maxLen = 0;
};

// Keyboard focus over an ordered set of widget ids — Godot's focus_next / focus_previous + Tab
// traversal. Register ids in tab order; next()/prev() cycle with wraparound; focus(id) jumps to a
// specific widget. Pure logic. kNone is returned when the chain is empty.
class FocusChain {
public:
    static constexpr uint32_t kNone = 0xffffffffu;

    void add(uint32_t id) {
        m_ids.push_back(id);
        if (m_index < 0) {
            m_index = 0;
        }
    }
    void clear() {
        m_ids.clear();
        m_index = -1;
    }
    std::size_t size() const { return m_ids.size(); }

    uint32_t focused() const {
        return m_index >= 0 ? m_ids[static_cast<std::size_t>(m_index)] : kNone;
    }
    bool isFocused(uint32_t id) const { return focused() == id; }

    void next() {
        if (!m_ids.empty()) {
            m_index = static_cast<int>((static_cast<std::size_t>(m_index) + 1) % m_ids.size());
        }
    }
    void prev() {
        if (!m_ids.empty()) {
            const std::size_t n = m_ids.size();
            m_index = static_cast<int>((static_cast<std::size_t>(m_index) + n - 1) % n);
        }
    }
    // Focus the widget with `id` if it is registered; otherwise leave focus unchanged.
    void focus(uint32_t id) {
        for (std::size_t i = 0; i < m_ids.size(); ++i) {
            if (m_ids[i] == id) {
                m_index = static_cast<int>(i);
                return;
            }
        }
    }

private:
    std::vector<uint32_t> m_ids;
    int m_index = -1; // -1 = nothing focused
};

// Per-frame editing input for a focused text field: characters typed this frame plus the editing keys.
// The caller fills this from the platform (e.g. mapping scancodes to characters); the Context applies
// it to whichever field owns focus.
struct TextEditInput {
    const char* typed = nullptr; // null-terminated run of characters typed this frame (may be null)
    bool backspace = false;
    bool del = false;
    bool left = false;
    bool right = false;
    bool home = false;
    bool end = false;
};

} // namespace maz::ui
