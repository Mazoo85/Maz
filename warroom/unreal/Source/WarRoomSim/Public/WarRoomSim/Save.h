// Saves: { version, state } as JSON, with migrations for older versions.
#pragma once
#include "Api.h"
#include <string>

#include "GameState.h"
#include "Json.h"

namespace warroom {

constexpr int kSaveVersion = kStateVersion;

WARROOMSIM_API Json ToSave(const GameState& s);
// Returns "" or an error message; `out` is only valid on success.
WARROOMSIM_API std::string FromSave(const Json& save, const Data& data, GameState& out);

WARROOMSIM_API std::string SaveToString(const GameState& s);
WARROOMSIM_API std::string LoadFromString(const std::string& text, const Data& data, GameState& out);

}  // namespace warroom
