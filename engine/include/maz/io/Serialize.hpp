#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

namespace maz::io {

// Binary serialization: the engine's backbone for save games, level files, and any structured data
// that must round-trip to disk (or across a wire later). ByteWriter appends; ByteReader consumes
// with bounds checking so truncated/corrupt input fails cleanly (ok() == false) instead of reading
// out of range. Layout is the host byte order — fine for the LE desktop targets the engine builds
// for; a byte-swap layer can slot in later if a BE target ever matters. Header-only.

class ByteWriter {
public:
    void writeBytes(const void* p, size_t n) {
        const auto* b = static_cast<const uint8_t*>(p);
        m_buf.insert(m_buf.end(), b, b + n);
    }
    // Append a trivially-copyable value (int, float, POD struct).
    template <typename T>
    void write(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>, "ByteWriter::write needs a POD type");
        writeBytes(&v, sizeof(T));
    }
    // Length-prefixed string (uint32 length + bytes).
    void writeString(const std::string& s) {
        write<uint32_t>(static_cast<uint32_t>(s.size()));
        writeBytes(s.data(), s.size());
    }
    // Length-prefixed array of POD elements.
    template <typename T>
    void writeVector(const std::vector<T>& v) {
        static_assert(std::is_trivially_copyable_v<T>, "writeVector needs POD elements");
        write<uint32_t>(static_cast<uint32_t>(v.size()));
        writeBytes(v.data(), v.size() * sizeof(T));
    }
    // A magic tag + version, so a reader can reject foreign or newer files up front.
    void writeHeader(uint32_t magic, uint32_t version) {
        write<uint32_t>(magic);
        write<uint32_t>(version);
    }

    const std::vector<uint8_t>& data() const { return m_buf; }
    size_t size() const { return m_buf.size(); }

private:
    std::vector<uint8_t> m_buf;
};

class ByteReader {
public:
    ByteReader(const uint8_t* p, size_t n) : m_p(p), m_n(n) {}
    explicit ByteReader(const std::vector<uint8_t>& v) : m_p(v.data()), m_n(v.size()) {}

    bool ok() const { return m_ok; }
    size_t remaining() const { return m_off <= m_n ? m_n - m_off : 0; }

    bool readBytes(void* out, size_t n) {
        if (!m_ok || m_off + n > m_n) {
            m_ok = false;
            return false;
        }
        std::memcpy(out, m_p + m_off, n);
        m_off += n;
        return true;
    }
    template <typename T>
    T read() {
        static_assert(std::is_trivially_copyable_v<T>, "ByteReader::read needs a POD type");
        T v{};
        readBytes(&v, sizeof(T));
        return v;
    }
    std::string readString() {
        const uint32_t len = read<uint32_t>();
        std::string s;
        if (m_ok && m_off + len <= m_n) {
            s.assign(reinterpret_cast<const char*>(m_p + m_off), len);
            m_off += len;
        } else {
            m_ok = false;
        }
        return s;
    }
    template <typename T>
    std::vector<T> readVector() {
        static_assert(std::is_trivially_copyable_v<T>, "readVector needs POD elements");
        const uint32_t count = read<uint32_t>();
        std::vector<T> v;
        if (m_ok && m_off + static_cast<size_t>(count) * sizeof(T) <= m_n) {
            v.resize(count);
            readBytes(v.data(), static_cast<size_t>(count) * sizeof(T));
        } else {
            m_ok = false;
        }
        return v;
    }
    // Read + validate a magic/version header. Returns false (and sets not-ok) on any mismatch.
    bool readHeader(uint32_t expectMagic, uint32_t expectVersion) {
        const uint32_t magic = read<uint32_t>();
        const uint32_t version = read<uint32_t>();
        if (!m_ok || magic != expectMagic || version != expectVersion) {
            m_ok = false;
            return false;
        }
        return true;
    }

private:
    const uint8_t* m_p;
    size_t m_n;
    size_t m_off = 0;
    bool m_ok = true;
};

// Write a byte buffer to a file (binary). Returns false if the file can't be opened.
inline bool writeFile(const std::string& path, const std::vector<uint8_t>& bytes) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        return false;
    }
    if (!bytes.empty()) {
        f.write(reinterpret_cast<const char*>(bytes.data()),
                static_cast<std::streamsize>(bytes.size()));
    }
    return static_cast<bool>(f);
}

// Read a whole file into a byte buffer. Returns false if it can't be opened.
inline bool readFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) {
        return false;
    }
    const std::streamsize n = f.tellg();
    out.resize(n > 0 ? static_cast<size_t>(n) : 0);
    if (n > 0) {
        f.seekg(0);
        f.read(reinterpret_cast<char*>(out.data()), n);
    }
    return static_cast<bool>(f) || n == 0;
}

} // namespace maz::io
