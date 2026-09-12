#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// maz::film noise — the stable pseudo-random numbers the sets are built from, ported from
// makeRng()/hashText() in film/js/parse.js and noise() in film/js/film-art.js.
//
// Every set scatters something: stars over a field, bottles behind a bar, rust on a warehouse wall,
// glints on the sea. None of it may be actually random. A film is identified by its seed, and "one
// idea and one seed give one film" is the promise the whole arcade rests on -- so the scatter comes
// from a hash of the shot's set name and the film's seed, and a frame drawn twice is drawn the same.
//
// This has to produce the SAME numbers as the browser, bit for bit, or a natively drawn set is a
// different room with the furniture moved. Two JavaScript details therefore matter and are spelled
// out rather than approximated:
//
//   * Math.imul is a 32-bit multiply that KEEPS THE LOW 32 BITS and discards the overflow, which is
//     exactly what unsigned 32-bit arithmetic does in C++ -- but nothing like what a 64-bit or
//     floating-point multiply does. Every multiply below is deliberately on std::uint32_t.
//   * `>>>` is an UNSIGNED shift, and `|0` and `^` coerce back to a 32-bit integer, so an addition
//     that overflowed into a double gets truncated to its low 32 bits anyway. Doing the whole thing
//     in uint32 lands on the same bits by a shorter route.
namespace maz::film {

// FNV-1a over the string's bytes. The set keys are ASCII, where JavaScript's charCodeAt and a byte
// agree; a non-ASCII key would need UTF-16 code units to match, and none is ever used.
inline std::uint32_t hashText(const std::string& text) {
    std::uint32_t h = 0x811c9dc5u;
    for (const char c : text) {
        h ^= static_cast<std::uint32_t>(static_cast<unsigned char>(c));
        h *= 0x01000193u; // wraps, as Math.imul does
    }
    return h;
}

// The same small counter-based generator the browser uses (mulberry32). Deterministic, and cheap
// enough to run per shot.
class Rng {
public:
    explicit Rng(std::uint32_t seed) : m_a(seed) {}

    // The next value in [0, 1).
    double next() {
        m_a += 0x6d2b79f5u;
        std::uint32_t t = (m_a ^ (m_a >> 15)) * (1u | m_a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return static_cast<double>(t ^ (t >> 14)) / 4294967296.0;
    }

private:
    std::uint32_t m_a;
};

// Three numbers per entry: the sets use them as an x, a y and a size or a brightness.
using NoiseTriple = std::array<double, 3>;

// `count` stable triples for a key. The browser memoizes these; a shot's worth is cheap enough that
// the caller can decide whether to.
inline std::vector<NoiseTriple> noise(const std::string& key, int count) {
    std::vector<NoiseTriple> out;
    if (count < 1) {
        return out;
    }
    Rng rng(hashText(key));
    out.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
        // Evaluation order matters: three calls, in this order, per entry.
        const double a = rng.next();
        const double b = rng.next();
        const double c = rng.next();
        out.push_back(NoiseTriple{a, b, c});
    }
    return out;
}

// The key a set's scatter is drawn from, so a room is furnished the same way every time it is cut
// back to, and differently in a different film.
inline std::string noiseKey(const std::string& set, std::uint32_t seed) {
    return "set-" + set + "-" + std::to_string(seed);
}

} // namespace maz::film
