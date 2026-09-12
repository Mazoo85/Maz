#pragma once

#include <cstddef>
#include <string>
#include <vector>

// maz::core::GapBuffer — the classic text-editor data structure: a character buffer with a movable "gap"
// (a run of empty slots) sitting at the cursor. Typing fills the gap and deleting widens it, so edits AT
// THE CURSOR are O(1) amortised — no shifting the whole document on every keystroke, which a plain
// std::string insert/erase would do (O(n) each). Moving the cursor pays only for the distance moved, which
// matches how people edit (many keystrokes in one place, occasional jumps). This is the buffer behind a
// real code/text editor, and directly useful for the engine's in-editor script editor, the developer
// console line, and a chat/input field. Header-only, std-only, deterministic. Godot's TextEdit is a heavy
// node; this is the lightweight algorithmic core.
namespace maz::core {

class GapBuffer {
public:
    GapBuffer() = default;
    explicit GapBuffer(const std::string& initial) { insert(initial); }

    // Number of characters in the document (excludes the gap).
    std::size_t size() const { return m_buf.size() - (m_gapEnd - m_gapStart); }
    bool empty() const { return size() == 0; }
    // Cursor position in document coordinates (where the next insert lands).
    std::size_t cursor() const { return m_gapStart; }

    // Insert a single character at the cursor; the cursor advances past it.
    void insert(char c) {
        ensureGap(1);
        m_buf[m_gapStart++] = c;
    }
    // Insert a string at the cursor.
    void insert(const std::string& s) {
        ensureGap(s.size());
        for (char c : s) {
            m_buf[m_gapStart++] = c;
        }
    }

    // Delete the character BEFORE the cursor (Backspace). Returns false at the start.
    bool backspace() {
        if (m_gapStart == 0) {
            return false;
        }
        --m_gapStart;
        return true;
    }
    // Delete the character AT the cursor (Delete/forward). Returns false at the end.
    bool deleteForward() {
        if (m_gapEnd == m_buf.size()) {
            return false;
        }
        ++m_gapEnd;
        return true;
    }

    // Move the cursor to an absolute document position (clamped to [0, size()]).
    void moveTo(std::size_t pos) {
        const std::size_t n = size();
        if (pos > n) {
            pos = n;
        }
        if (pos < m_gapStart) {
            const std::size_t count = m_gapStart - pos; // shift text after the gap
            for (std::size_t i = 0; i < count; ++i) {
                m_buf[m_gapEnd - 1 - i] = m_buf[m_gapStart - 1 - i];
            }
            m_gapStart = pos;
            m_gapEnd -= count;
        } else if (pos > m_gapStart) {
            const std::size_t count = pos - m_gapStart;
            for (std::size_t i = 0; i < count; ++i) {
                m_buf[m_gapStart + i] = m_buf[m_gapEnd + i];
            }
            m_gapStart += count;
            m_gapEnd += count;
        }
    }
    void moveLeft() {
        if (m_gapStart > 0) {
            moveTo(m_gapStart - 1);
        }
    }
    void moveRight() { moveTo(m_gapStart + 1); }

    // Character at a document index (0 for out of range).
    char at(std::size_t i) const {
        if (i >= size()) {
            return '\0';
        }
        return i < m_gapStart ? m_buf[i] : m_buf[i + (m_gapEnd - m_gapStart)];
    }

    // The full document text.
    std::string text() const {
        std::string out;
        out.reserve(size());
        out.append(m_buf.begin(), m_buf.begin() + static_cast<std::ptrdiff_t>(m_gapStart));
        out.append(m_buf.begin() + static_cast<std::ptrdiff_t>(m_gapEnd), m_buf.end());
        return out;
    }

    void clear() {
        m_buf.clear();
        m_gapStart = 0;
        m_gapEnd = 0;
    }

private:
    // Ensure the gap holds at least n free slots, growing (and re-centring the gap at the cursor) if needed.
    void ensureGap(std::size_t n) {
        if (m_gapEnd - m_gapStart >= n) {
            return;
        }
        const std::size_t textLen = size();
        std::size_t newCap = m_buf.empty() ? 16 : m_buf.size() * 2;
        while (newCap - textLen < n) {
            newCap *= 2;
        }
        std::vector<char> nb(newCap);
        for (std::size_t i = 0; i < m_gapStart; ++i) {
            nb[i] = m_buf[i];
        }
        const std::size_t tail = m_buf.size() - m_gapEnd;
        for (std::size_t i = 0; i < tail; ++i) {
            nb[newCap - tail + i] = m_buf[m_gapEnd + i];
        }
        m_gapEnd = newCap - tail;
        m_buf.swap(nb);
    }

    std::vector<char> m_buf;      // capacity buffer; [m_gapStart, m_gapEnd) is the gap
    std::size_t m_gapStart = 0;
    std::size_t m_gapEnd = 0;
};

} // namespace maz::core
