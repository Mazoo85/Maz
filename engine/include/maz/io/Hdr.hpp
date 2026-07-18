#pragma once

#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// maz::io Radiance HDR (.hdr / RGBE) decoder — the high-dynamic-range image format used for skyboxes
// and image-based lighting (Godot loads .hdr for its Sky/environment). Each pixel is stored as RGBE:
// three 8-bit mantissas sharing one 8-bit exponent, so a whole float-range image fits in 4 bytes/px;
// decoding expands it back to linear float RGB. This parses the text header (magic, FORMAT, the
// "-Y H +X W" resolution line), the modern per-channel run-length scanline encoding (and a raw
// fallback), and converts RGBE -> float via the standard ldexp reconstruction. Pure bytes in, floats
// out — no GPU, no file I/O in the core — so it unit-tests headlessly; the renderer uploads the floats.
namespace maz::io {

struct HdrImage {
    int width = 0;
    int height = 0;
    std::vector<float> rgb; // width*height*3, row-major, top row first
    bool valid() const { return width > 0 && height > 0 && rgb.size() == static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u; }
};

namespace detail {

inline void rgbeToFloat(uint8_t r, uint8_t g, uint8_t b, uint8_t e, float* out) {
    if (e == 0) {
        out[0] = out[1] = out[2] = 0.0f;
        return;
    }
    const float f = std::ldexp(1.0f, static_cast<int>(e) - (128 + 8));
    out[0] = static_cast<float>(r) * f;
    out[1] = static_cast<float>(g) * f;
    out[2] = static_cast<float>(b) * f;
}

} // namespace detail

// Decode a Radiance .hdr byte buffer. Returns an image with valid()==false on malformed input.
inline HdrImage decodeHdr(const uint8_t* data, std::size_t size) {
    HdrImage img;
    if (data == nullptr || size < 8) {
        return img;
    }
    std::size_t pos = 0;

    auto readLine = [&](std::string& line) -> bool {
        line.clear();
        while (pos < size && data[pos] != '\n') {
            line.push_back(static_cast<char>(data[pos]));
            ++pos;
        }
        if (pos < size) {
            ++pos; // consume '\n'
            return true;
        }
        return false;
    };

    // First line must be the Radiance magic.
    std::string line;
    if (!readLine(line)) {
        return img;
    }
    if (line.compare(0, 2, "#?") != 0) {
        return img;
    }

    // Header key=value lines, terminated by a blank line.
    bool sawBlank = false;
    while (readLine(line)) {
        if (line.empty()) {
            sawBlank = true;
            break;
        }
    }
    if (!sawBlank) {
        return img;
    }

    // Resolution line: "-Y <h> +X <w>" (standard orientation). Accept +Y too; sign only affects row
    // order, which we normalize to top-first for -Y.
    if (!readLine(line)) {
        return img;
    }
    int h = 0;
    int w = 0;
    bool topDown = true;
    {
        // Parse two tokens of the form <axis><num>.
        // Expect: [+-]Y <num> [+-]X <num>
        char yAxisSign = '-';
        // very small tokenizer
        std::vector<std::string> tok;
        std::string cur;
        for (char c : line) {
            if (c == ' ' || c == '\t' || c == '\r') {
                if (!cur.empty()) {
                    tok.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(c);
            }
        }
        if (!cur.empty()) {
            tok.push_back(cur);
        }
        if (tok.size() != 4) {
            return img;
        }
        // tok[0]=±Y, tok[1]=h, tok[2]=±X, tok[3]=w
        if (tok[0].size() < 2 || tok[0][1] != 'Y' || tok[2].size() < 2 || tok[2][1] != 'X') {
            return img;
        }
        yAxisSign = tok[0][0];
        topDown = (yAxisSign == '-'); // -Y means rows go top->bottom
        h = std::atoi(tok[1].c_str());
        w = std::atoi(tok[3].c_str());
    }
    if (w <= 0 || h <= 0) {
        return img;
    }

    img.width = w;
    img.height = h;
    img.rgb.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h) * 3u, 0.0f);

    std::vector<uint8_t> scan(static_cast<std::size_t>(w) * 4u); // per-row RGBE

    for (int y = 0; y < h; ++y) {
        // Decode one scanline into `scan` (RGBE interleaved).
        bool newRle = false;
        if (w >= 8 && w <= 0x7fff && pos + 4 <= size) {
            const uint8_t b0 = data[pos];
            const uint8_t b1 = data[pos + 1];
            const uint8_t b2 = data[pos + 2];
            const uint8_t b3 = data[pos + 3];
            if (b0 == 2 && b1 == 2 && ((static_cast<int>(b2) << 8) | b3) == w) {
                newRle = true;
                pos += 4;
            }
        }

        if (newRle) {
            // Four channels, each RLE-encoded across the row.
            for (int c = 0; c < 4; ++c) {
                int x = 0;
                while (x < w) {
                    if (pos >= size) {
                        return HdrImage{}; // truncated
                    }
                    const int count = data[pos++];
                    if (count > 128) {
                        // A run: (count-128) copies of the next byte.
                        if (pos >= size) {
                            return HdrImage{};
                        }
                        const uint8_t val = data[pos++];
                        const int run = count - 128;
                        for (int i = 0; i < run && x < w; ++i, ++x) {
                            scan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] = val;
                        }
                    } else {
                        // `count` literal bytes.
                        for (int i = 0; i < count && x < w; ++i, ++x) {
                            if (pos >= size) {
                                return HdrImage{};
                            }
                            scan[static_cast<std::size_t>(x) * 4u + static_cast<std::size_t>(c)] =
                                data[pos++];
                        }
                    }
                }
            }
        } else {
            // Raw (flat) scanline: 4 bytes per pixel.
            if (pos + static_cast<std::size_t>(w) * 4u > size) {
                return HdrImage{};
            }
            for (int x = 0; x < w * 4; ++x) {
                scan[static_cast<std::size_t>(x)] = data[pos++];
            }
        }

        const int destRow = topDown ? y : (h - 1 - y);
        for (int x = 0; x < w; ++x) {
            const std::size_t si = static_cast<std::size_t>(x) * 4u;
            float* out = &img.rgb[(static_cast<std::size_t>(destRow) * static_cast<std::size_t>(w) +
                                   static_cast<std::size_t>(x)) *
                                  3u];
            detail::rgbeToFloat(scan[si + 0], scan[si + 1], scan[si + 2], scan[si + 3], out);
        }
    }

    return img;
}

inline HdrImage decodeHdr(const std::vector<uint8_t>& bytes) {
    return decodeHdr(bytes.data(), bytes.size());
}

} // namespace maz::io
