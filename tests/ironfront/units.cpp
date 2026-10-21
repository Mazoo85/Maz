// tests/ironfront/units.cpp — the unit table matches the Stage 1 spec.
#include "check.hpp"
#include "ironfront/Units.hpp"

using namespace ironfront;

int main() {
    CHECK(stats(UnitType::Infantry).maxHp == 10);
    CHECK(stats(UnitType::Infantry).attack == 4 && stats(UnitType::Infantry).defense == 4);
    CHECK(stats(UnitType::Infantry).move == 1 && stats(UnitType::Infantry).vision == 2);
    CHECK(stats(UnitType::Tank).maxHp == 12 && stats(UnitType::Tank).attack == 7);
    CHECK(stats(UnitType::Tank).move == 2);
    CHECK(stats(UnitType::Artillery).range == 2 && stats(UnitType::Artillery).defense == 2);
    CHECK(stats(UnitType::Fighter).move == 4 && stats(UnitType::Fighter).vision == 4);
    CHECK(stats(UnitType::Fighter).domain == Domain::Air);
    CHECK(stats(UnitType::Destroyer).domain == Domain::Sea && stats(UnitType::Destroyer).move == 3);
    for (int i = 0; i < kUnitTypeCount; ++i) {
        const UnitStats& s = stats(static_cast<UnitType>(i));
        CHECK(s.maxHp > 0 && s.attack > 0 && s.move > 0 && s.range >= 1);
    }
    return finish("ironfront_units");
}
