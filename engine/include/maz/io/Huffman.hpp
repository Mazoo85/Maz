#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <string>
#include <vector>

// maz::io Huffman entropy coder — a self-contained, round-trip-exact byte compressor that assigns short
// bit codes to frequent bytes and long codes to rare ones (optimal prefix coding). It is the ENTROPY-
// coding companion to the engine's LZSS dictionary codec (Compression.hpp): LZSS removes repeated runs,
// Huffman squeezes skewed byte distributions, and the two together are exactly what "deflate" combines.
// Reach for Huffman on data with a lopsided byte histogram but few long repeats — packed tables, tile
// indices, quantised audio, text. The stream stores CANONICAL code lengths (one byte per present
// symbol) in its header, so the decoder rebuilds the identical code table deterministically with no
// separate model to ship. Godot bundles deflate/zstd but the engine only had a dictionary coder, so this
// adds the missing entropy path. Header-only, dependency-free, lossless.
namespace maz::io {

namespace detail {

// A node in the Huffman tree: leaves carry a byte symbol; internal nodes carry two child indices.
struct HuffNode {
    std::uint32_t freq = 0;
    int symbol = -1; // >= 0 for a leaf
    int left = -1;
    int right = -1;
    std::uint32_t order = 0; // deterministic tie-break
    int depth = 0;
};

// Build the optimal Huffman tree and return per-symbol code lengths (leaf depths). Deterministic.
inline std::array<std::uint8_t, 256>
huffmanCodeLengths(const std::vector<std::pair<int, std::uint32_t>>& leaves) {
    std::array<std::uint8_t, 256> lengths{};
    if (leaves.empty()) return lengths;
    if (leaves.size() == 1) {
        lengths[static_cast<std::size_t>(leaves[0].first)] = 1; // a lone symbol still needs one bit
        return lengths;
    }

    std::vector<HuffNode> nodes;
    nodes.reserve(leaves.size() * 2);
    std::uint32_t order = 0;
    auto cmp = [&nodes](int a, int b) {
        const HuffNode& na = nodes[static_cast<std::size_t>(a)];
        const HuffNode& nb = nodes[static_cast<std::size_t>(b)];
        if (na.freq != nb.freq) return na.freq > nb.freq;
        return na.order > nb.order;
    };
    std::priority_queue<int, std::vector<int>, decltype(cmp)> pq(cmp);
    for (const auto& [sym, freq] : leaves) {
        HuffNode n;
        n.freq = freq;
        n.symbol = sym;
        n.order = order++;
        nodes.push_back(n);
        pq.push(static_cast<int>(nodes.size()) - 1);
    }
    while (pq.size() > 1) {
        const int a = pq.top(); pq.pop();
        const int b = pq.top(); pq.pop();
        HuffNode parent;
        parent.freq = nodes[static_cast<std::size_t>(a)].freq + nodes[static_cast<std::size_t>(b)].freq;
        parent.left = a;
        parent.right = b;
        parent.order = order++;
        nodes.push_back(parent);
        pq.push(static_cast<int>(nodes.size()) - 1);
    }

    // Leaf depths via an explicit stack (avoids deep recursion for pathological trees).
    std::vector<std::pair<int, int>> stack; // (node, depth)
    stack.emplace_back(static_cast<int>(nodes.size()) - 1, 0);
    while (!stack.empty()) {
        const auto [ni, d] = stack.back();
        stack.pop_back();
        const HuffNode& n = nodes[static_cast<std::size_t>(ni)];
        if (n.symbol >= 0) {
            lengths[static_cast<std::size_t>(n.symbol)] = static_cast<std::uint8_t>(d < 1 ? 1 : d);
        } else {
            stack.emplace_back(n.left, d + 1);
            stack.emplace_back(n.right, d + 1);
        }
    }
    return lengths;
}

// Derive canonical codes from code lengths. codes[sym] holds the code value in its low lengths[sym] bits.
inline std::array<std::uint32_t, 256> canonicalCodes(const std::array<std::uint8_t, 256>& lengths) {
    std::array<std::uint32_t, 256> codes{};
    std::array<std::uint32_t, 33> blCount{};
    int maxLen = 0;
    for (std::uint8_t l : lengths) {
        ++blCount[l];
        if (l > maxLen) maxLen = l;
    }
    blCount[0] = 0;
    std::array<std::uint32_t, 33> nextCode{};
    std::uint32_t code = 0;
    for (int bits = 1; bits <= maxLen; ++bits) {
        code = (code + blCount[static_cast<std::size_t>(bits - 1)]) << 1;
        nextCode[static_cast<std::size_t>(bits)] = code;
    }
    for (int sym = 0; sym < 256; ++sym) {
        const std::uint8_t len = lengths[static_cast<std::size_t>(sym)];
        if (len > 0) codes[static_cast<std::size_t>(sym)] = nextCode[len]++;
    }
    return codes;
}

inline void put32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<std::uint8_t>((v >> 24) & 0xff));
}
inline std::uint32_t get32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8)
         | (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

} // namespace detail

// Compress `data` with canonical Huffman coding. Output layout: [origLen u32][numSymbols u32] then
// numSymbols x ([sym u8][codeLen u8]), then the bit-packed codes (MSB first). Empty input -> 8-byte
// header only.
inline std::vector<std::uint8_t> huffmanCompress(const std::uint8_t* data, std::size_t n) {
    using namespace detail;
    std::array<std::uint32_t, 256> freq{};
    for (std::size_t i = 0; i < n; ++i) ++freq[data[i]];

    std::vector<std::pair<int, std::uint32_t>> leaves;
    for (int s = 0; s < 256; ++s)
        if (freq[static_cast<std::size_t>(s)] > 0) leaves.emplace_back(s, freq[static_cast<std::size_t>(s)]);

    const std::array<std::uint8_t, 256> lengths = huffmanCodeLengths(leaves);
    const std::array<std::uint32_t, 256> codes = canonicalCodes(lengths);

    std::vector<std::uint8_t> out;
    put32(out, static_cast<std::uint32_t>(n));
    put32(out, static_cast<std::uint32_t>(leaves.size()));
    for (const auto& [sym, f] : leaves) {
        (void)f;
        out.push_back(static_cast<std::uint8_t>(sym));
        out.push_back(lengths[static_cast<std::size_t>(sym)]);
    }
    if (leaves.empty()) return out;

    // Bit-pack the codes MSB first.
    std::uint32_t cur = 0;
    int bits = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::size_t sym = data[i];
        const int len = lengths[sym];
        const std::uint32_t code = codes[sym];
        for (int b = len - 1; b >= 0; --b) {
            cur = (cur << 1) | ((code >> b) & 1u);
            if (++bits == 8) {
                out.push_back(static_cast<std::uint8_t>(cur & 0xff));
                cur = 0;
                bits = 0;
            }
        }
    }
    if (bits > 0) out.push_back(static_cast<std::uint8_t>((cur << (8 - bits)) & 0xff));
    return out;
}

inline std::vector<std::uint8_t> huffmanCompress(const std::vector<std::uint8_t>& data) {
    return huffmanCompress(data.data(), data.size());
}
inline std::vector<std::uint8_t> huffmanCompress(const std::string& s) {
    return huffmanCompress(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

// Decompress a stream produced by huffmanCompress. Malformed/truncated input decodes what it safely can.
inline std::vector<std::uint8_t> huffmanDecompress(const std::uint8_t* data, std::size_t n) {
    using namespace detail;
    std::vector<std::uint8_t> out;
    if (n < 8) return out;
    const std::uint32_t origLen = get32(data);
    const std::uint32_t numSyms = get32(data + 4);
    std::size_t pos = 8;

    std::array<std::uint8_t, 256> lengths{};
    for (std::uint32_t i = 0; i < numSyms; ++i) {
        if (pos + 2 > n) return out; // truncated header
        lengths[static_cast<std::size_t>(data[pos])] = data[pos + 1];
        pos += 2;
    }
    if (origLen == 0 || numSyms == 0) return out;
    const std::array<std::uint32_t, 256> codes = canonicalCodes(lengths);

    // Build a decode trie from the canonical codes.
    struct TrieNode { int child[2] = {-1, -1}; int sym = -1; };
    std::vector<TrieNode> trie(1);
    for (int sym = 0; sym < 256; ++sym) {
        const int len = lengths[static_cast<std::size_t>(sym)];
        if (len == 0) continue;
        const std::uint32_t code = codes[static_cast<std::size_t>(sym)];
        int node = 0;
        for (int b = len - 1; b >= 0; --b) {
            const int bit = static_cast<int>((code >> b) & 1u);
            if (trie[static_cast<std::size_t>(node)].child[bit] < 0) {
                trie.push_back(TrieNode{});
                trie[static_cast<std::size_t>(node)].child[bit] = static_cast<int>(trie.size()) - 1;
            }
            node = trie[static_cast<std::size_t>(node)].child[bit];
        }
        trie[static_cast<std::size_t>(node)].sym = sym;
    }

    out.reserve(origLen);
    // A single-symbol trie has its leaf one level below the root under bit 0.
    int node = 0;
    for (std::size_t bytePos = pos; bytePos < n && out.size() < origLen; ++bytePos) {
        const std::uint8_t byte = data[bytePos];
        for (int b = 7; b >= 0 && out.size() < origLen; --b) {
            const int bit = (byte >> b) & 1;
            const int nxt = trie[static_cast<std::size_t>(node)].child[bit];
            if (nxt < 0) break; // corrupt stream
            node = nxt;
            if (trie[static_cast<std::size_t>(node)].sym >= 0) {
                out.push_back(static_cast<std::uint8_t>(trie[static_cast<std::size_t>(node)].sym));
                node = 0;
            }
        }
    }
    return out;
}

inline std::vector<std::uint8_t> huffmanDecompress(const std::vector<std::uint8_t>& data) {
    return huffmanDecompress(data.data(), data.size());
}
inline std::string huffmanDecompressString(const std::vector<std::uint8_t>& data) {
    const std::vector<std::uint8_t> bytes = huffmanDecompress(data);
    return std::string(bytes.begin(), bytes.end());
}

} // namespace maz::io
