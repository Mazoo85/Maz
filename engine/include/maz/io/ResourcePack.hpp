#pragma once

#include "maz/io/Serialize.hpp"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace maz::io {

// Resource pack archive — Godot's PackedData / the .pck file its shipping games load every asset from.
// Maz could already serialize a single blob (io::Serialize) and read/write one file at a time, but there
// was no way to bundle MANY named resources — textures, level JSON, sound clips, prefab text — into ONE
// archive and pull them back out by path. That is what a game ships: one .pck instead of a loose tree of
// files. This is a self-contained byte-in / byte-out container built on the existing ByteWriter/ByteReader:
// `packResources` writes a magic+version header, a directory of (path, offset, size) records, then the
// concatenated blob data; `ResourcePack::load` parses that back and hands out each blob by path with full
// bounds checking, so a truncated or foreign archive fails cleanly instead of reading out of range. The app
// owns any real disk read/write (via io::writeFile / io::readFile) — this just does the packing.

struct PackEntry {
    std::string path;             // logical name, e.g. "levels/forest.json"
    std::vector<std::uint8_t> data;
};

namespace detail {
// 'M','Z','P','1' — Maz pack, format 1. Distinct from other Maz binary headers so a mismatched archive is
// rejected up front rather than parsed as garbage.
constexpr std::uint32_t kPackMagic = 0x31505A4D;
constexpr std::uint32_t kPackVersion = 1;
} // namespace detail

// Bundle named blobs into one archive byte stream. Directory offsets are relative to the start of the data
// section, so the archive is position-independent. Duplicate paths are written verbatim; on load the last
// occurrence wins (mirroring a filesystem overwrite).
inline std::vector<std::uint8_t> packResources(const std::vector<PackEntry>& entries) {
    ByteWriter w;
    w.writeHeader(detail::kPackMagic, detail::kPackVersion);
    w.write<std::uint32_t>(static_cast<std::uint32_t>(entries.size()));

    std::uint32_t offset = 0;
    for (const PackEntry& e : entries) {
        w.writeString(e.path);
        w.write<std::uint32_t>(offset);
        w.write<std::uint32_t>(static_cast<std::uint32_t>(e.data.size()));
        offset += static_cast<std::uint32_t>(e.data.size());
    }
    for (const PackEntry& e : entries) {
        w.writeBytes(e.data.data(), e.data.size());
    }
    return w.data();
}

class ResourcePack {
public:
    // Parse an archive. Returns false (and leaves the pack empty) on a foreign/short/corrupt stream.
    bool load(const std::uint8_t* data, std::size_t size) {
        m_entries.clear();
        m_order.clear();
        ByteReader r(data, size);
        if (!r.readHeader(detail::kPackMagic, detail::kPackVersion)) {
            return false;
        }
        const std::uint32_t count = r.read<std::uint32_t>();
        if (!r.ok()) {
            return false;
        }

        struct Dir {
            std::string path;
            std::uint32_t offset;
            std::uint32_t size;
        };
        std::vector<Dir> dir;
        dir.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i) {
            Dir d;
            d.path = r.readString();
            d.offset = r.read<std::uint32_t>();
            d.size = r.read<std::uint32_t>();
            if (!r.ok()) {
                return false;
            }
            dir.push_back(std::move(d));
        }

        // The data section begins exactly where the directory ended.
        const std::size_t dataStart = size - r.remaining();
        for (const Dir& d : dir) {
            const std::size_t begin = dataStart + d.offset;
            const std::size_t end = begin + d.size;
            if (begin < dataStart || end < begin || end > size) {
                m_entries.clear();
                m_order.clear();
                return false;
            }
            if (m_entries.find(d.path) == m_entries.end()) {
                m_order.push_back(d.path);
            }
            m_entries[d.path].assign(data + begin, data + end);
        }
        return true;
    }

    bool load(const std::vector<std::uint8_t>& bytes) { return load(bytes.data(), bytes.size()); }

    bool contains(const std::string& path) const { return m_entries.find(path) != m_entries.end(); }

    // Returns a pointer to the blob's bytes, or nullptr if the path isn't in the archive.
    const std::vector<std::uint8_t>* get(const std::string& path) const {
        const auto it = m_entries.find(path);
        return it == m_entries.end() ? nullptr : &it->second;
    }

    // Convenience: fetch a blob as a string (empty if absent).
    std::string getString(const std::string& path) const {
        const std::vector<std::uint8_t>* b = get(path);
        return b ? std::string(b->begin(), b->end()) : std::string();
    }

    // Paths in the order they were packed (deduplicated).
    const std::vector<std::string>& paths() const { return m_order; }

    std::size_t count() const { return m_entries.size(); }

private:
    std::unordered_map<std::string, std::vector<std::uint8_t>> m_entries;
    std::vector<std::string> m_order;
};

} // namespace maz::io
