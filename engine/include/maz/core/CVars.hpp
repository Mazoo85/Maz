#pragma once

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace maz::core {

// Config variables ("cvars"): a central registry of named, typed, self-describing tunables that any
// subsystem registers once and everyone can read or override — the engine's single source of truth
// for settings (render exposure, gameplay speed, UI scale, debug toggles). Each cvar carries a type,
// a default, a human-readable description, and — for numbers — an optional [lo, hi] clamp. Values can
// be set programmatically (typed setters clamp) or coerced from a string (for command-line flags and
// text configs), and the whole set can be iterated for a config UI or a settings file. This header is
// deliberately dependency-free (std only); JSON load/save lives in the io layer (io/Config.hpp) so
// core keeps zero dependencies.

class CVarRegistry {
public:
    enum class Type { Bool, Int, Float, String };

    struct Entry {
        std::string name;
        std::string desc;
        Type type = Type::Float;
        double num = 0.0;   // holds bool (0/1), int, or float
        std::string str;    // holds string values
        bool clamped = false;
        double lo = 0.0, hi = 0.0;
    };

    // --- registration (idempotent: re-registering an existing name keeps its current value) -------
    Entry& registerBool(const std::string& name, bool def, const std::string& desc = "") {
        return reg(name, Type::Bool, def ? 1.0 : 0.0, "", desc);
    }
    Entry& registerInt(const std::string& name, int def, const std::string& desc = "") {
        return reg(name, Type::Int, static_cast<double>(def), "", desc);
    }
    Entry& registerFloat(const std::string& name, float def, const std::string& desc = "") {
        return reg(name, Type::Float, static_cast<double>(def), "", desc);
    }
    Entry& registerString(const std::string& name, const std::string& def,
                          const std::string& desc = "") {
        return reg(name, Type::String, 0.0, def, desc);
    }

    // Mark a numeric cvar as clamped to [lo, hi]; clamps the current value immediately.
    void setRange(const std::string& name, double lo, double hi) {
        if (Entry* e = find(name)) {
            e->clamped = true;
            e->lo = lo;
            e->hi = hi;
            e->num = clampNum(e->num, *e);
        }
    }

    // --- queries ----------------------------------------------------------------------------------
    bool has(const std::string& name) const { return find(name) != nullptr; }
    const Entry* get(const std::string& name) const { return find(name); }
    const std::vector<Entry>& entries() const { return m_entries; }

    bool getBool(const std::string& name, bool def = false) const {
        const Entry* e = find(name);
        return e ? e->num != 0.0 : def;
    }
    int getInt(const std::string& name, int def = 0) const {
        const Entry* e = find(name);
        return e ? static_cast<int>(e->num) : def;
    }
    float getFloat(const std::string& name, float def = 0.0f) const {
        const Entry* e = find(name);
        return e ? static_cast<float>(e->num) : def;
    }
    std::string getString(const std::string& name, const std::string& def = "") const {
        const Entry* e = find(name);
        return e ? e->str : def;
    }

    // --- typed setters (numeric ones clamp; no-op on unknown name) ---------------------------------
    bool setBool(const std::string& name, bool v) {
        Entry* e = find(name);
        if (!e) return false;
        e->num = v ? 1.0 : 0.0;
        return true;
    }
    bool setInt(const std::string& name, int v) {
        Entry* e = find(name);
        if (!e) return false;
        e->num = clampNum(static_cast<double>(v), *e);
        return true;
    }
    bool setFloat(const std::string& name, float v) {
        Entry* e = find(name);
        if (!e) return false;
        e->num = clampNum(static_cast<double>(v), *e);
        return true;
    }
    bool setString(const std::string& name, const std::string& v) {
        Entry* e = find(name);
        if (!e) return false;
        e->str = v;
        return true;
    }

    // Coerce a string into the cvar's type (for CLI flags / text configs). Returns false on unknown
    // name or unparsable value; the cvar is left unchanged on failure.
    bool setFromString(const std::string& name, const std::string& value) {
        Entry* e = find(name);
        if (!e) return false;
        switch (e->type) {
        case Type::Bool: {
            if (value == "true" || value == "1" || value == "on" || value == "yes") {
                e->num = 1.0;
                return true;
            }
            if (value == "false" || value == "0" || value == "off" || value == "no") {
                e->num = 0.0;
                return true;
            }
            return false;
        }
        case Type::Int: {
            try {
                size_t used = 0;
                const long v = std::stol(value, &used);
                if (used == 0) return false;
                e->num = clampNum(static_cast<double>(v), *e);
                return true;
            } catch (...) {
                return false;
            }
        }
        case Type::Float: {
            try {
                size_t used = 0;
                const double v = std::stod(value, &used);
                if (used == 0) return false;
                e->num = clampNum(v, *e);
                return true;
            } catch (...) {
                return false;
            }
        }
        case Type::String: e->str = value; return true;
        }
        return false;
    }

    // Render a cvar's current value as a display string (for a config UI / settings dump).
    std::string valueString(const Entry& e) const {
        switch (e.type) {
        case Type::Bool: return e.num != 0.0 ? "true" : "false";
        case Type::Int: return std::to_string(static_cast<long>(e.num));
        case Type::Float: {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.3g", e.num);
            return buf;
        }
        case Type::String: return e.str;
        }
        return "";
    }

    // Apply "name=value" assignment tokens (e.g., from the command line). Returns the count applied.
    int applyAssignments(const std::vector<std::string>& tokens) {
        int applied = 0;
        for (const std::string& t : tokens) {
            const size_t eq = t.find('=');
            if (eq == std::string::npos) continue;
            if (setFromString(t.substr(0, eq), t.substr(eq + 1))) ++applied;
        }
        return applied;
    }

private:
    std::vector<Entry> m_entries;

    Entry* find(const std::string& name) {
        for (Entry& e : m_entries)
            if (e.name == name) return &e;
        return nullptr;
    }
    const Entry* find(const std::string& name) const {
        for (const Entry& e : m_entries)
            if (e.name == name) return &e;
        return nullptr;
    }

    static double clampNum(double v, const Entry& e) {
        if (!e.clamped) return v;
        if (v < e.lo) return e.lo;
        if (v > e.hi) return e.hi;
        return v;
    }

    Entry& reg(const std::string& name, Type type, double num, const std::string& str,
               const std::string& desc) {
        if (Entry* e = find(name)) return *e;  // keep existing value on re-registration
        Entry e;
        e.name = name;
        e.desc = desc;
        e.type = type;
        e.num = num;
        e.str = str;
        m_entries.push_back(std::move(e));
        return m_entries.back();
    }
};

} // namespace maz::core
