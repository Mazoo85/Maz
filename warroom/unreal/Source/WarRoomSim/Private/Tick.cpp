#include "WarRoomSim/Tick.h"

#include "WarRoomSim/Clock.h"
#include "WarRoomSim/Weather.h"

namespace warroom {

void Tick(GameState& s, const Data& data) {
    const long long hour = s.hour;
    if (IsNewDay(hour)) {
        TickWeather(s, data, hour);
    }
    s.hour = hour + 1;
}

void Advance(GameState& s, const Data& data, long long hours) {
    for (long long i = 0; i < hours; i++) Tick(s, data);
}

}  // namespace warroom
