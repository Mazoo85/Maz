// tests/game/markovname.cpp — verifies the character-level Markov name generator (MarkovName.hpp).
// Ground truths, deterministic:
//   * the same seed always produces the same name (reproducibility);
//   * different seeds produce a variety of names (not a constant);
//   * every generated word is VALID — each letter transition it emits was observed in training, so the
//     model never invents unseen patterns (the core Markov invariant);
//   * training only on words starting with a given letter yields names starting with that letter;
//   * an untrained model generates nothing.
#include "maz/game/MarkovName.hpp"

#include <cstdio>
#include <set>
#include <string>
#include <vector>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using maz::core::Pcg32;
using maz::game::MarkovName;

// Re-derive the padded transitions of a generated word and confirm every one was seen in training.
static bool allTransitionsKnown(const MarkovName& m, const std::string& word) {
    const int order = m.order();
    std::string padded(static_cast<size_t>(order), '\x02');
    padded += word;
    padded += '\x03';
    for (size_t i = static_cast<size_t>(order); i < padded.size(); ++i) {
        const std::string ctx = padded.substr(i - static_cast<size_t>(order), static_cast<size_t>(order));
        if (!m.hasTransition(ctx, padded[i])) return false;
    }
    return true;
}

int main() {
    const std::vector<std::string> elves = {"aelar", "aerin", "aramil", "aravae", "berrian",
                                            "carric", "erevan", "galinndan", "hadarai", "immeral",
                                            "soveliss", "thamior", "varis"};

    // --- 1. Determinism: same seed -> same name. ---
    {
        MarkovName m(2);
        m.train(elves);
        Pcg32 a(1234u, 1u), b(1234u, 1u);
        CHECK(m.generate(a) == m.generate(b), "same seed produces the same name");
    }

    // --- 2. Variety: many seeds give more than one distinct name. ---
    {
        MarkovName m(2);
        m.train(elves);
        std::set<std::string> names;
        for (uint32_t s = 0; s < 40; ++s) {
            Pcg32 rng(s, 7u);
            names.insert(m.generate(rng));
        }
        CHECK(names.size() > 3, "different seeds produce a variety of names");
    }

    // --- 3. Validity: every emitted transition was seen in training. ---
    {
        MarkovName m(2);
        m.train(elves);
        bool allValid = true;
        for (uint32_t s = 0; s < 60; ++s) {
            Pcg32 rng(s * 2654435761u + 1u, 3u);
            const std::string name = m.generate(rng);
            if (!name.empty() && !allTransitionsKnown(m, name)) allValid = false;
        }
        CHECK(allValid, "generated names only use letter transitions seen in training");
    }

    // --- 4. Starting-letter flavour carries through. ---
    {
        MarkovName m(2);
        m.train({"zephyr", "zenith", "zaltana", "zorander", "zima"});
        bool allZ = true;
        for (uint32_t s = 0; s < 30; ++s) {
            Pcg32 rng(s + 100u, 5u);
            const std::string name = m.generate(rng);
            if (!name.empty() && name[0] != 'z') allZ = false;
        }
        CHECK(allZ, "names trained only on 'z...' words start with 'z'");
    }

    // --- 5. Untrained model. ---
    {
        MarkovName m(2);
        Pcg32 rng(1u, 1u);
        CHECK(!m.trained(), "fresh model reports untrained");
        CHECK(m.generate(rng).empty(), "untrained model generates nothing");
    }

    if (g_fail == 0) {
        std::printf("markovname: OK — determinism, variety, transition validity, starting-letter "
                    "flavour, untrained.\n");
        return 0;
    }
    std::printf("markovname: %d failure(s).\n", g_fail);
    return 1;
}
