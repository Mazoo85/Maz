// Maz Engine — "LOOKUP" (core::Trie, core::AhoCorasick, core::FuzzyMatch, core::JaroWinkler,
// core::DamerauLevenshtein, core::Utf8, core::Uuid, core::NumberFormat — finding and naming things,
// toward Godot's String helpers)
// Eight modules a game reaches for the moment it has a search box, a chat filter or a save file. LEFT: a
// prefix tree answering "what can this become?" as someone types, and an Aho-Corasick automaton finding
// every banned word in one pass over a sentence — one pass whatever the list length, which is why a chat
// filter uses it rather than a loop of finds. MIDDLE: the three ways to ask "did they mean this?", run
// over the same misspelling so the answers can be compared — a subsequence match that scores a command
// palette, an edit distance that counts keystrokes, and a similarity that knows a typo near the start is
// worse than one at the end. RIGHT: the small things that get a game wrong — counting characters in text
// that is not ASCII, and printing a number a person can read. Fixed inputs, no randomness beyond one
// seeded UUID. --headless / --frames N for CI.

#include "maz/Engine.hpp"

// None of the eight are in maz/Engine.hpp: that umbrella carries 153 of the engine's 692 headers.
#include "maz/core/AhoCorasick.hpp"
#include "maz/core/DamerauLevenshtein.hpp"
#include "maz/core/FuzzyMatch.hpp"
#include "maz/core/JaroWinkler.hpp"
#include "maz/core/NumberFormat.hpp"
#include "maz/core/Pcg32.hpp"
#include "maz/core/Trie.hpp"
#include "maz/core/Utf8.hpp"
#include "maz/core/Uuid.hpp"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_scancode.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

using namespace maz;

namespace {

std::string num(double v, int decimals = 3) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.*f", decimals, v);
    return buf;
}

std::string join(const std::vector<std::string>& v, const char* sep = ", ") {
    std::string s;
    for (const std::string& x : v) {
        if (!s.empty()) s += sep;
        s += x;
    }
    return s;
}

} // namespace

int main(int argc, char** argv) {
    core::AppConfig cfg = core::parseArgs(argc, argv);
    MAZ_LOG_INFO("LOOKUP starting");

    platform::Window window;
    platform::WindowConfig wc;
    wc.title = "Maz Engine — Lookup";
    wc.width = cfg.width;
    wc.height = cfg.height;
    wc.headless = cfg.headless;
    if (!window.init(wc)) {
        return 1;
    }

    render::RendererConfig rc;
    rc.vsync = cfg.vsync;
    rc.allowHeadless = cfg.headless;
    auto renderer = render::createVulkanRenderer();
    if (!renderer->init(window, rc)) {
        return 1;
    }

    platform::Input input;
    core::Clock clock(1.0 / 60.0);

    ui::Font font;
    {
        const char* base = SDL_GetBasePath();
        const std::string fontPath =
            (base ? std::string(base) : std::string()) + "assets/fonts/DejaVuSans.ttf";
        font.load(*renderer, fontPath.c_str(), 34.0f);
    }

    // ---- 1. A prefix tree behind a search box ----------------------------------------------------------
    core::Trie commands;
    for (const char* c : {"spawn", "spawn_wave", "spawn_boss", "save", "save_as", "settings",
                          "screenshot", "quit", "quicksave", "reload"}) {
        commands.insert(c);
    }
    const std::size_t commandCount = commands.size();
    const std::vector<std::string> afterS = commands.collectWithPrefix("s");
    const std::vector<std::string> afterSpawn = commands.collectWithPrefix("spawn");
    const std::size_t countQ = commands.countWithPrefix("q");
    const bool hasSave = commands.contains("save");
    const bool hasSav = commands.contains("sav");          // a prefix is not a word
    const bool anyStartsSav = commands.startsWith("sav");

    // ---- 2. Every banned word in one pass ---------------------------------------------------------------
    core::AhoCorasick filter;
    const std::vector<std::string> banned = {"noob", "lag", "cheat", "hax"};
    for (const std::string& b : banned) {
        filter.addPattern(b);
    }
    filter.build();

    const std::string chat = "that noob is a total cheat and the lag is haxed";
    const std::vector<core::AhoCorasick::Match> hits = filter.findAll(chat);
    std::vector<std::string> hitLines;
    for (const core::AhoCorasick::Match& h : hits) {
        hitLines.push_back(banned[static_cast<std::size_t>(h.pattern)] + " at " +
                           std::to_string(h.begin));
    }
    const bool cleanLine = !filter.containsAny("good game, well played");

    // ---- 3. Three ways to ask "did they mean this?" ------------------------------------------------------
    // The same misspelling against the same candidates, so the three metrics can be compared rather
    // than each demonstrated in isolation.
    const std::string typed = "sawn_wave";      // "spawn_wave" with the p dropped: one edit away
    struct Candidate {
        const char* word;
        core::FuzzyResult fuzzy;
        std::size_t edits;
        double similarity;
    };
    std::vector<Candidate> candidates;
    for (const char* w : {"spawn_wave", "spawn_boss", "save_as", "screenshot"}) {
        Candidate c;
        c.word = w;
        c.fuzzy = core::fuzzyMatch(typed, w);
        c.edits = core::damerauLevenshtein(typed, w);
        c.similarity = core::jaroWinkler(typed, w);
        candidates.push_back(c);
    }

    // Damerau counts a transposition as ONE edit where plain Levenshtein counts two — the difference
    // between "they swapped two keys" and "they got two characters wrong".
    const std::size_t swapEdits = core::damerauLevenshtein("form", "from");
    // Jaro-Winkler leans on a shared prefix, because a typo at the front is the worse kind.
    const double frontTypo = core::jaroWinkler("dwarf", "swarf");
    const double backTypo = core::jaroWinkler("dwarf", "dwarg");

    // ---- 4. The small things that get a game wrong -------------------------------------------------------
    // A player name with an accent and an emoji: bytes and characters are not the same number, and a
    // name field that counts bytes will cut someone's name in half.
    const std::string playerName = "Bj\xC3\xB6rn \xE2\x9A\x94\xEF\xB8\x8F";   // "Björn ⚔️"
    const std::size_t nameBytes = playerName.size();
    const std::size_t nameChars = core::utf8Length(playerName);
    const std::u32string decoded = core::utf8Decode(playerName);
    const bool utf8RoundTrip = core::utf8Encode(decoded) == playerName;

    core::Pcg32 rng(4242u, 7u);
    const std::string saveId = core::uuidV4String(rng);
    const bool uuidValid = core::isValidUuid(saveId);

    const std::string score = core::groupThousands(1234567);
    const std::string played = core::clockDuration(9045.0);
    const std::string playedShort = core::compactDuration(9045.0);
    const std::string gold = core::abbreviateNumber(2400000.0);

    const render::Color kText{0.92f, 0.95f, 1.0f, 1};
    const render::Color kDim{0.60f, 0.66f, 0.78f, 1};
    const render::Color kHead{1.0f, 0.80f, 0.45f, 1};
    const render::Color kVal{0.55f, 0.85f, 1.0f, 1};
    const render::Color kOk{0.50f, 0.95f, 0.60f, 1};
    const render::Color kNo{1.0f, 0.48f, 0.42f, 1};

    while (!window.shouldClose()) {
        window.pumpEvents(input);
        if (input.keyPressed(SDL_SCANCODE_ESCAPE)) {
            window.requestClose();
        }

        clock.beginFrame();
        while (clock.consumeFixedStep()) {
        }

        renderer->setClearColor(render::Color{0.07f, 0.08f, 0.11f, 1.0f});
        if (renderer->beginFrame()) {
            render::Camera2D cam;
            cam.usePixelSpace = true;
            renderer->setCamera2D(cam);

            const float sz = 0.29f;
            font.drawText(*renderer, 16.0f, 12.0f, "MAZ ENGINE  -  LOOKUP", kText, 0.6f);
            font.drawText(*renderer, 16.0f, 48.0f,
                          "Trie + Aho-Corasick + fuzzy matching + UTF-8 + UUIDs + number formatting",
                          kDim, 0.34f);

            auto row = [&](float x, float y, const char* label, const std::string& value,
                           render::Color colour) {
                font.drawText(*renderer, x, y, label, kDim, sz);
                font.drawText(*renderer, x + 215.0f, y, value.c_str(), colour, sz);
            };

            // ---- column 1: prefixes and the chat filter ----
            float y = 106.0f;
            font.drawText(*renderer, 24.0f, y, "core::Trie  -  A SEARCH BOX", kHead, 0.35f);
            y += 34.0f;
            row(24.0f, y, "commands", std::to_string(commandCount), kText); y += 25.0f;
            row(24.0f, y, "typing \"s\"", std::to_string(afterS.size()) + " offered", kVal); y += 25.0f;
            row(24.0f, y, "typing \"spawn\"", join(afterSpawn), kVal); y += 25.0f;
            row(24.0f, y, "typing \"q\"", std::to_string(countQ) + " offered", kVal); y += 25.0f;
            row(24.0f, y, "\"save\" is a command", hasSave ? "yes" : "no", hasSave ? kOk : kNo);
            y += 25.0f;
            row(24.0f, y, "\"sav\" is a command", hasSav ? "yes" : "no  (only a prefix)", kDim);
            y += 25.0f;
            row(24.0f, y, "anything under \"sav\"", anyStartsSav ? "yes" : "no", kOk); y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "answered in the length of what was typed, not the size of the list", kDim,
                          0.26f);

            y += 40.0f;
            font.drawText(*renderer, 24.0f, y, "core::AhoCorasick  -  CHAT FILTER", kHead, 0.35f);
            y += 32.0f;
            font.drawText(*renderer, 24.0f, y, ("\"" + chat + "\"").c_str(), kText, 0.26f);
            y += 26.0f;
            row(24.0f, y, "banned words", std::to_string(banned.size()), kDim); y += 25.0f;
            row(24.0f, y, "found", join(hitLines, "   "), kNo); y += 25.0f;
            row(24.0f, y, "a clean line", cleanLine ? "passes" : "FLAGGED", cleanLine ? kOk : kNo);
            y += 30.0f;
            font.drawText(*renderer, 24.0f, y,
                          "one pass over the text whatever the list length — not a loop of finds",
                          kDim, 0.26f);

            // ---- column 2: did they mean this? ----
            y = 106.0f;
            font.drawText(*renderer, 610.0f, y, "\"DID THEY MEAN THIS?\"", kHead, 0.35f);
            y += 30.0f;
            font.drawText(*renderer, 610.0f, y, ("they typed  \"" + typed + "\"").c_str(), kText, sz);
            y += 32.0f;
            font.drawText(*renderer, 610.0f, y, "candidate", kDim, 0.26f);
            font.drawText(*renderer, 790.0f, y, "fuzzy", kDim, 0.26f);
            font.drawText(*renderer, 890.0f, y, "edits", kDim, 0.26f);
            font.drawText(*renderer, 975.0f, y, "similar", kDim, 0.26f);
            y += 24.0f;
            for (const Candidate& c : candidates) {
                font.drawText(*renderer, 610.0f, y, c.word, kText, 0.27f);
                font.drawText(*renderer, 790.0f, y,
                              c.fuzzy.matched ? std::to_string(c.fuzzy.score).c_str() : "-",
                              c.fuzzy.matched ? kVal : kDim, 0.27f);
                font.drawText(*renderer, 890.0f, y, std::to_string(c.edits).c_str(), kVal, 0.27f);
                font.drawText(*renderer, 975.0f, y, num(c.similarity).c_str(), kVal, 0.27f);
                y += 24.0f;
            }
            y += 14.0f;
            font.drawText(*renderer, 610.0f, y,
                          "fuzzy scores a command palette, edits count keystrokes,", kDim, 0.26f);
            y += 22.0f;
            font.drawText(*renderer, 610.0f, y, "similarity ranks suggestions", kDim, 0.26f);

            y += 36.0f;
            font.drawText(*renderer, 610.0f, y, "WHY THREE AND NOT ONE", kHead, 0.32f);
            y += 30.0f;
            row(610.0f, y, "\"form\" -> \"from\"", std::to_string(swapEdits) + " edit", kOk); y += 25.0f;
            font.drawText(*renderer, 610.0f, y,
                          "a swap is one mistake; plain Levenshtein would say two", kDim, 0.26f);
            y += 30.0f;
            row(610.0f, y, "typo at the front", num(frontTypo), kNo); y += 25.0f;
            row(610.0f, y, "typo at the end", num(backTypo), kOk); y += 25.0f;
            font.drawText(*renderer, 610.0f, y,
                          "Jaro-Winkler rewards a shared prefix, because people get", kDim, 0.26f);
            y += 22.0f;
            font.drawText(*renderer, 610.0f, y, "the start of a word right", kDim, 0.26f);

            // ---- column 3: names, ids, numbers ----
            y = 106.0f;
            font.drawText(*renderer, 1120.0f, y, "core::Utf8", kHead, 0.35f);
            y += 34.0f;
            font.drawText(*renderer, 1120.0f, y, ("name: " + playerName).c_str(), kText, sz);
            y += 26.0f;
            row(1120.0f, y, "bytes", std::to_string(nameBytes), kNo); y += 25.0f;
            row(1120.0f, y, "characters", std::to_string(nameChars), kOk); y += 25.0f;
            row(1120.0f, y, "round trip", utf8RoundTrip ? "exact" : "LOSSY",
                utf8RoundTrip ? kOk : kNo); y += 30.0f;
            font.drawText(*renderer, 1120.0f, y, "a name field that counts bytes", kDim, 0.26f);
            y += 22.0f;
            font.drawText(*renderer, 1120.0f, y, "cuts someone's name in half", kDim, 0.26f);

            y += 40.0f;
            font.drawText(*renderer, 1120.0f, y, "core::Uuid", kHead, 0.35f);
            y += 32.0f;
            font.drawText(*renderer, 1120.0f, y, saveId.c_str(), kVal, 0.25f);
            y += 24.0f;
            row(1120.0f, y, "valid v4", uuidValid ? "yes" : "NO", uuidValid ? kOk : kNo); y += 30.0f;

            font.drawText(*renderer, 1120.0f, y, "core::NumberFormat", kHead, 0.35f);
            y += 32.0f;
            row(1120.0f, y, "score", score, kVal); y += 25.0f;
            row(1120.0f, y, "time played", played, kVal); y += 25.0f;
            row(1120.0f, y, "compact", playedShort, kVal); y += 25.0f;
            row(1120.0f, y, "gold", gold, kVal);

            font.drawText(*renderer, 24.0f, 664.0f,
                          "Every result above is computed by the engine from the inputs shown. Fixed "
                          "inputs and one seeded UUID, so the picture is the test.",
                          render::Color{0.58f, 0.62f, 0.70f, 1}, 0.28f);

            renderer->endFrame();
        }

        if (cfg.frames >= 0 && clock.frameCount() >= static_cast<uint64_t>(cfg.frames)) {
            window.requestClose();
        }
    }

    MAZ_LOG_INFO("LOOKUP shutting down (renderer %s)", renderer->isActive() ? "active" : "inactive");
    renderer->shutdown();
    window.shutdown();
    return 0;
}
