#pragma once

#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <vector>

// maz::core::TypeDesc — minimal, type-SAFE reflection: register a struct's fields once (by
// member pointer) and then read/write them generically by name. This is the small slice of Godot's
// ClassDB / property system that actually earns its keep: it powers reflection-driven serialization
// (walk the properties, emit each) and an editor inspector (list name+type, get/set live) WITHOUT
// hand-writing a save/load and an inspector row per field.
//
// It uses member pointers (obj.*m), not raw byte offsets, so it's fully type-checked at
// registration and never invokes UB. Supported field types: bool, any integral, any floating-point,
// and std::string — the value scalars a data-driven game config needs. A PropValue is a tagged
// scalar; get()/set() convert between it and the real field. serialize()/deserialize() round-trip
// every registered field to a compact text blob. Header-only, no GPU, no threads.
namespace maz::core {

enum class PropType { Int, Float, Bool, String };

// A tagged scalar — the currency reflection reads/writes.
struct PropValue {
    PropType type = PropType::Int;
    int64_t i = 0;
    double f = 0.0;
    bool b = false;
    std::string s;

    static PropValue makeInt(int64_t v) {
        PropValue p;
        p.type = PropType::Int;
        p.i = v;
        return p;
    }
    static PropValue makeFloat(double v) {
        PropValue p;
        p.type = PropType::Float;
        p.f = v;
        return p;
    }
    static PropValue makeBool(bool v) {
        PropValue p;
        p.type = PropType::Bool;
        p.b = v;
        return p;
    }
    static PropValue makeString(std::string v) {
        PropValue p;
        p.type = PropType::String;
        p.s = std::move(v);
        return p;
    }
};

namespace detail {
template <typename M> constexpr PropType propTypeOf() {
    if constexpr (std::is_same_v<M, bool>) {
        return PropType::Bool;
    } else if constexpr (std::is_same_v<M, std::string>) {
        return PropType::String;
    } else if constexpr (std::is_integral_v<M>) {
        return PropType::Int;
    } else if constexpr (std::is_floating_point_v<M>) {
        return PropType::Float;
    } else {
        static_assert(sizeof(M) == 0, "TypeDesc::prop: unsupported field type");
        return PropType::Int;
    }
}

template <typename M> PropValue toValue(const M& v) {
    if constexpr (std::is_same_v<M, bool>) {
        return PropValue::makeBool(v);
    } else if constexpr (std::is_same_v<M, std::string>) {
        return PropValue::makeString(v);
    } else if constexpr (std::is_integral_v<M>) {
        return PropValue::makeInt(static_cast<int64_t>(v));
    } else {
        return PropValue::makeFloat(static_cast<double>(v));
    }
}

template <typename M> M fromValue(const PropValue& p) {
    if constexpr (std::is_same_v<M, bool>) {
        return p.type == PropType::Bool ? p.b : (p.i != 0);
    } else if constexpr (std::is_same_v<M, std::string>) {
        return p.s;
    } else if constexpr (std::is_integral_v<M>) {
        return static_cast<M>(p.type == PropType::Float ? static_cast<int64_t>(p.f) : p.i);
    } else {
        return static_cast<M>(p.type == PropType::Int ? static_cast<double>(p.i) : p.f);
    }
}
} // namespace detail

template <typename T> class TypeDesc {
  public:
    struct Property {
        std::string name;
        PropType type;
        std::function<PropValue(const T&)> get;
        std::function<void(T&, const PropValue&)> set;
    };

    // Register a field by member pointer. Type is deduced and checked at compile time. Chainable.
    template <typename M> TypeDesc& prop(const std::string& name, M T::*member) {
        Property p;
        p.name = name;
        p.type = detail::propTypeOf<M>();
        p.get = [member](const T& o) { return detail::toValue<M>(o.*member); };
        p.set = [member](T& o, const PropValue& v) { o.*member = detail::fromValue<M>(v); };
        m_props.push_back(std::move(p));
        return *this;
    }

    const std::vector<Property>& properties() const { return m_props; }
    size_t propertyCount() const { return m_props.size(); }

    // Read one field by name into `out`. Returns false for an unknown name.
    bool get(const T& obj, const std::string& name, PropValue& out) const {
        for (const Property& p : m_props) {
            if (p.name == name) {
                out = p.get(obj);
                return true;
            }
        }
        return false;
    }

    // Write one field by name (converting as needed). Returns false for an unknown name.
    bool set(T& obj, const std::string& name, const PropValue& v) const {
        for (const Property& p : m_props) {
            if (p.name == name) {
                p.set(obj, v);
                return true;
            }
        }
        return false;
    }

    // Every field as (name, value) — the reflection-driven "read all properties" an inspector or
    // serializer walks.
    std::vector<std::pair<std::string, PropValue>> read(const T& obj) const {
        std::vector<std::pair<std::string, PropValue>> out;
        out.reserve(m_props.size());
        for (const Property& p : m_props) {
            out.emplace_back(p.name, p.get(obj));
        }
        return out;
    }

    // Serialize every field to a compact text blob: one "name=T:value" line per property (T is a
    // single type char i/f/b/s). Order matches registration.
    std::string serialize(const T& obj) const {
        std::string out;
        for (const Property& p : m_props) {
            const PropValue v = p.get(obj);
            out += p.name;
            out += '=';
            switch (v.type) {
            case PropType::Int:
                out += "i:" + std::to_string(v.i);
                break;
            case PropType::Float:
                out += "f:" + floatStr(v.f);
                break;
            case PropType::Bool:
                out += std::string("b:") + (v.b ? "1" : "0");
                break;
            case PropType::String:
                out += "s:" + escape(v.s);
                break;
            }
            out += '\n';
        }
        return out;
    }

    // Apply a serialize() blob back onto an object. Unknown property names are skipped (forward-
    // compatible). Returns the number of fields applied.
    int deserialize(T& obj, const std::string& text) const {
        int applied = 0;
        size_t pos = 0;
        while (pos < text.size()) {
            size_t nl = text.find('\n', pos);
            const std::string line =
                text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
            pos = (nl == std::string::npos) ? text.size() : nl + 1;
            if (line.empty()) {
                continue;
            }
            const size_t eq = line.find('=');
            if (eq == std::string::npos || eq + 2 > line.size() || line[eq + 2] != ':') {
                continue;
            }
            const std::string name = line.substr(0, eq);
            const char tc = line[eq + 1];
            const std::string val = line.substr(eq + 3);
            PropValue v;
            switch (tc) {
            case 'i':
                v = PropValue::makeInt(static_cast<int64_t>(std::stoll(val)));
                break;
            case 'f':
                v = PropValue::makeFloat(std::stod(val));
                break;
            case 'b':
                v = PropValue::makeBool(val == "1");
                break;
            case 's':
                v = PropValue::makeString(unescape(val));
                break;
            default:
                continue;
            }
            if (set(obj, name, v)) {
                ++applied;
            }
        }
        return applied;
    }

  private:
    static std::string floatStr(double d) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.9g", d);
        return buf;
    }
    // Newlines/backslashes in strings are escaped so each property stays on one line.
    static std::string escape(const std::string& s) {
        std::string o;
        for (char c : s) {
            if (c == '\\') {
                o += "\\\\";
            } else if (c == '\n') {
                o += "\\n";
            } else {
                o += c;
            }
        }
        return o;
    }
    static std::string unescape(const std::string& s) {
        std::string o;
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\\' && i + 1 < s.size()) {
                const char n = s[++i];
                o += (n == 'n') ? '\n' : n;
            } else {
                o += s[i];
            }
        }
        return o;
    }

    std::vector<Property> m_props;
};

} // namespace maz::core
