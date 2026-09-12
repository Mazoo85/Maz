#pragma once

#include "maz/core/Pcg32.hpp"

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// maz::game character-level Markov chain — procedural NAME / word generation from example lists.
//
// Feed it a list of real names (elf names, town names, sci-fi surnames, potions...) and it learns the
// letter patterns — which letters tend to follow which short sequences — then invents NEW words that
// share that "flavour" without copying the inputs. This is the classic lightweight generator behind
// fantasy name makers and roguelike vocabularies. It is an order-`k` model: each next letter is chosen
// from the distribution that followed the previous `k` letters in the training data (higher order =
// closer to the source, lower = wilder). Start/end are handled with sentinel markers so generated words
// begin and end plausibly. Deterministic given a Pcg32 seed, so the same seed always yields the same
// name — unit-testable for reproducibility and for the invariant that every letter transition it emits
// was actually observed in training (it never invents unseen patterns). Header-only.
namespace maz::game {

class MarkovName {
public:
    explicit MarkovName(int order = 2) : order_(order < 1 ? 1 : order) {}

    // Learn from a list of example words. Can be called repeatedly to accumulate.
    void train(const std::vector<std::string>& words) {
        for (const std::string& w : words) {
            if (w.empty()) continue;
            std::string padded(static_cast<std::size_t>(order_), kStart);
            padded += w;
            padded += kEnd;
            for (std::size_t i = static_cast<std::size_t>(order_); i < padded.size(); ++i) {
                const std::string ctx = padded.substr(i - static_cast<std::size_t>(order_),
                                                      static_cast<std::size_t>(order_));
                trans_[ctx][padded[i]]++;
            }
        }
    }

    bool trained() const { return !trans_.empty(); }

    // True if `next` was ever observed following context `ctx` in the training data.
    bool hasTransition(const std::string& ctx, char next) const {
        const auto it = trans_.find(ctx);
        if (it == trans_.end()) return false;
        return it->second.find(next) != it->second.end();
    }

    int order() const { return order_; }

    // Generate one new word, drawing letters with `rng`. Bounded by `maxLen` letters (safety); returns
    // empty if the model is untrained or a dead-end context is hit immediately.
    std::string generate(core::Pcg32& rng, int maxLen = 24) const {
        if (trans_.empty()) return {};
        std::string ctx(static_cast<std::size_t>(order_), kStart);
        std::string out;
        for (int step = 0; step < maxLen; ++step) {
            const auto it = trans_.find(ctx);
            if (it == trans_.end()) break;
            const char next = sample(it->second, rng);
            if (next == kEnd) break;
            out += next;
            ctx.erase(ctx.begin());
            ctx += next;
        }
        return out;
    }

private:
    static constexpr char kStart = '\x02';
    static constexpr char kEnd = '\x03';

    // Weighted pick over an ordered {char -> count} map (ordered so sampling is deterministic per seed).
    static char sample(const std::map<char, int>& dist, core::Pcg32& rng) {
        std::uint32_t total = 0;
        for (const auto& kv : dist) total += static_cast<std::uint32_t>(kv.second);
        if (total == 0) return kEnd;
        std::uint32_t r = rng.nextBounded(total);
        for (const auto& kv : dist) {
            const std::uint32_t c = static_cast<std::uint32_t>(kv.second);
            if (r < c) return kv.first;
            r -= c;
        }
        return dist.rbegin()->first; // unreachable, guards rounding
    }

    int order_;
    std::unordered_map<std::string, std::map<char, int>> trans_;
};

} // namespace maz::game
