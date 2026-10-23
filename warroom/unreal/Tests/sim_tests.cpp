// Tests for the engine-independent simulation. Plain C++20, no Unreal needed:
//     make -C warroom/unreal/Tests && warroom/unreal/Tests/sim_tests warroom/data
// The same sources compile inside the Unreal module WarRoomSim.
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#include "WarRoomSim/WarRoomSim.h"

using namespace warroom;

static int passed = 0;
static std::vector<std::string> failures;
#define CHECK(cond, msg) do { if (!(cond)) { throw std::string(msg); } } while (0)
static void test(const std::string& name, const std::function<void()>& fn) {
    try { fn(); passed++; printf("  ok   %s\n", name.c_str()); }
    catch (const std::string& m) { failures.push_back(name + " — " + m); printf("  FAIL %s — %s\n", name.c_str(), m.c_str()); }
}

// A tiny world shaped like the real data: three countries, four states, eight land and two sea provinces.
static Data Fixture() {
    Data d;
    auto P = [&](int id, const char* name, const char* type, const char* terrain, int state, int region, bool coastal, int vp, double lat, double lon) {
        Province p; p.id = id; p.name = name; p.type = type; p.terrain = terrain; p.state = state; p.region = region; p.coastal = coastal; p.vp = vp; p.lat = lat; p.lon = lon; p.x = id; p.y = id; d.provinces.push_back(p);
    };
    d.provinces.push_back(Province{});
    P(1, "Aa", "land", "plains", 1, 1, true, 5, 50, 10); P(2, "Ab", "land", "forest", 1, 1, false, 0, 50, 11);
    P(3, "Ba", "land", "hills", 2, 1, true, 3, 50, 12); P(4, "Bb", "land", "plains", 2, 1, false, 0, 50, 13);
    P(5, "Ca", "land", "mountain", 3, 2, true, 10, 20, 10); P(6, "Cb", "land", "jungle", 3, 2, false, 0, 20, 11);
    P(7, "Da", "land", "desert", 4, 2, true, 1, 20, 12); P(8, "Db", "land", "urban", 4, 2, false, 0, 20, 13);
    P(9, "Sea 1", "sea", "ocean", 0, 3, false, 0, 50, 8); P(10, "Sea 2", "sea", "ocean", 0, 3, false, 0, 20, 8);
    d.adjacency = {{}, {2, 9}, {1, 4}, {4, 9}, {3, 2, 8}, {6, 10}, {5, 8}, {8, 10}, {7, 6, 4}, {1, 3, 10}, {5, 7, 9}};
    d.rivers.insert({1, 2});
    auto S = [&](int id, const char* name, const char* owner, std::vector<std::string> cores, std::vector<int> provs, int cap, long long mp, int infra) {
        StateDef s; s.id = id; s.name = name; s.owner = owner; s.cores = cores; s.provinces = provs; s.capital_province = cap; s.manpower = mp; s.infrastructure = infra; d.states.push_back(s);
    };
    S(1, "Alpha", "RED", {"RED"}, {1, 2}, 1, 100000, 3); S(2, "Beta", "RED", {"RED", "BLU"}, {3, 4}, 3, 50000, 2);
    S(3, "Gamma", "BLU", {"BLU"}, {5, 6}, 5, 80000, 2); S(4, "Delta", "GRN", {"GRN"}, {7, 8}, 7, 20000, 1);
    auto R = [&](int id, const char* name, const char* type, const char* climate, std::vector<int> provs) { Region r; r.id = id; r.name = name; r.type = type; r.climate = climate; r.provinces = provs; d.regions.push_back(r); };
    R(1, "North", "land", "continental", {1, 2, 3, 4}); R(2, "South", "land", "tropical", {5, 6, 7, 8}); R(3, "Western Sea", "sea", "ocean", {9, 10});
    auto C = [&](const char* tag, const char* name, const char* color, const char* ideo, int cs, int cp) { Country c; c.tag = tag; c.name = name; c.adjective = name; c.color = color; c.ideology = ideo; c.leader = "Leader"; c.capital_state = cs; c.capital_province = cp; c.flag.kind = "plain"; c.flag.colors = {color}; d.countries.push_back(c); };
    C("RED", "Redland", "#cc3333", "fascist", 1, 1); C("BLU", "Blueland", "#3333cc", "democratic", 3, 5); C("GRN", "Greenland", "#33aa33", "communist", 4, 7);
    // weather: a compact table with the same shape as data/common/weather.json
    auto W = [&](const char* climate, std::vector<std::pair<std::string, std::vector<std::pair<std::string, double>>>> from, std::map<std::string, std::array<double, 12>> season) {
        WeatherClimate c; for (auto& [k, v] : from) c.from[k] = v; c.season = season; d.weather.climates[climate] = c;
    };
    d.weather.types = kWeatherTypes;
    W("continental", {{"clear", {{"clear", 60}, {"rain", 22}, {"snow", 10}, {"frost", 8}}}, {"rain", {{"clear", 40}, {"rain", 40}, {"mud", 20}}}, {"snow", {{"clear", 30}, {"snow", 50}, {"frost", 20}}}, {"frost", {{"clear", 50}, {"frost", 30}, {"snow", 20}}}, {"mud", {{"clear", 50}, {"rain", 30}, {"mud", 20}}}},
      {{"snow", {3, 3, 2, 0.5, 0, 0, 0, 0, 0, 0.5, 2, 3}}, {"frost", {3, 3, 2, 0.5, 0, 0, 0, 0, 0, 1, 2, 3}}});
    W("tropical", {{"clear", {{"clear", 60}, {"rain", 35}, {"storm", 5}}}, {"rain", {{"clear", 40}, {"rain", 45}, {"storm", 15}}}, {"storm", {{"clear", 30}, {"rain", 50}, {"storm", 20}}}}, {});
    W("ocean", {{"clear", {{"clear", 70}, {"rain", 20}, {"storm", 10}}}, {"rain", {{"clear", 50}, {"rain", 40}, {"storm", 10}}}, {"storm", {{"clear", 40}, {"rain", 40}, {"storm", 20}}}}, {});
    Scenario sc; sc.start_hour = 0; sc.armies["RED"] = {Army{1, 12}}; sc.armies["BLU"] = {Army{3, 6}};
    d.scenarios["1936"] = sc;
    std::string err = IndexData(d);
    if (!err.empty()) { printf("fixture broken: %s\n", err.c_str()); std::exit(2); }
    return d;
}

int main(int argc, char** argv) {
    const std::string dataDir = argc > 1 ? argv[1] : "";

    test("rng: same seed, same sequence; different seeds differ; floats in range", [] {
        Rng a = SeedRng("war room"), b = SeedRng("war room");
        for (int i = 0; i < 100; i++) CHECK(NextU32(a) == NextU32(b), "sequences differ");
        Rng c = SeedRng("a"), d = SeedRng("b");
        int same = 0; for (int i = 0; i < 20; i++) if (NextU32(c) == NextU32(d)) same++;
        CHECK(same < 2, "different seeds matched");
        Rng e = SeedRng("f"); for (int i = 0; i < 10000; i++) { double f = NextFloat(e); CHECK(f >= 0 && f < 1, "float out of range"); }
        Rng g = SeedRng("dice"); int counts[6] = {0}; for (int i = 0; i < 6000; i++) counts[NextInt(g, 6)]++;
        for (int k : counts) CHECK(k > 850 && k < 1150, "dice not uniform");
    });

    test("clock: origin, leap day, round trips, formatting", [] {
        Date d = HourToDate(0);
        CHECK(d.year == 1936 && d.month == 1 && d.day == 1 && d.hour == 0 && d.weekday == 2, "hour 0");
        CHECK(IsLeapYear(1936) && !IsLeapYear(1937) && !IsLeapYear(1900) && IsLeapYear(2000), "leap years");
        long long h = DateToHour(1936, 2, 29, 12); Date e = HourToDate(h);
        CHECK(e.month == 2 && e.day == 29 && e.hour == 12 && HourToDate(h + 12).month == 3, "29 Feb 1936");
        for (long long x = -24 * 400; x < 24 * 366 * 13; x += 7919) { Date y = HourToDate(x); CHECK(DateToHour(y.year, y.month, y.day, y.hour) == x, "round trip " + std::to_string(x)); }
        CHECK(FormatDate(24 * 31) == "1 Feb 1936, 00:00", FormatDate(24 * 31));
        CHECK(FormatDate(DateToHour(1939, 9, 1, 4), true) == "1 September 1939, 04:00", "long format");
        CHECK(IsNewDay(24) && !IsNewDay(25) && IsNewMonth(24 * 31) && !IsNewMonth(24 * 30) && IsNewYear(24 * 366) && !IsNewYear(24 * 365), "flags");
    });

    Data fx = Fixture();

    test("new game builds countries, states, regions and armies", [&] {
        GameState s; CHECK(NewGame(fx, "x", "1936", s).empty(), "new game");
        CHECK(s.countries.size() == 3 && s.countries["RED"].states == std::vector<int>({1, 2}), "country states");
        CHECK(s.states[2].controller == "RED" && s.states[2].cores.size() == 2, "state 2");
        CHECK(s.regions[1].weather == "clear" && s.armies["RED"][0].divisions == 12, "regions/armies");
        CHECK(ProvinceController(s, fx, 3) == "RED" && ProvinceController(s, fx, 9).empty(), "controller");
        GameState bad; CHECK(!NewGame(fx, "x", "1066", bad).empty(), "unknown scenario refused");
    });

    test("commands validate, apply and log", [&] {
        GameState s; NewGame(fx, "x", "1936", s);
        Command c; c.type = "set_speed"; c.speed = 3; CHECK(ValidateCommand(s, c).empty(), "speed 3 ok");
        c.speed = 9; CHECK(!ValidateCommand(s, c).empty(), "speed 9 refused");
        c.speed = 4; CHECK(ApplyCommand(s, c).empty() && s.speed == 4, "apply speed 4");
        Command p; p.type = "pause"; ApplyCommand(s, p); CHECK(s.speed == 0, "paused");
        Command r; r.type = "resume"; ApplyCommand(s, r); CHECK(s.speed == 4, "resume remembers 4");
        Command sel; sel.type = "select_country"; sel.hasTag = true; sel.tag = "XXX"; CHECK(!ValidateCommand(s, sel).empty(), "unknown tag refused");
        sel.tag = "BLU"; ApplyCommand(s, sel); CHECK(s.player == "BLU", "player BLU");
        sel.tag = ""; ApplyCommand(s, sel); CHECK(s.player.empty(), "observer");
        Command nuke; nuke.type = "launch_nukes"; CHECK(!ApplyCommand(s, nuke).empty(), "unknown command refused");
        CHECK(s.log.size() == 5 && s.log[0].cmd.type == "set_speed", "log");
    });

    test("weather: continental snows, tropics never do, out of season weights vanish", [&] {
        GameState s; NewGame(fx, "snowy", "1936", s);
        int northSnow = 0, southSnow = 0, southRain = 0;
        for (int d = 0; d < 365; d++) { Advance(s, fx, 24); if (s.regions[1].weather == "snow") northSnow++; if (s.regions[2].weather == "snow") southSnow++; if (s.regions[2].weather == "rain") southRain++; }
        CHECK(northSnow > 5, "continental snow days " + std::to_string(northSnow));
        CHECK(southSnow == 0 && southRain > 60, "tropical weather");
        auto july = TransitionWeights(fx.weather, "continental", "clear", 7);
        for (auto& w : july) CHECK(w.first != "snow", "no snow in July");
        s.regions[2].weather = "storm"; CHECK(WeatherAt(s, fx, 6) == "storm" && WeatherAt(s, fx, 999) == "clear", "weatherAt");
    });

    test("determinism: same seed and commands give identical states; in pieces or at once", [&] {
        GameState a, b; NewGame(fx, "same", "1936", a); NewGame(fx, "same", "1936", b);
        Command c; c.type = "set_speed"; c.speed = 2;
        Advance(a, fx, 24 * 100); ApplyCommand(a, c); Advance(a, fx, 24 * 265);
        Advance(b, fx, 24 * 100); ApplyCommand(b, c); for (int i = 0; i < 265; i++) Advance(b, fx, 24);
        CHECK(SaveToString(a) == SaveToString(b) && a.hour == 24 * 365, "identical");
        GameState d; NewGame(fx, "other", "1936", d); Advance(d, fx, 24 * 365);
        CHECK(SaveToString(d) != SaveToString(a), "different seeds differ");
    });

    test("save: exact round trip; newer version and garbage refused; unknown state refused", [&] {
        GameState s; NewGame(fx, "rt", "1936", s); Advance(s, fx, 24 * 40);
        Command sel; sel.type = "select_country"; sel.hasTag = true; sel.tag = "GRN"; ApplyCommand(s, sel);
        GameState back; std::string err = LoadFromString(SaveToString(s), fx, back);
        CHECK(err.empty(), err);
        CHECK(SaveToString(back) == SaveToString(s), "round trip differs");
        Json j = ToSave(s); j["version"] = kSaveVersion + 1; GameState x;
        CHECK(FromSave(j, fx, x).find("newer") != std::string::npos, "newer refused");
        CHECK(!LoadFromString("{\"hello\":1}", fx, x).empty() && !LoadFromString("not json", fx, x).empty(), "garbage refused");
        Json k = ToSave(s); k["state"]["states"]["999"] = Json{{"owner", "RED"}};
        CHECK(FromSave(k, fx, x).find("unknown state 999") != std::string::npos, "unknown state refused");
    });

    if (!dataDir.empty()) {
        Data real;
        test("real data loads and cross-checks (" + dataDir + ")", [&] {
            std::string err = LoadData(dataDir, real);
            CHECK(err.empty(), err);
            CHECK(real.provinceCount > 10000 && real.states.size() > 900 && real.countries.size() >= 70, "world size");
        });
        if (!real.provinces.empty()) {
            test("real data: the great powers hold their capitals, 1936 borders", [&] {
                auto owner = [&](const char* name) { int best = 0; for (int i = 1; i <= real.provinceCount; i++) if (real.provinces[i].name == name && (!best || real.provinces[i].vp > real.provinces[best].vp)) best = i; return best ? real.state(real.provinces[best].state)->owner : std::string("?"); };
                CHECK(owner("Berlin") == "GER" && owner("Paris") == "FRA" && owner("Moscow") == "SOV" && owner("Tokyo") == "JAP" && owner("Washington") == "USA", "capitals");
                CHECK(owner("Kaliningrad") == "GER" && owner("Lviv") == "POL" && owner("Harbin") == "MAN" && owner("Vienna") == "AUS", "1936 borders");
                for (int a = 1; a <= real.provinceCount; a++) for (int b : real.adjacency[a]) { bool back = false; for (int c : real.adjacency[b]) if (c == a) back = true; CHECK(back, "adjacency not symmetric"); }
            });
            test("real data: a year of game time runs and saves", [&] {
                GameState s; CHECK(NewGame(real, "year", "1936", s).empty(), "new game");
                Advance(s, real, 24 * 365);
                CHECK(s.hour == 24 * 365, "hour");
                int snowy = 0; for (auto& [id, r] : s.regions) if (r.weather == "snow" || r.weather == "blizzard") snowy++;
                CHECK(snowy > 5, "some region has snow on 1 Jan 1937");
                GameState back; std::string err = LoadFromString(SaveToString(s), real, back);
                CHECK(err.empty() && SaveToString(back) == SaveToString(s), "save round trip on real data");
            });
        }
    }

    printf("\nsim tests: %d passed, %zu failed\n", passed, failures.size());
    for (auto& f : failures) printf("  - %s\n", f.c_str());
    return failures.empty() ? 0 : 1;
}
