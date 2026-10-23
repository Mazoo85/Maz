// Everything that changes during play, as plain data: a save is this struct as
// JSON. Sections for later stages exist already (empty) so saves stay compatible.
#pragma once
#include "Api.h"
#include <map>
#include <string>
#include <vector>

#include "Data.h"
#include "Rng.h"

namespace warroom {

constexpr int kStateVersion = 1;

struct Command {
    std::string type;        // set_speed | pause | resume | select_country
    int speed = -1;          // set_speed / resume
    bool hasTag = false;     // select_country: tag may be empty (= observer)
    std::string tag;
};

struct CountryState {
    std::string tag, name, adjective, color, ideology, leader;
    int capital = 0;
    Flag flag;
    std::vector<int> states;
};

struct StateState {
    std::string owner, controller;
    std::vector<std::string> cores;
    int infrastructure = 1;
    long long manpower = 0;
};

struct RegionState {
    std::string weather = "clear";
    int weatherDays = 0;
};

struct LogEntry {
    long long hour = 0;
    Command cmd;
};

struct GameState {
    int version = kStateVersion;
    std::string seed, scenario;
    long long hour = 0;
    int speed = 0;
    int lastSpeed = 1;
    std::string player;                              // empty = observer
    Rng rngWeather, rngEvents;
    std::map<std::string, CountryState> countries;   // ordered: determinism
    std::map<int, StateState> states;
    std::map<int, RegionState> regions;
    std::map<std::string, std::vector<Army>> armies;
    bool dirtyRegions = false;
    std::vector<LogEntry> log;
};

// Builds the starting state. Returns "" or an error.
WARROOMSIM_API std::string NewGame(const Data& data, const std::string& seed, const std::string& scenario, GameState& out);

// The controller of a province's state, or "" for sea, lakes and wasteland.
WARROOMSIM_API std::string ProvinceController(const GameState& s, const Data& data, int provinceId);

}  // namespace warroom
