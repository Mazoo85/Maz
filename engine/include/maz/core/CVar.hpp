#pragma once

#include <cstdint>
#include <cstddef>
#include <cstdlib>  // std::strtod
#include <cctype>   // std::isspace (leading-whitespace reject in float parse)
#include <cmath>    // std::isfinite (non-finite reject in float parse)
#include <string>
#include <string_view>
#include <variant>
#include <unordered_map>
#include <functional>
#include <vector>
#include <charconv>  // std::from_chars (int)

#include "maz/core/StringId.hpp"
#include "maz/core/Assert.hpp"

// A typed runtime console-variable / settings registry — the Godot
// ProjectSettings / CVar analog. Register named typed variables
// (bool/int/float/string) each with a default and an optional description;
// type-checked get/set never throws — a wrong-type or missing name yields the
// caller's fallback (getters) or false (setters). `setFromString` parses
// console/config text into the variable's declared type. Change callbacks
// registered via `onChanged` fire whenever a set*/setFromString/reset actually
// CHANGES the stored value (a set to the same value is a no-op and fires
// nothing). Distinct from maz::core::Config (that's startup argv parsing only).
// NOT thread-safe. ini/json file load/save + persistence, a callback-unregister
// handle, and typed flags (archive/cheat) are future refinements (ini/json can
// compose iter16 ByteWriter/ByteReader or a text parser later).

namespace maz::core {

enum class CVarType : std::uint8_t { Bool, Int, Float, String };

class CVarRegistry {
  public:
    using Value = std::variant<bool, std::int64_t, double, std::string>;

    // --- REGISTRATION --------------------------------------------------------
    // Registration asserts !has(name) in debug builds. In release the assert is
    // gone, so a duplicate register* becomes a plain unordered_map::emplace that
    // is a NO-OP: the original entry is kept and the new default is ignored (by
    // design — first registration wins).
    void registerBool(maz::core::StringId name, bool def, std::string description = {}) {
        MAZ_ASSERT(name.valid(), "CVarRegistry: invalid cvar name");
        MAZ_ASSERT(!has(name), "CVarRegistry: duplicate cvar");
        m_vars.emplace(name, Entry{ CVarType::Bool, Value{def}, Value{def}, std::move(description), {} });
    }

    void registerInt(maz::core::StringId name, std::int64_t def, std::string description = {}) {
        MAZ_ASSERT(name.valid(), "CVarRegistry: invalid cvar name");
        MAZ_ASSERT(!has(name), "CVarRegistry: duplicate cvar");
        m_vars.emplace(name, Entry{ CVarType::Int, Value{def}, Value{def}, std::move(description), {} });
    }

    void registerFloat(maz::core::StringId name, double def, std::string description = {}) {
        MAZ_ASSERT(name.valid(), "CVarRegistry: invalid cvar name");
        MAZ_ASSERT(!has(name), "CVarRegistry: duplicate cvar");
        m_vars.emplace(name, Entry{ CVarType::Float, Value{def}, Value{def}, std::move(description), {} });
    }

    void registerString(maz::core::StringId name, std::string def, std::string description = {}) {
        MAZ_ASSERT(name.valid(), "CVarRegistry: invalid cvar name");
        MAZ_ASSERT(!has(name), "CVarRegistry: duplicate cvar");
        m_vars.emplace(name, Entry{ CVarType::String, Value{def}, Value{std::move(def)}, std::move(description), {} });
    }

    // --- GETTERS (wrong-type or missing -> fallback, never throws) -----------
    bool getBool(maz::core::StringId name, bool fallback = false) const {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::Bool ||
            !std::holds_alternative<bool>(it->second.value)) { return fallback; }
        return std::get<bool>(it->second.value);
    }

    std::int64_t getInt(maz::core::StringId name, std::int64_t fallback = 0) const {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::Int ||
            !std::holds_alternative<std::int64_t>(it->second.value)) { return fallback; }
        return std::get<std::int64_t>(it->second.value);
    }

    double getFloat(maz::core::StringId name, double fallback = 0.0) const {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::Float ||
            !std::holds_alternative<double>(it->second.value)) { return fallback; }
        return std::get<double>(it->second.value);
    }

    // Returns BY VALUE so the result never dangles past a later set/clear.
    std::string getString(maz::core::StringId name, std::string fallback = {}) const {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::String ||
            !std::holds_alternative<std::string>(it->second.value)) { return fallback; }
        return std::get<std::string>(it->second.value);
    }

    // --- SETTERS (missing/wrong-type -> false; fire onChange only on change) --
    // Invariant: Entry::type is authoritative and ALWAYS matches the active
    // Value alternative (registration sets both in sync and setters only assign
    // matching-type values), so std::get<T> after a `type` check is safe here
    // (no holds_alternative needed, unlike the defensive getters).
    bool setBool(maz::core::StringId name, bool v) {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::Bool) { return false; }
        if (std::get<bool>(it->second.value) != v) {
            it->second.value = v;
            fire(it->second);
        }
        return true;
    }

    bool setInt(maz::core::StringId name, std::int64_t v) {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::Int) { return false; }
        if (std::get<std::int64_t>(it->second.value) != v) {
            it->second.value = v;
            fire(it->second);
        }
        return true;
    }

    bool setFloat(maz::core::StringId name, double v) {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::Float) { return false; }
        if (std::get<double>(it->second.value) != v) {
            it->second.value = v;
            fire(it->second);
        }
        return true;
    }

    bool setString(maz::core::StringId name, std::string v) {
        auto it = m_vars.find(name);
        if (it == m_vars.end() || it->second.type != CVarType::String) { return false; }
        if (std::get<std::string>(it->second.value) != v) {
            it->second.value = std::move(v);
            fire(it->second);
        }
        return true;
    }

    // Parse `text` per the cvar's declared type and set it (firing onChange on
    // an actual change). Returns false if the name is missing or the parse
    // fails (in which case the stored value is left unchanged).
    bool setFromString(maz::core::StringId name, std::string_view text) {
        auto it = m_vars.find(name);
        if (it == m_vars.end()) { return false; }
        switch (it->second.type) {
            case CVarType::Bool: {
                bool v = false;
                if (text == "true" || text == "1") { v = true; }
                else if (text == "false" || text == "0") { v = false; }
                else { return false; }
                return setBool(name, v);
            }
            case CVarType::Int: {
                if (text.empty()) { return false; }
                std::int64_t v = 0;
                const char* first = text.data();
                const char* last = text.data() + text.size();
                auto [ptr, ec] = std::from_chars(first, last, v);
                if (ec != std::errc{} || ptr != last) { return false; }
                return setInt(name, v);
            }
            case CVarType::Float: {
                // std::from_chars(double) is unavailable on some libstdc++, so
                // parse via std::strtod. Locale note: strtod is locale-dependent
                // — acceptable for this slice. Require a non-empty, non-all-blank
                // string that strtod consumes ENTIRELY (rejects "abc"/"1.0junk").
                if (text.empty()) { return false; }
                bool allBlank = true;
                for (char c : text) {
                    if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') {
                        allBlank = false;
                        break;
                    }
                }
                if (allBlank) { return false; }
                std::string tmp(text);
                // Reject leading whitespace so the float path matches the int
                // path: std::from_chars rejects " 1", but strtod would SKIP the
                // leading blank and accept " 2.0" — keep both strict/identical.
                if (!tmp.empty() && std::isspace(static_cast<unsigned char>(tmp.front()))) { return false; }
                char* endptr = nullptr;
                double v = std::strtod(tmp.c_str(), &endptr);
                if (endptr != tmp.c_str() + tmp.size()) { return false; }
                // Reject non-finite (strtod accepts "nan"/"inf"/hex floats). A
                // NaN-valued cvar would make the change-guard (v != v) always
                // true and refire callbacks on every set. NOTE: this guards only
                // the string-parse path; setFloat(name, NaN) called directly is
                // the caller's responsibility.
                if (!std::isfinite(v)) { return false; }
                return setFloat(name, v);
            }
            case CVarType::String:
                return setString(name, std::string(text));
        }
        return false;
    }

    // --- QUERIES / LIFECYCLE -------------------------------------------------
    bool has(maz::core::StringId name) const { return m_vars.find(name) != m_vars.end(); }

    CVarType typeOf(maz::core::StringId name, CVarType fallback = CVarType::Bool) const {
        auto it = m_vars.find(name);
        return it == m_vars.end() ? fallback : it->second.type;
    }

    // Restores value to its registered default (firing onChange if it changed);
    // false if the name is missing.
    bool reset(maz::core::StringId name) {
        auto it = m_vars.find(name);
        if (it == m_vars.end()) { return false; }
        if (it->second.value != it->second.defaultValue) {
            it->second.value = it->second.defaultValue;
            fire(it->second);
        }
        return true;
    }

    // Appends a callback fired whenever this cvar's value CHANGES (via any
    // successful set*/setFromString/reset that alters the value). There is no
    // unregister handle in this first slice.
    void onChanged(maz::core::StringId name, std::function<void()> cb) {
        auto it = m_vars.find(name);
        MAZ_ASSERT(it != m_vars.end(), "CVarRegistry::onChanged: unknown cvar");
        it->second.onChange.push_back(std::move(cb));
    }

    std::size_t count() const { return m_vars.size(); }
    void clear() { m_vars.clear(); }

  private:
    struct Entry {
        CVarType type;
        Value value;
        Value defaultValue;
        std::string description;
        std::vector<std::function<void()>> onChange;
    };

    struct StringIdHash {
        std::size_t operator()(maz::core::StringId s) const {
            return static_cast<std::size_t>(s.hash());
        }
    };

    // Fire callbacks AFTER the new value is stored so a callback that reads the
    // cvar observes the updated value.
    static void fire(const Entry& e) {
        for (const std::function<void()>& cb : e.onChange) { cb(); }
    }

    std::unordered_map<maz::core::StringId, Entry, StringIdHash> m_vars;
};

} // namespace maz::core
