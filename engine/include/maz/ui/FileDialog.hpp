#pragma once

#include <algorithm>
#include <cctype>
#include <functional>
#include <string>
#include <vector>

// maz::ui FileDialog — the model behind Godot's FileDialog control: a file browser with a current
// directory, a filtered/sorted entry list, name filters ("*.png, *.jpg ; Images"), a filename field,
// and a file mode (open one/many, pick a directory, or save). To stay pure and deterministic (and to
// unit-test without touching disk), it takes a *directory lister* callback — the widget/platform layer
// supplies the real filesystem; here it is any function dir -> entries, so tests inject an in-memory
// tree. Path joining/normalisation, hidden-file handling, extension filtering, dirs-before-files
// sorting, single/multi selection, and mode-specific confirmation all live here. Header-only.
namespace maz::ui {

enum class FileDialogMode { OpenFile, OpenFiles, OpenDir, SaveFile };

struct FileEntry {
    std::string name;
    bool isDir = false;
};

class FileDialog {
  public:
    using Lister = std::function<std::vector<FileEntry>(const std::string& dir)>;

    explicit FileDialog(Lister lister, const std::string& startDir = "/")
        : m_lister(std::move(lister)) {
        setCurrentDir(startDir);
    }

    // --- Mode ------------------------------------------------------------------------------
    void setMode(FileDialogMode m) {
        m_mode = m;
        m_selected.clear();
    }
    FileDialogMode mode() const { return m_mode; }

    // --- Filters ("*.png" or "*.png,*.jpg", optional description) ---------------------------
    void addFilter(const std::string& patterns, const std::string& description = "") {
        Filter f;
        f.description = description;
        std::string cur;
        for (char ch : patterns) {
            if (ch == ',') {
                trimPushPattern(f, cur);
                cur.clear();
            } else {
                cur.push_back(ch);
            }
        }
        trimPushPattern(f, cur);
        if (!f.patterns.empty()) {
            m_filters.push_back(std::move(f));
            if (m_currentFilter < 0) {
                m_currentFilter = 0;
            }
            refresh();
        }
    }
    void clearFilters() {
        m_filters.clear();
        m_currentFilter = -1;
        refresh();
    }
    std::size_t filterCount() const { return m_filters.size(); }
    // -1 means "all files"; any other index selects that filter (out-of-range ignored).
    void setCurrentFilter(int idx) {
        if (idx < 0) {
            m_currentFilter = -1;
        } else if (idx < static_cast<int>(m_filters.size())) {
            m_currentFilter = idx;
        } else {
            return;
        }
        refresh();
    }
    int currentFilter() const { return m_currentFilter; }
    const std::string& filterDescription(int idx) const {
        return (idx >= 0 && idx < static_cast<int>(m_filters.size())) ? m_filters[u(idx)].description
                                                                      : m_empty;
    }

    // --- Hidden files ----------------------------------------------------------------------
    void setShowHidden(bool on) {
        m_showHidden = on;
        refresh();
    }
    bool showHidden() const { return m_showHidden; }

    // --- Navigation ------------------------------------------------------------------------
    void setCurrentDir(const std::string& dir) {
        m_dir = normalize(dir);
        m_currentFile.clear();
        m_selected.clear();
        refresh();
    }
    const std::string& currentDir() const { return m_dir; }

    // Descend into a subdirectory of the current dir (must exist and be a directory).
    bool enterDir(const std::string& name) {
        for (const FileEntry& e : m_all) {
            if (e.isDir && e.name == name) {
                setCurrentDir(join(m_dir, name));
                return true;
            }
        }
        return false;
    }
    // Go to the parent directory. Returns false at the root.
    bool goUp() {
        if (m_dir == "/" || m_dir.empty()) {
            return false;
        }
        const std::size_t slash = m_dir.find_last_of('/');
        setCurrentDir(slash == 0 ? "/" : m_dir.substr(0, slash));
        return true;
    }
    void refresh() {
        m_all = m_lister ? m_lister(m_dir) : std::vector<FileEntry>{};
        rebuildVisible();
    }

    // The visible, filtered, sorted entries (dirs first alpha, then files matching the filter).
    const std::vector<FileEntry>& entries() const { return m_visible; }

    // --- Selection -------------------------------------------------------------------------
    // Type/point at a filename. For open modes it must be a visible file; SaveFile accepts any name.
    bool selectFile(const std::string& name) {
        if (m_mode == FileDialogMode::SaveFile) {
            m_currentFile = name;
            return true;
        }
        for (const FileEntry& e : m_visible) {
            if (!e.isDir && e.name == name) {
                m_currentFile = name;
                if (m_mode == FileDialogMode::OpenFiles && !containsSel(name)) {
                    m_selected.push_back(name);
                }
                return true;
            }
        }
        return false;
    }
    // Free-text filename field (SaveFile lets the user type a new name).
    void setCurrentFile(const std::string& name) { m_currentFile = name; }
    const std::string& currentFile() const { return m_currentFile; }
    std::string currentPath() const { return join(m_dir, m_currentFile); }

    // OpenFiles multi-select toggle. Returns the new selected state (false if the name is invalid).
    bool toggleSelect(const std::string& name) {
        for (const FileEntry& e : m_visible) {
            if (!e.isDir && e.name == name) {
                if (containsSel(name)) {
                    m_selected.erase(std::remove(m_selected.begin(), m_selected.end(), name),
                                     m_selected.end());
                    return false;
                }
                m_selected.push_back(name);
                return true;
            }
        }
        return false;
    }
    const std::vector<std::string>& selectedFiles() const { return m_selected; }

    // --- Confirm ---------------------------------------------------------------------------
    // Validate the current selection for the mode; on success fill `out` with the chosen path(s).
    // SaveFile appends the active filter's extension when the typed name has none.
    bool confirm(std::vector<std::string>& out) const {
        out.clear();
        switch (m_mode) {
        case FileDialogMode::OpenDir:
            out.push_back(m_dir);
            return true;
        case FileDialogMode::OpenFile:
            if (isVisibleFile(m_currentFile)) {
                out.push_back(currentPath());
                return true;
            }
            return false;
        case FileDialogMode::OpenFiles:
            if (m_selected.empty()) {
                return false;
            }
            for (const std::string& n : m_selected) {
                if (!isVisibleFile(n)) {
                    out.clear();
                    return false;
                }
                out.push_back(join(m_dir, n));
            }
            return true;
        case FileDialogMode::SaveFile:
            if (m_currentFile.empty()) {
                return false;
            }
            out.push_back(join(m_dir, withDefaultExtension(m_currentFile)));
            return true;
        }
        return false;
    }

  private:
    struct Filter {
        std::vector<std::string> patterns; // e.g. {"*.png","*.jpg"}
        std::string description;
    };

    static std::size_t u(int i) { return static_cast<std::size_t>(i); }

    static void trimPushPattern(Filter& f, std::string s) {
        std::size_t b = 0, e = s.size();
        while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) {
            ++b;
        }
        while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) {
            --e;
        }
        s = s.substr(b, e - b);
        if (!s.empty()) {
            f.patterns.push_back(s);
        }
    }

    static std::string lower(std::string s) {
        for (char& c : s) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return s;
    }

    // Join a directory and a leaf into a normalised path.
    static std::string join(const std::string& dir, const std::string& leaf) {
        if (leaf.empty()) {
            return dir;
        }
        if (dir.empty() || dir == "/") {
            return "/" + leaf;
        }
        return dir + "/" + leaf;
    }

    // Collapse duplicate slashes and strip a trailing slash (except the root).
    static std::string normalize(const std::string& p) {
        if (p.empty()) {
            return "/";
        }
        std::string out;
        out.reserve(p.size());
        bool prevSlash = false;
        for (char ch : p) {
            if (ch == '/') {
                if (!prevSlash) {
                    out.push_back('/');
                }
                prevSlash = true;
            } else {
                out.push_back(ch);
                prevSlash = false;
            }
        }
        if (out.size() > 1 && out.back() == '/') {
            out.pop_back();
        }
        if (out.empty()) {
            out = "/";
        }
        return out;
    }

    bool matchesFilter(const std::string& name) const {
        if (m_currentFilter < 0 || m_currentFilter >= static_cast<int>(m_filters.size())) {
            return true; // all files
        }
        const std::string ln = lower(name);
        for (const std::string& pat : m_filters[u(m_currentFilter)].patterns) {
            if (pat == "*" || pat == "*.*") {
                return true;
            }
            if (pat.size() >= 2 && pat[0] == '*' && pat[1] == '.') {
                const std::string suffix = lower(pat.substr(1)); // ".png"
                if (ln.size() >= suffix.size() &&
                    ln.compare(ln.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    return true;
                }
            } else if (ln == lower(pat)) {
                return true; // exact-name filter
            }
        }
        return false;
    }

    void rebuildVisible() {
        m_visible.clear();
        for (const FileEntry& e : m_all) {
            if (e.name == "." || e.name == "..") {
                continue;
            }
            if (!m_showHidden && !e.name.empty() && e.name[0] == '.') {
                continue;
            }
            if (e.isDir) {
                m_visible.push_back(e); // directories always shown
            } else if (matchesFilter(e.name)) {
                m_visible.push_back(e);
            }
        }
        std::sort(m_visible.begin(), m_visible.end(), [](const FileEntry& a, const FileEntry& b) {
            if (a.isDir != b.isDir) {
                return a.isDir; // dirs first
            }
            return a.name < b.name;
        });
        // Drop any multi-selection entries that are no longer visible files.
        m_selected.erase(std::remove_if(m_selected.begin(), m_selected.end(),
                                        [&](const std::string& n) { return !isVisibleFile(n); }),
                         m_selected.end());
    }

    bool isVisibleFile(const std::string& name) const {
        if (name.empty()) {
            return false;
        }
        for (const FileEntry& e : m_visible) {
            if (!e.isDir && e.name == name) {
                return true;
            }
        }
        return false;
    }

    bool containsSel(const std::string& name) const {
        return std::find(m_selected.begin(), m_selected.end(), name) != m_selected.end();
    }

    // If `name` has no extension and the active filter has a concrete "*.ext", append ".ext".
    std::string withDefaultExtension(const std::string& name) const {
        if (name.find('.') != std::string::npos) {
            return name; // already has an extension
        }
        if (m_currentFilter < 0 || m_currentFilter >= static_cast<int>(m_filters.size())) {
            return name;
        }
        for (const std::string& pat : m_filters[u(m_currentFilter)].patterns) {
            if (pat.size() >= 3 && pat[0] == '*' && pat[1] == '.' &&
                pat.find('*', 2) == std::string::npos) {
                return name + pat.substr(1); // "file" + ".png"
            }
        }
        return name;
    }

    Lister m_lister;
    FileDialogMode m_mode = FileDialogMode::OpenFile;
    std::string m_dir = "/";
    std::string m_currentFile;
    std::vector<Filter> m_filters;
    int m_currentFilter = -1;
    bool m_showHidden = false;
    std::vector<FileEntry> m_all;     // raw listing of m_dir
    std::vector<FileEntry> m_visible; // filtered + sorted
    std::vector<std::string> m_selected;
    std::string m_empty;
};

} // namespace maz::ui
