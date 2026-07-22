// tests/core/gapbuffer.cpp — verifies the text-editor gap buffer (core GapBuffer.hpp).
// Ground truths, deterministic (mirrored against a std::string + int cursor reference):
//   * typing, cursor movement, backspace, and forward-delete produce the expected text;
//   * inserting in the middle after moving the cursor works (the whole point of the gap);
//   * at()/size()/cursor() agree with the document;
//   * a randomized edit sequence (insert char/string, move, backspace, delete) matches the reference text
//     and cursor at every step, forcing several buffer growths.
#include "maz/core/GapBuffer.hpp"

#include <cstdint>
#include <cstdio>
#include <string>

using maz::core::GapBuffer;

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

struct Lcg {
    std::uint64_t s;
    std::uint32_t next() {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<std::uint32_t>(s >> 33);
    }
};

int main() {
    // --- 1. Basic typing + middle insert. ---
    {
        GapBuffer gb;
        gb.insert("hello world");
        CHECK(gb.text() == "hello world", "typed text");
        CHECK(gb.size() == 11 && gb.cursor() == 11, "size and cursor at end");
        gb.moveTo(5); // between "hello" and " world"
        gb.insert(",");
        CHECK(gb.text() == "hello, world", "middle insert via the gap");
        CHECK(gb.cursor() == 6, "cursor advanced past the inserted comma");
    }

    // --- 2. Backspace + forward delete. ---
    {
        GapBuffer gb("abcdef");
        gb.moveTo(3);          // cursor between c and d
        CHECK(gb.backspace(), "backspace removes 'c'");
        CHECK(gb.text() == "abdef", "text after backspace");
        CHECK(gb.deleteForward(), "forward-delete removes 'd'");
        CHECK(gb.text() == "abef", "text after forward delete");
        gb.moveTo(0);
        CHECK(!gb.backspace(), "backspace at start does nothing");
        gb.moveTo(gb.size());
        CHECK(!gb.deleteForward(), "forward-delete at end does nothing");
    }

    // --- 3. at() indexing. ---
    {
        GapBuffer gb("index");
        gb.moveTo(2);
        bool ok = true;
        const std::string ref = "index";
        for (std::size_t i = 0; i < gb.size(); ++i)
            if (gb.at(i) != ref[i]) ok = false;
        CHECK(ok, "at() returns the right character regardless of gap position");
        CHECK(gb.at(999) == '\0', "out-of-range at() returns NUL");
    }

    // --- 4. Randomized cross-check against a std::string reference. ---
    {
        GapBuffer gb;
        std::string ref;
        std::size_t cur = 0;
        Lcg rng{0xED170Bu};
        bool ok = true;
        for (int step = 0; step < 20000; ++step) {
            const std::uint32_t op = rng.next() % 6u;
            if (op == 0) { // insert a char
                const char c = static_cast<char>('a' + rng.next() % 26u);
                gb.moveTo(cur);
                gb.insert(c);
                ref.insert(ref.begin() + static_cast<std::ptrdiff_t>(cur), c);
                ++cur;
            } else if (op == 1) { // insert a short string
                std::string s;
                const std::uint32_t len = 1u + rng.next() % 5u;
                for (std::uint32_t k = 0; k < len; ++k) s.push_back(static_cast<char>('A' + rng.next() % 26u));
                gb.moveTo(cur);
                gb.insert(s);
                ref.insert(cur, s);
                cur += s.size();
            } else if (op == 2) { // move cursor
                cur = ref.empty() ? 0 : rng.next() % (static_cast<std::uint32_t>(ref.size()) + 1u);
                gb.moveTo(cur);
            } else if (op == 3) { // backspace
                gb.moveTo(cur);
                if (gb.backspace()) {
                    ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(cur - 1));
                    --cur;
                }
            } else if (op == 4) { // forward delete
                gb.moveTo(cur);
                if (gb.deleteForward()) {
                    ref.erase(ref.begin() + static_cast<std::ptrdiff_t>(cur));
                }
            } else { // move left/right
                if ((rng.next() & 1u) && cur > 0) --cur; else if (cur < ref.size()) ++cur;
                gb.moveTo(cur);
            }
            if (gb.text() != ref) { ok = false; break; }
            if (gb.size() != ref.size() || gb.cursor() != cur) { ok = false; break; }
        }
        CHECK(ok, "randomized edits match the std::string reference at every step");
    }

    if (g_fail == 0) {
        std::printf("gapbuffer: OK — typing, middle insert, backspace/delete, at(), random edits.\n");
        return 0;
    }
    std::printf("gapbuffer: %d failure(s).\n", g_fail);
    return 1;
}
