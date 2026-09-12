#pragma once

#include <string>
#include <vector>

// maz::core NodePath — Godot's NodePath type: a parsed reference to a node (and optionally a
// property chain) in the scene tree, e.g. "../Enemies/Boss:health:x". Godot stores this as three
// pieces: an absolute flag (a leading "/"), a list of NAME components (split on "/", where "."/".."
// mean current/parent), and a list of SUBNAMES (the ":"-separated tail, addressing a property and
// sub-properties). AnimationPlayer tracks, get_node paths, and Tween property targets are all
// NodePaths. This mirrors Godot's parsing and its accessors (get_name_count/get_name,
// get_subname_count/get_subname, is_absolute, get_concatenated_names/subnames, is_empty) and
// reconstructs the same string. Header-only, pure, deterministic; unit-tested by round-trip.
namespace maz::core {

class NodePath {
public:
    NodePath() = default;
    NodePath(const std::string& path) { parse(path); }
    NodePath(const char* path) { parse(std::string(path)); }

    bool isAbsolute() const { return m_absolute; }
    bool isEmpty() const { return m_names.empty() && m_subnames.empty() && !m_absolute; }

    std::size_t getNameCount() const { return m_names.size(); }
    const std::string& getName(std::size_t i) const { return m_names[i]; }
    const std::vector<std::string>& names() const { return m_names; }

    std::size_t getSubnameCount() const { return m_subnames.size(); }
    const std::string& getSubname(std::size_t i) const { return m_subnames[i]; }
    const std::vector<std::string>& subnames() const { return m_subnames; }

    // Names joined with "/" (Godot's get_concatenated_names).
    std::string getConcatenatedNames() const { return join(m_names, '/'); }
    // Subnames joined with ":" (Godot's get_concatenated_subnames).
    std::string getConcatenatedSubnames() const { return join(m_subnames, ':'); }

    // Reconstruct the canonical path string (Godot's operator String).
    std::string toString() const {
        std::string out;
        if (m_absolute) {
            out.push_back('/');
        }
        out += join(m_names, '/');
        if (!m_subnames.empty()) {
            out.push_back(':');
            out += join(m_subnames, ':');
        }
        return out;
    }

    bool operator==(const NodePath& o) const {
        return m_absolute == o.m_absolute && m_names == o.m_names && m_subnames == o.m_subnames;
    }
    bool operator!=(const NodePath& o) const { return !(*this == o); }

private:
    static std::string join(const std::vector<std::string>& parts, char sep) {
        std::string out;
        for (std::size_t i = 0; i < parts.size(); ++i) {
            if (i) {
                out.push_back(sep);
            }
            out += parts[i];
        }
        return out;
    }

    void parse(const std::string& path) {
        m_absolute = false;
        m_names.clear();
        m_subnames.clear();
        if (path.empty()) {
            return;
        }
        // Everything up to the first ':' is the node path; the rest are ':'-separated subnames.
        const std::size_t colon = path.find(':');
        const std::string namePart = colon == std::string::npos ? path : path.substr(0, colon);
        // Names: strip a single leading '/', then split on '/' (dropping empty tokens).
        std::size_t start = 0;
        if (!namePart.empty() && namePart[0] == '/') {
            m_absolute = true;
            start = 1;
        }
        splitInto(namePart, start, '/', m_names);
        // Subnames: split the tail after the first ':' on ':'.
        if (colon != std::string::npos) {
            splitInto(path, colon + 1, ':', m_subnames);
        }
    }

    // Append non-empty '/sep'-delimited tokens of s[from..] into out.
    static void splitInto(const std::string& s, std::size_t from, char sep,
                          std::vector<std::string>& out) {
        std::string cur;
        for (std::size_t i = from; i < s.size(); ++i) {
            if (s[i] == sep) {
                if (!cur.empty()) {
                    out.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(s[i]);
            }
        }
        if (!cur.empty()) {
            out.push_back(cur);
        }
    }

    bool m_absolute = false;
    std::vector<std::string> m_names;
    std::vector<std::string> m_subnames;
};

} // namespace maz::core
