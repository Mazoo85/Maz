// Seeded random numbers: xoshiro128** on four 32-bit words. The state is plain
// data so it lives inside the game state and round-trips through a save. Every
// subsystem gets its own stream so adding a roll in one never changes another.
#pragma once
#include "Api.h"
#include <cstdint>
#include <string>
#include <vector>

namespace warroom {

struct Rng {
    uint32_t s[4] = {1, 2, 3, 4};
};

WARROOMSIM_API Rng SeedRng(const std::string& seed);
WARROOMSIM_API Rng SeedStream(const std::string& seed, const std::string& name);
uint32_t NextU32(Rng& r);
double NextFloat(Rng& r);            // [0, 1)
int NextInt(Rng& r, int n);          // 0..n-1
WARROOMSIM_API int PickWeighted(Rng& r, const std::vector<double>& weights);

}  // namespace warroom
