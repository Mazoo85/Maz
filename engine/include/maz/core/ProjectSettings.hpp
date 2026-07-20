#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

// maz::core ProjectSettings — the central, project-wide settings store behind Godot's ProjectSettings
// singleton and its `project.godot` file: the one place that answers "what is this game called, what scene
// does it start on, how big is the window" plus any number of typed key->value settings organized by
// Godot-style dotted paths (e.g. "application/config/name", "display/window/size/viewport_width"). Both the
// editor (which needs the main scene + window size) and the export/packaging step read from here. Values are
// typed (bool / int / float / string); `save` serializes to Godot's sectioned `project.godot` text format
// (the first path segment becomes a `[section]`), and `load` reads it back, so settings round-trip exactly.
// Pure CPU string/number work — no filesystem calls here (the caller reads/writes the text) — so it
// unit-tests headlessly. It composes with `core::ConfigFile` (generic INI) rather than replacing it: this
// layer adds typed values, Godot key conventions, and the project-manifest convenience accessors.
namespace maz::core {

struct SettingValue {
    enum class Type { Bool, Int, Float, String };
    Type type = Type::String;
    bool b = false;
    std::int64_t i = 0;
    double d = 0.0;
    std::string s;

    SettingValue() = default;
    static SettingValue fromBool(bool v) { SettingValue x; x.type = Type::Bool; x.b = v; return x; }
    static SettingValue fromInt(std::int64_t v) { SettingValue x; x.type = Type::Int; x.i = v; return x; }
    static SettingValue fromFloat(double v) { SettingValue x; x.type = Type::Float; x.d = v; return x; }
    static SettingValue fromString(std::string v) { SettingValue x; x.type = Type::String; x.s = std::move(v); return x; }

    bool asBool() const {
        switch (type) {
            case Type::Bool: return b;
            case Type::Int: return i != 0;
            case Type::Float: return d != 0.0;
            case Type::String: return s == "true" || s == "1";
        }
        return false;
    }
    std::int64_t asInt() const {
        switch (type) {
            case Type::Bool: return b ? 1 : 0;
            case Type::Int: return i;
            case Type::Float: return static_cast<std::int64_t>(d);
            case Type::String: return static_cast<std::int64_t>(std::strtoll(s.c_str(), nullptr, 10));
        }
        return 0;
    }
    double asFloat() const {
        switch (type) {
            case Type::Bool: return b ? 1.0 : 0.0;
            case Type::Int: return static_cast<double>(i);
            case Type::Float: return d;
            case Type::String: return std::strtod(s.c_str(), nullptr);
        }
        return 0.0;
    }
    std::string asString() const {
        switch (type) {
            case Type::Bool: return b ? "true" : "false";
            case Type::Int: return std::to_string(i);
            case Type::Float: return SettingValue::formatFloat(d);
            case Type::String: return s;
        }
        return {};
    }

    // Format a double so it always reads back as a float (keeps a decimal point / exponent).
    static std::string formatFloat(double v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.9g", v);
        std::string out(buf);
        if (out.find_first_of(".eEnN") == std::string::npos) out += ".0"; // 3 -> 3.0 so type stays Float
        return out;
    }
};

class ProjectSettings {
public:
    void setBool(const std::string& key, bool v) { m_map[key] = SettingValue::fromBool(v); }
    void setInt(const std::string& key, std::int64_t v) { m_map[key] = SettingValue::fromInt(v); }
    void setFloat(const std::string& key, double v) { m_map[key] = SettingValue::fromFloat(v); }
    void setString(const std::string& key, const std::string& v) { m_map[key] = SettingValue::fromString(v); }
    void setValue(const std::string& key, const SettingValue& v) { m_map[key] = v; }

    bool has(const std::string& key) const { return m_map.find(key) != m_map.end(); }
    void erase(const std::string& key) { m_map.erase(key); }
    void clear() { m_map.clear(); }
    std::size_t count() const { return m_map.size(); }

    // Typed getters — return the stored value (coerced to the requested type) or `def` when absent.
    bool getBool(const std::string& key, bool def = false) const {
        auto it = m_map.find(key);
        return it == m_map.end() ? def : it->second.asBool();
    }
    std::int64_t getInt(const std::string& key, std::int64_t def = 0) const {
        auto it = m_map.find(key);
        return it == m_map.end() ? def : it->second.asInt();
    }
    double getFloat(const std::string& key, double def = 0.0) const {
        auto it = m_map.find(key);
        return it == m_map.end() ? def : it->second.asFloat();
    }
    std::string getString(const std::string& key, const std::string& def = std::string()) const {
        auto it = m_map.find(key);
        return it == m_map.end() ? def : it->second.asString();
    }

    std::vector<std::string> keys() const {
        std::vector<std::string> out;
        out.reserve(m_map.size());
        for (const auto& kv : m_map) out.push_back(kv.first);
        std::sort(out.begin(), out.end());
        return out;
    }

    // --- Project-manifest convenience accessors (standard Godot keys) ---
    std::string applicationName() const { return getString("application/config/name"); }
    void setApplicationName(const std::string& v) { setString("application/config/name", v); }
    std::string mainScene() const { return getString("application/run/main_scene"); }
    void setMainScene(const std::string& v) { setString("application/run/main_scene", v); }
    std::int64_t windowWidth() const { return getInt("display/window/size/viewport_width", 1152); }
    void setWindowWidth(std::int64_t v) { setInt("display/window/size/viewport_width", v); }
    std::int64_t windowHeight() const { return getInt("display/window/size/viewport_height", 648); }
    void setWindowHeight(std::int64_t v) { setInt("display/window/size/viewport_height", v); }

    // Serialize to Godot-style `project.godot` text: keys without a '/' go in a leading section-less block,
    // and each key "section/rest" is grouped under `[section]` as `rest=value`.
    std::string save() const {
        const std::vector<std::string> ks = keys();
        std::string out;
        // Leading block: keys with no section (no '/').
        for (const std::string& k : ks) {
            if (k.find('/') == std::string::npos) out += k + "=" + serialize(m_map.at(k)) + "\n";
        }
        std::string section;
        for (const std::string& k : ks) {
            const std::size_t slash = k.find('/');
            if (slash == std::string::npos) continue;
            const std::string sec = k.substr(0, slash);
            const std::string prop = k.substr(slash + 1);
            if (sec != section) {
                out += "\n[" + sec + "]\n";
                section = sec;
            }
            out += prop + "=" + serialize(m_map.at(k)) + "\n";
        }
        return out;
    }

    // Parse `project.godot`-style text. Replaces current contents. Always succeeds (unknown lines ignored).
    bool load(const std::string& text) {
        m_map.clear();
        std::string section;
        std::size_t pos = 0;
        while (pos <= text.size()) {
            std::size_t nl = text.find('\n', pos);
            if (nl == std::string::npos) nl = text.size();
            std::string line = text.substr(pos, nl - pos);
            pos = nl + 1;
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const std::string t = trim(line);
            if (t.empty() || t[0] == ';' || t[0] == '#') continue;
            if (t.front() == '[' && t.back() == ']') {
                section = t.substr(1, t.size() - 2);
                continue;
            }
            const std::size_t eq = t.find('=');
            if (eq == std::string::npos) continue;
            const std::string prop = trim(t.substr(0, eq));
            const std::string raw = trim(t.substr(eq + 1));
            const std::string key = section.empty() ? prop : section + "/" + prop;
            m_map[key] = parse(raw);
        }
        return true;
    }

private:
    static std::string trim(const std::string& s) {
        std::size_t a = 0, b = s.size();
        while (a < b && (s[a] == ' ' || s[a] == '\t')) ++a;
        while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t')) --b;
        return s.substr(a, b - a);
    }

    static std::string serialize(const SettingValue& v) {
        if (v.type == SettingValue::Type::String) {
            std::string out = "\"";
            for (char c : v.s) {
                if (c == '"' || c == '\\') out += '\\';
                out += c;
            }
            out += '"';
            return out;
        }
        return v.asString(); // bool/int/float render bare
    }

    static SettingValue parse(const std::string& raw) {
        if (raw.size() >= 2 && raw.front() == '"' && raw.back() == '"') {
            std::string s;
            for (std::size_t i = 1; i + 1 < raw.size(); ++i) {
                if (raw[i] == '\\' && i + 2 < raw.size()) { s += raw[i + 1]; ++i; }
                else s += raw[i];
            }
            return SettingValue::fromString(s);
        }
        if (raw == "true") return SettingValue::fromBool(true);
        if (raw == "false") return SettingValue::fromBool(false);
        // Numeric? Decide int vs float by the presence of a decimal point / exponent.
        if (!raw.empty()) {
            char* end = nullptr;
            const double d = std::strtod(raw.c_str(), &end);
            if (end == raw.c_str() + raw.size()) { // fully numeric
                if (raw.find_first_of(".eE") != std::string::npos)
                    return SettingValue::fromFloat(d);
                return SettingValue::fromInt(static_cast<std::int64_t>(std::strtoll(raw.c_str(), nullptr, 10)));
            }
        }
        return SettingValue::fromString(raw); // bare unquoted fallback
    }

    std::unordered_map<std::string, SettingValue> m_map;
};

} // namespace maz::core
