#pragma once

#include <string>
#include <vector>

// maz::io export presets — the data model behind Godot's export presets and the packaging pipeline
// (tools/package.sh): each preset names a target platform, an output path, a set of feature tags the
// build defines, and include/exclude resource filters deciding which files ship. The reusable core is
// a small glob matcher (* = any run, ? = one char) used by the filters. includes(path) applies the
// rules the way Godot does: an exclude match always drops a file; otherwise, an empty include list
// ships everything, and a non-empty one ships only matching files. Pure string logic, no filesystem
// or platform calls, so it unit-tests headlessly; the actual copy/zip step reads these decisions.
namespace maz::io {

// Wildcard match: '*' matches any run (including empty, and '/'), '?' matches exactly one character.
// Linear time with backtracking on the last star — no recursion.
inline bool globMatch(const std::string& pattern, const std::string& text) {
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string::npos;
    std::size_t mark = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            mark = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++mark;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

struct ExportPreset {
    std::string name;
    std::string platform;   // "windows", "linux", "macos", "web", ...
    std::string exportPath; // output file/dir the packager writes to
    std::vector<std::string> features;       // feature tags this build defines
    std::vector<std::string> includeFilters; // globs; empty => include everything not excluded
    std::vector<std::string> excludeFilters; // globs; a match always drops the file

    bool hasFeature(const std::string& f) const {
        for (const std::string& x : features) {
            if (x == f) {
                return true;
            }
        }
        return false;
    }

    // Should this resource path be packaged? Exclude wins; then empty-include => all, else must match.
    bool includes(const std::string& resPath) const {
        for (const std::string& ex : excludeFilters) {
            if (globMatch(ex, resPath)) {
                return false;
            }
        }
        if (includeFilters.empty()) {
            return true;
        }
        for (const std::string& in : includeFilters) {
            if (globMatch(in, resPath)) {
                return true;
            }
        }
        return false;
    }
};

class ExportConfig {
  public:
    void add(const ExportPreset& p) { m_presets.push_back(p); }
    std::size_t count() const { return m_presets.size(); }
    const std::vector<ExportPreset>& presets() const { return m_presets; }

    // First preset with the given name, or nullptr.
    const ExportPreset* find(const std::string& name) const {
        for (const ExportPreset& p : m_presets) {
            if (p.name == name) {
                return &p;
            }
        }
        return nullptr;
    }

    // All presets targeting a platform.
    std::vector<const ExportPreset*> forPlatform(const std::string& platform) const {
        std::vector<const ExportPreset*> out;
        for (const ExportPreset& p : m_presets) {
            if (p.platform == platform) {
                out.push_back(&p);
            }
        }
        return out;
    }

  private:
    std::vector<ExportPreset> m_presets;
};

} // namespace maz::io
