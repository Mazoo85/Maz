// The game's static data: the map, the countries, the tables. Loaded once from
// the JSON files the pipeline writes, checked, and never changed by play.
#pragma once
#include "Api.h"
#include <array>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace warroom {

struct Province {
    int id = 0;
    std::string name, type, terrain;
    int state = 0, region = 0;
    bool coastal = false;
    int vp = 0;
    int x = 0, y = 0;
    double lat = 0, lon = 0;
};

struct StateDef {
    int id = 0;
    std::string name, owner;
    std::vector<std::string> cores;
    std::vector<int> provinces;
    int capital_province = 0;
    long long manpower = 0;
    int infrastructure = 1;
};

struct Region {
    int id = 0;
    std::string name, type, climate;
    std::vector<int> provinces;
};

struct Flag {
    std::string kind;
    std::vector<std::string> colors;
};

struct Country {
    std::string tag, name, adjective, color, ideology, leader;
    int capital_state = 0, capital_province = 0;
    Flag flag;
};

struct WeatherClimate {
    // from[current] = list of (next weather, weight)
    std::map<std::string, std::vector<std::pair<std::string, double>>> from;
    std::map<std::string, std::array<double, 12>> season;
};

struct WeatherTable {
    std::vector<std::string> types;
    std::map<std::string, WeatherClimate> climates;
};

struct Army {
    int state = 0;
    int divisions = 0;
};

struct Scenario {
    long long start_hour = 0;
    std::map<std::string, std::vector<Army>> armies;
};

struct Data {
    std::vector<Province> provinces;             // indexed by id; [0] is unused
    int provinceCount = 0;
    std::vector<std::vector<int>> adjacency;     // indexed by id
    std::set<std::pair<int, int>> rivers;        // (lo, hi)
    std::vector<StateDef> states;
    std::map<int, const StateDef*> stateById;
    std::vector<Region> regions;
    std::map<int, const Region*> regionById;
    std::vector<Country> countries;
    std::map<std::string, const Country*> byTag;
    WeatherTable weather;
    std::map<std::string, Scenario> scenarios;

    const Province* province(int id) const { return (id > 0 && id <= provinceCount) ? &provinces[id] : nullptr; }
    const StateDef* state(int id) const { auto it = stateById.find(id); return it == stateById.end() ? nullptr : it->second; }
    const Region* region(int id) const { auto it = regionById.find(id); return it == regionById.end() ? nullptr : it->second; }
    const Country* country(const std::string& tag) const { auto it = byTag.find(tag); return it == byTag.end() ? nullptr : it->second; }
    bool isRiver(int a, int b) const { return rivers.count(a < b ? std::make_pair(a, b) : std::make_pair(b, a)) > 0; }
};

// Reads <dir>/map/*.json, <dir>/common/*.json and <dir>/scenarios/*.json.
// Returns an empty string on success, else a message naming the file and field.
WARROOMSIM_API std::string LoadData(const std::string& dir, Data& out);

// Builds the indexes and cross-checks references; LoadData calls it, tests use it
// directly on hand-built data. Returns "" or the problem.
WARROOMSIM_API std::string IndexData(Data& data);

}  // namespace warroom
