// tests/ironfront/save.cpp — a saved game loads back exactly, and junk is rejected.
#include "check.hpp"
#include "ironfront/Ai.hpp"
#include "ironfront/Save.hpp"

using namespace ironfront;

int main() {
    Game g = Game::newGame(France, 99);
    for (int i = 0; i < 17; ++i) playAiTurn(g); // a few rounds in: things have moved and died
    // Mid-turn too: one unit has acted.
    AiTurn t;
    t.step(g);

    const std::string text = serialize(g);
    const auto loaded = deserialize(text);
    CHECK(loaded.has_value());
    if (loaded) {
        CHECK(serialize(*loaded) == text);
        CHECK(loaded->player == France && loaded->turn == g.turn && loaded->current == g.current);
        CHECK(loaded->world.units.size() == g.world.units.size());
        CHECK(loaded->alive == g.alive);
        // The same future: both copies play on identically.
        Game a = *loaded, b = g;
        for (int i = 0; i < 6; ++i) {
            playAiTurn(a);
            playAiTurn(b);
        }
        CHECK(serialize(a) == serialize(b));
    }

    CHECK(!deserialize("").has_value());
    CHECK(!deserialize("hello world").has_value());
    CHECK(!deserialize("ironfront 999\n").has_value());
    std::string truncated = text.substr(0, text.size() / 2);
    CHECK(!deserialize(truncated).has_value());
    std::string badUnit = text;
    badUnit.replace(badUnit.find("units"), 5, "unitz");
    CHECK(!deserialize(badUnit).has_value());
    return finish("ironfront_save");
}
