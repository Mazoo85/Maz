#include "maz/io/TextFormat.hpp"

#include <cctype>
#include <cstdlib>
#include <ostream>

namespace maz::io {

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> tokens;
    std::size_t i = 0;
    while (i < line.size()) {
        if (std::isspace(static_cast<unsigned char>(line[i]))) {
            ++i;
            continue;
        }
        std::string tok;
        if (line[i] == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') {
                if (line[i] == '\\' && i + 1 < line.size()) {
                    ++i; // take the escaped character literally
                }
                tok += line[i++];
            }
            if (i < line.size()) {
                ++i; // closing quote
            }
        } else {
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
                tok += line[i++];
            }
        }
        tokens.push_back(tok);
    }
    return tokens;
}

std::string quote(const std::string& s) {
    std::string out = "\"";
    for (char c : s) {
        if (c == '"' || c == '\\') {
            out += '\\';
        }
        out += c;
    }
    out += '"';
    return out;
}

void writeVec3(std::ostream& os, const char* key, const math::vec3& v) {
    os << "  " << key << ' ' << v.x << ' ' << v.y << ' ' << v.z << '\n';
}

bool readVec3(const std::vector<std::string>& t, math::vec3& out) {
    if (t.size() < 4) {
        return false;
    }
    out.x = std::strtof(t[1].c_str(), nullptr);
    out.y = std::strtof(t[2].c_str(), nullptr);
    out.z = std::strtof(t[3].c_str(), nullptr);
    return true;
}

} // namespace maz::io
