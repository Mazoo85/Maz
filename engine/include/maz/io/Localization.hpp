#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace maz::io {

// CSV parsing + localization — Godot's Translation / CSV import. A shippable game needs its on-screen
// text in more than one language, and the standard authoring format (Godot's included) is a CSV whose
// first column is a message KEY and whose remaining columns are one LOCALE each. Maz could read JSON and
// its own prefab/binary formats but had no CSV reader and no translation lookup at all. This adds a
// robust RFC-4180-style CSV parser (quoted fields, embedded delimiters/newlines, "" escapes, CRLF or LF)
// plus a `TranslationTable` that loads such a CSV and answers tr(key) in the active locale with sensible
// fallback. Pure std, header-only, no engine deps.

// Parse CSV text into rows of fields. Honors: quoted fields ("a,b" keeps the comma), embedded newlines
// inside quotes, "" as an escaped quote, and CRLF or LF line endings. A trailing newline does not emit a
// spurious empty row. An empty input yields no rows.
inline std::vector<std::vector<std::string>> parseCsv(std::string_view text, char delim = ',') {
    std::vector<std::vector<std::string>> rows;
    std::vector<std::string> field;
    std::string cur;
    bool inQuotes = false;
    bool rowHasData = false; // did this line contribute any field/char (so we don't emit a blank tail row)?

    auto endField = [&]() {
        field.push_back(cur);
        cur.clear();
        rowHasData = true;
    };
    auto endRow = [&]() {
        endField();
        rows.push_back(field);
        field.clear();
        rowHasData = false;
    };

    const std::size_t n = text.size();
    for (std::size_t i = 0; i < n; ++i) {
        const char c = text[i];
        if (inQuotes) {
            if (c == '"') {
                if (i + 1 < n && text[i + 1] == '"') { // "" -> literal quote
                    cur.push_back('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                cur.push_back(c);
            }
            continue;
        }
        if (c == '"') {
            inQuotes = true;
            rowHasData = true;
        } else if (c == delim) {
            endField();
        } else if (c == '\n' || c == '\r') {
            // Consume CRLF as a single terminator.
            if (c == '\r' && i + 1 < n && text[i + 1] == '\n') {
                ++i;
            }
            if (rowHasData || !cur.empty() || !field.empty()) {
                endRow();
            }
        } else {
            cur.push_back(c);
            rowHasData = true;
        }
    }
    // Flush a final row that wasn't newline-terminated.
    if (rowHasData || !cur.empty() || !field.empty()) {
        endRow();
    }
    return rows;
}

// A message-key -> per-locale-text table loaded from a Godot-style translation CSV. Row 0 is the header:
// its first cell names the key column (conventionally "keys"), the rest name the locales ("en","es",…).
// Each later row is a key followed by that key's text in each locale.
class TranslationTable {
public:
    // Load from CSV text. Returns false if there isn't at least a header + one locale column. Replaces any
    // previously loaded data. The active locale resets to the first locale.
    bool loadCsv(std::string_view csv) {
        const std::vector<std::vector<std::string>> rows = parseCsv(csv);
        if (rows.empty() || rows[0].size() < 2) {
            return false;
        }
        locales_.assign(rows[0].begin() + 1, rows[0].end());
        keys_.clear();
        table_.clear();
        for (std::size_t r = 1; r < rows.size(); ++r) {
            const std::vector<std::string>& row = rows[r];
            if (row.empty() || row[0].empty()) {
                continue; // skip blank/keyless rows
            }
            std::vector<std::string> cells(locales_.size());
            for (std::size_t c = 0; c < locales_.size(); ++c) {
                cells[c] = (c + 1 < row.size()) ? row[c + 1] : std::string();
            }
            keys_.push_back(row[0]);
            table_.emplace(row[0], std::move(cells));
        }
        activeLocale_ = 0;
        return true;
    }

    // Switch the active locale by name (e.g. "es"). No-op (keeps the current locale) if unknown.
    void setLocale(std::string_view locale) {
        for (std::size_t i = 0; i < locales_.size(); ++i) {
            if (locales_[i] == locale) {
                activeLocale_ = static_cast<int>(i);
                return;
            }
        }
    }

    const std::string& locale() const {
        static const std::string kEmpty;
        return locales_.empty() ? kEmpty : locales_[static_cast<std::size_t>(activeLocale_)];
    }
    const std::vector<std::string>& locales() const { return locales_; }
    const std::vector<std::string>& keys() const { return keys_; }
    std::size_t count() const { return keys_.size(); }
    bool hasKey(std::string_view key) const { return table_.find(std::string(key)) != table_.end(); }

    // Translate `key` in the active locale. If the cell is empty, fall back to the FIRST locale (the
    // source language); if that's empty too — or the key is unknown — return the key itself, so missing
    // translations degrade to a visible identifier rather than blank text.
    std::string tr(std::string_view key) const { return trIn(key, activeLocale_); }

    // Translate `key` in a named locale (falls back like tr()). Unknown locale → source locale.
    std::string tr(std::string_view key, std::string_view locale) const {
        int idx = 0;
        for (std::size_t i = 0; i < locales_.size(); ++i) {
            if (locales_[i] == locale) {
                idx = static_cast<int>(i);
                break;
            }
        }
        return trIn(key, idx);
    }

private:
    std::string trIn(std::string_view key, int localeIdx) const {
        auto it = table_.find(std::string(key));
        if (it == table_.end()) {
            return std::string(key);
        }
        const std::vector<std::string>& cells = it->second;
        const std::size_t li = static_cast<std::size_t>(localeIdx);
        if (li < cells.size() && !cells[li].empty()) {
            return cells[li];
        }
        if (!cells.empty() && !cells[0].empty()) { // fall back to source language
            return cells[0];
        }
        return std::string(key);
    }

    std::vector<std::string> locales_;
    std::vector<std::string> keys_;
    std::unordered_map<std::string, std::vector<std::string>> table_;
    int activeLocale_ = 0;
};

} // namespace maz::io
