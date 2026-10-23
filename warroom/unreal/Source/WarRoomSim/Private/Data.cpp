#include "WarRoomSim/Data.h"

#include <fstream>
#include <sstream>

#include "WarRoomSim/Json.h"

namespace warroom {

static std::string ReadFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return "could not open " + path;
    std::ostringstream ss; ss << f.rdbuf();
    out = ss.str();
    return "";
}

static std::string ParseFile(const std::string& path, Json& out) {
    std::string text;
    std::string err = ReadFile(path, text);
    if (!err.empty()) return err;
    out = Json::parse(text, nullptr, false);
    if (out.is_discarded()) return path + " is not valid JSON";
    return "";
}

// Small typed getters that name the path on failure.
#define WR_FIELD(obj, key, path, getter, dest)                                                   \
    do {                                                                                          \
        auto it = (obj).find(key);                                                                \
        if (it == (obj).end()) return std::string(path) + "." + key + ": missing";                \
        if (!getter(*it, dest)) return std::string(path) + "." + key + ": wrong type";            \
    } while (0)

static bool GetStr(const Json& j, std::string& d) { if (!j.is_string()) return false; d = j.get<std::string>(); return true; }
static bool GetInt(const Json& j, int& d) { if (!j.is_number()) return false; d = j.get<int>(); return true; }
static bool GetLL(const Json& j, long long& d) { if (!j.is_number()) return false; d = j.get<long long>(); return true; }
static bool GetDbl(const Json& j, double& d) { if (!j.is_number()) return false; d = j.get<double>(); return true; }
static bool GetIntList(const Json& j, std::vector<int>& d) {
    if (!j.is_array()) return false;
    d.clear();
    for (const auto& v : j) { if (!v.is_number()) return false; d.push_back(v.get<int>()); }
    return true;
}
static bool GetStrList(const Json& j, std::vector<std::string>& d) {
    if (!j.is_array()) return false;
    d.clear();
    for (const auto& v : j) { if (!v.is_string()) return false; d.push_back(v.get<std::string>()); }
    return true;
}

static std::string ParseProvinces(const Json& j, Data& data) {
    if (!j.is_array()) return "provinces.json: expected a list";
    data.provinces.clear();
    data.provinces.push_back(Province{});
    int i = 0;
    for (const auto& o : j) {
        if (o.is_null()) continue;   // the browser build keeps a null at index 0
        std::string path = "provinces[" + std::to_string(i++) + "]";
        if (!o.is_object()) return path + ": expected object";
        Province p;
        WR_FIELD(o, "id", path, GetInt, p.id);
        WR_FIELD(o, "name", path, GetStr, p.name);
        WR_FIELD(o, "type", path, GetStr, p.type);
        WR_FIELD(o, "terrain", path, GetStr, p.terrain);
        WR_FIELD(o, "state", path, GetInt, p.state);
        WR_FIELD(o, "region", path, GetInt, p.region);
        WR_FIELD(o, "vp", path, GetInt, p.vp);
        WR_FIELD(o, "x", path, GetInt, p.x);
        WR_FIELD(o, "y", path, GetInt, p.y);
        WR_FIELD(o, "lat", path, GetDbl, p.lat);
        WR_FIELD(o, "lon", path, GetDbl, p.lon);
        if (auto c = o.find("coastal"); c != o.end() && c->is_boolean()) p.coastal = c->get<bool>();
        if (p.id != static_cast<int>(data.provinces.size())) return path + ": id " + std::to_string(p.id) + " is out of order; ids must be contiguous from 1";
        data.provinces.push_back(std::move(p));
    }
    data.provinceCount = static_cast<int>(data.provinces.size()) - 1;
    return "";
}

static std::string ParseStates(const Json& j, Data& data) {
    if (!j.is_array()) return "states.json: expected a list";
    data.states.clear();
    int i = 0;
    for (const auto& o : j) {
        std::string path = "states[" + std::to_string(i++) + "]";
        if (!o.is_object()) return path + ": expected object";
        StateDef s;
        WR_FIELD(o, "id", path, GetInt, s.id);
        WR_FIELD(o, "name", path, GetStr, s.name);
        WR_FIELD(o, "owner", path, GetStr, s.owner);
        WR_FIELD(o, "cores", path, GetStrList, s.cores);
        WR_FIELD(o, "provinces", path, GetIntList, s.provinces);
        WR_FIELD(o, "capital_province", path, GetInt, s.capital_province);
        WR_FIELD(o, "manpower", path, GetLL, s.manpower);
        WR_FIELD(o, "infrastructure", path, GetInt, s.infrastructure);
        data.states.push_back(std::move(s));
    }
    return "";
}

static std::string ParseRegions(const Json& j, Data& data) {
    if (!j.is_array()) return "regions.json: expected a list";
    data.regions.clear();
    int i = 0;
    for (const auto& o : j) {
        std::string path = "regions[" + std::to_string(i++) + "]";
        if (!o.is_object()) return path + ": expected object";
        Region r;
        WR_FIELD(o, "id", path, GetInt, r.id);
        WR_FIELD(o, "name", path, GetStr, r.name);
        WR_FIELD(o, "type", path, GetStr, r.type);
        WR_FIELD(o, "climate", path, GetStr, r.climate);
        WR_FIELD(o, "provinces", path, GetIntList, r.provinces);
        data.regions.push_back(std::move(r));
    }
    return "";
}

static std::string ParseCountries(const Json& j, Data& data) {
    if (!j.is_array()) return "countries.json: expected a list";
    data.countries.clear();
    int i = 0;
    for (const auto& o : j) {
        std::string path = "countries[" + std::to_string(i++) + "]";
        if (!o.is_object()) return path + ": expected object";
        Country c;
        WR_FIELD(o, "tag", path, GetStr, c.tag);
        WR_FIELD(o, "name", path, GetStr, c.name);
        WR_FIELD(o, "adjective", path, GetStr, c.adjective);
        WR_FIELD(o, "color", path, GetStr, c.color);
        WR_FIELD(o, "ideology", path, GetStr, c.ideology);
        WR_FIELD(o, "leader", path, GetStr, c.leader);
        WR_FIELD(o, "capital_state", path, GetInt, c.capital_state);
        WR_FIELD(o, "capital_province", path, GetInt, c.capital_province);
        auto f = o.find("flag");
        if (f == o.end() || !f->is_object()) return path + ".flag: missing";
        WR_FIELD(*f, "kind", path + ".flag", GetStr, c.flag.kind);
        WR_FIELD(*f, "colors", path + ".flag", GetStrList, c.flag.colors);
        data.countries.push_back(std::move(c));
    }
    return "";
}

static std::string ParseWeather(const Json& j, Data& data) {
    if (!j.is_object()) return "weather.json: expected object";
    WR_FIELD(j, "types", "weather", GetStrList, data.weather.types);
    auto cl = j.find("climates");
    if (cl == j.end() || !cl->is_object()) return "weather.climates: missing";
    data.weather.climates.clear();
    for (auto it = cl->begin(); it != cl->end(); ++it) {
        WeatherClimate c;
        const Json& from = it.value().value("from", Json::object());
        for (auto f = from.begin(); f != from.end(); ++f) {
            std::vector<std::pair<std::string, double>> row;
            for (auto t = f.value().begin(); t != f.value().end(); ++t) row.emplace_back(t.key(), t.value().get<double>());
            c.from[f.key()] = std::move(row);
        }
        const Json& season = it.value().value("season", Json::object());
        for (auto s = season.begin(); s != season.end(); ++s) {
            if (!s.value().is_array() || s.value().size() != 12) return "weather.climates." + it.key() + ".season." + s.key() + ": needs 12 numbers";
            std::array<double, 12> arr{};
            for (int m = 0; m < 12; m++) arr[m] = s.value()[m].get<double>();
            c.season[s.key()] = arr;
        }
        data.weather.climates[it.key()] = std::move(c);
    }
    return "";
}

static std::string ParseScenario(const Json& j, const std::string& name, Data& data) {
    if (!j.is_object()) return "scenario " + name + ": expected object";
    Scenario sc;
    WR_FIELD(j, "start_hour", "scenario " + name, GetLL, sc.start_hour);
    auto a = j.find("armies");
    if (a == j.end() || !a->is_object()) return "scenario " + name + ".armies: missing";
    for (auto it = a->begin(); it != a->end(); ++it) {
        std::vector<Army> list;
        if (!it.value().is_array()) return "scenario " + name + ".armies." + it.key() + ": expected list";
        for (const auto& o : it.value()) {
            Army ar;
            WR_FIELD(o, "state", "scenario " + name + ".armies." + it.key(), GetInt, ar.state);
            WR_FIELD(o, "divisions", "scenario " + name + ".armies." + it.key(), GetInt, ar.divisions);
            list.push_back(ar);
        }
        sc.armies[it.key()] = std::move(list);
    }
    data.scenarios[name] = std::move(sc);
    return "";
}

std::string LoadData(const std::string& dirIn, Data& data) {
    std::string dir = dirIn;
    if (!dir.empty() && dir.back() != '/' && dir.back() != '\\') dir += '/';
    Json j;
    std::string err;
    if (!(err = ParseFile(dir + "map/provinces.json", j)).empty()) return err;
    if (!(err = ParseProvinces(j, data)).empty()) return err;
    if (!(err = ParseFile(dir + "map/adjacency.json", j)).empty()) return err;
    if (!j.is_array()) return "adjacency.json: expected a list";
    data.adjacency.clear();
    for (const auto& row : j) { std::vector<int> v; if (!GetIntList(row, v)) return "adjacency.json: expected lists of ids"; data.adjacency.push_back(std::move(v)); }
    if (!(err = ParseFile(dir + "map/rivers.json", j)).empty()) return err;
    data.rivers.clear();
    for (const auto& pair : j) { if (!pair.is_array() || pair.size() != 2) return "rivers.json: expected pairs"; int a = pair[0].get<int>(), b = pair[1].get<int>(); data.rivers.insert(a < b ? std::make_pair(a, b) : std::make_pair(b, a)); }
    if (!(err = ParseFile(dir + "map/states.json", j)).empty()) return err;
    if (!(err = ParseStates(j, data)).empty()) return err;
    if (!(err = ParseFile(dir + "map/regions.json", j)).empty()) return err;
    if (!(err = ParseRegions(j, data)).empty()) return err;
    if (!(err = ParseFile(dir + "common/countries.json", j)).empty()) return err;
    if (!(err = ParseCountries(j, data)).empty()) return err;
    if (!(err = ParseFile(dir + "common/weather.json", j)).empty()) return err;
    if (!(err = ParseWeather(j, data)).empty()) return err;
    if (!(err = ParseFile(dir + "scenarios/1936.json", j)).empty()) return err;
    if (!(err = ParseScenario(j, "1936", data)).empty()) return err;
    return IndexData(data);
}

std::string IndexData(Data& data) {
    data.provinceCount = static_cast<int>(data.provinces.size()) - 1;
    data.stateById.clear(); data.regionById.clear(); data.byTag.clear();
    for (const auto& s : data.states) {
        if (data.stateById.count(s.id)) return "duplicate state id " + std::to_string(s.id);
        data.stateById[s.id] = &s;
        for (int pid : s.provinces) {
            const Province* p = data.province(pid);
            if (!p) return "state " + std::to_string(s.id) + " \"" + s.name + "\" lists unknown province " + std::to_string(pid);
            if (p->state != s.id) return "province " + std::to_string(pid) + " says state " + std::to_string(p->state) + " but state " + std::to_string(s.id) + " lists it";
        }
        if (!data.province(s.capital_province)) return "state " + std::to_string(s.id) + " capital province " + std::to_string(s.capital_province) + " does not exist";
    }
    for (int i = 1; i <= data.provinceCount; i++) {
        const Province& p = data.provinces[i];
        if (p.type == "land" && !data.state(p.state)) return "land province " + std::to_string(i) + " \"" + p.name + "\" has no state";
    }
    for (const auto& r : data.regions) {
        data.regionById[r.id] = &r;
        for (int pid : r.provinces) {
            const Province* p = data.province(pid);
            if (!p) return "region " + std::to_string(r.id) + " lists unknown province " + std::to_string(pid);
            if (p->region != r.id) return "province " + std::to_string(pid) + " says region " + std::to_string(p->region) + " but region " + std::to_string(r.id) + " lists it";
        }
        if (!data.weather.climates.count(r.climate)) return "region " + std::to_string(r.id) + " \"" + r.name + "\" has climate \"" + r.climate + "\" with no weather table";
    }
    for (const auto& c : data.countries) {
        if (data.byTag.count(c.tag)) return "duplicate country tag " + c.tag;
        data.byTag[c.tag] = &c;
        if (!data.state(c.capital_state)) return "country " + c.tag + " capital state " + std::to_string(c.capital_state) + " does not exist";
        if (!data.province(c.capital_province)) return "country " + c.tag + " capital province does not exist";
    }
    for (const auto& s : data.states) {
        if (!data.country(s.owner)) return "state " + std::to_string(s.id) + " \"" + s.name + "\" owner \"" + s.owner + "\" is not a country";
        for (const auto& t : s.cores) if (!data.country(t)) return "state " + std::to_string(s.id) + " core \"" + t + "\" is not a country";
    }
    if (data.adjacency.size() != data.provinces.size()) return "adjacency has " + std::to_string(data.adjacency.size()) + " entries for " + std::to_string(data.provinces.size()) + " provinces";
    for (auto& [name, sc] : data.scenarios) {
        for (auto& [tag, armies] : sc.armies) {
            if (!data.country(tag)) return "scenario " + name + ": armies for unknown country " + tag;
            for (const Army& a : armies) if (!data.state(a.state)) return "scenario " + name + ": " + tag + " army in unknown state " + std::to_string(a.state);
        }
    }
    return "";
}

}  // namespace warroom
