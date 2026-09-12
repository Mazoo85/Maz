#pragma once

#include "maz/io/ConfigFile.hpp"

#include <cstdint>
#include <string>
#include <vector>

// maz::io asset-import sidecar — Godot's `.import` pipeline. Every imported source asset (a .png, a
// .gltf, a .wav) gets a sibling `<source>.import` file describing HOW it was imported: which importer
// ran, the resource type/UID it produced, the source it came from, and the cooked file(s) written into
// the `.godot/imported/` cache. The engine loads the cooked resource, not the raw source, and reimports
// only when the source changed. This models that sidecar as data: parse/encode the INI-with-arrays
// format, the cooked-path convention, and an ImportDatabase that answers "does this need reimporting?"
// from a content hash. Pure string/data logic (no filesystem here) so it unit-tests headlessly; the
// editor/tool layer does the actual file reads + cooking on top.
namespace maz::io {

// Stable 64-bit FNV-1a content hash, hex-formatted. Deterministic across runs/platforms, so a source
// file's hash only changes when its bytes change — exactly what reimport detection needs. (Godot uses
// md5; any stable content hash drives the same decision. This is a hash, not a cryptographic digest.)
inline std::string contentHashHex(const std::string& bytes) {
    uint64_t h = 1469598103934665603ull; // FNV offset basis
    for (char c : bytes) {
        h ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        h *= 1099511628211ull; // FNV prime
    }
    static const char* kHex = "0123456789abcdef";
    std::string out(16, '0');
    for (int i = 15; i >= 0; --i) {
        out[static_cast<std::size_t>(i)] = kHex[h & 0xF];
        h >>= 4;
    }
    return out;
}

// The parsed form of a `.import` sidecar.
struct ImportFile {
    // [remap]
    std::string importer;     // e.g. "texture", "scene", "wav"
    std::string type;         // produced resource type, e.g. "CompressedTexture2D"
    std::string uid;          // stable resource id, e.g. "uid://abc123"
    std::string importedPath; // primary cooked resource, e.g. "res://.godot/imported/icon.png-<h>.ctex"
    // [deps]
    std::string sourceFile;               // "res://icon.png"
    std::vector<std::string> destFiles;   // all cooked outputs (usually includes importedPath)
    // [params] — importer options, raw strings, insertion-ordered (compress/mode, mipmaps/generate...).
    ConfigFile params;

    bool valid() const { return !importer.empty() && !sourceFile.empty(); }

    // Serialize to Godot's `.import` text: [remap], [deps], then [params].
    std::string encode() const {
        std::string out;
        out += "[remap]\n\n";
        out += "importer=" + quote(importer) + "\n";
        if (!type.empty()) {
            out += "type=" + quote(type) + "\n";
        }
        if (!uid.empty()) {
            out += "uid=" + quote(uid) + "\n";
        }
        if (!importedPath.empty()) {
            out += "path=" + quote(importedPath) + "\n";
        }
        out += "\n[deps]\n\n";
        out += "source_file=" + quote(sourceFile) + "\n";
        out += "dest_files=" + encodeArray(destFiles) + "\n";
        // [params]: reuse ConfigFile's encoding for the single "params" section.
        const std::vector<std::string> keys = params.sectionKeys("params");
        if (!keys.empty()) {
            out += "\n[params]\n\n";
            for (const std::string& k : keys) {
                out += k + "=" + params.getValue("params", k) + "\n";
            }
        }
        return out;
    }

    // Parse `.import` text. Lenient (INI parsing never fails); check valid() on the result.
    static ImportFile parse(const std::string& text) {
        ConfigFile cf;
        cf.parse(text);
        ImportFile f;
        f.importer = unquote(cf.getValue("remap", "importer"));
        f.type = unquote(cf.getValue("remap", "type"));
        f.uid = unquote(cf.getValue("remap", "uid"));
        f.importedPath = unquote(cf.getValue("remap", "path"));
        f.sourceFile = unquote(cf.getValue("deps", "source_file"));
        f.destFiles = parseArray(cf.getValue("deps", "dest_files"));
        for (const std::string& k : cf.sectionKeys("params")) {
            f.params.setValue("params", k, cf.getValue("params", k));
        }
        return f;
    }

    // The Godot cooked-resource path convention: res://.godot/imported/<basename>-<hash>.<ext>.
    // `sourceResPath` is a res:// path; `ext` is the cooked extension (no dot), `hash` a content hash.
    static std::string cookedPath(const std::string& sourceResPath, const std::string& hash,
                                  const std::string& ext) {
        std::string base = sourceResPath;
        const std::size_t slash = base.find_last_of('/');
        if (slash != std::string::npos) {
            base = base.substr(slash + 1);
        }
        return "res://.godot/imported/" + base + "-" + hash + "." + ext;
    }

  private:
    static std::string quote(const std::string& s) { return "\"" + s + "\""; }
    static std::string unquote(const std::string& s) {
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            return s.substr(1, s.size() - 2);
        }
        return s;
    }
    static std::string encodeArray(const std::vector<std::string>& items) {
        std::string out = "[";
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (i) {
                out += ", ";
            }
            out += quote(items[i]);
        }
        out += "]";
        return out;
    }
    static std::vector<std::string> parseArray(const std::string& raw) {
        std::vector<std::string> out;
        std::size_t a = 0, b = raw.size();
        while (a < b && (raw[a] == ' ' || raw[a] == '[')) {
            ++a;
        }
        while (b > a && (raw[b - 1] == ' ' || raw[b - 1] == ']')) {
            --b;
        }
        std::string inner = raw.substr(a, b - a);
        std::string cur;
        auto flush = [&]() {
            std::size_t x = 0, y = cur.size();
            while (x < y && (cur[x] == ' ' || cur[x] == '\t')) {
                ++x;
            }
            while (y > x && (cur[y - 1] == ' ' || cur[y - 1] == '\t')) {
                --y;
            }
            std::string t = cur.substr(x, y - x);
            if (!t.empty()) {
                out.push_back(unquote(t));
            }
            cur.clear();
        };
        for (char c : inner) {
            if (c == ',') {
                flush();
            } else {
                cur.push_back(c);
            }
        }
        flush();
        return out;
    }
};

// A registry of what has been imported, keyed by source res:// path — the model behind "reimport only
// changed files" in Godot's import dock. Records each source's last-imported content hash + cooked path;
// needsReimport() is true when a source is unknown or its bytes changed since it was recorded.
class ImportDatabase {
  public:
    struct Record {
        std::string sourcePath;
        std::string sourceHash;   // content hash at import time
        std::string importedPath; // cooked resource path
        std::string importer;
    };

    // Record (or update) an import result for a source path.
    void record(const std::string& sourcePath, const std::string& sourceHash,
                const std::string& importedPath, const std::string& importer) {
        for (Record& r : m_records) {
            if (r.sourcePath == sourcePath) {
                r.sourceHash = sourceHash;
                r.importedPath = importedPath;
                r.importer = importer;
                return;
            }
        }
        m_records.push_back(Record{sourcePath, sourceHash, importedPath, importer});
    }

    const Record* find(const std::string& sourcePath) const {
        for (const Record& r : m_records) {
            if (r.sourcePath == sourcePath) {
                return &r;
            }
        }
        return nullptr;
    }

    bool isImported(const std::string& sourcePath) const { return find(sourcePath) != nullptr; }

    // True if the source has never been imported, or its current hash differs from the recorded one.
    bool needsReimport(const std::string& sourcePath, const std::string& currentHash) const {
        const Record* r = find(sourcePath);
        return r == nullptr || r->sourceHash != currentHash;
    }

    void remove(const std::string& sourcePath) {
        for (std::size_t i = 0; i < m_records.size(); ++i) {
            if (m_records[i].sourcePath == sourcePath) {
                m_records.erase(m_records.begin() + static_cast<std::ptrdiff_t>(i));
                return;
            }
        }
    }

    std::size_t count() const { return m_records.size(); }
    const std::vector<Record>& records() const { return m_records; }

  private:
    std::vector<Record> m_records;
};

} // namespace maz::io
