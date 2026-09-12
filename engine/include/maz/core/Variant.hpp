#pragma once

#include "maz/core/StringUtils.hpp" // toInt, toFloat (string coercion)
#include "maz/math/Math.hpp"        // vec2, vec3

#include <cstdint>
#include <string>
#include <variant>

// maz::core Variant — Godot's Variant: one value that can hold any of the common gameplay types and
// convert between them the way Godot's dynamic typing does. Data-driven configs, script bridges,
// serialized properties, and generic containers all want a single "any" value with a known type tag
// and forgiving getters. This holds the everyday subset: Nil / Bool / Int (64-bit) / Float (double) /
// String / Vector2 / Vector3, with Godot-style coercion (asInt/asFloat/asBool), truthiness
// (booleanize), value equality (numeric types compare across Int/Float/Bool), and stringify. Pure,
// header-only, unit-tested. (Dictionary/Array and the full type zoo are out of scope here.)
namespace maz::core {

enum class VariantType { Nil, Bool, Int, Float, String, Vector2, Vector3 };

class Variant {
public:
    Variant() = default; // Nil
    Variant(bool b) : m_v(b) {}
    Variant(int i) : m_v(static_cast<std::int64_t>(i)) {}
    Variant(std::int64_t i) : m_v(i) {}
    Variant(float f) : m_v(static_cast<double>(f)) {}
    Variant(double d) : m_v(d) {}
    Variant(const char* s) : m_v(std::string(s)) {}
    Variant(std::string s) : m_v(std::move(s)) {}
    Variant(math::vec2 v) : m_v(v) {}
    Variant(math::vec3 v) : m_v(v) {}

    VariantType type() const { return static_cast<VariantType>(m_v.index()); }
    bool isNil() const { return type() == VariantType::Nil; }

    // ---- type-coercing getters (Godot's implicit conversions) --------------------------------------
    std::int64_t asInt() const {
        switch (type()) {
        case VariantType::Bool:
            return std::get<bool>(m_v) ? 1 : 0;
        case VariantType::Int:
            return std::get<std::int64_t>(m_v);
        case VariantType::Float:
            return static_cast<std::int64_t>(std::get<double>(m_v));
        case VariantType::String:
            return toInt(std::get<std::string>(m_v));
        default:
            return 0;
        }
    }
    double asFloat() const {
        switch (type()) {
        case VariantType::Bool:
            return std::get<bool>(m_v) ? 1.0 : 0.0;
        case VariantType::Int:
            return static_cast<double>(std::get<std::int64_t>(m_v));
        case VariantType::Float:
            return std::get<double>(m_v);
        case VariantType::String:
            return toFloat(std::get<std::string>(m_v));
        default:
            return 0.0;
        }
    }
    bool asBool() const { return booleanize(); }

    math::vec2 asVector2() const {
        return type() == VariantType::Vector2 ? std::get<math::vec2>(m_v) : math::vec2(0.0f);
    }
    math::vec3 asVector3() const {
        return type() == VariantType::Vector3 ? std::get<math::vec3>(m_v) : math::vec3(0.0f);
    }

    // Godot truthiness: Nil=false, Bool=itself, numbers != 0, String non-empty, Vectors non-zero.
    bool booleanize() const {
        switch (type()) {
        case VariantType::Nil:
            return false;
        case VariantType::Bool:
            return std::get<bool>(m_v);
        case VariantType::Int:
            return std::get<std::int64_t>(m_v) != 0;
        case VariantType::Float:
            return std::get<double>(m_v) != 0.0;
        case VariantType::String:
            return !std::get<std::string>(m_v).empty();
        case VariantType::Vector2: {
            const auto v = std::get<math::vec2>(m_v);
            return v.x != 0.0f || v.y != 0.0f;
        }
        case VariantType::Vector3: {
            const auto v = std::get<math::vec3>(m_v);
            return v.x != 0.0f || v.y != 0.0f || v.z != 0.0f;
        }
        }
        return false;
    }

    // Human/serialized text — Godot's str(). Floats trim trailing zeros; vectors as "(x, y[, z])".
    std::string stringify() const {
        switch (type()) {
        case VariantType::Nil:
            return "null";
        case VariantType::Bool:
            return std::get<bool>(m_v) ? "true" : "false";
        case VariantType::Int:
            return std::to_string(std::get<std::int64_t>(m_v));
        case VariantType::Float:
            return trimFloat(std::get<double>(m_v));
        case VariantType::String:
            return std::get<std::string>(m_v);
        case VariantType::Vector2: {
            const auto v = std::get<math::vec2>(m_v);
            return "(" + trimFloat(v.x) + ", " + trimFloat(v.y) + ")";
        }
        case VariantType::Vector3: {
            const auto v = std::get<math::vec3>(m_v);
            return "(" + trimFloat(v.x) + ", " + trimFloat(v.y) + ", " + trimFloat(v.z) + ")";
        }
        }
        return "";
    }

    // Value equality. Numeric types (Bool/Int/Float) compare by numeric value across each other;
    // everything else must match type and value. Two Nils are equal.
    bool operator==(const Variant& o) const {
        if (isNumeric() && o.isNumeric()) {
            return asFloat() == o.asFloat();
        }
        if (type() != o.type()) {
            return false;
        }
        switch (type()) {
        case VariantType::Nil:
            return true;
        case VariantType::String:
            return std::get<std::string>(m_v) == std::get<std::string>(o.m_v);
        case VariantType::Vector2:
            return std::get<math::vec2>(m_v) == std::get<math::vec2>(o.m_v);
        case VariantType::Vector3:
            return std::get<math::vec3>(m_v) == std::get<math::vec3>(o.m_v);
        default:
            return false; // numeric handled above
        }
    }
    bool operator!=(const Variant& o) const { return !(*this == o); }

private:
    bool isNumeric() const {
        const VariantType t = type();
        return t == VariantType::Bool || t == VariantType::Int || t == VariantType::Float;
    }

    static std::string trimFloat(double d) {
        std::string s = std::to_string(d);
        const std::size_t dot = s.find('.');
        if (dot == std::string::npos) {
            return s;
        }
        std::size_t end = s.size();
        while (end > dot + 1 && s[end - 1] == '0') {
            --end;
        }
        if (end == dot + 1) {
            end = dot; // drop the trailing dot too ("2." -> "2")
        }
        return s.substr(0, end);
    }

    // Index order MUST match VariantType.
    std::variant<std::monostate, bool, std::int64_t, double, std::string, math::vec2, math::vec3>
        m_v;
};

} // namespace maz::core
