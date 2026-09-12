#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

// maz::io DEFLATE / zlib inflate (RFC 1951 + RFC 1950) — a header-only, dependency-free decompressor.
// This is the missing building block under PNG import, gzip/zlib assets, and KTX2 ZLIB supercompression:
// Godot leans on zlib for all of these, and Maz previously had no inflate at all. `inflateRaw` expands a
// raw DEFLATE stream; `zlibInflate` validates and strips the 2-byte zlib header (and optional preset
// dictionary) before inflating. The decoder is the canonical bit-at-a-time Huffman walk (the well-known
// "puff" approach), handling stored, fixed-Huffman, and dynamic-Huffman blocks with LZ77 back-references.
// Pure CPU byte work — unit-tested headlessly against golden streams produced by the reference zlib.
//
// Scope note (honest): this decompresses; it does not compress, and it does not verify the trailing Adler-32
// checksum (it stops cleanly at the final block). Malformed input returns false rather than throwing.
namespace maz::io {

namespace detail {

struct InflateState {
    const std::uint8_t* in = nullptr;
    std::size_t inLen = 0;
    std::size_t inPos = 0;
    int bitBuf = 0;
    int bitCnt = 0;
    bool error = false;
    std::vector<std::uint8_t>* out = nullptr;

    // Read `need` bits, least-significant bit first (per RFC 1951 §3.1.1).
    int bits(int need) {
        int val = bitBuf;
        while (bitCnt < need) {
            if (inPos >= inLen) {
                error = true;
                return 0;
            }
            val |= static_cast<int>(in[inPos++]) << bitCnt;
            bitCnt += 8;
        }
        bitBuf = val >> need;
        bitCnt -= need;
        return val & ((1 << need) - 1);
    }
};

struct Huffman {
    std::array<int, 16> count{};
    std::vector<int> symbol;
};

// Build a canonical Huffman table from per-symbol code lengths. Returns <0 on an invalid (over-subscribed)
// set, 0 on a complete set, >0 on an incomplete set (tolerated for the fixed distance table).
inline int construct(Huffman& h, const int* length, int n) {
    h.count.fill(0);
    h.symbol.assign(static_cast<std::size_t>(n), 0);
    for (int s = 0; s < n; ++s) h.count[static_cast<std::size_t>(length[s])]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len <= 15; ++len) {
        left <<= 1;
        left -= h.count[static_cast<std::size_t>(len)];
        if (left < 0) return left;
    }
    std::array<int, 16> offs{};
    offs[1] = 0;
    for (int len = 1; len < 15; ++len)
        offs[static_cast<std::size_t>(len + 1)] =
            offs[static_cast<std::size_t>(len)] + h.count[static_cast<std::size_t>(len)];
    for (int s = 0; s < n; ++s) {
        if (length[s] != 0) {
            h.symbol[static_cast<std::size_t>(offs[static_cast<std::size_t>(length[s])]++)] = s;
        }
    }
    return left;
}

inline int decode(InflateState& s, const Huffman& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len <= 15; ++len) {
        code |= s.bits(1);
        if (s.error) return -1;
        const int count = h.count[static_cast<std::size_t>(len)];
        if (code - count < first) return h.symbol[static_cast<std::size_t>(index + (code - first))];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

inline bool inflateBlock(InflateState& s, const Huffman& lit, const Huffman& dist) {
    static const int lbase[29] = {3,  4,  5,  6,   7,   8,   9,   10,  11,  13,  15,  17,  19,  23, 27,
                                  31, 35, 43, 51,  59,  67,  83,  99,  115, 131, 163, 195, 227, 258};
    static const int lext[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
    static const int dbase[30] = {1,   2,   3,   4,   5,    7,    9,    13,   17,    25,
                                  33,  49,  65,  97,  129,  193,  257,  385,  513,   769,
                                  1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
    static const int dext[30] = {0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
                                 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
    for (;;) {
        int sym = decode(s, lit);
        if (s.error || sym < 0) return false;
        if (sym == 256) return true; // end of block
        if (sym < 256) {
            s.out->push_back(static_cast<std::uint8_t>(sym));
            continue;
        }
        sym -= 257;
        if (sym >= 29) return false;
        const int len = lbase[sym] + s.bits(lext[sym]);
        const int dsym = decode(s, dist);
        if (s.error || dsym < 0 || dsym >= 30) return false;
        const int distance = dbase[dsym] + s.bits(dext[dsym]);
        if (s.error || distance <= 0 || static_cast<std::size_t>(distance) > s.out->size()) return false;
        const std::size_t start = s.out->size() - static_cast<std::size_t>(distance);
        for (int k = 0; k < len; ++k) s.out->push_back((*s.out)[start + static_cast<std::size_t>(k)]);
    }
}

inline bool fixedBlock(InflateState& s) {
    int litLen[288];
    for (int i = 0; i < 144; ++i) litLen[i] = 8;
    for (int i = 144; i < 256; ++i) litLen[i] = 9;
    for (int i = 256; i < 280; ++i) litLen[i] = 7;
    for (int i = 280; i < 288; ++i) litLen[i] = 8;
    int distLen[30];
    for (int i = 0; i < 30; ++i) distLen[i] = 5;
    Huffman lit, dist;
    construct(lit, litLen, 288);
    construct(dist, distLen, 30);
    return inflateBlock(s, lit, dist);
}

inline bool dynamicBlock(InflateState& s) {
    static const int order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int hlit = s.bits(5) + 257;
    const int hdist = s.bits(5) + 1;
    const int hclen = s.bits(4) + 4;
    if (s.error || hlit > 286 || hdist > 30) return false;

    int clLen[19] = {0};
    for (int i = 0; i < hclen; ++i) clLen[order[i]] = s.bits(3);
    if (s.error) return false;
    Huffman clHuff;
    if (construct(clHuff, clLen, 19) < 0) return false;

    int lengths[286 + 30] = {0};
    int idx = 0;
    const int total = hlit + hdist;
    while (idx < total) {
        int sym = decode(s, clHuff);
        if (s.error || sym < 0) return false;
        if (sym < 16) {
            lengths[idx++] = sym;
        } else if (sym == 16) {
            if (idx == 0) return false;
            const int prev = lengths[idx - 1];
            int rep = s.bits(2) + 3;
            while (rep-- && idx < total) lengths[idx++] = prev;
        } else if (sym == 17) {
            int rep = s.bits(3) + 3;
            while (rep-- && idx < total) lengths[idx++] = 0;
        } else { // 18
            int rep = s.bits(7) + 11;
            while (rep-- && idx < total) lengths[idx++] = 0;
        }
        if (s.error) return false;
    }
    Huffman lit, dist;
    if (construct(lit, lengths, hlit) < 0) return false;
    construct(dist, lengths + hlit, hdist); // incomplete distance tables are legal
    return inflateBlock(s, lit, dist);
}

inline bool storedBlock(InflateState& s) {
    s.bitBuf = 0;
    s.bitCnt = 0; // align to byte boundary
    if (s.inPos + 4 > s.inLen) return false;
    const int len = static_cast<int>(s.in[s.inPos]) | (static_cast<int>(s.in[s.inPos + 1]) << 8);
    const int nlen = static_cast<int>(s.in[s.inPos + 2]) | (static_cast<int>(s.in[s.inPos + 3]) << 8);
    s.inPos += 4;
    if ((len ^ 0xFFFF) != nlen) return false;
    if (s.inPos + static_cast<std::size_t>(len) > s.inLen) return false;
    for (int i = 0; i < len; ++i) s.out->push_back(s.in[s.inPos++]);
    return true;
}

} // namespace detail

inline bool inflateRaw(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out) {
    detail::InflateState s;
    s.in = data;
    s.inLen = size;
    s.out = &out;
    int final = 0;
    do {
        final = s.bits(1);
        const int type = s.bits(2);
        if (s.error) return false;
        bool ok = false;
        if (type == 0) ok = detail::storedBlock(s);
        else if (type == 1) ok = detail::fixedBlock(s);
        else if (type == 2) ok = detail::dynamicBlock(s);
        else return false; // reserved
        if (!ok || s.error) return false;
    } while (!final);
    return true;
}

inline bool inflateRaw(const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& out) {
    return inflateRaw(bytes.data(), bytes.size(), out);
}

inline bool zlibInflate(const std::uint8_t* data, std::size_t size, std::vector<std::uint8_t>& out) {
    if (size < 2) return false;
    const unsigned cmf = data[0];
    const unsigned flg = data[1];
    if ((cmf & 0x0F) != 8) return false;             // compression method must be DEFLATE
    if (((cmf << 8) | flg) % 31 != 0) return false;  // header checksum
    std::size_t off = 2;
    if (flg & 0x20) off += 4; // FDICT: skip the 4-byte dictionary id
    if (off > size) return false;
    return inflateRaw(data + off, size - off, out);
}

inline bool zlibInflate(const std::vector<std::uint8_t>& bytes, std::vector<std::uint8_t>& out) {
    return zlibInflate(bytes.data(), bytes.size(), out);
}

} // namespace maz::io
