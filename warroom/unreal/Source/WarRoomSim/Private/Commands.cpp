#include "WarRoomSim/Commands.h"

#include "WarRoomSim/Clock.h"

namespace warroom {

const std::vector<std::string> kCommandTypes = {"set_speed", "pause", "resume", "select_country"};

static std::string ValidateSpeed(int speed) {
    if (speed < 0 || speed > 5) return "speed must be 0 to 5";
    return "";
}

std::string ValidateCommand(const GameState& s, const Command& cmd) {
    if (cmd.type == "set_speed") return ValidateSpeed(cmd.speed);
    if (cmd.type == "pause") return "";
    if (cmd.type == "resume") return cmd.speed >= 0 ? ValidateSpeed(cmd.speed) : "";
    if (cmd.type == "select_country") {
        if (!cmd.hasTag) return "select_country needs a tag (empty for observer)";
        if (!cmd.tag.empty() && !s.countries.count(cmd.tag)) return "no country \"" + cmd.tag + "\"";
        return "";
    }
    return "unknown command \"" + cmd.type + "\"";
}

std::string ApplyCommand(GameState& s, const Command& cmd) {
    std::string reason = ValidateCommand(s, cmd);
    if (!reason.empty()) return reason;
    if (cmd.type == "set_speed") { if (cmd.speed > 0) s.lastSpeed = cmd.speed; s.speed = cmd.speed; }
    else if (cmd.type == "pause") { s.speed = 0; }
    else if (cmd.type == "resume") { s.speed = cmd.speed >= 0 ? cmd.speed : (s.lastSpeed > 0 ? s.lastSpeed : 1); if (s.speed > 0) s.lastSpeed = s.speed; }
    else if (cmd.type == "select_country") { s.player = cmd.tag; }
    s.log.push_back(LogEntry{s.hour, cmd});
    if (s.log.size() > 10000) s.log.erase(s.log.begin(), s.log.begin() + static_cast<long>(s.log.size() - 10000));
    return "";
}

}  // namespace warroom
