// Commands are the only way anything changes the game state. The interface
// sends them, the AI sends them, a multiplayer relay will forward them.
#pragma once
#include "Api.h"
#include <string>
#include <vector>

#include "GameState.h"

namespace warroom {

// "" when the command is allowed, else a short reason in plain words.
WARROOMSIM_API std::string ValidateCommand(const GameState& s, const Command& cmd);

// Applies a valid command. Returns the validation reason if it was refused.
WARROOMSIM_API std::string ApplyCommand(GameState& s, const Command& cmd);

extern WARROOMSIM_API const std::vector<std::string> kCommandTypes;

}  // namespace warroom
