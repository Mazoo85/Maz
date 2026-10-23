// One tick is one hour. Subsystems run in a fixed order so a game replays the
// same on every machine.
#pragma once
#include "Api.h"
#include "GameState.h"

namespace warroom {

WARROOMSIM_API void Tick(GameState& s, const Data& data);
WARROOMSIM_API void Advance(GameState& s, const Data& data, long long hours);

}  // namespace warroom
