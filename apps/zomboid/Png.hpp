// Minimal, dependency-free PNG writer (8-bit RGB) for the headless renderer.
// Uses stored (uncompressed) DEFLATE blocks so no zlib is required.
#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace zbpng {

inline uint32_t crc32(const uint8_t* data, size_t len, uint32_t crc = 0xFFFFFFFFu) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t n = 0; n < 256; n++) {
            uint32_t c = n;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[n] = c;
        }
        init = true;
    }
    for (size_t i = 0; i < len; i++) crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return crc;
}

inline void putU32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>(v & 0xFF));
}

inline void chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    putU32(out, static_cast<uint32_t>(data.size()));
    const size_t typeStart = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    const uint32_t c = crc32(out.data() + typeStart, out.size() - typeStart);
    putU32(out, c ^ 0xFFFFFFFFu);
}

// Wrap raw bytes in a zlib stream of stored DEFLATE blocks + Adler-32.
inline std::vector<uint8_t> zlibStored(const std::vector<uint8_t>& raw) {
    std::vector<uint8_t> z;
    z.push_back(0x78); // CMF
    z.push_back(0x01); // FLG (no dict, fastest)
    size_t pos = 0;
    while (pos < raw.size()) {
        const size_t remain = raw.size() - pos;
        const uint16_t block = static_cast<uint16_t>(remain > 65535 ? 65535 : remain);
        const bool last = (pos + block) >= raw.size();
        z.push_back(last ? 1 : 0); // BFINAL, BTYPE=00 (stored)
        z.push_back(static_cast<uint8_t>(block & 0xFF));
        z.push_back(static_cast<uint8_t>((block >> 8) & 0xFF));
        const uint16_t nlen = static_cast<uint16_t>(~block);
        z.push_back(static_cast<uint8_t>(nlen & 0xFF));
        z.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        z.insert(z.end(), raw.begin() + static_cast<long>(pos),
                 raw.begin() + static_cast<long>(pos + block));
        pos += block;
    }
    // Adler-32 over raw.
    uint32_t a = 1, b = 0;
    for (uint8_t byte : raw) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    putU32(z, (b << 16) | a);
    return z;
}

// Write an 8-bit RGB image (px is row-major, 3 bytes/pixel) to a PNG file.
inline bool write(const std::string& path, int w, int h, const std::vector<uint8_t>& px) {
    // Filtered scanlines: each row prefixed with filter byte 0 (None).
    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>(h) * (1 + static_cast<size_t>(w) * 3));
    const size_t rowBytes = static_cast<size_t>(w) * 3;
    for (int y = 0; y < h; y++) {
        raw.push_back(0);
        const size_t off = static_cast<size_t>(y) * rowBytes;
        raw.insert(raw.end(), px.begin() + static_cast<long>(off),
                   px.begin() + static_cast<long>(off + rowBytes));
    }

    std::vector<uint8_t> out = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    std::vector<uint8_t> ihdr;
    putU32(ihdr, static_cast<uint32_t>(w));
    putU32(ihdr, static_cast<uint32_t>(h));
    ihdr.push_back(8); // bit depth
    ihdr.push_back(2); // color type: truecolor RGB
    ihdr.push_back(0); // compression
    ihdr.push_back(0); // filter
    ihdr.push_back(0); // interlace
    chunk(out, "IHDR", ihdr);
    chunk(out, "IDAT", zlibStored(raw));
    chunk(out, "IEND", {});

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t wrote = std::fwrite(out.data(), 1, out.size(), f);
    std::fclose(f);
    return wrote == out.size();
}

} // namespace zbpng
