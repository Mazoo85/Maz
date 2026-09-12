#pragma once

#include <algorithm>
#include <string>
#include <vector>

// maz::game DICE NOTATION — parse and roll the classic tabletop dice strings ("2d6+3", "d20", "4d8-1") that RPGs,
// board-game ports, and loot/damage tables are written in. `parseDice` turns the text into a `DiceSpec` (dice
// count, sides, flat modifier); `rollDice` rolls it with any engine RNG that offers `range(lo,hi)` (e.g.
// `core::Pcg32`), returning the total and the individual dice; and `minRoll`/`maxRoll`/`averageRoll` give the
// distribution bounds without rolling (for tooltips, balancing, and AI expected-value decisions). Godot ships no
// dice parser, so this is a beyond-Godot gameplay utility. Header-only, std-only, deterministic (the randomness
// lives in the caller's seeded RNG).
namespace maz::game {

// A parsed dice expression: `count` dice of `sides` faces each, plus a flat `modifier`, and an optional keep rule
// `keep` (0 = keep all; > 0 = keep the highest `keep`; < 0 = keep the lowest `-keep`) for advantage/disadvantage
// and stat generation (e.g. "4d6kh3"). `valid` is false when the source string could not be parsed.
struct DiceSpec {
    int count = 1;
    int sides = 6;
    int modifier = 0;
    int keep = 0;
    bool valid = false;
};

// The outcome of a roll: the final `total` (kept dice + modifier), every die actually rolled in `rolls`, and the
// subset that counted toward the total in `kept` (== `rolls` when no keep rule is set).
struct RollResult {
    int total = 0;
    std::vector<int> rolls;
    std::vector<int> kept;
};

namespace detail {
// How many dice actually count, given a keep rule.
inline int keptCount(const DiceSpec& d) {
    const int k = d.keep < 0 ? -d.keep : d.keep;
    if (d.keep == 0) return d.count;
    return k < d.count ? k : d.count;
}
} // namespace detail

namespace detail {
// Read a run of ASCII digits starting at `i`; returns false if none. Saturates instead of overflowing.
inline bool readDiceInt(const std::string& s, std::size_t& i, std::size_t end, long& out) {
    const std::size_t start = i;
    long v = 0;
    while (i < end && s[i] >= '0' && s[i] <= '9') {
        v = v * 10 + (s[i] - '0');
        if (v > 100000000L) v = 100000000L;
        ++i;
    }
    if (i == start) return false;
    out = v;
    return true;
}
} // namespace detail

// Parse "[N]dM[+K|-K]" (case-insensitive 'd'): "2d6+3", "d20", "4d8-1", "10D10". Whitespace around the string is
// ignored. Returns a DiceSpec with `valid == false` on any malformed input (missing sides, trailing junk, a
// non-positive count or sides, a dangling sign).
inline DiceSpec parseDice(const std::string& str) {
    DiceSpec d;
    std::size_t b = 0, e = str.size();
    while (b < e && (str[b] == ' ' || str[b] == '\t')) ++b;
    while (e > b && (str[e - 1] == ' ' || str[e - 1] == '\t')) --e;
    std::size_t i = b;

    long count = 1;
    if (i < e && str[i] != 'd' && str[i] != 'D') {
        if (!detail::readDiceInt(str, i, e, count)) return d; // leading non-digit, non-'d'
    }
    if (i >= e || (str[i] != 'd' && str[i] != 'D')) return d; // require the 'd'
    ++i;

    long sides = 0;
    if (!detail::readDiceInt(str, i, e, sides)) return d; // sides are mandatory

    // Optional keep clause: "kh<K>" (keep highest) or "kl<K>" (keep lowest).
    long keep = 0;
    if (i < e && (str[i] == 'k' || str[i] == 'K')) {
        ++i;
        if (i >= e) return d;
        const char hl = str[i];
        if (hl != 'h' && hl != 'H' && hl != 'l' && hl != 'L') return d;
        ++i;
        long kv = 0;
        if (!detail::readDiceInt(str, i, e, kv)) return d; // keep count is mandatory
        if (kv < 1) return d;
        keep = (hl == 'l' || hl == 'L') ? -kv : kv;
    }

    long mod = 0;
    if (i < e) {
        const char sign = str[i];
        if (sign != '+' && sign != '-') return d;
        ++i;
        long m = 0;
        if (!detail::readDiceInt(str, i, e, m)) return d; // dangling sign
        mod = (sign == '-') ? -m : m;
    }
    if (i != e) return d; // trailing junk
    if (count < 1 || sides < 1) return d;

    d.count = static_cast<int>(count);
    d.sides = static_cast<int>(sides);
    d.modifier = static_cast<int>(mod);
    d.keep = static_cast<int>(keep);
    d.valid = true;
    return d;
}

// Lowest possible result: every KEPT die shows 1. (Invalid spec → 0.)
inline int minRoll(const DiceSpec& d) { return d.valid ? detail::keptCount(d) + d.modifier : 0; }

// Highest possible result: every KEPT die shows its max face. (Invalid spec → 0.)
inline int maxRoll(const DiceSpec& d) { return d.valid ? detail::keptCount(d) * d.sides + d.modifier : 0; }

// Expected (mean) result. Exact for no-keep specs (count*(sides+1)/2 + modifier); for keep rules it is computed by
// exact enumeration when the outcome space (sides^count) is small enough (≤ 4,000,000 — covers typical tabletop
// specs like 4d6kh3), and otherwise falls back to the no-selection-bias estimate keptCount*(sides+1)/2 + modifier.
// (Invalid spec → 0.)
inline double averageRoll(const DiceSpec& d) {
    if (!d.valid) return 0.0;
    if (d.keep == 0)
        return static_cast<double>(d.count) * (static_cast<double>(d.sides) + 1.0) * 0.5 +
               static_cast<double>(d.modifier);

    // Feasibility of exact enumeration.
    long long combos = 1;
    for (int i = 0; i < d.count; ++i) {
        combos *= d.sides;
        if (combos > 4000000) { combos = -1; break; }
    }
    const int kc = detail::keptCount(d);
    if (combos < 0)
        return static_cast<double>(kc) * (static_cast<double>(d.sides) + 1.0) * 0.5 +
               static_cast<double>(d.modifier);

    // Odometer over all outcomes; sum the kept faces of each.
    std::vector<int> face(static_cast<std::size_t>(d.count), 1);
    std::vector<int> sorted(static_cast<std::size_t>(d.count));
    double sumKept = 0.0;
    long long n = 0;
    while (true) {
        sorted = face;
        std::sort(sorted.begin(), sorted.end(), std::greater<int>()); // descending
        int keptSum = 0;
        if (d.keep > 0)
            for (int j = 0; j < kc; ++j) keptSum += sorted[static_cast<std::size_t>(j)];
        else
            for (int j = 0; j < kc; ++j) keptSum += sorted[static_cast<std::size_t>(d.count - 1 - j)];
        sumKept += keptSum;
        ++n;
        int p = 0;
        for (; p < d.count; ++p) {
            if (++face[static_cast<std::size_t>(p)] <= d.sides) break;
            face[static_cast<std::size_t>(p)] = 1;
        }
        if (p == d.count) break;
    }
    return sumKept / static_cast<double>(n) + static_cast<double>(d.modifier);
}

// Roll `d` with `rng` (any type exposing `int range(int lo, int hi)` inclusive, e.g. core::Pcg32). Returns the
// total and each die face. An invalid spec rolls to an empty result (total 0).
template <typename Rng>
inline RollResult rollDice(const DiceSpec& d, Rng& rng) {
    RollResult r;
    if (!d.valid) return r;
    r.rolls.reserve(static_cast<std::size_t>(d.count));
    for (int k = 0; k < d.count; ++k) r.rolls.push_back(rng.range(1, d.sides));

    if (d.keep == 0) {
        r.kept = r.rolls;
    } else {
        std::vector<int> sorted = r.rolls;
        std::sort(sorted.begin(), sorted.end(), std::greater<int>()); // descending
        const int kc = detail::keptCount(d);
        r.kept.reserve(static_cast<std::size_t>(kc));
        if (d.keep > 0)
            for (int j = 0; j < kc; ++j) r.kept.push_back(sorted[static_cast<std::size_t>(j)]);
        else
            for (int j = 0; j < kc; ++j) r.kept.push_back(sorted[static_cast<std::size_t>(d.count - 1 - j)]);
    }
    int sum = 0;
    for (int v : r.kept) sum += v;
    r.total = sum + d.modifier;
    return r;
}

// Convenience: parse and roll in one call. `out` (optional) receives the parsed spec so the caller can tell a
// failed parse (spec.valid == false) from a real roll.
template <typename Rng>
inline RollResult rollDice(const std::string& expr, Rng& rng, DiceSpec* out = nullptr) {
    const DiceSpec d = parseDice(expr);
    if (out) *out = d;
    return rollDice(d, rng);
}

} // namespace maz::game
