#pragma once

#include "maz/core/CVars.hpp"
#include "maz/io/Json.hpp"

namespace maz::io {

// The bridge between the config registry (core::CVarRegistry, dependency-free) and JSON. This lives
// in the io layer so core stays zero-dependency while apps still get "config.json drives the engine":
// loadConfig applies a parsed JSON object's fields onto matching cvars (coercing JSON types into each
// cvar's declared type, so a bool cvar ignores a stray number), and configToJson serializes the whole
// registry back out for a settings file. Unknown keys are ignored, so a config file may target a
// subset (or a superset) of the registered cvars without error.

// Apply a JSON object of overrides onto the registry. Returns the number of cvars changed. Fields
// whose names aren't registered are ignored; type mismatches fall back to string coercion.
inline int loadConfig(core::CVarRegistry& reg, const JsonValue& obj) {
    if (!obj.isObject()) return 0;
    int applied = 0;
    for (const auto& kv : obj.fields().items) {
        const std::string& name = kv.first;
        const JsonValue& v = kv.second;
        const core::CVarRegistry::Entry* e = reg.get(name);
        if (!e) continue;
        bool ok = false;
        switch (e->type) {
        case core::CVarRegistry::Type::Bool:
            if (v.isBool())
                ok = reg.setBool(name, v.asBool());
            else if (v.isNumber())
                ok = reg.setBool(name, v.asNumber() != 0.0);
            else if (v.isString())
                ok = reg.setFromString(name, v.asString());
            break;
        case core::CVarRegistry::Type::Int:
            if (v.isNumber())
                ok = reg.setInt(name, v.asInt());
            else if (v.isString())
                ok = reg.setFromString(name, v.asString());
            break;
        case core::CVarRegistry::Type::Float:
            if (v.isNumber())
                ok = reg.setFloat(name, v.asFloat());
            else if (v.isString())
                ok = reg.setFromString(name, v.asString());
            break;
        case core::CVarRegistry::Type::String:
            if (v.isString())
                ok = reg.setString(name, v.asString());
            else
                ok = reg.setString(name, v.dump());
            break;
        }
        if (ok) ++applied;
    }
    return applied;
}

// Serialize the whole registry to a JSON object (name -> typed value), preserving registration order.
inline JsonValue configToJson(const core::CVarRegistry& reg) {
    JsonValue obj = JsonValue::object();
    for (const auto& e : reg.entries()) {
        switch (e.type) {
        case core::CVarRegistry::Type::Bool: obj.set(e.name, JsonValue(e.num != 0.0)); break;
        case core::CVarRegistry::Type::Int:
            obj.set(e.name, JsonValue(static_cast<int64_t>(e.num)));
            break;
        case core::CVarRegistry::Type::Float: obj.set(e.name, JsonValue(e.num)); break;
        case core::CVarRegistry::Type::String: obj.set(e.name, JsonValue(e.str)); break;
        }
    }
    return obj;
}

// Convenience: load config overrides directly from a JSON file on disk. Returns the count applied
// (0 if the file is missing/unparsable — callers that care can parse explicitly for the error).
inline int loadConfigFile(core::CVarRegistry& reg, const std::string& path) {
    auto parsed = parseJsonFile(path);
    if (!parsed.ok) return 0;
    return loadConfig(reg, parsed.value);
}

// Convenience: write the registry to a JSON file on disk (pretty-printed). Returns false on failure.
inline bool saveConfigFile(const core::CVarRegistry& reg, const std::string& path) {
    return writeJsonFile(path, configToJson(reg), 2);
}

} // namespace maz::io
