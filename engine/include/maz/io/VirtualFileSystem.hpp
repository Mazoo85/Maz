#pragma once

#include <string>
#include <unordered_map>
#include <vector>

// maz::io — a virtual filesystem with scheme paths, Maz's answer to Godot's res:// / user:// path
// model. A game shouldn't hard-code absolute OS paths: it refers to assets as
// "res://textures/hero.png" and to saves as "user://save1.dat", and the VFS maps each scheme to a
// real directory chosen at startup (the install dir, the per-user save dir, a mounted mod
// folder...). That indirection is what lets the same game run from a dev tree, an installed bundle,
// or a packed archive without changing a single path in the game code.
//
// The valuable, testable core is pure path logic: normalizePath collapses '.', '..', and duplicate
// slashes; resolve() maps a scheme path to a real path AND refuses any '..' that would escape the
// mount root (so "res://../../etc/passwd" is rejected, not silently followed) — the traversal guard
// a naive string-concat filesystem forgets. The actual byte reading/writing stays in platform code;
// this header is the routing + safety layer, header-only and unit-testable with no disk.
namespace maz::io {

// Collapse '.', '..', and duplicate '/' in a path. Preserves a leading '/' (absolute) and keeps
// leading '..' segments for relative paths (they can't be resolved without a base). "" -> ".".
inline std::string normalizePath(const std::string& p) {
    const bool absolute = !p.empty() && p[0] == '/';
    std::vector<std::string> out;
    size_t i = 0;
    while (i < p.size()) {
        size_t j = p.find('/', i);
        const std::string tok = p.substr(i, j == std::string::npos ? std::string::npos : j - i);
        i = (j == std::string::npos) ? p.size() : j + 1;
        if (tok.empty() || tok == ".") {
            continue;
        }
        if (tok == "..") {
            if (!out.empty() && out.back() != "..") {
                out.pop_back();
            } else if (!absolute) {
                out.push_back(".."); // relative path may legitimately go up
            }
            // absolute + already at root: drop (can't escape above /)
            continue;
        }
        out.push_back(tok);
    }
    std::string res;
    for (size_t k = 0; k < out.size(); ++k) {
        if (k) {
            res += '/';
        }
        res += out[k];
    }
    if (absolute) {
        return "/" + res;
    }
    return res.empty() ? "." : res;
}

// Join two path fragments with a single separator (b, if absolute, replaces a).
inline std::string joinPath(const std::string& a, const std::string& b) {
    if (b.empty()) {
        return a;
    }
    if (a.empty() || (!b.empty() && b[0] == '/')) {
        return b;
    }
    if (a.back() == '/') {
        return a + b;
    }
    return a + "/" + b;
}

// The final path component ("a/b/c.png" -> "c.png").
inline std::string fileName(const std::string& p) {
    const size_t s = p.find_last_of('/');
    return s == std::string::npos ? p : p.substr(s + 1);
}

// The extension WITHOUT the dot ("a/b/c.tar.png" -> "png"), or "" if none. A leading-dot file
// ("/.gitignore") has no extension.
inline std::string extension(const std::string& p) {
    const std::string name = fileName(p);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return "";
    }
    return name.substr(dot + 1);
}

// The file name without its extension ("a/b/c.png" -> "c").
inline std::string fileStem(const std::string& p) {
    std::string name = fileName(p);
    const size_t dot = name.find_last_of('.');
    if (dot == std::string::npos || dot == 0) {
        return name;
    }
    return name.substr(0, dot);
}

// The parent directory ("a/b/c.png" -> "a/b"), or "" if there is no separator.
inline std::string parentPath(const std::string& p) {
    const size_t s = p.find_last_of('/');
    if (s == std::string::npos) {
        return "";
    }
    return s == 0 ? "/" : p.substr(0, s);
}

class VirtualFileSystem {
  public:
    // Map a scheme (e.g. "res", "user", "mod") to a real base directory. Re-mounting replaces it.
    void mount(const std::string& scheme, const std::string& realDir) {
        m_mounts[scheme] = normalizePath(realDir);
    }
    bool isMounted(const std::string& scheme) const { return m_mounts.count(scheme) != 0; }
    void unmount(const std::string& scheme) { m_mounts.erase(scheme); }

    // Split "scheme://rest" into its parts. Returns false if there's no "scheme://" prefix.
    static bool parse(const std::string& vpath, std::string& scheme, std::string& rest) {
        const size_t sep = vpath.find("://");
        if (sep == std::string::npos) {
            return false;
        }
        scheme = vpath.substr(0, sep);
        rest = vpath.substr(sep + 3);
        return !scheme.empty();
    }

    // Resolve a scheme path ("res://textures/hero.png") to a real filesystem path. Returns "" if
    // the scheme isn't mounted, the path lacks a scheme, OR the relative part escapes the mount
    // root via
    // "..". The returned path is normalized.
    std::string resolve(const std::string& vpath) const {
        std::string scheme, rest;
        if (!parse(vpath, scheme, rest)) {
            return "";
        }
        auto it = m_mounts.find(scheme);
        if (it == m_mounts.end()) {
            return "";
        }
        // Normalize the RELATIVE part alone; if it now begins with "..", it escaped the mount.
        const std::string normRest = normalizePath(rest);
        if (normRest == ".." || normRest.rfind("../", 0) == 0) {
            return ""; // traversal escape — refuse
        }
        return normalizePath(joinPath(it->second, normRest));
    }

    // True if `vpath` resolves cleanly (mounted scheme, no escape).
    bool canResolve(const std::string& vpath) const { return !resolve(vpath).empty(); }

    std::vector<std::string> schemes() const {
        std::vector<std::string> out;
        out.reserve(m_mounts.size());
        for (const auto& [k, v] : m_mounts) {
            out.push_back(k);
        }
        return out;
    }

  private:
    std::unordered_map<std::string, std::string> m_mounts;
};

} // namespace maz::io
