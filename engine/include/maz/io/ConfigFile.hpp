#pragma once

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace maz::io {

// ConfigFile — Godot's ConfigFile: an INI-style `[section]` + `key=value` store, the format behind
// project settings, input maps, and hand-editable save/options files. Values are held as raw strings with
// typed accessors (getBool/getInt/getFloat coerce; setBool/setInt/setFloat format), which keeps it
// dependency-free while covering the overwhelmingly common settings-file use. Sections and keys preserve
// INSERTION ORDER so `encode()` produces stable, diff-friendly text that round-trips through `parse()`.
// Keys written before any `[section]` header live in the unnamed global section (Godot allows this).
// Parsing is lenient: blank lines and `;` / `#` comments are skipped, whitespace around keys/values is
// trimmed, and a value wrapped in matching quotes has them stripped. Header-only, deterministic — it
// unit-tests exactly and drives a golden (a rendered settings table + its encoded text).

class ConfigFile {
public:
    void clear() { m_sections.clear(); }

    // --- raw string values ---
    void setValue(const std::string& section, const std::string& key, const std::string& value) {
        ensure(section).set(key, value);
    }
    std::string getValue(const std::string& section, const std::string& key,
                         const std::string& def = std::string()) const {
        const Section* s = find(section);
        if (!s) {
            return def;
        }
        const std::string* v = s->get(key);
        return v ? *v : def;
    }

    // --- typed convenience ---
    void setBool(const std::string& section, const std::string& key, bool v) {
        setValue(section, key, v ? "true" : "false");
    }
    void setInt(const std::string& section, const std::string& key, long long v) {
        setValue(section, key, std::to_string(v));
    }
    void setFloat(const std::string& section, const std::string& key, double v) {
        setValue(section, key, formatFloat(v));
    }

    bool getBool(const std::string& section, const std::string& key, bool def = false) const {
        const Section* s = find(section);
        const std::string* v = s ? s->get(key) : nullptr;
        if (!v) {
            return def;
        }
        return *v == "true" || *v == "1" || *v == "yes" || *v == "on";
    }
    long long getInt(const std::string& section, const std::string& key, long long def = 0) const {
        const Section* s = find(section);
        const std::string* v = s ? s->get(key) : nullptr;
        if (!v || v->empty()) {
            return def;
        }
        return std::strtoll(v->c_str(), nullptr, 10);
    }
    double getFloat(const std::string& section, const std::string& key, double def = 0.0) const {
        const Section* s = find(section);
        const std::string* v = s ? s->get(key) : nullptr;
        if (!v || v->empty()) {
            return def;
        }
        return std::strtod(v->c_str(), nullptr);
    }

    // --- structure queries ---
    bool hasSection(const std::string& section) const { return find(section) != nullptr; }
    bool hasSectionKey(const std::string& section, const std::string& key) const {
        const Section* s = find(section);
        return s && s->get(key) != nullptr;
    }
    void eraseSectionKey(const std::string& section, const std::string& key) {
        Section* s = find(section);
        if (s) {
            s->erase(key);
        }
    }
    void eraseSection(const std::string& section) {
        for (std::size_t i = 0; i < m_sections.size(); ++i) {
            if (m_sections[i].name == section) {
                m_sections.erase(m_sections.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    std::vector<std::string> sections() const {
        std::vector<std::string> out;
        out.reserve(m_sections.size());
        for (const Section& s : m_sections) {
            out.push_back(s.name);
        }
        return out;
    }
    std::vector<std::string> sectionKeys(const std::string& section) const {
        std::vector<std::string> out;
        const Section* s = find(section);
        if (s) {
            out.reserve(s->entries.size());
            for (const KV& e : s->entries) {
                out.push_back(e.key);
            }
        }
        return out;
    }

    // --- serialization ---
    // INI text: the unnamed global section (if any) is written first without a header, then each
    // `[section]` with a trailing blank line.
    std::string encode() const {
        std::string out;
        for (const Section& s : m_sections) {
            if (!s.name.empty()) {
                out += '[';
                out += s.name;
                out += "]\n";
            }
            for (const KV& e : s.entries) {
                out += e.key;
                out += '=';
                out += e.value;
                out += '\n';
            }
            out += '\n';
        }
        return out;
    }

    // Parse INI text, merging into the current contents. Always succeeds (lenient); returns the number of
    // key/value pairs read.
    int parse(const std::string& text) {
        int count = 0;
        std::string current; // global section
        std::size_t i = 0;
        while (i < text.size()) {
            std::size_t end = text.find('\n', i);
            if (end == std::string::npos) {
                end = text.size();
            }
            std::string line = trim(text.substr(i, end - i));
            i = end + 1;
            if (line.empty() || line[0] == ';' || line[0] == '#') {
                continue;
            }
            if (line.front() == '[' && line.back() == ']') {
                current = trim(line.substr(1, line.size() - 2));
                ensure(current); // materialize even an empty section
                continue;
            }
            const std::size_t eq = line.find('=');
            if (eq == std::string::npos) {
                continue; // not a key=value line
            }
            const std::string key = trim(line.substr(0, eq));
            std::string value = stripQuotes(trim(line.substr(eq + 1)));
            if (!key.empty()) {
                ensure(current).set(key, value);
                ++count;
            }
        }
        return count;
    }

private:
    struct KV {
        std::string key;
        std::string value;
    };
    struct Section {
        std::string name;
        std::vector<KV> entries;

        const std::string* get(const std::string& k) const {
            for (const KV& e : entries) {
                if (e.key == k) {
                    return &e.value;
                }
            }
            return nullptr;
        }
        void set(const std::string& k, const std::string& v) {
            for (KV& e : entries) {
                if (e.key == k) {
                    e.value = v;
                    return;
                }
            }
            entries.push_back(KV{k, v});
        }
        void erase(const std::string& k) {
            for (std::size_t i = 0; i < entries.size(); ++i) {
                if (entries[i].key == k) {
                    entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(i));
                    return;
                }
            }
        }
    };

    Section* find(const std::string& name) {
        for (Section& s : m_sections) {
            if (s.name == name) {
                return &s;
            }
        }
        return nullptr;
    }
    const Section* find(const std::string& name) const {
        for (const Section& s : m_sections) {
            if (s.name == name) {
                return &s;
            }
        }
        return nullptr;
    }
    Section& ensure(const std::string& name) {
        if (Section* s = find(name)) {
            return *s;
        }
        m_sections.push_back(Section{name, {}});
        return m_sections.back();
    }

    static std::string trim(const std::string& s) {
        std::size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) {
            ++a;
        }
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) {
            --b;
        }
        return s.substr(a, b - a);
    }
    static std::string stripQuotes(const std::string& s) {
        if (s.size() >= 2 && ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\''))) {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }
    static std::string formatFloat(double v) {
        // Compact, round-trippable-enough representation; trim trailing zeros.
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.6g", v);
        return std::string(buf);
    }

    std::vector<Section> m_sections;
};

} // namespace maz::io
