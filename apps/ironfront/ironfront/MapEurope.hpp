#pragma once

#include "ironfront/Map.hpp"
#include "ironfront/Units.hpp"

#include <string>
#include <vector>

// Iron Front — the "Europe 1939" scenario: a 40x40 hex map (odd-r offset rows, north at the top)
// with five playable powers, neutral states, cities and starting armies. Generated from rough
// lon/lat outlines (one column ~1.4 degrees of longitude, one row ~0.8 degrees of latitude) and
// then kept as plain data here, so it can be edited by hand.
namespace ironfront {

struct UnitSpawn {
    UnitType type;
    int owner;
    int col, row; // offset coordinates
};

struct Scenario {
    Map map;
    std::vector<UnitSpawn> units;
};

inline Scenario europe1939() {
    // '.' plains, 'f' forest, 'm' mountain, '~' sea
    static const std::vector<std::string> terrain = {
        "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~",
        "~~~~~~~~~~~~~~~mmmm....fffffffffffffffff",
        "~~~~~~~~~~~~~~~mmmm....~ffffffffffffffff",
        "~~~~~~~~~~~~~mmmmm.f.~~fffffffffffffffff",
        "~~~~~~~~~~~~~mmmmm.f.~~fffffffffffffffff",
        "~~~~~~~~~~~mmmmmmfff~~.fffffffffffffffff",
        "~~~~~~~~~~~mmmmmm.ff~~~fffffffffffffffff",
        "~~~~~~~~~~~mmm...fff~~~~.....fffffffffff",
        "~~~~~~~~~~~mm....fff.~~~~~~~..ffffffffff",
        "~~~~~~~~~~~...~..fff~~~~~~...fffffffffff",
        "~~~~m.~~~~~~~~~~.f..~~~ff~ff.fffffffffff",
        "~~~mm.~~~~~~~..~...~~~.f.ff..fffffffffff",
        "~~~...~~~~~~~~.~~..~~~~fffff............",
        "~~~...~~~~~~~.....~~~~..................",
        "~......~~~~~~~..~~~~~...................",
        "...~~..~~~~~~...........................",
        "........~~~~............fffff...........",
        "...~....~~..............fffff...........",
        "..~~.....~..............fffff...........",
        "~~~~..~~~.......f.......................",
        "~~~~.~~~~..f..fff.......................",
        "~~~~~~~......ffff.....mm................",
        "~~~~..........f.......mmmm..............",
        "~~~~~..ff..............mmm..............",
        "~~~~~~........mmmmm....mmm..............",
        "~~~~~~......mmmmmmm....mmm...~~.........",
        "~~~~~~~.....mm.m.~......m....~~.........",
        "~~~~~~.....mm...~~.........~~~~~~~~.....",
        "~~~~~~~......~~.m~~m........~~~~~~~~mmm.",
        "~..mm.mmm.~~~.~.m~~~.......~~~~~~~~~~~~~",
        "~.........~~~~~~.m~~~m......~~~~~~~~~~~~",
        "~.......~~~~~~~~~..~~.m...............m~",
        "~.......~~~~~~.~~~m..~mm~~~.............",
        ".......~~~~~~.~~~~..~.mm~~..............",
        "~.......~~~~~~~~~~~~~~.m.~...mmmmmmmmmm.",
        "~......~~~~~~~~~..~~~~...~...mmmmmmmmmm.",
        "~..mmm.~~~~~~~.~~..~~~~.~~~.............",
        "~~~..~~~.......~~~~~~~~~~~~~~~~~~.......",
        "~~~.~~~........~~~~~~~~~~~~~~~~~~.......",
        "~.........m....~~~~~~~~~~~~~~~~~~.......",
    };
    // G Germany, U United Kingdom, F France, S Soviet Union, I Italy, '-' neutral (blank = sea)
    static const std::vector<std::string> owners = {
        "                                        ",
        "               --------------SSSSSSSSSSS",
        "               -------- ------SSSSSSSSSS",
        "             --------  -------SSSSSSSSSS",
        "             --------  -------SSSSSSSSSS",
        "           ---------  --------SSSSSSSSSS",
        "           ---------   --------SSSSSSSSS",
        "           ---------    ----SSSSSSSSSSSS",
        "           ----------       SSSSSSSSSSSS",
        "           --- -----      -SSSSSSSSSSSSS",
        "    UU          ----   -- --SSSSSSSSSSSS",
        "   UUU       -- ---   -----SSSSSSSSSSSSS",
        "   UUU        -  --    -----SSSSSSSSSSSS",
        "   UUU       -----    -----SSSSSSSSSSSSS",
        " -UUUUU       --     -GG---SSSSSSSSSSSSS",
        "-UU  UU      GGGG----GG---SSSSSSSSSSSSSS",
        "----UUUU    -GGGGGG--------SSSSSSSSSSSSS",
        "--U UUUU  --GGGGGGG--------SSSSSSSSSSSSS",
        "--  UUUUU --GGGGGGG--------SSSSSSSSSSSSS",
        "    UU   --GGGGGGG---------SSSSSSSSSSSSS",
        "    U    FF-GGGGGGGG-------SSSSSSSSSSSSS",
        "       FFFFFGGGGGGGG-----S-SSSSSSSSSSSSS",
        "    FFFFFFFFFFGGGGGG------SSSSSSSSSSSSSS",
        "     FFFFFFFFGGGGGG-------SSSSSSSSSSSSSS",
        "      FFFFFF---I-----------SSSSSSSSSSSSS",
        "      FFFFFFIIIII-----------S  SSSSSSSSS",
        "       FFFFFFIIII -----------  -SSSSSSSS",
        "      FFFFFFIIII  ---------        SSSSS",
        "       FFFFFF  II  ---------        SSSS",
        " -------FF   F II   -------             ",
        " ---------      II   I------            ",
        " -------         II  I----------------- ",
        " -------      I   III I-   -------------",
        "-------      I    -- ---  --------------",
        " -------              --- --------------",
        " ------         II    --- --------------",
        " -----F       F  II    -   -------------",
        "   U-   FFFFFFF                  -------",
        "   U   FFFFFFFF                  -------",
        " -----FFFFFFFFF                  -------",
    };
    struct CityRow {
        const char* name;
        int col, row;
        char owner;
        bool capital;
    };
    static const CityRow cityRows[] = {
        {"Berlin", 16, 17, 'G', true},
        {"Hamburg", 14, 16, 'G', false},
        {"Cologne", 12, 19, 'G', false},
        {"Munich", 15, 22, 'G', false},
        {"Vienna", 19, 22, 'G', false},
        {"Prague", 17, 20, 'G', false},
        {"Konigsberg", 22, 14, 'G', false},
        {"London", 7, 18, 'U', true},
        {"Manchester", 6, 16, 'U', false},
        {"Glasgow", 4, 13, 'U', false},
        {"Plymouth", 4, 20, 'U', false},
        {"Belfast", 3, 14, 'U', false},
        {"Gibraltar", 3, 37, 'U', false},
        {"Paris", 8, 21, 'F', true},
        {"Lyon", 10, 25, 'F', false},
        {"Marseille", 11, 28, 'F', false},
        {"Bordeaux", 6, 27, 'F', false},
        {"Brest", 4, 22, 'F', false},
        {"Strasbourg", 13, 22, 'F', false},
        {"Algiers", 9, 37, 'F', false},
        {"Moscow", 34, 13, 'S', true},
        {"Leningrad", 29, 8, 'S', false},
        {"Kiev", 28, 19, 'S', false},
        {"Minsk", 26, 15, 'S', false},
        {"Kharkov", 33, 20, 'S', false},
        {"Odessa", 29, 24, 'S', false},
        {"Stalingrad", 39, 22, 'S', false},
        {"Rome", 16, 30, 'I', true},
        {"Milan", 14, 26, 'I', false},
        {"Venice", 16, 26, 'I', false},
        {"Naples", 17, 31, 'I', false},
        {"Palermo", 16, 35, 'I', false},
        {"Tirana", 21, 31, 'I', false},
        {"Cagliari", 13, 33, 'I', false},
        {"Madrid", 4, 32, '-', false},
        {"Lisbon", 1, 34, '-', false},
        {"Warsaw", 22, 17, '-', false},
        {"Brussels", 10, 19, '-', false},
        {"Amsterdam", 10, 17, '-', false},
        {"Copenhagen", 16, 13, '-', false},
        {"Oslo", 15, 8, '-', false},
        {"Stockholm", 20, 8, '-', false},
        {"Helsinki", 24, 7, '-', false},
        {"Budapest", 20, 23, '-', false},
        {"Bucharest", 25, 27, '-', false},
        {"Belgrade", 21, 27, '-', false},
        {"Athens", 24, 35, '-', false},
        {"Istanbul", 27, 31, '-', false},
        {"Ankara", 30, 33, '-', false},
        {"Riga", 24, 11, '-', false},
        {"Dublin", 3, 16, '-', false},
    };
    struct UnitRow {
        UnitType type;
        char owner;
        int col, row;
    };
    static const UnitRow unitRows[] = {
        {UnitType::Tank, 'G', 16, 17},
        {UnitType::Tank, 'G', 14, 16},
        {UnitType::Infantry, 'G', 12, 19},
        {UnitType::Infantry, 'G', 15, 22},
        {UnitType::Infantry, 'G', 19, 22},
        {UnitType::Infantry, 'G', 17, 20},
        {UnitType::Artillery, 'G', 22, 14},
        {UnitType::Artillery, 'G', 17, 17},
        {UnitType::Fighter, 'G', 15, 16},
        {UnitType::Fighter, 'G', 13, 19},
        {UnitType::Infantry, 'U', 7, 18},
        {UnitType::Infantry, 'U', 6, 16},
        {UnitType::Infantry, 'U', 4, 13},
        {UnitType::Artillery, 'U', 4, 20},
        {UnitType::Tank, 'U', 3, 14},
        {UnitType::Fighter, 'U', 3, 37},
        {UnitType::Destroyer, 'U', 6, 19},
        {UnitType::Destroyer, 'U', 8, 16},
        {UnitType::Infantry, 'F', 8, 21},
        {UnitType::Infantry, 'F', 10, 25},
        {UnitType::Infantry, 'F', 11, 28},
        {UnitType::Infantry, 'F', 6, 27},
        {UnitType::Infantry, 'F', 4, 22},
        {UnitType::Tank, 'F', 13, 22},
        {UnitType::Artillery, 'F', 9, 37},
        {UnitType::Artillery, 'F', 9, 21},
        {UnitType::Fighter, 'F', 11, 25},
        {UnitType::Infantry, 'S', 34, 13},
        {UnitType::Infantry, 'S', 29, 8},
        {UnitType::Infantry, 'S', 28, 19},
        {UnitType::Infantry, 'S', 26, 15},
        {UnitType::Infantry, 'S', 33, 20},
        {UnitType::Infantry, 'S', 29, 24},
        {UnitType::Tank, 'S', 39, 22},
        {UnitType::Tank, 'S', 35, 13},
        {UnitType::Artillery, 'S', 30, 8},
        {UnitType::Fighter, 'S', 29, 19},
        {UnitType::Infantry, 'I', 16, 30},
        {UnitType::Infantry, 'I', 14, 26},
        {UnitType::Infantry, 'I', 16, 26},
        {UnitType::Tank, 'I', 17, 31},
        {UnitType::Artillery, 'I', 16, 35},
        {UnitType::Fighter, 'I', 21, 31},
        {UnitType::Destroyer, 'I', 14, 33},
        {UnitType::Destroyer, 'I', 15, 30},
    };

    Scenario s;
    s.map = Map::fromAscii(terrain, owners);
    for (const CityRow& c : cityRows)
        s.map.cities().push_back(City{c.name, offsetToHex(c.col, c.row), countryFromLetter(c.owner), c.capital});
    for (const UnitRow& u : unitRows) s.units.push_back(UnitSpawn{u.type, countryFromLetter(u.owner), u.col, u.row});
    return s;
}

} // namespace ironfront
