#include "WarRoomSim/Rng.h"

namespace warroom {

static uint32_t Rotl(uint32_t x, int k) { return (x << k) | (x >> (32 - k)); }

static Rng Hash(const std::string& str) {
    uint32_t h = 0x811c9dc5u;
    for (unsigned char c : str) { h ^= c; h *= 0x01000193u; }
    Rng r;
    bool allZero = true;
    for (int i = 0; i < 4; i++) {
        h += 0x9e3779b9u;
        uint32_t z = h;
        z = (z ^ (z >> 16)) * 0x85ebca6bu;
        z = (z ^ (z >> 13)) * 0xc2b2ae35u;
        z = z ^ (z >> 16);
        r.s[i] = z;
        if (z) allZero = false;
    }
    if (allZero) r.s[0] = 1;
    return r;
}

Rng SeedRng(const std::string& seed) { return Hash(seed); }
Rng SeedStream(const std::string& seed, const std::string& name) { return Hash(seed + "/" + name); }

uint32_t NextU32(Rng& r) {
    uint32_t* s = r.s;
    const uint32_t result = Rotl(s[1] * 5u, 7) * 9u;
    const uint32_t t = s[1] << 9;
    s[2] ^= s[0]; s[3] ^= s[1]; s[1] ^= s[2]; s[0] ^= s[3];
    s[2] ^= t;
    s[3] = Rotl(s[3], 11);
    return result;
}

double NextFloat(Rng& r) { return (NextU32(r) >> 8) / 16777216.0; }
int NextInt(Rng& r, int n) { return static_cast<int>(NextFloat(r) * n); }

int PickWeighted(Rng& r, const std::vector<double>& weights) {
    double total = 0;
    for (double w : weights) total += w;
    double roll = NextFloat(r) * total;
    for (size_t i = 0; i < weights.size(); i++) {
        roll -= weights[i];
        if (roll < 0) return static_cast<int>(i);
    }
    return static_cast<int>(weights.size()) - 1;
}

}  // namespace warroom
