// tests/ironfront/ai.cpp — the computer opponent's priorities, and that whole AI games always finish turns.
#include "check.hpp"
#include "ironfront/Ai.hpp"

#include <chrono>

using namespace ironfront;

static Game twoPower(const std::vector<std::string>& terrain, const std::vector<std::string>& owners) {
    Scenario s;
    s.map = Map::fromAscii(terrain, owners);
    Game g = Game::fromScenario(s, France, 3); // the "player" must be one of the two live powers
    g.alive = {true, false, true, false, false};
    return g;
}

static int addUnit(Game& g, UnitType t, int owner, int col, int row) {
    Unit u;
    u.id = g.nextId++;
    u.type = t;
    u.owner = owner;
    u.hex = offsetToHex(col, row);
    u.hp = stats(t).maxHp;
    g.world.units.push_back(u);
    return u.id;
}

int main() {
    {   // An undefended enemy capital in reach is taken at once, ending the war.
        Game g = twoPower({"......", "......"}, {"GGGFFF", "GGGFFF"});
        g.world.map.cities().push_back(City{"Westcap", offsetToHex(0, 0), Germany, true});
        g.world.map.cities().push_back(City{"Eastcap", offsetToHex(3, 0), France, true});
        g.world.map.cities().push_back(City{"Village", offsetToHex(1, 1), kNeutral, false});
        const int inf = addUnit(g, UnitType::Infantry, Germany, 2, 0);
        addUnit(g, UnitType::Infantry, France, 5, 1);
        const auto a = AiTurn().next(g);
        CHECK(a.has_value());
        CHECK(a && a->kind == AiAction::Move && a->unitId == inf && a->target == offsetToHex(3, 0));
        playAiTurn(g);
        CHECK(!g.alive[France] && g.winner() == Germany);
    }
    {   // Given a sure kill and a weak poke, it takes the kill.
        Game g = twoPower({"......", "......"}, {"GGGFFF", "GGGFFF"});
        g.world.map.cities().push_back(City{"Westcap", offsetToHex(0, 0), Germany, true});
        g.world.map.cities().push_back(City{"Eastcap", offsetToHex(5, 0), France, true});
        const int tank = addUnit(g, UnitType::Tank, Germany, 2, 0);
        const int weak = addUnit(g, UnitType::Infantry, France, 3, 0);
        const int fresh = addUnit(g, UnitType::Tank, France, 2, 1);
        g.world.unitById(weak)->hp = 3;
        (void)fresh;
        const auto a = AiTurn().next(g);
        CHECK(a && a->kind == AiAction::Attack && a->unitId == tank && a->target == offsetToHex(3, 0));
    }
    {   // A battered unit with an enemy close by falls back toward home.
        Game g = twoPower({"........", "........"}, {"GGGGFFFF", "GGGGFFFF"});
        g.world.map.cities().push_back(City{"Westcap", offsetToHex(0, 0), Germany, true});
        g.world.map.cities().push_back(City{"Eastcap", offsetToHex(7, 0), France, true});
        const int tank = addUnit(g, UnitType::Tank, Germany, 4, 0);
        addUnit(g, UnitType::Tank, France, 6, 0); // 2 hexes away: a threat, not yet attackable
        g.world.unitById(tank)->hp = 3;
        const int before = maz::game::hexDistance(g.world.unitById(tank)->hex, offsetToHex(0, 0));
        const auto a = AiTurn().next(g);
        CHECK(a && a->kind == AiAction::Move && a->unitId == tank);
        if (a) CHECK(maz::game::hexDistance(a->target, offsetToHex(0, 0)) < before);
    }
    {   // Healthy units advance on the enemy.
        Game g = twoPower({"..........", ".........."}, {"GGGGGFFFFF", "GGGGGFFFFF"});
        g.world.map.cities().push_back(City{"Westcap", offsetToHex(0, 0), Germany, true});
        g.world.map.cities().push_back(City{"Eastcap", offsetToHex(9, 0), France, true});
        const int inf = addUnit(g, UnitType::Infantry, Germany, 1, 0);
        const auto a = AiTurn().next(g);
        CHECK(a && a->kind == AiAction::Move && a->unitId == inf);
        if (a) CHECK(maz::game::hexDistance(a->target, offsetToHex(9, 0)) < 8);
    }
    {   // Whole AI-only games on the real map: every turn finishes, the state stays legal.
        const auto t0 = std::chrono::steady_clock::now();
        for (uint64_t seed = 1; seed <= 3; ++seed) {
            Game g = Game::newGame(Germany, seed);
            int steps = 0;
            while (!g.isOver() && g.turn <= 120) {
                int actions = 0;
                AiTurn turn;
                while (auto a = turn.next(g)) {
                    CHECK(applyAiAction(g, *a));
                    if (++actions > 60) break; // at most 2 per unit; 60 means a loop
                }
                CHECK(actions <= 60);
                g.endTurn();
                ++steps;
                for (const Unit& u : g.world.units) {
                    CHECK(u.hp > 0 && u.hp <= u.st().maxHp);
                    CHECK(g.alive[static_cast<std::size_t>(u.owner)]);
                    CHECK(g.world.map.inBounds(u.hex));
                    if (u.st().domain == Domain::Sea) CHECK(g.world.map.terrain(u.hex) == Terrain::Sea);
                }
                if (g_fail) break;
            }
            int cities = 0;
            for (int c = 0; c < CountryCount; ++c) cities += g.alive[static_cast<std::size_t>(c)] ? g.cityCount(c) : 0;
            std::printf("seed %llu: %d country-turns, round %d, %d powers left, %d cities held\n",
                        static_cast<unsigned long long>(seed), steps, g.turn, g.aliveCount(), cities);
            CHECK(g.aliveCount() >= 1);
        }
        const double s = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("AI soak took %.2fs\n", s);
    }
    return finish("ironfront_ai");
}
