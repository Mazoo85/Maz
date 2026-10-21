#pragma once

#include "ironfront/Units.hpp"

#include "maz/game/HexGrid.hpp"

#include <string>
#include <vector>

// Iron Front — the hex map: terrain, land ownership and cities. Stored row-major in "odd-r" offset
// coordinates (odd rows shifted half a hex right, which is how pointy-top hexes tile a rectangle);
// the rest of the game addresses hexes by axial maz::game::Hex.
namespace ironfront {

using maz::game::Hex;

enum class Terrain { Plains, Forest, Mountain, Sea };

enum Country : int { Germany, Britain, France, Soviet, Italy, CountryCount };
constexpr int kNeutral = -1;

inline const char* countryName(int c) {
    static const char* names[CountryCount] = {"Germany", "United Kingdom", "France", "Soviet Union", "Italy"};
    return (c >= 0 && c < CountryCount) ? names[c] : "Neutral";
}

// Owner letters used by the ASCII map layers.
inline int countryFromLetter(char ch) {
    switch (ch) {
        case 'G': return Germany;
        case 'U': return Britain;
        case 'F': return France;
        case 'S': return Soviet;
        case 'I': return Italy;
        default: return kNeutral;
    }
}

inline Hex offsetToHex(int col, int row) { return Hex(col - (row - (row & 1)) / 2, row); }

inline void hexToOffset(const Hex& h, int& col, int& row) {
    row = h.r;
    col = h.q + (h.r - (h.r & 1)) / 2;
}

struct City {
    std::string name;
    Hex hex;
    int owner = kNeutral;
    bool capital = false;
};

class Map {
public:
    int width() const { return m_w; }
    int height() const { return m_h; }

    bool inBounds(const Hex& h) const {
        int c = 0, r = 0;
        hexToOffset(h, c, r);
        return c >= 0 && r >= 0 && c < m_w && r < m_h;
    }
    Terrain terrain(const Hex& h) const { return inBounds(h) ? m_terrain[index(h)] : Terrain::Sea; }
    int owner(const Hex& h) const { return inBounds(h) ? m_owner[index(h)] : kNeutral; }
    void setOwner(const Hex& h, int c) {
        if (inBounds(h)) m_owner[index(h)] = c;
    }

    const City* cityAt(const Hex& h) const {
        for (const City& c : m_cities)
            if (c.hex == h) return &c;
        return nullptr;
    }
    City* cityAt(const Hex& h) {
        for (City& c : m_cities)
            if (c.hex == h) return &c;
        return nullptr;
    }
    std::vector<City>& cities() { return m_cities; }
    const std::vector<City>& cities() const { return m_cities; }

    // Terrain rows use '.' plains, 'f' forest, 'm' mountain, '~' sea. Owner rows use G U F S I for the
    // five countries and anything else for neutral. Both layers must be the same size.
    static Map fromAscii(const std::vector<std::string>& terrainRows, const std::vector<std::string>& ownerRows) {
        Map m;
        m.m_h = static_cast<int>(terrainRows.size());
        m.m_w = m.m_h > 0 ? static_cast<int>(terrainRows[0].size()) : 0;
        m.m_terrain.assign(static_cast<std::size_t>(m.m_w * m.m_h), Terrain::Sea);
        m.m_owner.assign(static_cast<std::size_t>(m.m_w * m.m_h), kNeutral);
        for (int r = 0; r < m.m_h; ++r) {
            for (int c = 0; c < m.m_w; ++c) {
                const std::size_t i = static_cast<std::size_t>(r * m.m_w + c);
                const char t = c < static_cast<int>(terrainRows[static_cast<std::size_t>(r)].size())
                                   ? terrainRows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]
                                   : '~';
                m.m_terrain[i] = t == 'f' ? Terrain::Forest
                                 : t == 'm' ? Terrain::Mountain
                                 : t == '~' ? Terrain::Sea
                                            : Terrain::Plains;
                if (r < static_cast<int>(ownerRows.size()) &&
                    c < static_cast<int>(ownerRows[static_cast<std::size_t>(r)].size()) && m.m_terrain[i] != Terrain::Sea)
                    m.m_owner[i] = countryFromLetter(ownerRows[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]);
            }
        }
        return m;
    }

    // Raw access for saving/loading ownership.
    std::vector<int>& ownerGrid() { return m_owner; }
    const std::vector<int>& ownerGrid() const { return m_owner; }

private:
    std::size_t index(const Hex& h) const {
        int c = 0, r = 0;
        hexToOffset(h, c, r);
        return static_cast<std::size_t>(r * m_w + c);
    }

    int m_w = 0;
    int m_h = 0;
    std::vector<Terrain> m_terrain;
    std::vector<int> m_owner;
    std::vector<City> m_cities;
};

} // namespace ironfront
