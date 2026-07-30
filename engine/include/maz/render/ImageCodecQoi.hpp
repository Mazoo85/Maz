#pragma once

#include "maz/render/Image.hpp" // Image, color8

#include <cstddef>
#include <cstdint>
#include <vector>

// maz::render QOI codec — a headless, dependency-free encoder/decoder between render::Image and the
// "Quite OK Image" (.qoi) byte format, the fast lossless format Godot 4 imports natively. Encodes
// 32-bit RGBA; decodes 3- or 4-channel QOI. Pure CPU bytes (no GPU upload), exact to the QOI
// specification (https://qoiformat.org): running-array index, per-channel diff / luma deltas, and
// run-length runs, with the canonical 8-byte end marker.
namespace maz::render {

namespace detail {
inline int qoiHash(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a) {
    return (r * 3 + g * 5 + b * 7 + a * 11) & 63;
}
} // namespace detail

// Encode an Image to a QOI byte blob (channels = 4, sRGB with linear alpha). Empty image -> empty.
inline std::vector<std::uint8_t> encodeQoi(const Image& img) {
    std::vector<std::uint8_t> out;
    if (img.empty()) {
        return out;
    }
    const std::uint32_t w = static_cast<std::uint32_t>(img.width());
    const std::uint32_t h = static_cast<std::uint32_t>(img.height());
    auto push32 = [&out](std::uint32_t v) {
        out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xFF));
        out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
        out.push_back(static_cast<std::uint8_t>(v & 0xFF));
    };
    out.push_back('q');
    out.push_back('o');
    out.push_back('i');
    out.push_back('f');
    push32(w);
    push32(h);
    out.push_back(4); // channels
    out.push_back(0); // colorspace: sRGB with linear alpha

    std::uint8_t ir[64] = {0}, ig[64] = {0}, ib[64] = {0}, ia[64] = {0};
    std::uint8_t pr = 0, pg = 0, pb = 0, pa = 255;
    int run = 0;
    const std::vector<std::uint8_t>& px = img.data();
    const std::size_t n = px.size() / 4;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint8_t cr = px[i * 4], cg = px[i * 4 + 1], cb = px[i * 4 + 2], ca = px[i * 4 + 3];
        if (cr == pr && cg == pg && cb == pb && ca == pa) {
            ++run;
            if (run == 62 || i == n - 1) {
                out.push_back(static_cast<std::uint8_t>(0xC0 | (run - 1)));
                run = 0;
            }
        } else {
            if (run > 0) {
                out.push_back(static_cast<std::uint8_t>(0xC0 | (run - 1)));
                run = 0;
            }
            const int idx = detail::qoiHash(cr, cg, cb, ca);
            if (ir[idx] == cr && ig[idx] == cg && ib[idx] == cb && ia[idx] == ca) {
                out.push_back(static_cast<std::uint8_t>(idx)); // QOI_OP_INDEX (tag 00)
            } else {
                ir[idx] = cr;
                ig[idx] = cg;
                ib[idx] = cb;
                ia[idx] = ca;
                if (ca == pa) {
                    const std::int8_t vr = static_cast<std::int8_t>(cr - pr);
                    const std::int8_t vg = static_cast<std::int8_t>(cg - pg);
                    const std::int8_t vb = static_cast<std::int8_t>(cb - pb);
                    const std::int8_t vgr = static_cast<std::int8_t>(vr - vg);
                    const std::int8_t vgb = static_cast<std::int8_t>(vb - vg);
                    if (vr >= -2 && vr <= 1 && vg >= -2 && vg <= 1 && vb >= -2 && vb <= 1) {
                        out.push_back(static_cast<std::uint8_t>(0x40 | ((vr + 2) << 4) |
                                                               ((vg + 2) << 2) | (vb + 2)));
                    } else if (vg >= -32 && vg <= 31 && vgr >= -8 && vgr <= 7 && vgb >= -8 &&
                               vgb <= 7) {
                        out.push_back(static_cast<std::uint8_t>(0x80 | (vg + 32)));
                        out.push_back(static_cast<std::uint8_t>(((vgr + 8) << 4) | (vgb + 8)));
                    } else {
                        out.push_back(0xFE); // QOI_OP_RGB
                        out.push_back(cr);
                        out.push_back(cg);
                        out.push_back(cb);
                    }
                } else {
                    out.push_back(0xFF); // QOI_OP_RGBA
                    out.push_back(cr);
                    out.push_back(cg);
                    out.push_back(cb);
                    out.push_back(ca);
                }
            }
        }
        pr = cr;
        pg = cg;
        pb = cb;
        pa = ca;
    }
    for (int i = 0; i < 7; ++i) {
        out.push_back(0);
    }
    out.push_back(1); // end marker
    return out;
}

// Decode a QOI blob into an Image. Returns an empty Image on any malformed / truncated input.
inline Image decodeQoi(const std::uint8_t* d, std::size_t size) {
    if (d == nullptr || size < 14) {
        return Image{};
    }
    if (!(d[0] == 'q' && d[1] == 'o' && d[2] == 'i' && d[3] == 'f')) {
        return Image{};
    }
    const std::uint32_t w = (static_cast<std::uint32_t>(d[4]) << 24) |
                            (static_cast<std::uint32_t>(d[5]) << 16) |
                            (static_cast<std::uint32_t>(d[6]) << 8) | static_cast<std::uint32_t>(d[7]);
    const std::uint32_t h = (static_cast<std::uint32_t>(d[8]) << 24) |
                            (static_cast<std::uint32_t>(d[9]) << 16) |
                            (static_cast<std::uint32_t>(d[10]) << 8) |
                            static_cast<std::uint32_t>(d[11]);
    const std::uint8_t channels = d[12];
    if (w == 0 || h == 0 || (channels != 3 && channels != 4)) {
        return Image{};
    }
    // A hostile 14-byte header can declare enormous dimensions (e.g. 100000x100000); the Image(w,h)
    // allocation below would then attempt tens of gigabytes and OOM-crash the process on a tiny file (and a
    // width past INT_MAX would make the static_cast<int> below negative). QOI encodes at most 62 pixels per
    // byte (a QOI_OP_RUN), so a real WxH image always ships far more than pixels/62 bytes. Reject implausible
    // dimensions: cap each axis, cap total pixels to an absolute ceiling (defeats a large-file bomb), and
    // require the pixel count to stay within a generous multiple of the input size (defeats a tiny-file one).
    constexpr std::uint64_t kMaxDim = 1u << 16;    // 65536 per axis — beyond any real QOI texture
    constexpr std::uint64_t kMaxPixels = 1u << 28; // ~268M px (16384^2) — covers any real image
    const std::uint64_t pixels = static_cast<std::uint64_t>(w) * static_cast<std::uint64_t>(h);
    if (w > kMaxDim || h > kMaxDim || pixels > kMaxPixels ||
        pixels > static_cast<std::uint64_t>(size) * 64u) {
        return Image{};
    }
    Image img(static_cast<int>(w), static_cast<int>(h));
    std::uint8_t ir[64] = {0}, ig[64] = {0}, ib[64] = {0}, ia[64] = {0};
    std::uint8_t pr = 0, pg = 0, pb = 0, pa = 255;
    std::size_t p = 14;
    const std::size_t total = static_cast<std::size_t>(w) * static_cast<std::size_t>(h);
    int run = 0;
    for (std::size_t i = 0; i < total; ++i) {
        if (run > 0) {
            --run;
        } else {
            if (p >= size) {
                return Image{};
            }
            const std::uint8_t b1 = d[p++];
            if (b1 == 0xFE) {
                if (p + 3 > size) {
                    return Image{};
                }
                pr = d[p++];
                pg = d[p++];
                pb = d[p++];
            } else if (b1 == 0xFF) {
                if (p + 4 > size) {
                    return Image{};
                }
                pr = d[p++];
                pg = d[p++];
                pb = d[p++];
                pa = d[p++];
            } else if ((b1 & 0xC0) == 0x00) {
                const int idx = b1 & 63;
                pr = ir[idx];
                pg = ig[idx];
                pb = ib[idx];
                pa = ia[idx];
            } else if ((b1 & 0xC0) == 0x40) {
                pr = static_cast<std::uint8_t>(pr + ((b1 >> 4) & 3) - 2);
                pg = static_cast<std::uint8_t>(pg + ((b1 >> 2) & 3) - 2);
                pb = static_cast<std::uint8_t>(pb + (b1 & 3) - 2);
            } else if ((b1 & 0xC0) == 0x80) {
                if (p + 1 > size) {
                    return Image{};
                }
                const std::uint8_t b2 = d[p++];
                const int vg = (b1 & 63) - 32;
                pr = static_cast<std::uint8_t>(pr + vg - 8 + ((b2 >> 4) & 15));
                pg = static_cast<std::uint8_t>(pg + vg);
                pb = static_cast<std::uint8_t>(pb + vg - 8 + (b2 & 15));
            } else { // (b1 & 0xC0) == 0xC0 -> QOI_OP_RUN
                run = b1 & 63;
            }
            const int idx = detail::qoiHash(pr, pg, pb, pa);
            ir[idx] = pr;
            ig[idx] = pg;
            ib[idx] = pb;
            ia[idx] = pa;
        }
        const int x = static_cast<int>(i % w);
        const int y = static_cast<int>(i / w);
        img.setPixel(x, y, color8(pr, pg, pb, pa));
    }
    return img;
}

// Convenience overload for a byte vector.
inline Image decodeQoi(const std::vector<std::uint8_t>& bytes) {
    return decodeQoi(bytes.data(), bytes.size());
}

} // namespace maz::render
