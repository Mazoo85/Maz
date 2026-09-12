// tests/core/uuid.cpp — verifies the v4 UUID generator (core::makeUuidV4 / uuidV4String / isValidUuid). Ground
// truths: the output is canonical (36 chars, hyphens at 8/13/18/23, lowercase hex); the version nibble is '4' and
// the variant nibble is one of 8/9/a/b (RFC 4122); generation is deterministic for a seed and differs across
// seeds/draws; a batch is collision-free; isValidUuid accepts good layouts (incl. uppercase) and rejects malformed
// ones. Pure CPU, deterministic.
#include "maz/core/Uuid.hpp"
#include "maz/core/Pcg32.hpp"

#include <cstdio>
#include <set>
#include <string>

static int g_fail = 0;
#define CHECK(c, m) do{ if(!(c)){ std::printf("FAIL: %s\n",(m)); ++g_fail; } }while(0)

using namespace maz;

int main() {
    // --- 1. Canonical format + version/variant bits. ---
    {
        core::Pcg32 rng(0xDEADBEEFu, 0x1234u);
        const std::string s = core::uuidV4String(rng);
        CHECK(s.size() == 36, "uuid is 36 characters");
        CHECK(s[8] == '-' && s[13] == '-' && s[18] == '-' && s[23] == '-', "hyphens at 8/13/18/23");
        CHECK(s[14] == '4', "version nibble is 4");
        const char var = s[19];
        CHECK(var == '8' || var == '9' || var == 'a' || var == 'b', "variant nibble is 8/9/a/b");
        bool lower = true;
        for (char c : s)
            if (c >= 'A' && c <= 'F') lower = false;
        CHECK(lower, "hex is lowercase");
        CHECK(core::isValidUuid(s), "a generated uuid validates");
    }

    // --- 2. Determinism: same seed -> same uuid; different seed -> different; consecutive draws differ. ---
    {
        core::Pcg32 a(42u, 7u), b(42u, 7u);
        CHECK(core::makeUuidV4(a) == core::makeUuidV4(b), "same seed -> identical uuid");
        core::Pcg32 c(43u, 7u);
        core::Pcg32 d(42u, 7u);
        CHECK(core::makeUuidV4(c) != core::makeUuidV4(d), "different seed -> different uuid");
        core::Pcg32 e(100u, 1u);
        CHECK(core::makeUuidV4(e) != core::makeUuidV4(e), "consecutive uuids from one stream differ");
    }

    // --- 3. Collision-free batch. ---
    {
        core::Pcg32 rng(555u, 3u);
        std::set<std::string> seen;
        bool allUnique = true, allValid = true;
        for (int i = 0; i < 2000; ++i) {
            const std::string s = core::uuidV4String(rng);
            if (!core::isValidUuid(s)) allValid = false;
            if (!seen.insert(s).second) allUnique = false;
        }
        CHECK(allUnique, "2000 generated uuids are all distinct");
        CHECK(allValid, "2000 generated uuids all validate");
    }

    // --- 4. isValidUuid: accepts good (incl. uppercase), rejects malformed. ---
    {
        CHECK(core::isValidUuid("123e4567-e89b-12d3-a456-426614174000"), "accepts a canonical literal");
        CHECK(core::isValidUuid("123E4567-E89B-12D3-A456-426614174000"), "accepts uppercase hex");
        CHECK(!core::isValidUuid(""), "rejects empty");
        CHECK(!core::isValidUuid("123e4567-e89b-12d3-a456-42661417400"), "rejects too short");
        CHECK(!core::isValidUuid("123e4567-e89b-12d3-a456-4266141740000"), "rejects too long");
        CHECK(!core::isValidUuid("123e4567xe89b-12d3-a456-426614174000"), "rejects a bad hyphen position");
        CHECK(!core::isValidUuid("123e4567-e89b-12d3-a456-42661417400g"), "rejects a non-hex character");
    }

    if (g_fail == 0) {
        std::printf("uuid: OK — canonical v4 format, version/variant bits, deterministic, collision-free, validation.\n");
        return 0;
    }
    std::printf("uuid: %d failure(s).\n", g_fail);
    return 1;
}
