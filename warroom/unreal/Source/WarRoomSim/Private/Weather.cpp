#include "WarRoomSim/Weather.h"

#include "WarRoomSim/Clock.h"

namespace warroom {

const std::vector<std::string> kWeatherTypes = {"clear", "rain", "storm", "snow", "blizzard", "sandstorm", "mud", "frost"};

std::vector<std::pair<std::string, double>> TransitionWeights(const WeatherTable& t, const std::string& climate,
                                                              const std::string& current, int month) {
    std::vector<std::pair<std::string, double>> out;
    auto c = t.climates.find(climate);
    if (c == t.climates.end()) { out.emplace_back("clear", 1.0); return out; }
    auto row = c->second.from.find(current);
    if (row == c->second.from.end()) row = c->second.from.find("clear");
    if (row == c->second.from.end()) { out.emplace_back("clear", 1.0); return out; }
    for (const auto& [type, w] : row->second) {
        double k = 1.0;
        auto season = c->second.season.find(type);
        if (season != c->second.season.end()) k = season->second[month - 1];
        double weight = w * k;
        if (weight > 0) out.emplace_back(type, weight);
    }
    if (out.empty()) out.emplace_back("clear", 1.0);
    return out;
}

void TickWeather(GameState& s, const Data& data, long long hour) {
    const int month = HourToDate(hour).month;
    for (const Region& r : data.regions) {
        RegionState& reg = s.regions[r.id];
        auto weights = TransitionWeights(data.weather, r.climate, reg.weather, month);
        double total = 0;
        for (const auto& w : weights) total += w.second;
        double roll = NextFloat(s.rngWeather) * total;
        std::string next = weights.back().first;
        for (const auto& [type, w] : weights) { roll -= w; if (roll < 0) { next = type; break; } }
        if (next == reg.weather) reg.weatherDays += 1;
        else { reg.weather = next; reg.weatherDays = 1; }
    }
    s.dirtyRegions = true;
}

std::string WeatherAt(const GameState& s, const Data& data, int provinceId) {
    const Province* p = data.province(provinceId);
    if (!p) return "clear";
    auto it = s.regions.find(p->region);
    return it == s.regions.end() ? "clear" : it->second.weather;
}

}  // namespace warroom
