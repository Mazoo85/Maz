#include "WarRoomSim/Save.h"

namespace warroom {

static Json FlagJson(const Flag& f) { return Json{{"kind", f.kind}, {"colors", f.colors}}; }
static Json RngJson(const Rng& r) { return Json::array({r.s[0], r.s[1], r.s[2], r.s[3]}); }
static Json CmdJson(const Command& c) {
    Json j{{"type", c.type}};
    if (c.speed >= 0) j["speed"] = c.speed;
    if (c.hasTag) j["tag"] = c.tag;
    return j;
}

Json ToSave(const GameState& s) {
    Json st;
    st["version"] = s.version; st["seed"] = s.seed; st["scenario"] = s.scenario;
    st["hour"] = s.hour; st["speed"] = s.speed; st["lastSpeed"] = s.lastSpeed; st["player"] = s.player;
    st["rng"] = Json{{"weather", RngJson(s.rngWeather)}, {"events", RngJson(s.rngEvents)}};
    Json countries = Json::object();
    for (const auto& [tag, c] : s.countries) {
        countries[tag] = Json{{"tag", c.tag}, {"name", c.name}, {"adjective", c.adjective}, {"color", c.color}, {"ideology", c.ideology},
                              {"leader", c.leader}, {"capital", c.capital}, {"flag", FlagJson(c.flag)}, {"states", c.states}};
    }
    st["countries"] = std::move(countries);
    Json states = Json::object();
    for (const auto& [id, x] : s.states) {
        states[std::to_string(id)] = Json{{"owner", x.owner}, {"controller", x.controller}, {"cores", x.cores}, {"infrastructure", x.infrastructure}, {"manpower", x.manpower}};
    }
    st["states"] = std::move(states);
    Json regions = Json::object();
    for (const auto& [id, r] : s.regions) regions[std::to_string(id)] = Json{{"weather", r.weather}, {"weatherDays", r.weatherDays}};
    st["regions"] = std::move(regions);
    Json armies = Json::object();
    for (const auto& [tag, list] : s.armies) {
        Json arr = Json::array();
        for (const Army& a : list) arr.push_back(Json{{"state", a.state}, {"divisions", a.divisions}});
        armies[tag] = std::move(arr);
    }
    st["land"] = Json{{"armies", std::move(armies)}};
    for (const char* k : {"economy", "research", "politics", "diplomacy", "naval", "air", "intel"}) st[k] = Json::object();
    Json log = Json::array();
    for (const LogEntry& e : s.log) log.push_back(Json{{"hour", e.hour}, {"cmd", CmdJson(e.cmd)}});
    st["log"] = std::move(log);
    return Json{{"version", kSaveVersion}, {"state", std::move(st)}};
}

static bool ReadRng(const Json& j, Rng& r) {
    if (!j.is_array() || j.size() != 4) return false;
    for (int i = 0; i < 4; i++) { if (!j[i].is_number()) return false; r.s[i] = j[i].get<uint32_t>(); }
    return true;
}

std::string FromSave(const Json& save, const Data& data, GameState& out) {
    if (!save.is_object() || !save.contains("state")) return "not a War Room save";
    if (!save.contains("version") || !save["version"].is_number()) return "save has no version";
    const int version = save["version"].get<int>();
    if (version > kSaveVersion) return "this save is from a newer version of the game (" + std::to_string(version) + "); update the game";
    // (migrations from older versions go here)
    const Json& st = save["state"];
    GameState s;
    s.version = kSaveVersion;
    s.seed = st.value("seed", std::string()); s.scenario = st.value("scenario", std::string("1936"));
    s.hour = st.value("hour", 0LL); s.speed = st.value("speed", 0); s.lastSpeed = st.value("lastSpeed", 1);
    if (st.contains("player") && st["player"].is_string()) s.player = st["player"].get<std::string>();
    if (!st.contains("rng") || !ReadRng(st["rng"].value("weather", Json()), s.rngWeather) || !ReadRng(st["rng"].value("events", Json()), s.rngEvents)) return "save has no random streams";
    if (!st.contains("countries") || !st["countries"].is_object()) return "save has no countries";
    for (auto it = st["countries"].begin(); it != st["countries"].end(); ++it) {
        const Json& c = it.value();
        CountryState cs;
        cs.tag = c.value("tag", it.key()); cs.name = c.value("name", std::string()); cs.adjective = c.value("adjective", std::string());
        cs.color = c.value("color", std::string("#888888")); cs.ideology = c.value("ideology", std::string()); cs.leader = c.value("leader", std::string());
        cs.capital = c.value("capital", 0);
        if (c.contains("flag")) { cs.flag.kind = c["flag"].value("kind", std::string("plain")); if (c["flag"].contains("colors")) for (const auto& col : c["flag"]["colors"]) cs.flag.colors.push_back(col.get<std::string>()); }
        if (c.contains("states")) for (const auto& v : c["states"]) cs.states.push_back(v.get<int>());
        s.countries[it.key()] = std::move(cs);
    }
    if (!st.contains("states") || !st["states"].is_object()) return "save has no states";
    for (auto it = st["states"].begin(); it != st["states"].end(); ++it) {
        int id = std::atoi(it.key().c_str());
        if (!data.state(id)) return "save refers to unknown state " + it.key() + "; the map changed";
        const Json& x = it.value();
        StateState ss;
        ss.owner = x.value("owner", std::string()); ss.controller = x.value("controller", ss.owner);
        if (x.contains("cores")) for (const auto& t : x["cores"]) ss.cores.push_back(t.get<std::string>());
        ss.infrastructure = x.value("infrastructure", 1); ss.manpower = x.value("manpower", 0LL);
        s.states[id] = std::move(ss);
    }
    if (st.contains("regions") && st["regions"].is_object()) {
        for (auto it = st["regions"].begin(); it != st["regions"].end(); ++it) {
            RegionState r; r.weather = it.value().value("weather", std::string("clear")); r.weatherDays = it.value().value("weatherDays", 0);
            s.regions[std::atoi(it.key().c_str())] = r;
        }
    }
    for (const Region& r : data.regions) if (!s.regions.count(r.id)) s.regions[r.id] = RegionState{};
    if (st.contains("land") && st["land"].contains("armies")) {
        for (auto it = st["land"]["armies"].begin(); it != st["land"]["armies"].end(); ++it) {
            std::vector<Army> list;
            for (const auto& a : it.value()) list.push_back(Army{a.value("state", 0), a.value("divisions", 0)});
            s.armies[it.key()] = std::move(list);
        }
    }
    if (st.contains("log") && st["log"].is_array()) {
        for (const auto& e : st["log"]) {
            LogEntry le; le.hour = e.value("hour", 0LL);
            const Json& c = e.value("cmd", Json::object());
            le.cmd.type = c.value("type", std::string()); le.cmd.speed = c.value("speed", -1);
            if (c.contains("tag")) { le.cmd.hasTag = true; le.cmd.tag = c["tag"].is_null() ? "" : c["tag"].get<std::string>(); }
            s.log.push_back(std::move(le));
        }
    }
    out = std::move(s);
    return "";
}

std::string SaveToString(const GameState& s) { return ToSave(s).dump(); }

std::string LoadFromString(const std::string& text, const Data& data, GameState& out) {
    Json j = Json::parse(text, nullptr, false);
    if (j.is_discarded()) return "the save file is not valid JSON";
    return FromSave(j, data, out);
}

}  // namespace warroom
