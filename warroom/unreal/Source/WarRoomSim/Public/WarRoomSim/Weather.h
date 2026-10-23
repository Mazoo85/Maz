// Daily weather per strategic region, rolled from a transition table per climate
// and biased by the month.
#pragma once
#include "Api.h"
#include <string>
#include <utility>
#include <vector>

#include "GameState.h"

namespace warroom {

extern WARROOMSIM_API const std::vector<std::string> kWeatherTypes;

std::vector<std::pair<std::string, double>> TransitionWeights(const WeatherTable& t, const std::string& climate,
                                                              const std::string& current, int month);
WARROOMSIM_API void TickWeather(GameState& s, const Data& data, long long hour);
WARROOMSIM_API std::string WeatherAt(const GameState& s, const Data& data, int provinceId);

}  // namespace warroom
