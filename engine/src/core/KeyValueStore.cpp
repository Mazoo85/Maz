#include "maz/core/KeyValueStore.hpp"

#include "maz/core/Log.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>

namespace maz::core {

namespace {
std::string trim(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) {
        return "";
    }
    const auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(begin, end - begin + 1);
}
} // namespace

bool KeyValueStore::load(const std::string& path) {
    m_path = path;
    m_values.clear();

    std::ifstream f(path);
    if (!f) {
        return true; // missing file => empty store (first run)
    }
    std::string line;
    while (std::getline(f, line)) {
        const std::string t = trim(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        const auto eq = t.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        m_values[trim(t.substr(0, eq))] = trim(t.substr(eq + 1));
    }
    return true;
}

bool KeyValueStore::save() const {
    if (m_path.empty()) {
        MAZ_LOG_WARN("KeyValueStore::save with no path (call load first)");
        return false;
    }
    std::ofstream f(m_path, std::ios::trunc);
    if (!f) {
        MAZ_LOG_ERROR("KeyValueStore: cannot write '%s'", m_path.c_str());
        return false;
    }
    for (const auto& [key, value] : m_values) {
        f << key << '=' << value << '\n';
    }
    return true;
}

bool KeyValueStore::has(const std::string& key) const {
    return m_values.find(key) != m_values.end();
}

int KeyValueStore::getInt(const std::string& key, int fallback) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) {
        return fallback;
    }
    return static_cast<int>(std::strtol(it->second.c_str(), nullptr, 10));
}

float KeyValueStore::getFloat(const std::string& key, float fallback) const {
    const auto it = m_values.find(key);
    if (it == m_values.end()) {
        return fallback;
    }
    return std::strtof(it->second.c_str(), nullptr);
}

std::string KeyValueStore::getString(const std::string& key, const std::string& fallback) const {
    const auto it = m_values.find(key);
    return it == m_values.end() ? fallback : it->second;
}

void KeyValueStore::set(const std::string& key, int value) {
    m_values[key] = std::to_string(value);
}

void KeyValueStore::set(const std::string& key, float value) {
    std::ostringstream ss;
    ss << value;
    m_values[key] = ss.str();
}

void KeyValueStore::setString(const std::string& key, const std::string& value) {
    m_values[key] = value;
}

} // namespace maz::core
