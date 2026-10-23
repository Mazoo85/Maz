#include "WarRoomSim/GameState.h"

namespace warroom {

std::string NewGame(const Data& data, const std::string& seed, const std::string& scenario, GameState& s) {
    auto sc = data.scenarios.find(scenario);
    if (sc == data.scenarios.end()) return "unknown scenario \"" + scenario + "\"";
    s = GameState{};
    s.seed = seed;
    s.scenario = scenario;
    s.hour = sc->second.start_hour;
    s.rngWeather = SeedStream(seed, "weather");
    s.rngEvents = SeedStream(seed, "events");
    for (const Country& c : data.countries) {
        CountryState cs;
        cs.tag = c.tag; cs.name = c.name; cs.adjective = c.adjective; cs.color = c.color;
        cs.ideology = c.ideology; cs.leader = c.leader; cs.capital = c.capital_state; cs.flag = c.flag;
        s.countries[c.tag] = std::move(cs);
    }
    for (const StateDef& d : data.states) {
        auto owner = s.countries.find(d.owner);
        if (owner == s.countries.end()) return "state " + std::to_string(d.id) + " \"" + d.name + "\" has unknown owner \"" + d.owner + "\"";
        StateState ss;
        ss.owner = d.owner; ss.controller = d.owner; ss.cores = d.cores;
        ss.infrastructure = d.infrastructure; ss.manpower = d.manpower;
        s.states[d.id] = std::move(ss);
        owner->second.states.push_back(d.id);
    }
    for (const Region& r : data.regions) s.regions[r.id] = RegionState{};
    for (const auto& [tag, list] : sc->second.armies) {
        if (!s.countries.count(tag)) return "scenario army for unknown country \"" + tag + "\"";
        s.armies[tag] = list;
    }
    return "";
}

std::string ProvinceController(const GameState& s, const Data& data, int provinceId) {
    const Province* p = data.province(provinceId);
    if (!p || p->state == 0) return "";
    auto it = s.states.find(p->state);
    return it == s.states.end() ? "" : it->second.controller;
}

}  // namespace warroom
